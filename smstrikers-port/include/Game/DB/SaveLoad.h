#ifndef _SAVELOAD_H_
#define _SAVELOAD_H_

#include <dolphin/card.h>
#include "Game/Sys/gcmemcard.h"

// PORT: u32, not unsigned long; the format is twelve bytes and every caller counts in twelves.
struct MCFILE_HEADER
{
    /* 0x0 */ u32 Size;
    /* 0x4 */ u32 CRC;
    /* 0x8 */ u32 IconCRC;
}; // total size: 0xC

void LoadMemoryCardIconData();

namespace SaveLoad
{
bool CardBusy();
long StartSave(int slot, void (*callback)(long));
long StartLoad(int Slot, void (*pCB)(long), bool PerformLoad, bool testOnly);
u8 DidGameIDChange();
long StartDelete(int slot, void (*callback)(long));
long StartFormat(int slot, void (*callback)(long));
long StartFileExistsCheck(int slot, void (*callback)(long));
long StartMemoryCardIDCheck(int slot, void (*callback)(long));
int GetSaveBlockSize(int slot);
u8 HasEnoughFreeSpace(int Slot);
void FreeAllCallbackMemory();
void RememberCurrentMemCardSerialID(int id);
} // namespace SaveLoad

struct MemoryCardIDCallbacks
{
    unsigned long CardMountCB(unsigned long Slot, long Result, void* pUserData);
};

struct FileExistsCallbacks
{
    unsigned long CardMountCB(unsigned long Slot, long Result, void* pUserData);
};

struct FormatCallbacks
{
    unsigned long FormatDoneCB(unsigned long Slot, long Result, void* pUserData);
    unsigned long CardMountCB(unsigned long Slot, long Result, void* pUserData);
};

struct DeleteCallbacks
{
    unsigned long DeleteDoneCB(unsigned long Slot, long Result, void* pUserData);
    unsigned long CardMountCB(unsigned long Slot, long Result, void* pUserData);
};

struct LoadCallbacks
{
    LoadCallbacks();
    unsigned long LoadIconDataDoneCB(unsigned long Slot, long Result, void* pUserData);
    unsigned long ReadDoneCB(unsigned long Slot, long Result, void* pUserData);
    unsigned long CardMountCB(unsigned long Slot, long Result, void* pUserData);

    /* 0x00 */ void* m_pReadBuffer;
    /* 0x04 */ unsigned long m_AlignedReadBufferDataSize;
    /* 0x08 */ void* m_pIconReadBuffer;
    /* 0x0C */ unsigned long m_AlignedIconReadBufferDataSize;
    /* 0x10 */ MemCard::MC_FILE* m_pLoadFile;
    /* 0x14 */ bool m_TestGameID;
    /* 0x15 */ u8 m_GameIDTestResult;
    /* 0x16 */ bool m_PerformLoad;
    /* 0x17 */ bool m_MustFreeBuffers;
    /* 0x18 */ unsigned long m_IconLoadedCRC;
}; // total size: 0x1C

struct SaveCallbacks
{
    SaveCallbacks();
    void HandleError(unsigned long Slot, long Result);
    unsigned long FileWriteCB(unsigned long Slot, long Result, void* pUserData);
    unsigned long FileWriteIconCB(unsigned long Slot, long Result, void* pUserData);
    unsigned long CreateFileCB(unsigned long Slot, long Result, void* pUserData);
    long DoSave(unsigned long Slot);
    unsigned long CardMountCB(unsigned long Slot, long Result, void* pUserData);

    /* 0x00 */ MemCard::MC_FILE* m_pSaveFile;
    /* 0x04 */ void* m_pSaveGameBuffer;
    /* 0x08 */ unsigned long m_Slot;
    /* 0x0C */ unsigned long m_IconCRC;
    /* 0x10 */ unsigned char m_MustFreeMemory;
}; // total size: 0x14

struct IconDataCache
{
    IconDataCache();
    ~IconDataCache();
    /* 0x00 */ MemCard::ICON_CONFIG mIconConfig;
    /* 0x10 */ MemCard::ICON_DATA_INFO mIconDataInfo;
    /* 0x40 */ void* mIconHdrBuffer;
    /* 0x44 */ void* mIconBuffer;
    /* 0x48 */ void* mBannerBuffer;
}; // total size: 0x4C

extern IconDataCache gIconDataCache;

#endif // _SAVELOAD_H_
