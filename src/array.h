#ifndef ARRAY_H
#define ARRAY_H

#include "core.h"
#include "memory.h"

#include <initializer_list>

// In honour of Dirk
#define ANON_ARRAY(fields) struct anon_##__LINE__ { fields; }; Array<anon_##__LINE__>

template<typename T>
struct Array {
    T *data = nullptr;
    i32 count = 0;


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
    i32 capacity = 0;
    Allocator alloc = {};
};

template<typename T>
void array_resize(DynamicArray<T> *arr, i32 count)
{
    if (!arr->alloc.proc) arr->alloc = mem_dynamic;

    if (arr->capacity < count) {
        arr->data = REALLOC_ARR(arr->alloc, T, arr->data, arr->capacity, count);
        arr->capacity = count;
    }

    arr->count = count;
}

template<typename T>
void array_create(Array<T> *arr, i32 count, Allocator mem = mem_dynamic)
{
    T *nptr = ALLOC_ARR(mem, T, count);
    if (arr->count > 0) {
        for (i32 i = 0; i < arr->count; i++) nptr[i] = arr->data[i];
    }

    arr->data = nptr;
    arr->count = count;
}

template<typename T>
void array_reserve(DynamicArray<T> *arr, i32 capacity)
{
    if (arr->alloc.proc == nullptr) arr->alloc = mem_dynamic;

    arr->data = ALLOC_ARR(arr->alloc, T, capacity);
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
void array_copy(DynamicArray<T> *dst, Array<T> src)
{
    array_resize(dst, src.count);
    for (i32 i = 0; i < src.count; i++) dst->data[i] = src.data[i];
}

template<typename T>
DynamicArray<T> array_duplicate(Array<T> arr, Allocator mem)
{
    DynamicArray<T> result{ .alloc = mem };
    array_copy(&result, arr);
    return result;
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
        if (arr->alloc.proc == nullptr) arr->alloc = mem_dynamic;

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
        if (arr->alloc.proc == nullptr) arr->alloc = mem_dynamic;

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
        if (arr->alloc.proc == nullptr) arr->alloc = mem_dynamic;

        i32 old_capacity = arr->capacity;
        arr->capacity = MAX(arr->count+es.count, arr->capacity*2);
        arr->data = REALLOC_ARR(arr->alloc, T, arr->data, old_capacity, arr->capacity);
    }

    memcpy(&arr->data[arr->count], es.data, es.count*sizeof(T));
    arr->count += es.count;
    return arr->count;
}

template<typename T>
i32 array_add(DynamicArray<T> *arr, std::initializer_list<T> list)
{
    Array<T> l{ .data = (T*)list.begin(), .count = (i32)list.size() };
    return array_add(arr, l);
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
i32 array_replace(DynamicArray<T> *arr, i32 start, i32 end, Array<T> values)
{
    ASSERT(start >= 0);
    ASSERT(end >= 0);

    i32 remove_count = end-start;

    i32 old_count = arr->count;
    i32 new_count = arr->count - remove_count + values.count;

    if (new_count > arr->capacity) {
        if (arr->alloc.proc == nullptr) arr->alloc = mem_dynamic;

        i32 old_capacity = arr->capacity;
        arr->capacity = MAX(new_count, arr->capacity*2);
        arr->data = REALLOC_ARR(arr->alloc, T, arr->data, old_capacity, arr->capacity);
    }

    if (remove_count > 0 && values.count > remove_count) {
        i32 move_count = new_count-end-1;
        for (i32 i = 0; i < move_count; i++) arr->data[new_count-i-1] = arr->data[old_count-i-1];
    }

    for (i32 i = 0; i < values.count; i++) arr->data[start+i] = values.data[i];

    if (remove_count > values.count) {
        for (i32 i = start+values.count; i < new_count; i++) arr->data[i] = arr->data[i+1];
    }

    arr->count = new_count;
    return new_count;
}

template<typename T>
i32 array_replace(DynamicArray<T> *arr, i32 start, i32 end, std::initializer_list<T> values)
{
    return array_replace(arr, start, end, { .data = (T*)values.begin(), .count = (i32)values.size() });
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
void array_remove(Array<T> *arr, i32 index)
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

template<typename T>
bool array_equals(Array<T> lhs, Array<T> rhs)
{
    if (lhs.count != rhs.count) return false;
    for (i32 i = 0; i < lhs.count; i++) {
        if (lhs[i] != rhs[i]) return false;
    }
    return true;
}

template<typename T>
bool array_equals(Array<T> lhs, std::initializer_list<T> rhs)
{
    return array_equals(lhs, { .data = (T*)rhs.begin(), .count = (i32)rhs.size() });
}


template<typename T>
void array_swap(Array<T> arr, i32 a, i32 b)
{
    SWAP(arr[a], arr[b]);
}

template<typename T, typename K>
void array_swap(Array<T> arr, i32 a, i32 b, Array<K> arr1)
{
    SWAP(arr[a], arr[b]);
    SWAP(arr1[a], arr1[b]);
}

template<typename T, typename K, typename... Tail>
void array_swap(Array<T> arr, i32 a, i32 b, Array<K> arr1, Array<Tail>... tail)
{
    SWAP(arr[a], arr[b]);
    array_swap(arr1, a, b, tail...);
}


template<typename T, typename... Tail>
void exchange_sort(Array<T> arr, Array<Tail>... tail)
{
    for (int i = 0; i < arr.count; i++) {
        for (int j = i+1; j < arr.count; j++) {
            if (arr[i] > arr[j]) array_swap(arr, i, j, tail...);
        }
    }

}

template<typename T, typename... Tail>
void quick_sort_desc(Array<T> arr, i32 l, i32 r, Array<Tail>... tail)
{
    if (l < 0 || r < 0 || l >= r) return;

    T pivot = arr[(r+l)/2];

    i32 i = l-1;
    i32 j = r+1;

    i32 pi = -1;
    while (true) {
        do i += 1; while(arr[i] > pivot);
        do j -= 1; while(arr[j] < pivot);

        if (i >= j) {
            pi = j;
            break;
        }

        array_swap(arr, j, i, tail...);
    }

    ASSERT(pi >= 0);

    quick_sort_desc(arr, l, pi, tail...);
    quick_sort_desc(arr, pi+1, r, tail...);
}

template<typename T, typename... Tail>
void quick_sort_asc(Array<T> arr, i32 l, i32 r, Array<Tail>... tail)
{
    if (l < 0 || r < 0 || l >= r) return;

    T pivot = arr[(r+l)/2];

    i32 i = l-1;
    i32 j = r+1;

    i32 pi = -1;
    while (true) {
        do i += 1; while(arr[i] < pivot);
        do j -= 1; while(arr[j] > pivot);

        if (i >= j) {
            pi = j;
            break;
        }

        array_swap(arr, i, j, tail...);
    }

    ASSERT(pi >= 0);

    quick_sort_asc(arr, l, pi, tail...);
    quick_sort_asc(arr, pi+1, r, tail...);
}

template<typename T, typename... Tail>
void quick_sort_desc(Array<T> arr, Array<Tail>... tail)
{
    quick_sort_desc(arr, 0, arr.count-1, tail...);
}

template<typename T, typename... Tail>
void quick_sort_asc(Array<T> arr, Array<Tail>... tail)
{
    quick_sort_asc(arr, 0, arr.count-1, tail...);
}




#endif // ARRAY_H
