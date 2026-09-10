#include "Game/AI/Scripts/Plays/DefaultOffensive.h"

#include "Game/AI/Fuzzy.h"
#include "Game/AI/Scripts/ScriptCaching.h"
#include "Game/AI/Scripts/ScriptQuestions.h"
#include "Game/AI/SpaceSearch.h"

class cTeam;

extern cFielder* g_pScriptCurrentFielder;
extern cTeam* g_pScriptCurrentTeam;
extern cTeam* g_pCurrentlyUpdatingTeam;
extern FuzzyVariant fvNotSet;
// PORT: the other two, so all five are declared once and at file scope.
extern cGame* g_pGame;
extern cTeam* g_pScriptOtherTeam;

/**
 * Offset/Address/Size: 0x6B9C | 0x80093628 | size: 0x43C
 */
FuzzyVariant Fuzzy::AbortOffensivePlay(cDecisionEntity* pDecision)
{
    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    float fTrueConfidence = Offensive(g_pScriptCurrentTeam);
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

static inline float OffensiveDanger(const FuzzyVariant& value)
{
    return value.mData.f;
}

static inline float OffensiveAnd(float a, float b)
{
    return (a <= b) ? a : b;
}

static inline float OffensiveAnd(float a, float b, float c)
{
    b = (b <= c) ? b : c;
    return (a <= b) ? a : b;
}

static inline float OffensiveOr(float a, float b)
{
    return (a >= b) ? a : b;
}

static inline float OffensiveOr(float a, float b, float c)
{
    b = (b >= c) ? b : c;
    return (a >= b) ? a : b;
}

static inline float OffensiveNot(float value)
{
    return 1.0f - value;
}

static inline float NotUserControlled(cTeam* team)
{
    float value = UserControlledT(team);
    value = 1.0f - value;
    return value;
}

/**
 * Offset/Address/Size: 0x51D8 | 0x80091C64 | size: 0x19C4
 */
FuzzyVariant Fuzzy::DefaultOffensivePlay(cDecisionEntity* pDecision)
{
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    float fTrueConfidence = BallOwner(g_pScriptCurrentFielder);
    float fFalseConfidence = 1.0f - fTrueConfidence;

    float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fBranchRatio = fMin / fMax;
    float fTopLevelBranchRatio = fBranchRatio;
    float fTopLevelFalseConfidence = fFalseConfidence;

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);
        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            fConfidence = fConfidence * fBranchRatio;

        const FuzzyVariant& doShooting = DoShooting(fConfidence, pDecision);
        float fDoShooting = (doShooting.mData.f >= 0.0f) ? doShooting.mData.f : 0.0f;

        float fDoPassing = OffensiveDanger(DoPassing(fConfidence, pDecision));
        fDoPassing = fBestConfidence = OffensiveOr(fDoPassing, fDoShooting);

        float fGoodBallCarrier = OffensiveDanger(GoodBallCarrier(g_pScriptCurrentFielder));

        float fNotRepeatingDeke = OffensiveNot(RepeatingLastDesire(g_pScriptCurrentFielder, edDeke));
        float fNotCloseSideline = OffensiveNot(CloseToSideline(g_pScriptCurrentFielder));
        float fNotInvincible = OffensiveNot(Invincible(g_pScriptCurrentFielder));

        {
            float fTrueConfidence = InControlOfBall(g_pScriptCurrentFielder);
            fNotCloseSideline = (fNotCloseSideline <= fNotRepeatingDeke) ? fNotCloseSideline : fNotRepeatingDeke;
            fNotInvincible = (fNotInvincible <= fNotCloseSideline) ? fNotInvincible : fNotCloseSideline;
            fTrueConfidence = (fTrueConfidence <= fNotInvincible) ? fTrueConfidence : fNotInvincible;

            float fFalseConfidence = 1.0f - fTrueConfidence;
            float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fBranchRatio = fMin / fMax;

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM2(&fConfidence);
                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    fConfidence = fConfidence * fBranchRatio;

                fTrueConfidence = FGREATER(Attacked(g_pScriptCurrentFielder), 0.4f);
                fFalseConfidence = 1.0f - fTrueConfidence;

                fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                fBranchRatio = fMin / fMax;

                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM3(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;

                    fBestConfidence = OffensiveOr(fBestConfidence, fConfidence);

                    pDecision->QueueActionSetDesire(2, fConfidence, -1.0f, fvNotSet, fvNotSet);

                    SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                    pDecision->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(pTweaks->Off_DekeChance, Deker(g_pScriptCurrentFielder));
                }

                if (fFalseConfidence > 0.0f)
                {
                    SaveConfidence PushDOM3(&fConfidence);
                    fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;

                    if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;

                    fTrueConfidence = FLESS(Open(g_pScriptCurrentFielder), 0.5f);
                    float fOpenFalseConfidence = 1.0f - fTrueConfidence;

                    fMin = (fTrueConfidence <= fOpenFalseConfidence) ? fTrueConfidence : fOpenFalseConfidence;
                    fMax = (fTrueConfidence >= fOpenFalseConfidence) ? fTrueConfidence : fOpenFalseConfidence;
                    fBranchRatio = fMin / fMax;

                    if (fTrueConfidence > 0.0f)
                    {
                        SaveConfidence PushDOM4(&fConfidence);
                        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                            fConfidence = fConfidence * fBranchRatio;

                        fBestConfidence = OffensiveOr(fBestConfidence, fConfidence);

                        pDecision->QueueActionSetDesire(2, fConfidence, -1.0f, fvNotSet, fvNotSet);

                        SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                        pDecision->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(0.5f * pTweaks->Off_DekeChance, Deker(g_pScriptCurrentFielder));
                    }
                }
            }
        }

        {
            float fTrueConfidence = OffensiveAnd(
                NotUserControlled(g_pScriptCurrentTeam),
                BallOwner(g_pScriptCurrentFielder));

            float fFalseConfidence = 1.0f - fTrueConfidence;
            float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fBranchRatio = fMin / fMax;

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM2(&fConfidence);
                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    fConfidence = fConfidence * fBranchRatio;

                float fUsePowerup = UsePowerupOffensive(fConfidence, pDecision).mData.f;
                if (fUsePowerup >= fBestConfidence)
                    fBestConfidence = fUsePowerup;
            }

            {
                float fTrueConfidence = FGREATER(fGoodBallCarrier, (fDoShooting >= fDoPassing) ? fDoShooting : fDoPassing);
                fGoodBallCarrier = (fGoodBallCarrier <= fTrueConfidence) ? fGoodBallCarrier : fTrueConfidence;

                float fDifficult = FLESS(Difficult(g_pScriptCurrentTeam), 0.1f);
                float fFallback = (0.0f == fBestConfidence) ? 1.0f : 0.0f;

                fTrueConfidence = (fDifficult >= fGoodBallCarrier) ? fDifficult : fGoodBallCarrier;
                fTrueConfidence = (fFallback >= fTrueConfidence) ? fFallback : fTrueConfidence;

                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMin / fMax;

                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM2(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;

                    fBestConfidence = OffensiveOr(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetDesire(9, fConfidence, -1.0f, fvNotSet, fvNotSet);
                }
            }
        }
    }

    fBranchRatio = fTopLevelBranchRatio;
    fFalseConfidence = fTopLevelFalseConfidence;

    if (fFalseConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);
        fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;

        if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
            fConfidence = fConfidence * fBranchRatio;

        float fCutAndBreak = CutAndBreak(g_pScriptCurrentFielder).mData.f;

        fTrueConfidence = Striker(g_pScriptCurrentFielder);
        fTrueConfidence = (fTrueConfidence <= fCutAndBreak) ? fTrueConfidence : fCutAndBreak;

        float fCutFalseConfidence = 1.0f - fTrueConfidence;
        fMin = (fTrueConfidence <= fCutFalseConfidence) ? fTrueConfidence : fCutFalseConfidence;
        fMax = (fTrueConfidence >= fCutFalseConfidence) ? fTrueConfidence : fCutFalseConfidence;
        fBranchRatio = fMin / fMax;

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM2(&fConfidence);
            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                fConfidence = fConfidence * fBranchRatio;

            fBestConfidence = OffensiveOr(fBestConfidence, fConfidence);

            pDecision->QueueActionSetDesire(1, fConfidence, -1.0f, fvNotSet, fvNotSet);

            SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            pDecision->m_pLastQueuedAction->m_fSelectionChance = pTweaks->Off_CutAndBreakChance;
        }

        FuzzyVariant strategicBallOwner = GetStrategicBallCarrier(g_pScriptCurrentTeam);

        {
            float fWinger = OffensiveOr(
                Winger(g_pScriptCurrentFielder),
                Striker(g_pScriptCurrentFielder),
                BallOwner(g_pScriptCurrentTeam->GetGoalie()));

            float fFalseConfidence = 1.0f - fWinger;
            float fTrueConfidence = fWinger;

            float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fBranchRatio = fMin / fMax;

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM2(&fConfidence);
                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    fConfidence = fConfidence * fBranchRatio;

                float fNotStrategicConfidence = OffensiveOr(
                    OffensiveNot(strategicBallOwner.Confidence),
                    OffensiveNot(NearToTheirNet(g_pScriptCurrentFielder)),
                    WindingUpForShot((cFielder*)strategicBallOwner.mData.pPlayer));

                float fFalseConfidence = 1.0f - fNotStrategicConfidence;
                float fTrueConfidence = fNotStrategicConfidence;

                float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMin / fMax;

                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM3(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;

                    fBestConfidence = OffensiveOr(fBestConfidence, fConfidence);

                    pDecision->QueueActionSetDesire(10, fConfidence, -1.0f, fvNotSet, fvNotSet);
                }

                if (fFalseConfidence > 0.0f)
                {
                    SaveConfidence PushDOM3(&fConfidence);
                    fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;

                    if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;

                    fBestConfidence = OffensiveOr(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetDesire(4, fConfidence, -1.0f, fvNotSet, fvNotSet);
                }
            }

            if (fFalseConfidence > 0.0f)
            {
                SaveConfidence PushDOM2(&fConfidence);
                fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;

                if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
                    fConfidence = fConfidence * fBranchRatio;

                float fNotFarToTheirNet = 1.0f - FarToTheirNet(g_pScriptCurrentFielder);
                fTrueConfidence = (strategicBallOwner.Confidence <= fNotFarToTheirNet) ? strategicBallOwner.Confidence : fNotFarToTheirNet;
                fFalseConfidence = 1.0f - fTrueConfidence;

                fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                fBranchRatio = fMin / fMax;

                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM3(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;

                    fBestConfidence = OffensiveOr(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetDesire(4, fConfidence, -1.0f, fvNotSet, fvNotSet);
                }

                if (fFalseConfidence > 0.0f)
                {
                    SaveConfidence PushDOM3(&fConfidence);
                    fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;

                    if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;

                    fBestConfidence = OffensiveOr(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetDesire(3, fConfidence, -1.0f, fvNotSet, fvNotSet);
                }
            }
        }
    }

    return FuzzyVariant(fBestConfidence);
}

/**
 * Offset/Address/Size: 0x4A94 | 0x80091520 | size: 0x744
 */
FuzzyVariant Fuzzy::DoPassing(float fConfidence, cDecisionEntity* pDecision)
{

    float fPassTargetFalseConfidence;
    float fBestConfidence = 0.0f;

    float fFalseConfidence = 1.0f - Invincible(g_pScriptCurrentFielder);
    float fTrueConfidence = 1.0f - fFalseConfidence;
    float fMin = (fFalseConfidence <= fTrueConfidence) ? fFalseConfidence : fTrueConfidence;
    float fMax = (fFalseConfidence >= fTrueConfidence) ? fFalseConfidence : fTrueConfidence;
    float fBranchRatio = fMin / fMax;

    if (fFalseConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);
        fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;

        if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
            fConfidence = fConfidence * fBranchRatio;

        float fAvoidingPowerups = AvoidingPowerups(g_pScriptCurrentFielder);
        float fOne = 1.0f;
        FuzzyVariant theBestPassTarget = GetBestPassTarget(g_pScriptCurrentFielder);
        float fAdjustedConfidence = theBestPassTarget.Confidence * (fOne - fAvoidingPowerups) + fOne * fAvoidingPowerups;

        fTrueConfidence = FGREATER(theBestPassTarget.Confidence, 0.15f);
        fTrueConfidence = (fTrueConfidence <= fAdjustedConfidence) ? fTrueConfidence : fAdjustedConfidence;

        {
            fPassTargetFalseConfidence = 1.0f - fTrueConfidence;
            float fFalseConfidence = fPassTargetFalseConfidence;
            float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fBranchRatio = fMin / fMax;

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM(&fConfidence);
                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    fConfidence = fConfidence * fBranchRatio;

                {
                    float fOpenConfidence = OpenTo(g_pScriptCurrentFielder, theBestPassTarget.mData.pPlayer);
                    float fOpenFalseConfidence = 1.0f - fOpenConfidence;
                    float fOpenMin = (fOpenConfidence <= fOpenFalseConfidence) ? fOpenConfidence : fOpenFalseConfidence;
                    float fOpenMax = (fOpenConfidence >= fOpenFalseConfidence) ? fOpenConfidence : fOpenFalseConfidence;
                    float fOpenBranchRatio = fOpenMin / fOpenMax;

                    if (fOpenConfidence > 0.0f)
                    {
                        SaveConfidence PushDOM(&fConfidence);
                        fConfidence = (fConfidence <= fOpenConfidence) ? fConfidence : fOpenConfidence;

                        if (fConfidence < fOpenConfidence && fOpenConfidence < 0.5f)
                            fConfidence = fConfidence * fOpenBranchRatio;

                        float fCurrentConfidence = fConfidence;
                        if (0.0f >= fCurrentConfidence)
                            fBestConfidence = 0.0f;
                        else
                            fBestConfidence = fCurrentConfidence;

                        pDecision->QueueActionSetDesire(19, fConfidence, 0.0f, theBestPassTarget, FuzzyVariant(false));

                        SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                        pDecision->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(pTweaks->Off_GroundPassChance, Passer(g_pScriptCurrentFielder));
                    }

                    if (fOpenFalseConfidence > 0.0f)
                    {
                        SaveConfidence PushDOM(&fConfidence);
                        fConfidence = (fConfidence <= fOpenFalseConfidence) ? fConfidence : fOpenFalseConfidence;

                        if (fConfidence < fOpenFalseConfidence && fOpenFalseConfidence < 0.5f)
                            fConfidence = fConfidence * fOpenBranchRatio;

                        if (fBestConfidence >= fConfidence)
                            fBestConfidence = fBestConfidence;
                        else
                            fBestConfidence = fConfidence;

                        pDecision->QueueActionSetDesire(19, fConfidence, 0.0f, theBestPassTarget, FuzzyVariant(true));

                        SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                        pDecision->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(pTweaks->Off_VolleyPassChance, Passer(g_pScriptCurrentFielder));
                    }
                }
            }
        }
    }

    return FuzzyVariant(fBestConfidence);
}

/**
 * Offset/Address/Size: 0x4490 | 0x80090F1C | size: 0x604
 */
FuzzyVariant Fuzzy::GoodBallCarrier(cFielder* TheFielder)
{

    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fWindupScore;
    float fBestConfidence = 0.0f;

    FuzzyVariant fvFielder((cPlayer*)TheFielder);
    ((Variant*)&fvFielder)->GetHash();
    FuzzyVariant fvFielder2((cPlayer*)TheFielder);

    fWindupScore = InGoodWindupPosition(g_pScriptCurrentFielder).mData.f;

    float fOnMushrooms = OnMushrooms(g_pScriptCurrentFielder);
    float fInvincible = Invincible(g_pScriptCurrentFielder);

    float fTrueConfidence = (fInvincible >= fOnMushrooms) ? fInvincible : fOnMushrooms;
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
            float fLessWindup = FLESS(fWindupScore, 0.8f);
            bestValue = FuzzyVariant(fLessWindup);
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
            fOnMushrooms = FLESS(fWindupScore, 0.8f);
            fInvincible = 1.0f - CloseToMyNet(g_pScriptCurrentFielder);
            bestValue = FuzzyVariant((fFalseConfidence = 1.0f - OffensiveDanger(InDangerDelayed(g_pScriptCurrentFielder)),
                fInvincible = (fInvincible <= fOnMushrooms) ? fInvincible : fOnMushrooms,
                fFalseConfidence = (fFalseConfidence <= fInvincible) ? fFalseConfidence : fInvincible));
        }
    }

    bestValue.Confidence = fBestConfidence;

    return bestValue;
}

static inline uintptr_t OffensiveQuestionHash(
    uintptr_t functionAddress,
    FuzzyVariant& argument)
{
    return functionAddress + ((Variant*)&argument)->GetHash();
}

static inline float OffensiveHalfScore(const FuzzyVariant& value, float fLosing)
{
    float fGoodToShoot = OffensiveDanger(value);
    float fHalf = 0.5f;
    float fWeightedGoodToShoot = fGoodToShoot * fHalf;
    return fLosing * fHalf + fWeightedGoodToShoot;
}

static inline void OffensiveBranchValues(
    float fTrueConfidence, float& fBranchRatio, float& fFalseConfidence)
{
    fFalseConfidence = 1.0f - fTrueConfidence;
    float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    fBranchRatio = fMinVal / fMaxVal;
}

static inline float max_float(float a, float b)
{
    if (a >= b)
    {
        return a;
    }
    return b;
}

static inline float WeightedScore2(float fScoreA, float fWeightA, float fScoreB, float fWeightB)
{
    return fScoreA * fWeightA + fScoreB * fWeightB;
}

static inline float WeightedScore3(float fScoreA, float fWeightA, float fScoreB, float fWeightB, float fScoreC, float fWeightC)
{
    return fScoreA * fWeightA + fScoreB * fWeightB + fScoreC * fWeightC;
}

/**
 * Offset/Address/Size: 0x3128 | 0x8008FBB4 | size: 0x1368
 */
FuzzyVariant Fuzzy::InGoodWindupPosition(cFielder* TheFielder)
{
    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    FuzzyVariant fvFielder((cPlayer*)TheFielder);
    union FunctionAddress
    {
        FuzzyVariant (*function)(cFielder*);
        // PORT: pointer-width.
        uintptr_t address;
    } functionAddress;
    functionAddress.function = InGoodWindupPosition;
    uintptr_t hash = OffensiveQuestionHash(functionAddress.address, fvFielder);
    FuzzyVariant fvFielder2((cPlayer*)TheFielder);

    if (ScriptQuestionCache::Instance()->Lookup(hash, bestValue, NULL))
    {
        bestValue.Confidence = bestValue.Confidence;
        ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
        return bestValue;
    }

    float fTrueConfidence = OffensiveOr(
        FGREATER(FarToTheirNet(g_pScriptCurrentFielder), 0.85f),
        FLESS(InOffensiveZone(g_pScriptCurrentFielder), 0.4f));
    float fFalseConfidence;
    float fBranchRatio;
    OffensiveBranchValues(fTrueConfidence, fBranchRatio, fFalseConfidence);

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            fConfidence = fConfidence * fBranchRatio;

        if (fConfidence > 0.0f)
        {
            fBestConfidence = fConfidence;
            bestValue = FuzzyVariant(0.0f);
        }
    }

    if (fFalseConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
        if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
            fConfidence = fConfidence * fBranchRatio;

        float fTrueConfidence = Invincible(g_pScriptCurrentFielder);
        float fFalseConfidence = 1.0f - fTrueConfidence;
        float fBranchRatio;
        float fMinVal = OffensiveAnd(fTrueConfidence, fFalseConfidence);
        float fMaxVal = OffensiveOr(fTrueConfidence, fFalseConfidence);
        fBranchRatio = fMinVal / fMaxVal;

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM2(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                fConfidence = fConfidence * fBranchRatio;

            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                bestValue = FuzzyVariant(FGREATER(InOffensiveZone(g_pScriptCurrentFielder), 0.0f));
            }
        }

        if (fFalseConfidence > 0.0f)
        {
            SaveConfidence PushDOM2(&fConfidence);

            fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
            if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
                fConfidence = fConfidence * fBranchRatio;

            float fOpen = OffensiveAnd(
                InOffensiveZone(g_pScriptCurrentFielder),
                FGREATER(WideOpen(g_pScriptCurrentFielder), 0.4f));

            float fNotInDanger = OffensiveNot(OffensiveDanger(InDanger(g_pScriptCurrentFielder)));
            float fFarToTheirNet = FarToTheirNet(g_pScriptCurrentFielder);
            float fNotFarToTheirNet = OffensiveAnd(1.0f - fFarToTheirNet, fNotInDanger);

            float fLosing = OffensiveAnd(TimeNearlyOver(g_pGame), Losing(g_pScriptCurrentTeam));

            float fInFront = FLESS(InFrontOfTheirNet(g_pScriptCurrentFielder), 0.4f);
            float fNotInFront = 1.0f - fInFront;
            float fInFrontMin = OffensiveAnd(fInFront, fNotInFront);
            float fInFrontMax = OffensiveOr(fInFront, fNotInFront);
            float fInFrontBranchRatio = fInFrontMin / fInFrontMax;

            if (fInFront > 0.0f)
            {
                SaveConfidence PushDOM4(&fConfidence);

                fConfidence = (fConfidence <= fInFront) ? fConfidence : fInFront;
                if (fConfidence < fInFront && fInFront < 0.5f)
                    fConfidence = fConfidence * fInFrontBranchRatio;

                float fLosingFalse = 1.0f - fLosing;
                float fLosingMin = OffensiveAnd(fLosing, fLosingFalse);
                float fLosingMax = OffensiveOr(fLosing, fLosingFalse);
                float fLosingBranchRatio = fLosingMin / fLosingMax;

                if (fLosing > 0.0f)
                {
                    SaveConfidence PushDOM5(&fConfidence);

                    fConfidence = (fConfidence <= fLosing) ? fConfidence : fLosing;
                    if (fConfidence < fLosing && fLosing < 0.5f)
                        fConfidence = fConfidence * fLosingBranchRatio;

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(OffensiveHalfScore(
                            GoodToShoot(g_pScriptCurrentFielder), fLosing));
                    }
                }

                if (fLosingFalse > 0.0f)
                {
                    SaveConfidence PushDOM5(&fConfidence);

                    fConfidence = (fConfidence <= fLosingFalse) ? fConfidence : fLosingFalse;
                    if (fConfidence < fLosingFalse && fLosingFalse < 0.5f)
                        fConfidence = fConfidence * fLosingBranchRatio;

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = GoodToShoot(g_pScriptCurrentFielder);
                    }
                }
            }

            if (fNotInFront > 0.0f)
            {
                SaveConfidence PushDOM4(&fConfidence);

                fConfidence = (fConfidence <= fNotInFront) ? fConfidence : fNotInFront;
                if (fConfidence < fNotInFront && fNotInFront < 0.5f)
                    fConfidence = fConfidence * fInFrontBranchRatio;

                float fTrueConfidence = LikelyToScore(g_pScriptCurrentFielder);
                float fFalseConfidence = OffensiveNot(fTrueConfidence);
                float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMinVal / fMaxVal;

                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM5(&fConfidence);

                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(fConfidence);
                    }
                }

                {
                    float fFalseConfidence = 1.0f - fLosing;
                    float fMinVal = (fLosing <= fFalseConfidence) ? fLosing : fFalseConfidence;
                    float fMaxVal = (fLosing >= fFalseConfidence) ? fLosing : fFalseConfidence;
                    float fBranchRatio = fMinVal / fMaxVal;

                    if (fLosing > 0.0f)
                    {
                        SaveConfidence PushDOM5(&fConfidence);

                        fConfidence = (fConfidence <= fLosing) ? fConfidence : fLosing;
                        if (fConfidence < fLosing && fLosing < 0.5f)
                            fConfidence = fConfidence * fBranchRatio;

                        if (fConfidence > fBestConfidence)
                        {
                            fBestConfidence = fConfidence;
                            bestValue = FuzzyVariant(WeightedScore3(
                                fLosing, 0.25f, InFrontOfTheirNet(g_pScriptCurrentFielder), 0.2f, max_float(fOpen, max_float(fNotFarToTheirNet, OffensiveDanger(GoodToShoot(g_pScriptCurrentFielder)))), 0.55f));
                        }
                    }

                    if (fFalseConfidence > 0.0f)
                    {
                        SaveConfidence PushDOM5(&fConfidence);

                        fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
                        if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
                            fConfidence = fConfidence * fBranchRatio;

                        if (fConfidence > fBestConfidence)
                        {
                            fBestConfidence = fConfidence;
                            bestValue = FuzzyVariant(WeightedScore2(
                                InFrontOfTheirNet(g_pScriptCurrentFielder), 0.3f, max_float(fOpen, max_float(fNotFarToTheirNet, OffensiveDanger(GoodToShoot(g_pScriptCurrentFielder)))), 0.7f));
                        }
                    }
                }
            }
        }
    }

    bestValue.Confidence = fBestConfidence;

    ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);

    return bestValue;
}

/**
 * Offset/Address/Size: 0x2B2C | 0x8008F5B8 | size: 0x5FC
 */
FuzzyVariant Fuzzy::CutAndBreak(cFielder* TheFielder)
{
    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    FuzzyVariant fvFielder((cPlayer*)TheFielder);
    ((Variant*)&fvFielder)->GetHash();
    FuzzyVariant fvFielder2((cPlayer*)TheFielder);

    float fTrueConfidence = InOffensiveZone(TheFielder);
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

        SSearchCutAndBreak ssearch = SSearchCutAndBreak(TheFielder);
        nlVector3 v3Position;
        nlVector3& (*PositionOfFielder)(cFielder*) = PositionOf<cFielder*>;
        float fCutAndBreakScore = ssearch.FindBestPosition(v3Position, PositionOfFielder(TheFielder), DIR_NONE, NULL, 8.0f, 0x8000);

        if (fConfidence > 0.0f)
        {
            fBestConfidence = fConfidence;
            bestValue = FuzzyVariant(fCutAndBreakScore);
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
            bestValue = FuzzyVariant(0.0f);
        }
    }

    bestValue.Confidence = fBestConfidence;

    return bestValue;
}

/**
 * Offset/Address/Size: 0x22A0 | 0x8008ED2C | size: 0x88C
 */
FuzzyVariant Fuzzy::DoShooting(float fConfidence, cDecisionEntity* pDecision)
{

    float fBestConfidence = 0.0f;

    float fTrueConfidence = InGoodWindupPosition(g_pScriptCurrentFielder).mData.f;
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

        float fCurrentConfidence = fConfidence;
        if (0.0f >= fCurrentConfidence)
            fBestConfidence = 0.0f;
        else
            fBestConfidence = fCurrentConfidence;

        pDecision->QueueActionSetDesire(20, fConfidence, -1.0f, fvNotSet, fvNotSet);

        SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
        pDecision->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(pTweaks->Off_ShootingChance, Shooter(g_pScriptCurrentFielder));
    }

    {
        fTrueConfidence = OffensiveDanger(GoodToChipShot(g_pScriptCurrentFielder));
        float fFarToMyNet = FarToMyNet(g_pScriptCurrentFielder);
        fTrueConfidence = (fFarToMyNet <= fTrueConfidence) ? fFarToMyNet : fTrueConfidence;
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

            if (fBestConfidence >= fConfidence)
                fBestConfidence = fBestConfidence;
            else
                fBestConfidence = fConfidence;

            pDecision->QueueActionSetDesire(14, fConfidence, 0.0f, FuzzyVariant(false), FuzzyVariant(true));

            SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            pDecision->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(pTweaks->Off_ChipShotChance, Shooter(g_pScriptCurrentFielder));
        }
    }

    {
        fTrueConfidence = OffensiveAnd(TimeNearlyOver(g_pGame), Winning(g_pScriptCurrentTeam));

        float fStunnedGoalie = OffensiveOr(
            FGREATER(FurthestBackOnMyTeam(g_pScriptCurrentFielder).mData.f, 0.5f),
            OffensiveAnd(
                CloseToMyNet(g_pScriptCurrentFielder),
                Stunned(g_pScriptCurrentTeam->GetGoalie()),
                InDanger(g_pScriptCurrentFielder).mData.f),
            fTrueConfidence);

        fTrueConfidence = OffensiveAnd(InDefensiveZone(g_pScriptCurrentFielder), fStunnedGoalie);

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

            if (fBestConfidence >= fConfidence)
                fBestConfidence = fBestConfidence;
            else
                fBestConfidence = fConfidence;

            pDecision->QueueActionSetDesire(14, fConfidence, -1.0f, fvNotSet, fvNotSet);
            pDecision->m_pLastQueuedAction->m_fSelectionChance = 0.3f;
        }
    }

    return FuzzyVariant(fBestConfidence);
}

/**
 * Offset/Address/Size: 0x1EF8 | 0x8008E984 | size: 0x3A8
 */
FuzzyVariant Fuzzy::FurthestBackOnMyTeam(cFielder* TheFielder)
{
    FuzzyVariant bestValue;
    float fFarTo;
    float fTotalUpfieldScore;

    FuzzyVariant fvFielder((cPlayer*)TheFielder);
    ((Variant*)&fvFielder)->GetHash();
    FuzzyVariant fvFielder2((cPlayer*)TheFielder);

    fTotalUpfieldScore = 0.0f;

    for (int i = 0; i < 4; i++)
    {
        cFielder* TeamMate = TheFielder->m_pTeam->GetFielder(i);

        if (TeamMate != TheFielder)
        {
            float fUpfield;

            fFarTo = FarTo((cPlayer*)TeamMate, (cPlayer*)TheFielder);
            fUpfield = UpfieldFrom((cPlayer*)TeamMate, (cPlayer*)TheFielder);

            fUpfield = (fUpfield <= fFarTo) ? fUpfield : fFarTo;
            fTotalUpfieldScore += fUpfield;
        }
    }

    fTotalUpfieldScore = fTotalUpfieldScore / 3.0f;

    FuzzyVariant fvResult(fTotalUpfieldScore);
    bestValue = fvResult;
    bestValue.Confidence = 1.0f;

    return bestValue;
}

/**
 * Offset/Address/Size: 0x560 | 0x8008CFEC | size: 0x1998
 */
FuzzyVariant Fuzzy::UsePowerupOffensive(float fConfidence, cDecisionEntity* pDecision)
{

    float fBestConfidence = 0.0f;
    FuzzyVariant theTarget = GetPowerupTargetOffensive(g_pScriptCurrentTeam);

    float fPressured = Pressured(g_pScriptCurrentFielder);
    float fCaptain = Captain(g_pScriptCurrentFielder);

    float fSmallWeight = 0.2f;
    float fTargetWeight = 0.6f;
    float fTrueConfidence = theTarget.Confidence * fTargetWeight + fCaptain * fSmallWeight + fPressured * fSmallWeight;
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

        {
            float fLikely = LikelyToUsePowerup(g_pScriptCurrentFielder, 0);
            float fLFalse = 1.0f - fLikely;
            float fLMin = (fLikely <= fLFalse) ? fLikely : fLFalse;
            float fLMax = (fLikely >= fLFalse) ? fLikely : fLFalse;
            float fLRatio = fLMin / fLMax;

            if (fLikely > 0.0f)
            {
                SaveConfidence PushDOM2(&fConfidence);
                fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;

                if (fConfidence < fLikely && fLikely < 0.5f)
                    fConfidence = fConfidence * fLRatio;

                float fCurrentConfidence = fConfidence;
                if (0.0f >= fCurrentConfidence)
                    fBestConfidence = 0.0f;
                else
                    fBestConfidence = fCurrentConfidence;
                pDecision->QueueActionSetDesire(18, fConfidence, 0.0f, FuzzyVariant(0), theTarget);
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
                SaveConfidence PushDOM(&fConfidence);
                fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;

                if (fConfidence < fLikely && fLikely < 0.5f)
                    fConfidence = fConfidence * fLRatio;

                if (fBestConfidence >= fConfidence)
                    fBestConfidence = fBestConfidence;
                else
                    fBestConfidence = fConfidence;
                pDecision->QueueActionSetDesire(18, fConfidence, 0.0f, FuzzyVariant(1), theTarget);
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
                SaveConfidence PushDOM(&fConfidence);
                fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;

                if (fConfidence < fLikely && fLikely < 0.5f)
                    fConfidence = fConfidence * fLRatio;

                if (fBestConfidence >= fConfidence)
                    fBestConfidence = fBestConfidence;
                else
                    fBestConfidence = fConfidence;
                pDecision->QueueActionSetDesire(18, fConfidence, 0.0f, FuzzyVariant(2), theTarget);
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
                SaveConfidence PushDOM(&fConfidence);
                fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;

                if (fConfidence < fLikely && fLikely < 0.5f)
                    fConfidence = fConfidence * fLRatio;

                if (fBestConfidence >= fConfidence)
                    fBestConfidence = fBestConfidence;
                else
                    fBestConfidence = fConfidence;
                pDecision->QueueActionSetDesire(18, fConfidence, 0.0f, FuzzyVariant(3), theTarget);
            }
        }

        {
            float fClosing = ClosingTo(g_pScriptCurrentFielder, theTarget.mData.pPlayer);
            float fNear = NearTo(g_pScriptCurrentFielder, theTarget.mData.pPlayer);
            float fTrueConfidence = (fNear + fClosing) * 0.5f;
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

                {
                    float fLikely = LikelyToUsePowerup(g_pScriptCurrentFielder, 4);
                    float fLFalse = 1.0f - fLikely;
                    float fLMin = (fLikely <= fLFalse) ? fLikely : fLFalse;
                    float fLMax = (fLikely >= fLFalse) ? fLikely : fLFalse;
                    float fLRatio = fLMin / fLMax;

                    if (fLikely > 0.0f)
                    {
                        SaveConfidence PushDOM2(&fConfidence);
                        fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;

                        if (fConfidence < fLikely && fLikely < 0.5f)
                            fConfidence = fConfidence * fLRatio;

                        if (fBestConfidence >= fConfidence)
                            fBestConfidence = fBestConfidence;
                        else
                            fBestConfidence = fConfidence;
                        pDecision->QueueActionSetDesire(18, fConfidence, 0.0f, FuzzyVariant(4), theTarget);
                    }
                }
            }
        }

        {
            float fTrueConfidence = FGREATER(InGoodWindupPosition(g_pScriptCurrentFielder).mData.f, 0.3f);
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

                {
                    float fLikely = LikelyToUsePowerup(g_pScriptCurrentFielder, 5);
                    float fLFalse = 1.0f - fLikely;
                    float fLMin = (fLikely <= fLFalse) ? fLikely : fLFalse;
                    float fLMax = (fLikely >= fLFalse) ? fLikely : fLFalse;
                    float fLRatio = fLMin / fLMax;

                    if (fLikely > 0.0f)
                    {
                        SaveConfidence PushDOM2(&fConfidence);
                        fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;

                        if (fConfidence < fLikely && fLikely < 0.5f)
                            fConfidence = fConfidence * fLRatio;

                        if (fBestConfidence >= fConfidence)
                            fBestConfidence = fBestConfidence;
                        else
                            fBestConfidence = fConfidence;
                        pDecision->QueueActionSetDesire(18, fConfidence, 0.0f, FuzzyVariant(5), theTarget);
                    }
                }
            }
        }
    }

    {
        float fCloseSideline = CloseToSideline(g_pScriptCurrentFielder);
        float fFacingSideline = FacingSideline(g_pScriptCurrentFielder);
        fTrueConfidence = 1.0f - ((fFacingSideline <= fCloseSideline) ? fFacingSideline : fCloseSideline);
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

            fTrueConfidence = 1.0f - OnMushrooms(g_pScriptCurrentFielder);
            float fFalseConfidence = 1.0f - fTrueConfidence;
            float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fBranchRatio = fMin / fMax;

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM2(&fConfidence);
                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    fConfidence = fConfidence * fBranchRatio;

                float fOpen = Open(g_pScriptCurrentFielder);
                float fOpenToNet = OpenToTheirNet(g_pScriptCurrentFielder);
                const FuzzyVariant& goodBallCarrier = GoodBallCarrier(g_pScriptCurrentFielder);
                float fGoodBallCarrier = goodBallCarrier.mData.f;

                float fOpenWeight = 0.15f;
                float fCarrierWeight = 0.55f;
                float fNetWeight = 0.3f;
                fOpen = fOpen * fOpenWeight + (fGoodBallCarrier * fCarrierWeight + fOpenToNet * fNetWeight);
                fTrueConfidence = OnBreakaway(g_pScriptCurrentFielder);
                fTrueConfidence = (fTrueConfidence >= fOpen) ? fTrueConfidence : fOpen;

                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMin / fMax;

                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM3(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;

                    {
                        float fLikely = LikelyToUsePowerup(g_pScriptCurrentFielder, 7);
                        float fLFalse = 1.0f - fLikely;
                        float fLMin = (fLikely <= fLFalse) ? fLikely : fLFalse;
                        float fLMax = (fLikely >= fLFalse) ? fLikely : fLFalse;
                        float fLRatio = fLMin / fLMax;

                        if (fLikely > 0.0f)
                        {
                            SaveConfidence PushDOM4(&fConfidence);
                            fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;

                            if (fConfidence < fLikely && fLikely < 0.5f)
                                fConfidence = fConfidence * fLRatio;

                            if (fBestConfidence >= fConfidence)
                                fBestConfidence = fBestConfidence;
                            else
                                fBestConfidence = fConfidence;
                            pDecision->QueueActionSetDesire(18, fConfidence, 0.0f, FuzzyVariant(7), fvNotSet);
                        }
                    }
                }
            }
        }
    }

    {
        fTrueConfidence = Captain(g_pScriptCurrentFielder);
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

            fTrueConfidence = FGREATER(InGoodWindupPosition(g_pScriptCurrentFielder).mData.f, 0.0f);
            float fWideOpen = WideOpen(g_pScriptCurrentFielder);
            float fShotConfidence = (1.0f - fWideOpen >= fTrueConfidence) ? (1.0f - fWideOpen) : fTrueConfidence;

            float fFalseConfidence = 1.0f - fShotConfidence;
            float fMin = (fShotConfidence <= fFalseConfidence) ? fShotConfidence : fFalseConfidence;
            float fMax = (fShotConfidence >= fFalseConfidence) ? fShotConfidence : fFalseConfidence;
            float fBranchRatio = fMin / fMax;

            if (fShotConfidence > 0.0f)
            {
                SaveConfidence PushDOM2(&fConfidence);
                fConfidence = (fConfidence <= fShotConfidence) ? fConfidence : fShotConfidence;

                if (fConfidence < fShotConfidence && fShotConfidence < 0.5f)
                    fConfidence = fConfidence * fBranchRatio;

                {
                    float fLikely = LikelyToUsePowerup(g_pScriptCurrentFielder, 8);
                    float fLFalse = 1.0f - fLikely;
                    float fLMin = (fLikely <= fLFalse) ? fLikely : fLFalse;
                    float fLMax = (fLikely >= fLFalse) ? fLikely : fLFalse;
                    float fLRatio = fLMin / fLMax;

                    if (fLikely > 0.0f)
                    {
                        SaveConfidence PushDOM3(&fConfidence);
                        fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;

                        if (fConfidence < fLikely && fLikely < 0.5f)
                            fConfidence = fConfidence * fLRatio;

                        if (fBestConfidence >= fConfidence)
                            fBestConfidence = fBestConfidence;
                        else
                            fBestConfidence = fConfidence;
                        pDecision->QueueActionSetDesire(18, fConfidence, 0.0f, FuzzyVariant(8), fvNotSet);
                    }
                }
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
            SaveConfidence PushDOM(&fConfidence);
            fConfidence = (fConfidence <= fLikely) ? fConfidence : fLikely;

            if (fConfidence < fLikely && fLikely < 0.5f)
                fConfidence = fConfidence * fLRatio;

            if (fBestConfidence >= fConfidence)
                fBestConfidence = fBestConfidence;
            else
                fBestConfidence = fConfidence;
            pDecision->QueueActionSetDesire(18, fConfidence, 0.0f, FuzzyVariant(6), fvNotSet);
        }
    }

    return FuzzyVariant(fBestConfidence);
}

/**
 * Offset/Address/Size: 0x0 | 0x8008CA8C | size: 0x560
 */
FuzzyVariant Fuzzy::GetPowerupTargetOffensive(cTeam* TheTeam)
{

    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;
    FuzzyVariant fvTeam(TheTeam);
    ((Variant*)&fvTeam)->GetHash();
    FuzzyVariant fvTeam2(TheTeam);
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
            float fMarking = Marking(theOpponent, (cPlayer*)g_pScriptCurrentFielder);
            float fDownfield = DownfieldFrom((cPlayer*)g_pScriptCurrentFielder, (cPlayer*)theOpponent);
            float fClosing = ClosingTo((cPlayer*)g_pScriptCurrentFielder, (cPlayer*)theOpponent);
            float fFar = FarTo((cPlayer*)g_pScriptCurrentFielder, (cPlayer*)theOpponent);
            float fLowWeight = 0.2f;
            float fHighWeight = 0.3f;
            float fMarkingConfidence = fMarking * fHighWeight + (fDownfield * fHighWeight + ((1.0f - fFar) * fLowWeight + fClosing * fLowWeight));
            float fNotMarkingConfidence = 1.0f - fMarkingConfidence;
            float fMarkingMin = (fMarkingConfidence <= fNotMarkingConfidence) ? fMarkingConfidence : fNotMarkingConfidence;
            float fMarkingMax = (fMarkingConfidence >= fNotMarkingConfidence) ? fMarkingConfidence : fNotMarkingConfidence;
            float fMarkingBranchRatio = fMarkingMin / fMarkingMax;
            if (fMarkingConfidence > 0.0f)
            {
                SaveConfidence PushDOM2(&fConfidence);
                fConfidence = (fConfidence <= fMarkingConfidence) ? fConfidence : fMarkingConfidence;
                if (fConfidence < fMarkingConfidence && fMarkingConfidence < 0.5f)
                    fConfidence = fConfidence * fMarkingBranchRatio;
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
