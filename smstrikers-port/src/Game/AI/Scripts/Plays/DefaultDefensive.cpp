#include "Game/AI/Scripts/Plays/DefaultDefensive.h"
#include "Game/AI/AiUtil.h"
#include "Game/AI/Fuzzy.h"
#include "Game/AI/ScriptAction.h"
#include "Game/AI/Scripts/ScriptCaching.h"
#include "Game/AI/Scripts/ScriptQuestions.h"
#include "Game/GameTweaks.h"

extern cTeam* g_pScriptCurrentTeam;
extern cTeam* g_pScriptOtherTeam;
extern cFielder* g_pScriptCurrentFielder;
extern cFielder* g_pScriptBallOwner;
extern cTeam* g_pCurrentlyUpdatingTeam;
extern FuzzyVariant fvNotSet;
extern cBall* g_pScriptBall;
extern cFielder* g_pScriptCurrentMark;

float Defensive(cTeam*);

static inline float DefensiveIdentity(float value)
{
    return value;
}

static inline float DefensiveOneMinus(float value)
{
    return 1.0f - value;
}

static inline float DefensiveMin(float a, float b)
{
    return (a <= b) ? a : b;
}

static inline float CalculateDefensiveThreat()
{
    float fUpfield = UpfieldFrom(g_pScriptCurrentFielder, g_pScriptBallOwner);
    float fNearTo = NearTo(g_pScriptCurrentFielder, g_pScriptBallOwner);
    float fThreat0 = (1.0f - fNearTo >= fUpfield) ? (1.0f - fNearTo) : fUpfield;
    float fNotMush = 1.0f - OnMushrooms(g_pScriptCurrentFielder);
    float fMarking = Marking(g_pScriptCurrentFielder, g_pScriptBallOwner);
    float fThreat1 = (fNotMush <= fThreat0) ? fNotMush : fThreat0;
    return (fMarking <= fThreat1) ? fMarking : fThreat1;
}

/**
 * Offset/Address/Size: 0x5590 | 0x8008AC48 | size: 0x43C
 */
FuzzyVariant Fuzzy::AbortDefencePlay(cDecisionEntity* pEntity)
{
    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    float fTrueConfidence = Defensive(g_pScriptCurrentTeam);
    float fFalseConfidence = 1.0f - fTrueConfidence;

    float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fBranchRatio = fMin / fMax;

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);
        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            fConfidence = fConfidence * fBranchRatio;
        if (fConfidence > 0.0f)
        {
            fBestConfidence = fConfidence;
            bestValue = FuzzyVariant(false);
        }
    }

    if (fFalseConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);
        fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
        if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
            fConfidence = fConfidence * fBranchRatio;
        if (fConfidence > fBestConfidence)
        {
            fBestConfidence = fConfidence;
            bestValue = FuzzyVariant(true);
        }
    }

    bestValue.Confidence = fBestConfidence;
    return bestValue;
}

/**
 * Offset/Address/Size: 0x48B4 | 0x80089F6C | size: 0xCDC
 */
FuzzyVariant Fuzzy::DefaultDefencePlay(cDecisionEntity* pDecision)
{
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    // Branch 1: Not user controlled -> Use powerup
    float fTrueConf1 = 1.0f - UserControlledT(g_pScriptCurrentTeam);
    float fFalseConf1 = 1.0f - fTrueConf1;
    float fMin1 = (fTrueConf1 <= fFalseConf1) ? fTrueConf1 : fFalseConf1;
    float fMax1 = (fTrueConf1 >= fFalseConf1) ? fTrueConf1 : fFalseConf1;
    float fBranchRatio1 = fMin1 / fMax1;

    if (fTrueConf1 > 0.0f)
    {
        SaveConfidence PushDOM1(&fConfidence);
        fConfidence = (fConfidence <= fTrueConf1) ? fConfidence : fTrueConf1;
        if (fConfidence < fTrueConf1 && fTrueConf1 < 0.5f)
            fConfidence = fConfidence * fBranchRatio1;
        float fUsePowerupResult = UsePowerupDefensive(fConfidence, pDecision).mData.f;
        fBestConfidence = (fUsePowerupResult >= 0.0f) ? fUsePowerupResult : 0.0f;
    }

    // Branch 2: Other team's goalie is ball owner
    float fGoalieBallOwner = BallOwner(g_pScriptOtherTeam->GetGoalie());
    float fNotGoalieBallOwner = 1.0f - fGoalieBallOwner;
    float fMin2 = (fGoalieBallOwner <= fNotGoalieBallOwner) ? fGoalieBallOwner : fNotGoalieBallOwner;
    float fMax2 = (fGoalieBallOwner >= fNotGoalieBallOwner) ? fGoalieBallOwner : fNotGoalieBallOwner;
    float fBranchRatio2 = fMin2 / fMax2;

    if (fGoalieBallOwner > 0.0f)
    {
        SaveConfidence PushDOM2(&fConfidence);
        fConfidence = (fConfidence <= fGoalieBallOwner) ? fConfidence : fGoalieBallOwner;
        if (fConfidence < fGoalieBallOwner && fGoalieBallOwner < 0.5f)
            fConfidence = fConfidence * fBranchRatio2;
        if (fBestConfidence >= fConfidence)
            fBestConfidence = fBestConfidence;
        else
            fBestConfidence = fConfidence;
        pDecision->QueueActionSetDesire(11, fConfidence, -1.0f, fvNotSet, fvNotSet);
    }

    if (fNotGoalieBallOwner > 0.0f)
    {
        SaveConfidence PushDOM3(&fConfidence);
        fConfidence = (fConfidence <= fNotGoalieBallOwner) ? fConfidence : fNotGoalieBallOwner;
        if (fConfidence < fNotGoalieBallOwner && fNotGoalieBallOwner < 0.5f)
            fConfidence = fConfidence * fBranchRatio2;

        // DefendPassInPlay
        const FuzzyVariant& defendPass = DefendPassInPlay(fConfidence, pDecision);
        float fPassConf = (defendPass.mData.f >= fBestConfidence) ? defendPass.mData.f : fBestConfidence;
        fBestConfidence = fPassConf;

        // TryAttacking sub-branch
        float fTryConf = DefensiveOneMinus(fPassConf);
        float fNotTryConf = 1.0f - fTryConf;
        float fMin3 = (fTryConf <= fNotTryConf) ? fTryConf : fNotTryConf;
        float fMax3 = (fTryConf >= fNotTryConf) ? fTryConf : fNotTryConf;
        float fBranchRatio3 = fMin3 / fMax3;

        if (fTryConf > 0.0f)
        {
            SaveConfidence PushDOM4(&fConfidence);
            fConfidence = (fConfidence <= fTryConf) ? fConfidence : fTryConf;
            if (fConfidence < fTryConf && fTryConf < 0.5f)
                fConfidence = fConfidence * fBranchRatio3;

            const FuzzyVariant& tryAttack = TryAttacking(fConfidence, pDecision);
            fBestConfidence = (tryAttack.mData.f >= fPassConf) ? tryAttack.mData.f : fPassConf;

            // Positional sub-branch
            float fCapConf = (0.01f >= (1.0f - fBestConfidence)) ? 0.01f : (1.0f - fBestConfidence);
            float fNotCapConf = 1.0f - fCapConf;
            float fMin4 = (fCapConf <= fNotCapConf) ? fCapConf : fNotCapConf;
            float fMax4 = (fCapConf >= fNotCapConf) ? fCapConf : fNotCapConf;
            float fBranchRatio4 = fMin4 / fMax4;

            if (fCapConf > 0.0f)
            {
                SaveConfidence PushDOM5(&fConfidence);
                fConfidence = (fConfidence <= fCapConf) ? fConfidence : fCapConf;
                if (fConfidence < fCapConf && fCapConf < 0.5f)
                    fConfidence = fConfidence * fBranchRatio4;

                float fFar = DefensiveIdentity(FarToTheirNet(g_pScriptCurrentFielder));
                fNotGoalieBallOwner = DefensiveIdentity(Midfield(g_pScriptCurrentFielder));
                fGoalieBallOwner = Defence(g_pScriptCurrentFielder);
                float fMarkBO = BallOwner(g_pScriptCurrentMark);

                fNotGoalieBallOwner = (fNotGoalieBallOwner >= fFar) ? fNotGoalieBallOwner : fFar;
                fGoalieBallOwner = (fGoalieBallOwner >= fNotGoalieBallOwner) ? fGoalieBallOwner : fNotGoalieBallOwner;
                fMarkBO = (fMarkBO >= fGoalieBallOwner) ? fMarkBO : fGoalieBallOwner;

                float fNotMarkBO = 1.0f - fMarkBO;
                float fMin5 = (fMarkBO <= fNotMarkBO) ? fMarkBO : fNotMarkBO;
                float fMax5 = (fMarkBO >= fNotMarkBO) ? fMarkBO : fNotMarkBO;
                float fBranchRatio5 = fMin5 / fMax5;

                if (fMarkBO > 0.0f)
                {
                    SaveConfidence PushDOM6(&fConfidence);
                    fConfidence = (fConfidence <= fMarkBO) ? fConfidence : fMarkBO;
                    if (fConfidence < fMarkBO && fMarkBO < 0.5f)
                        fConfidence = fConfidence * fBranchRatio5;
                    if (fBestConfidence >= fConfidence)
                        fBestConfidence = fBestConfidence;
                    else
                        fBestConfidence = fConfidence;
                    pDecision->QueueActionSetDesire(7, fConfidence, -1.0f, fvNotSet, fvNotSet);
                }

                if (fNotMarkBO > 0.0f)
                {
                    SaveConfidence PushDOM7(&fConfidence);
                    fConfidence = (fConfidence <= fNotMarkBO) ? fConfidence : fNotMarkBO;
                    if (fConfidence < fNotMarkBO && fNotMarkBO < 0.5f)
                        fConfidence = fConfidence * fBranchRatio5;
                    if (fBestConfidence >= fConfidence)
                        fBestConfidence = fBestConfidence;
                    else
                        fBestConfidence = fConfidence;
                    pDecision->QueueActionSetDesire(11, fConfidence, -1.0f, fvNotSet, fvNotSet);
                }
            }
        }
    }

    // Fallback: if no action was queued
    float fFallback = 0.0f;
    if (fBestConfidence == 0.0f)
        fFallback = 1.0f;
    float fNotFallback = 1.0f - fFallback;
    float fMin6 = (fFallback <= fNotFallback) ? fFallback : fNotFallback;
    float fMax6 = (fFallback >= fNotFallback) ? fFallback : fNotFallback;
    float fBranchRatio6 = fMin6 / fMax6;

    if (fFallback > 0.0f)
    {
        SaveConfidence PushDOM8(&fConfidence);
        fConfidence = (fConfidence <= fFallback) ? fConfidence : fFallback;
        if (fConfidence < fFallback && fFallback < 0.5f)
            fConfidence = fConfidence * fBranchRatio6;
        if (fBestConfidence >= fConfidence)
            fBestConfidence = fBestConfidence;
        else
            fBestConfidence = fConfidence;
        pDecision->QueueActionSetDesire(7, fConfidence, -1.0f, fvNotSet, fvNotSet);
    }

    return FuzzyVariant(fBestConfidence);
}

/**
 * Offset/Address/Size: 0x3A58 | 0x80089110 | size: 0xE5C
 */
FuzzyVariant Fuzzy::DefendPassInPlay(float fConfidence, cDecisionEntity* pEntity)
{
    float fBestConfidence = 0.0f;

    float fTrueConfidence = IsPassInPlayDelayed();
    float fFalseConfidence = 1.0f - fTrueConfidence;

    float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fBranchRatio = fMin / fMax;

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);
        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            fConfidence = fConfidence * fBranchRatio;

        cPlayer* pTarget = g_pScriptBall->m_pPassTarget;

        fTrueConfidence = 1.0f - ReceivingVolleyPassDelayed(pTarget);
        fFalseConfidence = 1.0f - High(g_pScriptBall);
        float fInPassingLane = InPassingLane(g_pScriptCurrentFielder);

        fFalseConfidence = (fFalseConfidence <= fTrueConfidence) ? fFalseConfidence : fTrueConfidence;
        fTrueConfidence = (fInPassingLane <= fFalseConfidence) ? fInPassingLane : fFalseConfidence;

        float fFalseConf2 = 1.0f - fTrueConfidence;

        float fMin2 = (fTrueConfidence <= fFalseConf2) ? fTrueConfidence : fFalseConf2;
        float fMax2 = (fTrueConfidence >= fFalseConf2) ? fTrueConfidence : fFalseConf2;
        float fRatio2 = fMin2 / fMax2;

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM2(&fConfidence);
            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                fConfidence = fConfidence * fRatio2;

            float fCurrentConfidence = fConfidence;
            if (0.0f >= fCurrentConfidence)
                fBestConfidence = 0.0f;
            else
                fBestConfidence = fCurrentConfidence;

            pEntity->QueueActionSetDesire(6, fConfidence, -1.0f, fvNotSet, fvNotSet);

            SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            pEntity->m_pLastQueuedAction->m_fSelectionChance = pTweaks->Def_BlockPassChance;
        }

        if (fFalseConf2 > 0.0f)
        {
            SaveConfidence PushDOM3(&fConfidence);
            fConfidence = (fConfidence <= fFalseConf2) ? fConfidence : fFalseConf2;
            if (fConfidence < fFalseConf2 && fFalseConf2 < 0.5f)
                fConfidence = fConfidence * fRatio2;

            fTrueConfidence = OnScreen(pTarget);
            float fNotOnScreen = 1.0f - fTrueConfidence;

            float fMin3 = (fTrueConfidence <= fNotOnScreen) ? fTrueConfidence : fNotOnScreen;
            float fMax3 = (fTrueConfidence >= fNotOnScreen) ? fTrueConfidence : fNotOnScreen;
            float fRatio3 = fMin3 / fMax3;

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM4(&fConfidence);
                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    fConfidence = fConfidence * fRatio3;

                float fClose = CloseTo(g_pScriptCurrentFielder, pTarget);
                float fAtIdeal = AtIdealDistanceForTackling(g_pScriptCurrentFielder, pTarget);
                fClose = (fAtIdeal >= fClose) ? fAtIdeal : fClose;

                fTrueConfidence = 1.0f - SeparatingFrom(g_pScriptCurrentFielder, pTarget);
                fTrueConfidence = (fTrueConfidence <= fClose) ? fTrueConfidence : fClose;

                float fFalseConf4 = 1.0f - fTrueConfidence;

                float fMin4 = (fTrueConfidence <= fFalseConf4) ? fTrueConfidence : fFalseConf4;
                float fMax4 = (fTrueConfidence >= fFalseConf4) ? fTrueConfidence : fFalseConf4;
                float fRatio4 = fMin4 / fMax4;

                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM5(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fRatio4;

                    fTrueConfidence = ReceivingVolleyPassDelayed(pTarget);
                    float fNotRecVolley = 1.0f - fTrueConfidence;

                    float fMin5 = (fTrueConfidence <= fNotRecVolley) ? fTrueConfidence : fNotRecVolley;
                    float fMax5 = (fTrueConfidence >= fNotRecVolley) ? fTrueConfidence : fNotRecVolley;
                    float fRatio5 = fMin5 / fMax5;

                    if (fTrueConfidence > 0.0f)
                    {
                        SaveConfidence PushDOM6(&fConfidence);
                        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                            fConfidence = fConfidence * fRatio5;

                        fTrueConfidence = 1.0f - RepeatingLastDesire(g_pScriptCurrentFielder, edHeavyAttack);
                        float fRepeat = 1.0f - fTrueConfidence;

                        float fMin6 = (fTrueConfidence <= fRepeat) ? fTrueConfidence : fRepeat;
                        float fMax6 = (fTrueConfidence >= fRepeat) ? fTrueConfidence : fRepeat;
                        float fRatio6 = fMin6 / fMax6;

                        if (fTrueConfidence > 0.0f)
                        {
                            SaveConfidence PushDOM7(&fConfidence);
                            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                                fConfidence = fConfidence * fRatio6;

                            if (fBestConfidence >= fConfidence)
                                fBestConfidence = fBestConfidence;
                            else
                                fBestConfidence = fConfidence;

                            pEntity->QueueActionSetDesire(5, fConfidence, 0.0f, FuzzyVariant(pTarget), fvNotSet);

                            SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                            pEntity->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(pTweaks->Def_VolleyPassDefendChance, Aggressive(g_pScriptCurrentFielder));
                        }
                    }

                    if (fNotRecVolley > 0.0f)
                    {
                        SaveConfidence PushDOM8(&fConfidence);
                        fConfidence = (fConfidence <= fNotRecVolley) ? fConfidence : fNotRecVolley;
                        if (fConfidence < fNotRecVolley && fNotRecVolley < 0.5f)
                            fConfidence = fConfidence * fRatio5;

                        fTrueConfidence = 1.0f - RepeatingLastDesire(g_pScriptCurrentFielder, edHeavyAttack);
                        float fRepeat2 = 1.0f - fTrueConfidence;

                        float fMin7 = (fTrueConfidence <= fRepeat2) ? fTrueConfidence : fRepeat2;
                        float fMax7 = (fTrueConfidence >= fRepeat2) ? fTrueConfidence : fRepeat2;
                        float fRatio7 = fMin7 / fMax7;

                        if (fTrueConfidence > 0.0f)
                        {
                            SaveConfidence PushDOM9(&fConfidence);
                            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                                fConfidence = fConfidence * fRatio7;

                            if (fBestConfidence >= fConfidence)
                                fBestConfidence = fBestConfidence;
                            else
                                fBestConfidence = fConfidence;

                            pEntity->QueueActionSetDesire(5, fConfidence, 0.0f, FuzzyVariant(pTarget), fvNotSet);

                            SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                            pEntity->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(pTweaks->Def_HeavyAttackChance, Aggressive(g_pScriptCurrentFielder));
                        }
                    }
                }

                if (fFalseConf4 > 0.0f)
                {
                    SaveConfidence PushDOM10(&fConfidence);
                    fConfidence = (fConfidence <= fFalseConf4) ? fConfidence : fFalseConf4;
                    if (fConfidence < fFalseConf4 && fFalseConf4 < 0.5f)
                        fConfidence = fConfidence * fRatio4;

                    float fIsCurrentMark;
                    if (pTarget == g_pScriptCurrentMark)
                        fIsCurrentMark = 1.0f;
                    else
                        fIsCurrentMark = 0.0f;
                    float fNotCurrentMark = 1.0f - fIsCurrentMark;

                    float fMin8 = (fIsCurrentMark <= fNotCurrentMark) ? fIsCurrentMark : fNotCurrentMark;
                    float fMax8 = (fIsCurrentMark >= fNotCurrentMark) ? fIsCurrentMark : fNotCurrentMark;
                    float fRatio8 = fMin8 / fMax8;

                    if (fIsCurrentMark > 0.0f)
                    {
                        SaveConfidence PushDOM11(&fConfidence);
                        fConfidence = (fConfidence <= fIsCurrentMark) ? fConfidence : fIsCurrentMark;
                        if (fConfidence < fIsCurrentMark && fIsCurrentMark < 0.5f)
                            fConfidence = fConfidence * fRatio8;

                        if (fBestConfidence >= fConfidence)
                            fBestConfidence = fBestConfidence;
                        else
                            fBestConfidence = fConfidence;

                        pEntity->QueueActionSetDesire(6, fConfidence, -1.0f, fvNotSet, fvNotSet);

                        SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                        pEntity->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(pTweaks->Def_BlockPassChance, Aggressive(g_pScriptCurrentFielder));
                    }
                }
            }
        }
    }

    return FuzzyVariant(fBestConfidence);
}

/**
 * Offset/Address/Size: 0x2140 | 0x800877F8 | size: 0x1918
 */
FuzzyVariant Fuzzy::TryAttacking(float fConfidence, cDecisionEntity* pEntity)
{
    float fBestConfidence = 0.0f;

    float fTrueConf1 = OnTheirTeam(g_pScriptBallOwner);
    float fFalseConf1 = 1.0f - fTrueConf1;
    float fMin1 = (fTrueConf1 <= fFalseConf1) ? fTrueConf1 : fFalseConf1;
    float fMax1 = (fTrueConf1 >= fFalseConf1) ? fTrueConf1 : fFalseConf1;
    float fRatio1 = fMin1 / fMax1;

    if (fTrueConf1 > 0.0f)
    {
        SaveConfidence PushDOM1(&fConfidence);
        fConfidence = (fConfidence <= fTrueConf1) ? fConfidence : fTrueConf1;
        if (fConfidence < fTrueConf1 && fTrueConf1 < 0.5f)
            fConfidence = fConfidence * fRatio1;

        float fTrueConf2 = Marking(g_pScriptCurrentFielder, g_pScriptBallOwner);
        float fFalseConf2 = 1.0f - fTrueConf2;
        float fMin2 = (fTrueConf2 <= fFalseConf2) ? fTrueConf2 : fFalseConf2;
        float fMax2 = (fTrueConf2 >= fFalseConf2) ? fTrueConf2 : fFalseConf2;
        float fRatio2 = fMin2 / fMax2;

        if (fTrueConf2 > 0.0f)
        {
            SaveConfidence PushDOM2(&fConfidence);
            fConfidence = (fConfidence <= fTrueConf2) ? fConfidence : fTrueConf2;
            if (fConfidence < fTrueConf2 && fTrueConf2 < 0.5f)
                fConfidence = fConfidence * fRatio2;

            float fDifficulty = NormalizeVal(Difficult(g_pScriptCurrentTeam), 1.0f, 0.0f);
            float fTrueConf3 = FGREATER(Stalling(g_pScriptOtherTeam), fDifficulty);
            float fFalseConf3 = 1.0f - fTrueConf3;
            float fMin3 = (fTrueConf3 <= fFalseConf3) ? fTrueConf3 : fFalseConf3;
            float fMax3 = (fTrueConf3 >= fFalseConf3) ? fTrueConf3 : fFalseConf3;
            float fRatio3 = fMin3 / fMax3;

            if (fTrueConf3 > 0.0f)
            {
                SaveConfidence PushDOM3(&fConfidence);
                fConfidence = (fConfidence <= fTrueConf3) ? fConfidence : fTrueConf3;
                if (fConfidence < fTrueConf3 && fTrueConf3 < 0.5f)
                    fConfidence = fConfidence * fRatio3;
                float fCandidateBest = fConfidence;
                if (0.0f >= fCandidateBest)
                    fBestConfidence = 0.0f;
                else
                    fBestConfidence = fCandidateBest;
                pEntity->QueueActionSetDesire(6, fConfidence, -1.0f, fvNotSet, fvNotSet);
            }
        }

        float fOnMush = OnMushrooms(g_pScriptCurrentFielder);
        float fInvinc = Invincible(g_pScriptCurrentFielder);
        float fTrueConf4 = (fInvinc >= fOnMush) ? fInvinc : fOnMush;
        float fFalseConf4 = 1.0f - fTrueConf4;
        float fMin4 = (fTrueConf4 <= fFalseConf4) ? fTrueConf4 : fFalseConf4;
        float fMax4 = (fTrueConf4 >= fFalseConf4) ? fTrueConf4 : fFalseConf4;
        float fRatio4 = fMin4 / fMax4;

        if (fTrueConf4 > 0.0f)
        {
            SaveConfidence PushDOM4(&fConfidence);
            fConfidence = (fConfidence <= fTrueConf4) ? fConfidence : fTrueConf4;
            if (fConfidence < fTrueConf4 && fTrueConf4 < 0.5f)
                fConfidence = fConfidence * fRatio4;

            float fOnMush2, fNotRepeatSlide;
            fNotRepeatSlide = 1.0f - RepeatingLastDesire(g_pScriptCurrentFielder, edSlideAttack);
            fOnMush2 = OnMushrooms(g_pScriptCurrentFielder);
            float fNearTo = NearTo(g_pScriptCurrentFielder, g_pScriptBallOwner);
            float fMaxNearMush = (fNearTo >= fOnMush2) ? fNearTo : fOnMush2;
            float fTrueConf5 = (fMaxNearMush <= fNotRepeatSlide) ? fMaxNearMush : fNotRepeatSlide;
            float fFalseConf5 = 1.0f - fTrueConf5;
            float fMin5 = (fTrueConf5 <= fFalseConf5) ? fTrueConf5 : fFalseConf5;
            float fMax5 = (fTrueConf5 >= fFalseConf5) ? fTrueConf5 : fFalseConf5;
            float fRatio5 = fMin5 / fMax5;

            if (fTrueConf5 > 0.0f)
            {
                SaveConfidence PushDOM5(&fConfidence);
                fConfidence = (fConfidence <= fTrueConf5) ? fConfidence : fTrueConf5;
                if (fConfidence < fTrueConf5 && fTrueConf5 < 0.5f)
                    fConfidence = fConfidence * fRatio5;
                if (fBestConfidence >= fConfidence)
                    fBestConfidence = fBestConfidence;
                else
                    fBestConfidence = fConfidence;
                pEntity->QueueActionSetDesire(15, fConfidence, 0.0f, FuzzyVariant(g_pScriptBallOwner), fvNotSet);
            }
        }

        float fWindUp = WindingUpForShot(g_pScriptBallOwner);
        float fStunned = Stunned(g_pScriptCurrentTeam->GetGoalie());
        float fTrueConf6 = (fStunned >= fWindUp) ? fStunned : fWindUp;
        float fFalseConf6 = 1.0f - fTrueConf6;
        float fMin6 = (fTrueConf6 <= fFalseConf6) ? fTrueConf6 : fFalseConf6;
        float fMax6 = (fTrueConf6 >= fFalseConf6) ? fTrueConf6 : fFalseConf6;
        float fRatio6 = fMin6 / fMax6;

        if (fTrueConf6 > 0.0f)
        {
            SaveConfidence PushDOM6(&fConfidence);
            fConfidence = (fConfidence <= fTrueConf6) ? fConfidence : fTrueConf6;
            if (fConfidence < fTrueConf6 && fTrueConf6 < 0.5f)
                fConfidence = fConfidence * fRatio6;

            float fAtIdeal = AtIdealDistanceForTackling(g_pScriptCurrentFielder, g_pScriptBallOwner);
            float fClose = CloseTo(g_pScriptCurrentFielder, g_pScriptBallOwner);
            float fTrueConf7 = (fClose >= fAtIdeal) ? fClose : fAtIdeal;
            float fFalseConf7 = 1.0f - fTrueConf7;
            float fMin7 = (fTrueConf7 <= fFalseConf7) ? fTrueConf7 : fFalseConf7;
            float fMax7 = (fTrueConf7 >= fFalseConf7) ? fTrueConf7 : fFalseConf7;
            float fRatio7 = fMin7 / fMax7;

            if (fTrueConf7 > 0.0f)
            {
                SaveConfidence PushDOM7(&fConfidence);
                fConfidence = (fConfidence <= fTrueConf7) ? fConfidence : fTrueConf7;
                if (fConfidence < fTrueConf7 && fTrueConf7 < 0.5f)
                    fConfidence = fConfidence * fRatio7;

                float fNotRepeatHit = 1.0f - RepeatingLastDesire(g_pScriptCurrentFielder, edHeavyAttack);
                float fRepeatHit = 1.0f - fNotRepeatHit;
                float fMin8 = (fNotRepeatHit <= fRepeatHit) ? fNotRepeatHit : fRepeatHit;
                float fMax8 = (fNotRepeatHit >= fRepeatHit) ? fNotRepeatHit : fRepeatHit;
                float fRatio8 = fMin8 / fMax8;

                if (fNotRepeatHit > 0.0f)
                {
                    SaveConfidence PushDOM8(&fConfidence);
                    fConfidence = (fConfidence <= fNotRepeatHit) ? fConfidence : fNotRepeatHit;
                    if (fConfidence < fNotRepeatHit && fNotRepeatHit < 0.5f)
                        fConfidence = fConfidence * fRatio8;
                    if (fBestConfidence >= fConfidence)
                        fBestConfidence = fBestConfidence;
                    else
                        fBestConfidence = fConfidence;
                    pEntity->QueueActionSetDesire(5, fConfidence, 0.0f, FuzzyVariant(g_pScriptBallOwner), fvNotSet);
                    SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                    pEntity->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(pTweaks->Def_BlockShotChance, Aggressive(g_pScriptCurrentFielder));
                }
            }

            if (fFalseConf7 > 0.0f)
            {
                SaveConfidence PushDOM9(&fConfidence);
                fConfidence = (fConfidence <= fFalseConf7) ? fConfidence : fFalseConf7;
                if (fConfidence < fFalseConf7 && fFalseConf7 < 0.5f)
                    fConfidence = fConfidence * fRatio7;

                float fMarking2 = Marking(g_pScriptCurrentFielder, g_pScriptBallOwner);
                float fNearTo2 = NearTo(g_pScriptCurrentFielder, g_pScriptBallOwner);
                float fTrueConf9 = (fNearTo2 >= fMarking2) ? fNearTo2 : fMarking2;
                float fFalseConf9 = 1.0f - fTrueConf9;
                float fMin9 = (fTrueConf9 <= fFalseConf9) ? fTrueConf9 : fFalseConf9;
                float fMax9 = (fTrueConf9 >= fFalseConf9) ? fTrueConf9 : fFalseConf9;
                float fRatio9 = fMin9 / fMax9;

                if (fTrueConf9 > 0.0f)
                {
                    SaveConfidence PushDOM10(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConf9) ? fConfidence : fTrueConf9;
                    if (fConfidence < fTrueConf9 && fTrueConf9 < 0.5f)
                        fConfidence = fConfidence * fRatio9;
                    if (fBestConfidence >= fConfidence)
                        fBestConfidence = fBestConfidence;
                    else
                        fBestConfidence = fConfidence;
                    pEntity->QueueActionSetDesire(6, fConfidence, -1.0f, fvNotSet, fvNotSet);
                    SkillTweaks* pTweaks2 = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                    pEntity->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(pTweaks2->Def_BlockShotChance, Aggressive(g_pScriptCurrentFielder));
                }
            }
        }
    }

    // Section 2
    float fMarkingOuter = Marking(g_pScriptCurrentFielder, g_pScriptBallOwner);
    float fOnTheirTeamOuter = OnTheirTeam(g_pScriptBallOwner);
    float fTrueConf10 = (fOnTheirTeamOuter <= fMarkingOuter) ? fOnTheirTeamOuter : fMarkingOuter;
    float fFalseConf10 = 1.0f - fTrueConf10;
    float fMin10 = (fTrueConf10 <= fFalseConf10) ? fTrueConf10 : fFalseConf10;
    float fMax10 = (fTrueConf10 >= fFalseConf10) ? fTrueConf10 : fFalseConf10;
    float fRatio10 = fMin10 / fMax10;

    if (fTrueConf10 > 0.0f)
    {
        SaveConfidence PushDOM11(&fConfidence);
        fConfidence = (fConfidence <= fTrueConf10) ? fConfidence : fTrueConf10;
        if (fConfidence < fTrueConf10 && fTrueConf10 < 0.5f)
            fConfidence = fConfidence * fRatio10;

        float fFrozen = Frozen(g_pScriptBallOwner);
        float fNotFrozen = 1.0f - fFrozen;
        float fMin11 = (fFrozen <= fNotFrozen) ? fFrozen : fNotFrozen;
        float fMax11 = (fFrozen >= fNotFrozen) ? fFrozen : fNotFrozen;
        float fRatio11 = fMin11 / fMax11;

        if (fFrozen > 0.0f)
        {
            SaveConfidence PushDOM12(&fConfidence);
            fConfidence = (fConfidence <= fFrozen) ? fConfidence : fFrozen;
            if (fConfidence < fFrozen && fFrozen < 0.5f)
                fConfidence = fConfidence * fRatio11;

            float fNotRepeatHit2 = 1.0f - RepeatingLastDesire(g_pScriptCurrentFielder, edHeavyAttack);
            float fCloseMark = CloseTo(g_pScriptCurrentFielder, g_pScriptCurrentMark);
            float fTrueConf12 = (fCloseMark <= fNotRepeatHit2) ? fCloseMark : fNotRepeatHit2;
            float fFalseConf12 = 1.0f - fTrueConf12;
            float fMin12 = (fTrueConf12 <= fFalseConf12) ? fTrueConf12 : fFalseConf12;
            float fMax12 = (fTrueConf12 >= fFalseConf12) ? fTrueConf12 : fFalseConf12;
            float fRatio12 = fMin12 / fMax12;

            if (fTrueConf12 > 0.0f)
            {
                SaveConfidence PushDOM13(&fConfidence);
                fConfidence = (fConfidence <= fTrueConf12) ? fConfidence : fTrueConf12;
                if (fConfidence < fTrueConf12 && fTrueConf12 < 0.5f)
                    fConfidence = fConfidence * fRatio12;
                if (fBestConfidence >= fConfidence)
                    fBestConfidence = fBestConfidence;
                else
                    fBestConfidence = fConfidence;
                pEntity->QueueActionSetDesire(5, fConfidence, 0.0f, FuzzyVariant(g_pScriptBallOwner), fvNotSet);
            }

            if (fFalseConf12 > 0.0f)
            {
                SaveConfidence PushDOM14(&fConfidence);
                fConfidence = (fConfidence <= fFalseConf12) ? fConfidence : fFalseConf12;
                if (fConfidence < fFalseConf12 && fFalseConf12 < 0.5f)
                    fConfidence = fConfidence * fRatio12;
                if (fBestConfidence >= fConfidence)
                    fBestConfidence = fBestConfidence;
                else
                    fBestConfidence = fConfidence;
                pEntity->QueueActionSetDesire(6, fConfidence, -1.0f, fvNotSet, fvNotSet);
            }
        }

        float fNearNet = NearToPlayersNet(g_pScriptBall, g_pScriptCurrentFielder);
        float fNotNearNet = 1.0f - fNearNet;
        float fMin13 = (fNearNet <= fNotNearNet) ? fNearNet : fNotNearNet;
        float fMax13 = (fNearNet >= fNotNearNet) ? fNearNet : fNotNearNet;
        float fRatio13 = fMin13 / fMax13;

        if (fNearNet > 0.0f)
        {
            SaveConfidence PushDOM15(&fConfidence);
            fConfidence = (fConfidence <= fNearNet) ? fConfidence : fNearNet;
            if (fConfidence < fNearNet && fNearNet < 0.5f)
                fConfidence = fConfidence * fRatio13;

            float fInControl = InControlOfBall(g_pScriptBallOwner);
            float fOpenMyNet = OpenToMyNet(g_pScriptBallOwner);
            float fLikelyScore = LikelyToScore(g_pScriptBallOwner);
            float fK2b = 0.2f;
            float fK4b = 0.4f;
            float fWeighted1 = fLikelyScore * fK4b + fOpenMyNet * fK4b + (1.0f - fInControl) * fK2b;
            float fWindUp2 = WindingUpForShot(g_pScriptBallOwner);
            float fTrueConf14 = (fWindUp2 >= fWeighted1) ? fWindUp2 : fWeighted1;
            float fFalseConf14 = 1.0f - fTrueConf14;
            float fMin14 = (fTrueConf14 <= fFalseConf14) ? fTrueConf14 : fFalseConf14;
            float fMax14 = (fTrueConf14 >= fFalseConf14) ? fTrueConf14 : fFalseConf14;
            float fRatio14 = fMin14 / fMax14;

            if (fTrueConf14 > 0.0f)
            {
                SaveConfidence PushDOM16(&fConfidence);
                fConfidence = (fConfidence <= fTrueConf14) ? fConfidence : fTrueConf14;
                if (fConfidence < fTrueConf14 && fTrueConf14 < 0.5f)
                    fConfidence = fConfidence * fRatio14;
                float fResult1 = AttackBallOwner(fConfidence, pEntity).mData.f;
                if (fResult1 >= fBestConfidence)
                    fBestConfidence = fResult1;
            }
        }

        if (fNotNearNet > 0.0f)
        {
            SaveConfidence PushDOM17(&fConfidence);
            fConfidence = (fConfidence <= fNotNearNet) ? fConfidence : fNotNearNet;
            if (fConfidence < fNotNearNet && fNotNearNet < 0.5f)
                fConfidence = fConfidence * fRatio13;

            float fFarTheirNet = FarToTheirNetB(g_pScriptBall);
            float fNotFarTheirNet = 1.0f - fFarTheirNet;
            float fMin15 = (fFarTheirNet <= fNotFarTheirNet) ? fFarTheirNet : fNotFarTheirNet;
            float fMax15 = (fFarTheirNet >= fNotFarTheirNet) ? fFarTheirNet : fNotFarTheirNet;
            float fRatio15 = fMin15 / fMax15;

            if (fFarTheirNet > 0.0f)
            {
                SaveConfidence PushDOM18(&fConfidence);
                fConfidence = (fConfidence <= fFarTheirNet) ? fConfidence : fFarTheirNet;
                if (fConfidence < fFarTheirNet && fFarTheirNet < 0.5f)
                    fConfidence = fConfidence * fRatio15;

                float fFacing = Facing(g_pScriptCurrentFielder, g_pScriptBallOwner);
                float fInControl2 = InControlOfBall(g_pScriptBallOwner);
                float fNearTo3 = NearTo(g_pScriptBallOwner, g_pScriptCurrentFielder);
                float fK3a = 0.3f;
                float fK4a = 0.4f;
                float fWeighted2 = fNearTo3 * fK4a + (1.0f - fInControl2) * fK3a + fFacing * fK3a;
                float fFalseConf16 = 1.0f - fWeighted2;
                float fMin16 = (fWeighted2 <= fFalseConf16) ? fWeighted2 : fFalseConf16;
                float fMax16 = (fWeighted2 >= fFalseConf16) ? fWeighted2 : fFalseConf16;
                float fRatio16 = fMin16 / fMax16;

                if (fWeighted2 > 0.0f)
                {
                    SaveConfidence PushDOM19(&fConfidence);
                    fConfidence = (fConfidence <= fWeighted2) ? fConfidence : fWeighted2;
                    if (fConfidence < fWeighted2 && fWeighted2 < 0.5f)
                        fConfidence = fConfidence * fRatio16;
                    float fResult2 = AttackBallOwner(fConfidence, pEntity).mData.f;
                    if (fResult2 >= fBestConfidence)
                        fBestConfidence = fResult2;
                }
            }

            if (fNotFarTheirNet > 0.0f)
            {
                SaveConfidence PushDOM20(&fConfidence);
                fConfidence = (fConfidence <= fNotFarTheirNet) ? fConfidence : fNotFarTheirNet;
                if (fConfidence < fNotFarTheirNet && fNotFarTheirNet < 0.5f)
                    fConfidence = fConfidence * fRatio15;

                float fFacing2 = Facing(g_pScriptCurrentFielder, g_pScriptBallOwner);
                float fInControl3 = InControlOfBall(g_pScriptBallOwner);
                float fNearTo4 = NearTo(g_pScriptBallOwner, g_pScriptCurrentFielder);
                float fOpenTheirNet = OpenToTheirNet(g_pScriptCurrentFielder);
                float fLikelyScore2 = LikelyToScore(g_pScriptCurrentFielder);

                float fK2 = 0.2f;
                float fK13 = 0.13f;
                float fK12 = 0.12f;
                float fK15 = 0.15f;
                float fWeighted3 = fLikelyScore2 * fK2;
                float fWeighted3Final = fWeighted3 + fConfidence * fK13
                                      + fOpenTheirNet * fK2 + fNearTo4 * fK2
                                      + (1.0f - fInControl3) * fK12 + fFacing2 + fK15;

                float fFalseConf17 = 1.0f - fWeighted3Final;
                float fMin17 = (fWeighted3Final <= fFalseConf17) ? fWeighted3Final : fFalseConf17;
                float fMax17 = (fWeighted3Final >= fFalseConf17) ? fWeighted3Final : fFalseConf17;
                float fRatio17 = fMin17 / fMax17;

                if (fWeighted3Final > 0.0f)
                {
                    SaveConfidence PushDOM21(&fConfidence);
                    fConfidence = (fConfidence <= fWeighted3Final) ? fConfidence : fWeighted3Final;
                    if (fConfidence < fWeighted3Final && fWeighted3Final < 0.5f)
                        fConfidence = fConfidence * fRatio17;
                    float fResult3 = AttackBallOwner(fConfidence, pEntity).mData.f;
                    if (fResult3 >= fBestConfidence)
                        fBestConfidence = fResult3;
                }
            }
        }
    }

    return FuzzyVariant(fBestConfidence);
}

/**
 * Offset/Address/Size: 0x1A90 | 0x80087148 | size: 0x6B0
 */
FuzzyVariant Fuzzy::AttackBallOwner(float fConfidence, cDecisionEntity* pEntity)
{
    float fBestConfidence = 0.0f;

    float fDist = AtIdealDistanceForTackling(g_pScriptCurrentFielder, g_pScriptBallOwner);
    float fClose = CloseTo(g_pScriptCurrentFielder, g_pScriptBallOwner);
    if (fClose >= fDist)
        fDist = fClose;

    float fFacing1 = Facing(g_pScriptCurrentFielder, g_pScriptBallOwner);
    float fFacing2 = Facing(g_pScriptBallOwner, g_pScriptCurrentFielder);

    float k5 = 0.5f, k3 = 0.3f, k2 = 0.2f;
    float fW1 = fFacing1 * k2;
    float fTrueConfidence = fW1 + fFacing2 * k3 + fDist * k5;
    float fFalseConfidence = 1.0f - fTrueConfidence;

    float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fBranchRatio = fMin / fMax;

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);
        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            fConfidence = fConfidence * fBranchRatio;

        float fNotFacingSideline = 1.0f - FacingSideline(g_pScriptCurrentFielder);
        float fTrueConfidence2 = 1.0f - RepeatingLastDesire(g_pScriptCurrentFielder, edSlideAttack);

        if (fTrueConfidence2 <= fNotFacingSideline)
        {
            goto _abo_keep2;
        }
        else
        {
            fTrueConfidence2 = fNotFacingSideline;
        }
    _abo_keep2:;

        float fFalseConfidence2 = 1.0f - fTrueConfidence2;
        float fMin2 = (fTrueConfidence2 <= fFalseConfidence2) ? fTrueConfidence2 : fFalseConfidence2;
        float fMax2 = (fTrueConfidence2 >= fFalseConfidence2) ? fTrueConfidence2 : fFalseConfidence2;
        float fRatio2 = fMin2 / fMax2;

        if (fTrueConfidence2 > 0.0f)
        {
            SaveConfidence PushDOM2(&fConfidence);
            fConfidence = (fConfidence <= fTrueConfidence2) ? fConfidence : fTrueConfidence2;
            if (fConfidence < fTrueConfidence2 && fTrueConfidence2 < 0.5f)
                fConfidence = fConfidence * fRatio2;

            float fQ = fConfidence;
            if (0.0f >= fQ)
                fBestConfidence = 0.0f;
            else
                fBestConfidence = fQ;

            pEntity->QueueActionSetDesire(15, fConfidence, 0.0f, FuzzyVariant(g_pScriptBallOwner), fvNotSet);

            SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            pEntity->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(pTweaks->Def_SlideAttackChance, Aggressive(g_pScriptCurrentFielder));
        }

        float fNotSeparating = 1.0f - SeparatingFrom(g_pScriptCurrentFielder, g_pScriptBallOwner);
        float fTrueConfidence3 = 1.0f - RepeatingLastDesire(g_pScriptCurrentFielder, edHeavyAttack);

        if (fTrueConfidence3 <= fNotSeparating)
        {
            goto _abo_keep3;
        }
        else
        {
            fTrueConfidence3 = fNotSeparating;
        }
    _abo_keep3:;

        float fFalseConfidence3 = 1.0f - fTrueConfidence3;
        float fMin3 = (fTrueConfidence3 <= fFalseConfidence3) ? fTrueConfidence3 : fFalseConfidence3;
        float fMax3 = (fTrueConfidence3 >= fFalseConfidence3) ? fTrueConfidence3 : fFalseConfidence3;
        float fRatio3 = fMin3 / fMax3;

        if (fTrueConfidence3 > 0.0f)
        {
            SaveConfidence PushDOM3(&fConfidence);
            fConfidence = (fConfidence <= fTrueConfidence3) ? fConfidence : fTrueConfidence3;
            if (fConfidence < fTrueConfidence3 && fTrueConfidence3 < 0.5f)
                fConfidence = fConfidence * fRatio3;

            if (fBestConfidence >= fConfidence)
                fBestConfidence = fBestConfidence;
            else
                fBestConfidence = fConfidence;

            pEntity->QueueActionSetDesire(5, fConfidence, 0.0f, FuzzyVariant(g_pScriptBallOwner), fvNotSet);

            SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            pEntity->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(pTweaks->Def_HeavyAttackChance, Aggressive(g_pScriptCurrentFielder));
        }
    }

    return FuzzyVariant(fBestConfidence);
}

/**
 * Offset/Address/Size: 0x5C4 | 0x80085C7C | size: 0x14CC
 */
FuzzyVariant Fuzzy::UsePowerupDefensive(float fConfidence, cDecisionEntity* pEntity)
{
    float fBestConfidence = 0.0f;
    FuzzyVariant target = GetPowerupTargetDefensive(g_pScriptCurrentTeam);

    float fOnScreen = DefensiveMin(
        OnScreen(g_pScriptCurrentFielder),
        OnScreen(target.mData.pPlayer));

    float fFalse = 1.0f - fOnScreen;
    float fMin = (fOnScreen <= fFalse) ? fOnScreen : fFalse;
    float fMax = (fOnScreen >= fFalse) ? fOnScreen : fFalse;
    float fRatio = fMin / fMax;

    if (fOnScreen > 0.0f)
    {
        SaveConfidence pushOnScreen(&fConfidence);
        fConfidence = (fConfidence <= fOnScreen) ? fConfidence : fOnScreen;
        if (fConfidence < fOnScreen && fOnScreen < 0.5f)
            fConfidence = fConfidence * fRatio;

        float fCaptain = Captain(g_pScriptCurrentFielder);
        float fSmallWeight = 0.2f;
        float fTargetWeight = 0.8f;
        float fCaptainConf = target.Confidence;
        fCaptainConf = fCaptainConf * fTargetWeight + fCaptain * fSmallWeight;
        float fCaptainFalse = 1.0f - fCaptainConf;
        float fCaptainMin = (fCaptainConf <= fCaptainFalse) ? fCaptainConf : fCaptainFalse;
        float fCaptainMax = (fCaptainConf >= fCaptainFalse) ? fCaptainConf : fCaptainFalse;
        float fCaptainRatio = fCaptainMin / fCaptainMax;

        if (fCaptainConf > 0.0f)
        {
            SaveConfidence pushCaptain(&fConfidence);
            fConfidence = (fConfidence <= fCaptainConf) ? fConfidence : fCaptainConf;
            if (fConfidence < fCaptainConf && fCaptainConf < 0.5f)
                fConfidence = fConfidence * fCaptainRatio;

            {
                float fLikely = LikelyToUsePowerup(g_pScriptCurrentFielder, 0);
                float fLFalse = 1.0f - fLikely;
                float fLMin = (fLikely <= fLFalse) ? fLikely : fLFalse;
                float fLMax = (fLikely >= fLFalse) ? fLikely : fLFalse;
                float fLRatio = fLMin / fLMax;
                if (fLikely > 0.0f)
                {
                    SaveConfidence pushP(&fConfidence);
                    fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;
                    if (fConfidence < fLikely && fLikely < 0.5f)
                        fConfidence = fConfidence * fLRatio;
                    float fQueuedConfidence = fConfidence;
                    fBestConfidence = (0.0f >= fQueuedConfidence) ? 0.0f : fQueuedConfidence;
                    pEntity->QueueActionSetDesire(FIELDERDESIRE_USE_POWERUP, fConfidence, 0.0f, FuzzyVariant(0), target);
                }
            }
            {
                float fLikely = LikelyToUsePowerup(g_pScriptCurrentFielder, 1);
                float fLFalse = 1.0f - fLikely;
                float fLMin = (fLikely <= fLFalse) ? fLikely : fLFalse;
                float fLMax = (fLikely >= fLFalse) ? fLikely : fLFalse;
                float fLRatio = fLMin / fLMax;
                if (fLikely > 0.0f)
                {
                    SaveConfidence pushP(&fConfidence);
                    fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;
                    if (fConfidence < fLikely && fLikely < 0.5f)
                        fConfidence = fConfidence * fLRatio;
                    float fQueuedConfidence = fConfidence;
                    fBestConfidence = (fBestConfidence >= fQueuedConfidence) ? fBestConfidence : fQueuedConfidence;
                    pEntity->QueueActionSetDesire(FIELDERDESIRE_USE_POWERUP, fConfidence, 0.0f, FuzzyVariant(1), target);
                }
            }
            {
                float fLikely = LikelyToUsePowerup(g_pScriptCurrentFielder, 2);
                float fLFalse = 1.0f - fLikely;
                float fLMin = (fLikely <= fLFalse) ? fLikely : fLFalse;
                float fLMax = (fLikely >= fLFalse) ? fLikely : fLFalse;
                float fLRatio = fLMin / fLMax;
                if (fLikely > 0.0f)
                {
                    SaveConfidence pushP(&fConfidence);
                    fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;
                    if (fConfidence < fLikely && fLikely < 0.5f)
                        fConfidence = fConfidence * fLRatio;
                    float fQueuedConfidence = fConfidence;
                    fBestConfidence = (fBestConfidence >= fQueuedConfidence) ? fBestConfidence : fQueuedConfidence;
                    pEntity->QueueActionSetDesire(FIELDERDESIRE_USE_POWERUP, fConfidence, 0.0f, FuzzyVariant(2), target);
                }
            }
            {
                float fLikely = LikelyToUsePowerup(g_pScriptCurrentFielder, 3);
                float fLFalse = 1.0f - fLikely;
                float fLMin = (fLikely <= fLFalse) ? fLikely : fLFalse;
                float fLMax = (fLikely >= fLFalse) ? fLikely : fLFalse;
                float fLRatio = fLMin / fLMax;
                if (fLikely > 0.0f)
                {
                    SaveConfidence pushP(&fConfidence);
                    fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;
                    if (fConfidence < fLikely && fLikely < 0.5f)
                        fConfidence = fConfidence * fLRatio;
                    float fQueuedConfidence = fConfidence;
                    fBestConfidence = (fBestConfidence >= fQueuedConfidence) ? fBestConfidence : fQueuedConfidence;
                    pEntity->QueueActionSetDesire(FIELDERDESIRE_USE_POWERUP, fConfidence, 0.0f, FuzzyVariant(3), target);
                }
            }
            {
                float fLikely = LikelyToUsePowerup(g_pScriptCurrentFielder, 4);
                float fLFalse = 1.0f - fLikely;
                float fLMin = (fLikely <= fLFalse) ? fLikely : fLFalse;
                float fLMax = (fLikely >= fLFalse) ? fLikely : fLFalse;
                float fLRatio = fLMin / fLMax;
                if (fLikely > 0.0f)
                {
                    SaveConfidence pushP(&fConfidence);
                    fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;
                    if (fConfidence < fLikely && fLikely < 0.5f)
                        fConfidence = fConfidence * fLRatio;
                    float fQueuedConfidence = fConfidence;
                    fBestConfidence = (fBestConfidence >= fQueuedConfidence) ? fBestConfidence : fQueuedConfidence;
                    pEntity->QueueActionSetDesire(FIELDERDESIRE_USE_POWERUP, fConfidence, 0.0f, FuzzyVariant(4), target);
                }
            }
            {
                float fWindup = FGREATER(InGoodWindupPosition(g_pScriptCurrentFielder).mData.f, 0.3f);
                float fWindupFalse = 1.0f - fWindup;
                float fWindupMin = (fWindup <= fWindupFalse) ? fWindup : fWindupFalse;
                float fWindupMax = (fWindup >= fWindupFalse) ? fWindup : fWindupFalse;
                float fWindupRatio = fWindupMin / fWindupMax;
                if (fWindup > 0.0f)
                {
                    SaveConfidence pushWindup(&fConfidence);
                    fConfidence = (fConfidence <= fWindup) ? fConfidence : fWindup;
                    if (fConfidence < fWindup && fWindup < 0.5f)
                        fConfidence = fConfidence * fWindupRatio;
                    {
                        float fLikely = LikelyToUsePowerup(g_pScriptCurrentFielder, 5);
                        float fLFalse = 1.0f - fLikely;
                        float fLMin = (fLikely <= fLFalse) ? fLikely : fLFalse;
                        float fLMax = (fLikely >= fLFalse) ? fLikely : fLFalse;
                        float fLRatio = fLMin / fLMax;
                        if (fLikely > 0.0f)
                        {
                            SaveConfidence pushP(&fConfidence);
                            fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;
                            if (fConfidence < fLikely && fLikely < 0.5f)
                                fConfidence = fConfidence * fLRatio;
                            float fQueuedConfidence = fConfidence;
                            fBestConfidence = (fBestConfidence >= fQueuedConfidence) ? fBestConfidence : fQueuedConfidence;
                            pEntity->QueueActionSetDesire(FIELDERDESIRE_USE_POWERUP, fConfidence, 0.0f, FuzzyVariant(5), target);
                        }
                    }
                }
            }
        }
    }

    float fThreat = CalculateDefensiveThreat();

    float fThreatFalse = 1.0f - fThreat;
    float fThreatMin = (fThreat <= fThreatFalse) ? fThreat : fThreatFalse;
    float fThreatMax = (fThreat >= fThreatFalse) ? fThreat : fThreatFalse;
    float fThreatRatio = fThreatMin / fThreatMax;
    if (fThreat > 0.0f)
    {
        SaveConfidence pushThreat(&fConfidence);
        fConfidence = (fConfidence <= fThreat) ? fConfidence : fThreat;
        if (fConfidence < fThreat && fThreat < 0.5f)
            fConfidence = fConfidence * fThreatRatio;
        {
            float fLikely = LikelyToUsePowerup(g_pScriptCurrentFielder, 7);
            float fLFalse = 1.0f - fLikely;
            float fLMin = (fLikely <= fLFalse) ? fLikely : fLFalse;
            float fLMax = (fLikely >= fLFalse) ? fLikely : fLFalse;
            float fLRatio = fLMin / fLMax;
            if (fLikely > 0.0f)
            {
                SaveConfidence pushP(&fConfidence);
                fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;
                if (fConfidence < fLikely && fLikely < 0.5f)
                    fConfidence = fConfidence * fLRatio;
                float fQueuedConfidence = fConfidence;
                fBestConfidence = (fBestConfidence >= fQueuedConfidence) ? fBestConfidence : fQueuedConfidence;
                pEntity->QueueActionSetDesire(FIELDERDESIRE_USE_POWERUP, fConfidence, 0.0f, FuzzyVariant(7), fvNotSet);
            }
        }
    }
    {
        float fLikely = LikelyToUsePowerup(g_pScriptCurrentFielder, 6);
        float fLFalse = 1.0f - fLikely;
        float fLMin = (fLikely <= fLFalse) ? fLikely : fLFalse;
        float fLMax = (fLikely >= fLFalse) ? fLikely : fLFalse;
        float fLRatio = fLMin / fLMax;
        if (fLikely > 0.0f)
        {
            SaveConfidence pushP(&fConfidence);
            fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;
            if (fConfidence < fLikely && fLikely < 0.5f)
                fConfidence = fConfidence * fLRatio;
            float fQueuedConfidence = fConfidence;
            fBestConfidence = (fBestConfidence >= fQueuedConfidence) ? fBestConfidence : fQueuedConfidence;
            pEntity->QueueActionSetDesire(FIELDERDESIRE_USE_POWERUP, fConfidence, 0.0f, FuzzyVariant(6), fvNotSet);
        }
    }
    return FuzzyVariant(fBestConfidence);
}

/**
 * Offset/Address/Size: 0x0 | 0x800856B8 | size: 0x5C4
 */
FuzzyVariant Fuzzy::GetPowerupTargetDefensive(cTeam* TheTeam)
{
    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;
    FuzzyVariant(TheTeam).GetHash();
    (void)FuzzyVariant(TheTeam);
    for (int i = 0; i < 4; i++)
    {
        cFielder* theOpponent = g_pScriptOtherTeam->GetFielder(i);
        float fNotInvincible = Invincible(theOpponent);
        fNotInvincible = 1.0f - fNotInvincible;
        float fTrueConfidence = Incapacitated((cPlayer*)theOpponent);
        fTrueConfidence = 1.0f - fTrueConfidence;
        if (fTrueConfidence <= fNotInvincible)
        {
            fTrueConfidence = fTrueConfidence;
        }
        else
        {
            fTrueConfidence = fNotInvincible;
        }
        float fFalseConfidence = 1.0f - fTrueConfidence;
        float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fBranchRatio = fMin / fMax;
        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);
            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                fConfidence = fConfidence * fBranchRatio;
            fTrueConfidence = ReceivingPassDelayed(theOpponent);
            fFalseConfidence = BallOwner((cPlayer*)theOpponent);
            float fChasingBall = ChasingBall((cPlayer*)theOpponent);
            fFalseConfidence = (fFalseConfidence >= fTrueConfidence) ? fFalseConfidence : fTrueConfidence;
            fFalseConfidence = (fChasingBall >= fFalseConfidence) ? fChasingBall : fFalseConfidence;
            FuzzyVariant generalThreatConfidence(fFalseConfidence);
            float fMarking = Marking(g_pScriptCurrentFielder, (cPlayer*)theOpponent);
            float fTrueConfidence2 = (generalThreatConfidence.mData.f <= fMarking) ? generalThreatConfidence.mData.f : fMarking;
            float fFalseConfidence2 = 1.0f - fTrueConfidence2;
            float fMin2 = (fTrueConfidence2 <= fFalseConfidence2) ? fTrueConfidence2 : fFalseConfidence2;
            float fMax2 = (fTrueConfidence2 >= fFalseConfidence2) ? fTrueConfidence2 : fFalseConfidence2;
            float fBranchRatio2 = fMin2 / fMax2;
            if (fTrueConfidence2 > 0.0f)
            {
                SaveConfidence PushDOM2(&fConfidence);
                fConfidence = (fConfidence <= fTrueConfidence2) ? fConfidence : fTrueConfidence2;
                if (fConfidence < fTrueConfidence2 && fTrueConfidence2 < 0.5f)
                    fConfidence = fConfidence * fBranchRatio2;
                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    bestValue = FuzzyVariant((cPlayer*)theOpponent);
                }
            }
        }
    }

    bestValue.Confidence = fBestConfidence;
    return bestValue;
}
