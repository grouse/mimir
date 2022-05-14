#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "memory.h"
#include "hash.h"

#define HASH_TABLE_INITIAL_CAPACITY 16
#define HASH_TABLE_LOAD_FACTOR 0.5

template<typename K, typename V>
struct HashTable {
    struct Pair {
        K key;
        V value;
        bool occupied;
    };

    Pair *slots;
    i32 count;
    i32 capacity;
    Allocator alloc;
};

template<typename K, typename V>
void set(HashTable<K, V> *table, K key, V value);

template<typename K, typename V>
i32 find_slot(HashTable<K, V> *table, K key)
{
    // TODO(jesper): round robin to reduce the upper bound of the probing
    if (table->capacity == 0) return -1;
    i32 i = hash32(key) % table->capacity;
    i32 end_probe = i;
    while (table->slots[i].occupied && table->slots[i].key != key) {
        i = (i+1) % table->capacity;
        if (i == end_probe) return -1;
    }

    return i;
}

template<typename K, typename V>
V* find(HashTable<K, V> *table, K key)
{
    i32 i = find_slot(table, key);
    if (i == -1 || !table->slots[i].occupied) return nullptr;
    return &table->slots[i].value;
}

template<typename K, typename V>
void grow_table(HashTable<K, V> *table, i32 new_capacity)
{
    ASSERT(new_capacity > table->capacity);
    if (table->alloc.proc == nullptr) table->alloc = mem_dynamic;

    using Pair = typename HashTable<K,V>::Pair;
    HashTable<K, V> old_table = *table;

    table->slots = ALLOC_ARR(table->alloc, Pair, new_capacity);
    for (i32 i = 0; i < new_capacity; i++) table->slots[i].occupied = false;
    table->capacity = new_capacity;
    table->count = 0;

    for (i32 i = 0; i < old_table.capacity; i++) {
        if (old_table.slots[i].occupied) {
            set(table, old_table.slots[i].key, old_table.slots[i].value);
        }
    }

    if (old_table.slots) FREE(table->alloc, old_table.slots);
}

template<typename K, typename V>
void set(HashTable<K, V> *table, K key, V value)
{
    i32 i = find_slot(table, key);
    if (i >= 0 && table->slots[i].occupied) {
        table->slots[i].value = value;
        return;
    }

    if (table->count >= table->capacity*HASH_TABLE_LOAD_FACTOR) {
        i32 new_capacity = table->capacity == 0 ? HASH_TABLE_INITIAL_CAPACITY : table->capacity*2;
        grow_table(table, new_capacity);
    }

    i = find_slot(table, key);
    ASSERT(i >= 0);

    table->slots[i] = { .key = key, .value = value, .occupied = true };
    table->count++;
}

template<typename V>
V* find(HashTable<String, V> *table, String key)
{
    i32 i = find_slot(table, key);
    if (i == -1 || !table->slots[i].occupied) return nullptr;
    return &table->slots[i].value;
}

template<typename V>
void set(HashTable<String, V> *table, String key, V value)
{
    i32 i = find_slot(table, key);
    if (i >= 0 && table->slots[i].occupied) {
        table->slots[i].value = value;
        return;
    }

    if (table->count >= table->capacity*HASH_TABLE_LOAD_FACTOR) {
        i32 new_capacity = table->capacity == 0 ? HASH_TABLE_INITIAL_CAPACITY : table->capacity*2;
        grow_table(table, new_capacity);
    }

    i = find_slot(table, key);
    ASSERT(i >= 0);

    table->slots[i] = { .key = duplicate_string(key, table->alloc), .value = value, .occupied = true };
    table->count++;
}



#endif // HASH_TABLE_H
