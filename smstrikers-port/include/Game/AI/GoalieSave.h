#ifndef _GOALIESAVE_H_
#define _GOALIESAVE_H_

#include "NL/nlMath.h"
#include "NL/nlList.h"
#include "NL/nlAVLTree.h"

struct SaveInfo;
class Goalie;

class SavePositionData
{
public:
    void Init(Goalie* pGoalie, int animID);

    /* 0x00 */ int mnAnimID;
    /* 0x04 */ float mfAnimDistance;
    /* 0x08 */ float mfAnimTime;
    /* 0x0C */ float mfAnimVelocity;
}; // total size: 0x10

class SaveData
{
public:
    static float LookupFatigueValue(Goalie* pGoalie, const SaveInfo& info);
    void Init(Goalie* pGoalie, const SaveInfo& info, unsigned int uIndex);
    void PostInit(const SaveInfo& info);
    float GetMilestoneTime(int milestone) const
    {
        return mfMilestonePercent[milestone] * mfDuration;
    }

    /* 0x00 */ int mnAnimID;
    /* 0x04 */ SaveData* mpFailAnimData;
    /* 0x08 */ int mnRecoverAnimID;
    /* 0x0C */ unsigned int muSaveType;
    /* 0x10 */ nlVector3 mv3SavePos;
    /* 0x1C */ nlVector3 mv3TakeoffPos;
    /* 0x28 */ float mfDuration;
    /* 0x2C */ float mfMilestonePercent[5];
    /* 0x40 */ float mfFatigueValue;
    /* 0x44 */ SaveData* mpConnectedSaveData[4];
    /* 0x54 */ nlVector3 mv3GroupMinCoords;
    /* 0x60 */ nlVector3 mv3GroupMaxCoords;
    /* 0x6C */ char mszName[16];
    /* 0x7C */ int muIndex;
}; // total size: 0x80

struct SaveBlendInfo
{
public:
    /* 0x00 */ float mfStartTime;
    /* 0x04 */ float mfMilestoneTime[5];
    /* 0x18 */ float mfMilestoneScale[4][5];
    /* 0x68 */ float mfSaveBlendPrimary;
    /* 0x6C */ float mfSaveBlendSecondary;
    /* 0x70 */ float mfSaveBlendComposite;
    /* 0x74 */ class SaveData* mpSaveData[4];
    /* 0x84 */ class nlVector3 mv3BlendedSavePos;
}; // total size: 0x90

class GoalieSave
{
    static void FindVerticalBoundingPoints(SaveData* pSaveData, const nlVector3& v3TargetPoint, SaveData** pLoPoint, SaveData** pHiPoint);

public:
    static SaveData* FindSaveData(int animID);
    static void ClearData();
    static void InitData(Goalie* pGoalie);
    static SaveData* FindBestSave(SaveBlendInfo& blendInfo, const nlVector3& v3LocalPos, float fTime, bool bDoNearSearch, unsigned int uSaveType, bool bFromTakeoff);
    static SaveData* FindBestInList(SaveBlendInfo& blendInfo, nlListContainer<SaveData*>& SaveList, const nlVector3& v3LocalPos, float fTime, unsigned int uSaveType, bool bFromTakeoff);
    static SaveData* GetClosestBlendedPos(SaveBlendInfo& blendInfo, const nlVector3& v3TargetPos, SaveData* pSaveData);
    static SaveData* GetMissChipSaveData(bool bLeft, bool bFar);
    static SaveData* GetRandomSTSMissData(bool bCatchAnimOnly);
    static SaveData* GetSTSSpinMissData(bool bLeft);
    static SaveData* GetRandomSTSSaveData();
    static bool TriggerCallback(float fTime, float fDuration, unsigned long uEventID, float fIntensity, void* pUserData);
    static void AddAreaToGrid(SaveData* pSaveData);
    static void AddSegmentToGrid(SaveData* pSaveData1, SaveData* pSaveData2);
    static void AddChainToGrid(SaveData* pSaveData, bool bVertical);
    static void AddToGrid(SaveData* pSaveData);
    static void ClearGrid();
    static float SetCatchAllowDist(float fDist);
    static float GridSectionWidth();
    static float GridSectionHeight();

    static float mfCatchAllowDist;
    static float mfCatchAllowDistSq;
    static SaveData* mpSaveTable;
    static unsigned int muNumSaveEntries;
    static unsigned int muNumPositionEntries;
    static unsigned int muSTSMissIndexStart;
    static unsigned int muSTSMissCount;
    static unsigned int muSTSGoalIndexStart;
    static unsigned int muSTSGoalCount;
    static unsigned int muSTSSaveIndexStart;
    static unsigned int muSTSSaveCount;
    static unsigned int muMissChipIndexStart;
    static unsigned int muMissChipCount;
    static float mfCrouchDuration;
    static SavePositionData* mpPositionTable;
    static unsigned char mbInitialized;
};

#endif // _GOALIESAVE_H_
