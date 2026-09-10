#include "PowerPC_EABI_Support/MetroTRK/trk.h"
#include "PowerPC_EABI_Support/MetroTRK/custconn/CircleBuffer.h"
#include "dolphin/Amc/AmcExi2Stubs.h"
#include "dolphin/db.h"
#include "types.h"

#define GDEV_BUF_SIZE (0x500)

static CircleBuffer gRecvCB;
static u8 gRecvBuf[GDEV_BUF_SIZE];

static int gLastError;
static BOOL gIsInitialized = FALSE;

static BOOL IsInitialized(void)
{
    return gIsInitialized;
}

/**
 * Offset/Address/Size: 0x2C4 | 0x8022AE34 | size: 0x88
 */
int gdev_cc_initialize(void* flagOut, __OSInterruptHandler handler)
{
    MWTRACE(1, "CALLING EXI2_Init\n");
    DBInitComm(flagOut, (int*)handler);
    MWTRACE(1, "DONE CALLING EXI2_Init\n");
    CircleBufferInitialize(&gRecvCB, gRecvBuf, GDEV_BUF_SIZE);
    return 0;
}

/**
 * Offset/Address/Size: 0x2BC | 0x8022AE2C | size: 0x8
 */
int gdev_cc_shutdown()
{
    return 0;
}

/**
 * Offset/Address/Size: 0x298 | 0x8022AE08 | size: 0x24
 */
int gdev_cc_open()
{
    if (gIsInitialized)
    {
        return -0x2715;
    }

    gIsInitialized = TRUE;
    return 0;
}

/**
 * Offset/Address/Size: 0x290 | 0x8022AE00 | size: 0x8
 */
int gdev_cc_close()
{
    return 0;
}

/**
 * Offset/Address/Size: 0x19C | 0x8022AD0C | size: 0xF4
 */
int gdev_cc_read(u8* data, int size)
{
    u8 buff[GDEV_BUF_SIZE];
    int p1;
    u32 retval;
    int p2;
    int poll;
    retval = 0;
    if (!gIsInitialized)
    {
        return -0x2711;
    }

    MWTRACE(1, "Expected packet size : 0x%08x (%ld)\n", size, size);

    p1 = size;
    p2 = size;
    while ((u32)CBGetBytesAvailableForRead(&gRecvCB) < p2)
    {
        retval = 0;
        poll = DBQueryData();
        if (poll != 0)
        {
            retval = DBRead(buff, p2);
            if (retval == 0)
            {
                CircleBufferWriteBytes(&gRecvCB, buff, poll);
            }
        }
    }

    if (retval == 0)
    {
        CircleBufferReadBytes(&gRecvCB, data, p1);
    }
    else
    {
        MWTRACE(8, "cc_read : error reading bytes from EXI2 %ld\n", retval);
    }

    return retval;
}

/**
 * Offset/Address/Size: 0xDC | 0x8022AC4C | size: 0xC0
 */
int gdev_cc_write(const u8* bytes, int length)
{
    int exi2Len;
    int n_copy;
    u32 hexCopy;

    hexCopy = (u32)bytes;
    n_copy = length;

    if (gIsInitialized == FALSE)
    {
        MWTRACE(8, "cc not initialized\n");
        return -0x2711;
    }

    MWTRACE(8, "cc_write : Output data 0x%08x %ld bytes\n", bytes, length);

    while (n_copy > 0)
    {
        MWTRACE(1, "cc_write sending %ld bytes\n", n_copy);
        exi2Len = DBWrite((const void*)hexCopy, n_copy);
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
 * Offset/Address/Size: 0xB8 | 0x8022AC28 | size: 0x24
 */
int gdev_cc_pre_continue()
{
    DBClose();
    return 0;
}

/**
 * Offset/Address/Size: 0x94 | 0x8022AC04 | size: 0x24
 */
int gdev_cc_post_stop()
{
    DBOpen();
    return 0;
}

/**
 * Offset/Address/Size: 0x24 | 0x8022AB94 | size: 0x70
 */
int gdev_cc_peek()
{
    int poll;
    u8 buff[GDEV_BUF_SIZE];

    poll = DBQueryData();
    if (poll <= 0)
    {
        return 0;
    }

    if ((int)DBRead(buff, poll) == 0)
    {
        CircleBufferWriteBytes(&gRecvCB, buff, poll);
    }
    else
    {
        return -0x2719;
    }

    return poll;
}

/**
 * Offset/Address/Size: 0x0 | 0x8022AB70 | size: 0x24
 */
int gdev_cc_initinterrupts()
{
    DBInitInterrupts();
    return 0;
}
