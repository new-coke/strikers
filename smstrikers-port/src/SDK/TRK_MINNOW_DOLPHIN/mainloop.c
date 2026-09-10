#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/serpoll.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msgbuf.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/dispatch.h"
#include "TRK_MINNOW_DOLPHIN/Os/dolphin/targcont.h"
#include "TRK_MINNOW_DOLPHIN/ppc/Generic/targimpl.h"

inline void TRKHandleRequestEvent(TRKEvent* event)
{
    TRKBuffer* buffer = TRKGetBuffer(event->msgBufID);
    TRKDispatchMessage(buffer);
}

inline void TRKHandleSupportEvent(TRKEvent* event)
{
    TRKTargetSupportRequest();
}

inline void TRKIdle()
{
    if (TRKTargetStopped() == FALSE)
    {
        TRKTargetContinue();
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x802248C0 | size: 0xF8
 */
void TRKNubMainLoop(void)
{
    void* msg;
    TRKEvent event;
    BOOL isShutdownRequested;
    BOOL isNewInput;

    isShutdownRequested = FALSE;
    isNewInput = FALSE;
    while (isShutdownRequested == FALSE)
    {
        if (TRKGetNextEvent(&event) != FALSE)
        {

            isNewInput = FALSE;

            switch ((u32)event.eventType)
            {
            case NUBEVENT_Null:
                break;

            case NUBEVENT_Request:
                TRKHandleRequestEvent(&event);
                break;

            case NUBEVENT_Shutdown:
                isShutdownRequested = TRUE;
                break;

            case NUBEVENT_Breakpoint:
            case NUBEVENT_Exception:
                TRKTargetInterrupt(&event);
                break;

            case NUBEVENT_Support:
                TRKHandleSupportEvent(&event);
                break;
            }

            TRKDestructEvent(&event);
            continue;
        }

        if ((isNewInput == FALSE) || (*(u8*)gTRKInputPendingPtr != '\0'))
        {
            isNewInput = TRUE;
            TRKGetInput();
            continue;
        }

        TRKIdle();
        isNewInput = FALSE;
    }
}
