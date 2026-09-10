#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/nubevent.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/mutex_TRK.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msgbuf.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/mem_TRK.h"

TRKEventQueue gTRKEventQueue;

/**
 * Offset/Address/Size: 0x1D0 | 0x80224B88 | size: 0x58
 */
DSError TRKInitializeEventQueue()
{
    TRKInitializeMutex(&gTRKEventQueue);
    TRKAcquireMutex(&gTRKEventQueue);
    gTRKEventQueue.count = 0;
    gTRKEventQueue.next = 0;
    gTRKEventQueue.eventID = 0x100;
    TRKReleaseMutex(&gTRKEventQueue);
    return DS_NoError;
}

inline void TRKCopyEvent(TRKEvent* dstEvent, const TRKEvent* srcEvent)
{
    TRK_memcpy(dstEvent, srcEvent, sizeof(TRKEvent));
}

/**
 * Offset/Address/Size: 0x11C | 0x80224AD4 | size: 0xB4
 */
BOOL TRKGetNextEvent(TRKEvent* event)
{
    BOOL status = 0;
    TRKAcquireMutex(&gTRKEventQueue);
    if (0 < gTRKEventQueue.count)
    {
        TRKCopyEvent(event, &gTRKEventQueue.events[gTRKEventQueue.next]);
        gTRKEventQueue.count--;
        gTRKEventQueue.next++;
        if (gTRKEventQueue.next == 2)
            gTRKEventQueue.next = 0;

        status = 1;
    }
    TRKReleaseMutex(&gTRKEventQueue);
    return status;
}

/**
 * Offset/Address/Size: 0x3C | 0x802249F4 | size: 0xE0
 */
DSError TRKPostEvent(TRKEvent* event)
{
    DSError ret = DS_NoError;
    int nextEventID;

    TRKAcquireMutex(&gTRKEventQueue);

    if (gTRKEventQueue.count == 2)
    {
        ret = DS_EventQueueFull;
    }
    else
    {
        nextEventID = (gTRKEventQueue.next + gTRKEventQueue.count) % 2;
        TRKCopyEvent(&gTRKEventQueue.events[nextEventID], event);
        gTRKEventQueue.events[nextEventID].eventID = gTRKEventQueue.eventID;
        gTRKEventQueue.eventID++;
        if (gTRKEventQueue.eventID < 0x100)
            gTRKEventQueue.eventID = 0x100;

        gTRKEventQueue.count++;
    }

    TRKReleaseMutex(&gTRKEventQueue);
    return ret;
}

/**
 * Offset/Address/Size: 0x24 | 0x802249DC | size: 0x18
 */
void TRKConstructEvent(TRKEvent* event, /*NubEventType*/ u32 eventType)
{
    event->eventType = eventType;
    event->eventID = 0;
    event->msgBufID = -1;
}

/**
 * Offset/Address/Size: 0x0 | 0x802249B8 | size: 0x24
 */
void TRKDestructEvent(TRKEvent* event)
{
    TRKReleaseBuffer(event->msgBufID);
}
