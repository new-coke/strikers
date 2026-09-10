#include "PowerPC_EABI_Support/MetroTRK/trk.h"
#include "PowerPC_EABI_Support/MetroTRK/custconn/CircleBuffer.h"
#include "dolphin/Amc/AmcExi2Stubs.h"
#include "dolphin/os.h"

#define DDH_ERR_NOT_INITIALIZED     -0x2711
#define DDH_ERR_ALREADY_INITIALIZED -0x2715
#define DDH_ERR_READ_ERROR          -0x2719

#define DDH_BUF_SIZE (0x800)

static CircleBuffer gRecvCB;
static u8 gRecvBuf[DDH_BUF_SIZE];

static int gLastError;
static BOOL gIsInitialized = FALSE;

static BOOL IsInitialized(void)
{
    return gIsInitialized;
}

/**
 * Offset/Address/Size: 0x2BC | 0x8022A880 | size: 0x88
 */
int ddh_cc_initialize(void* flagOut, __OSInterruptHandler handler)
{
    MWTRACE(1, "CALLING EXI2_Init\n");
    EXI2_Init(flagOut, handler);
    MWTRACE(1, "DONE CALLING EXI2_Init\n");
    CircleBufferInitialize(&gRecvCB, gRecvBuf, DDH_BUF_SIZE);
    return 0;
}

/**
 * Offset/Address/Size: 0x2B4 | 0x8022A878 | size: 0x8
 */
int ddh_cc_shutdown()
{
    return 0;
}

/**
 * Offset/Address/Size: 0x290 | 0x8022A854 | size: 0x24
 */
int ddh_cc_open()
{
    if (gIsInitialized != 0)
    {
        return DDH_ERR_ALREADY_INITIALIZED;
    }

    gIsInitialized = TRUE;
    return 0;
}

/**
 * Offset/Address/Size: 0x288 | 0x8022A84C | size: 0x8
 */
int ddh_cc_close()
{
    return 0;
}

/**
 * Offset/Address/Size: 0x19C | 0x8022A760 | size: 0xEC
 */
int ddh_cc_read(u8* data, int size)
{
    u8 buff[DDH_BUF_SIZE];
    int originalDataSize;
    u32 result;
    int expectedDataSize;
    int poll;

    result = 0;
    if (!gIsInitialized)
    {
        return DDH_ERR_NOT_INITIALIZED;
    }

    MWTRACE(1, "Expected packet size : 0x%08x (%ld)\n", size, size);

    originalDataSize = expectedDataSize = size;
    while ((u32)CBGetBytesAvailableForRead(&gRecvCB) < expectedDataSize)
    {
        result = 0;

        poll = EXI2_Poll();
        if (poll != 0)
        {
            result = EXI2_ReadN(buff, poll);
            if (result == 0)
            {
                CircleBufferWriteBytes(&gRecvCB, buff, poll);
            }
        }
    }

    if (result == 0)
    {
        CircleBufferReadBytes(&gRecvCB, data, originalDataSize);
    }
    else
    {
        MWTRACE(8, "cc_read : error reading bytes from EXI2 %ld\n", result);
    }

    return result;
}

/**
 * Offset/Address/Size: 0xDC | 0x8022A6A0 | size: 0xC0
 */
int ddh_cc_write(const u8* bytes, int length)
{
    int exi2Len;
    int n_copy;
    u32 hexCopy;

    hexCopy = (u32)bytes;
    n_copy = length;

    if (gIsInitialized == FALSE)
    {
        MWTRACE(8, "cc not initialized\n");
        return DDH_ERR_NOT_INITIALIZED;
    }

    MWTRACE(8, "cc_write : Output data 0x%08x %ld bytes\n", bytes, length);

    while (n_copy > 0)
    {
        MWTRACE(1, "cc_write sending %ld bytes\n", n_copy);
        exi2Len = EXI2_WriteN((const void*)hexCopy, n_copy);
        if (exi2Len == AMC_EXI_NO_ERROR)
        {
            break;
        }
        hexCopy += exi2Len;
        n_copy -= exi2Len;
    }

    return 0;
}

/**
 * Offset/Address/Size: 0xB8 | 0x8022A67C | size: 0x24
 */
int ddh_cc_pre_continue()
{
    EXI2_Unreserve();
    return 0;
}

/**
 * Offset/Address/Size: 0x94 | 0x8022A658 | size: 0x24
 */
int ddh_cc_post_stop()
{
    EXI2_Reserve();
    return 0;
}

/**
 * Offset/Address/Size: 0x24 | 0x8022A5E8 | size: 0x70
 */
int ddh_cc_peek()
{
    int poll;
    u8 buff[DDH_BUF_SIZE];

    poll = EXI2_Poll();
    if (poll <= 0)
    {
        return 0;
    }

    if (EXI2_ReadN(buff, poll) == 0)
    {
        CircleBufferWriteBytes(&gRecvCB, buff, poll);
    }
    else
    {
        return DDH_ERR_READ_ERROR;
    }

    return poll;
}

/**
 * Offset/Address/Size: 0x0 | 0x8022A5C4 | size: 0x24
 */
int ddh_cc_initinterrupts()
{
    EXI2_EnableInterrupts();
    return 0;
}
