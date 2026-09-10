#ifndef _GCMEMCARD_H_
#define _GCMEMCARD_H_

// #include "Game/DB/SaveLoad.h"
#include <dolphin/card.h>
#include "NL/nlSortedSlot.h"
#include "NL/nlString.h"

class MemCard;
extern MemCard** g_MemCards;

enum INTERNAL_STATE
{
    IS_IDLE = 0,
    IS_ERROR = 1,
    IS_MOUNTING = 2,
    IS_CARDCHECK = 3,
    IS_MOUNTED = 4,
    IS_MOUNTED_ERROR = 5,
    IS_FORMATTING = 6,
    IS_CREATING = 7,
    IS_DELETING = 8,
    IS_READING = 9,
    IS_WRITING = 10,
    IS_WRITINGSTATUS = 11,
};

enum CARD_STATE
{
    CS_IDLE = 0,
    CS_MOUNTING = 1,
    CS_MOUNTED = 2,
    CS_MOUNTED_ERROR = 3,
    CS_FORMATTING = 4,
    CS_CREATING = 5,
    CS_DELETING = 6,
    CS_READING = 7,
    CS_WRITING = 8,
    CS_END = 9,
};

struct CARD_INFO
{
    /* 0x0 */ s32 CardSize;
    /* 0x4 */ s32 SectorSize;
    /* 0x8 */ s32 FreeBytes;
    /* 0xC */ s32 FreeFiles;
}; // total size: 0x10

// PORT: a C++98-safe compile-time assert.
#define NL_STATIC_ASSERT(cond, msg) \
    typedef char msg[(cond) ? 1 : -1]; (void)sizeof(msg)

class MemCardFunctor
{
public:
    MemCardFunctor();

    class MCInternalFunctorBase
    {
    public:
        MCInternalFunctorBase()
            : m_pData(NULL)
        {
        }
        MCInternalFunctorBase(void* pData)
            : m_pData(pData)
        {
        }
        ~MCInternalFunctorBase();
        virtual void Call(unsigned long, long) = 0;
        void Destroy();

        /* 0x04 */ void* m_pData;
    };

    void Call(unsigned long slot, long result)
    {
        MCInternalFunctorBase* pFunctor = (MCInternalFunctorBase*)m_FunctorMem;
        if (*(long*)pFunctor != 0)
        {
            pFunctor->Call(slot, result);
        }
        else
        {
            nlPrintf("Trying to call unset MC functor");
        }
    }

    template <class T>
    class MCMemberFunctor : public MCInternalFunctorBase
    {
    public:
        typedef unsigned long (T::*MemberCB)(unsigned long, long, void*);

        MCMemberFunctor(T* obj, const MemberCB& cb)
        {
            // PORT: this is built with placement new inside MemCardFunctor::m_FunctorMem.
            NL_STATIC_ASSERT(sizeof(MCMemberFunctor) <= kFunctorMemSize,
                             functor_does_not_fit_in_MemCardFunctor_storage);
            m_cb = cb;
            m_pObject = obj;
        }
        MCMemberFunctor(T* obj, const MemberCB& cb, void* pData)
            : MCInternalFunctorBase(pData)
        {
            m_cb = cb;
            m_pObject = obj;
        }
        ~MCMemberFunctor();
        virtual void Call(unsigned long slot, long result)
        {
            (m_pObject->*m_cb)(slot, result, m_pData);
        }
        void Destroy();

        /* 0x08 */ MemberCB m_cb;
        /* 0x14 */ T* m_pObject;
    };

    // PORT: 24 on console, where a pointer is four bytes and a pointer-to-member is twelve.
    enum { kFunctorMemSize = 64 };
    /* 0x00 */ mutable unsigned char m_FunctorMem[kFunctorMemSize];
}; // total size: 0x18

inline MemCardFunctor::MemCardFunctor()
{
    *(unsigned long*)m_FunctorMem = 0;
}

class MemCard
{
public:
    struct ICON_DATA_INFO
    {
        /* 0x00 */ unsigned short Comment1Offset;
        /* 0x02 */ unsigned short Comment2Offset;
        /* 0x04 */ unsigned short BannerOffset;
        /* 0x06 */ unsigned short BannerCLUTOffset;
        /* 0x08 */ unsigned long IconOffset[8];
        /* 0x28 */ unsigned long IconCLUTOffset;
        /* 0x2C */ unsigned char* pHeaderData;
    }; // total size: 0x30

    struct ICON_CONFIG
    {
        ICON_CONFIG()
        {
            Initialize();
        }

        void Initialize()
        {
            BannerFormat = 0;
            IconCount = 0;
            IconFormat = 0;
            IconAnimType = 0;
            memset(IconSpeeds, 0, 8);
        }

        void GetValidDataInfo(ICON_DATA_INFO& DataInfo) const;

        void CalculateHeaderSize()
        {
            unsigned char iconCount = IconCount;
            int bannerFormat = BannerFormat;
            char iconFormat = IconFormat;
            int iconPixels = iconFormat << 10;
            int bannerHeader = 0;
            bannerHeader += ((bannerFormat == 1) ? 0x200 : 0);
            bannerHeader += bannerFormat * 0xC00;
            unsigned long iconClut = ((iconFormat == 1) ? 0x200 : 0);
            int headerSize = iconClut;
            iconPixels = bannerHeader + (iconCount * iconPixels);
            headerSize = iconPixels + headerSize;
            headerSize += 0x40;
            HeaderSize = headerSize;
        }

        /* 0x0 */ unsigned char BannerFormat;
        /* 0x1 */ unsigned char IconCount;
        /* 0x2 */ char IconFormat;
        /* 0x3 */ char IconAnimType;
        /* 0x4 */ char IconSpeeds[8];
        /* 0xC */ unsigned long HeaderSize;
    }; // total size: 0x10

    struct MC_FILE
    {
        /* 0x00 */ CARDFileInfo FileInfo;
        /* 0x14 */ ICON_CONFIG IconCfg;
        /* 0x24 */ unsigned long TotalHeaderSize;
    }; // total size: 0x28

    MemCard(unsigned long slot);

    long BeginCardAccess(const MemCardFunctor& Callback);
    long CreateFile(const char* FileName, unsigned long FileSize, MemCard::ICON_CONFIG* pIconConfig, MemCard::MC_FILE*& pFile, const MemCardFunctor& Callback);
    long OpenFile(const char* FileName, MemCard::MC_FILE*& pFile, unsigned long* pFileLength);
    long FormatCard(const MemCardFunctor& Callback);
    long DeleteFile(const char* FileName, const MemCardFunctor& Callback);
    long InternalReadFile(MemCard::MC_FILE* pFile, void* Buffer, unsigned long Length, unsigned long StartAt, const MemCardFunctor& Callback);
    long InternalWriteFile(MemCard::MC_FILE* pFile, void* Buffer, unsigned long Length, unsigned long StartAt, const MemCardFunctor& Callback, bool ResetTransfer);
    long CloseFile(MemCard::MC_FILE* pFile);
    long FileExists(const char* fileName);
    long WriteFileIconData(MemCard::MC_FILE* pFile, void* pData, const MemCardFunctor& functor);
    unsigned long AlignBytesToSectorSize(unsigned long bytes);

private:
    void CardRemoved(long result);
    void SetStatusDone(long Result);
    void WriteFileDone(long result);
    void ReadFileDone(long result);
    void DeleteFileDone(long result);
    void CreateFileDone(long result);
    void FormatDone(long result);
    void CardCheckBrokenDone(long result);
    void CardCheckDone(long result);
    void MountDone(long result);

public:
    s64 GetSerialID() const;

    /* 0x000  */ INTERNAL_STATE m_State;
    /* 0x004  */ unsigned long m_Slot;
    /* 0x008  */ CARD_STATE m_CardState;
    /* 0x00C  */ CARD_INFO m_CardInfo;
    /* 0x01C */ unsigned long m_LastTransferSize;
    /* 0x020 */ unsigned long m_TargetTransferSize;
    /* 0x028 */ s64 m_SerialID;
    /* 0x030 */ MemCardFunctor m_CB[9];
    /* 0x108 */ nlStaticSortedSlot<MC_FILE, 16> m_OpenFiles;
    /* 0x41C */ MC_FILE* m_pFileCB;
    /* 0x420 */ void* m_pDataCB;
    /* 0x424 */ unsigned long m_GameId;
    /* 0x428 */ unsigned short m_CompanyId;
    /* 0x42A */ unsigned char m_CardWorkArea[41472];

    static MemCard* At(int slot)
    {
        return g_MemCards[slot];
    }

    static bool s_InitDone;

    static void CardRemovedCB(s32 channel, s32 result) { g_MemCards[channel]->CardRemoved(result); }
    static void MountDoneCB(s32 channel, s32 result) { g_MemCards[channel]->MountDone(result); }
    static void CardCheckDoneCB(s32 channel, s32 result) { g_MemCards[channel]->CardCheckDone(result); }
    static void CardCheckBrokenDoneCB(s32 channel, s32 result) { g_MemCards[channel]->CardCheckBrokenDone(result); }
    static void FormatDoneCB(s32 channel, s32 result) { g_MemCards[channel]->FormatDone(result); }
    static void CreateFileDoneCB(s32 channel, s32 result) { g_MemCards[channel]->CreateFileDone(result); }
    static void DeleteFileDoneCB(s32 channel, s32 result) { g_MemCards[channel]->DeleteFileDone(result); }
    static void WriteFileDoneCB(s32 channel, s32 result) { g_MemCards[channel]->WriteFileDone(result); }
    static void ReadFileDoneCB(s32 channel, s32 result) { g_MemCards[channel]->ReadFileDone(result); }
    static void SetStatusDoneCB(s32 channel, s32 result) { g_MemCards[channel]->SetStatusDone(result); }
}; // total size: 0xA620

#endif // _GCMEMCARD_H_
