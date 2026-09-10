#include "PowerPC_EABI_Support/MetroTRK/trk.h"

void TRKWaitForACK(void)
{
    // UNUSED FUNCTION
}

/**
 * Offset/Address/Size: 0x0 | 0x80227250 | size: 0x98
 */
DSError TRKDoNotifyStopped(MessageCommandID cmd)
{
    int reqIdx;
    int bufIdx;
    TRKBuffer* msg;
    DSError err;
    DSError bufError;

    bufError = TRKGetFreeBuffer(&bufIdx, &msg);
    if ((err = bufError) == FALSE)
    {
        if (err == DS_NoError)
        {
            if (cmd == DSMSG_NotifyStopped)
            {
                TRKTargetAddStopInfo(msg);
            }
            else
            {
                TRKTargetAddExceptionInfo(msg);
            }
        }
        bufError = TRKRequestSend(msg, &reqIdx, 2, 3, 1);
        err = bufError;
        if (err == DS_NoError)
        {
            TRKReleaseBuffer(reqIdx);
        }
        TRKReleaseBuffer(bufIdx);
    }
    return err;
}
