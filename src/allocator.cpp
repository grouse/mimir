#include "allocator.h"
#include "core.h"
#include "thread.h"

thread_local Allocator mem_tmp;
Allocator mem_dynamic;

void* align_ptr(void *ptr, u8 alignment, u8 header_size);

template<typename T>
T* get_header(void *aligned_ptr) { return (T*)((size_t)aligned_ptr-sizeof(T)); }

#ifdef _WIN32
#include "win32_allocator.cpp"
#endif

struct LinearAllocatorState {
    u8 *start;
    u8 *end;
    u8 *current;
    
    void *last;
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
