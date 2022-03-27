#ifndef ARRAY_H
#define ARRAY_H

#include "core.h"
#include "allocator.h"

template<typename T>
struct Array {
    T *data;
    i32 count;

    T& operator[](i32 i) 
    { 
        ASSERT(i < count && i >= 0);
        return data[i]; 
    }
    
    T& at(i32 i) 
    { 
        ASSERT(i < count && i >= 0);
        return data[i]; 
    }


    T* begin() { return &data[0]; }
    T* end() { return &data[count]; }
};

template<typename T>
struct DynamicArray : Array<T> {
    i32 capacity;
    Allocator alloc;
};

template<typename T>
void array_create(DynamicArray<T> *arr, i32 capacity = 0, Allocator mem = mem_tmp)
{
    arr->data = capacity > 0 ? ALLOC_ARR(mem, T, capacity) : nullptr;
    arr->count = arr->capacity = capacity;
    arr->alloc = mem;
}

template<typename T>
void array_create(Array<T> *arr, i32 capacity = 0, Allocator mem = mem_dynamic)
{
    arr->data = capacity > 0 ? ALLOC_ARR(mem, T, capacity) : nullptr;
    arr->count = capacity;
}

template<typename T>
void array_reserve(DynamicArray<T> *arr, i32 capacity)
{
    if (arr->alloc.proc == nullptr) {
        arr->alloc = mem_dynamic;
    }

    arr->data = (T*)ALLOC(arr->alloc, capacity*sizeof(T));
    arr->capacity = capacity;
}

template<typename T>
void array_reserve_add(DynamicArray<T> *arr, i32 extra_capacity)
{
    if (arr->alloc.proc == nullptr) arr->alloc = mem_dynamic;

    i32 diff = arr->count + extra_capacity - arr->capacity;
    if (diff > 0) {
        i32 old_capacity = arr->capacity;
        arr->capacity += diff;
        arr->data = REALLOC_ARR(arr->alloc, T, arr->data, old_capacity, arr->capacity);
    }
}

template<typename T>
void array_resize(DynamicArray<T> *arr, i32 count)
{
    if (arr->alloc.proc == nullptr) arr->alloc = mem_dynamic;
    
    if (arr->capacity < count) {
        arr->data = REALLOC_ARR(arr->alloc, T, arr->data, arr->capacity, count);
        arr->capacity = count;
    }
    
    arr->count = count;
}

template<typename T>
void array_copy(DynamicArray<T> *dst, Array<T> src)
{
    array_resize(dst, src.count);
    for (i32 i = 0; i < src.count; i++) dst->data[i] = src.data[i];
}

template<typename T>
void array_reset(DynamicArray<T> *arr, Allocator alloc)
{
    arr->alloc = alloc;
    arr->count = arr->capacity = 0;
    arr->data = nullptr;
}

template<typename T>
void array_reset(DynamicArray<T> *arr, Allocator alloc, i32 reserve_capacity)
{
    arr->alloc = alloc;
    arr->count = arr->capacity = 0;
    arr->data = nullptr;

    array_reserve(arr, reserve_capacity);
}

template<typename T>
i32 array_add(DynamicArray<T> *arr, T e)
{
    if (arr->count+1 > arr->capacity) {
        if (arr->alloc.proc == nullptr) {
            arr->alloc = mem_dynamic;
        }

        i32 old_capacity = arr->capacity;
        arr->capacity = arr->capacity == 0 ? 1 : arr->capacity*2;
        arr->data = REALLOC_ARR(arr->alloc, T, arr->data, old_capacity, arr->capacity);
    }

    arr->data[arr->count] = e;
    return arr->count++;
}

template<typename T>
i32 array_add(DynamicArray<T> *arr, T *es, i32 count)
{
    if (arr->count+count > arr->capacity) {
        if (arr->alloc.proc == nullptr) {
            arr->alloc = mem_dynamic;
        }

        i32 old_capacity = arr->capacity;
        arr->capacity = MAX(arr->count+count, arr->capacity*2);
        arr->data = REALLOC_ARR(arr->alloc, T, arr->data, old_capacity, arr->capacity);
    }

    memcpy(&arr->data[arr->count], es, count*sizeof *es);
    arr->count += count;
    return arr->count;
}

template<typename T>
i32 array_add(DynamicArray<T> *arr, Array<T> es)
{
    if (arr->count+es.count > arr->capacity) {
        if (arr->alloc.proc == nullptr) {
            arr->alloc = mem_dynamic;
        }

        i32 old_capacity = arr->capacity;
        arr->capacity = MAX(arr->count+es.count, arr->capacity*2);
        arr->data = REALLOC_ARR(arr->alloc, T, arr->data, old_capacity, arr->capacity);
    }

    memcpy(&arr->data[arr->count], es.data, es.count*sizeof(T));
    arr->count += es.count;
    return arr->count;
}


template<typename T>
i32 array_insert(DynamicArray<T> *arr, i32 insert_at, T e)
{
    ASSERT(insert_at <= arr->count);

    if (arr->count+1 > arr->capacity) {
        if (arr->alloc.proc == nullptr) arr->alloc = mem_dynamic;

        i32 old_capacity = arr->capacity;
        arr->capacity = arr->capacity == 0 ? 1 : arr->capacity*2;
        arr->data = REALLOC_ARR(arr->alloc, T, arr->data, old_capacity, arr->capacity);
    }

    for (i32 i = arr->count; i > insert_at; i--) arr->data[i] = arr->data[i-1];
    arr->data[insert_at] = e;
    arr->count++;
    return insert_at;
}

template<typename T>
i32 array_insert(DynamicArray<T> *arr, i32 insert_at, T *es, i32 count)
{
    ASSERT(insert_at <= arr->count);

    if (arr->count+count > arr->capacity) {
        if (arr->alloc.proc == nullptr) arr->alloc = mem_dynamic;

        i32 old_capacity = arr->capacity;
        arr->capacity = MAX(arr->count+count, arr->capacity*2);
        arr->data = REALLOC_ARR(arr->alloc, T, arr->data, old_capacity, arr->capacity);
    }

    for (i32 i = arr->count+count-1; i > insert_at; i--) arr->data[i] = arr->data[i-1];
    for (i32 i = 0; i < count; i++) arr->data[insert_at+i] = es[i];
    arr->count += count;
    return insert_at;
}

template<typename T>
i32 array_insert(DynamicArray<T> *arr, i32 insert_at, Array<T> es)
{
    ASSERT(insert_at >= 0);
    ASSERT(insert_at <= arr->count);

    if (arr->count+es.count > arr->capacity) {
        if (arr->alloc.proc == nullptr) arr->alloc = mem_dynamic;

        i32 old_capacity = arr->capacity;
        arr->capacity = MAX(arr->count+es.count, arr->capacity*2);
        arr->data = REALLOC_ARR(arr->alloc, T, arr->data, old_capacity, arr->capacity);
    }

    for (i32 i = arr->count+es.count-1; i > insert_at; i--) arr->data[i] = arr->data[i-1];
    for (i32 i = 0; i < es.count; i++) arr->data[insert_at+i] = es[i];
    arr->count += es.count;
    return insert_at;
}


template<typename T>
i32 array_replace_range(DynamicArray<T> *arr, i32 start, i32 end, Array<T> values)
{
    ASSERT(start >= 0);
    ASSERT(end >= 0);
    ASSERT(start < end);
    
    i32 remove_count = end-start;

    i32 old_count = arr->count;
    i32 new_count = arr->count - remove_count + values.count;
    
    if (new_count > arr->capacity) {
        if (arr->alloc.proc == nullptr) arr->alloc = mem_dynamic;

        i32 old_capacity = arr->capacity;
        arr->capacity = MAX(new_count, arr->capacity*2);
        arr->data = REALLOC_ARR(arr->alloc, T, arr->data, old_capacity, arr->capacity);
    }
    
    if (values.count > remove_count) {
        i32 move_count = new_count-1-end;
        for (i32 i = 0; i < move_count; i++) {
            arr->data[new_count-i-1] = arr->data[old_count-1-i];
        }
    }
    
    for (i32 i = 0; i < values.count; i++) arr->data[start+i] = values.data[i];
    
    if (remove_count > values.count) {
        for (i32 i = 0; i < values.count-end; i++) {
            arr->data[start+i] = arr->data[end+i];
        }
    }
         
    arr->count = new_count;
    return new_count;
}

template<typename T>
void array_remove_unsorted(Array<T> *arr, i32 index)
{
    ASSERT(index >= 0);
    ASSERT(index < arr->count);
    
    arr->data[index] = arr->data[arr->count-1];
    arr->count--;
}

template<typename T>
void array_remove_sorted(Array<T> *arr, i32 index)
{
    ASSERT(index >= 0);
    ASSERT(index < arr->count);
    
    memmove(&arr->data[index], &arr->data[index+1], (arr->count-index-1)*sizeof(T));
    arr->count--;
}

template<typename T>
Array<T> slice(Array<T> arr, i32 start, i32 end)
{
    Array<T> r;
    r.data = &arr.data[start];
    r.count = end - start;
    return r;
}

template<typename T>
Array<T> slice(Array<T> arr, i32 start)
{
    Array<T> r;
    r.data = &arr.data[start];
    r.count = arr.count - start;
    return r;
}

template<typename T>
bool array_contains(Array<T> arr, T e)
{
    for (T &t : arr) if (t == e) return true;
    return false;
}

template<typename T>
i32 array_find_index(Array<T> arr, T value)
{
    for (i32 i = 0; i < arr.count; i++) if (arr[i] == value) return i;
    return -1;
}

#endif // ARRAY_H