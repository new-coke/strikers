#include <dolphin.h>
#include <dolphin/ar.h>

#include "__ar.h"

#ifdef DEBUG
const char* __ARQVersion = "<< Dolphin SDK - ARQ\tdebug build: Apr  5 2004 03:56:20 (0x2301) >>";
#elif defined(VERSION_G4QP01)
const char* __ARQVersion = "<< Dolphin SDK - ARQ\trelease build: Nov 26 2003 05:19:43 (0x2301) >>";
#else
const char* __ARQVersion = "<< Dolphin SDK - ARQ\trelease build: Apr  5 2004 04:15:04 (0x2301) >>";
#endif

static ARQRequest* __ARQRequestQueueHi;
static ARQRequest* __ARQRequestTailHi;
static ARQRequest* __ARQRequestQueueLo;
static ARQRequest* __ARQRequestTailLo;
static ARQRequest* __ARQRequestQueueTemp;
static ARQRequest* __ARQRequestTailTemp;
static ARQRequest* __ARQRequestPendingHi;
static ARQRequest* __ARQRequestPendingLo;
static ARQCallback __ARQCallbackHi;
static ARQCallback __ARQCallbackLo;
static u32 __ARQChunkSize;
static BOOL __ARQ_init_flag;

void __ARQPopTaskQueueHi(void)
{
    if (__ARQRequestQueueHi)
    {
        if (__ARQRequestQueueHi->type == 0)
        {
            ARStartDMA(__ARQRequestQueueHi->type, __ARQRequestQueueHi->source, __ARQRequestQueueHi->dest, __ARQRequestQueueHi->length);
        }
        else
        {
            ARStartDMA(__ARQRequestQueueHi->type, __ARQRequestQueueHi->dest, __ARQRequestQueueHi->source, __ARQRequestQueueHi->length);
        }
        __ARQCallbackHi = __ARQRequestQueueHi->callback;
        __ARQRequestPendingHi = __ARQRequestQueueHi;
        __ARQRequestQueueHi = __ARQRequestQueueHi->next;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8023D3EC | size: 0x100
 */
void __ARQServiceQueueLo(void)
{
    if (__ARQRequestPendingLo == 0 && __ARQRequestQueueLo)
    {
        __ARQRequestPendingLo = __ARQRequestQueueLo;
        __ARQRequestQueueLo = __ARQRequestQueueLo->next;
    }

    if (__ARQRequestPendingLo)
    {
        if (__ARQRequestPendingLo->length <= __ARQChunkSize)
        {
            if (__ARQRequestPendingLo->type == 0)
            {
                ARStartDMA(__ARQRequestPendingLo->type, __ARQRequestPendingLo->source, __ARQRequestPendingLo->dest, __ARQRequestPendingLo->length);
            }
            else
            {
                ARStartDMA(__ARQRequestPendingLo->type, __ARQRequestPendingLo->dest, __ARQRequestPendingLo->source, __ARQRequestPendingLo->length);
            }
            __ARQCallbackLo = __ARQRequestPendingLo->callback;
        }
        else if (__ARQRequestPendingLo->type == 0)
        {
            ARStartDMA(__ARQRequestPendingLo->type, __ARQRequestPendingLo->source, __ARQRequestPendingLo->dest, __ARQChunkSize);
        }
        else
        {
            ARStartDMA(__ARQRequestPendingLo->type, __ARQRequestPendingLo->dest, __ARQRequestPendingLo->source, __ARQChunkSize);
        }

        __ARQRequestPendingLo->length -= __ARQChunkSize;
        __ARQRequestPendingLo->source += __ARQChunkSize;
        __ARQRequestPendingLo->dest += __ARQChunkSize;
    }
}

/**
 * Offset/Address/Size: 0x100 | 0x8023D4EC | size: 0x4
 */
void __ARQCallbackHack(u32 pointerToARQRequest)
{
}

/**
 * Offset/Address/Size: 0x104 | 0x8023D4F0 | size: 0xCC
 */
void __ARQInterruptServiceRoutine()
{
    if (__ARQCallbackHi)
    {
        __ARQCallbackHi((u32)__ARQRequestPendingHi);
        __ARQRequestPendingHi = NULL;
        __ARQCallbackHi = NULL;
    }
    else if (__ARQCallbackLo)
    {
        __ARQCallbackLo((u32)__ARQRequestPendingLo);
        __ARQRequestPendingLo = NULL;
        __ARQCallbackLo = NULL;
    }

    __ARQPopTaskQueueHi();

    if (__ARQRequestPendingHi == 0)
    {
        __ARQServiceQueueLo();
    }
}

void __ARQInitTempQueue(void)
{
    __ARQRequestQueueTemp = NULL;
    __ARQRequestTailTemp = NULL;
}

void __ARQPushTempQueue(ARQRequest* task)
{
    if (!__ARQRequestQueueTemp)
    {
        __ARQRequestQueueTemp = task;
        __ARQRequestTailTemp = task;
    }
    else
    {
        __ARQRequestTailTemp->next = task;
        __ARQRequestTailTemp = task;
    }
}

/**
 * Offset/Address/Size: 0x1D0 | 0x8023D5BC | size: 0x70
 */
void ARQInit(void)
{
    if (__ARQ_init_flag != TRUE)
    {
        OSRegisterVersion(__ARQVersion);

        __ARQRequestQueueHi = __ARQRequestQueueLo = NULL;
        __ARQChunkSize = 0x1000;
        ARRegisterDMACallback(__ARQInterruptServiceRoutine);
        __ARQRequestPendingHi = NULL;
        __ARQRequestPendingLo = NULL;
        __ARQCallbackHi = NULL;
        __ARQCallbackLo = NULL;
        __ARQ_init_flag = TRUE;
    }
}

/**
 * Offset/Address/Size: 0x240 | 0x8023D62C | size: 0xC
 */
void ARQReset(void)
{
    __ARQ_init_flag = FALSE;
}

/**
 * Offset/Address/Size: 0x24C | 0x8023D638 | size: 0x15C
 */
void ARQPostRequest(ARQRequest* request, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length, ARQCallback callback)
{
    BOOL level;

    ASSERTLINE(437, request);
    ASSERTLINE(438, (type == ARQ_TYPE_MRAM_TO_ARAM) || (type == ARQ_TYPE_ARAM_TO_MRAM));
    ASSERTLINE(439, (priority == ARQ_PRIORITY_LOW) || (priority == ARQ_PRIORITY_HIGH));
    ASSERTLINE(442, (length % ARQ_DMA_ALIGNMENT) == 0);

    request->next = NULL;
    request->owner = owner;
    request->type = type;
    request->source = source;
    request->dest = dest;
    request->length = length;
    if (callback)
    {
        request->callback = callback;
    }
    else
    {
        request->callback = __ARQCallbackHack;
    }

    level = OSDisableInterrupts();
    switch (priority)
    {
    case ARQ_PRIORITY_LOW:
        if (__ARQRequestQueueLo)
        {
            __ARQRequestTailLo->next = request;
        }
        else
        {
            __ARQRequestQueueLo = request;
        }
        __ARQRequestTailLo = request;
        break;
    case ARQ_PRIORITY_HIGH:
        if (__ARQRequestQueueHi)
        {
            __ARQRequestTailHi->next = request;
        }
        else
        {
            __ARQRequestQueueHi = request;
        }
        __ARQRequestTailHi = request;
        break;
    }

    if ((__ARQRequestPendingHi == 0) && (__ARQRequestPendingLo == 0))
    {
        __ARQPopTaskQueueHi();
        if (__ARQRequestPendingHi == 0)
        {
            __ARQServiceQueueLo();
        }
    }

    OSRestoreInterrupts(level);
}

void ARQRemoveRequest(ARQRequest* request)
{
    ARQRequest* thisRequest;
    BOOL level;

    level = OSDisableInterrupts();
    __ARQInitTempQueue();

    for (thisRequest = __ARQRequestQueueHi; thisRequest; thisRequest = thisRequest->next)
    {
        if (thisRequest != request)
        {
            __ARQPushTempQueue(thisRequest);
        }
    }

    __ARQRequestQueueHi = __ARQRequestQueueTemp;
    __ARQRequestTailHi = __ARQRequestTailTemp;
    __ARQInitTempQueue();

    for (thisRequest = __ARQRequestQueueLo; thisRequest; thisRequest = thisRequest->next)
    {
        if (thisRequest != request)
        {
            __ARQPushTempQueue(thisRequest);
        }
    }

    __ARQRequestQueueLo = __ARQRequestQueueTemp;
    __ARQRequestTailLo = __ARQRequestTailTemp;
    OSRestoreInterrupts(level);
}

void ARQRemoveOwnerRequest(u32 owner)
{
    ARQRequest* thisRequest;
    BOOL level;

    level = OSDisableInterrupts();
    __ARQInitTempQueue();

    for (thisRequest = __ARQRequestQueueHi; thisRequest; thisRequest = thisRequest->next)
    {
        if (thisRequest->owner != owner)
        {
            __ARQPushTempQueue(thisRequest);
        }
    }

    __ARQRequestQueueHi = __ARQRequestQueueTemp;
    __ARQRequestTailHi = __ARQRequestTailTemp;
    __ARQInitTempQueue();

    for (thisRequest = __ARQRequestQueueLo; thisRequest; thisRequest = thisRequest->next)
    {
        if (thisRequest->owner != owner)
        {
            __ARQPushTempQueue(thisRequest);
        }
    }

    __ARQRequestQueueLo = __ARQRequestQueueTemp;
    __ARQRequestTailLo = __ARQRequestTailTemp;
    OSRestoreInterrupts(level);
}

void ARQFlushQueue(void)
{
    BOOL level;

    level = OSDisableInterrupts();
    __ARQRequestQueueHi = NULL;
    __ARQRequestTailHi = NULL;
    __ARQRequestQueueLo = NULL;
    __ARQRequestTailLo = NULL;

    OSRestoreInterrupts(level);
}

/**
 * Offset/Address/Size: 0x3A8 | 0x8023D794 | size: 0x20
 */
void ARQSetChunkSize(u32 size)
{
    u32 i;

    i = size & 0x1F;
    if (i)
    {
        __ARQChunkSize = size + (0x20 - i);
        return;
    }
    __ARQChunkSize = size;
}

/**
 * Offset/Address/Size: 0x3C8 | 0x8023D7B4 | size: 0x8
 */
u32 ARQGetChunkSize(void)
{
    return __ARQChunkSize;
}

BOOL ARQCheckInit(void)
{
    return __ARQ_init_flag;
}
