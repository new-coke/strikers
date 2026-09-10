#include "Game/AI/Scripts/FormationScript.h"
#include "Game/AI/Fuzzy.h"
#include "Game/AI/Scripts/ScriptCaching.h"
#include "Game/AI/Scripts/ScriptQuestions.h"

/**
 * Offset/Address/Size: 0x1604 | 0x8007E640 | size: 0x320
 */
FuzzyVariant Fuzzy::GetBestDefensiveFormation(cTeam* TheTeam)
{
    FuzzyVariant bestValue;

    FuzzyVariant teamVar1(TheTeam);
    Variant* pv1 = &teamVar1;
    pv1->GetHash();

    FuzzyVariant teamVar2(TheTeam);

    FuzzyVariant formResult(0x4);

    bestValue = formResult;
    bestValue.Confidence = 1.0f;

    return bestValue;
}

/**
 * Offset/Address/Size: 0x12E4 | 0x8007E320 | size: 0x320
 */
FuzzyVariant Fuzzy::GetBestOffensiveFormation(cTeam* TheTeam)
{
    FuzzyVariant bestValue;

    FuzzyVariant teamVar1(TheTeam);
    Variant* pv1 = &teamVar1;
    pv1->GetHash();

    FuzzyVariant teamVar2(TheTeam);

    FuzzyVariant formResult(0x5);

    bestValue = formResult;
    bestValue.Confidence = 1.0f;

    return bestValue;
}

/**
 * Offset/Address/Size: 0x0 | 0x8007D03C | size: 0x12E4
 */
FuzzyVariant Fuzzy::GetBestBallFormationSet(cTeam* TheTeam)
{
    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    FuzzyVariant teamVar1(TheTeam);
    Variant* pv1 = &teamVar1;
    pv1->GetHash();

    FuzzyVariant teamVar2(TheTeam);

    {
        float fTrueConfidence = Losing(TheTeam);
        float fFalseConfidence = 1.0f - fTrueConfidence;
        float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fBranchRatio = fMin / fMax;
        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);
            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                fConfidence = fConfidence * fBranchRatio;
            }

            {
                float fTrueConfidence = TimeCloseToOver(g_pGame);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    {
                        fConfidence = fConfidence * fBranchRatio;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(3);
                    }
                }
            }

            {
                float fTrueConfidence = TimeNearlyOver(g_pGame);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    {
                        fConfidence = fConfidence * fBranchRatio;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(3);
                    }
                }
            }

            {
                float fTrueConfidence = TimeFarFromOver(g_pGame);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    {
                        fConfidence = fConfidence * fBranchRatio;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(2);
                    }
                }
            }
        }
    }

    {
        float fTrueConfidence = Tied(TheTeam);
        float fFalseConfidence = 1.0f - fTrueConfidence;
        float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fBranchRatio = fMin / fMax;
        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);
            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                fConfidence = fConfidence * fBranchRatio;
            }

            {
                float fTrueConfidence = TimeCloseToOver(g_pGame);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    {
                        fConfidence = fConfidence * fBranchRatio;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(3);
                    }
                }
            }

            {
                float fTrueConfidence = TimeNearlyOver(g_pGame);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    {
                        fConfidence = fConfidence * fBranchRatio;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(2);
                    }
                }
            }

            {
                float fTrueConfidence = TimeFarFromOver(g_pGame);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    {
                        fConfidence = fConfidence * fBranchRatio;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(2);
                    }
                }
            }
        }
    }

    {
        float fTrueConfidence = Winning(TheTeam);
        float fFalseConfidence = 1.0f - fTrueConfidence;
        float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fBranchRatio = fMin / fMax;
        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);
            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                fConfidence = fConfidence * fBranchRatio;
            }

            {
                float fTrueConfidence = TimeCloseToOver(g_pGame);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    {
                        fConfidence = fConfidence * fBranchRatio;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(2);
                    }
                }
            }

            {
                float fTrueConfidence = TimeNearlyOver(g_pGame);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    {
                        fConfidence = fConfidence * fBranchRatio;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(2);
                    }
                }
            }

            {
                float fTrueConfidence = TimeFarFromOver(g_pGame);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMax = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMin / fMax;
                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    {
                        fConfidence = fConfidence * fBranchRatio;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(2);
                    }
                }
            }
        }
    }

    {
        float fTrueConfidence = 0.0f;
        if (0.0f == fBestConfidence)
        {
            fTrueConfidence = 1.0f;
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
            {
                fConfidence = fConfidence * fBranchRatio;
            }

            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                bestValue = FuzzyVariant(2);
            }
        }
    }

    bestValue.Confidence = fBestConfidence;
    return bestValue;
}
