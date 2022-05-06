#ifndef HASH_H
#define HASH_H

#include "external/MurmurHash/MurmurHash3.h"

#define MURMUR3_SEED (0xdeadbeef)

u32 hash32(u32 value)
{
    u32 hashed;
    MurmurHash3_x86_32(&value, sizeof value, MURMUR3_SEED, &hashed);
    return hashed;
}

u32 hash32(String str)
{
    u32 hashed;
    MurmurHash3_x86_32(str.data, str.length, MURMUR3_SEED, &hashed);
    return hashed;
}

#endif // HASH_H
