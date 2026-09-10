#include "PowerPC_EABI_Support/Runtime/NMWException.h"
#include "PowerPC_EABI_Support/Runtime/MWCPlusLib.h"

#pragma exceptions on

#define ARRAY_HEADER_SIZE 16

class __partial_array_destructor
{
private:
    void* p;
    volatile size_t size;
    size_t n;
    ConstructorDestructor dtor;

public:
    size_t i;

    __partial_array_destructor(void* array, size_t elementsize, size_t nelements, ConstructorDestructor destructor)
    {
        p = array;
        size = elementsize;
        n = nelements;
        dtor = destructor;
        i = n;
    }

    /**
     * Offset/Address/Size: 0x1F0 | 0x8023A3A4 | size: 0xB8
     */
    ~__partial_array_destructor()
    {
        char* ptr;

        if (i < n && dtor)
        {
            for (ptr = (char*)p + size * i; i > 0; i--)
            {
                ptr -= size;
                DTORCALL_COMPLETE(dtor, ptr);
            }
        }
    }
};

/**
 * Offset/Address/Size: 0x2A8 | 0x8023A45C | size: 0x104
 */
extern void* __construct_new_array(void* block, ConstructorDestructor ctor, ConstructorDestructor dtor, size_t size, size_t n)
{
    char* ptr;

    if ((ptr = (char*)block) != 0L)
    {
        size_t* p = (size_t*)ptr;

        p[0] = size;
        p[1] = n;
        ptr += ARRAY_HEADER_SIZE;

        if (ctor)
        {
            __partial_array_destructor pad(ptr, size, n, dtor);
            char* p;

            for (pad.i = 0, p = (char*)ptr; pad.i < n; pad.i++, p += size)
            {
                CTORCALL_COMPLETE(ctor, p);
            }
        }
    }
    return ptr;
}

static inline void __construct_array_loop(char* ptr, ConstructorDestructor ctor, size_t size, size_t n, size_t* i)
{
    for (*i = 0; *i < n; (*i)++, ptr += size)
    {
        CTORCALL_COMPLETE(ctor, ptr);
    }
}

/**
 * Offset/Address/Size: 0xF4 | 0x8023A2A8 | size: 0xFC
 */
extern void __construct_array(void* ptr, ConstructorDestructor ctor, ConstructorDestructor dtor, size_t size, size_t n)
{
    __partial_array_destructor pad(ptr, size, n, dtor);

    __construct_array_loop((char*)ptr, ctor, size, n, &pad.i);
}

/**
 * Offset/Address/Size: 0x7C | 0x8023A230 | size: 0x78
 */
extern void __destroy_arr(void* block, ConstructorDestructor* dtor, size_t size, size_t n)
{
    char* p;

    for (p = (char*)block + size * n; n > 0; n--)
    {
        p -= size;
        DTORCALL_COMPLETE(dtor, p);
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8023A1B4 | size: 0x7C
 */
extern void __destroy_new_array(void* block, ConstructorDestructor dtor)
{
    if (block)
    {
        if (dtor)
        {
            size_t i, objects, objectsize;
            char* p;

            objectsize = *(size_t*)((char*)block - ARRAY_HEADER_SIZE);
            objects = ((size_t*)((char*)block - ARRAY_HEADER_SIZE))[1];
            p = (char*)block + (objectsize * objects);

            for (i = 0; i < objects; i++)
            {
                p -= objectsize;
                DTORCALL_COMPLETE(dtor, p);
            }
        }

        ::operator delete[]((char*)block - ARRAY_HEADER_SIZE);
    }
}
