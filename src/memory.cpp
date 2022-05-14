#include "memory.h"
#include "core.h"
#include "thread.h"

thread_local Allocator mem_tmp;
Allocator mem_dynamic;

void* align_ptr(void *ptr, u8 alignment, u8 header_size);

template<typename T>
T* get_header(void *aligned_ptr) { return (T*)((size_t)aligned_ptr-sizeof(T)); }

#if defined(_WIN32)
#include "win32_memory.cpp"
#elif defined(__linux__)
#include "linux_memory.cpp"
#else
#error "unsupported platform"
#endif

struct LinearAllocatorState {
    u8 *start;
    u8 *end;
    u8 *current;

    void *last;
};

#define DEBUG_VIRTUAL_HEAP_ALLOC 0

// TODO(jesper): I think I want a thread-local version of this that doesn't do
// any thread safe guarding
struct VMFreeListState {
    struct Block {
        Block *next;
        Block *prev;
        i64 size;
    };

    u8 *mem;
    i64 reserved;
    i64 committed;

    Block *free_block;
    Mutex *mutex;

    i32 page_size;
};

void init_thread_allocators(i32 tmp_size = 10*1024*1024)
{
    ASSERT(mem_tmp.proc == nullptr);
    mem_tmp = linear_allocator(tmp_size);
}

void init_default_allocators()
{
    init_thread_allocators();
    mem_dynamic = malloc_allocator();
}

void* align_ptr(void *ptr, u8 alignment, u8 header_size)
{
    u8 offset = header_size + alignment - (((size_t)ptr + header_size) & ((size_t)alignment-1));
    return (void*)((size_t)ptr + offset);
}

void* linear_alloc(void *v_state, AllocatorCmd cmd, void *old_ptr, i64 old_size, i64 size, u8 alignment)
{
    auto state = (LinearAllocatorState*)v_state;

    switch (cmd) {
    case ALLOCATOR_CMD_ALLOC:
        {
            if (size == 0) return nullptr;

            if (state->current + size >= state->end) {
                LOG_ERROR("allocator does not have enough memory for allocation: %lld", size);
                return nullptr;
            }
            u8 *ptr = state->current;
            state->current += size;
            state->last = ptr;
            return ptr;
        } break;
    case ALLOCATOR_CMD_FREE:
        LOG_ERROR("free called on linear allocator, unsupported");
        return nullptr;
    case ALLOCATOR_CMD_REALLOC:
        {
            if (size == 0) return nullptr;

            if (old_ptr && state->last == old_ptr) {
                state->current += size-old_size;
                return old_ptr;
            }

            //LOG_INFO("leaking %lld bytes", old_size);
            void *ptr = linear_alloc(v_state, ALLOCATOR_CMD_ALLOC, nullptr, 0, size, alignment);
            if (old_size > 0) memcpy(ptr, old_ptr, old_size);
            return ptr;
        } break;
    case ALLOCATOR_CMD_RESET:
        state->last = nullptr;
        state->current = state->start;
        return nullptr;
    }
}

void* malloc_alloc(void */*v_state*/, AllocatorCmd cmd, void *old_ptr, i64 /*old_size*/, i64 size, u8 alignment)
{
    struct Header {
        u8 offset;
        u8 alignment;
    };

    switch (cmd) {
    case ALLOCATOR_CMD_ALLOC: {
            if (size == 0) return nullptr;

            u8 header_size = sizeof(Header);
            void *ptr = malloc(size+alignment+header_size);
            void *aligned_ptr = align_ptr(ptr, alignment, header_size);

            auto header = get_header<Header>(aligned_ptr); header->offset = (u8)((size_t)aligned_ptr - (size_t)ptr);
            header->alignment = alignment;
            return aligned_ptr;
        }
    case ALLOCATOR_CMD_FREE:
        if (old_ptr) {
            Header *header = get_header<Header>(old_ptr);
            void *unaligned_ptr = (void*)((size_t)old_ptr - header->offset);
            free(unaligned_ptr);
        }
        return nullptr;
    case ALLOCATOR_CMD_REALLOC: {
            void *old_unaligned_ptr = nullptr;
            if (old_ptr) {
                Header *header = get_header<Header>(old_ptr);
                ASSERT(header->alignment == alignment);
                old_unaligned_ptr = (void*)((size_t)old_ptr - header->offset);
            }

            u8 header_size = sizeof(Header);
            void *ptr = realloc(old_unaligned_ptr, size+alignment+header_size);
            void *aligned_ptr = align_ptr(ptr, alignment, header_size);

            auto header = get_header<Header>(aligned_ptr);
            header->offset = (u8)((size_t)aligned_ptr - (size_t)ptr);
            header->alignment = alignment;

            return aligned_ptr;
        }
    case ALLOCATOR_CMD_RESET:
        LOG_ERROR("reset called on malloc allocator, unsupported");
        return nullptr;
    }
}

Allocator linear_allocator(i64 size)
{
    u8 *mem = (u8*)malloc(size);

    LinearAllocatorState *state = (LinearAllocatorState*)mem;
    state->start = mem + sizeof *state;
    state->end = mem+size;
    state->current = state->start;
    state->last = nullptr;

    return Allocator{ state, linear_alloc };
}

Allocator malloc_allocator()
{
    return Allocator{ nullptr, malloc_alloc };
}


void* vm_freelist_alloc(void *v_state, AllocatorCmd cmd, void *old_ptr, i64 old_size, i64 size, u8 alignment)
{
    struct Header {
        u64 total_size;
        u8 offset;
        u8 alignment;
    };

    using Block = VMFreeListState::Block;

    auto state = (VMFreeListState*)v_state;

    switch (cmd) {
    case ALLOCATOR_CMD_ALLOC: {
            if (size == 0) return nullptr;

            lock_mutex(state->mutex);
            defer { unlock_mutex(state->mutex); };

            u8 header_size = sizeof(Header);
            i64 minimum_size = sizeof(Block);
            i64 required_size = size+alignment+header_size;

            auto block = state->free_block;
            while (block && block->size < required_size) block = block->next;
            if (!block || block->size - required_size < minimum_size) {
                i64 commit_size = ROUND_TO(required_size, state->page_size);
                if (state->committed + commit_size > state->reserved) {
                    LOG_ERROR("virtual size exceeded");
                    return nullptr;
                }

                block = (Block*)virtual_commit(state->mem+state->committed, commit_size);
                block->next = state->free_block;
                block->prev = nullptr;
                block->size = commit_size;

                if (state->free_block) state->free_block->prev = block;
                state->free_block = block;

                state->committed += commit_size;

#if DEBUG_VIRTUAL_HEAP_ALLOC
                LOG_INFO("commmitted new block [%p] of size %lld, total committed: %lld", block, commit_size, state->committed);
#endif
            }

            void *ptr;
            i64 total_size = required_size;
            if (block->size-required_size < minimum_size) {
#if DEBUG_VIRTUAL_HEAP_ALLOC
                LOG_INFO("popping block [%p] of size %d", block, block->size);
#endif
                if (state->free_block == block) {
                    state->free_block = block->next ? block->next : block->prev;
                }

                if (block->next->prev) block->next->prev = block->prev;
                if (block->prev) block->prev->next = block->next;
                total_size = block->size;
                ptr = block;
            } else {
                ptr = (u8*)block + block->size - required_size;
                block->size -= required_size;
#if DEBUG_VIRTUAL_HEAP_ALLOC
                LOG_INFO("shrinking block [%p] by %d bytes, %lld remain", block, required_size, block->size);
#endif
            }

            void *aligned_ptr = align_ptr(ptr, alignment, header_size);

            Header* header = get_header<Header>(aligned_ptr);
            header->offset = (u8)((size_t)aligned_ptr - (size_t)ptr);
            header->alignment = alignment;
            header->total_size = total_size;

#if DEBUG_VIRTUAL_HEAP_ALLOC
            for (Block *b = state->free_block; b; b = b->next) {
                LOG_RAW("\t\t[%p] size: %lld, prev [%p], next [%p]\n", b, b->size, b->prev, b->next);
            }

            LOG_INFO("allocated [%p]", aligned_ptr);
#endif

            return aligned_ptr;
        } break;
    case ALLOCATOR_CMD_FREE: {
            if (!old_ptr) break;

            lock_mutex(state->mutex);
            defer { unlock_mutex(state->mutex); };

            // TODO(jesper): do we try to expand neighbor free blocks to reduce fragmentation?
            Header* header = get_header<Header>(old_ptr);
            void *unaligned_ptr = (void*)((size_t)old_ptr - header->offset);
            i64 total_size = header->total_size;

            auto block = (Block*)unaligned_ptr;
            block->size = total_size;
            block->next = state->free_block;
            block->prev = nullptr;

            if (state->free_block) state->free_block->prev = block;
            state->free_block = block;

#if DEBUG_VIRTUAL_HEAP_ALLOC
            LOG_INFO("inserting free block [%p] size %lld", block, block->size);
            for (Block *b = state->free_block; b; b = b->next) {
                LOG_RAW("\t\t[%p] size: %lld, prev [%p], next [%p]\n", b, b->size, b->prev, b->next);
            }
            LOG_INFO("freed [%p]", old_ptr);
#endif

        } break;
    case ALLOCATOR_CMD_REALLOC: {
            if (size == 0) return nullptr;

            // TODO(jesper): if size < old_size, shrink the allocation and push the free block to the list
            // TODO(jesper): we should check if the current allocation can be expanded
            void *nptr = vm_freelist_alloc(v_state, ALLOCATOR_CMD_ALLOC, nullptr, 0, size, alignment);
            if (old_ptr == nullptr) return nptr;

            memcpy(nptr, old_ptr, old_size);
            vm_freelist_alloc(v_state, ALLOCATOR_CMD_FREE, old_ptr, 0, 0, 0);
            return nptr;
        } break;
    case ALLOCATOR_CMD_RESET:
        LOG_ERROR("unsupported command called for vm_freelist_allocator: reset");
        break;
    }

    return nullptr;
}

Allocator vm_freelist_allocator(i64 max_size)
{
	i32 page_size = get_page_size();
    i64 reserve_size = ROUND_TO(max_size, page_size);
    void *mem = virtual_reserve(reserve_size);

    VMFreeListState *state = (VMFreeListState *)malloc(sizeof *state);
    state->mem = (u8*)mem;
    state->reserved = reserve_size;
    state->committed = 0;
    state->mutex = create_mutex();
    state->free_block = nullptr;
    state->page_size = page_size;

    return Allocator{ state, vm_freelist_alloc };
}
