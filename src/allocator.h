#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "platform.h"

#define DEFAULT_ALIGN 16

#define ALLOC(alloc, size) alloc.proc(alloc.state, ALLOCATOR_CMD_ALLOC, nullptr, 0, size, DEFAULT_ALIGN)
#define ALLOC_A(alloc, size, alignment) alloc.proc(alloc.state, ALLOCATOR_CMD_ALLOC, nullptr, 0, size, alignment)
#define ALLOC_T(alloc, T) (T*)alloc.proc(alloc.state, ALLOCATOR_CMD_ALLOC, nullptr, 0, sizeof(T), alignof(T))

#define ALLOC_ARR(alloc, T, count) (T*)alloc.proc(alloc.state, ALLOCATOR_CMD_ALLOC, nullptr, 0, sizeof(T)*count, alignof(T))
#define REALLOC_ARR(alloc, T, old_ptr, old_count, new_count) (T*)alloc.proc(alloc.state, ALLOCATOR_CMD_REALLOC, old_ptr, old_count*sizeof(T), sizeof(T)*new_count, alignof(T))
#define REALLOC_ARR_A(alloc, T, old_ptr, old_count, new_count, alignment) (T*)alloc.proc(alloc.state, ALLOCATOR_CMD_REALLOC, old_ptr, old_count*sizeof(T), sizeof(T)*new_count, alignment)


#define FREE(alloc, ptr) alloc.proc(alloc.state, ALLOCATOR_CMD_FREE, nullptr, 0, 0, 0)
#define REALLOC(alloc, old_ptr, old_size, size) alloc.proc(alloc.state, ALLOCATOR_CMD_REALLOC, old_ptr, old_size, size, DEFAULT_ALIGN)
#define RESET_ALLOC(alloc) alloc.proc(alloc.state, ALLOCATOR_CMD_RESET, nullptr, 0, 0, 0)

enum AllocatorCmd {
    ALLOCATOR_CMD_ALLOC,
    ALLOCATOR_CMD_FREE,
    ALLOCATOR_CMD_REALLOC,
    ALLOCATOR_CMD_RESET,
};

typedef void* AllocatorProc(void *state, AllocatorCmd cmd, void *old_ptr, i64 old_size, i64 size, u8 alignment);

struct Allocator {
    void *state;
    AllocatorProc *proc;
};

extern thread_local Allocator mem_tmp;
extern Allocator mem_dynamic;

void init_default_allocators();
Allocator linear_allocator(i64 size);
Allocator malloc_allocator();

#endif // ALLOCATOR_H