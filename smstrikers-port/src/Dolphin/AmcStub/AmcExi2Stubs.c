#include "types.h"

#include "dolphin/Amc/AmcExi2Stubs.h"

/**
 * Offset/Address/Size: 0x0 | 0x8023B7F4 | size: 0x4
 */
void EXI2_Init(vu8** buf, AmcEXICallback callback)
{
}

/**
 * Offset/Address/Size: 0x4 | 0x8023B7F8 | size: 0x4
 */
void EXI2_EnableInterrupts(void)
{
}

/**
 * Offset/Address/Size: 0x8 | 0x8023B7FC | size: 0x8
 */
int EXI2_Poll(void)
{
    return 0;
}

/**
 * Offset/Address/Size: 0x10 | 0x8023B804 | size: 0x8
 */
AmcExiError EXI2_ReadN(void* buf, u32 len)
{
    return AMC_EXI_NO_ERROR;
}

/**
 * Offset/Address/Size: 0x18 | 0x8023B80C | size: 0x8
 */
AmcExiError EXI2_WriteN(const void* buf, u32 len)
{
    return AMC_EXI_NO_ERROR;
}

/**
 * Offset/Address/Size: 0x20 | 0x8023B814 | size: 0x4
 */
void EXI2_Reserve(void)
{
}

/**
 * Offset/Address/Size: 0x24 | 0x8023B818 | size: 0x4
 */
void EXI2_Unreserve(void)
{
}

/**
 * Offset/Address/Size: 0x28 | 0x8023B81C | size: 0x8
 */
BOOL AMC_IsStub(void)
{
    return TRUE;
}
