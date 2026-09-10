#include "PowerPC_EABI_Support/Runtime/NMWException.h"

#define MAXFRAGMENTS 1

typedef struct ProcessInfo
{
    __eti_init_info* exception_info;
    char* TOC;
    int active;
} ProcessInfo;

static ProcessInfo fragmentinfo[MAXFRAGMENTS];

/**
 * Offset/Address/Size: 0x0 | 0x8023AD10 | size: 0x34
 */
void __unregister_fragment(int fragmentID)
{
    ProcessInfo* f;

    if (fragmentID >= 0 && fragmentID < MAXFRAGMENTS)
    {
        f = &fragmentinfo[fragmentID];
        f->exception_info = 0;
        f->TOC = 0;
        f->active = 0;
    }
}

/**
 * Offset/Address/Size: 0x34 | 0x8023AD44 | size: 0x34
 */
int __register_fragment(struct __eti_init_info* info, char* TOC)
{
    ProcessInfo* f;
    int i;

    for (i = 0, f = fragmentinfo; i < MAXFRAGMENTS; ++i, ++f)
    {
        if (f->active == 0)
        {
            f->exception_info = info;
            f->TOC = TOC;
            f->active = 1;
            return i;
        }
    }

    return -1;
}
