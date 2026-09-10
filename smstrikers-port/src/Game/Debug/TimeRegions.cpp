#include "NL/nlDLRing.h"
#include "Game/Debug/TimeRegions.h"

#include "Game/BasicStadium.h"
#include "NL/nlTask.h"
#include "Game/Ball.h"
#include "Game/Camera/CameraMan.h"

#include "Game/Debug/FrameCounter.h"
#include "NL/nlPrint.h"

TimeRegion* pGamePlayTimeRegion;
TimeRegion* pNISTimeRegion;
TimeRegion* pAutoReplayTimeRegion;
TimeRegion* pCentreFieldTimeRegion;
TimeRegion* pLeftFieldTimeRegion;
TimeRegion* pRightFieldTimeRegion;
TimeRegion* pBowserTimeRegion;
TimeRegion* pShotTimeRegion;

/**
 * Offset/Address/Size: 0x838 | 0x80162658 | size: 0x50
 */
void WriteFrameRateStatsToFile()
{
    char buffer[0x78];
    BasicStadium* stadium = BasicStadium::GetCurrentStadium();
    nlSNPrintf(buffer, sizeof(buffer), "\n\nStats collected in %s\n", stadium->m_szBaseName);
    g_FrameCounter.WriteFrameRateStatsToFile(buffer);
}

/**
 * Offset/Address/Size: 0x820 | 0x80162640 | size: 0x18
 */
static bool IsDuringGameplay()
{
    return (nlTaskManager::m_pInstance->m_CurrState == 0x2);
}

/**
 * Offset/Address/Size: 0x7B8 | 0x801625D8 | size: 0x68
 */
static bool CentreOfField()
{
    const nlVector3& targetPosition = (*nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack)).GetTargetPosition();
    bool isCenter = false;
    if ((nlTaskManager::m_pInstance->m_CurrState == 0x2) && ((float)fabs(targetPosition.x) < 5.7f))
    {
        isCenter = true;
    }
    return isCenter;
}

inline bool IsInCenterZone()
{
    const nlVector3& v = nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack)->GetTargetPosition();
    unsigned long curState = nlTaskManager::m_pInstance->m_CurrState;

    return curState == 2 && fabsf(v.x) < 5.7f;
}

/**
 * Offset/Address/Size: 0x6DC | 0x801624FC | size: 0xDC
 */
static bool LeftSideOfField()
{
    const nlVector3& v = nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack)->GetTargetPosition();
    unsigned long curState = nlTaskManager::m_pInstance->m_CurrState;

    return curState == 2 && !IsInCenterZone() && v.x < 0.0f;
}

/**
 * Offset/Address/Size: 0x600 | 0x80162420 | size: 0xDC
 */
static bool RightSideOfField()
{
    const nlVector3& v = nlDLRingGetStart<cBaseCamera>(cCameraManager::m_cameraStack)->GetTargetPosition();
    unsigned long curState = nlTaskManager::m_pInstance->m_CurrState;

    return curState == 2 && !IsInCenterZone() && v.x > 0.0f;
}

/**
 * Offset/Address/Size: 0x5E8 | 0x80162408 | size: 0x18
 */
static bool IsDuringNIS()
{
    return (nlTaskManager::m_pInstance->m_CurrState == 0x100);
}

/**
 * Offset/Address/Size: 0x5D0 | 0x801623F0 | size: 0x18
 */
static bool IsDuringAutoreplay()
{
    return (nlTaskManager::m_pInstance->m_CurrState == 0x10);
}

/**
 * Offset/Address/Size: 0x598 | 0x801623B8 | size: 0x38
 */
static bool IsBowserAround()
{
    return (BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->meBowserState != BOWSER_STATE_HIDDEN);
}

/**
 * Offset/Address/Size: 0x580 | 0x801623A0 | size: 0x18
 */
static bool IsShotInProgress()
{
    return (g_pBall->m_tShotTimer.m_uPackedTime != 0);
}

/**
 * Offset/Address/Size: 0x11C | 0x80161F3C | size: 0x464
 */
void InitializeTimeRegions()
{
    pGamePlayTimeRegion = new TimeRegion("during gameplay", IsDuringGameplay);
    pNISTimeRegion = new TimeRegion("during NIS", IsDuringNIS);
    pAutoReplayTimeRegion = new TimeRegion("during auto-replay", IsDuringAutoreplay);
    pCentreFieldTimeRegion = new TimeRegion("at centre of field", CentreOfField);
    pLeftFieldTimeRegion = new TimeRegion("on left side of field", LeftSideOfField);
    pRightFieldTimeRegion = new TimeRegion("on right side of field", RightSideOfField);
    pBowserTimeRegion = new TimeRegion("when bowser is around", IsBowserAround);
    pShotTimeRegion = new TimeRegion("when shot in progress", IsShotInProgress);
}

/**
 * Offset/Address/Size: 0x0 | 0x80161E20 | size: 0x11C
 */
void DestroyTimeRegions()
{
    delete pGamePlayTimeRegion;
    delete pNISTimeRegion;
    delete pAutoReplayTimeRegion;
    delete pCentreFieldTimeRegion;
    delete pLeftFieldTimeRegion;
    delete pRightFieldTimeRegion;
    delete pBowserTimeRegion;
    delete pShotTimeRegion;
}
