#include "PowerPC_EABI_Support/Runtime/NMWException.h"
#include "PowerPC_EABI_Support/Runtime/__init_cpp_exceptions.h"

static int fragmentID = -2;

/**
 * Offset/Address/Size: 0x0 | 0x8023AC94 | size: 0x8
 */
static asm char* GetR2(void)
{
    // clang-format off
    nofralloc
    mr r3, r2
    blr
    // clang-format on
}

/**
 * Offset/Address/Size: 0x8 | 0x8023AC9C | size: 0x34
 */
void __fini_cpp_exceptions(void)
{
    if (fragmentID != -2)
    {
        __unregister_fragment(fragmentID);
        fragmentID = -2;
    }
}

/**
 * Offset/Address/Size: 0x3C | 0x8023ACD0 | size: 0x40
 */
void __init_cpp_exceptions(void)
{
    if (fragmentID == -2)
    {
        fragmentID = __register_fragment(_eti_init_info, GetR2());
    }
}

__declspec(section ".ctors") extern void* const __init_cpp_exceptions_reference = __init_cpp_exceptions;
__declspec(section ".dtors") extern void* const __destroy_global_chain_reference = __destroy_global_chain;
__declspec(section ".dtors") extern void* const __fini_cpp_exceptions_reference = __fini_cpp_exceptions;
