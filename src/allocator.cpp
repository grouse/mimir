#include "allocator.h"

#include "core.h"

struct LinearAllocatorState {
    u8 *start;
    u8 *end;
    u8 *current;
};

struct AllocationHeader {
    u8 offset;
};

void* align_ptr(void *ptr, u8 alignment, u8 header_size)
{
    u8 offset = header_size + alignment - (((size_t)ptr + header_size) & ((size_t)alignment-1));
    return (void*)((size_t)ptr + offset);
}

void write_header(void *ptr, void *aligned_ptr)
{
    AllocationHeader *header = (AllocationHeader*)((size_t)aligned_ptr - sizeof *header);
    header->offset = (u8)((size_t)aligned_ptr - (size_t)ptr);
}

AllocationHeader* get_header(void *aligned_ptr)
{
    return (AllocationHeader*)((size_t)aligned_ptr-sizeof(AllocationHeader));
}

void* linear_alloc(void *v_state, AllocatorCmd cmd, void *old_ptr, i64 old_size, i64 size, u8 alignment)
{
    auto state = (LinearAllocatorState*)v_state;

    switch (cmd) {
    case ALLOCATOR_CMD_ALLOC: 
        {
            if (state->current + size >= state->end) {
                LOG_ERROR("allocator does not have enough memory for allocation: %lld", size);
                return nullptr;
            }
            u8 *ptr = state->current;
            state->current += size;
            return ptr;
        } break;
    case ALLOCATOR_CMD_FREE: 
        LOG_ERROR("free called on linear allocator, unsupported");
        return nullptr;
    case ALLOCATOR_CMD_REALLOC:
        {
            void *ptr = linear_alloc(v_state, ALLOCATOR_CMD_ALLOC, nullptr, 0, size, alignment);
            if (old_size > 0) memcpy(ptr, old_ptr, old_size);
            return ptr;
        } break;
    case ALLOCATOR_CMD_RESET:
        state->current = state->start;
        return nullptr;
    }
}

void* malloc_alloc(void */*v_state*/, AllocatorCmd cmd, void *old_ptr, i64 /*old_size*/, i64 size, u8 alignment)
{
    switch (cmd) {
    case ALLOCATOR_CMD_ALLOC: {
            u8 header_size = sizeof(AllocationHeader);
            void *ptr = malloc(size+alignment+header_size);
            void *aligned_ptr = align_ptr(ptr, alignment, header_size);
            write_header(ptr, aligned_ptr);
            return aligned_ptr;
        } 
    case ALLOCATOR_CMD_FREE: {
            if (old_ptr) {
                AllocationHeader *header = get_header(old_ptr);
                void *unaligned_ptr = (void*)((size_t)old_ptr - header->offset);
                free(unaligned_ptr);
            }
            return nullptr;
        } 
    case ALLOCATOR_CMD_REALLOC: {
            void *old_unaligned_ptr = nullptr;
            if (old_ptr) {
                AllocationHeader *header = get_header(old_ptr);
                old_unaligned_ptr = (void*)((size_t)old_ptr - header->offset);
            }
            
            u8 header_size = sizeof(AllocationHeader);
            void *ptr = realloc(old_unaligned_ptr, size+alignment+header_size);
            void *aligned_ptr = align_ptr(ptr, alignment, header_size);
            write_header(ptr, aligned_ptr);
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
    
    return Allocator{ state, linear_alloc };
}

Allocator malloc_allocator()
{
    return Allocator{ nullptr, malloc_alloc };
}

void init_default_allocators()
{
    mem_tmp = linear_allocator(10*1024*1024);
    mem_dynamic = malloc_allocator();
}
                   
