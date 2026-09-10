// nlAVLTree.h and nlDLListContainer.h are included directly, and ahead of this
// TU's own header, because that registration order sets the order of the weak
// template groups this TU emits. GoalieSave.h pulls nlListContainer.h in via
// nlList.h, so leaving these to be picked up transitively would emit the
// nlListContainer group first. Do not reorder or drop them.
#include "NL/nlAVLTree.h"
#include "NL/nlDLListContainer.h"
#include "Game/AI/GoalieSave.h"
#include "Game/AI/AiUtil.h"
#include "Game/AI/FilteredRandom.h"
#include "Game/AnimInventory.h"
#include "Game/CharacterTriggers.h"
#include "Game/Field.h"
#include "Game/Goalie.h"
#include "Game/SAnim/pnSAnimController.h"
#include "NL/nlMemory.h"
#include "NL/nlString.h"
#include "PowerPC_EABI_Support/Runtime/MWCPlusLib.h"

float GoalieSave::mfCatchAllowDist;
float GoalieSave::mfCatchAllowDistSq = 0.25f;

SaveData* GoalieSave::mpSaveTable;
unsigned char GoalieSave::mbInitialized;
unsigned int GoalieSave::muNumSaveEntries;
SavePositionData* GoalieSave::mpPositionTable;
unsigned int GoalieSave::muNumPositionEntries;
unsigned int GoalieSave::muSTSGoalIndexStart;
unsigned int GoalieSave::muSTSGoalCount;
unsigned int GoalieSave::muSTSMissIndexStart;
unsigned int GoalieSave::muSTSMissCount;
unsigned int GoalieSave::muSTSSaveIndexStart;
unsigned int GoalieSave::muSTSSaveCount;
unsigned int GoalieSave::muMissChipIndexStart;
unsigned int GoalieSave::muMissChipCount;
float GoalieSave::mfCrouchDuration;

static nlAVLTree<int, SaveData*, DefaultKeyCompare<int> > gSaveMap;
nlListContainer<SaveData*> gSaveGrid[7][5];
static float fDefaultMilestoneValues[2] = { 0.4f, 0.7f };

struct MyMiniData;

struct SaveInfo
{
    int mnAnimID;
    int mnFailAnimID;
    int mnRecoverAnimID;
    unsigned int muSaveType;
    int mConnectedSaveID[4];
    char mszName[16];
};

float GoalieSave::SetCatchAllowDist(float fDist)
{
    float fOldDist = mfCatchAllowDist;
    mfCatchAllowDist = fDist;
    mfCatchAllowDistSq = fDist * fDist;
    return fOldDist;
}

SaveData* GoalieSave::FindSaveData(int animID)
{
    SaveData** ppSaveData;
    if (animID >= 0 && gSaveMap.FindGet(animID, &ppSaveData))
    {
        return *ppSaveData;
    }

    return NULL;
}

void SaveData::PostInit(const SaveInfo& info)
{
    mpFailAnimData = GoalieSave::FindSaveData(info.mnFailAnimID);

    for (int i = 0; i < 4; i++)
    {
        mpConnectedSaveData[i] = GoalieSave::FindSaveData(info.mConnectedSaveID[i]);
    }
}

static const nlVector3 v3Zero = { 0.0f, 0.0f, 0.0f };

float SaveData::LookupFatigueValue(Goalie* pGoalie, const SaveInfo& info)
{
    GoalieTweaks* pTweaks = (GoalieTweaks*)pGoalie->m_pTweaks;
    if (info.muSaveType & 0x1)
    {
        return pTweaks->fShotFatigueStandCatch;
    }
    else if (info.muSaveType & 0x2)
    {
        return pTweaks->fShotFatigueDiveCatch;
    }
    else if (info.muSaveType & 0x4)
    {
        return pTweaks->fShotFatigueStandDeflect;
    }
    else if (info.muSaveType & 0x8)
    {
        return pTweaks->fShotFatigueDiveDeflect;
    }
    else if (info.muSaveType & 0x10)
    {
        return pTweaks->fShotFatigueStandPunch;
    }
    else if (info.muSaveType & 0x20)
    {
        return pTweaks->fShotFatigueLegSave;
    }
    else if (info.muSaveType & 0x00010000)
    {
        return pTweaks->fShotFatigueSTSStun;
    }
    else if (info.muSaveType & 0x00060000)
    {
        return pTweaks->fShotFatigueSTSStun;
    }

    return pTweaks->fShotFatigueDefault;
}

void SaveData::Init(Goalie* pGoalie, const SaveInfo& info, unsigned int uIndex)
{
    mnAnimID = info.mnAnimID;
    mnRecoverAnimID = info.mnRecoverAnimID;
    muSaveType = info.muSaveType;
    mfFatigueValue = LookupFatigueValue(pGoalie, info);

    mv3SavePos = v3Zero;

    mfMilestonePercent[0] = 0.0f;
    mfMilestonePercent[1] = 0.0f;
    mfMilestonePercent[2] = 0.0f;
    mfMilestonePercent[3] = 0.0f;
    mfMilestonePercent[4] = 0.0f;

    mv3TakeoffPos = v3Zero;
    mv3GroupMinCoords = v3Zero;
    mv3GroupMaxCoords = v3Zero;

    nlStrNCpy<char>(mszName, info.mszName, 16);
    muIndex = uIndex;
}

void SavePositionData::Init(Goalie* pGoalie, int animID)
{
    mnAnimID = animID;

    cPN_SAnimController* pController = ::new (AllocateSAnimController()) cPN_SAnimController(
        pGoalie->GetAnimInventory()->GetAnim(animID), NULL, PM_HOLD, NULL, 0, pGoalie->GetAnimInventory()->GetMirrored(animID));

    pController->m_fPrevTime = pController->m_fTime;
    pController->m_fTime = 1.0f;

    nlVector3 v3RootTrans;
    pController->GetRootTrans(&v3RootTrans, 0);

    mfAnimDistance = v3RootTrans.y;
    mfAnimTime = (float)pController->m_pSAnim->m_nNumKeys / 30.0f;
    mfAnimVelocity = mfAnimDistance / mfAnimTime;

    delete pController;
}

int gPositionAnimID[6] = { 25, 28, 20, 23, 34, 35 };
SaveInfo gSaveInfo[70] = {
    { 47, 88, -1, 0x00000001, { -1, 48, 52, 57 }, "Ctch Ctr XHiJmp" },
    { 48, 89, -1, 0x00000001, { 47, 49, 53, 58 }, "Ctch Ctr Hi Jmp" },
    { 49, 90, -1, 0x00000001, { 48, 50, 54, 59 }, "Catch Ctr Hi" },
    { 50, 91, -1, 0x00000001, { 49, 51, 55, 60 }, "Catch Ctr Med" },
    { 51, 92, -1, 0x00000001, { 50, -1, 56, 61 }, "Catch Ctr Lo" },
    { 52, 93, -1, 0x00000001, { -1, 53, -1, 47 }, "Catch Rt XHiJmp" },
    { 53, 94, -1, 0x00000001, { 52, 54, -1, 48 }, "Catch Rt Hi Jmp" },
    { 54, 95, -1, 0x00000001, { 53, 55, -1, 49 }, "Catch Rt Hi" },
    { 55, 96, -1, 0x00000001, { 54, 56, -1, 50 }, "Catch Rt Med" },
    { 56, 97, -1, 0x00000001, { 55, -1, -1, 51 }, "Catch Rt Lo" },
    { 57, 98, -1, 0x00000001, { -1, 58, 47, -1 }, "Catch Lf XHiJmp" },
    { 58, 99, -1, 0x00000001, { 57, 59, 48, -1 }, "Catch Lft HiJmp" },
    { 59, 100, -1, 0x00000001, { 58, 60, 49, -1 }, "Catch Lft Hi" },
    { 60, 101, -1, 0x00000001, { 59, 61, 50, -1 }, "Catch Lft Med" },
    { 61, 101, -1, 0x00000001, { 60, -1, 51, -1 }, "Catch Lft Lo" },
    { 106, 79, -1, 0x00000002, { -1, -1, -1, -1 }, "DiveCatchRHiSpc" },
    { 107, 85, -1, 0x00000002, { -1, -1, -1, -1 }, "DiveCatchLHiSpc" },
    { 62, 76, 132, 0x00000002, { -1, 63, 68, -1 }, "Dive Catch R Hi" },
    { 63, 75, 132, 0x00000002, { 62, 64, 69, -1 }, "Dive Catch R Md" },
    { 64, 74, 132, 0x00000002, { 63, -1, 70, -1 }, "Dive Catch R Lo" },
    { 65, 82, 133, 0x00000002, { -1, 66, -1, 71 }, "Dive Catch L Hi" },
    { 66, 81, 133, 0x00000002, { 65, 67, -1, 72 }, "Dive Catch L Md" },
    { 67, 80, 133, 0x00000002, { 66, -1, -1, 73 }, "Dive Catch L Lo" },
    { 68, 79, 132, 0x00000002, { -1, 69, -1, 62 }, "DiveCtch RHiFar" },
    { 69, 78, 132, 0x00000002, { 68, 70, -1, 63 }, "DiveCtch RMdFar" },
    { 70, 77, 132, 0x00000002, { 69, -1, -1, 64 }, "DiveCtch RLoFar" },
    { 71, 85, 133, 0x00000002, { -1, 72, 65, -1 }, "DiveCtch LHiFar" },
    { 72, 84, 133, 0x00000002, { 71, 73, 66, -1 }, "DiveCtch LMdFar" },
    { 73, 83, 133, 0x00000002, { 72, -1, 67, -1 }, "DiveCtch LLoFar" },
    { 88, -1, -1, 0x00000004, { -1, 89, 93, 98 }, "Deflect XHi Jmp" },
    { 89, -1, -1, 0x00000004, { 88, 90, 94, 99 }, "Deflect Hi Jmp" },
    { 90, -1, -1, 0x00000004, { 89, 91, 95, 100 }, "Deflect Hi" },
    { 91, -1, -1, 0x00000004, { 90, 92, 96, 101 }, "Deflect Med" },
    { 92, -1, -1, 0x00000004, { 91, -1, 97, 102 }, "Deflect Lo" },
    { 93, -1, -1, 0x00000004, { -1, 94, -1, 88 }, "Defl R XHi Jmp" },
    { 94, -1, -1, 0x00000004, { 93, 95, -1, 89 }, "Defl R Hi Jmp" },
    { 95, -1, -1, 0x00000004, { 94, 96, -1, 90 }, "Deflect R Hi" },
    { 96, -1, -1, 0x00000004, { 95, 97, -1, 91 }, "Deflect R Med" },
    { 97, -1, -1, 0x00000004, { 96, -1, -1, 92 }, "Deflect R Lo" },
    { 98, -1, -1, 0x00000004, { -1, 99, 88, -1 }, "Defl L XHi Jmp" },
    { 99, -1, -1, 0x00000004, { 98, 100, 89, -1 }, "Defl L Hi Jmp" },
    { 100, -1, -1, 0x00000004, { 99, 101, 90, -1 }, "Deflect L Hi" },
    { 101, -1, -1, 0x00000004, { 100, 102, 91, -1 }, "Deflect L Med" },
    { 102, -1, -1, 0x00000004, { 101, -1, 92, -1 }, "Deflect L Lo" },
    { 76, -1, 134, 0x00000008, { -1, 75, 79, -1 }, "Dive Dfl R Hi" },
    { 75, -1, 134, 0x00000008, { 76, 74, 78, -1 }, "Dive Dfl R Med" },
    { 74, -1, 134, 0x00000008, { 75, -1, 77, -1 }, "Dive Dfl R Lo" },
    { 79, -1, 134, 0x00000008, { -1, 78, -1, 76 }, "DiveDfl R HiFar" },
    { 78, -1, 134, 0x00000008, { 79, 77, -1, 75 }, "DiveDfl R MdFar" },
    { 77, -1, 134, 0x00000008, { 78, -1, -1, 74 }, "DiveDfl R LoFar" },
    { 82, -1, 135, 0x00000008, { -1, 81, -1, 85 }, "Dive Dfl L Hi" },
    { 81, -1, 135, 0x00000008, { 82, 80, -1, 84 }, "Dive Dfl L Med" },
    { 80, -1, 135, 0x00000008, { 81, -1, -1, 83 }, "Dive Dfl L Lo" },
    { 85, -1, 135, 0x00000008, { -1, 84, 82, -1 }, "DiveDfl L HiFar" },
    { 84, -1, 135, 0x00000008, { 85, 83, 81, -1 }, "DiveDfl L MdFar" },
    { 83, -1, 135, 0x00000008, { 84, -1, 80, -1 }, "DiveDfl L LoFar" },
    { 86, -1, -1, 0x00000020, { -1, -1, -1, -1 }, "Leg R Lo" },
    { 87, -1, -1, 0x00000020, { -1, -1, -1, -1 }, "Leg L Lo" },
    { 103, -1, -1, 0x00000010, { -1, -1, 104, 105 }, "Punch Hi" },
    { 104, -1, -1, 0x00000010, { -1, -1, -1, 103 }, "Punch R Hi" },
    { 105, -1, -1, 0x00000010, { -1, -1, 103, -1 }, "Punch L Hi" },
    { 108, -1, -1, 0x00020000, { -1, -1, -1, -1 }, "S2S Blast Net" },
    { 114, -1, 116, 0x00040000, { -1, -1, -1, -1 }, "S2S Spin L" },
    { 115, -1, 117, 0x00040000, { -1, -1, -1, -1 }, "S2S Spin R" },
    { 110, -1, 111, 0x00010000, { -1, -1, -1, -1 }, "S2S Save Stun" },
    { 128, -1, 134, 0x00100000, { -1, -1, -1, -1 }, "Miss Chip R" },
    { 129, -1, 135, 0x00100000, { -1, -1, -1, -1 }, "Miss Chip L" },
    { 130, -1, 134, 0x00100000, { -1, -1, -1, -1 }, "MissChipShort R" },
    { 131, -1, 135, 0x00100000, { -1, -1, -1, -1 }, "MissChipShort L" },
    { -1, 0, 0, 0x00000000, { -1, -1, -1, -1 }, "Empty" },
};

/**
 * Offset/Address/Size: 0x2B44 | 0x80055F64 | size: 0xE0
 */
void GoalieSave::ClearData()
{
    if (!mbInitialized)
    {
        return;
    }

    gSaveMap.Clear();

    ClearGrid();

    if (mpSaveTable != NULL)
    {
        delete[] ((u8*)mpSaveTable - 0x10);
    }

    if (mpPositionTable != NULL)
    {
        delete[] ((u8*)mpPositionTable - 0x10);
    }

    mbInitialized = 0;
}

/**
 * Offset/Address/Size: 0x23F4 | 0x80055814 | size: 0x750
 */
void GoalieSave::InitData(Goalie* pGoalie)
{
    if (mbInitialized)
    {
        return;
    }

    muNumSaveEntries = 0x45;
    // PORT: 0x80/0x2290 are the console's sizeof(SaveData) and the array that fits 0x45 of them.
    mpSaveTable = (SaveData*)__construct_new_array(
        nlMalloc((unsigned long)(16 + sizeof(SaveData) * muNumSaveEntries), 8, false),
        NULL, NULL, sizeof(SaveData), muNumSaveEntries);

    muSTSGoalIndexStart = 0;
    muSTSGoalCount = 0;
    muSTSMissIndexStart = 0;
    muSTSMissCount = 0;
    muSTSSaveIndexStart = 0;
    muSTSSaveCount = 0;
    muMissChipIndexStart = 0;
    muMissChipCount = 0;

    mfCrouchDuration = (float)pGoalie->m_pAnimInventory->GetAnim(0x2E)->m_nNumKeys / 30.0f;

    for (unsigned int i = 0; i < muNumSaveEntries; i++)
    {
        mpSaveTable[i].Init(pGoalie, gSaveInfo[i], i);

        if (mpSaveTable[i].muSaveType & 0x00020000)
        {
            muSTSGoalCount++;
            if (muSTSGoalCount == 1)
            {
                muSTSGoalIndexStart = i;
            }
        }
        else if (mpSaveTable[i].muSaveType & 0x00040000)
        {
            muSTSMissCount++;
            if (muSTSMissCount == 1)
            {
                muSTSMissIndexStart = i;
            }
        }
        else if (mpSaveTable[i].muSaveType & 0x00010000)
        {
            muSTSSaveCount++;
            if (muSTSSaveCount == 1)
            {
                muSTSSaveIndexStart = i;
            }
        }
        else if (mpSaveTable[i].muSaveType & 0x00100000)
        {
            muMissChipCount++;
            if (muMissChipCount == 1)
            {
                muMissChipIndexStart = i;
            }
        }

        int animKey = mpSaveTable[i].mnAnimID;
        SaveData* pValue = &mpSaveTable[i];
        gSaveMap.Add(animKey, pValue);
    }

    for (unsigned int i = 0; i < muNumSaveEntries; i++)
    {
        mpSaveTable[i].PostInit(gSaveInfo[i]);
    }

    muNumPositionEntries = 6;
    mpPositionTable = (SavePositionData*)__construct_new_array(nlMalloc(0x70, 8, false), NULL, NULL, 0x10, 6);

    for (unsigned int count = 0; count < muNumPositionEntries; count++)
    {
        mpPositionTable[count].Init(pGoalie, gPositionAnimID[count]);
    }

    ClearGrid();

    int nBallJointIndex = pGoalie->GetBallJointIndex();

    for (unsigned int i = 0; i < muNumSaveEntries; i++)
    {
        SaveData* pSaveData = &mpSaveTable[i];
        GetAnimTriggerInfo(pGoalie, pSaveData->mnAnimID, TriggerCallback, pSaveData);
        pSaveData->mfMilestonePercent[4] = 1.0f;
        pGoalie->GetJointPositionFuture(&pSaveData->mv3SavePos, pSaveData->mnAnimID, nBallJointIndex, pSaveData->mfMilestonePercent[2], true, true, false);
        if (pSaveData->mfMilestonePercent[1] > 0.0f)
        {
            pGoalie->GetJointPositionFuture(&pSaveData->mv3TakeoffPos, pSaveData->mnAnimID, -1, pSaveData->mfMilestonePercent[1], true, true, false);
        }
    }

    int nCount = (int)muNumSaveEntries - 1;
    while (nCount >= 0)
    {
        SaveData* pSaveData = &mpSaveTable[nCount];
        if ((pSaveData->muSaveType & 0xFFFF) != 0)
        {
            AddToGrid(pSaveData);
        }
        nCount--;
    }

    if (getenv("STRIKERS_LOG_NIS") != NULL)
    {
        unsigned int nGridded = 0;
        for (unsigned int i = 0; i < muNumSaveEntries; i++)
        {
            if ((mpSaveTable[i].muSaveType & 0xFFFF) != 0)
            {
                nGridded++;
            }
        }
        OSReport("[goalie] entries=%u gridded=%u net=%.2fx%.2f balljoint=%d\n",
                 muNumSaveEntries, nGridded,
                 (double)cField::GetNet(1.0f)->GetNetWidth(),
                 (double)cField::GetNet(1.0f)->GetNetHeight(),
                 nBallJointIndex);
        float fMinY = 1e9f, fMaxY = -1e9f, fMinZ = 1e9f, fMaxZ = -1e9f;
        int nCols[7];
        for (int c = 0; c < 7; c++) { nCols[c] = 0; }
        for (unsigned int i = 0; i < muNumSaveEntries; i++)
        {
            float fy = mpSaveTable[i].mv3SavePos.y;
            float fz = mpSaveTable[i].mv3SavePos.z;
            if (fy < fMinY) { fMinY = fy; }
            if (fy > fMaxY) { fMaxY = fy; }
            if (fz < fMinZ) { fMinZ = fz; }
            if (fz > fMaxZ) { fMaxZ = fz; }
            float fW = cField::GetNet(1.0f)->GetNetWidth();
            int col = (int)(7.0f * (0.5f * fW + fy) / fW);
            if (col < 0) { col = 0; } else if (col > 6) { col = 6; }
            nCols[col]++;
        }
        OSReport("[goalie] y=[%.2f..%.2f] z=[%.2f..%.2f] cols=%d/%d/%d/%d/%d/%d/%d\n",
                 (double)fMinY, (double)fMaxY, (double)fMinZ, (double)fMaxZ,
                 nCols[0], nCols[1], nCols[2], nCols[3], nCols[4], nCols[5], nCols[6]);
    }

    mbInitialized = 1;
}

template <typename T>
class nlSingleton
{
public:
    static T* s_pInstance;
};

class GameInfoManager
{
public:
    bool IsStunnedGoaliesOn() const;
};

struct MyMiniData
{
    int dist;
    nlListContainer<SaveData*>* list;
};

class MyMiniEntryCursor
{
public:
    MyMiniEntryCursor(DLListEntry<MyMiniData*>* entry)
        : m_Entry(entry)
    {
    }

    bool IsValid() const { return m_Entry != NULL; }
    DLListEntry<MyMiniData*>* Current() const { return m_Entry; }
    /**
     * Offset/Address/Size: 0x2C48 | 0x80056068 | size: 0x58
     */
    void Clear() { m_Entry = NULL; }
    void Next() { m_Entry = m_Entry->m_next; }

private:
    DLListEntry<MyMiniData*>* m_Entry;
};

/**
 * Offset/Address/Size: 0x227C | 0x8005569C | size: 0x178
 */
static void InsertSorted(nlDLListContainer<MyMiniData*>& list, MyMiniData* data)
{
    DLListEntry<MyMiniData*>* head;
    DLListEntry<MyMiniData*>* current = nlDLRingGetStart(list.m_Head);
    head = list.m_Head;

    while (current != NULL)
    {
        if (current->entry->dist > data->dist)
        {
            if (nlDLRingIsStart(head, current))
            {
                DLListEntry<MyMiniData*>* entry = (DLListEntry<MyMiniData*>*)nlMalloc(sizeof(DLListEntry<MyMiniData*>), 8, 0);
                if (entry != NULL)
                {
                    entry->m_next = NULL;
                    entry->m_prev = NULL;
                    entry->entry = data;
                }
                nlDLRingAddStart(&list.m_Head, entry);
                return;
            }

            if (nlDLRingIsStart(head, current))
            {
                head = NULL;
            }
            else
            {
                head = current->m_prev;
            }

            DLListEntry<MyMiniData*>* entry = (DLListEntry<MyMiniData*>*)nlMalloc(sizeof(DLListEntry<MyMiniData*>), 8, 0);
            if (entry != NULL)
            {
                entry->m_next = NULL;
                entry->m_prev = NULL;
                entry->entry = data;
            }
            nlDLRingInsert(&list.m_Head, head, entry);
            return;
        }

        if (nlDLRingIsEnd(head, current) || current == NULL)
        {
            current = NULL;
        }
        else
        {
            current = current->m_next;
        }
    }

    DLListEntry<MyMiniData*>* entry = (DLListEntry<MyMiniData*>*)nlMalloc(sizeof(DLListEntry<MyMiniData*>), 8, 0);
    if (entry != NULL)
    {
        entry->m_next = NULL;
        entry->m_prev = NULL;
        entry->entry = data;
    }
    nlDLRingAddEnd(&list.m_Head, entry);
}

/**
 * Offset/Address/Size: 0x1FC0 | 0x800553E0 | size: 0x2BC
 */
SaveData* GoalieSave::FindBestSave(SaveBlendInfo& blendInfo, const nlVector3& v3LocalPos, float fTime, bool bDoNearSearch, unsigned int uSaveType, bool bFromTakeoff)
{
    int i;
    int j;
    SaveData* pSaveData;
    MyMiniData griddata[7][5];
    int across;
    int up;

    float y;
    float z;
    z = v3LocalPos.z;
    y = v3LocalPos.y;

    float netWidth = cField::GetNet(1.0f)->GetNetWidth();
    float netHeight = cField::GetNet(1.0f)->GetNetHeight();

    i = (int)(7.0f * (0.5f * netWidth + y) / netWidth);
    if (i < 0)
        i = 0;
    else if (i >= 7)
        i = 6;

    j = (int)(5.0f * z / netHeight);
    if (j < 0)
        j = 0;
    else if (j >= 5)
        j = 4;

    if (nlSingleton<GameInfoManager>::s_pInstance->IsStunnedGoaliesOn())
        uSaveType &= ~3;

    pSaveData = GoalieSave::FindBestInList(
        blendInfo,
        gSaveGrid[i][j],
        v3LocalPos,
        fTime,
        uSaveType,
        bFromTakeoff);

    if (bDoNearSearch && pSaveData == NULL)
    {
        nlDLListContainer<MyMiniData*> mylist;

        for (across = 0; across < 7; across++)
        {
            MyMiniData* gridRow = griddata[across];
            nlListContainer<SaveData*>* saveRow = gSaveGrid[across];
            for (up = 0; up < 5; up++)
            {
                int du = j - up;
                int dz = i - across;
                int testDist = dz * dz + du * du;

                if (testDist <= 8)
                {
                    gridRow[up].dist = testDist;
                    gridRow[up].list = &saveRow[up];
                    InsertSorted(mylist, &gridRow[up]);
                }
            }
        }

        MyMiniEntryCursor cursor(nlDLRingGetStart(mylist.m_Head));
        MyMiniEntryCursor head(mylist.m_Head);

        if (nlDLRingIsEnd(head.Current(), cursor.Current()) || !cursor.IsValid())
            cursor.Clear();
        else
            cursor.Next();

        while (cursor.IsValid())
        {
            DLListEntry<MyMiniData*>* current = cursor.Current();
            MyMiniData* data = current->entry;
            nlListContainer<SaveData*>* cellList = data->list;

            if (cellList != NULL)
            {
                pSaveData = GoalieSave::FindBestInList(
                    blendInfo,
                    *cellList,
                    v3LocalPos,
                    fTime,
                    uSaveType,
                    bFromTakeoff);

                if (pSaveData != NULL)
                    break;
            }
            if (nlDLRingIsEnd(head.Current(), current) || current == NULL)
                cursor.Clear();
            else
                cursor.Next();
        }
    }

    return pSaveData;
}

/**
 * Offset/Address/Size: 0x1A1C | 0x80054E3C | size: 0x5A4
 */
SaveData* GoalieSave::FindBestInList(SaveBlendInfo& blendInfo, nlListContainer<SaveData*>& SaveList, const nlVector3& v3LocalPos, float fTime, unsigned int uSaveType, bool bFromTakeoff)
{
    float fClosest = 10000.0f;
    SaveBlendInfo candidateBlendInfo;
    nlVector3 v3AdjLocalPos;
    float fSaveTime;
    SaveData* pConnected;
    int segment;
    int anim;
    float fLastTime;
    float fThisTime;
    unsigned char bEmptySpot;
    float fInvSegTime;

    int milestone = bFromTakeoff ? 1 : 0;
    SaveData* pClosest = NULL;
    nlListIterator<SaveData*> iterator = SaveList.Begin();

    while (iterator.IsValid())
    {
        SaveData* pCur = iterator.Current();

        if (uSaveType & pCur->muSaveType)
        {
            fSaveTime = pCur->mfMilestonePercent[2] * pCur->mfDuration;
            {
                float fMilestoneVal = pCur->mfMilestonePercent[milestone];

                if (fMilestoneVal > 0.0f)
                {
                    float fMilDur = fMilestoneVal * pCur->mfDuration;
                    fSaveTime = fSaveTime - fMilDur;
                    if (bFromTakeoff)
                        nlVec3Add(v3AdjLocalPos, v3LocalPos, pCur->mv3TakeoffPos);
                    else
                        v3AdjLocalPos = v3LocalPos;
                }
                else
                {
                    float fScale = 1.0f - fDefaultMilestoneValues[milestone];
                    v3AdjLocalPos = v3LocalPos;
                    fSaveTime = fSaveTime * fScale;
                }
            }

            if (fSaveTime <= fTime)
            {
                pCur = GoalieSave::GetClosestBlendedPos(candidateBlendInfo, v3AdjLocalPos, pCur);

                fSaveTime = candidateBlendInfo.mfMilestoneTime[2];
                {
                    float fThisTime = candidateBlendInfo.mfMilestoneTime[milestone];

                    if (fThisTime > 0.0f)
                    {
                        fSaveTime = fSaveTime - fThisTime;
                        if (bFromTakeoff)
                            nlVec3Add(v3AdjLocalPos, v3LocalPos, pCur->mv3TakeoffPos);
                    }
                    else
                    {
                        float fScale = 1.0f - fDefaultMilestoneValues[milestone];
                        fSaveTime = fSaveTime * fScale;
                    }
                }

                if (fSaveTime <= fTime)
                {
                    float fDistY = v3AdjLocalPos.y - candidateBlendInfo.mv3BlendedSavePos.y;
                    float fDistSq = fDistY * fDistY
                                  + (v3AdjLocalPos.z - candidateBlendInfo.mv3BlendedSavePos.z)
                                        * (v3AdjLocalPos.z - candidateBlendInfo.mv3BlendedSavePos.z);

                    if (fDistSq < fClosest)
                    {
                        if (fDistSq < mfCatchAllowDistSq || !(pCur->muSaveType & 3))
                        {
                            fClosest = fDistSq;
                            pClosest = pCur;

                            blendInfo = candidateBlendInfo;

                            blendInfo.mfStartTime = (0.0f >= blendInfo.mfMilestoneTime[2] - fTime) ? 0.0f : blendInfo.mfMilestoneTime[2] - fTime;

                            if (bFromTakeoff)
                            {
                                nlVec3Sub(blendInfo.mv3BlendedSavePos, blendInfo.mv3BlendedSavePos, pCur->mv3TakeoffPos);
                            }

                            if (fDistSq < 0.05f * 0.05f)
                                break;
                        }
                    }
                }
            }
        }

        iterator.Next();
    }

    if (pClosest != NULL)
    {

        bEmptySpot = 0;

        {
            for (anim = 0; anim < 4; anim++)
            {
                pConnected = blendInfo.mpSaveData[anim];
                if (pConnected == NULL)
                    continue;

                {
                    fLastTime = 0.0f;

                    for (segment = 0; segment < 5; segment++)
                    {
                        fThisTime = pConnected->mfMilestonePercent[segment] * pConnected->mfDuration;
                        if (fThisTime > 0.0f)
                        {
                            blendInfo.mfMilestoneScale[anim][segment] = fThisTime - fLastTime;
                            fLastTime = fThisTime;
                        }
                        else
                        {
                            blendInfo.mfMilestoneScale[anim][segment] = -1.0f;
                            bEmptySpot = 1;
                        }
                    }
                }
            }
        }

        {
            fLastTime = 0.0f;
            for (segment = 0; segment < 5; segment++)
            {
                fThisTime = blendInfo.mfMilestoneTime[segment];
                if (fThisTime > 0.0f)
                {
                    float fSegDuration = fThisTime - fLastTime;
                    fInvSegTime = 1.0f / fSegDuration;
                    fLastTime = fThisTime;

                    if (blendInfo.mpSaveData[0])
                        blendInfo.mfMilestoneScale[0][segment] *= fInvSegTime;
                    if (blendInfo.mpSaveData[1])
                        blendInfo.mfMilestoneScale[1][segment] *= fInvSegTime;
                    if (blendInfo.mpSaveData[2])
                        blendInfo.mfMilestoneScale[2][segment] *= fInvSegTime;
                    if (blendInfo.mpSaveData[3])
                        blendInfo.mfMilestoneScale[3][segment] *= fInvSegTime;
                }
            }
        }

        if ((unsigned char)bEmptySpot)
        {
            for (segment = 3; segment >= 0; segment--)
            {
                if (blendInfo.mfMilestoneTime[segment] <= 0.0f)
                {
                    for (anim = 0; anim < 4; anim++)
                    {
                        blendInfo.mfMilestoneScale[anim][segment] = blendInfo.mfMilestoneScale[anim][segment + 1];
                    }
                }
            }
        }
    }

    return pClosest;
}

void GoalieSave::FindVerticalBoundingPoints(SaveData* pSaveData, const nlVector3& v3TargetPoint, SaveData** pLoPoint, SaveData** pHiPoint)
{
    SaveData* pHiSaveData = pSaveData;
    SaveData* pLoSaveData = pSaveData;

    while (pHiSaveData != NULL && v3TargetPoint.z > pHiSaveData->mv3SavePos.z)
    {
        pLoSaveData = pHiSaveData;
        pHiSaveData = pHiSaveData->mpConnectedSaveData[0];
    }
    while (pLoSaveData != NULL && v3TargetPoint.z < pLoSaveData->mv3SavePos.z)
    {
        pHiSaveData = pLoSaveData;
        pLoSaveData = pLoSaveData->mpConnectedSaveData[1];
    }
    if (pLoSaveData == NULL)
    {
        *pLoPoint = pHiSaveData;
        *pHiPoint = pHiSaveData;
    }
    else if (pHiSaveData == NULL)
    {
        *pLoPoint = pLoSaveData;
        *pHiPoint = pLoSaveData;
    }
    else
    {
        *pHiPoint = pHiSaveData;
        *pLoPoint = pLoSaveData;
    }
}

/**
 * Offset/Address/Size: 0xF90 | 0x800543B0 | size: 0xA8C
 */
SaveData* GoalieSave::GetClosestBlendedPos(SaveBlendInfo& blendInfo, const nlVector3& v3TargetPos, SaveData* pSaveData)
{
    SaveData* pClosest = pSaveData;
    SaveData* pEdge = NULL;

    SaveData* pLeft = NULL;
    SaveData* pRight = NULL;
    SaveData* pLeftUp = NULL;
    SaveData* pRightUp = NULL;

    float fScaleLeft;
    float fScaleRight;

    blendInfo.mfSaveBlendPrimary = 0.0f;
    blendInfo.mfSaveBlendSecondary = 0.0f;
    blendInfo.mfSaveBlendComposite = 0.0f;

    if (pSaveData->mv3GroupMaxCoords.y > v3TargetPos.y)
    {
        if (pSaveData->mv3GroupMinCoords.y < v3TargetPos.y)
        {
            FindVerticalBoundingPoints(pSaveData, v3TargetPos, &pLeft, &pLeftUp);

            pRight = pLeft;
            pRightUp = pLeftUp;

            unsigned char done = 0;
            while (!done)
            {
                if (v3TargetPos.y <= pLeft->mv3SavePos.y || v3TargetPos.y <= pLeftUp->mv3SavePos.y)
                {
                    if (v3TargetPos.y >= pLeft->mv3SavePos.y || v3TargetPos.y >= pLeftUp->mv3SavePos.y || pLeft->mpConnectedSaveData[3] == NULL)
                    {
                        pEdge = pLeft;
                        break;
                    }
                    else
                    {
                        pRight = pLeft;
                        pRightUp = pLeftUp;
                        FindVerticalBoundingPoints(pLeft->mpConnectedSaveData[3], v3TargetPos, &pLeft, &pLeftUp);
                    }
                }
                else if (v3TargetPos.y >= pRight->mv3SavePos.y || v3TargetPos.y >= pRightUp->mv3SavePos.y)
                {
                    if (v3TargetPos.y <= pRight->mv3SavePos.y || v3TargetPos.y <= pRightUp->mv3SavePos.y || pRight->mpConnectedSaveData[2] == NULL)
                    {
                        pEdge = pRight;
                        break;
                    }
                    else
                    {
                        pLeft = pRight;
                        pLeftUp = pRightUp;
                        FindVerticalBoundingPoints(pRight->mpConnectedSaveData[2], v3TargetPos, &pRight, &pRightUp);
                    }
                }
                else
                {
                    fScaleLeft = 0.0f;
                    fScaleRight = 0.0f;

                    if (pLeft != pLeftUp)
                    {
                        fScaleLeft = (v3TargetPos.z - pLeft->mv3SavePos.z) / (pLeftUp->mv3SavePos.z - pLeft->mv3SavePos.z);
                    }

                    if (pRight != pRightUp)
                    {
                        fScaleRight = (v3TargetPos.z - pRight->mv3SavePos.z) / (pRightUp->mv3SavePos.z - pRight->mv3SavePos.z);
                    }

                    float fLefty = Interpolate(pLeft->mv3SavePos.y, pLeftUp->mv3SavePos.y, fScaleLeft);
                    float fRighty = Interpolate(pRight->mv3SavePos.y, pRightUp->mv3SavePos.y, fScaleRight);
                    float fComposite = (v3TargetPos.y - fLefty) / (fRighty - fLefty);

                    if (fComposite <= 0.001f)
                    {
                        pEdge = pLeft;
                        break;
                    }

                    if (fComposite >= 0.999f)
                    {
                        pEdge = pRight;
                        break;
                    }

                    done = 1;
                    blendInfo.mfSaveBlendComposite = fComposite;

                    float fTimeLeft[5];
                    float fLeftZ;
                    {
                        int milestone;

                        blendInfo.mpSaveData[1] = NULL;
                        if (fScaleLeft <= 0.999f)
                        {
                            blendInfo.mpSaveData[0] = pLeft;
                            if (fScaleLeft >= 0.001f)
                            {
                                blendInfo.mpSaveData[1] = pLeftUp;
                                pClosest = pLeft;
                                fLeftZ = v3TargetPos.z;
                                blendInfo.mfSaveBlendPrimary = fScaleLeft;

                                for (milestone = 0; milestone < 5; milestone++)
                                {
                                    float fLeftTime = pLeft->GetMilestoneTime(milestone);
                                    float fLeftUpTime = pLeftUp->GetMilestoneTime(milestone);

                                    fTimeLeft[milestone] = (fLeftTime <= 0.001f) ? 0.0f : Interpolate(fLeftTime, fLeftUpTime, fScaleLeft);
                                }
                            }
                            else
                            {
                                fLeftZ = pLeft->mv3SavePos.z;
                                for (milestone = 0; milestone < 5; milestone++)
                                {
                                    fTimeLeft[milestone] = pLeft->GetMilestoneTime(milestone);
                                }
                            }
                        }
                        else
                        {
                            blendInfo.mpSaveData[0] = pLeftUp;
                            fLeftZ = pLeftUp->mv3SavePos.z;
                            for (milestone = 0; milestone < 5; milestone++)
                            {
                                fTimeLeft[milestone] = pLeftUp->GetMilestoneTime(milestone);
                            }
                        }
                    }

                    float fTimeRight[5];
                    float fRightZ;
                    {
                        int milestone;

                        blendInfo.mpSaveData[3] = NULL;
                        if (fScaleRight <= 0.999f)
                        {
                            blendInfo.mpSaveData[2] = pRight;
                            if (fScaleRight >= 0.001f)
                            {
                                blendInfo.mpSaveData[3] = pRightUp;
                                fRightZ = v3TargetPos.z;
                                blendInfo.mfSaveBlendSecondary = fScaleRight;

                                for (milestone = 0; milestone < 5; milestone++)
                                {
                                    float fRightTime = pRight->GetMilestoneTime(milestone);
                                    float fRightUpTime = pRightUp->GetMilestoneTime(milestone);

                                    fTimeRight[milestone] = (fRightTime <= 0.001f) ? 0.0f : Interpolate(fRightTime, fRightUpTime, fScaleRight);
                                }
                            }
                            else
                            {
                                fRightZ = pRight->mv3SavePos.z;
                                for (milestone = 0; milestone < 5; milestone++)
                                {
                                    fTimeRight[milestone] = pRight->GetMilestoneTime(milestone);
                                }
                            }
                        }
                        else
                        {
                            blendInfo.mpSaveData[2] = pRightUp;
                            fRightZ = pRightUp->mv3SavePos.z;
                            for (milestone = 0; milestone < 5; milestone++)
                            {
                                fTimeRight[milestone] = pRightUp->GetMilestoneTime(milestone);
                            }
                        }
                    }

                    blendInfo.mv3BlendedSavePos.y = v3TargetPos.y;
                    blendInfo.mv3BlendedSavePos.z = Interpolate(fLeftZ, fRightZ, fComposite);

                    {
                        int milestone;

                        for (milestone = 0; milestone < 5; milestone++)
                        {
                            float fRightTime = fTimeRight[milestone];
                            float fLeftTime = fTimeLeft[milestone];
                            blendInfo.mfMilestoneTime[milestone] = (fLeftTime <= 0.001f) ? 0.0f : Interpolate(fLeftTime, fRightTime, fComposite);
                        }
                    }

                    if (fComposite <= 0.5f)
                    {
                        if (fScaleLeft <= 0.5f)
                            pClosest = pLeft;
                        else
                            pClosest = pLeftUp;
                    }
                    else
                    {
                        if (fScaleRight <= 0.5f)
                            pClosest = pRight;
                        else
                            pClosest = pRightUp;
                    }
                }
            }
        }
        else
        {
            SaveData* pLast;
            SaveData* pCurEdge = pSaveData;
            while (pCurEdge != NULL)
            {
                pLast = pCurEdge;
                pCurEdge = pCurEdge->mpConnectedSaveData[3];
            }
            pEdge = pLast;
        }
    }
    else
    {
        SaveData* pLast;
        SaveData* pCurEdge = pSaveData;
        while (pCurEdge != NULL)
        {
            pLast = pCurEdge;
            pCurEdge = pCurEdge->mpConnectedSaveData[2];
        }
        pEdge = pLast;
    }

    if (pEdge != NULL)
    {
        SaveData* pDown;
        SaveData* pUp;
        int milestone;

        FindVerticalBoundingPoints(pEdge, v3TargetPos, &pDown, &pUp);

        blendInfo.mpSaveData[0] = pDown;
        blendInfo.mpSaveData[1] = NULL;
        blendInfo.mpSaveData[3] = NULL;
        blendInfo.mpSaveData[2] = NULL;

        if (pDown != pUp)
        {
            float fPrimary = (v3TargetPos.z - pDown->mv3SavePos.z) / (pUp->mv3SavePos.z - pDown->mv3SavePos.z);
            if (fPrimary >= 0.999f)
            {
                blendInfo.mv3BlendedSavePos = pUp->mv3SavePos;
                blendInfo.mpSaveData[0] = pUp;
                for (milestone = 0; milestone < 5; milestone++)
                {
                    blendInfo.mfMilestoneTime[milestone] = pUp->GetMilestoneTime(milestone);
                }
            }
            else if (fPrimary <= 0.001f)
            {
                blendInfo.mv3BlendedSavePos = pDown->mv3SavePos;
                for (milestone = 0; milestone < 5; milestone++)
                {
                    blendInfo.mfMilestoneTime[milestone] = pDown->GetMilestoneTime(milestone);
                }
            }
            else
            {
                blendInfo.mfSaveBlendPrimary = fPrimary;
                blendInfo.mv3BlendedSavePos.x = pDown->mv3SavePos.x;
                blendInfo.mv3BlendedSavePos.y = Interpolate(pDown->mv3SavePos.y, pUp->mv3SavePos.y, fPrimary);
                blendInfo.mv3BlendedSavePos.z = v3TargetPos.z;
                blendInfo.mpSaveData[1] = pUp;

                for (milestone = 0; milestone < 5; milestone++)
                {
                    float fDownTime = pDown->GetMilestoneTime(milestone);
                    float fUpTime = pUp->GetMilestoneTime(milestone);

                    blendInfo.mfMilestoneTime[milestone] = (fDownTime <= 0.001f) ? 0.0f : Interpolate(fDownTime, fUpTime, fPrimary);
                }
            }
        }
        else
        {
            blendInfo.mv3BlendedSavePos = pDown->mv3SavePos;
            for (milestone = 0; milestone < 5; milestone++)
            {
                blendInfo.mfMilestoneTime[milestone] = pDown->GetMilestoneTime(milestone);
            }

            if (pDown->mpConnectedSaveData[1] == NULL && pDown->mpConnectedSaveData[0] == NULL)
            {
                const float fNudge = 0.1f;

                if (fabsf(pDown->mv3SavePos.y - v3TargetPos.y) < fNudge)
                {
                    blendInfo.mv3BlendedSavePos.y = v3TargetPos.y;
                }
                else if (pDown->mv3SavePos.y > v3TargetPos.y)
                {
                    blendInfo.mv3BlendedSavePos.y -= fNudge;
                }
                else
                {
                    blendInfo.mv3BlendedSavePos.y += fNudge;
                }

                if (fabsf(pDown->mv3SavePos.z - v3TargetPos.z) < fNudge)
                {
                    blendInfo.mv3BlendedSavePos.z = v3TargetPos.z;
                }
                else if (pDown->mv3SavePos.z > v3TargetPos.z)
                {
                    blendInfo.mv3BlendedSavePos.z -= fNudge;
                }
                else
                {
                    blendInfo.mv3BlendedSavePos.z += fNudge;
                }
            }
        }

        if (blendInfo.mfSaveBlendPrimary < 0.5f)
            pClosest = blendInfo.mpSaveData[0];
        else
            pClosest = blendInfo.mpSaveData[1];
    }

    blendInfo.mv3BlendedSavePos.x = pClosest->mv3SavePos.x;
    return pClosest;
}
/**
 * Offset/Address/Size: 0xF4C | 0x8005436C | size: 0x44
 */
SaveData* GoalieSave::GetMissChipSaveData(bool bLeft, bool bFar)
{
    u32 farFlag = (bFar != 0);
    u32 leftFlag = (bLeft != 0);
    int index = muMissChipIndexStart + (farFlag ? 0 : 2) + leftFlag;
    return &mpSaveTable[index];
}

/**
 * Offset/Address/Size: 0xEBC | 0x800542DC | size: 0x90
 */
SaveData* GoalieSave::GetRandomSTSMissData(bool bCatchAnimOnly)
{
    int index = muSTSGoalIndexStart;
    if (!bCatchAnimOnly)
    {
        if ((u32)muSTSGoalCount > 1)
        {
            static FilteredRandomRange randgen;
            index += randgen.genrand(muSTSGoalCount);
        }
    }
    return &mpSaveTable[index];
}

/**
 * Offset/Address/Size: 0xE98 | 0x800542B8 | size: 0x24
 */
SaveData* GoalieSave::GetSTSSpinMissData(bool bLeft)
{
    u32 index = muSTSMissIndexStart + ((!bLeft) ? 1 : 0);
    return &mpSaveTable[index];
}

/**
 * Offset/Address/Size: 0xE24 | 0x80054244 | size: 0x74
 */
SaveData* GoalieSave::GetRandomSTSSaveData()
{
    static FilteredRandomRange randgen;
    int index = muSTSSaveIndexStart + randgen.genrand(muSTSSaveCount);
    return &mpSaveTable[index];
}

/**
 * Offset/Address/Size: 0xDCC | 0x800541EC | size: 0x58
 */
bool GoalieSave::TriggerCallback(float fTime, float fDuration, unsigned long uEventID, float fIntensity, void* pUserData)
{
    SaveData* pSaveData = (SaveData*)pUserData;

    // PORT: compare against the whole id.
    if (uEventID == 0xCF84E7CDul)
    {
        pSaveData->mfMilestonePercent[2] = fTime;
        pSaveData->mfDuration = fDuration;
    }
    else if (uEventID == 0x56264BBEul)
    {
        pSaveData->mfMilestonePercent[0] = fTime;
    }
    else if (uEventID == 0x0F9524BAul)
    {
        pSaveData->mfMilestonePercent[1] = fTime;
    }
    else if (uEventID == 0x045424B9ul)
    {
        pSaveData->mfMilestonePercent[3] = fTime;
    }
    return true;
}

static inline void AddPointToGrid(SaveData* pSaveData, const nlVector3& v3Point)
{
    float z = v3Point.z;
    float y = v3Point.y;

    float netWidth = cField::GetNet(1.0f)->GetNetWidth();
    float netHeight = cField::GetNet(1.0f)->GetNetHeight();

    int i = (int)(7.0f * (0.5f * netWidth + y) / netWidth);
    if (i < 0)
        i = 0;
    else if (i >= 7)
        i = 6;

    int j = (int)(5.0f * z / netHeight);
    if (j < 0)
        j = 0;
    else if (j >= 5)
        j = 4;

    nlListContainer<SaveData*>& cell = gSaveGrid[i][j];

    nlListIterator<SaveData*> iterator = cell.Begin();
    if (iterator.IsValid())
    {
        while (iterator.IsValid())
        {
            if (iterator.Current() == pSaveData)
                return;
            iterator.Next();
        }
    }

    {
        ListEntry<SaveData*>* newEntry = (ListEntry<SaveData*>*)nlMalloc(sizeof(ListEntry<SaveData*>), 8, false);
        if (newEntry != NULL)
        {
            newEntry->next = NULL;
            newEntry->entry = pSaveData;
        }
        nlListAddStart<ListEntry<SaveData*> >(&cell.m_Head, newEntry, &cell.m_Tail);
    }
}

/**
 * Offset/Address/Size: 0x780 | 0x80053BA0 | size: 0x64C
 */
void GoalieSave::AddAreaToGrid(SaveData* pSaveData)
{
    SaveData* const pRoot = pSaveData;
    SaveData* pCur;
    nlVector3 v3TopRight;
    nlVector3 v3BotLeft;
    float yInc;
    float zInc;
    nlVector3 v3CurColPos;
    nlVector3 v3CurRowPos;
    SaveData* pNextRight;
    SaveData* pCurBot;
    SaveData* pRightCorner;
    SaveData* pNextNextRight;
    SaveData* pCurLeft;
    SaveData* pCurRight;
    SaveData* pCurUp;
    SaveData* pCurRightUp;
    SaveData* pClosest;
    float fCloseDist;

    pCur = pRoot;
    while (pCur != NULL)
    {
        pCurRightUp = pCur;
        pCur = pCur->mpConnectedSaveData[3];
    }
    while (pCurRightUp != NULL)
    {
        pCurRightUp = pCurRightUp->mpConnectedSaveData[0];
    }

    pCur = pRoot;
    while (pCur != NULL)
    {
        pCurUp = pCur;
        pCur = pCur->mpConnectedSaveData[2];
    }
    while (pCurUp != NULL)
    {
        pCurBot = pCurUp;
        pCurUp = pCurUp->mpConnectedSaveData[0];
    }

    {
        SaveData* end;
        pCur = pRoot;
        while (pCur != NULL)
        {
            end = pCur;
            pCur = pCur->mpConnectedSaveData[3];
        }
        while (end != NULL)
        {
            pRightCorner = end;
            end = end->mpConnectedSaveData[1];
        }
    }

    {
        SaveData* end;
        pCur = pRoot;
        while (pCur != NULL)
        {
            end = pCur;
            pCur = pCur->mpConnectedSaveData[2];
        }
        while (end != NULL)
        {
            end = end->mpConnectedSaveData[1];
        }
    }

    yInc = (float)(0.95 * GridSectionWidth());
    zInc = (float)(0.95 * GridSectionHeight());

    pCur = pCurBot;
    v3TopRight = pCurBot->mv3SavePos;
    while (pCur != NULL)
    {
        if (pCur->mv3SavePos.y > v3TopRight.y)
            v3TopRight.y = pCur->mv3SavePos.y;
        pCur = pCur->mpConnectedSaveData[1];
    }
    while (pCurBot != NULL)
    {
        if (pCurBot->mv3SavePos.z > v3TopRight.z)
            v3TopRight.z = pCurBot->mv3SavePos.z;
        pCurBot = pCurBot->mpConnectedSaveData[3];
    }

    pRoot->mv3GroupMaxCoords = v3TopRight;
    float halfYInc = 0.51f * yInc;
    float halfZInc = 0.51f * zInc;
    v3TopRight.y += halfYInc;
    v3TopRight.z += halfZInc;

    pCur = pRightCorner;
    v3BotLeft = pRightCorner->mv3SavePos;
    while (pCur != NULL)
    {
        if (pCur->mv3SavePos.y < v3BotLeft.y)
            v3BotLeft.y = pCur->mv3SavePos.y;
        pCur = pCur->mpConnectedSaveData[0];
    }
    pCur = pRightCorner;
    while (pCur != NULL)
    {
        if (pCur->mv3SavePos.z < v3BotLeft.z)
            v3BotLeft.z = pCur->mv3SavePos.z;
        pCur = pCur->mpConnectedSaveData[2];
    }

    pRoot->mv3GroupMinCoords = v3BotLeft;
    v3BotLeft.y -= halfYInc;
    v3BotLeft.z -= halfZInc;

    pNextRight = pRightCorner;
    pCurBot = pNextRight;
    pNextNextRight = pNextRight->mpConnectedSaveData[2];
    v3CurRowPos = v3BotLeft;

    while (v3CurRowPos.y < v3TopRight.y)
    {
        if (v3CurRowPos.y >= pNextRight->mv3SavePos.y && pNextNextRight != NULL)
        {
            pCurBot = pNextRight;
            pNextRight = pNextNextRight;
            pNextNextRight = pNextNextRight->mpConnectedSaveData[2];
        }
        pCurLeft = pCurBot;
        pCurRight = pNextRight;
        v3CurColPos = v3CurRowPos;

        while (v3CurColPos.z < v3TopRight.z)
        {
            FindVerticalBoundingPoints(pCurLeft, v3CurColPos, &pCurLeft, &pCurUp);

            FindVerticalBoundingPoints(pCurRight, v3CurColPos, &pCurRight, &pCurRightUp);

            {
                float dy = pCurLeft->mv3SavePos.y - v3CurColPos.y;
                float dz = pCurLeft->mv3SavePos.z - v3CurColPos.z;
                fCloseDist = nlGetLengthSquared2D(dy, dz);
                pClosest = pCurLeft;

                if (pCurLeft != pCurUp)
                {
                    float upDy = pCurUp->mv3SavePos.y - v3CurColPos.y;
                    float upDz = pCurUp->mv3SavePos.z - v3CurColPos.z;
                    float d = nlGetLengthSquared2D(upDy, upDz);
                    if (d < fCloseDist)
                    {
                        fCloseDist = d;
                        pClosest = pCurUp;
                    }
                }

                if (pCurLeft != pCurRight)
                {
                    float rightDy = pCurRight->mv3SavePos.y - v3CurColPos.y;
                    float rightDz = pCurRight->mv3SavePos.z - v3CurColPos.z;
                    float d = nlGetLengthSquared2D(rightDy, rightDz);
                    if (d < fCloseDist)
                    {
                        fCloseDist = d;
                        pClosest = pCurRight;
                    }
                    if (pCurRight != pCurRightUp)
                    {
                        float upRightDy = pCurRightUp->mv3SavePos.y - v3CurColPos.y;
                        float upRightDz = pCurRightUp->mv3SavePos.z - v3CurColPos.z;
                        float fUpRightDistSq = nlGetLengthSquared2D(upRightDy, upRightDz);
                        if (fUpRightDistSq < fCloseDist)
                        {
                            fCloseDist = fUpRightDistSq;
                            pClosest = pCurRightUp;
                        }
                    }
                }
            }

            pClosest->mv3GroupMinCoords = pRoot->mv3GroupMinCoords;
            pClosest->mv3GroupMaxCoords = pRoot->mv3GroupMaxCoords;
            AddPointToGrid(pClosest, v3CurColPos);
            v3CurColPos.z += zInc;
        }
        v3CurRowPos.y += yInc;
    }
}

void GoalieSave::ClearGrid()
{
    for (int i = 0; i < 7; ++i)
    {
        for (int j = 0; j < 5; ++j)
        {
            gSaveGrid[i][j].Clear();
        }
    }
}

static inline void Local2GridCoords(float y, float z, int& i, int& j)
{
    float netWidth = cField::GetNet(1.0f)->GetNetWidth();
    float netHeight = cField::GetNet(1.0f)->GetNetHeight();
    i = (int)(7.0f * (0.5f * netWidth + y) / netWidth);
    if (i < 0)
        i = 0;
    else if (i >= 7)
        i = 6;
    j = (int)(5.0f * z / netHeight);
    if (j < 0)
        j = 0;
    else if (j >= 5)
        j = 4;
}

static inline int AbsInt(int value)
{
    return value < 0 ? -value : value;
}

static inline float IdentityFloat(float value)
{
    return value;
}

/**
 * Offset/Address/Size: 0x390 | 0x800537B0 | size: 0x3F0
 */
void GoalieSave::AddSegmentToGrid(SaveData* pSaveData1, SaveData* pSaveData2)
{
    int divisions;
    SaveData* pCurSaveData;
    int count;
    int i, j, m, n;
    nlVector3 v3CurPos;
    nlVector3 v3Delta;

    Local2GridCoords(pSaveData1->mv3SavePos.y, pSaveData1->mv3SavePos.z, i, j);
    Local2GridCoords(pSaveData2->mv3SavePos.y, pSaveData2->mv3SavePos.z, m, n);
    divisions = AbsInt(i - m) + AbsInt(j - n);
    nlVec3Sub(v3Delta, pSaveData2->mv3SavePos, pSaveData1->mv3SavePos);
    if (divisions > 0)
    {
        nlVec3Scale(v3Delta, v3Delta, 1.0f / (float)divisions);
    }
    v3CurPos = pSaveData1->mv3SavePos;
    for (count = 0; count <= divisions; count++)
    {
        if (nlGetLengthSquared2D(pSaveData1->mv3SavePos.y - v3CurPos.y,
                pSaveData1->mv3SavePos.z - v3CurPos.z)
            < nlGetLengthSquared2D(pSaveData2->mv3SavePos.y - v3CurPos.y,
                IdentityFloat(pSaveData2->mv3SavePos.z - v3CurPos.z)))
            pCurSaveData = pSaveData1;
        else
            pCurSaveData = pSaveData2;
        AddPointToGrid(pCurSaveData, v3CurPos);
        nlVec3Add(v3CurPos, v3CurPos, v3Delta);
    }
}

/**
 * Offset/Address/Size: 0x20C | 0x8005362C | size: 0x184
 */
void GoalieSave::AddChainToGrid(SaveData* pSaveData, bool bVertical)
{
    SaveData* pEnd;
    int dir;
    int oppdir;

    if (bVertical)
    {
        dir = 0;
        oppdir = 1;
    }
    else
    {
        dir = 2;
        oppdir = 3;
    }

    SaveData* p = pSaveData;
    while (p != NULL)
    {
        pEnd = p;
        p = p->mpConnectedSaveData[oppdir];
    }

    pSaveData->mv3GroupMaxCoords = pSaveData->mv3SavePos;
    pSaveData->mv3GroupMinCoords = pSaveData->mv3SavePos;

    SaveData* pCur = pEnd;
    SaveData* pLast;

    while (pCur != NULL)
    {
        if (pCur->mv3SavePos.x > pSaveData->mv3GroupMaxCoords.x)
            pSaveData->mv3GroupMaxCoords.x = pCur->mv3SavePos.x;
        if (pCur->mv3SavePos.y > pSaveData->mv3GroupMaxCoords.y)
            pSaveData->mv3GroupMaxCoords.y = pCur->mv3SavePos.y;
        if (pCur->mv3SavePos.z > pSaveData->mv3GroupMaxCoords.z)
            pSaveData->mv3GroupMaxCoords.z = pCur->mv3SavePos.z;

        if (pCur->mv3SavePos.x < pSaveData->mv3GroupMinCoords.x)
            pSaveData->mv3GroupMinCoords.x = pCur->mv3SavePos.x;
        if (pCur->mv3SavePos.y < pSaveData->mv3GroupMinCoords.y)
            pSaveData->mv3GroupMinCoords.y = pCur->mv3SavePos.y;
        if (pCur->mv3SavePos.z < pSaveData->mv3GroupMinCoords.z)
            pSaveData->mv3GroupMinCoords.z = pCur->mv3SavePos.z;

        pLast = pCur;
        SaveData* next = pCur->mpConnectedSaveData[dir];
        pCur = next;
        if (next != NULL)
        {
            AddSegmentToGrid(pLast, next);
        }
    }

    while (pLast != NULL)
    {
        pLast->mv3GroupMaxCoords = pSaveData->mv3GroupMaxCoords;
        pLast->mv3GroupMinCoords = pSaveData->mv3GroupMinCoords;
        pLast = pLast->mpConnectedSaveData[oppdir];
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80053420 | size: 0x20C
 */
#pragma dont_inline on
void GoalieSave::AddToGrid(SaveData* pSaveData)
{
    if (pSaveData->mpConnectedSaveData[1] != NULL || pSaveData->mpConnectedSaveData[0] != NULL)
    {
        if (pSaveData->mpConnectedSaveData[2] != NULL || pSaveData->mpConnectedSaveData[3] != NULL)
        {
            AddAreaToGrid(pSaveData);
            return;
        }
        AddChainToGrid(pSaveData, true);
        return;
    }

    if (pSaveData->mpConnectedSaveData[2] != NULL || pSaveData->mpConnectedSaveData[3] != NULL)
    {
        AddChainToGrid(pSaveData, false);
        return;
    }

    AddPointToGrid(pSaveData, pSaveData->mv3SavePos);
    pSaveData->mv3GroupMinCoords = pSaveData->mv3SavePos;
    pSaveData->mv3GroupMaxCoords = pSaveData->mv3SavePos;
}
#pragma dont_inline reset

float GoalieSave::GridSectionWidth()
{
    return cField::GetNet(1.0f)->GetNetWidth() / 7.0f;
}

float GoalieSave::GridSectionHeight()
{
    return cField::GetNet(1.0f)->GetNetHeight() / 5.0f;
}
