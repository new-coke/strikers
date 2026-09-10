#include "Game/AI/Scripts/Plays/DefaultLoose.h"
#include "Game/AI/Fuzzy.h"
#include "Game/AI/ScriptAction.h"
#include "Game/AI/Scripts/ScriptCaching.h"
#include "Game/AI/Scripts/ScriptQuestions.h"
#include "Game/GameTweaks.h"

extern cFielder* g_pScriptCurrentFielder;
extern cFielder* g_pScriptCurrentMark;
extern cTeam* g_pScriptCurrentTeam;
extern cTeam* g_pScriptOtherTeam;
extern cTeam* g_pCurrentlyUpdatingTeam;
extern cBall* g_pScriptBall;
extern FuzzyVariant fvNotSet;

extern float Loose(cTeam*);

static inline const FuzzyVariant& KeepFirst(const FuzzyVariant& value, float)
{
    return value;
}

static inline float NotGoaliePickup(const FuzzyVariant& value)
{
    return 1.0f - value.mData.f;
}

/**
 * Offset/Address/Size: 0x15CC | 0x8008C650 | size: 0x43C
 */
FuzzyVariant Fuzzy::AbortLoosePlay(cDecisionEntity* pDecision)
{
    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    float fTrueConfidence = Loose(g_pScriptCurrentTeam);
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
 * Offset/Address/Size: 0x0 | 0x8008B084 | size: 0x15CC
 */
FuzzyVariant Fuzzy::DefaultLoosePlay(cDecisionEntity* pDecision)
{
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    float fTrueConfidence = ChasingBall((cPlayer*)g_pScriptCurrentFielder);
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

        fTrueConfidence = GonnaGetBall(g_pScriptCurrentTeam);
        fFalseConfidence = 1.0f - fTrueConfidence;
        float fNotFalse = 1.0f - fFalseConfidence;
        fMin = (fFalseConfidence <= fNotFalse) ? fFalseConfidence : fNotFalse;
        fMax = (fFalseConfidence >= fNotFalse) ? fFalseConfidence : fNotFalse;
        fBranchRatio = fMin / fMax;

        if (fFalseConfidence > 0.0f)
        {
            SaveConfidence PushDOM2(&fConfidence);
            fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
            if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
                fConfidence = fConfidence * fBranchRatio;

            for (int i = 0; i < 4; i++)
            {
                cFielder* theOpponent = g_pScriptOtherTeam->GetFielder(i);

                fTrueConfidence = 1.0f - RepeatingLastDesire(g_pScriptCurrentFielder, edHeavyAttack);
                fFalseConfidence = ChasingBall((cPlayer*)theOpponent);
                float fIdealTackle = AtIdealDistanceForTackling((cPlayer*)g_pScriptCurrentFielder, (cPlayer*)theOpponent);

                fFalseConfidence = (fFalseConfidence <= fTrueConfidence) ? fFalseConfidence : fTrueConfidence;
                fTrueConfidence = (fIdealTackle <= fFalseConfidence) ? fIdealTackle : fFalseConfidence;
                float fFalseConfidence = 1.0f - fTrueConfidence;

                fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                fBranchRatio = fMin / fMax;

                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM3(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;

                    if (fBestConfidence >= fConfidence)
                        fBestConfidence = fBestConfidence;
                    else
                        fBestConfidence = fConfidence;

                    pDecision->QueueActionSetDesire(5, fConfidence, 0.0f, FuzzyVariant((cPlayer*)theOpponent), fvNotSet);
                    SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                    pDecision->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(pTweaks->Loose_HeavyAttackChance, Aggressive(g_pScriptCurrentFielder));
                }
            }
        }
    }

    FuzzyVariant bestBallInterceptor = GetBestBallInterceptor(g_pScriptCurrentTeam);
    if (bestBallInterceptor.mData.u == (uintptr_t)g_pScriptCurrentFielder)
        fTrueConfidence = 1.0f;
    else
        fTrueConfidence = 0.0f;
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

        {
            float fNotOtherGoaliePickup;
            fTrueConfidence = 1.0f - KeepFirst(GoalieAndGonnaPickupBall((cPlayer*)(g_pScriptCurrentFielder ? (g_pScriptCurrentFielder ? g_pScriptCurrentFielder->m_pTeam : (cTeam*)0)->GetGoalie() : (Goalie*)0)), fNotOtherGoaliePickup = NotGoaliePickup(GoalieAndGonnaPickupBall((cPlayer*)(g_pScriptCurrentFielder ? (g_pScriptCurrentFielder ? g_pScriptCurrentFielder->m_pTeam->GetOtherTeam() : (cTeam*)0)->GetGoalie() : (Goalie*)0)))).mData.f;

            fTrueConfidence = (fTrueConfidence <= fNotOtherGoaliePickup) ? fTrueConfidence : fNotOtherGoaliePickup;
            float fFalseConfidence = 1.0f - fTrueConfidence;
            float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fBranchRatio = fMin / fMax;

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM4(&fConfidence);
                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    fConfidence = fConfidence * fBranchRatio;

                if (fBestConfidence >= fConfidence)
                    fBestConfidence = fBestConfidence;
                else
                    fBestConfidence = fConfidence;

                pDecision->QueueActionSetDesire(6, fConfidence, -1.0f, fvNotSet, fvNotSet);
            }

            if (fFalseConfidence > 0.0f)
            {
                SaveConfidence PushDOM5(&fConfidence);
                fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
                if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
                    fConfidence = fConfidence * fBranchRatio;

                if (fBestConfidence >= fConfidence)
                    fBestConfidence = fBestConfidence;
                else
                    fBestConfidence = fConfidence;

                pDecision->QueueActionSetDesire(10, fConfidence, -1.0f, fvNotSet, fvNotSet);
            }
        }
    }

    if (fFalseConfidence > 0.0f)
    {
        SaveConfidence PushDOM5(&fConfidence);
        fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
        if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
            fConfidence = fConfidence * fBranchRatio;

        float fWinger = Winger(g_pScriptCurrentFielder);
        float fNotWinger = 1.0f - fWinger;
        float fMin3 = (fWinger <= fNotWinger) ? fWinger : fNotWinger;
        float fMax3 = (fWinger >= fNotWinger) ? fWinger : fNotWinger;
        float fBranchRatio3 = fMin3 / fMax3;

        if (fWinger > 0.0f)
        {
            SaveConfidence PushDOM6(&fConfidence);
            fConfidence = (fConfidence <= fWinger) ? fConfidence : fWinger;
            if (fConfidence < fWinger && fWinger < 0.5f)
                fConfidence = fConfidence * fBranchRatio3;

            fTrueConfidence = 1.0f - GonnaGetBall(g_pScriptCurrentTeam);
            fFalseConfidence = 1.0f - InOffensiveZoneOfPlayer(g_pScriptBall, (cPlayer*)g_pScriptCurrentFielder);
            fFalseConfidence = (fFalseConfidence <= fTrueConfidence) ? fFalseConfidence : fTrueConfidence;
            fTrueConfidence = InDefensiveZoneOfPlayer(g_pScriptBall, (cPlayer*)g_pScriptCurrentFielder);
            fTrueConfidence = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            fFalseConfidence = 1.0f - fTrueConfidence;
            fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            fBranchRatio = fMin / fMax;

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM7(&fConfidence);
                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    fConfidence = fConfidence * fBranchRatio;

                if (fBestConfidence >= fConfidence)
                    fBestConfidence = fBestConfidence;
                else
                    fBestConfidence = fConfidence;
                pDecision->QueueActionSetDesire(7, fConfidence, -1.0f, fvNotSet, fvNotSet);
            }

            if (fFalseConfidence > 0.0f)
            {
                SaveConfidence PushDOM8(&fConfidence);
                fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
                if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
                    fConfidence = fConfidence * fBranchRatio;

                float fInDefensive = InDefensiveZone((cPlayer*)g_pScriptCurrentFielder);
                float fNotDef = 1.0f - fInDefensive;
                float fMin5 = (fInDefensive <= fNotDef) ? fInDefensive : fNotDef;
                float fMax5 = (fInDefensive >= fNotDef) ? fInDefensive : fNotDef;
                float fBranchRatio5 = fMin5 / fMax5;

                if (fInDefensive > 0.0f)
                {
                    SaveConfidence PushDOM9(&fConfidence);
                    fConfidence = (fConfidence <= fInDefensive) ? fConfidence : fInDefensive;
                    if (fConfidence < fInDefensive && fInDefensive < 0.5f)
                        fConfidence = fConfidence * fBranchRatio5;
                    if (fBestConfidence >= fConfidence)
                        fBestConfidence = fBestConfidence;
                    else
                        fBestConfidence = fConfidence;
                    pDecision->QueueActionSetDesire(10, fConfidence, -1.0f, fvNotSet, fvNotSet);
                }

                if (fNotDef > 0.0f)
                {
                    SaveConfidence PushDOM10(&fConfidence);
                    fConfidence = (fConfidence <= fNotDef) ? fConfidence : fNotDef;
                    if (fConfidence < fNotDef && fNotDef < 0.5f)
                        fConfidence = fConfidence * fBranchRatio5;

                    float fInOffensive = InOffensiveZone((cPlayer*)g_pScriptCurrentFielder);
                    float fNotOffensive = 1.0f - fInOffensive;
                    float fMin6 = (fInOffensive <= fNotOffensive) ? fInOffensive : fNotOffensive;
                    float fMax6 = (fInOffensive >= fNotOffensive) ? fInOffensive : fNotOffensive;
                    float fBranchRatio6 = fMin6 / fMax6;

                    if (fInOffensive > 0.0f)
                    {
                        SaveConfidence PushDOM11(&fConfidence);
                        fConfidence = (fConfidence <= fInOffensive) ? fConfidence : fInOffensive;
                        if (fConfidence < fInOffensive && fInOffensive < 0.5f)
                            fConfidence = fConfidence * fBranchRatio6;
                        if (fBestConfidence >= fConfidence)
                            fBestConfidence = fBestConfidence;
                        else
                            fBestConfidence = fConfidence;
                        pDecision->QueueActionSetDesire(1, fConfidence, -1.0f, fvNotSet, fvNotSet);
                        SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                        pDecision->m_pLastQueuedAction->m_fSelectionChance = pTweaks->Off_CutAndBreakChance;
                    }

                    if (fNotOffensive > 0.0f)
                    {
                        SaveConfidence PushDOM12(&fConfidence);
                        fConfidence = (fConfidence <= fNotOffensive) ? fConfidence : fNotOffensive;
                        if (fConfidence < fNotOffensive && fNotOffensive < 0.5f)
                            fConfidence = fConfidence * fBranchRatio6;
                        if (fBestConfidence >= fConfidence)
                            fBestConfidence = fBestConfidence;
                        else
                            fBestConfidence = fConfidence;
                        pDecision->QueueActionSetDesire(4, fConfidence, -1.0f, fvNotSet, fvNotSet);
                    }
                }
            }
        }

        float fRoleMid = Midfield(g_pScriptCurrentFielder);
        float fRoleDef = Defence(g_pScriptCurrentFielder);
        float fRole = (fRoleDef >= fRoleMid) ? fRoleDef : fRoleMid;
        float fNotRole = 1.0f - fRole;
        float fMin7 = (fRole <= fNotRole) ? fRole : fNotRole;
        float fMax7 = (fRole >= fNotRole) ? fRole : fNotRole;
        float fBranchRatio7 = fMin7 / fMax7;

        if (fRole > 0.0f)
        {
            SaveConfidence PushDOM13(&fConfidence);
            fConfidence = (fConfidence <= fRole) ? fConfidence : fRole;
            if (fConfidence < fRole && fRole < 0.5f)
                fConfidence = fConfidence * fBranchRatio7;

            fTrueConfidence = 1.0f - GonnaGetBall(g_pScriptCurrentTeam);
            fFalseConfidence = 1.0f - InOffensiveZoneOfPlayer(g_pScriptBall, (cPlayer*)g_pScriptCurrentFielder);
            fFalseConfidence = (fFalseConfidence <= fTrueConfidence) ? fFalseConfidence : fTrueConfidence;
            fTrueConfidence = InDefensiveZoneOfPlayer(g_pScriptBall, (cPlayer*)g_pScriptCurrentFielder);
            fTrueConfidence = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            fFalseConfidence = 1.0f - fTrueConfidence;
            fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            fBranchRatio = fMin / fMax;

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM14(&fConfidence);
                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    fConfidence = fConfidence * fBranchRatio;

                if (fBestConfidence >= fConfidence)
                    fBestConfidence = fBestConfidence;
                else
                    fBestConfidence = fConfidence;
                pDecision->QueueActionSetDesire(7, fConfidence, -1.0f, fvNotSet, fvNotSet);
            }

            if (fFalseConfidence > 0.0f)
            {
                SaveConfidence PushDOM15(&fConfidence);
                fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
                if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
                    fConfidence = fConfidence * fBranchRatio;

                float fNearMyNet = NearToMyNet((cPlayer*)g_pScriptCurrentFielder);
                float fNotNearMyNet = 1.0f - fNearMyNet;
                float fMin9 = (fNearMyNet <= fNotNearMyNet) ? fNearMyNet : fNotNearMyNet;
                float fMax9 = (fNearMyNet >= fNotNearMyNet) ? fNearMyNet : fNotNearMyNet;
                float fBranchRatio9 = fMin9 / fMax9;

                if (fNearMyNet > 0.0f)
                {
                    SaveConfidence PushDOM16(&fConfidence);
                    fConfidence = (fConfidence <= fNearMyNet) ? fConfidence : fNearMyNet;
                    if (fConfidence < fNearMyNet && fNearMyNet < 0.5f)
                        fConfidence = fConfidence * fBranchRatio9;
                    if (fBestConfidence >= fConfidence)
                        fBestConfidence = fBestConfidence;
                    else
                        fBestConfidence = fConfidence;
                    pDecision->QueueActionSetDesire(10, fConfidence, -1.0f, fvNotSet, fvNotSet);
                }

                if (fNotNearMyNet > 0.0f)
                {
                    SaveConfidence PushDOM17(&fConfidence);
                    fConfidence = (fConfidence <= fNotNearMyNet) ? fConfidence : fNotNearMyNet;
                    if (fConfidence < fNotNearMyNet && fNotNearMyNet < 0.5f)
                        fConfidence = fConfidence * fBranchRatio9;

                    float fInOffensive2 = InOffensiveZone((cPlayer*)g_pScriptCurrentFielder);
                    float fNotInOffensive2 = 1.0f - fInOffensive2;
                    float fMin10 = (fInOffensive2 <= fNotInOffensive2) ? fInOffensive2 : fNotInOffensive2;
                    float fMax10 = (fInOffensive2 >= fNotInOffensive2) ? fInOffensive2 : fNotInOffensive2;
                    float fBranchRatio10 = fMin10 / fMax10;

                    if (fInOffensive2 > 0.0f)
                    {
                        SaveConfidence PushDOM18(&fConfidence);
                        fConfidence = (fConfidence <= fInOffensive2) ? fConfidence : fInOffensive2;
                        if (fConfidence < fInOffensive2 && fInOffensive2 < 0.5f)
                            fConfidence = fConfidence * fBranchRatio10;
                        if (fBestConfidence >= fConfidence)
                            fBestConfidence = fBestConfidence;
                        else
                            fBestConfidence = fConfidence;
                        pDecision->QueueActionSetDesire(4, fConfidence, -1.0f, fvNotSet, fvNotSet);
                    }

                    if (fNotInOffensive2 > 0.0f)
                    {
                        SaveConfidence PushDOM19(&fConfidence);
                        fConfidence = (fConfidence <= fNotInOffensive2) ? fConfidence : fNotInOffensive2;
                        if (fConfidence < fNotInOffensive2 && fNotInOffensive2 < 0.5f)
                            fConfidence = fConfidence * fBranchRatio10;
                        if (fBestConfidence >= fConfidence)
                            fBestConfidence = fBestConfidence;
                        else
                            fBestConfidence = fConfidence;
                        pDecision->QueueActionSetDesire(3, fConfidence, -1.0f, fvNotSet, fvNotSet);
                    }
                }
            }
        }
    }

    float fNearBall = NearToBall((cPlayer*)g_pScriptCurrentMark);
    float fGoalieStunned = Stunned(g_pScriptCurrentTeam->GetGoalie());
    fNearBall = (fGoalieStunned <= fNearBall) ? fGoalieStunned : fNearBall;

    float fNotNearBall = 1.0f - fNearBall;
    float fMin10 = (fNearBall <= fNotNearBall) ? fNearBall : fNotNearBall;
    float fMax10 = (fNearBall >= fNotNearBall) ? fNearBall : fNotNearBall;
    float fBranchRatio10 = fMin10 / fMax10;

    if (fNearBall > 0.0f)
    {
        SaveConfidence PushDOM20(&fConfidence);
        fConfidence = (fConfidence <= fNearBall) ? fConfidence : fNearBall;
        if (fConfidence < fNearBall && fNearBall < 0.5f)
            fConfidence = fConfidence * fBranchRatio10;

        float fIdealTackle = AtIdealDistanceForTackling((cPlayer*)g_pScriptCurrentFielder, (cPlayer*)g_pScriptCurrentMark);
        float fNotIdealTackle = 1.0f - fIdealTackle;
        float fMin11 = (fIdealTackle <= fNotIdealTackle) ? fIdealTackle : fNotIdealTackle;
        float fMax11 = (fIdealTackle >= fNotIdealTackle) ? fIdealTackle : fNotIdealTackle;
        float fBranchRatio11 = fMin11 / fMax11;

        if (fIdealTackle > 0.0f)
        {
            SaveConfidence PushDOM21(&fConfidence);
            fConfidence = (fConfidence <= fIdealTackle) ? fConfidence : fIdealTackle;
            if (fConfidence < fIdealTackle && fIdealTackle < 0.5f)
                fConfidence = fConfidence * fBranchRatio11;

            if (fBestConfidence >= fConfidence)
                fBestConfidence = fBestConfidence;
            else
                fBestConfidence = fConfidence;

            pDecision->QueueActionSetDesire(5, fConfidence, 0.0f, FuzzyVariant((cPlayer*)g_pScriptCurrentMark), fvNotSet);
            SkillTweaks* pTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            pDecision->m_pLastQueuedAction->m_fSelectionChance = CalcSelectChance(pTweaks->Loose_HeavyAttackChance, Aggressive(g_pScriptCurrentFielder));
        }
    }

    fNearBall = 1.0f - fBestConfidence;
    float fFallbackNotNearBall = 1.0f - fNearBall;
    fMin10 = (fNearBall <= fFallbackNotNearBall) ? fNearBall : fFallbackNotNearBall;
    fMax10 = (fNearBall >= fFallbackNotNearBall) ? fNearBall : fFallbackNotNearBall;
    fBranchRatio10 = fMin10 / fMax10;

    if (fNearBall > 0.0f)
    {
        SaveConfidence PushDOM22(&fConfidence);
        fConfidence = (fConfidence <= fNearBall) ? fConfidence : fNearBall;
        if (fConfidence < fNearBall && fNearBall < 0.5f)
            fConfidence = fConfidence * fBranchRatio10;
        if (fBestConfidence >= fConfidence)
            fBestConfidence = fBestConfidence;
        else
            fBestConfidence = fConfidence;
        pDecision->QueueActionSetDesire(11, fConfidence, -1.0f, fvNotSet, fvNotSet);
    }

    return FuzzyVariant(fBestConfidence);
}
