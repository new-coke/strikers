#include "__card.h"

// prototypes
static void WriteCallback(s32 chan, s32 result);
static void EraseCallback(s32 chan, s32 result);

/**
 * Offset/Address/Size: 0x0 | 0x80240604 | size: 0x8
 */
CARDDir* __CARDGetDirBlock(CARDControl* card)
{
    ASSERTLINE(54, card->currentDir);
    return card->currentDir;
}

/**
 * Offset/Address/Size: 0x8 | 0x8024060C | size: 0xD0
 */
static void WriteCallback(s32 chan, s32 result)
{
    CARDControl* card = &__CARDBlock[chan];
    CARDCallback callback;

    if (result >= 0)
    {
        CARDDir* dir0 = (CARDDir*)((u8*)card->workArea + CARD_SYSTEM_BLOCK_SIZE);
        CARDDir* dir1 = (CARDDir*)((u8*)card->workArea + CARD_SYSTEM_BLOCK_SIZE * 2);

        ASSERTLINE(79, card->currentDir);

        if (card->currentDir == dir0)
        {
            card->currentDir = dir1;
            memcpy(dir1, dir0, CARD_SYSTEM_BLOCK_SIZE);
        }
        else
        {
            ASSERTLINE(87, card->currentDir == dir1);
            card->currentDir = dir0;
            memcpy(dir0, dir1, CARD_SYSTEM_BLOCK_SIZE);
        }
    }

    if (!card->apiCallback)
        __CARDPutControlBlock(card, result);

    callback = card->eraseCallback;
    if (callback)
    {
        card->eraseCallback = NULL;
        callback(chan, result);
    }
}

/**
 * Offset/Address/Size: 0xD8 | 0x802406DC | size: 0xC8
 */
static void EraseCallback(s32 chan, s32 result)
{
    CARDControl* card = &__CARDBlock[chan];
    CARDCallback callback;
    CARDDir* dir;
    u32 addr;

    if (result >= 0)
    {
        dir = __CARDGetDirBlock(card);
        addr = ((u32)dir - (u32)card->workArea) / CARD_SYSTEM_BLOCK_SIZE * card->sectorSize;
        result = __CARDWrite(chan, addr, CARD_SYSTEM_BLOCK_SIZE, dir, WriteCallback);
        if (result >= 0)
            return;
    }

    if (!card->apiCallback)
        __CARDPutControlBlock(card, result);

    callback = card->eraseCallback;
    if (callback)
    {
        card->eraseCallback = NULL;
        callback(chan, result);
    }
}

/**
 * Offset/Address/Size: 0x1A0 | 0x802407A4 | size: 0xC4
 */
s32 __CARDUpdateDir(s32 chan, CARDCallback callback)
{
    CARDControl* card;
    CARDDirCheck* check;
    u32 addr;
    CARDDir* dir;

    ASSERTLINE(173, 0 <= chan && chan < 2);

    card = &__CARDBlock[chan];
    if (!card->attached)
        return CARD_RESULT_NOCARD;

    dir = __CARDGetDirBlock(card);
    check = CARDGetDirCheck(dir);
    ++check->checkCode;
    __CARDCheckSum(dir, CARD_SYSTEM_BLOCK_SIZE - sizeof(u32), &check->checkSum, &check->checkSumInv);
    DCStoreRange(dir, CARD_SYSTEM_BLOCK_SIZE);

    card->eraseCallback = callback;
    addr = ((u32)dir - (u32)card->workArea) / CARD_SYSTEM_BLOCK_SIZE * card->sectorSize;
    return __CARDEraseSector(chan, addr, EraseCallback);
}
