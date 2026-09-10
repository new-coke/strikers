// Metrowerks runtime helpers the decompiled sources call by name, from the decomp's runtime.c.

#include <stddef.h>
#include <stdint.h>

#include "dolphin/types.h"

// Saturating float to unsigned: fctiwz yields a signed result, so the range splits at 2^31.
u32 __cvt_fp2unsigned(f64 d)
{
    if (d < 0.0)
        return 0u;
    if (d >= 4294967296.0)
        return 0xFFFFFFFFu;
    if (d < 2147483648.0)
        return (u32)(s32)d;
    return (u32)(s32)(d - 2147483648.0) + 0x80000000u;
}

// Size and count go in a 16-byte cookie ahead of the data; __destroy_new_array reads it back.
#define ARRAY_HEADER_SIZE 16

typedef void (*CtorDtor)(void*);

void* __construct_new_array(void* block, void* ctor, void* dtor,
                            size_t size, size_t n)
{
    (void)dtor;
    char* ptr = (char*)block;
    if (ptr == NULL)
        return ptr;

    size_t* cookie = (size_t*)ptr;
    cookie[0] = size;
    cookie[1] = n;
    ptr += ARRAY_HEADER_SIZE;

    if (ctor)
    {
        CtorDtor c = (CtorDtor)ctor;
        char* p = ptr;
        for (size_t i = 0; i < n; i++, p += size)
            c(p);
    }
    return ptr;
}

void __destroy_new_array(void* block, void* dtor)
{
    if (block == NULL || dtor == NULL)
        return;
    char* base = (char*)block - ARRAY_HEADER_SIZE;
    size_t size = ((size_t*)base)[0];
    size_t n = ((size_t*)base)[1];

    // Reverse construction order.
    CtorDtor d = (CtorDtor)dtor;
    char* p = (char*)block + size * n;
    for (size_t i = 0; i < n; i++)
    {
        p -= size;
        d(p);
    }
}
