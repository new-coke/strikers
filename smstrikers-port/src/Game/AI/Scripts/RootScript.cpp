#include "Game/AI/Scripts/RootScript.h"
#include "Game/AI/AIPlay.h"
#include "Game/AI/Scripts/ScriptCaching.h"
#include "Game/AI/Scripts/ScriptQuestions.h"

extern cFielder* g_pScriptCurrentFielder;
extern cTeam* g_pScriptCurrentTeam;

#include "TestCases.cpp"

namespace Fuzzy
{
FuzzyVariant DefaultOffensivePlay(cDecisionEntity*);
FuzzyVariant AbortOffensivePlay(cDecisionEntity*);
FuzzyVariant DefaultDefencePlay(cDecisionEntity*);
FuzzyVariant AbortDefencePlay(cDecisionEntity*);
FuzzyVariant DefaultLoosePlay(cDecisionEntity*);
FuzzyVariant AbortLoosePlay(cDecisionEntity*);
} // namespace Fuzzy

typedef FuzzyVariant (*DecisionFn)(cDecisionEntity*);

cDecisionEntity g_pDecisionEntities[44] = {
    cDecisionEntity(DECISION_ENTITY_STRATEGY, 0, (DecisionFn)StrategyChoosePlay, NULL),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_NULL, NULL, NULL),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_OFFENSE_STRIKER_AGGRESSIVE, (DecisionFn)Fuzzy::DefaultOffensivePlay, (DecisionFn)Fuzzy::AbortOffensivePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_OFFENSE_STRIKER_MODERATE, (DecisionFn)Fuzzy::DefaultOffensivePlay, (DecisionFn)Fuzzy::AbortOffensivePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_OFFENSE_STRIKER_PASSIVE, (DecisionFn)Fuzzy::DefaultOffensivePlay, (DecisionFn)Fuzzy::AbortOffensivePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_OFFENSE_WINGER_AGGRESSIVE, (DecisionFn)Fuzzy::DefaultOffensivePlay, (DecisionFn)Fuzzy::AbortOffensivePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_OFFENSE_WINGER_MODERATE, (DecisionFn)Fuzzy::DefaultOffensivePlay, (DecisionFn)Fuzzy::AbortOffensivePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_OFFENSE_WINGER_PASSIVE, (DecisionFn)Fuzzy::DefaultOffensivePlay, (DecisionFn)Fuzzy::AbortOffensivePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_OFFENSE_MIDFIELD_AGGRESSIVE, (DecisionFn)Fuzzy::DefaultOffensivePlay, (DecisionFn)Fuzzy::AbortOffensivePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_OFFENSE_MIDFIELD_MODERATE, (DecisionFn)Fuzzy::DefaultOffensivePlay, (DecisionFn)Fuzzy::AbortOffensivePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_OFFENSE_MIDFIELD_PASSIVE, (DecisionFn)Fuzzy::DefaultOffensivePlay, (DecisionFn)Fuzzy::AbortOffensivePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_OFFENSE_DEFENDER_AGGRESSIVE, (DecisionFn)Fuzzy::DefaultOffensivePlay, (DecisionFn)Fuzzy::AbortOffensivePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_OFFENSE_DEFENDER_MODERATE, (DecisionFn)Fuzzy::DefaultOffensivePlay, (DecisionFn)Fuzzy::AbortOffensivePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_OFFENSE_DEFENDER_PASSIVE, (DecisionFn)Fuzzy::DefaultOffensivePlay, (DecisionFn)Fuzzy::AbortOffensivePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_DEFENSE_STRIKER_AGGRESSIVE, (DecisionFn)Fuzzy::DefaultDefencePlay, (DecisionFn)Fuzzy::AbortDefencePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_DEFENSE_STRIKER_MODERATE, (DecisionFn)Fuzzy::DefaultDefencePlay, (DecisionFn)Fuzzy::AbortDefencePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_DEFENSE_STRIKER_PASSIVE, (DecisionFn)Fuzzy::DefaultDefencePlay, (DecisionFn)Fuzzy::AbortDefencePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_DEFENSE_WINGER_AGGRESSIVE, (DecisionFn)Fuzzy::DefaultDefencePlay, (DecisionFn)Fuzzy::AbortDefencePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_DEFENSE_WINGER_MODERATE, (DecisionFn)Fuzzy::DefaultDefencePlay, (DecisionFn)Fuzzy::AbortDefencePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_DEFENSE_WINGER_PASSIVE, (DecisionFn)Fuzzy::DefaultDefencePlay, (DecisionFn)Fuzzy::AbortDefencePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_DEFENSE_MIDFIELD_AGGRESSIVE, (DecisionFn)Fuzzy::DefaultDefencePlay, (DecisionFn)Fuzzy::AbortDefencePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_DEFENSE_MIDFIELD_MODERATE, (DecisionFn)Fuzzy::DefaultDefencePlay, (DecisionFn)Fuzzy::AbortDefencePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_DEFENSE_MIDFIELD_PASSIVE, (DecisionFn)Fuzzy::DefaultDefencePlay, (DecisionFn)Fuzzy::AbortDefencePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_DEFENSE_DEFENDER_AGGRESSIVE, (DecisionFn)Fuzzy::DefaultDefencePlay, (DecisionFn)Fuzzy::AbortDefencePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_DEFENSE_DEFENDER_MODERATE, (DecisionFn)Fuzzy::DefaultDefencePlay, (DecisionFn)Fuzzy::AbortDefencePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_DEFENSE_DEFENDER_PASSIVE, (DecisionFn)Fuzzy::DefaultDefencePlay, (DecisionFn)Fuzzy::AbortDefencePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_LOOSE_STRIKER_AGGRESSIVE, (DecisionFn)Fuzzy::DefaultLoosePlay, (DecisionFn)Fuzzy::AbortLoosePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_LOOSE_STRIKER_MODERATE, (DecisionFn)Fuzzy::DefaultLoosePlay, (DecisionFn)Fuzzy::AbortLoosePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_LOOSE_STRIKER_PASSIVE, (DecisionFn)Fuzzy::DefaultLoosePlay, (DecisionFn)Fuzzy::AbortLoosePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_LOOSE_WINGER_AGGRESSIVE, (DecisionFn)Fuzzy::DefaultLoosePlay, (DecisionFn)Fuzzy::AbortLoosePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_LOOSE_WINGER_MODERATE, (DecisionFn)Fuzzy::DefaultLoosePlay, (DecisionFn)Fuzzy::AbortLoosePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_LOOSE_WINGER_PASSIVE, (DecisionFn)Fuzzy::DefaultLoosePlay, (DecisionFn)Fuzzy::AbortLoosePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_LOOSE_MIDFIELD_AGGRESSIVE, (DecisionFn)Fuzzy::DefaultLoosePlay, (DecisionFn)Fuzzy::AbortLoosePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_LOOSE_MIDFIELD_MODERATE, (DecisionFn)Fuzzy::DefaultLoosePlay, (DecisionFn)Fuzzy::AbortLoosePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_LOOSE_MIDFIELD_PASSIVE, (DecisionFn)Fuzzy::DefaultLoosePlay, (DecisionFn)Fuzzy::AbortLoosePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_LOOSE_DEFENDER_AGGRESSIVE, (DecisionFn)Fuzzy::DefaultLoosePlay, (DecisionFn)Fuzzy::AbortLoosePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_LOOSE_DEFENDER_MODERATE, (DecisionFn)Fuzzy::DefaultLoosePlay, (DecisionFn)Fuzzy::AbortLoosePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_LOOSE_DEFENDER_PASSIVE, (DecisionFn)Fuzzy::DefaultLoosePlay, (DecisionFn)Fuzzy::AbortLoosePlay),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_GSTATE_PREKICKOFF, NULL, NULL),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_GSTATE_BALLOUTOFPLAY, NULL, NULL),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_GSTATE_ENDHALF, NULL, NULL),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_GSTATE_ENDGAME, NULL, NULL),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_GSTATE_POSTGOAL, NULL, NULL),
    cDecisionEntity(DECISION_ENTITY_PLAY, AIPLAY_GSTATE_POSTREDCARD, NULL, NULL),
};

/**
 * Offset/Address/Size: 0x1E8C | 0x8007C478 | size: 0x8
 */
int GetNumDecisionEntities()
{
    return 0x2C;
}

/**
 * Offset/Address/Size: 0x0 | 0x8007A5EC | size: 0x1E8C
 */
FuzzyVariant StrategyChoosePlay(cDecisionEntity* pDecision)
{
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    if (Striker(g_pScriptCurrentFielder))
    {
        if (AggressiveT(g_pScriptCurrentTeam))
        {
            {
                float fTrueConfidence = Offensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_STRIKER_AGGRESSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Defensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_STRIKER_AGGRESSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Loose(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_LOOSE_STRIKER_AGGRESSIVE, fConfidence, -1.0f);
                }
            }
        }
        else if (Moderate(g_pScriptCurrentTeam))
        {
            {
                float fTrueConfidence = Offensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_STRIKER_MODERATE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Defensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_STRIKER_MODERATE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Loose(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_LOOSE_STRIKER_MODERATE, fConfidence, -1.0f);
                }
            }
        }
        else if (Passive(g_pScriptCurrentTeam))
        {
            {
                float fTrueConfidence = Offensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_STRIKER_PASSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Defensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_STRIKER_PASSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Loose(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_LOOSE_STRIKER_PASSIVE, fConfidence, -1.0f);
                }
            }
        }
    }

    else if (Winger(g_pScriptCurrentFielder))
    {
        if (AggressiveT(g_pScriptCurrentTeam))
        {
            {
                float fTrueConfidence = Offensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_WINGER_AGGRESSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Defensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_WINGER_AGGRESSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Loose(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_LOOSE_WINGER_AGGRESSIVE, fConfidence, -1.0f);
                }
            }
        }
        else if (Moderate(g_pScriptCurrentTeam))
        {
            {
                float fTrueConfidence = Offensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_WINGER_MODERATE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Defensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_WINGER_MODERATE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Loose(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_LOOSE_WINGER_MODERATE, fConfidence, -1.0f);
                }
            }
        }
        else if (Passive(g_pScriptCurrentTeam))
        {
            {
                float fTrueConfidence = Offensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_WINGER_PASSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Defensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_WINGER_PASSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Loose(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_LOOSE_WINGER_PASSIVE, fConfidence, -1.0f);
                }
            }
        }
    }

    else if (Midfield(g_pScriptCurrentFielder))
    {
        if (AggressiveT(g_pScriptCurrentTeam))
        {
            {
                float fTrueConfidence = Offensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_MIDFIELD_AGGRESSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Defensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_MIDFIELD_AGGRESSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Loose(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_LOOSE_MIDFIELD_AGGRESSIVE, fConfidence, -1.0f);
                }
            }
        }
        else if (Moderate(g_pScriptCurrentTeam))
        {
            {
                float fTrueConfidence = Offensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_MIDFIELD_MODERATE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Defensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_MIDFIELD_MODERATE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Loose(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_LOOSE_MIDFIELD_MODERATE, fConfidence, -1.0f);
                }
            }
        }
        else if (Passive(g_pScriptCurrentTeam))
        {
            {
                float fTrueConfidence = Offensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_MIDFIELD_PASSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Defensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_MIDFIELD_PASSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Loose(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_LOOSE_MIDFIELD_PASSIVE, fConfidence, -1.0f);
                }
            }
        }
    }

    else if (Defence(g_pScriptCurrentFielder))
    {
        if (AggressiveT(g_pScriptCurrentTeam))
        {
            {
                float fTrueConfidence = Offensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_DEFENDER_AGGRESSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Defensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_DEFENDER_AGGRESSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Loose(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_LOOSE_DEFENDER_AGGRESSIVE, fConfidence, -1.0f);
                }
            }
        }
        else if (Moderate(g_pScriptCurrentTeam))
        {
            {
                float fTrueConfidence = Offensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_DEFENDER_MODERATE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Defensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_DEFENDER_MODERATE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Loose(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_LOOSE_DEFENDER_MODERATE, fConfidence, -1.0f);
                }
            }
        }
        else if (Passive(g_pScriptCurrentTeam))
        {
            {
                float fTrueConfidence = Offensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_DEFENDER_PASSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Defensive(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_DEFENDER_PASSIVE, fConfidence, -1.0f);
                }
            }
            {
                float fTrueConfidence = Loose(g_pScriptCurrentTeam);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = FMIN(fTrueConfidence, fFalseConfidence);
                float fMax = FMAX(fTrueConfidence, fFalseConfidence);
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = FMIN(fConfidence, fTrueConfidence);
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        fConfidence = fConfidence * fBranchRatio;
                    fBestConfidence = FMAX(fBestConfidence, fConfidence);
                    pDecision->QueueActionSetPlay(AIPLAY_LOOSE_DEFENDER_PASSIVE, fConfidence, -1.0f);
                }
            }
        }
    }

    return FuzzyVariant(fBestConfidence);
}
