#include "Game/Team.h"

#include "Game/AI/Fielder.h"
#include "Game/Audio/WorldAudio.h"
#include "Game/GameInfo.h"
#include "Game/OverlayHandlerHUD.h"
#include "Game/OverlayManager.h"
#include "Game/AI/AiUtil.h"
#include "Game/AI/Powerups.h"
#include "Game/Ball.h"
#include "Game/Game.h"
#include "Game/BasicStadium.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/Sys/audio.h"

#include "Game/AI/Fuzzy.h"
#include "Game/AI/Scripts/ScriptCaching.h"
#include "Game/AI/Scripts/ScriptQuestions.h"
#include "Game/FormationDefines.h"

#include "NL/nlMath.h"
#include <stdlib.h>

cTeam* g_pTeams[2] = { NULL, NULL };
cTeam* g_pCurrentlyUpdatingTeam;
static const nlVector2 g_vMarkDistanceConfidence = { 15.0f, 1.0f };
static const unsigned short g_aNeutralPlayerFacingDirections[5] = {
    0,
    0,
    0,
    0,
    0,
};
static const unsigned short g_aAdvantagePlayerFacingDirections[5] = {
    0x00B4,
    0x00B4,
    0x00B4,
    0x00B4,
    0,
};

/**
 * Offset/Address/Size: 0x1CF0 | 0x8006609C | size: 0x138
 */
cTeam::cTeam(int nSide)
    : m_nSide(nSide)
    , m_nScore(0)
    , m_nCurrentPowerUp(0)
    , mfPowerupMeter(0.0f)
    , mfPowerupTimer(0.0f)
    , meCurrentTeamStyle(TEAM_STYLE_MODERATE)
    , mbHasToggledPowerup(false)
{
    m_ePowerupList[0].nnumOfPowerups = 0;
    m_ePowerupList[0].eType = POWER_UP_NONE;
    m_ePowerupList[1].nnumOfPowerups = 0;
    m_ePowerupList[1].eType = POWER_UP_NONE;

    for (int i = 0; i < 5; i++)
    {
        m_pPlayers[i] = NULL;
    }

    for (int i = 0; i < 4; i++)
    {
        m_pAIOrderedFielders[i] = NULL;
        m_pBallInterceptOrderedFielders[i] = NULL;
    }

    m_pNet = new (nlMalloc(sizeof(cNet), 8, false)) cNet(nSide);
    m_pFormationManager = new (nlMalloc(sizeof(FormationManager), 8, false)) FormationManager(this);
}

/**
 * Offset/Address/Size: 0x1C8C | 0x80066038 | size: 0x64
 */
cTeam::~cTeam()
{
    delete m_pNet;
    delete m_pFormationManager;
}

/**
 * Offset/Address/Size: 0x1C1C | 0x80065FC8 | size: 0x70
 */
void cTeam::SetDifficulty(eDifficultyID difficulty)
{
    if (difficulty < 0)
    {
        difficulty = DIFF_MEDIUM;
    }

    SkillTweaks::GetSkillTweaks(m_nSide)->Init(difficulty, false);
    SkillTweaks::GetSkillTweaks(m_nSide)->HookupTweakeables(m_nSide); // actually a dead function
}

/**
 * Offset/Address/Size: 0x1C00 | 0x80065FAC | size: 0x1C
 */
void cTeam::ClearAllPowerUps()
{
    m_ePowerupList[0].eType = POWER_UP_NONE;
    m_ePowerupList[0].nnumOfPowerups = 0;
    m_ePowerupList[1].eType = POWER_UP_NONE;
    m_ePowerupList[1].nnumOfPowerups = 0;
}

/**
 * Offset/Address/Size: 0x1B94 | 0x80065F40 | size: 0x6C
 */
void cTeam::ClearCurrentPowerUp()
{
    if (m_ePowerupList[0].eType == POWER_UP_NONE)
    {
        m_ePowerupList[1].eType = POWER_UP_NONE;
        m_ePowerupList[1].nnumOfPowerups = 0;
        mbHasToggledPowerup = false;
        return;
    }

    m_ePowerupList[0].eType = POWER_UP_NONE;
    m_ePowerupList[0].nnumOfPowerups = 0;

    if (m_ePowerupList[1].eType != POWER_UP_NONE)
    {
        if (!mbHasToggledPowerup)
        {
            m_ePowerupList[0].eType = m_ePowerupList[1].eType;
            m_ePowerupList[0].nnumOfPowerups = m_ePowerupList[1].nnumOfPowerups;
            m_ePowerupList[1].eType = POWER_UP_NONE;
            m_ePowerupList[1].nnumOfPowerups = 0;
        }
    }
    else
    {
        mbHasToggledPowerup = false;
    }
}

/**
 * Offset/Address/Size: 0x1A4C | 0x80065DF8 | size: 0x148
 */
#pragma push
#pragma opt_propagation off
void cTeam::TogglePowerup(bool bIsSilent)
{
    static bool bAudioToggleSwitch = true;

    PowerUpTeamType eTemp = m_ePowerupList[1];
    m_ePowerupList[1] = m_ePowerupList[0];
    m_ePowerupList[0] = eTemp;

    if (!bIsSilent)
    {
        unsigned int alwaysZero = 0;
        if (alwaysZero == 0)
        {
            if ((eTemp.eType != POWER_UP_NONE) || (m_ePowerupList[1].eType != POWER_UP_NONE))
            {
                if (bAudioToggleSwitch)
                {
                    Audio::gWorldSFX.Play(Audio::WORLDSFX_FILTER_START, 100.0f, -1.0f, true, 100.0f);
                }
                else
                {
                    Audio::gWorldSFX.Play(Audio::WORLDSFX_FILTER_END, 100.0f, -1.0f, true, 100.0f);
                }
                bAudioToggleSwitch = !bAudioToggleSwitch;
            }
        }
    }

    OverlayManager* pOverlayManager = OverlayManager::s_pInstance;
    HUDOverlay* HUD = (HUDOverlay*)pOverlayManager->GetScene(OVERLAY_HUD);
    HUD->SwapPowerUps(m_nSide);

    if (m_ePowerupList[1].eType != POWER_UP_NONE || m_ePowerupList[0].eType == POWER_UP_NONE)
    {
        mbHasToggledPowerup = true;
    }
    else
    {
        mbHasToggledPowerup = false;
    }
}
#pragma pop

/**
 * Offset/Address/Size: 0x1910 | 0x80065CBC | size: 0x13C
 */
bool cTeam::IncrementPowerupMeter(float fAdjustAmount)
{
    int nPowerupIndex = -1;
    mfPowerupMeter += fAdjustAmount;
    if (mfPowerupMeter >= 1.0f)
    {
        mfPowerupMeter -= 1.0f;
        nPowerupIndex = PowerupBase::AwardPowerup(this);
    }
    if (nPowerupIndex < 0)
    {
        bool bEmptySpot = false;
        for (int i = 0; i < 2; i++)
        {
            if (GetPowerUpByIndex(i).eType == POWER_UP_NONE)
            {
                bEmptySpot = true;
            }
        }
        if (!bEmptySpot)
        {
            return false;
        }
    }
    return true;
}

/**
 * Offset/Address/Size: 0x18D0 | 0x80065C7C | size: 0x40
 */
PowerUpTeamType cTeam::GetCurrentPowerUp() const
{
    if (m_ePowerupList[0].eType == POWER_UP_NONE)
    {
        return m_ePowerupList[1];
    }
    return m_ePowerupList[0];
}

/**
 * Offset/Address/Size: 0x18BC | 0x80065C68 | size: 0x14
 */
bool cTeam::IsCurrentNoPowerup() const
{
    return m_ePowerupList[0].eType == POWER_UP_NONE;
}

/**
 * Offset/Address/Size: 0x18A8 | 0x80065C54 | size: 0x14
 */
bool cTeam::IsCurrentMushroom() const
{
    return m_ePowerupList[0].eType == POWER_UP_MUSHROOM;
}

/**
 * Offset/Address/Size: 0x1894 | 0x80065C40 | size: 0x14
 */
bool cTeam::IsCurrentStar() const
{
    return m_ePowerupList[0].eType == POWER_UP_STAR;
}

/**
 * Offset/Address/Size: 0x183C | 0x80065BE8 | size: 0x58
 */
PowerUpTeamType cTeam::GetPowerUpByIndex(int index) const
{
    PowerUpTeamType eDummy;
    if (index >= 0)
    {
        return m_ePowerupList[index];
    }
    eDummy.eType = POWER_UP_NONE;
    eDummy.nnumOfPowerups = 0;
    return eDummy;
}

/**
 * Offset/Address/Size: 0x1824 | 0x80065BD0 | size: 0x18
 */
void cTeam::SetIsPowerUpNew(int index, bool isNew)
{
    if (index >= 0)
    {
        m_ePowerupList[index].bIsNew = isNew;
    }
}

/**
 * Offset/Address/Size: 0x17D0 | 0x80065B7C | size: 0x54
 */
void cTeam::SetCurrentPowerUp(ePowerUpType eNewPowerUpType, int nnumOfPowerups)
{
    unsigned char bGivenNewPowerup = 0;
    for (int a = 0; a < 2; ++a)
    {
        if (m_ePowerupList[a].eType == POWER_UP_NONE && !bGivenNewPowerup)
        {
            m_ePowerupList[a].eType = eNewPowerUpType;
            bGivenNewPowerup = 1;
            m_ePowerupList[a].nnumOfPowerups = nnumOfPowerups;
            m_ePowerupList[a].bIsNew = 1;
        }
    }
}

/**
 * Offset/Address/Size: 0x17B0 | 0x80065B5C | size: 0x20
 */
void cTeam::SetPlayer(cPlayer* pPlayer, int nIndex)
{
    m_pPlayers[nIndex] = pPlayer;
    if (nIndex < 4)
    {
        m_pAIOrderedFielders[nIndex] = (cFielder*)pPlayer;
        m_pBallInterceptOrderedFielders[nIndex] = (cFielder*)pPlayer;
    }
}

/**
 * Offset/Address/Size: 0x17A8 | 0x80065B54 | size: 0x8
 */
void cTeam::SetGoalie(Goalie* pGoalie)
{
    m_pPlayers[4] = pGoalie;
}

/**
 * Offset/Address/Size: 0x17A0 | 0x80065B4C | size: 0x8
 */
Goalie* cTeam::GetGoalie()
{
    return (Goalie*)m_pPlayers[4];
}

/**
 * Offset/Address/Size: 0x1734 | 0x80065AE0 | size: 0x6C
 */
cPlayer* cTeam::GetControlledPlayer(cGlobalPad* pController)
{
    cPlayer* pRetval = nullptr;
    for (int i = 0; i < 5; i++)
    {
        if (m_pPlayers[i]->GetGlobalPad() == pController)
        {
            pRetval = m_pPlayers[i];
            break;
        }
    }
    return pRetval;
}

/**
 * Offset/Address/Size: 0x16B4 | 0x80065A60 | size: 0x80
 */
int cTeam::GetNumAssignedControllers()
{
    int mySide, numAssignedControllers;
    unsigned short i;
    short playingSide;

    numAssignedControllers = 0;
    for (i = 0; i < 4; i++)
    {
        mySide = m_nSide;
        playingSide = GameInfoManager::Instance()->GetPlayingSide(i);
        if (playingSide == mySide)
        {
            numAssignedControllers++;
        }
    }
    return numAssignedControllers;
}

/**
 * Offset/Address/Size: 0x16A4 | 0x80065A50 | size: 0x10
 */
cFielder* cTeam::GetFielder(int nIndex)
{
    return (cFielder*)m_pPlayers[nIndex];
}

/**
 * Offset/Address/Size: 0x1694 | 0x80065A40 | size: 0x10
 */
cPlayer* cTeam::GetPlayer(int nIndex)
{
    return m_pPlayers[nIndex];
}

/**
 * Offset/Address/Size: 0x167C | 0x80065A28 | size: 0x18
 */
cTeam* cTeam::GetOtherTeam()
{
    return g_pTeams[m_nSide == HOME ? AWAY : HOME];
}

/**
 * Offset/Address/Size: 0x1660 | 0x80065A0C | size: 0x1C
 */
cNet* cTeam::GetOtherNet()
{
    return g_pTeams[m_nSide == HOME ? AWAY : HOME]->m_pNet;
}

/**
 * Offset/Address/Size: 0x15F8 | 0x800659A4 | size: 0x68
 */
void cTeam::PreUpdate(float fDeltaT)
{
    for (int i = 0; i < 5; i++)
    {
        m_pPlayers[i]->PreUpdate(fDeltaT);
    }
}

/**
 * Offset/Address/Size: 0x132C | 0x800656D8 | size: 0x2CC
 */
void cTeam::Update(float fDeltaT)
{
    g_pCurrentlyUpdatingTeam = this;
    if (g_pGame->IsGameplayOrOvertime())
    {
        mfPowerupTimer -= fDeltaT;
        if (mfPowerupTimer < 0.0f)
        {
            mfPowerupTimer = 10.0f;
            if (nlSingleton<GameInfoManager>::Instance()->IsInfinitePowerupsOn())
                PowerupBase::AwardPowerup(this);
        }
        mtTeamStyleTimer.Countdown(fDeltaT, 0.0f);
        mtMarkTimer.Countdown(fDeltaT, 0.0f);
        mtRoleTimer.Countdown(fDeltaT, 0.0f);
        mtBallInterceptTimer.Countdown(fDeltaT, 0.0f);
        float offensive = Offensive(this);
        if (offensive && InOffensiveZone(g_pBall->m_v3Position, (eTeamSide)m_nSide) < 0.5f)
        {
            float stalling = Stalling(this);
            if (stalling < 1.0f)
                mtDefensiveZoneTimer.Countup(fDeltaT, 10.0f);
        }
        else
            mtDefensiveZoneTimer.Countdown(2.0f * fDeltaT, 0.0f);
    }
    UpdateBallInterceptTime(fDeltaT);
    UpdateTeamAI(fDeltaT);
    UpdatePlays(fDeltaT);
}

void cTeam::UpdatePlays(float fDeltaT)
{
    int i;
    for (i = 0; i < 4; i++)
        m_pAIOrderedFielders[i]->UpdatePlay(fDeltaT);
}

/**
 * Offset/Address/Size: 0x1100 | 0x800654AC | size: 0x22C
 */
void cTeam::UpdateControllers()
{
    cAIPad* pAIPads = AIPadManager::mAIPads;

    for (int i = 0; i < 4; i++)
    {
        if (m_nSide != (s16)nlSingleton<GameInfoManager>::Instance()->GetPlayingSide((u16)i))
        {
            for (int j = 0; j < 5; j++)
            {
                if (m_pPlayers[j]->m_pController == &pAIPads[i])
                {
                    m_pPlayers[j]->SetAIPad(NULL);
                    break;
                }
            }
        }
        else
        {
            int playerIdx;
            for (playerIdx = 0; playerIdx < 5; playerIdx++)
            {
                if (m_pPlayers[playerIdx]->m_pController == &pAIPads[i])
                {
                    break;
                }
            }

            if (playerIdx == 5)
            {
                for (int j = 0; j < 5; j++)
                {
                    if (m_pPlayers[j]->m_pController == NULL)
                    {
                        m_pPlayers[j]->SetAIPad(&pAIPads[i]);
                        break;
                    }
                }
            }
        }
    }

    cPlayer* pOwner = g_pBall->m_pOwner;
    if (pOwner != NULL)
    {
        if (pOwner->m_eClassType == GOALIE)
        {
            if (g_pGame->IsGameplayOrOvertime() && !((Goalie*)pOwner)->mbNoUserControl)
            {
            }
            else
            {
                return;
            }
        }

        if (pOwner->m_pTeam != this)
        {
            return;
        }
        if (pOwner->m_pController != NULL)
        {
            return;
        }

        for (int j = 0; j < 5; j++)
        {
            if (m_pPlayers[j]->m_pController != NULL)
            {
                cAIPad* pPad = m_pPlayers[j]->m_pController;
                m_pPlayers[j]->SetAIPad(NULL);
                pOwner->SetAIPad(pPad);
                break;
            }
        }
    }
}

/**
 * Offset/Address/Size: 0xC98 | 0x80065044 | size: 0x468
 */
void cTeam::ResetCharacters()
{
    int i;
    int nAssignmentOrder[5];

    for (i = 0; i < 5; i++)
    {
        m_pPlayers[i]->SetAIPad(NULL);
    }

    UpdateControllers();

    for (i = 0; i < 5; i++)
    {
        nAssignmentOrder[i] = i;
    }

    if (GetNumAssignedControllers() > 0 && m_pPlayers[nAssignmentOrder[0]]->m_pController == NULL)
    {
        for (i = 0; i <= 4; i++)
        {
            if (m_pPlayers[nAssignmentOrder[i]]->m_pController != NULL)
            {
                break;
            }
        }
        nAssignmentOrder[0] = i;
        nAssignmentOrder[i] = 0;
    }

    const unsigned short* pFacingDirectionTable;
    const FormationSpec* pFormation;
    if (g_pGame->m_nLastTeamToScore == g_pTeams[m_nSide == 0 ? 1 : 0]->m_nSide)
    {
        pFacingDirectionTable = g_aAdvantagePlayerFacingDirections;
        pFormation = FormationManager::GetFormationSpec(FORMATION_OFF_DEF_KICKOFF_ADVANTAGE);
    }
    else
    {
        pFacingDirectionTable = g_aNeutralPlayerFacingDirections;
        pFormation = FormationManager::GetFormationSpec(FORMATION_OFF_DEF_KICKOFF_NEUTRAL);
    }

    unsigned char bFlipPositions = 0;
    if (g_pTeams[m_nSide == 0 ? 1 : 0]->m_pNet->m_v3NetLocation.x < 0.0f)
    {
        bFlipPositions = 1;
    }

    for (i = 0; i < 5; i++)
    {
        cFielder* pFielder = GetFielder(nAssignmentOrder[i]);
        nlVector3 v3NewPosition;
        const unsigned short aOriginalFacingDirection = pFacingDirectionTable[i];
        unsigned short aNewFacingDirection;

        if (i < 4)
        {
            nlVector2 v2Position;
            pFormation->m_Positions[i].GetLocationForTeam(v2Position, pFielder->m_pTeam->m_nSide);
            nlVec3Set(v3NewPosition, v2Position.x, v2Position.y, 0.0f);
        }
        else
        {
            nlVec3Set(v3NewPosition, bFlipPositions ? 18.0f : -18.0f, 0.0f, 0.0f);
        }

        if (bFlipPositions)
        {
            aNewFacingDirection = aOriginalFacingDirection
                                + ((s16)(0x4000 - aOriginalFacingDirection)) * 2;
        }
        else
        {
            aNewFacingDirection = aOriginalFacingDirection;
            v3NewPosition.y = -v3NewPosition.y;
        }

        pFielder->SetPosition(v3NewPosition);
        pFielder->SetFacingDirection(aNewFacingDirection);
        pFielder->m_aActualMovementDirection = aNewFacingDirection;
        pFielder->ResetDesiredDirections(aNewFacingDirection);
        pFielder->m_ePositionSeekState = PSS_ARRIVED;
        pFielder->ResetEffects();
        pFielder->EndBlur();
        pFielder->InitActionPostWhistle();
        pFielder->ClearSwapControllerTimer();
        pFielder->ResetUnPossessionTimer();

        if (pFielder->m_eClassType == FIELDER)
        {
            pFielder->ClearQueuedDesire();
            pFielder->CleanUpDesire((eFielderDesireState)0);
            pFielder->CleanUpAction();
            pFielder->ClearTimers();
            pFielder->ClearPowerupAnimState(true);
            pFielder->SetPowerup(POWER_UP_NONE, 1, NULL);

            if (pFielder->IsCaptain() == false)
            {
                cPlayer* pCaptain = m_pPlayers[0];
                ((FielderTweaks*)pFielder->m_pTweaks)->fAggression = ((FielderTweaks*)pCaptain->m_pTweaks)->fAggression;
                ((FielderTweaks*)pFielder->m_pTweaks)->fDekeing = ((FielderTweaks*)pCaptain->m_pTweaks)->fDekeing;
                ((FielderTweaks*)pFielder->m_pTweaks)->fPassing = ((FielderTweaks*)pCaptain->m_pTweaks)->fPassing;
                ((FielderTweaks*)pFielder->m_pTweaks)->fShotMinSpeed = ((FielderTweaks*)pCaptain->m_pTweaks)->fShotMinSpeed;
                ((FielderTweaks*)pFielder->m_pTweaks)->fShotMaxSpeed = ((FielderTweaks*)pCaptain->m_pTweaks)->fShotMaxSpeed;
                ((FielderTweaks*)pFielder->m_pTweaks)->fShotChipMinSpeed = ((FielderTweaks*)pCaptain->m_pTweaks)->fShotChipMinSpeed;
                ((FielderTweaks*)pFielder->m_pTweaks)->fShotChipMaxSpeed = ((FielderTweaks*)pCaptain->m_pTweaks)->fShotChipMaxSpeed;
                pFielder->m_pTweaks->fPassGroundSpeedMin = pCaptain->m_pTweaks->fPassGroundSpeedMin;
                pFielder->m_pTweaks->fPassGroundSpeedMax = pCaptain->m_pTweaks->fPassGroundSpeedMax;
                pFielder->m_pTweaks->fPassVolleySpeedMin = pCaptain->m_pTweaks->fPassVolleySpeedMin;
                pFielder->m_pTweaks->fPassVolleySpeedMax = pCaptain->m_pTweaks->fPassVolleySpeedMax;
            }
        }
    }

    StopGameplayEffectsAndSounds();
    mfPowerupTimer = 0.0f;
}

/**
 * Offset/Address/Size: 0xAF4 | 0x80064EA0 | size: 0x1A4
 */
void cTeam::StopGameplayEffectsAndSounds()
{
    FORCE_DONT_INLINE;

    Audio::ActivateFilterOnAllCurrentSFX(false);
    Audio::SetPitchBendOnAllDialogueSFX(0x2000);

    s32 side = m_nSide;
    s32 i_player = 0;
    do
    {
        g_pTeams[side]->m_pPlayers[i_player]->StopPlayingAllTrackedSFX();
        i_player++;
    } while (i_player < 5);

    Audio::gWorldSFX.Stop(Audio::REPLAYSFX_CAMERA_ZOOM_OUT, cGameSFX::SFX_STOP_FIRST);
    Audio::gStadGenSFX.Stop((Audio::eWorldSFX)0xBD, cGameSFX::SFX_STOP_FIRST);
    Audio::gStadGenSFX.Stop((Audio::eWorldSFX)0xCE, cGameSFX::SFX_STOP_ALL);
    Audio::gStadGenSFX.Stop((Audio::eWorldSFX)0xCC, cGameSFX::SFX_STOP_FIRST);
    Audio::gStadGenSFX.Stop((Audio::eWorldSFX)0xBA, cGameSFX::SFX_STOP_FIRST);
    Audio::gPowerupSFX.Stop((Audio::eWorldSFX)0x88, cGameSFX::SFX_STOP_FIRST);
    Audio::gPowerupSFX.Stop((Audio::eWorldSFX)0x78, cGameSFX::SFX_STOP_FIRST);
    Audio::gPowerupSFX.Stop((Audio::eWorldSFX)0x72, cGameSFX::SFX_STOP_FIRST);
    Audio::gPowerupSFX.Stop((Audio::eWorldSFX)0x6C, cGameSFX::SFX_STOP_FIRST);
    Audio::gPowerupSFX.Stop((Audio::eWorldSFX)0x66, cGameSFX::SFX_STOP_FIRST);
    Audio::gPowerupSFX.Stop((Audio::eWorldSFX)0x5E, cGameSFX::SFX_STOP_FIRST);
    Audio::gPowerupSFX.Stop((Audio::eWorldSFX)0x8E, cGameSFX::SFX_STOP_FIRST);

    BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->m_pCharacterSFX->StopPlayingAllTrackedSFX();

    s32 i = 0;
    do
    {
        EmissionManager::Destroy((unsigned long)i, nullptr);
        i++;
    } while (i < 10);
}

/**
 * Offset/Address/Size: 0xAD0 | 0x80064E7C | size: 0x24
 */
bool cTeam::CalculateFormationPosition(nlVector3& v3DestPosition, cFielder* pFielder, bool bInPosition, float fBallPosFormationWeight)
{
    return m_pFormationManager->CalculateFielderPosition(v3DestPosition, pFielder, bInPosition, fBallPosFormationWeight);
}

void cTeam::CalculateNewBallInterceptTimes()
{
    nlVector3* pBallPosition;
    cPlayer* pPlayer;
    int i;

    for (i = 0; i < 4; i++)
    {
        pPlayer = m_pPlayers[i];
        float speed = pPlayer->m_fActualSpeed;
        float runSpeed = pPlayer->m_pTweaks->fRunningSpeed;
        speed = (speed >= runSpeed) ? speed : runSpeed;

        nlVector3 v3PredictedLandingSpot;
        float landingTime = g_pBall->PredictLandingSpotAndTime(v3PredictedLandingSpot);
        float interceptTime;

        if (landingTime > 0.0f)
        {
            float dx = v3PredictedLandingSpot.x - pPlayer->m_v3Position.x;
            interceptTime = nlSqrt(nlGetLengthSquared2D(dx, v3PredictedLandingSpot.y - pPlayer->m_v3Position.y), true) / speed;
        }
        else
        {
            int nNumSolutions = 0;
            interceptTime = 100000000.0f;
            pBallPosition = &g_pBall->m_v3Position;
            nlVector3* pAIVelocity = g_pBall->GetAIVelocity();
            float pSolutions[2];
            CalcInterceptXY(pPlayer->m_v3Position, speed, 0.0f, *pBallPosition, *pAIVelocity, nNumSolutions, pSolutions);

            if (nNumSolutions != 0)
            {
                if (nNumSolutions == 2)
                {
                    float solution1 = pSolutions[1];
                    interceptTime = pSolutions[0];
                    interceptTime = (interceptTime <= solution1) ? interceptTime : solution1;
                }
                else
                {
                    interceptTime = pSolutions[0];
                }
            }
        }

        mfBallInterceptTimes[i] = interceptTime;
    }
}

void cTeam::UpdateBallInterceptTime(float fDeltaT)
{
    CalculateNewBallInterceptTimes();
    // PORT: element size; the console's elements were pointer-sized.
    qsort(m_pBallInterceptOrderedFielders, 4,
          sizeof(m_pBallInterceptOrderedFielders[0]), BestAbleToInterceptBall);
    mtBallInterceptTimer.SetSeconds(1.0f / 30.0f);
}
/**
 * Offset/Address/Size: 0xA68 | 0x80064E14 | size: 0x68
 */
int BestAbleToInterceptBall(const void* a, const void* b)
{
    cPlayer* p1 = *(cPlayer**)a;
    cPlayer* p2 = *(cPlayer**)b;

    float fScore1 = AbleToInterceptBall(p1);
    float fScore2 = AbleToInterceptBall(p2);

    if (fScore1 == fScore2)
    {
        return 0;
    }

    if (fScore1 > fScore2)
    {
        return -1;
    }

    return 1;
}

void cTeam::AbortPlays()
{
    for (int i = 0; i < 4; i++)
    {
        m_pAIOrderedFielders[i]->AbortPlay();
    }
}

bool cTeam::AssignSituation()
{
    cPlayer* pBallOwner = g_pBall->m_pOwner;
    eSituation eLastSituation = mpCurrentSituation;

    if (pBallOwner == NULL)
    {
        pBallOwner = g_pBall->m_pPassTarget;
        if ((pBallOwner != NULL) && (pBallOwner->m_eClassType == FIELDER))
        {
            if (!ReceivingPass((cFielder*)pBallOwner))
            {
                nlPrintf("cTeam::AssignSituation - caught bad pass case, with no proper receiver.\n");
                g_pBall->ClearPassTarget();
                pBallOwner = NULL;
            }
        }
    }

    if (pBallOwner != NULL)
    {
        if (pBallOwner->m_pTeam == this)
        {
            if (mpCurrentSituation != SITUATION_OFFENSE)
            {
                mpCurrentSituation = SITUATION_OFFENSE;
                meCurrentTeamStyle = TEAM_STYLE_AGGRESSIVE;
                mtTeamStyleTimer.SetSeconds(1.0f);
                AbortPlays();
            }
        }
        else if (mpCurrentSituation != SITUATION_DEFENSE)
        {
            mpCurrentSituation = SITUATION_DEFENSE;
            meCurrentTeamStyle = TEAM_STYLE_AGGRESSIVE;
            mtTeamStyleTimer.SetSeconds(1.0f);
            AbortPlays();
        }
    }
    else if (mpCurrentSituation != SITUATION_LOOSE)
    {
        mpCurrentSituation = SITUATION_LOOSE;
        meCurrentTeamStyle = TEAM_STYLE_AGGRESSIVE;
        mtTeamStyleTimer.SetSeconds(1.0f);
        AbortPlays();
    }

    return eLastSituation != mpCurrentSituation;
}

/**
 * Offset/Address/Size: 0x6F0 | 0x80064A9C | size: 0x378
 */
void cTeam::UpdateTeamAI(float fDeltaT)
{
    if (mtTeamStyleTimer.m_uPackedTime == 0)
    {
        meCurrentTeamStyle = TEAM_STYLE_AGGRESSIVE;
        mtTeamStyleTimer.SetSeconds(1.0f);
        AbortPlays();
    }

    m_pFormationManager->Update(fDeltaT);

    bool bSituationChanged = AssignSituation();

    if ((mtRoleTimer.m_uPackedTime == 0) || bSituationChanged)
    {
        switch (mpCurrentSituation)
        {
        case SITUATION_OFFENSE:
            m_pAIOrderedFielders[0] = (cFielder*)m_pPlayers[0];
            m_pAIOrderedFielders[1] = (cFielder*)m_pPlayers[1];
            m_pAIOrderedFielders[2] = (cFielder*)m_pPlayers[2];
            m_pAIOrderedFielders[3] = (cFielder*)m_pPlayers[3];
            qsort(m_pAIOrderedFielders, 4, sizeof(m_pAIOrderedFielders[0]),
                  MostOffensiveThreat);
            m_pAIOrderedFielders[0]->m_eRole = ROLE_STRIKER;
            m_pAIOrderedFielders[1]->m_eRole = ROLE_WINGER;
            m_pAIOrderedFielders[2]->m_eRole = ROLE_MIDFIELD;
            m_pAIOrderedFielders[3]->m_eRole = ROLE_DEFENCE;
            break;

        case SITUATION_DEFENSE:
            m_pAIOrderedFielders[0] = (cFielder*)m_pPlayers[0];
            m_pAIOrderedFielders[1] = (cFielder*)m_pPlayers[1];
            m_pAIOrderedFielders[2] = (cFielder*)m_pPlayers[2];
            m_pAIOrderedFielders[3] = (cFielder*)m_pPlayers[3];
            qsort(m_pAIOrderedFielders, 4, sizeof(m_pAIOrderedFielders[0]),
                  MostDefensivePlayer);
            m_pAIOrderedFielders[0]->m_eRole = ROLE_STRIKER;
            m_pAIOrderedFielders[1]->m_eRole = ROLE_WINGER;
            m_pAIOrderedFielders[2]->m_eRole = ROLE_MIDFIELD;
            m_pAIOrderedFielders[3]->m_eRole = ROLE_DEFENCE;
            break;

        case SITUATION_LOOSE:
            m_pAIOrderedFielders[0] = m_pBallInterceptOrderedFielders[0];
            m_pAIOrderedFielders[1] = m_pBallInterceptOrderedFielders[1];
            m_pAIOrderedFielders[2] = m_pBallInterceptOrderedFielders[2];
            m_pAIOrderedFielders[3] = m_pBallInterceptOrderedFielders[3];
            m_pAIOrderedFielders[0]->m_eRole = ROLE_STRIKER;
            m_pAIOrderedFielders[1]->m_eRole = ROLE_WINGER;
            m_pAIOrderedFielders[2]->m_eRole = ROLE_MIDFIELD;
            m_pAIOrderedFielders[3]->m_eRole = ROLE_DEFENCE;
            break;
        }

        mtRoleTimer.SetSeconds(0.33f);
    }

    AssignMarks(bSituationChanged);
}

/**
 * Offset/Address/Size: 0x598 | 0x80064944 | size: 0x158
 */
int MostOffensiveThreat(const void* a, const void* b)
{
    cPlayer* p1 = *(cPlayer**)a;
    cPlayer* p2 = *(cPlayer**)b;

    const nlVector3& offNetLocA = p1->GetAIOffNetLocation(NULL);

    float dxA = offNetLocA.x - p1->m_v3Position.x;
    float dyA = offNetLocA.y - p1->m_v3Position.y;
    float fP1TotalDistance = nlSqrt(dxA * dxA + dyA * dyA, true);

    if (p1->IsCaptain())
    {
        fP1TotalDistance *= 0.96f;
    }

    float fZero = 0.0f;
    float strategicScoreA = StrategicBallOwner((cFielder*)p1);
    if (strategicScoreA != fZero)
    {
        fP1TotalDistance *= 0.92f;
    }

    const nlVector3& offNetLocB = p2->GetAIOffNetLocation(NULL);

    float dxB = offNetLocB.x - p2->m_v3Position.x;
    float dyB = offNetLocB.y - p2->m_v3Position.y;
    float fP2TotalDistance = nlSqrt(dxB * dxB + dyB * dyB, true);

    if (p2->IsCaptain())
    {
        fP2TotalDistance *= 0.96f;
    }

    float strategicScoreB = StrategicBallOwner((cFielder*)p2);
    if (strategicScoreB != fZero)
    {
        fP2TotalDistance *= 0.92f;
    }

    if (fP1TotalDistance == fP2TotalDistance)
    {
        return 0;
    }

    if (fP1TotalDistance < fP2TotalDistance)
    {
        return -1;
    }

    return 1;
}

/**
 * Offset/Address/Size: 0x4D0 | 0x8006487C | size: 0xC8
 */
int MostDefensivePlayer(const void* a, const void* b)
{
    cPlayer* p1 = *(cPlayer**)a;
    cPlayer* p2 = *(cPlayer**)b;

    const nlVector3& netLocA = p1->GetAIDefNetLocation(NULL);
    float dxA = netLocA.x - p1->m_v3Position.x;
    float dyA = netLocA.y - p1->m_v3Position.y;
    float distSqA = dxA * dxA + dyA * dyA;

    const nlVector3& netLocB = p2->GetAIDefNetLocation(NULL);
    float dxB = netLocB.x - p2->m_v3Position.x;
    float dyB = netLocB.y - p2->m_v3Position.y;
    float distSqB = dxB * dxB + dyB * dyB;

    if (distSqA == distSqB)
    {
        return 0;
    }

    if (distSqA > distSqB)
    {
        return -1;
    }

    return 1;
}

/**
 * Offset/Address/Size: 0x120 | 0x800644CC | size: 0x3B0
 */
void cTeam::AssignMarks(bool bForceReMark)
{
    cTeam* pOpponentTeam;
    cFielder* pMyFielder;
    cFielder* pOppFielder;
    float fDistanceScore;

    if (mtMarkTimer.m_uPackedTime != 0 && !bForceReMark)
    {
        return;
    }

    for (int i = 0; i < 4; i++)
    {
        ((cFielder*)m_pPlayers[i])->SetMark(NULL);
    }

    if (mpCurrentSituation != SITUATION_OFFENSE)
    {
        pOpponentTeam = GetOtherTeam();

        float fFielderMarkScores[4][4];

        for (int i_fielder = 0; i_fielder < 4; i_fielder++)
        {
            for (int i_otherf = 0; i_otherf < 4; i_otherf++)
            {
                pMyFielder = GetFielder(i_fielder);
                pOppFielder = pOpponentTeam->GetFielder(i_otherf);

                float fDownfieldMax = FGREATER(DownfieldFrom(pMyFielder, pOppFielder), 0.5f);
                fDownfieldMax = fDownfieldMax;
                float fInBetween = InBetweenMyNetAnd(pMyFielder, pOppFielder);
                if (fInBetween >= fDownfieldMax)
                {
                    fDownfieldMax = fInBetween;
                }

                float dx = pMyFielder->m_v3Position.x - pOppFielder->m_v3Position.x;
                float dy = pMyFielder->m_v3Position.y - pOppFielder->m_v3Position.y;
                float fDist = nlSqrt(dx * dx + dy * dy, true);
                fDistanceScore = NormalizeVal(fDist, g_vMarkDistanceConfidence);

                float fScore = 0.0f;
                bool bUseDefaultCalculation = true;

                float fDefZone = InDefensiveZone(pOppFielder);
                if (1.0f - fDefZone >= 0.3f)
                {
                    float fBreakaway = OnBreakaway(pOppFielder);
                    if (fBreakaway > 0.5f)
                    {
                        float fDF = DownfieldFrom(pMyFielder, pOppFielder);
                        if (fDistanceScore >= fDF)
                        {
                            fScore = fDistanceScore;
                        }
                        else
                        {
                            fScore = fDF;
                        }
                        bUseDefaultCalculation = false;
                    }
                    else
                    {
                        float fReceiving = ReceivingPass(pOppFielder);
                        float fBallOwn = BallOwner(pOppFielder);
                        float fMax;
                        if (fBallOwn >= fReceiving)
                        {
                            fMax = fBallOwn;
                        }
                        else
                        {
                            fMax = fReceiving;
                        }

                        if (fMax)
                        {
                            bUseDefaultCalculation = false;
                            float fHalf = 0.5f;
                            fScore = fDistanceScore * fHalf + fDownfieldMax * fHalf;
                        }
                        else
                        {
                            float fChasing = ChasingBall(pOppFielder);
                            if (fChasing)
                            {
                                float fIntercept = AbleToInterceptBall(pMyFielder);
                                bUseDefaultCalculation = false;
                                fScore = fIntercept;
                            }
                        }
                    }

                    float fBallOwn2 = BallOwner(pOppFielder);
                    if (fBallOwn2)
                    {
                        fScore *= 2.0f;
                    }
                }

                if (bUseDefaultCalculation)
                {
                    float fDistanceWeight = 0.7f;
                    float fDownfieldWeight = 0.3f;
                    fScore = fDistanceScore * fDistanceWeight + fDownfieldMax * fDownfieldWeight;
                }

                if (Incapacitated(pMyFielder))
                {
                    fScore *= 0.5f;
                }

                fFielderMarkScores[i_fielder][i_otherf] = fScore;
            }
        }

        unsigned int pMarkIDs[4];
        SortToMinOrMaxTotalSum(pMarkIDs, fFielderMarkScores, false);

        for (int i_fielder = 0; i_fielder < 4; i_fielder++)
        {
            ((cFielder*)m_pPlayers[i_fielder])->SetMark((cFielder*)pOpponentTeam->m_pPlayers[pMarkIDs[i_fielder]]);
        }

        cFielder* pBallOwner = g_pBall->GetOwnerFielder();
        if (pBallOwner != NULL)
        {
            pBallOwner = g_pBall->GetOwnerFielder();
            if (pBallOwner->m_pTeam != this)
            {
                pOppFielder = g_pBall->GetOwnerFielder()->GetMarker();
                if (Incapacitated(pOppFielder))
                {
                    pBallOwner = g_pBall->GetOwnerFielder();
                    pOppFielder = pBallOwner->GetMarker();
                    for (int k = 0; k < 4; k++)
                    {
                        pMyFielder = GetBallInterceptFielder(k);
                        if (pMyFielder != pOppFielder)
                        {
                            cFielder* pOldMark = pMyFielder->GetMark();
                            pMyFielder->SetMark(g_pBall->GetOwnerFielder());
                            pOppFielder->SetMark(pOldMark);
                            break;
                        }
                    }
                }
            }
        }
    }

    mtMarkTimer.SetSeconds(0.5f);
}

/**
 * Offset/Address/Size: 0x118 | 0x800644C4 | size: 0x8
 */
cFielder* cTeam::GetCaptain()
{
    return (cFielder*)m_pPlayers[0];
}

/**
 * Offset/Address/Size: 0x110 | 0x800644BC | size: 0x8
 */
cFielder* cTeam::GetStriker() const
{
    return m_pAIOrderedFielders[0];
}

/**
 * Offset/Address/Size: 0x108 | 0x800644B4 | size: 0x8
 */
cFielder* cTeam::GetMidfield() const
{
    return m_pAIOrderedFielders[2];
}

/**
 * Offset/Address/Size: 0x100 | 0x800644AC | size: 0x8
 */
cFielder* cTeam::GetDefence() const
{
    return m_pAIOrderedFielders[3];
}

/**
 * Offset/Address/Size: 0x80 | 0x8006442C | size: 0x80
 */
cFielder* cTeam::GetFrontMostFielder()
{
    cFielder* pFielder;
    cFielder* pFrontMostFielder = NULL;

    for (int i_fielder = 0; i_fielder < 4; i_fielder++)
    {
        pFielder = (cFielder*)m_pPlayers[i_fielder];
        if ((pFrontMostFielder == NULL) || (pFielder->m_v3AIPosition.x > pFrontMostFielder->m_v3AIPosition.x))
        {
            pFrontMostFielder = pFielder;
        }
    }

    return pFrontMostFielder;
}

/**
 * Offset/Address/Size: 0x0 | 0x800643AC | size: 0x80
 */
cFielder* cTeam::GetRearMostFielder()
{
    cFielder* pFielder;
    cFielder* pRearMostFielder = NULL;

    for (int i_fielder = 0; i_fielder < 4; i_fielder++)
    {
        pFielder = (cFielder*)m_pPlayers[i_fielder];
        if ((pRearMostFielder == NULL) || (pFielder->m_v3AIPosition.x < pRearMostFielder->m_v3AIPosition.x))
        {
            pRearMostFielder = pFielder;
        }
    }

    return pRearMostFielder;
}
