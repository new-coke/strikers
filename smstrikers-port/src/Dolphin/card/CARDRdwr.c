#include "__card.h"

// prototypes
static void BlockReadCallback(s32 chan, s32 result);
static void BlockWriteCallback(s32 chan, s32 result);

/**
 * Offset/Address/Size: 0x0 | 0x8023FF58 | size: 0xDC
 */
static void BlockReadCallback(s32 chan, s32 result)
{
    CARDControl* card;
    CARDCallback callback;

    card = &__CARDBlock[chan];

    if (result >= 0)
    {
        card->xferred += CARD_SEG_SIZE;
        card->addr += CARD_SEG_SIZE;
        ((u8*)card->buffer) += CARD_SEG_SIZE;

        if (--card->repeat > 0)
        {
            result = __CARDReadSegment(chan, BlockReadCallback);
            if (result >= 0)
            {
                return;
            }
        }
    }

    if (!card->apiCallback)
    {
        __CARDPutControlBlock(card, result);
    }

    callback = card->xferCallback;
    if (callback)
    {
        card->xferCallback = NULL;
        callback(chan, result);
    }
}

/**
 * Offset/Address/Size: 0xDC | 0x80240034 | size: 0x64
 */
s32 __CARDRead(s32 chan, u32 addr, s32 length, void* dst, CARDCallback callback)
{
    CARDControl* card;

    ASSERTLINE(91, 0 < length && length % CARD_SEG_SIZE == 0);
    ASSERTLINE(92, 0 <= chan && chan < 2);

    card = &__CARDBlock[chan];
    if (card->attached == 0)
    {
        return CARD_RESULT_NOCARD;
    }
    card->xferCallback = callback;
    card->repeat = (length / CARD_SEG_SIZE);
    card->addr = addr;
    card->buffer = dst;
    return __CARDReadSegment(chan, BlockReadCallback);
}

/**
 * Offset/Address/Size: 0x140 | 0x80240098 | size: 0xE8
 */
static void BlockWriteCallback(s32 chan, s32 result)
{
#if defined(VERSION_G4QP01)
    CARDControl* card;
    CARDCallback callback;

    card = &__CARDBlock[chan];
    if (result < 0)
    {
        goto error;
    }

    card->xferred += CARD_PAGE_SIZE;
    card->addr += CARD_PAGE_SIZE;
    (u8*)card->buffer += CARD_PAGE_SIZE;
    if (--card->repeat <= 0)
    {
        goto error;
    }

    result = __CARDWritePage(chan, BlockWriteCallback);
    if (result < 0)
    {
        goto error;
    }
    return;

error:
    if (card->apiCallback == NULL)
    {
        __CARDPutControlBlock(card, result);
    }
    callback = card->xferCallback;
    if (callback)
    {
        card->xferCallback = NULL;
        callback(chan, result);
    }
#else
    CARDControl* card;
    CARDCallback callback;

    card = &__CARDBlock[chan];
    if (result >= 0)
    {
        card->xferred += card->pageSize;
        card->addr += card->pageSize;
        ((u8*)card->buffer) += card->pageSize;

        if (--card->repeat > 0)
        {
            result = __CARDWritePage(chan, BlockWriteCallback);
            if (result >= 0)
            {
                return;
            }
        }
    }

    if (!card->apiCallback)
    {
        __CARDPutControlBlock(card, result);
    }

    callback = card->xferCallback;
    if (callback)
    {
        card->xferCallback = NULL;
        callback(chan, result);
    }
#endif
}

/**
 * Offset/Address/Size: 0x228 | 0x80240180 | size: 0x68
 */
s32 __CARDWrite(s32 chan, u32 addr, s32 length, void* dst, CARDCallback callback)
{
    CARDControl* card;
    card = &__CARDBlock[chan];

#if defined(VERSION_G4QP01)
    if (!card->attached)
    {
        return CARD_RESULT_NOCARD;
    }
    card->xferCallback = callback;
    card->repeat = (int)(length / CARD_PAGE_SIZE);
    card->addr = addr;
    card->buffer = dst;
#else
    ASSERTLINE(153, 0 < length && length % card->pageSize == 0);
    ASSERTLINE(154, 0 <= chan && chan < 2);

    if (card->attached == 0)
    {
        return CARD_RESULT_NOCARD;
    }
    card->xferCallback = callback;
    card->repeat = (length / card->pageSize);
    card->addr = addr;
    card->buffer = dst;
#endif
    return __CARDWritePage(chan, BlockWriteCallback);
}

/**
 * Offset/Address/Size: 0x290 | 0x802401E8 | size: 0x18
 */
s32 CARDGetXferredBytes(s32 chan)
{
    ASSERTLINE(183, 0 <= chan && chan < 2);
    return __CARDBlock[chan].xferred;
}
