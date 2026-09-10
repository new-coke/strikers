static FuzzyVariant GetConfidenceForPassingTo(cFielder* TheFielder)
{
    FuzzyVariant bestValue;

    bestValue = FuzzyVariant(Open(TheFielder));
    bestValue = FuzzyVariant(FMIN(bestValue.GetFloat(), 1.0f - OnMushrooms(TheFielder)));
    bestValue = FuzzyVariant(FMIN(bestValue.GetFloat(), 1.0f - FallenDown(TheFielder)));

    return bestValue;
}

static FuzzyVariant PassToBestFielder(float confidence, cDecisionEntity* pDecision)
{
    float fConfidence = 1.0f;

    for (int i = 0; i < 5; i++)
    {
        cFielder* teammate = g_pScriptCurrentFielder->GetTeam()->GetFielder(i);
        if (teammate != g_pScriptCurrentFielder)
        {
            float fTrueConfidence = confidence;
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
                pDecision->QueueActionSetPlay(AIPLAY_NULL, fConfidence, -1.0f);
            }
        }
    }

    return FuzzyVariant(fConfidence);
}

static FuzzyVariant TestPlay(cDecisionEntity* pDecision)
{
    for (int i = 0; i < 5; i++)
    {
        cFielder* teammate = g_pScriptCurrentFielder->GetTeam()->GetFielder(i);
        if (teammate != g_pScriptCurrentFielder)
            pDecision->QueueActionSetPlay(AIPLAY_NULL, 1.0f, -1.0f);
    }

    return FuzzyVariant(1.0f);
}

static FuzzyVariant TestRatioBranching(cDecisionEntity* pDecision)
{
    float fConfidence = 1.0f;

    {
        float fTrueConfidence = 1.0f;
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
            pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_STRIKER_AGGRESSIVE, fConfidence, -1.0f);
            {
                float fTrueConfidence = 0.75f;
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
                    pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_STRIKER_MODERATE, fConfidence, -1.0f);
                    {
                        float fTrueConfidence = 0.5f;
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
                            pDecision->QueueActionSetPlay(AIPLAY_OFFENSE_STRIKER_PASSIVE, fConfidence, -1.0f);
                        }
                    }
                }
            }
        }
    }

    {
        float fTrueConfidence = 0.75f;
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
            pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_STRIKER_AGGRESSIVE, fConfidence, -1.0f);
            {
                float fTrueConfidence = 0.5f;
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
                    pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_STRIKER_MODERATE, fConfidence, -1.0f);
                    {
                        float fTrueConfidence = 0.25f;
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
                            pDecision->QueueActionSetPlay(AIPLAY_DEFENSE_STRIKER_PASSIVE, fConfidence, -1.0f);
                        }
                    }
                }
            }
        }
    }

    {
        float fTrueConfidence = 0.5f;
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
            pDecision->QueueActionSetPlay(AIPLAY_LOOSE_STRIKER_AGGRESSIVE, fConfidence, -1.0f);
            {
                float fTrueConfidence = 0.25f;
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
                    pDecision->QueueActionSetPlay(AIPLAY_LOOSE_STRIKER_MODERATE, fConfidence, -1.0f);
                    {
                        float fTrueConfidence = 0.1f;
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
                            pDecision->QueueActionSetPlay(AIPLAY_LOOSE_STRIKER_PASSIVE, fConfidence, -1.0f);
                        }
                    }
                }
            }
        }
    }

    return fvNotSet;
}
