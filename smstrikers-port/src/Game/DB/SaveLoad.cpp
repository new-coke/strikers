#include "port/region.h"  // PORT: one binary, three discs
#include "Game/DB/SaveLoad.h"
#include "Game/GameInfo.h"
#include "Game/Sys/gcmemcard.h"
#include "Game/Sys/debug.h"
#include "Game/FE/feHelpFuncs.h"
#include "NL/nlFileGC.h"
#include "NL/nlMain.h"
#include "NL/nlMath.h"
#include "Game/ResetTask.h"
#include <dolphin/charPipeline/texPalette.h>
#include <string.h>
#include <dolphin/os.h>
// PORT: defined in GameInfo.cpp.
extern "C" void PortLogUserInfo(const char* tag);

static void (*g_Callback)(long);
static bool InOperation = false;

struct MemCardIDInfo
{
    s64 serialID;
};
static MemCardIDInfo mLastKnownMemCardID;

struct IconCRC
{
    IconCRC()
        : mIconCRC(0)
    {
    }

    IconCRC& operator=(unsigned long crc)
    {
        mIconCRC = crc;
        return *this;
    }
    operator unsigned long() const { return mIconCRC; }

private:
    unsigned long mIconCRC;
};

IconCRC gIconCRC;
s64 mRequiredMemoryCardID;
static const char* MarioSoccerFileName = "MarioSoccer";

IconDataCache gIconDataCache;
LoadCallbacks LoadSystem;
SaveCallbacks SaveSystem;
DeleteCallbacks DeleteSystem;
FormatCallbacks FormatSystem;
FileExistsCallbacks FileExistsSystem;
MemoryCardIDCallbacks MemoryCardIDSystem;

inline IconDataCache::IconDataCache()
{
    mIconHdrBuffer = NULL;
    mIconBuffer = NULL;
    mBannerBuffer = NULL;
}

inline LoadCallbacks::LoadCallbacks()
    : m_pReadBuffer(NULL)
    , m_pIconReadBuffer(NULL)
    , m_pLoadFile(NULL)
    , m_MustFreeBuffers(false)
    , m_IconLoadedCRC(0)
{
}

inline SaveCallbacks::SaveCallbacks()
    : m_pSaveFile(NULL)
    , m_pSaveGameBuffer(NULL)
    , m_IconCRC(0)
{
}

/**
 * Offset/Address/Size: 0x38E4 | 0x8018D240 | size: 0x94
 */
inline IconDataCache::~IconDataCache()
{
    if (mIconHdrBuffer)
    {
        delete[] mIconHdrBuffer;
        mIconHdrBuffer = NULL;
    }
    if (mIconBuffer)
    {
        delete[] mIconBuffer;
        mIconBuffer = NULL;
    }
    if (mBannerBuffer)
    {
        delete[] mBannerBuffer;
        mBannerBuffer = NULL;
    }
}

/**
 * Offset/Address/Size: 0x38DC | 0x8018D238 | size: 0x8
 */
bool SaveLoad::CardBusy()
{
    return InOperation;
}

static void ConstructIconCfg(MemCard::ICON_CONFIG& IconCfg)
{
    memset(&IconCfg, 0, sizeof(MemCard::ICON_CONFIG));

    IconCfg.IconCount = 1;
    IconCfg.IconFormat = 2;
    IconCfg.IconSpeeds[0] = 3;
    IconCfg.BannerFormat = 2;
}

static inline u32 port_TPLBE32(const void* p)
{
    const u8* b = (const u8*)p;
    return ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | (u32)b[3];
}

static inline TEXHeaderPtr GetTPLImageHeader(TEXPalettePtr tpl)
{
    const u8* base = (const u8*)tpl;
    const u32 descriptorArray = port_TPLBE32(base + 8);      // TEXPalette.descriptorArray
    return (TEXHeaderPtr)(uintptr_t)port_TPLBE32(base + descriptorArray); // TEXDescriptor.textureHeader
}

// The TEXHeader's data field: four big-endian bytes at +8 of the header, which is a file offset from the palette base.
static inline u32 port_TPLImageDataOffset(const void* tpl, TEXHeaderPtr header)
{
    const uintptr_t headerOffset = (uintptr_t)header;
    return port_TPLBE32((const u8*)tpl + headerOffset + 8);
}

/**
 * Offset/Address/Size: 0x3720 | 0x8018D07C | size: 0x1BC
 */
void LoadMemoryCardIconData()
{
    nlFile* bannerFile;
    unsigned int iconSize;
    unsigned int bannerSize;

    memset(&gIconDataCache, 0, sizeof(MemCard::ICON_CONFIG));

    gIconDataCache.mIconConfig.IconCount = 1;
    gIconDataCache.mIconConfig.IconFormat = 2;
    gIconDataCache.mIconConfig.IconSpeeds[0] = 3;
    gIconDataCache.mIconConfig.BannerFormat = 2;

    gIconDataCache.mIconConfig.GetValidDataInfo(gIconDataCache.mIconDataInfo);

    s8 iconFmt = gIconDataCache.mIconConfig.IconFormat;
    int iconPixels = iconFmt << 10;
    int bannerFmt = gIconDataCache.mIconConfig.BannerFormat;

    u32 bannerHeader = 0;
    bannerHeader += ((bannerFmt == 1) ? 0x200 : 0);
    bannerHeader += bannerFmt * 0xC00;
    u32 iconClut = ((iconFmt == 1) ? 0x200 : 0);
    u32 headerSize = 0x40 + bannerHeader
                   + gIconDataCache.mIconConfig.IconCount * iconPixels + iconClut;
    gIconDataCache.mIconConfig.HeaderSize = headerSize;

    gIconDataCache.mIconHdrBuffer = nlMalloc(headerSize, 0x20, false);
    gIconDataCache.mIconDataInfo.pHeaderData = (unsigned char*)gIconDataCache.mIconHdrBuffer;
    void* pHdrBuf = gIconDataCache.mIconDataInfo.pHeaderData;

    nlStrNCpy((char*)pHdrBuf, GetMemCardTitle(), 0x20);
    char* pDescDst = (char*)gIconDataCache.mIconDataInfo.pHeaderData + 0x20;
    nlStrNCpy(pDescDst, GetMemCardDescription(), 0x20);

    bannerFile = nlOpen("art/fe/MC_Banner.tpl");
    nlFileSize(bannerFile, &bannerSize);
    gIconDataCache.mBannerBuffer = nlMalloc(bannerSize, 0x20, true);
    nlRead(bannerFile, gIconDataCache.mBannerBuffer, bannerSize);
    nlClose(bannerFile);

    nlFile* iconFile = nlOpen("art/fe/MC_icon.tpl");
    nlFileSize(iconFile, &iconSize);
    gIconDataCache.mIconBuffer = nlMalloc(iconSize, 0x20, true);
    nlRead(iconFile, gIconDataCache.mIconBuffer, iconSize);
    nlClose(iconFile);
}

/**
 * Offset/Address/Size: 0x355C | 0x8018CEB8 | size: 0x1C4
 */
inline unsigned long LoadCallbacks::LoadIconDataDoneCB(unsigned long Slot, long Result, void* pUserData)
{
    MCFILE_HEADER* fileheader = (MCFILE_HEADER*)m_pReadBuffer;
    MemCard::MC_FILE* file = (MemCard::MC_FILE*)pUserData;
    s8 iconFmt = file->IconCfg.IconFormat;
    int iconPixels = iconFmt << 10;
    int bannerFmt = file->IconCfg.BannerFormat;

    int bannerHeader = 0;
    bannerHeader += ((bannerFmt == 1) ? 0x200 : 0);
    bannerHeader += bannerFmt * 0xC00;
    u32 iconClut = ((iconFmt == 1) ? 0x200 : 0);
    int headerSize = bannerHeader + (file->IconCfg.IconCount * iconPixels);
    headerSize = headerSize + iconClut;
    headerSize += 0x40;
    file->IconCfg.HeaderSize = headerSize;

    u32 crc = nlChecksum32(m_pIconReadBuffer, headerSize);
    m_IconLoadedCRC = crc;
    m_MustFreeBuffers = true;

    if (fileheader->IconCRC != m_IconLoadedCRC)
    {
        if (file != NULL)
        {
            g_MemCards[Slot]->CloseFile((MemCard::MC_FILE*)pUserData);
        }

        MemCard* card = g_MemCards[Slot];
        card->m_State = IS_IDLE;
        card->m_CardState = CS_IDLE;
        CARDUnmount(card->m_Slot);
        InOperation = false;
        g_Callback(-1000);
        m_MustFreeBuffers = true;
        return -1;
    }

    gIconCRC = fileheader->IconCRC;
    if (m_TestGameID)
    {
        m_GameIDTestResult = nlSingleton<GameInfoManager>::Instance()->CheckSaveIDChanged((void*)((u8*)m_pReadBuffer + 12));
    }
    else
    {
        nlSingleton<GameInfoManager>::Instance()->SetMemoryCardData((void*)((u8*)m_pReadBuffer + 12));
        PortLogUserInfo("after-load");
    }

    mRequiredMemoryCardID = g_MemCards[Slot]->GetSerialID();
    g_MemCards[Slot]->CloseFile((MemCard::MC_FILE*)pUserData);

    MemCard* card = g_MemCards[Slot];
    card->m_State = IS_IDLE;
    card->m_CardState = CS_IDLE;
    CARDUnmount(card->m_Slot);
    InOperation = false;
    g_Callback(Result);
    m_MustFreeBuffers = true;
    return (-Result | Result) >> 31;
}

/**
 * Offset/Address/Size: 0x3254 | 0x8018CBB0 | size: 0x308
 */
unsigned long LoadCallbacks::ReadDoneCB(unsigned long Slot, long Result, void* pUserData)
{
    union FunctorStorage
    {
        FunctorStorage() {}
        MemCardFunctor functor;
    } functorStorage;
    MemCardFunctor& functor = functorStorage.functor;
    typedef unsigned long (LoadCallbacks::*MemberCB)(unsigned long, long, void*);
    MemberCB cb;
    MemCard::ICON_CONFIG localCfg;
    MCFILE_HEADER* header;
    unsigned long calculatedcrc;
    unsigned long filesize;

    memset(&localCfg, 0, sizeof(MemCard::ICON_CONFIG));

    localCfg.IconCount = 1;
    localCfg.IconFormat = 2;
    localCfg.IconSpeeds[0] = 3;
    localCfg.BannerFormat = 2;

    bool configValid;
    if (memcmp(&localCfg, &((MemCard::MC_FILE*)pUserData)->IconCfg, 1) != 0)
    {
        configValid = 0;
    }
    else
    {
        header = (MCFILE_HEADER*)m_pReadBuffer;
        configValid = (header->Size == (u32)nlSingleton<GameInfoManager>::Instance()->GetMemoryCardDataSize());
    }

    if (!configValid)
    {
        OSReport("[card] corrupt reason=size\n");
        if (pUserData != NULL)
        {
            g_MemCards[Slot]->CloseFile((MemCard::MC_FILE*)pUserData);
        }
        MemCard* card = g_MemCards[Slot];
        card->m_State = IS_IDLE;
        card->m_CardState = CS_IDLE;
        CARDUnmount(card->m_Slot);
        InOperation = false;
        g_Callback(-1000);
        m_MustFreeBuffers = true;
        return -1;
    }

    header = (MCFILE_HEADER*)m_pReadBuffer;
    filesize = nlSingleton<GameInfoManager>::Instance()->GetMemoryCardDataSize();
    calculatedcrc = nlChecksum32((MCFILE_HEADER*)m_pReadBuffer + 1, filesize);
    if (header->CRC != calculatedcrc)
    {
        OSReport("[card] corrupt reason=crc\n");
        if (pUserData != NULL)
        {
            g_MemCards[Slot]->CloseFile((MemCard::MC_FILE*)pUserData);
        }
        MemCard* card = g_MemCards[Slot];
        card->m_State = IS_IDLE;
        card->m_CardState = CS_IDLE;
        CARDUnmount(card->m_Slot);
        InOperation = false;
        g_Callback(-1000);
        m_MustFreeBuffers = true;
        return -1;
    }

    m_pLoadFile = (MemCard::MC_FILE*)pUserData;

    cb = &LoadCallbacks::LoadIconDataDoneCB;
    new (functor.m_FunctorMem) MemCardFunctor::MCMemberFunctor<LoadCallbacks>(this, cb, pUserData);

    MemCard::MC_FILE* pLoadFile = m_pLoadFile;
    void* iconBuf = m_pIconReadBuffer;
    s8 iconFmt = pLoadFile->IconCfg.IconFormat;
    int iconPixels = iconFmt << 10;
    int bannerFmt = pLoadFile->IconCfg.BannerFormat;
    int bannerHeader = 0;
    MemCard** cards = g_MemCards;
    bannerHeader += ((bannerFmt == 1) ? 0x200 : 0);
    MemCard* card = cards[Slot];
    bannerHeader += bannerFmt * 0xC00;
    u32 iconClut = ((iconFmt == 1) ? 0x200 : 0);
    int headerSize = bannerHeader + (pLoadFile->IconCfg.IconCount * iconPixels);
    headerSize = headerSize + iconClut;
    u32 totalSize = (pLoadFile->IconCfg.HeaderSize = headerSize + 0x40);
    u32 alignedSize = (totalSize + 0x1FF) & ~0x1FF;

    long result = card->InternalReadFile(pLoadFile, iconBuf, alignedSize, 0, functor);

    if (result != 0)
    {
        if (pUserData != NULL)
        {
            g_MemCards[Slot]->CloseFile((MemCard::MC_FILE*)pUserData);
        }
        MemCard* card = g_MemCards[Slot];
        card->m_State = IS_IDLE;
        card->m_CardState = CS_IDLE;
        CARDUnmount(card->m_Slot);
        InOperation = false;
        g_Callback(result);
        m_MustFreeBuffers = true;
        return -1;
    }

    return result;
}

/**
 * Offset/Address/Size: 0xE88 | 0x8018A7E4 | size: 0x24C
 */
inline unsigned long LoadCallbacks::CardMountCB(unsigned long Slot, long Result, void* pUserData)
{
    if (Result != 0)
    {
        MemCard* card = g_MemCards[Slot];
        card->m_State = IS_IDLE;
        card->m_CardState = CS_IDLE;
        CARDUnmount(card->m_Slot);
        InOperation = false;
        g_Callback(Result);
        m_MustFreeBuffers = true;
        return -1;
    }

    MemCard::MC_FILE* pFile;
    unsigned long FileLength;
    Result = g_MemCards[Slot]->OpenFile(MarioSoccerFileName, pFile, &FileLength);

    if (Result != 0)
    {
        MemCard* card = g_MemCards[Slot];
        card->m_State = IS_IDLE;
        card->m_CardState = CS_IDLE;
        CARDUnmount(card->m_Slot);
        InOperation = false;
        g_Callback(Result);
        m_MustFreeBuffers = true;
        return -1;
    }

    if (!m_PerformLoad)
    {
        if (pFile != NULL)
        {
            g_MemCards[Slot]->CloseFile(pFile);
        }
        MemCard* card = g_MemCards[Slot];
        card->m_State = IS_IDLE;
        card->m_CardState = CS_IDLE;
        CARDUnmount(card->m_Slot);
        InOperation = false;
        g_Callback(Result);
        m_MustFreeBuffers = true;
        return 0;
    }

    memset(m_pReadBuffer, 0, m_AlignedReadBufferDataSize);

    MemCard::MC_FILE* pFileLocal = pFile;
    typedef unsigned long (LoadCallbacks::*MemberCB)(unsigned long, long, void*);
    MemberCB cb = &LoadCallbacks::ReadDoneCB;
    union FunctorStorage
    {
        FunctorStorage() {}
        MemCardFunctor functor;
    } functorStorage;
    MemCardFunctor& functor = functorStorage.functor;
    void* functorMem = functor.m_FunctorMem;
    new (functorMem) MemCardFunctor::MCMemberFunctor<LoadCallbacks>(&LoadSystem, cb, pFileLocal);

    Result = g_MemCards[Slot]->InternalReadFile(pFile, m_pReadBuffer, m_AlignedReadBufferDataSize, pFile->TotalHeaderSize, functor);

    if (Result != 0)
    {
        if (pFile != NULL)
        {
            g_MemCards[Slot]->CloseFile(pFile);
        }
        MemCard* card = g_MemCards[Slot];
        card->m_State = IS_IDLE;
        card->m_CardState = CS_IDLE;
        CARDUnmount(card->m_Slot);
        InOperation = false;
        g_Callback(Result);
        m_MustFreeBuffers = true;
        return -1;
    }

    return 0;
}

/**
 * Offset/Address/Size: 0x2DA8 | 0x8018C704 | size: 0x4AC
 */
inline unsigned long SaveCallbacks::FileWriteCB(unsigned long Slot, long Result, void* pUserData)
{
    if (Result != 0)
    {
        HandleError(Slot, Result);
        return -1;
    }

    if (mRequiredMemoryCardID != 0)
    {
        s64 serialID = g_MemCards[Slot]->GetSerialID();
        if (mRequiredMemoryCardID != serialID)
        {
            long serialError = -1001;
            HandleError(Slot, serialError);
            return -1;
        }
    }
    else
    {
        mRequiredMemoryCardID = g_MemCards[Slot]->GetSerialID();
    }

    g_MemCards[Slot]->CloseFile(m_pSaveFile);
    MemCard* card = g_MemCards[Slot];
    card->m_State = IS_IDLE;
    card->m_CardState = CS_IDLE;
    CARDUnmount(card->m_Slot);
    InOperation = false;
    m_MustFreeMemory = true;
    g_Callback(Result);
    ResetTask::s_resetPaused = false;
    return 0;
}

/**
 * Offset/Address/Size: 0x2708 | 0x8018C064 | size: 0x6A0
 */
inline long SaveCallbacks::DoSave(unsigned long Slot)
{
    typedef unsigned long (SaveCallbacks::*MemberCB)(unsigned long, long, void*);
    MemberCB cb;
    union FunctorStorage
    {
        FunctorStorage() {}
        MemCardFunctor functor;
    } functorStorage;
    MemCardFunctor& functor = functorStorage.functor;

    MemCard::ICON_CONFIG localCfg;
    memset(&localCfg, 0, sizeof(MemCard::ICON_CONFIG));

    localCfg.IconCount = 1;
    localCfg.IconFormat = 2;
    localCfg.IconSpeeds[0] = 3;
    localCfg.BannerFormat = 2;
    u8 configValid = (memcmp(&localCfg, &m_pSaveFile->IconCfg, 1) == 0);
    if (!configValid)
    {
        HandleError(Slot, -1000);
        return -1;
    }

    if (gIconCRC == 0)
    {
        MemCard::ICON_DATA_INFO bannerDataInfo;
        m_pSaveFile->IconCfg.GetValidDataInfo(bannerDataInfo);
        bannerDataInfo.pHeaderData = (unsigned char*)gIconDataCache.mIconHdrBuffer;
        TEXPalettePtr bannerTpl = (TEXPalettePtr)gIconDataCache.mBannerBuffer;
        TEXHeaderPtr bannerHeader = GetTPLImageHeader(bannerTpl);
        void* srcBanner = (u8*)bannerTpl + port_TPLImageDataOffset(bannerTpl, bannerHeader);
        void* destBanner = bannerDataInfo.pHeaderData + bannerDataInfo.BannerOffset;
        u8 bannerFmt = m_pSaveFile->IconCfg.BannerFormat;
        memcpy(destBanner, srcBanner, ((bannerFmt == 1) ? 0x200 : 0) + bannerFmt * 0xC00);

        MemCard::ICON_DATA_INFO iconDataInfo;
        m_pSaveFile->IconCfg.GetValidDataInfo(iconDataInfo);
        iconDataInfo.pHeaderData = (unsigned char*)gIconDataCache.mIconHdrBuffer;
        TEXPalettePtr iconTpl = (TEXPalettePtr)gIconDataCache.mIconBuffer;
        TEXHeaderPtr iconHeader = GetTPLImageHeader(iconTpl);
        memcpy(
            iconDataInfo.pHeaderData + iconDataInfo.IconOffset[0],
            (u8*)iconTpl + port_TPLImageDataOffset(iconTpl, iconHeader),
            m_pSaveFile->IconCfg.IconFormat << 10);

        char iconFmtH = m_pSaveFile->IconCfg.IconFormat;
        int iconPixelsH = iconFmtH << 10;
        int bannerFmtH = m_pSaveFile->IconCfg.BannerFormat;
        int bannerHeaderH = 0;
        bannerHeaderH += ((bannerFmtH == 1) ? 0x200 : 0);
        bannerHeaderH += bannerFmtH * 0xC00;
        u32 iconClutH = ((iconFmtH == 1) ? 0x200 : 0);
        int headerSize = bannerHeaderH + (m_pSaveFile->IconCfg.IconCount * iconPixelsH);
        headerSize = headerSize + iconClutH;
        headerSize += 0x40;
        m_pSaveFile->IconCfg.HeaderSize = headerSize;
        u32 crc = nlChecksum32(iconDataInfo.pHeaderData, headerSize);
        m_IconCRC = crc;
        gIconCRC = m_IconCRC;
    }

    unsigned long DataSize = nlSingleton<GameInfoManager>::Instance()->GetMemoryCardDataSize() + 12;
    m_pSaveGameBuffer = nlMalloc(DataSize, 0x20, true);
    GameInfoManager* pGIM = nlSingleton<GameInfoManager>::s_pInstance;
    pGIM->mUserInfo.mSaveID = nlRandom(-1, &nlDefaultSeed);
    PortLogUserInfo("before-save");
    nlSingleton<GameInfoManager>::Instance()->GetMemoryCardData((void*)((u8*)m_pSaveGameBuffer + 12));
    MCFILE_HEADER* header = (MCFILE_HEADER*)m_pSaveGameBuffer;
    header->Size = nlSingleton<GameInfoManager>::Instance()->GetMemoryCardDataSize();
    header->CRC = nlChecksum32((void*)((u8*)m_pSaveGameBuffer + 12), nlSingleton<GameInfoManager>::Instance()->GetMemoryCardDataSize());
    header->IconCRC = gIconCRC;
    void* saveBuffer = m_pSaveGameBuffer;
    cb = &SaveCallbacks::FileWriteCB;
    void* functorMem = functor.m_FunctorMem;
    new (functorMem) MemCardFunctor::MCMemberFunctor<SaveCallbacks>(this, cb, saveBuffer);
    long Result = g_MemCards[Slot]->InternalWriteFile(m_pSaveFile, m_pSaveGameBuffer, DataSize, m_pSaveFile->TotalHeaderSize, functor, true);
    if (Result != 0)
    {
        HandleError(Slot, Result);
        return -1;
    }
    return 0;
}

inline void SaveCallbacks::HandleError(unsigned long Slot, long Result)
{
    if (m_pSaveFile != NULL)
    {
        g_MemCards[Slot]->CloseFile(m_pSaveFile);
        m_pSaveFile = NULL;
    }

    MemCard* card = g_MemCards[Slot];
    card->m_State = IS_IDLE;
    card->m_CardState = CS_IDLE;
    CARDUnmount(card->m_Slot);

    InOperation = false;

    if (Result == -4 && !SaveLoad::HasEnoughFreeSpace(Slot))
        Result = -9;

    m_MustFreeMemory = true;
    g_Callback(Result);
    ResetTask::s_resetPaused = false;
}

/**
 * Offset/Address/Size: 0x24EC | 0x8018BE48 | size: 0x21C
 */
inline unsigned long SaveCallbacks::FileWriteIconCB(unsigned long Slot, long Result, void* pUserData)
{
    if (Result != 0)
    {
        HandleError(Slot, Result);
        return -1;
    }

    DoSave(Slot);

    // PORT: success. The error path above returns -1 and every sibling callback ends `return 0`.
    return 0;
}

/**
 * Offset/Address/Size: 0x1F30 | 0x8018B88C | size: 0x5BC
 */
unsigned long SaveCallbacks::CreateFileCB(unsigned long Slot, long Result, void* pUserData)
{
    typedef unsigned long (SaveCallbacks::*MemberCB)(unsigned long, long, void*);
    MemberCB cb;

    if (Result != 0)
    {
        m_pSaveFile = NULL;
        HandleError(Slot, Result);
        return -1;
    }
    MemCard::ICON_DATA_INFO bannerDataInfo;
    m_pSaveFile->IconCfg.GetValidDataInfo(bannerDataInfo);
    bannerDataInfo.pHeaderData = (unsigned char*)gIconDataCache.mIconHdrBuffer;
    TEXPalettePtr bannerTpl = (TEXPalettePtr)gIconDataCache.mBannerBuffer;
    TEXHeaderPtr bannerHeader = GetTPLImageHeader(bannerTpl);
    void* srcBanner = (u8*)bannerTpl + port_TPLImageDataOffset(bannerTpl, bannerHeader);
    u8 bannerFmt = m_pSaveFile->IconCfg.BannerFormat;
    memcpy((u8*)gIconDataCache.mIconHdrBuffer + bannerDataInfo.BannerOffset, srcBanner, ((bannerFmt == 1) ? 0x200 : 0) + bannerFmt * 0xC00);

    MemCard::ICON_DATA_INFO iconDataInfo;
    m_pSaveFile->IconCfg.GetValidDataInfo(iconDataInfo);
    iconDataInfo.pHeaderData = (unsigned char*)gIconDataCache.mIconHdrBuffer;
    TEXPalettePtr iconTpl = (TEXPalettePtr)gIconDataCache.mIconBuffer;
    TEXHeaderPtr iconHeader = GetTPLImageHeader(iconTpl);
    memcpy(
        iconDataInfo.pHeaderData + iconDataInfo.IconOffset[0],
        (u8*)iconTpl + port_TPLImageDataOffset(iconTpl, iconHeader),
        m_pSaveFile->IconCfg.IconFormat << 10);

    char iconFmtH = m_pSaveFile->IconCfg.IconFormat;
    int iconPixelsH = iconFmtH << 10;
    int bannerFmtH = m_pSaveFile->IconCfg.BannerFormat;
    int bannerHeaderH = 0;
    bannerHeaderH += ((bannerFmtH == 1) ? 0x200 : 0);
    bannerHeaderH += bannerFmtH * 0xC00;
    u32 iconClutH = ((iconFmtH == 1) ? 0x200 : 0);
    int headerSize = bannerHeaderH + (m_pSaveFile->IconCfg.IconCount * iconPixelsH);
    headerSize = headerSize + iconClutH;
    headerSize += 0x40;
    m_pSaveFile->IconCfg.HeaderSize = headerSize;

    u32 crc = nlChecksum32(iconDataInfo.pHeaderData, headerSize);
    m_IconCRC = crc;
    gIconCRC = m_IconCRC;
    void* headerData = gIconDataCache.mIconDataInfo.pHeaderData;
    cb = &SaveCallbacks::FileWriteIconCB;
    union FunctorStorage
    {
        FunctorStorage() {}
        MemCardFunctor functor;
    } functorStorage;
    MemCardFunctor& functor = functorStorage.functor;
    void* functorMem = functor.m_FunctorMem;
    new (functorMem) MemCardFunctor::MCMemberFunctor<SaveCallbacks>(this, cb, headerData);
    Result = g_MemCards[m_Slot]->WriteFileIconData(m_pSaveFile, gIconDataCache.mIconDataInfo.pHeaderData, functor);
    if (Result != 0)
    {
        Slot = m_Slot;
        HandleError(Slot, Result);
    }
    return Result;
}

/**
 * Offset/Address/Size: 0x1EC4 | 0x8018B820 | size: 0x6C
 */
unsigned long DeleteCallbacks::DeleteDoneCB(unsigned long Slot, long Result, void* pUserData)
{
    MemCard* card = g_MemCards[Slot];
    card->m_State = IS_IDLE;
    card->m_CardState = CS_IDLE;
    CARDUnmount(card->m_Slot);
    InOperation = false;
    g_Callback(Result);
    return (-Result | Result) >> 31;
}

/**
 * Offset/Address/Size: 0x1E58 | 0x8018B7B4 | size: 0x6C
 */
unsigned long FormatCallbacks::FormatDoneCB(unsigned long Slot, long Result, void* pUserData)
{
    MemCard* card = g_MemCards[Slot];
    card->m_State = IS_IDLE;
    card->m_CardState = CS_IDLE;
    CARDUnmount(card->m_Slot);
    InOperation = false;
    g_Callback(Result);
    return (-Result | Result) >> 31;
}

/**
 * Offset/Address/Size: 0x11DC | 0x8018AB38 | size: 0xC7C
 */
inline unsigned long SaveCallbacks::CardMountCB(unsigned long Slot, long Result, void* pUserData)
{
    typedef unsigned long (SaveCallbacks::*MemberCB)(unsigned long, long, void*);
    union IconCfgStorage
    {
        IconCfgStorage() {}
        MemCard::ICON_CONFIG IconCfg;
    } iconCfgStorage;
    MemCard::ICON_CONFIG& IconCfg = iconCfgStorage.IconCfg;

    m_Slot = Slot;
    if (Result != 0)
    {
        HandleError(Slot, Result);
        return -1;
    }
    long dataSize = nlSingleton<GameInfoManager>::Instance()->GetMemoryCardDataSize() + 12;
    if (port_region() == PORT_REGION_JAPAN && mLastKnownMemCardID.serialID != 0
        && mLastKnownMemCardID.serialID != g_MemCards[Slot]->GetSerialID())
    {
        Result = -1001;
        HandleError(Slot, Result);
        return -1;
    }
    Result = g_MemCards[Slot]->FileExists(MarioSoccerFileName);
    switch (Result)
    {
    case 0:
    {
        m_pSaveFile = NULL;
        unsigned long openSize;
        long openResult = g_MemCards[Slot]->OpenFile(MarioSoccerFileName, m_pSaveFile, &openSize);
        if (openResult != 0)
        {
            HandleError(Slot, openResult);
            return -1;
        }
        MemCard::ICON_DATA_INFO bannerDataInfo;
        m_pSaveFile->IconCfg.GetValidDataInfo(bannerDataInfo);
        bannerDataInfo.pHeaderData = (unsigned char*)gIconDataCache.mIconHdrBuffer;
        TEXPalettePtr bannerTpl = (TEXPalettePtr)gIconDataCache.mBannerBuffer;
        TEXHeaderPtr bannerHeader = GetTPLImageHeader(bannerTpl);
        void* srcBanner = (u8*)bannerTpl + port_TPLImageDataOffset(bannerTpl, bannerHeader);
        void* destBanner = bannerDataInfo.pHeaderData + bannerDataInfo.BannerOffset;
        u8 bannerFmt = m_pSaveFile->IconCfg.BannerFormat;
        memcpy(
            destBanner, srcBanner, ((bannerFmt == 1) ? 0x200 : 0) + bannerFmt * 0xC00);

        MemCard::ICON_DATA_INFO iconDataInfo;
        m_pSaveFile->IconCfg.GetValidDataInfo(iconDataInfo);
        iconDataInfo.pHeaderData = (unsigned char*)gIconDataCache.mIconHdrBuffer;
        TEXPalettePtr iconTpl = (TEXPalettePtr)gIconDataCache.mIconBuffer;
        TEXHeaderPtr iconHeader = GetTPLImageHeader(iconTpl);
        memcpy(
            iconDataInfo.pHeaderData + iconDataInfo.IconOffset[0],
            (u8*)iconTpl + port_TPLImageDataOffset(iconTpl, iconHeader),
            m_pSaveFile->IconCfg.IconFormat << 10);

        MemCard::MC_FILE* fileH = m_pSaveFile;
        char iconFmtH = fileH->IconCfg.IconFormat;
        int iconPixelsH = iconFmtH << 10;
        int bannerFmtH = fileH->IconCfg.BannerFormat;
        int bannerHeaderH = 0;
        bannerHeaderH += ((bannerFmtH == 1) ? 0x200 : 0);
        bannerHeaderH += bannerFmtH * 0xC00;
        u32 iconClutH = ((iconFmtH == 1) ? 0x200 : 0);
        int headerSize = bannerHeaderH + (fileH->IconCfg.IconCount * iconPixelsH);
        headerSize = headerSize + iconClutH;
        headerSize += 0x40;
        fileH->IconCfg.HeaderSize = headerSize;

        u32 crc = nlChecksum32(iconDataInfo.pHeaderData, headerSize);
        m_IconCRC = crc;
        gIconCRC = m_IconCRC;
        void* headerData = gIconDataCache.mIconDataInfo.pHeaderData;
        MemberCB iconCb = &SaveCallbacks::FileWriteIconCB;
        union FunctorStorage
        {
            FunctorStorage() {}
            MemCardFunctor functor;
        } functorStorage;
        MemCardFunctor& functor = functorStorage.functor;
        void* functorMem = functor.m_FunctorMem;
        new (functorMem) MemCardFunctor::MCMemberFunctor<SaveCallbacks>(this, iconCb, headerData);
        long writeResult = g_MemCards[Slot]->WriteFileIconData(m_pSaveFile, gIconDataCache.mIconDataInfo.pHeaderData, functor);
        if (writeResult != 0)
        {
            HandleError(Slot, writeResult);
            return -1;
        }
    }
    break;
    case -4:
    {
        IconCfg.Initialize();
        memset(&IconCfg, 0, sizeof(MemCard::ICON_CONFIG));
        IconCfg.IconCount = 1;
        IconCfg.IconFormat = 2;
        IconCfg.IconSpeeds[0] = 3;
        IconCfg.BannerFormat = 2;
        m_pSaveFile = NULL;
        MemberCB cb = &SaveCallbacks::CreateFileCB;
        union FunctorStorage
        {
            FunctorStorage() {}
            MemCardFunctor functor;
        } functorStorage;
        MemCardFunctor& functor = functorStorage.functor;
        new (functor.m_FunctorMem) MemCardFunctor::MCMemberFunctor<SaveCallbacks>(this, cb);
        long createResult = g_MemCards[Slot]->CreateFile(MarioSoccerFileName, dataSize, &IconCfg, m_pSaveFile, functor);
        mRequiredMemoryCardID = 0;
        if (createResult != 0)
        {
            HandleError(Slot, createResult);
            return -1;
        }
    }
    break;
    default:
    {
        HandleError(Slot, Result);
        return -1;
    }
    break;
    }
    return 0;
}

/**
 * Offset/Address/Size: 0xB10 | 0x8018A46C | size: 0xDC
 */
inline unsigned long DeleteCallbacks::CardMountCB(unsigned long Slot, long Result, void* pUserData)
{
    if (Result != 0)
    {
        MemCard* card = g_MemCards[Slot];
        card->m_State = IS_IDLE;
        card->m_CardState = CS_IDLE;
        CARDUnmount(card->m_Slot);
        InOperation = false;
        g_Callback(Result);
        return -1;
    }

    s64 serialID = g_MemCards[Slot]->GetSerialID();
    if (port_region() == PORT_REGION_JAPAN
        && mLastKnownMemCardID.serialID != serialID)
    {
        MemCard* card = g_MemCards[Slot];
        card->m_State = IS_IDLE;
        card->m_CardState = CS_IDLE;
        CARDUnmount(card->m_Slot);
        InOperation = false;
        g_Callback(-1001);
        return -1;
    }

    typedef unsigned long (DeleteCallbacks::*MemberCB)(unsigned long, long, void*);
    MemberCB cb = &DeleteCallbacks::DeleteDoneCB;

    union FunctorStorage
    {
        FunctorStorage() {}
        MemCardFunctor functor;
    } functorStorage;
    MemCardFunctor& functor = functorStorage.functor;
    new (functor.m_FunctorMem) MemCardFunctor::MCMemberFunctor<DeleteCallbacks>(this, cb);

    g_MemCards[Slot]->DeleteFile(MarioSoccerFileName, functor);
    return 0;
}

/**
 * Offset/Address/Size: 0x900 | 0x8018A25C | size: 0x154
 */
inline unsigned long FormatCallbacks::CardMountCB(unsigned long Slot, long Result, void* pUserData)
{
    if (Result != 0 && Result != -13 && Result != -6)
    {
        MemCard* card = g_MemCards[Slot];
        card->m_State = IS_IDLE;
        card->m_CardState = CS_IDLE;
        CARDUnmount(card->m_Slot);
        InOperation = false;
        g_Callback(Result);
        return -1;
    }

    s64 serialID = g_MemCards[Slot]->GetSerialID();
    if (mLastKnownMemCardID.serialID != serialID)
    {
        MemCard* card = g_MemCards[Slot];
        card->m_State = IS_IDLE;
        card->m_CardState = CS_IDLE;
        CARDUnmount(card->m_Slot);
        InOperation = false;
        g_Callback(-1001);
        return -1;
    }

    typedef unsigned long (FormatCallbacks::*MemberCB)(unsigned long, long, void*);
    MemberCB cb = &FormatCallbacks::FormatDoneCB;

    union FunctorStorage
    {
        FunctorStorage() {}
        MemCardFunctor functor;
    } functorStorage;
    MemCardFunctor& functor = functorStorage.functor;
    new (functor.m_FunctorMem) MemCardFunctor::MCMemberFunctor<FormatCallbacks>(this, cb);

    g_MemCards[Slot]->FormatCard(functor);
    return 0;
}

/**
 * Offset/Address/Size: 0x10D4 | 0x8018AA30 | size: 0x108
 */
long SaveLoad::StartSave(int slot, void (*callback)(long))
{
    nlPrintf("Starting memory card save\n");
    OSReport("[card] start op=save slot=%d\n", slot);

    if (SaveSystem.m_MustFreeMemory)
    {
        if (SaveSystem.m_pSaveGameBuffer != nullptr)
        {
            nlFree(SaveSystem.m_pSaveGameBuffer);
            SaveSystem.m_pSaveGameBuffer = nullptr;
        }
    }

    SaveSystem.m_MustFreeMemory = false;
    InOperation = true;
    g_Callback = callback;

    typedef unsigned long (SaveCallbacks::*MemberCB)(unsigned long, long, void*);
    MemberCB cb = &SaveCallbacks::CardMountCB;

    union FunctorStorage
    {
        FunctorStorage() {}
        MemCardFunctor functor;
    } functorStorage;
    MemCardFunctor& functor = functorStorage.functor;
    new (functor.m_FunctorMem) MemCardFunctor::MCMemberFunctor<SaveCallbacks>(&SaveSystem, cb);

    s32 result = g_MemCards[slot]->BeginCardAccess(functor);
    if (result != 0)
    {
        InOperation = false;
    }

    return result;
}

/**
 * Offset/Address/Size: 0xBFC | 0x8018A558 | size: 0x28C
 */
long SaveLoad::StartLoad(int Slot, void (*pCB)(long), bool PerformLoad, bool testOnly)
{
    union FunctorStorage
    {
        FunctorStorage() {}
        MemCardFunctor functor;
    } functorStorage;
    MemCardFunctor& functor = functorStorage.functor;
    typedef unsigned long (LoadCallbacks::*MemberCB)(unsigned long, long, void*);
    MemberCB cb;

    tDebugPrintManager::Print(DC_FE, "Starting memory card load\n");
    OSReport("[card] start op=load slot=%d\n", Slot);

    if (LoadSystem.m_MustFreeBuffers)
    {
        nlFree(LoadSystem.m_pReadBuffer);
        LoadSystem.m_pReadBuffer = NULL;
        nlFree(LoadSystem.m_pIconReadBuffer);
        LoadSystem.m_pIconReadBuffer = NULL;
    }

    LoadSystem.m_MustFreeBuffers = false;

    if (PerformLoad)
    {
        long dataSize = nlSingleton<GameInfoManager>::Instance()->GetMemoryCardDataSize() + 0xC;
        LoadSystem.m_AlignedReadBufferDataSize = dataSize;
        dataSize = (dataSize + 0x1FF) & ~0x1FF;
        LoadSystem.m_AlignedReadBufferDataSize = dataSize;
        LoadSystem.m_pReadBuffer = nlMalloc(dataSize, 0x20, true);
        memset(LoadSystem.m_pReadBuffer, 0, LoadSystem.m_AlignedReadBufferDataSize);

        MemCard::ICON_CONFIG IconCfg;
        ConstructIconCfg(IconCfg);
        {
            unsigned char iconCount = IconCfg.IconCount;
            unsigned char bannerFormat = IconCfg.BannerFormat;
            char iconFormat = IconCfg.IconFormat;
            int iconPixels = iconFormat << 10;
            int bannerHeader = 0;
            bannerHeader += ((bannerFormat == 1) ? 0x200 : 0);
            bannerHeader += bannerFormat * 0xC00;
            unsigned long iconClut = ((iconFormat == 1) ? 0x200 : 0);
            int headerSize = iconClut;
            iconPixels = bannerHeader + (iconCount * iconPixels);
            headerSize = iconPixels + headerSize;
            headerSize += 0x40;
            IconCfg.HeaderSize = headerSize;
        }

        u32 allocSize = (IconCfg.HeaderSize + 0x1FF) & ~0x1FF;
        LoadSystem.m_pIconReadBuffer = nlMalloc(allocSize, 0x20, true);

        char iconFmt = IconCfg.IconFormat;
        int iconPixels = iconFmt << 10;
        int bannerFmt = IconCfg.BannerFormat;
        int bannerHeader = 0;
        bannerHeader += ((bannerFmt == 1) ? 0x200 : 0);
        bannerHeader += bannerFmt * 0xC00;
        u32 iconClut = ((iconFmt == 1) ? 0x200 : 0);
        int headerSize = bannerHeader + (IconCfg.IconCount * iconPixels);
        headerSize = headerSize + iconClut;
        headerSize += 0x40;
        IconCfg.HeaderSize = headerSize;

        memset(LoadSystem.m_pIconReadBuffer, 0, headerSize);

        LoadSystem.m_MustFreeBuffers = false;
    }

    InOperation = true;
    g_Callback = pCB;
    LoadSystem.m_TestGameID = testOnly;
    LoadSystem.m_PerformLoad = PerformLoad;

    cb = &LoadCallbacks::CardMountCB;

    new (functor.m_FunctorMem) MemCardFunctor::MCMemberFunctor<LoadCallbacks>(&LoadSystem, cb);

    s32 result = g_MemCards[Slot]->BeginCardAccess(functor);
    if (result != 0)
    {
        InOperation = false;
    }

    return result;
}

/**
 * Offset/Address/Size: 0xBEC | 0x8018A548 | size: 0x10
 */
u8 SaveLoad::DidGameIDChange()
{
    return LoadSystem.m_GameIDTestResult;
}

/**
 * Offset/Address/Size: 0xA54 | 0x8018A3B0 | size: 0xBC
 */
long SaveLoad::StartDelete(int slot, void (*callback)(long))
{
    nlPrintf("Starting memory card file delete\n");
    OSReport("[card] start op=delete slot=%d\n", slot);

    InOperation = true;

    typedef unsigned long (DeleteCallbacks::*MemberCB)(unsigned long, long, void*);
    MemberCB cb = &DeleteCallbacks::CardMountCB;

    union FunctorStorage
    {
        FunctorStorage() {}
        MemCardFunctor functor;
    } functorStorage;
    MemCardFunctor& functor = functorStorage.functor;
    new (functor.m_FunctorMem) MemCardFunctor::MCMemberFunctor<DeleteCallbacks>(&DeleteSystem, cb);

    s32 result = g_MemCards[slot]->BeginCardAccess(functor);
    if (result != 0)
    {
        InOperation = false;
    }

    return result;
}

/**
 * Offset/Address/Size: 0x844 | 0x8018A1A0 | size: 0xBC
 */
long SaveLoad::StartFormat(int slot, void (*callback)(long))
{
    nlPrintf("Starting memory card format\n");
    OSReport("[card] start op=format slot=%d\n", slot);

    InOperation = true;

    typedef unsigned long (FormatCallbacks::*MemberCB)(unsigned long, long, void*);
    MemberCB cb = &FormatCallbacks::CardMountCB;

    union FunctorStorage
    {
        FunctorStorage() {}
        MemCardFunctor functor;
    } functorStorage;
    MemCardFunctor& functor = functorStorage.functor;
    new (functor.m_FunctorMem) MemCardFunctor::MCMemberFunctor<FormatCallbacks>(&FormatSystem, cb);

    s32 result = g_MemCards[slot]->BeginCardAccess(functor);
    if (result != 0)
    {
        InOperation = false;
    }

    return result;
}

/**
 * Offset/Address/Size: 0x5EC | 0x80189F48 | size: 0x258
 */
inline unsigned long FileExistsCallbacks::CardMountCB(unsigned long Slot, long Result, void* pUserData)
{
    if (Result == 0)
    {
        Result = g_MemCards[Slot]->FileExists(MarioSoccerFileName);
    }

    if (Result != -5)
    {
        MemCard* card = g_MemCards[Slot];
        card->m_State = IS_IDLE;
        card->m_CardState = CS_IDLE;
        CARDUnmount(card->m_Slot);
    }

    InOperation = false;

    if (mRequiredMemoryCardID != 0
        && mRequiredMemoryCardID != g_MemCards[Slot]->GetSerialID())
    {
        Result = -1001;
    }
    else if (Result == -4)
    {
        if (!SaveLoad::HasEnoughFreeSpace(Slot))
            Result = -9;
    }

    g_Callback(Result);
    return -1;
}

/**
 * Offset/Address/Size: 0x520 | 0x80189E7C | size: 0xCC
 */
long SaveLoad::StartFileExistsCheck(int slot, void (*callback)(long))
{
    nlPrintf("Starting memory card file exists check!\n");
    OSReport("[card] start op=exists slot=%d\n", slot);

    InOperation = true;
    g_Callback = callback;

    typedef unsigned long (FileExistsCallbacks::*MemberCB)(unsigned long, long, void*);
    MemberCB cb = &FileExistsCallbacks::CardMountCB;

    union FunctorStorage
    {
        FunctorStorage() {}
        MemCardFunctor functor;
    } functorStorage;
    MemCardFunctor& functor = functorStorage.functor;
    new (functor.m_FunctorMem) MemCardFunctor::MCMemberFunctor<FileExistsCallbacks>(&FileExistsSystem, cb);

    s32 result = g_MemCards[slot]->BeginCardAccess(functor);
    if (result != 0)
    {
        InOperation = false;
    }

    return result;
}

/**
 * Offset/Address/Size: 0x45C | 0x80189DB8 | size: 0xC4
 */
inline unsigned long MemoryCardIDCallbacks::CardMountCB(unsigned long Slot, long Result, void* pUserData)
{
    if (Result == 0)
    {
        if (mRequiredMemoryCardID != 0)
        {
            s64 serialID = g_MemCards[Slot]->GetSerialID();
            if (mRequiredMemoryCardID != serialID)
            {
                Result = -1001;
            }
        }
    }

    if (Result != -5)
    {
        MemCard* card = g_MemCards[Slot];
        card->m_State = IS_IDLE;
        card->m_CardState = CS_IDLE;
        CARDUnmount(card->m_Slot);
    }

    InOperation = false;
    g_Callback(Result);
    return -1;
}

/**
 * Offset/Address/Size: 0x390 | 0x80189CEC | size: 0xCC
 */
long SaveLoad::StartMemoryCardIDCheck(int slot, void (*callback)(long))
{
    nlPrintf("Starting Memory Card ID Check!\n");
    OSReport("[card] start op=idcheck slot=%d\n", slot);

    InOperation = true;
    g_Callback = callback;

    typedef unsigned long (MemoryCardIDCallbacks::*MemberCB)(unsigned long, long, void*);
    MemberCB cb = &MemoryCardIDCallbacks::CardMountCB;

    union FunctorStorage
    {
        FunctorStorage() {}
        MemCardFunctor functor;
    } functorStorage;
    MemCardFunctor& functor = functorStorage.functor;
    new (functor.m_FunctorMem) MemCardFunctor::MCMemberFunctor<MemoryCardIDCallbacks>(&MemoryCardIDSystem, cb);

    s32 result = g_MemCards[slot]->BeginCardAccess(functor);
    if (result != 0)
    {
        InOperation = false;
    }

    return result;
}

/**
 * Offset/Address/Size: 0x264 | 0x80189BC0 | size: 0x12C
 */
int SaveLoad::GetSaveBlockSize(int slot)
{
    int dataSize = nlSingleton<GameInfoManager>::Instance()->GetMemoryCardDataSize();
    int numBlocks = 0;

    dataSize += 12;
    while (dataSize > 0)
    {
        numBlocks++;
        dataSize -= 0x2000;
    }

    MemCard::ICON_CONFIG IconCfg;
    ConstructIconCfg(IconCfg);
    IconCfg.CalculateHeaderSize();
    dataSize = IconCfg.HeaderSize;
    while (dataSize > 0)
    {
        numBlocks++;
        dataSize -= 0x2000;
    }

    return numBlocks;
}

/**
 * Offset/Address/Size: 0xD8 | 0x80189A34 | size: 0x18C
 */
u8 SaveLoad::HasEnoughFreeSpace(int Slot)
{
    CARD_INFO& cardInfo = MemCard::At(Slot)->m_CardInfo;
    int numBlocks = GetSaveBlockSize(Slot);
    unsigned long bytestosave = numBlocks * cardInfo.SectorSize;
    unsigned long alignedSize = MemCard::At(Slot)->AlignBytesToSectorSize(bytestosave);
    if (alignedSize > (unsigned long)g_MemCards[Slot]->m_CardInfo.FreeBytes)
        return 0;

    if (g_MemCards[Slot]->m_CardInfo.FreeFiles < 1)
        return 0;

    return 1;
}

/**
 * Offset/Address/Size: 0x34 | 0x80189990 | size: 0xA4
 */
void SaveLoad::FreeAllCallbackMemory()
{
    if (SaveSystem.m_MustFreeMemory)
    {
        if (SaveSystem.m_pSaveGameBuffer != nullptr)
        {
            nlFree(SaveSystem.m_pSaveGameBuffer);
            SaveSystem.m_pSaveGameBuffer = nullptr;
        }
    }
    SaveSystem.m_MustFreeMemory = false;

    if (LoadSystem.m_MustFreeBuffers)
    {
        nlFree(LoadSystem.m_pReadBuffer);
        LoadSystem.m_pReadBuffer = nullptr;
        nlFree(LoadSystem.m_pIconReadBuffer);
        LoadSystem.m_pIconReadBuffer = nullptr;
    }
    LoadSystem.m_MustFreeBuffers = false;
}

/**
 * Offset/Address/Size: 0x0 | 0x8018995C | size: 0x34
 */
void SaveLoad::RememberCurrentMemCardSerialID(int id)
{
    mLastKnownMemCardID.serialID = g_MemCards[id]->GetSerialID();
}
