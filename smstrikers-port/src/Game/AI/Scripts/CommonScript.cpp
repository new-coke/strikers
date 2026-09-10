#include "Game/AI/Scripts/CommonScript.h"
#include "Game/AI/Scripts/ScriptQuestions.h"

#include "Game/Team.h"
#include "Game/Player.h"
#include "Game/AI/Fielder.h"
#include "Game/Ball.h"
#include "Game/Field.h"
#include "Game/MathHelpers.h"

// Global helper/script-question declarations hoisted from block scope so they
// resolve to global symbols (Fuzzy is now a namespace, not a class).
class cGame;
class Goalie;
extern cTeam* g_pCurrentlyUpdatingTeam;
extern float InFrontOfTheirNet(cFielder*);
extern float IsPerfectPassInPlay();
extern float ReceivingVolleyPass(cPlayer*);
extern float CloseToTheirGoalie(cPlayer*);
extern float NearToTheirNet(cPlayer*);
extern float Stunned(Goalie*);
extern float Shooter(cFielder*);
extern float CalcSelectChance(float, float);
extern float Incapacitated(cPlayer*);
extern float UpfieldFrom(cPlayer*, cPlayer*);
extern float FarTo(cPlayer*, cPlayer*);
extern float NearTo(cPlayer*, cPlayer*);
extern float Open(cFielder*);
extern float OpenTo(cPlayer*, cPlayer*);
extern float FGREATER(float, float);
extern float AbleToUsePowerup(cFielder*, int);
extern float Captain(cFielder*);
extern float PerfectPassCandidateFrom(cFielder*, cFielder*);
extern float OnScreen(cPlayer*);
extern float LastBallOwner(cPlayer*);
extern float DownfieldFrom(cPlayer*, cPlayer*);
extern float Invincible(cFielder*);
extern float FallenDown(cFielder*);
extern float ChasingBall(cPlayer*);
extern float Facing(cPlayer*, cPlayer*);
extern float CloseToTheirNet(cPlayer*);
extern float InDefensiveZone(cPlayer*);
extern float InOffensiveZone(cPlayer*);
extern float LikelyToScore(cFielder*);
extern float PlayerShotDistance(cFielder*);
extern float OpenToTheirNet(cFielder*);
extern float GoalieOutOfPosition(cFielder*);
extern float OutOfNet(Goalie*);
extern cTeam* g_pScriptCurrentTeam;
extern float UserControlledT(cTeam*);
extern float ReceivingPass(cFielder*);
extern float WideOpen(cFielder*);
extern float ReceivingVolleyPassDelayed(cPlayer*);
extern float Aggressive(cFielder*);
extern float NearToBall(cPlayer*);
extern float ClosingTo(cPlayer*, cPlayer*);
extern float AtIdealDistanceForTackling(cPlayer*, cPlayer*);
extern float CloseTo(cPlayer*, cPlayer*);
extern float BallOwner(cPlayer*);
extern float OnTheGround(cPlayer*);
extern float PassReceiveCloseToDone(cFielder*);
extern float Passer(cFielder*);
extern float FLESS(float, float);
extern cTeam* g_pScriptOtherTeam;
extern cBall* g_pScriptBall;
extern cGame* g_pGame;
extern float RandomChance(float);
extern float TimeNearlyOver(cGame*);
extern float Stalling(cTeam*);
extern float Losing(cTeam*);
extern float Difficult(cTeam*);
extern float NormalizeVal(float, float, float);
extern float OnMushrooms(cFielder*);
extern float Ownerless(cBall*);
extern float Pressured(cFielder*);
extern float FacingSideline(cFielder*);
extern float SeparatingFrom(cPlayer*, cPlayer*);
extern float FarToBall(cPlayer*);
extern float WindingUpForShot(cFielder*);
extern float ReceivingPassDelayed(cFielder*);
extern float InDefensiveZoneOfPlayer(cBall*, cPlayer*);
extern float AbleToInterceptBall(cPlayer*);
extern float Marking(cFielder*, cPlayer*);
extern float GonnaGetBall(cTeam*);
extern float CloseToSideline(cFielder*);
extern float RepeatingLastDesire(cFielder*, eScriptFielderDesire);
extern float Attacked(cFielder*);
extern float Deker(cFielder*);
extern float NearToTheirGoalie(cPlayer*);
extern cBall* g_pBall;
extern cFielder* g_pScriptCurrentFielder;
extern float LikelyToUsePowerup(cFielder*, int);
extern float High(cBall*);
extern float AvoidingPowerups(cFielder*);
extern float StuckOnSidelines(cFielder*);
extern float Interpolate(float, float, float);

extern "C" double fabs(double);

union FunctionAddress
{
    FuzzyVariant (*teamFunction)(cTeam*);
    FuzzyVariant (*playerFunction)(cPlayer*);
    FuzzyVariant (*fielderFunction)(cFielder*);
    // PORT: pointer-width; this aliases the function pointers above.
    uintptr_t address;
};

/*
 * The script-question cache key. Every Fuzzy question builds the same key: a
 * FuzzyVariant wrapping the subject, hashed with the question function's own
 * address. Each site also constructs a second FuzzyVariant from the same subject
 * and discards it; retail's code contains both constructions, so both are here.
 *
 * Each macro declares hash, fvQuestion, fvQuestion2 and functionAddress into the
 * CALLER's scope -- that is where the `hash` used by Lookup/AddToCache further
 * down each function comes from.
 *
 * Two spellings, NOT interchangeable: forcing _REF everywhere costs +529 diff
 * rows across the TU, and it does not compile where a bare identifier is passed.
 *   SCRIPT_QUESTION_KEY      subject held in a named FuzzyVariant object
 *   SCRIPT_QUESTION_KEY_REF  subject held by const reference to a temporary
 * Pass a CAST as `arg` to _REF: its trailing (void)FuzzyVariant(arg) is an
 * expression only while arg is not a bare identifier; with one it would be the
 * most vexing parse and would silently declare a variable instead.
 *
 * Six sites do not consult the cache and previously carried a shorter form with
 * no address arithmetic; the macro re-adds it. Measured inert, and verified three
 * ways: deleting the arithmetic again is byte-identical, retail's object holds no
 * self-referencing relocation in those functions, and they sit at 0 diff rows.
 * `hash` there is unused, so the arithmetic is dead-stripped and only the two
 * constructions and the GetHash call survive -- what those functions emit.
 *
 * UNCONFIRMED: that this was a macro in retail. It reproduces retail's BYTES at
 * all 20 sites, but dwarf.txt's CommonScript CU names no functionAddress, hash or
 * fv* local anywhere, so the original spelling is not attested.
 */
#define SCRIPT_QUESTION_KEY_REF(member, fn, arg)                                     \
    const FuzzyVariant& fvQuestion = FuzzyVariant(arg);                              \
    FunctionAddress functionAddress;                                                 \
    functionAddress.member = fn;                                                     \
    uintptr_t hash = StrategicQuestionHash(functionAddress.address, fvQuestion); \
    (void)FuzzyVariant(arg)

#define SCRIPT_QUESTION_KEY(member, fn, arg)                                         \
    FuzzyVariant fvQuestion(arg);                                                    \
    FunctionAddress functionAddress;                                                 \
    functionAddress.member = fn;                                                     \
    uintptr_t hash = StrategicQuestionHash(functionAddress.address, fvQuestion); \
    FuzzyVariant fvQuestion2(arg)

float InBetweenMyNetAnd(cFielder*, cFielder*);
float InBetweenMyNetAnd(cFielder*, cBall*);
float AbleToInterceptBall(cPlayer*);
float AbleToInterceptBallForSwapController(cFielder*);
float ClosingTo(cPlayer*, cBall*);
float CloseToBall(cPlayer*);
float GoalieType(cPlayer*);
float StrategicBallOwner(cFielder*);
float BallOwner(cPlayer*);
float BallOwnerT(cTeam*);
float UserControlled(cFielder*);
float Defensive(cTeam*);
float Offensive(cTeam*);
float InOffensiveZone(cPlayer*);
float FarToMyNet(cPlayer*);
float FarToTheirNet(cPlayer*);
float FarToBall(cPlayer*);
float ReceivingPass(cFielder*);
float ReceivingVolleyPass(cPlayer*);
float NormalizeVal(float fromVal, float fromMin, float fromMax);

extern cFielder* g_pScriptCurrentFielder; // size: 0x4, address: 0x803977E0
extern cFielder* g_pScriptCurrentMark;    // size: 0x4, address: 0x803977E4
extern cFielder* g_pScriptBallOwner;
extern cTeam* g_pScriptCurrentTeam;
extern cTeam* g_pScriptOtherTeam;
extern cBall* g_pScriptBall;

#include "Game/AI/Scripts/ScriptCaching.h"

static inline uintptr_t StrategicQuestionHash(
    uintptr_t functionAddress,
    const FuzzyVariant& argument)
{
    return functionAddress + ((const Variant*)&argument)->GetHash();
}

/**
 * Offset/Address/Size: 0xF1B0 | 0x80079380 | size: 0x7D4
 */
FuzzyVariant Fuzzy::GetStrategicBallCarrier(cTeam* TheTeam)
{
    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    SCRIPT_QUESTION_KEY(teamFunction, GetStrategicBallCarrier, TheTeam);

    // The reference binding is load-bearing: taking the result by value instead
    // changes this function's callee-saved allocation and loses the match.
    ScriptQuestionCache* const& cache = ScriptQuestionCache::Instance();
    if (cache->Lookup(hash, bestValue, NULL))
    {
        fBestConfidence = bestValue.Confidence;
        bestValue.Confidence = fBestConfidence;
        ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
        return bestValue;
    }

    for (int i = 0; i < 4; i++)
    {
        cFielder* pFielder = TheTeam->GetFielder(i);
        float fTrueConfidence = StrategicBallOwner(pFielder);
        float fFalseConfidence = 1.0f - fTrueConfidence;
        float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fBranchRatio = fMinVal / fMaxVal;
        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);
            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                fConfidence = (float)(double)fConfidence * fBranchRatio;
            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                bestValue = FuzzyVariant((cPlayer*)pFielder);
            }
        }
    }

    bestValue.Confidence = fBestConfidence;
    ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
    return bestValue;
}

/**
 * Offset/Address/Size: 0xE9DC | 0x80078BAC | size: 0x7D4
 */
FuzzyVariant Fuzzy::GetBestBallInterceptor(cTeam* TheTeam)
{
    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    SCRIPT_QUESTION_KEY(teamFunction, GetBestBallInterceptor, TheTeam);

    // The reference binding is load-bearing: taking the result by value instead
    // changes this function's callee-saved allocation and loses the match.
    ScriptQuestionCache* const& cache = ScriptQuestionCache::Instance();
    if (cache->Lookup(hash, bestValue, NULL))
    {
        fBestConfidence = bestValue.Confidence;
        bestValue.Confidence = fBestConfidence;
        ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
        return bestValue;
    }

    for (int i = 0; i < 4; i++)
    {
        cFielder* pFielder = TheTeam->GetFielder(i);
        float fTrueConfidence = AbleToInterceptBall((cPlayer*)pFielder);
        float fFalseConfidence = 1.0f - fTrueConfidence;
        float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fBranchRatio = fMinVal / fMaxVal;

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);
            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio;
            }
            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                bestValue = FuzzyVariant((cPlayer*)pFielder);
            }
        }
    }

    bestValue.Confidence = fBestConfidence;
    ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
    return bestValue;
}

/**
 * Offset/Address/Size: 0xE40C | 0x800785DC | size: 0x5D0
 */
FuzzyVariant Fuzzy::GetSwapControllerScore(cPlayer* ThePlayer)
{
    FuzzyVariant bestValue;

    SCRIPT_QUESTION_KEY(playerFunction, GetSwapControllerScore, (cPlayer*)ThePlayer);

    unsigned char flag = 0;
    float weightedSum = 0.0f;
    float totalWeight = 0.0f;
    float passWeight = 0.0f;

    cFielder* passTarget = (cFielder*)g_pBall->GetPassTargetFielder();

    if (ReceivingPass(passTarget) && passTarget != (cFielder*)ThePlayer)
    {
        cTeam* targetTeam = passTarget != NULL ? passTarget->m_pTeam : NULL;
        cTeam* playerTeam = ThePlayer != NULL ? ThePlayer->m_pTeam : NULL;

        if (playerTeam != targetTeam)
        {
            flag = 1;
            if (ReceivingVolleyPass((cPlayer*)passTarget))
            {
                passWeight = 2.0f;
            }
            else
            {
                passWeight = 1.5f;
            }
        }
    }

    cTeam* team = ThePlayer != NULL ? ThePlayer->m_pTeam : NULL;
    if (team->GetNumAssignedControllers() > 1)
    {
        team = ThePlayer != NULL ? ThePlayer->m_pTeam : NULL;
        if (BallOwnerT(team) && ThePlayer->m_eClassType == FIELDER)
        {
            const float& shootScore = Fuzzy::GoodToShoot((cFielder*)ThePlayer).mData.f;
            float weight = 0.5f;
            flag = 0;
            totalWeight += weight;
            weightedSum += weight * shootScore;
        }
    }

    if (flag)
    {
        float dt = 0.1f;
        float px = ThePlayer->m_v3Position.x + dt * ThePlayer->m_v3Velocity.x;
        float tx = passTarget->m_v3Position.x + dt * passTarget->m_v3Velocity.x;
        float pz = ThePlayer->m_v3Position.y + dt * ThePlayer->m_v3Velocity.y;
        float tz = passTarget->m_v3Position.y + dt * passTarget->m_v3Velocity.y;
        float dx = px - tx;
        float dy = pz - tz;
        float dist = nlSqrt(dx * dx + dy * dy, true);
        float maxDist = 2.0f * cField::mv3FieldPosition.x;
        float range = 0.5f * maxDist;
        float normalized = NormalizeVal(dist, range, 0.0f);
        weightedSum += normalized * passWeight;
        totalWeight += passWeight;
    }

    if (ThePlayer->m_eClassType == FIELDER)
    {
        float intercept = AbleToInterceptBallForSwapController((cFielder*)ThePlayer);
        weightedSum += intercept;
        totalWeight += 1.0f;
        float offensive = InOffensiveZone(ThePlayer);
        float defense = 1.0f - offensive;
        team = ThePlayer != NULL ? ThePlayer->m_pTeam : NULL;
        float defResult = Defensive(team);
        defResult = (defResult <= defense) ? defResult : defense;
        if (defResult)
        {
            float inBetween = InBetweenMyNetAnd((cFielder*)ThePlayer, g_pBall);
            float weight = 0.175f;
            weightedSum += weight * inBetween;
            totalWeight += weight;
        }
        team = ThePlayer != NULL ? ThePlayer->m_pTeam : NULL;
        float notBallOwner = 1.0f - BallOwnerT(team);
        if (notBallOwner)
            ClosingTo(ThePlayer, g_pBall);
    }

    float result = 0.0f;
    if (totalWeight > 0.0f)
        result = weightedSum / totalWeight;

    u8 isIdle;
    isIdle = 0;
    if ((float)fabs(ThePlayer->m_v3ScreenPosition.x) <= 1.0f)
    {
        if ((float)fabs(ThePlayer->m_v3ScreenPosition.y) <= 1.0f)
            isIdle = 1;
    }

    if (isIdle)
        result = result;

    bestValue = FuzzyVariant(result);
    bestValue.Confidence = 1.0f;
    return bestValue;
}

/**
 * Offset/Address/Size: 0xDC78 | 0x80077E48 | size: 0x794
 */
FuzzyVariant Fuzzy::ShouldIStrafeBall(cFielder* TheFielder)
{
    FuzzyVariant bestValue;

    float confidence = 0.0f;

    SCRIPT_QUESTION_KEY(fielderFunction, ShouldIStrafeBall, (cPlayer*)TheFielder);

    if (!((bool)StrategicBallOwner(TheFielder)))
    {
        if (((bool)BallOwner(g_pScriptCurrentTeam->GetGoalie())) || ((bool)BallOwner(g_pScriptOtherTeam->GetGoalie())))
        {
            confidence = 1.0f;
            FuzzyVariant fvResult(1.0f);
            bestValue = fvResult;
        }
        else if (!((bool)UserControlled(TheFielder)) && TheFielder->m_fDesiredSpeed < 2.0f)
        {
            confidence = 1.0f;
            FuzzyVariant fvResult(1.0f);
            bestValue = fvResult;
        }
        else
        {
            cTeam* team = TheFielder != NULL ? TheFielder->m_pTeam : NULL;
            if ((bool)Defensive(team))
            {
                confidence = 1.0f;
                float farToMyNet = FarToMyNet(TheFielder);
                float inBetween = InBetweenMyNetAnd(TheFielder, g_pScriptBall);
                float a = (1.0f - farToMyNet) / 2.0f;
                FuzzyVariant fvResult(a + inBetween / 2.0f);
                bestValue = fvResult;
            }
            else
            {
                team = TheFielder != NULL ? TheFielder->m_pTeam : NULL;
                if ((bool)Offensive(team))
                {
                    confidence = 1.0f;
                    float farToTheirNet = FarToTheirNet(TheFielder);
                    FuzzyVariant fvResult(1.0f - farToTheirNet);
                    bestValue = fvResult;
                }
                else
                {
                    confidence = 1.0f;
                    float farToBall = FarToBall(TheFielder);
                    FuzzyVariant fvResult(1.0f - farToBall);
                    bestValue = fvResult;
                }
            }
        }
    }

    bestValue.Confidence = confidence;
    return bestValue;
}

/**
 * Offset/Address/Size: 0xD92C | 0x80077AFC | size: 0x34C
 */
FuzzyVariant Fuzzy::ShouldIStrafeMark(cFielder* TheFielder)
{
    FuzzyVariant bestValue;

    SCRIPT_QUESTION_KEY(fielderFunction, ShouldIStrafeMark, (cPlayer*)TheFielder);

    cFielder* mark = TheFielder != NULL ? TheFielder->m_pMark : NULL;
    float inBetween = InBetweenMyNetAnd(TheFielder, mark);

    FuzzyVariant fvResult(inBetween);

    bestValue = fvResult;
    bestValue.Confidence = 1.0f;

    return bestValue;
}

extern cFielder* g_pScriptCurrentFielder;
extern cFielder* g_pScriptBallOwner;

float Marking(cFielder*, cPlayer*);
float UpfieldFrom(cPlayer*, cPlayer*);
float Incapacitated(cPlayer*);

/**
 * Offset/Address/Size: 0xD188 | 0x80077358 | size: 0x7A4
 */
FuzzyVariant Fuzzy::ShouldIMarkBallOwner(cFielder* pFielder)
{
    FuzzyVariant bestValue;
    float confidence = 1.0f;
    float bestConfidence = 0.0f;

    SCRIPT_QUESTION_KEY(fielderFunction, ShouldIMarkBallOwner, (cPlayer*)pFielder);

    float score = 1.0f - Marking(g_pScriptCurrentFielder, g_pScriptBallOwner);
    float complement = 1.0f - score;
    float minVal = (score <= complement) ? score : complement;
    float maxVal = (score >= complement) ? score : complement;
    float ratio = minVal / maxVal;

    if (score > 0.0f)
    {
        SaveConfidence sc1(&confidence);

        confidence = (confidence <= score) ? confidence : score;
        if (confidence < score && score < 0.5f)
        {
            double d = confidence;
            confidence = (float)d * ratio;
        }

        float combined = nlMaxEquals(
            Incapacitated(
                (cPlayer*)(g_pScriptBallOwner != NULL ? g_pScriptBallOwner->m_pMarker : NULL)),
            UpfieldFrom(
                (cPlayer*)(g_pScriptBallOwner != NULL ? g_pScriptBallOwner->m_pMarker : NULL),
                (cPlayer*)g_pScriptBallOwner));
        float notCombined = 1.0f - combined;

        float minVal2 = (combined <= notCombined) ? combined : notCombined;
        float maxVal2 = (combined >= notCombined) ? combined : notCombined;
        float ratio2 = minVal2 / maxVal2;

        if (combined > 0.0f)
        {
            SaveConfidence sc2(&confidence);

            confidence = (confidence <= combined) ? confidence : combined;
            if (confidence < combined && combined < 0.5f)
            {
                double d = confidence;
                confidence = (float)d * ratio2;
            }

            if (confidence > 0.0f)
            {
                bestConfidence = confidence;
                FuzzyVariant returnValue(confidence);
                bestValue = returnValue;
            }
        }

        if (notCombined > 0.0f)
        {
            SaveConfidence sc3(&confidence);

            confidence = (confidence <= notCombined) ? confidence : notCombined;
            if (confidence < notCombined && notCombined < 0.5f)
            {
                double d = confidence;
                confidence = (float)d * ratio2;
            }

            if (confidence > bestConfidence)
            {
                bestConfidence = confidence;
                FuzzyVariant returnValue(0.0f);
                bestValue = returnValue;
            }
        }
    }

    if (complement > 0.0f)
    {
        SaveConfidence sc4(&confidence);

        confidence = (confidence <= complement) ? confidence : complement;
        if (confidence < complement && complement < 0.5f)
        {
            double d = confidence;
            confidence = (float)d * ratio;
        }

        if (confidence > bestConfidence)
        {
            bestConfidence = confidence;
            FuzzyVariant returnValue(0.0f);
            bestValue = returnValue;
        }
    }

    bestValue.Confidence = bestConfidence;
    return bestValue;
}

static inline float max_float(float a, float b)
{
    if (a >= b)
    {
        return a;
    }
    return b;
}

static inline float min_float(float a, float b)
{
    if (a <= b)
    {
        return a;
    }
    return b;
}

static inline float FuzzyNot(const FuzzyVariant& v)
{
    return 1.0f - v.mData.f;
}

static inline float FuzzyNot(float f)
{
    return 1.0f - f;
}

static inline float nlMinFour(float a, float b, float c, float d)
{
    return min_float(a, min_float(b, min_float(c, d)));
}

static inline float nlMinThree(float a, float b, float c)
{
    return min_float(a, min_float(b, c));
}

static inline float nlMaxThree(float a, float b, float c)
{
    return max_float(a, max_float(b, c));
}

static inline float nlMinEqualsFour(float a, float b, float c, float d)
{
    return nlMinEquals(a, nlMinEquals(b, nlMinEquals(c, d)));
}

static inline float nlMinEqualsThree(float a, float b, float c)
{
    return nlMinEquals(a, nlMinEquals(b, c));
}

static inline float nlMaxEqualsThree(float a, float b, float c)
{
    return nlMaxEquals(a, nlMaxEquals(b, c));
}

static inline float nlMaxFour(float a, float b, float c, float d)
{
    return max_float(a, max_float(b, max_float(c, d)));
}

static inline float nlMaxFive(float a, float b, float c, float d, float e)
{
    return max_float(a, max_float(b, max_float(c, max_float(d, e))));
}

static inline float WeightedScore2(float fScoreA, float fWeightA, float fScoreB, float fWeightB)
{
    return fScoreA * fWeightA + fScoreB * fWeightB;
}

static inline float WeightedScore3(
    float fScoreA, float fWeightA, float fScoreB, float fWeightB, float fScoreC, float fWeightC)
{
    return fScoreA * fWeightA + fScoreB * fWeightB + fScoreC * fWeightC;
}

/**
 * Offset/Address/Size: 0xC144 | 0x80076314 | size: 0x1044
 */
FuzzyVariant Fuzzy::ShouldIAttemptOneTimer(cFielder* TheFielder)
{

    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    SCRIPT_QUESTION_KEY_REF(fielderFunction, ShouldIAttemptOneTimer, (cPlayer*)TheFielder);

    {
        ScriptQuestionCache* const lookupCache = ScriptQuestionCache::Instance();
        unsigned char cacheHit = lookupCache->Lookup(hash, bestValue, NULL);
        if (cacheHit)
        {
            fBestConfidence = bestValue.Confidence;
            bestValue.Confidence = fBestConfidence;
            ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
            return bestValue;
        }
    }

    float fTrueConfidence = nlMaxEquals(
        FarToTheirNet((cPlayer*)TheFielder), 1.0f - InFrontOfTheirNet(TheFielder));

    float fFalseConfidence = 1.0f - fTrueConfidence;
    float fBranchRatio;
    float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    fBranchRatio = fMinVal / fMaxVal;

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
        {
            fConfidence = (float)(double)fConfidence * fBranchRatio;
        }

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
        {
            fConfidence = (float)(double)fConfidence * fBranchRatio;
        }

        float fTrueConfidence2 = IsPerfectPassInPlay();
        float fFalseConfidence2 = 1.0f - fTrueConfidence2;
        float fMinVal2 = (fTrueConfidence2 <= fFalseConfidence2) ? fTrueConfidence2 : fFalseConfidence2;
        float fMaxVal2 = (fTrueConfidence2 >= fFalseConfidence2) ? fTrueConfidence2 : fFalseConfidence2;
        float fBranchRatio2 = fMinVal2 / fMaxVal2;

        if (fTrueConfidence2 > 0.0f)
        {
            SaveConfidence PushDOM2(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence2) ? fConfidence : fTrueConfidence2;
            if (fConfidence < fTrueConfidence2 && fTrueConfidence2 < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio2;
            }

            {
                FuzzyVariant returnValue(1.0f);
                SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                returnValue.SelectionChance = CalcSelectChance(pSkillTweaks->Off_VolleyOneTimerChance, Shooter(TheFielder));
                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    bestValue = returnValue;
                }
            }
        }

        float fTrueConfidence3 = 1.0f - FarToTheirNet((cPlayer*)TheFielder);
        float fBranchRatio3;
        float fFalseConfidence3 = 1.0f - fTrueConfidence3;
        float fMinVal3 = (fTrueConfidence3 <= fFalseConfidence3) ? fTrueConfidence3 : fFalseConfidence3;
        float fMaxVal3 = (fTrueConfidence3 >= fFalseConfidence3) ? fTrueConfidence3 : fFalseConfidence3;
        fBranchRatio3 = fMinVal3 / fMaxVal3;

        if (fTrueConfidence3 > 0.0f)
        {
            SaveConfidence PushDOM2(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence3) ? fConfidence : fTrueConfidence3;
            if (fConfidence < fTrueConfidence3 && fTrueConfidence3 < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio3;
            }

            float fTrueConfidence4 = ReceivingVolleyPass((cPlayer*)TheFielder);
            float fFalseConfidence4 = 1.0f - fTrueConfidence4;
            float fMinVal4 = (fTrueConfidence4 <= fFalseConfidence4) ? fTrueConfidence4 : fFalseConfidence4;
            float fMaxVal4 = (fTrueConfidence4 >= fFalseConfidence4) ? fTrueConfidence4 : fFalseConfidence4;
            float fBranchRatio4 = fMinVal4 / fMaxVal4;

            if (fTrueConfidence4 > 0.0f)
            {
                SaveConfidence PushDOM3(&fConfidence);

                fConfidence = (fConfidence <= fTrueConfidence4) ? fConfidence : fTrueConfidence4;
                if (fConfidence < fTrueConfidence4 && fTrueConfidence4 < 0.5f)
                {
                    fConfidence = (float)(double)fConfidence * fBranchRatio4;
                }

                {
                    FuzzyVariant returnValue(nlMaxThree(
                        Stunned(TheFielder != NULL
                                    ? ((TheFielder != NULL) ? TheFielder->m_pTeam->GetOtherTeam() : NULL)->GetGoalie()
                                    : NULL),
                        GoodToShoot(TheFielder).mData.f,
                        NearToTheirNet((cPlayer*)TheFielder) / 2.0f
                            + nlMaxEquals(InDanger(TheFielder).mData.f,
                                  CloseToTheirGoalie((cPlayer*)TheFielder))
                                  / 2.0f));
                    SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                    returnValue.SelectionChance = CalcSelectChance(pSkillTweaks->Off_VolleyOneTimerChance, Shooter(TheFielder));
                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = returnValue;
                    }
                }
            }

            if (fFalseConfidence4 > 0.0f)
            {
                SaveConfidence PushDOM3(&fConfidence);

                fConfidence = (fConfidence <= fFalseConfidence4) ? fConfidence : fFalseConfidence4;
                if (fConfidence < fFalseConfidence4 && fFalseConfidence4 < 0.5f)
                {
                    fConfidence = (float)(double)fConfidence * fBranchRatio4;
                }

                float fDanger;
                Goalie* pGoalie;
                if (TheFielder != NULL)
                {
                    cTeam* pOtherTeam = (TheFielder != NULL) ? TheFielder->m_pTeam->GetOtherTeam() : NULL;
                    pGoalie = pOtherTeam->GetGoalie();
                }
                else
                {
                    pGoalie = NULL;
                }

                float fGoalieStunned = Stunned(pGoalie);
                fDanger = GoodToShoot(TheFielder).mData.f / 2.0f
                        + nlMaxEquals(InDanger(TheFielder).mData.f, fGoalieStunned) / 2.0f;

                {
                    FuzzyVariant returnValue(fDanger);
                    SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                    returnValue.SelectionChance = CalcSelectChance(pSkillTweaks->Off_GroundOneTimerChance, Shooter(TheFielder));
                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = returnValue;
                    }
                }
            }
        }

        if (fFalseConfidence3 > 0.0f)
        {
            SaveConfidence PushDOM2(&fConfidence);

            fConfidence = (fConfidence <= fFalseConfidence3) ? fConfidence : fFalseConfidence3;
            if (fConfidence < fFalseConfidence3 && fFalseConfidence3 < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio3;
            }

            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                bestValue = FuzzyVariant(0.0f);
            }
        }
    }

    bestValue.Confidence = fBestConfidence;

    ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
    return bestValue;
}

/**
 * Offset/Address/Size: 0xB89C | 0x80075A6C | size: 0x8A8
 */
FuzzyVariant Fuzzy::GetBestLooseBallPassTarget(cFielder* TheFielder)
{
    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;
    cFielder* const fielder = TheFielder;

    SCRIPT_QUESTION_KEY_REF(fielderFunction, GetBestLooseBallPassTarget, (cPlayer*)fielder);

    ScriptQuestionCache* const& cache = ScriptQuestionCache::Instance();
    if (cache->Lookup(hash, bestValue, NULL))
    {
        bestValue.Confidence = bestValue.Confidence;
        ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
        return bestValue;
    }

    float fTrueConfidence = InDanger(fielder).mData.f;
    float fFalseConfidence = 1.0f - fTrueConfidence;
    float fBranchRatio = min_float(fTrueConfidence, fFalseConfidence)
                       / max_float(fTrueConfidence, fFalseConfidence);

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);
        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            fConfidence = (float)(double)fConfidence * fBranchRatio;

        FuzzyVariant theBestPassTarget = GetBestPassTarget((cPlayer*)fielder);

        float fPassConfidence = (theBestPassTarget.Confidence <= fConfidence) ? theBestPassTarget.Confidence : fConfidence;
        float fPassFalseConfidence = 1.0f - fPassConfidence;
        float fPassBranchRatio = min_float(fPassConfidence, fPassFalseConfidence)
                               / max_float(fPassConfidence, fPassFalseConfidence);

        if (fPassConfidence > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);
            fConfidence = (fConfidence <= fPassConfidence) ? fConfidence : fPassConfidence;
            if (fConfidence < fPassConfidence && fPassConfidence < 0.5f)
                fConfidence = (float)(double)fConfidence * fPassBranchRatio;
            if (fConfidence > 0.0f)
            {
                fBestConfidence = fConfidence;
                bestValue = theBestPassTarget;
            }
        }
    }

    bestValue.Confidence = fBestConfidence;
    ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
    return bestValue;
}

static inline void UpdatePassTargetConfidence(float& confidence, float incapacitated, float farValue, float nearValue, float upfield)
{
    confidence = 1.0f - incapacitated;
    nearValue = (nearValue <= upfield) ? nearValue : upfield;
    farValue = (farValue <= nearValue) ? farValue : nearValue;
    confidence = (confidence <= farValue) ? confidence : farValue;
}

/**
 * Offset/Address/Size: 0xAC34 | 0x80074E04 | size: 0xC68
 */
FuzzyVariant Fuzzy::GetBestPassTarget(cPlayer* ThePlayer)
{

    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    SCRIPT_QUESTION_KEY_REF(playerFunction, GetBestPassTarget, (cPlayer*)ThePlayer);

    ScriptQuestionCache* const& cache = ScriptQuestionCache::Instance();
    if (cache->Lookup(hash, bestValue, NULL))
    {
        fBestConfidence = bestValue.Confidence;
        bestValue.Confidence = fBestConfidence;
        ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);

        return bestValue;
    }

    float fTrueConfidence = GoalieType(ThePlayer);
    float fFalseConfidence = 1.0f - fTrueConfidence;
    float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fBranchRatio = fMinVal / fMaxVal;

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
        {
            double d = fConfidence;
            fConfidence = (float)d * fBranchRatio;
        }

        for (int i = 0; i < 4; i++)
        {
            cFielder* TeamMate = ThePlayer->m_pTeam->GetFielder(i);
            if (TeamMate != (cFielder*)ThePlayer)
            {
                UpdatePassTargetConfidence(fTrueConfidence, Incapacitated((cPlayer*)TeamMate), 1.0f - FarTo(ThePlayer, (cPlayer*)TeamMate), 1.0f - NearTo(ThePlayer, (cPlayer*)TeamMate), UpfieldFrom((cPlayer*)TeamMate, ThePlayer));
                {
                    float fFalseConfidence = 1.0f - fTrueConfidence;
                    float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                    float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                    float fBranchRatio = fMinVal / fMaxVal;

                    if (fTrueConfidence > 0.0f)
                    {
                        SaveConfidence PushDOM2(&fConfidence);

                        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        {
                            double d = fConfidence;
                            fConfidence = (float)d * fBranchRatio;
                        }

                        float fOpen = Open(TeamMate);
                        float fOpenTo = OpenTo(ThePlayer, (cPlayer*)TeamMate);
                        float fHalf = 0.5f;
                        float fOpenWeighted = fOpen * fHalf;
                        fTrueConfidence = fOpenTo * fHalf + fOpenWeighted;

                        {
                            float fFalseConfidence = 1.0f - fTrueConfidence;
                            float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                            float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                            float fBranchRatio = fMinVal / fMaxVal;

                            if (fTrueConfidence > 0.0f)
                            {
                                SaveConfidence PushDOM3(&fConfidence);

                                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                                {
                                    double d = fConfidence;
                                    fConfidence = (float)d * fBranchRatio;
                                }
                                if (fConfidence > fBestConfidence)
                                {
                                    fBestConfidence = fConfidence;
                                    bestValue = FuzzyVariant((cPlayer*)TeamMate);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (fFalseConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;

        if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
        {
            double d = fConfidence;
            fConfidence = (float)d * fBranchRatio;
        }

        FuzzyVariant TheFielder((cPlayer*)ThePlayer);

        for (int i = 0; i < 4; i++)
        {
            cFielder* TeamMate = TheFielder.mData.pPlayer->m_pTeam->GetFielder(i);
            if (TeamMate != (cFielder*)TheFielder.mData.pPlayer)
            {
                fTrueConfidence = GoodPassTargetFrom(TeamMate, (cFielder*)TheFielder.mData.pPlayer).mData.f;
                {
                    float fFalseConfidence = 1.0f - fTrueConfidence;
                    float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                    float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                    float fBranchRatio = fMinVal / fMaxVal;

                    if (fTrueConfidence > 0.0f)
                    {
                        SaveConfidence PushDOM2(&fConfidence);

                        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                        {
                            double d = fConfidence;
                            fConfidence = (float)d * fBranchRatio;
                        }
                        if (fConfidence > fBestConfidence)
                        {
                            fBestConfidence = fConfidence;
                            bestValue = FuzzyVariant((cPlayer*)TeamMate);
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
 * Offset/Address/Size: 0xA138 | 0x80074308 | size: 0xAFC
 */
FuzzyVariant Fuzzy::GoodPassTargetFrom(cFielder* TheTargetFielder, cFielder* TheBallOwner)
{

    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fFalseConfidence;
    float fBestConfidence = 0.0f;

    float fTrueConfidence = Incapacitated((cPlayer*)TheTargetFielder);
    fFalseConfidence = 1.0f - fTrueConfidence;
    float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fBranchRatio = fMinVal / fMaxVal;

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
        if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
        {
            fConfidence = (float)(double)fConfidence * fBranchRatio;
        }

        if (fConfidence > 0.0f)
        {
            fBestConfidence = fConfidence;
            FuzzyVariant fvResult(0.0f);
            bestValue = fvResult;
        }
    }

    if (fFalseConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
        if ((fConfidence < fFalseConfidence) && (fFalseConfidence < 0.5f))
        {
            fConfidence = (float)(double)fConfidence * fBranchRatio;
        }

        float fTrueConfidenceNear = 1.0f - NearTo((cPlayer*)TheTargetFielder, (cPlayer*)TheBallOwner);
        float fFalseConfidenceNear = 1.0f - fTrueConfidenceNear;
        float fMinValNear = (fTrueConfidenceNear <= fFalseConfidenceNear) ? fTrueConfidenceNear : fFalseConfidenceNear;
        float fMaxValNear = (fTrueConfidenceNear >= fFalseConfidenceNear) ? fTrueConfidenceNear : fFalseConfidenceNear;
        float fBranchRatioNear = fMinValNear / fMaxValNear;

        if (fTrueConfidenceNear > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidenceNear) ? fConfidence : fTrueConfidenceNear;
            if ((fConfidence < fTrueConfidenceNear) && (fTrueConfidenceNear < 0.5f))
            {
                fConfidence = (float)(double)fConfidence * fBranchRatioNear;
            }

            float fShootWeight = 0.5f;
            float fTotalSum = WeightedScore2(
                FGREATER(Fuzzy::GoodToShoot(TheTargetFielder).GetFloat(), 0.3f), fShootWeight, FGREATER(Fuzzy::GoodToShoot(TheTargetFielder).GetFloat(), Fuzzy::GoodToShoot(TheBallOwner).GetFloat()), fShootWeight);
            float fCaptainBonus = 1.0f;
            if (AbleToUsePowerup(TheTargetFielder, 8) && Captain(TheTargetFielder))
            {
                fCaptainBonus = 2.0f;
            }

            float fPlayerWeighting = PerfectPassCandidateFrom(TheTargetFielder, TheBallOwner);
            float fNetWeighting = OpenTo((cPlayer*)TheBallOwner, (cPlayer*)TheTargetFielder);
            float fOnScreen = OnScreen((cPlayer*)TheTargetFielder);
            float fTotalSumWeight = 0.2f;
            float fOnScreenNetWeight = 0.15f;
            float fPlayerWeight = 0.5f;
            float fTrueConfidence2 = fPlayerWeighting * fPlayerWeight
                                   + (fNetWeighting * fOnScreenNetWeight
                                       + (fOnScreen * fOnScreenNetWeight
                                           + fTotalSum * fTotalSumWeight));

            float fFalseConfidence2 = 1.0f - fTrueConfidence2;
            float fMinVal2 = (fTrueConfidence2 <= fFalseConfidence2) ? fTrueConfidence2 : fFalseConfidence2;
            float fMaxVal2 = (fTrueConfidence2 >= fFalseConfidence2) ? fTrueConfidence2 : fFalseConfidence2;
            float fBranchRatio2 = fMinVal2 / fMaxVal2;

            if (fTrueConfidence2 > 0.0f)
            {
                SaveConfidence PushDOM(&fConfidence);

                fConfidence = (fConfidence <= fTrueConfidence2) ? fConfidence : fTrueConfidence2;
                if ((fConfidence < fTrueConfidence2) && (fTrueConfidence2 < 0.5f))
                {
                    fConfidence = (float)(double)fConfidence * fBranchRatio2;
                }

                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    bestValue = FuzzyVariant(fConfidence * fCaptainBonus);
                }
            }

            float fTrueConfidence3 = NearToTheirNet((cPlayer*)TheTargetFielder);
            float fFalseConfidence3 = 1.0f - fTrueConfidence3;
            float fMinVal3 = (fTrueConfidence3 <= fFalseConfidence3) ? fTrueConfidence3 : fFalseConfidence3;
            float fMaxVal3 = (fTrueConfidence3 >= fFalseConfidence3) ? fTrueConfidence3 : fFalseConfidence3;
            float fBranchRatio3 = fMinVal3 / fMaxVal3;

            if (fTrueConfidence3 > 0.0f)
            {
                SaveConfidence PushDOM(&fConfidence);

                fConfidence = (fConfidence <= fTrueConfidence3) ? fConfidence : fTrueConfidence3;
                if ((fConfidence < fTrueConfidence3) && (fTrueConfidence3 < 0.5f))
                {
                    fConfidence = (float)(double)fConfidence * fBranchRatio3;
                }

                float fLastBallOwner = LastBallOwner((cPlayer*)TheTargetFielder);
                float fOpenToBallOwner = OpenTo((cPlayer*)TheBallOwner, (cPlayer*)TheTargetFielder);
                float fDownfield = DownfieldFrom((cPlayer*)TheBallOwner, (cPlayer*)TheTargetFielder);

                float fDownfieldOpenWeight = 0.2f;
                float fTotalSumWeight = 0.425f;
                float fLastOwnerWeight = 0.175f;
                float fTrueConfidence4 = (1.0f - fLastBallOwner) * fLastOwnerWeight
                                       + (fOpenToBallOwner * fDownfieldOpenWeight
                                           + (fTotalSum * fTotalSumWeight
                                               + fDownfield * fDownfieldOpenWeight));
                float fFalseConfidence4 = 1.0f - fTrueConfidence4;
                float fMinVal4 = (fTrueConfidence4 <= fFalseConfidence4) ? fTrueConfidence4 : fFalseConfidence4;
                float fMaxVal4 = (fTrueConfidence4 >= fFalseConfidence4) ? fTrueConfidence4 : fFalseConfidence4;
                float fBranchRatio4 = fMinVal4 / fMaxVal4;

                if (fTrueConfidence4 > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);

                    fConfidence = (fConfidence <= fTrueConfidence4) ? fConfidence : fTrueConfidence4;
                    if ((fConfidence < fTrueConfidence4) && (fTrueConfidence4 < 0.5f))
                    {
                        fConfidence = (float)(double)fConfidence * fBranchRatio4;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(fConfidence * fCaptainBonus);
                    }
                }
            }

            if (fFalseConfidence3 > 0.0f)
            {
                SaveConfidence PushDOM(&fConfidence);

                fConfidence = (fConfidence <= fFalseConfidence3) ? fConfidence : fFalseConfidence3;
                if ((fConfidence < fFalseConfidence3) && (fFalseConfidence3 < 0.5f))
                {
                    fConfidence = (float)(double)fConfidence * fBranchRatio3;
                }

                float fLastBallOwner = LastBallOwner((cPlayer*)TheTargetFielder);
                float fOpenToBallOwner = OpenTo((cPlayer*)TheBallOwner, (cPlayer*)TheTargetFielder);
                float fDownfield = DownfieldFrom((cPlayer*)TheBallOwner, (cPlayer*)TheTargetFielder);

                float fDownfieldWeight = 0.35f;
                float fTotalSumWeight = 0.3f;
                float fOpenWeight = 0.2f;
                float fLastOwnerWeight = 0.15f;
                float fTrueConfidence4 = (1.0f - fLastBallOwner) * fLastOwnerWeight
                                       + (fOpenToBallOwner * fOpenWeight
                                           + (fTotalSum * fTotalSumWeight
                                               + fDownfield * fDownfieldWeight));
                float fFalseConfidence4 = 1.0f - fTrueConfidence4;
                float fMinVal4 = (fTrueConfidence4 <= fFalseConfidence4) ? fTrueConfidence4 : fFalseConfidence4;
                float fMaxVal4 = (fTrueConfidence4 >= fFalseConfidence4) ? fTrueConfidence4 : fFalseConfidence4;
                float fBranchRatio4 = fMinVal4 / fMaxVal4;

                if (fTrueConfidence4 > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);

                    fConfidence = (fConfidence <= fTrueConfidence4) ? fConfidence : fTrueConfidence4;
                    if ((fConfidence < fTrueConfidence4) && (fTrueConfidence4 < 0.5f))
                    {
                        fConfidence = (float)(double)fConfidence * fBranchRatio4;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(fConfidence * fCaptainBonus);
                    }
                }
            }
        }
    }

    bestValue.Confidence = fBestConfidence;
    return bestValue;
}

/**
 * Offset/Address/Size: 0x95A0 | 0x80073770 | size: 0xB98
 */
FuzzyVariant Fuzzy::GetBestHitTarget(cFielder* TheFielder)
{

    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    SCRIPT_QUESTION_KEY(fielderFunction, GetBestHitTarget, TheFielder);

    ScriptQuestionCache* const& cache = ScriptQuestionCache::Instance();
    if (cache->Lookup(hash, bestValue, NULL))
    {
        bestValue.Confidence = bestValue.Confidence;
        ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
        return bestValue;
    }

    for (int i = 0; i < 4; i++)
    {
        cFielder* theOpponent = g_pScriptOtherTeam->GetFielder(i);

        float fNotInvincible = FuzzyNot(Invincible(theOpponent));
        float fNotFallen = FuzzyNot(FallenDown(theOpponent));
        float fTrueConfidence = (fNotFallen <= fNotInvincible) ? fNotFallen : fNotInvincible;

        float fFalseConfidence = 1.0f - fTrueConfidence;
        float fBranchRatio = min_float(fTrueConfidence, fFalseConfidence)
                           / max_float(fTrueConfidence, fFalseConfidence);

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);
            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                double d = fConfidence;
                fConfidence = (float)d * fBranchRatio;
            }

            float fBallOwner = nlMaxThree(BallOwner((cPlayer*)theOpponent), ReceivingPass(theOpponent), ChasingBall((cPlayer*)theOpponent));

            float fTrueConfidence = fBallOwner;
            float fFalseConfidence = 1.0f - fBallOwner;
            float fBranchRatio2 = min_float(fTrueConfidence, fFalseConfidence)
                                / max_float(fTrueConfidence, fFalseConfidence);

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM2(&fConfidence);
                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                {
                    double d = fConfidence;
                    fConfidence = (float)d * fBranchRatio2;
                }

                float fTrueConfidence2 = 1.0f - FarTo((cPlayer*)TheFielder, (cPlayer*)theOpponent);
                float fFalseConfidence2 = 1.0f - fTrueConfidence2;
                float fBranchRatio2 = min_float(fTrueConfidence2, fFalseConfidence2)
                                    / max_float(fTrueConfidence2, fFalseConfidence2);

                if (fTrueConfidence2 > 0.0f)
                {
                    SaveConfidence PushDOM3(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence2) ? fConfidence : fTrueConfidence2;
                    if (fConfidence < fTrueConfidence2 && fTrueConfidence2 < 0.5f)
                    {
                        double d = fConfidence;
                        fConfidence = (float)d * fBranchRatio2;
                    }
                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(theOpponent);
                    }
                }
            }

            if (fFalseConfidence > 0.0f)
            {
                SaveConfidence PushDOM4(&fConfidence);
                fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
                if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
                {
                    double d = fConfidence;
                    fConfidence = (float)d * fBranchRatio2;
                }

                float fFacing = Facing((cPlayer*)TheFielder, (cPlayer*)theOpponent);
                float fNearTo = NearTo((cPlayer*)TheFielder, (cPlayer*)theOpponent);
                float fNearWeight = 0.6f;
                float fFacingWeight = 0.3f;
                float fZero = 0.0f;
                float fTotalSum = fNearTo * fNearWeight + fZero;
                float fTrueConfidence3 = fFacing * fFacingWeight + fTotalSum;
                float fFalseConfidence3 = 1.0f - fTrueConfidence3;
                float fBranchRatio3 = min_float(fTrueConfidence3, fFalseConfidence3)
                                    / max_float(fTrueConfidence3, fFalseConfidence3);

                if (fTrueConfidence3 > 0.0f)
                {
                    SaveConfidence PushDOM5(&fConfidence);
                    fConfidence = (fConfidence <= fTrueConfidence3) ? fConfidence : fTrueConfidence3;
                    if (fConfidence < fTrueConfidence3 && fTrueConfidence3 < 0.5f)
                    {
                        double d = fConfidence;
                        fConfidence = (float)d * fBranchRatio3;
                    }
                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(theOpponent);
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
 * Offset/Address/Size: 0x8D80 | 0x80072F50 | size: 0x820
 */
FuzzyVariant Fuzzy::GetPassDirection(cPlayer* pFromPlayer, cPlayer* pTargetPlayer)
{

    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    float fTrueConfidence = CloseToTheirNet(pTargetPlayer);
    float fFalseConfidence = 1.0f - fTrueConfidence;
    float fBranchMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fBranchRatio = fBranchMin / ((fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence);

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);
        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
        if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
            fConfidence = fConfidence * fBranchRatio;
        if (fConfidence > fBestConfidence)
        {
            fBestConfidence = fConfidence;
            bestValue = FuzzyVariant(2);
        }
    }
    if (fFalseConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);
        fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
        if ((fConfidence < fFalseConfidence) && (fFalseConfidence < 0.5f))
            fConfidence = fConfidence * fBranchRatio;
        float inDefensiveZone = FGREATER(InDefensiveZone(pTargetPlayer), 0.7f);
        float inOffensiveZone = FGREATER(InOffensiveZone(pTargetPlayer), 0.5f);
        float fTrueConfidence = (inOffensiveZone >= inDefensiveZone) ? inOffensiveZone : inDefensiveZone;
        float fFalseConfidence = 1.0f - fTrueConfidence;
        float fBranchMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fBranchRatio = fBranchMin / ((fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence);
        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);
            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
                fConfidence = fConfidence * fBranchRatio;
            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                bestValue = FuzzyVariant(0);
            }
        }
        if (fFalseConfidence > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);
            fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
            if ((fConfidence < fFalseConfidence) && (fFalseConfidence < 0.5f))
                fConfidence = fConfidence * fBranchRatio;
            float fTrueConfidence = FarTo(pFromPlayer, pTargetPlayer);
            float fFalseConfidence = 1.0f - fTrueConfidence;
            float fBranchMin = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fBranchRatio = fBranchMin / ((fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence);
            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM(&fConfidence);
                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
                    fConfidence = fConfidence * fBranchRatio;
                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    bestValue = FuzzyVariant(3);
                }
            }
            if (fFalseConfidence > 0.0f)
            {
                SaveConfidence PushDOM(&fConfidence);
                fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
                if ((fConfidence < fFalseConfidence) && (fFalseConfidence < 0.5f))
                    fConfidence = fConfidence * fBranchRatio;
                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    bestValue = FuzzyVariant(1);
                }
            }
        }
    }

    bestValue.Confidence = fBestConfidence;
    return bestValue;
}

/**
 * Offset/Address/Size: 0x801C | 0x800721EC | size: 0xD64
 */
FuzzyVariant Fuzzy::GoodToShoot(cFielder* TheFielder)
{
    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    SCRIPT_QUESTION_KEY(fielderFunction, GoodToShoot, (cPlayer*)TheFielder);

    ScriptQuestionCache* const& cache = ScriptQuestionCache::Instance();
    if (cache->Lookup(hash, bestValue, NULL))
    {
        bestValue.Confidence = bestValue.Confidence;
        ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
        return bestValue;
    }

    float fInFrontOfNet = 1.0f - InFrontOfTheirNet(TheFielder);
    float fTrueConfidence = FarToTheirNet((cPlayer*)TheFielder);
    fTrueConfidence = (fTrueConfidence >= fInFrontOfNet) ? fTrueConfidence : fInFrontOfNet;

    float fFalseConfidence = 1.0f - fTrueConfidence;
    float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fBranchRatio = fMinVal / fMaxVal;

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
        if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
        {
            fConfidence = (float)(double)fConfidence * fBranchRatio;
        }

        if (fConfidence > 0.0f)
        {
            fBestConfidence = fConfidence;
            FuzzyVariant fvResult((1.0f - fConfidence) * 0.5f);
            bestValue = fvResult;
        }
    }

    if (fFalseConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
        if ((fConfidence < fFalseConfidence) && (fFalseConfidence < 0.5f))
        {
            fConfidence = (float)(double)fConfidence * fBranchRatio;
        }

        float fNetOpeness = LikelyToScore(TheFielder);
        float fPlayerDistance = PlayerShotDistance(TheFielder);
        float fNetWeighting = g_pGame->m_pGameTweaks->fShotMeterNetOpenWeight;
        float fPlayerWeighting = g_pGame->m_pGameTweaks->fShotMeterPlayerDistanceWeight;
        float fTotalSum = 0.0f;
        float fTotalWeight = 0.0f;
        fTotalSum += fNetOpeness * fNetWeighting;
        fTotalWeight += fNetWeighting;
        fTotalSum += fPlayerDistance * fPlayerWeighting;
        fTotalWeight += fPlayerWeighting;
        fNetOpeness = 0.0f;

        if (fTotalWeight > 0.0f)
        {
            fNetOpeness = fTotalSum / fTotalWeight;
        }

        fNetOpeness = (fNetOpeness >= 0.0f) ? fNetOpeness : 0.0f;
        fNetOpeness = (fNetOpeness <= 1.0f) ? fNetOpeness : 1.0f;

        Goalie* pGoalie;
        if (TheFielder != NULL)
        {
            cTeam* pOtherTeam = (TheFielder != NULL) ? TheFielder->m_pTeam->GetOtherTeam() : NULL;
            pGoalie = pOtherTeam->GetGoalie();
        }
        else
        {
            pGoalie = NULL;
        }

        float fTrueConfidence2 = Stunned(pGoalie);
        float fFalseConfidence2 = 1.0f - fTrueConfidence2;
        float fMinVal2 = (fTrueConfidence2 <= fFalseConfidence2) ? fTrueConfidence2 : fFalseConfidence2;
        float fMaxVal2 = (fTrueConfidence2 >= fFalseConfidence2) ? fTrueConfidence2 : fFalseConfidence2;
        float fBranchRatio2 = fMinVal2 / fMaxVal2;

        if (fTrueConfidence2 > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence2) ? fConfidence : fTrueConfidence2;
            if ((fConfidence < fTrueConfidence2) && (fTrueConfidence2 < 0.5f))
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio2;
            }

            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                FuzzyVariant fvResult(OpenToTheirNet(TheFielder));
                bestValue = fvResult;
            }
        }

        float fTrueConfidence3 = CloseToTheirGoalie((cPlayer*)TheFielder);
        float fFalseConfidence3 = 1.0f - fTrueConfidence3;
        float fMinVal3 = (fTrueConfidence3 <= fFalseConfidence3) ? fTrueConfidence3 : fFalseConfidence3;
        float fMaxVal3 = (fTrueConfidence3 >= fFalseConfidence3) ? fTrueConfidence3 : fFalseConfidence3;
        float fBranchRatio3 = fMinVal3 / fMaxVal3;

        if (fTrueConfidence3 > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence3) ? fConfidence : fTrueConfidence3;
            if ((fConfidence < fTrueConfidence3) && (fTrueConfidence3 < 0.5f))
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio3;
            }

            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                FuzzyVariant fvResult(fNetOpeness);
                bestValue = fvResult;
            }
        }

        if (fFalseConfidence3 > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);

            fConfidence = (fConfidence <= fFalseConfidence3) ? fConfidence : fFalseConfidence3;
            if ((fConfidence < fFalseConfidence3) && (fFalseConfidence3 < 0.5f))
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio3;
            }

            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                float fNearToNet = NearToTheirNet((cPlayer*)TheFielder);
                FuzzyVariant fvResult(WeightedScore2(fNetOpeness, 0.7f, fNearToNet, 0.3f));
                bestValue = fvResult;
            }
        }
    }

    bestValue.Confidence = fBestConfidence;
    ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
    return bestValue;
}

/**
 * Offset/Address/Size: 0x71F4 | 0x800713C4 | size: 0xE28
 */
FuzzyVariant Fuzzy::GoodToChipShot(cFielder* TheFielder)
{

    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;
    float fFalseConfidence;
    float fBranchRatio;

    SCRIPT_QUESTION_KEY(fielderFunction, GoodToChipShot, TheFielder);

    if (ScriptQuestionCache::Instance()->Lookup(hash, bestValue, NULL))
    {
        fBestConfidence = bestValue.Confidence;
        bestValue.Confidence = fBestConfidence;
        ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
        return bestValue;
    }

    float fTrueConfidence = nlMaxFour(
        FarToTheirNet((cPlayer*)TheFielder),
        1.0f - OnScreen((cPlayer*)(TheFielder != NULL ? ((TheFielder != NULL) ? TheFielder->m_pTeam->GetOtherTeam() : NULL)->GetGoalie() : NULL)),
        1.0f - InFrontOfTheirNet(TheFielder),
        ReceivingVolleyPass((cPlayer*)TheFielder));

    fFalseConfidence = 1.0f - fTrueConfidence;
    float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    fBranchRatio = fMinVal / fMaxVal;

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
        if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
        {
            fConfidence = (float)(double)fConfidence * fBranchRatio;
        }

        if (fConfidence > 0.0f)
        {
            fBestConfidence = fConfidence;
            FuzzyVariant fvResult(0.0f);
            bestValue = fvResult;
        }
    }

    if (fFalseConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
        if ((fConfidence < fFalseConfidence) && (fFalseConfidence < 0.5f))
        {
            fConfidence = (float)(double)fConfidence * fBranchRatio;
        }

        float fPositionScore = GoalieOutOfPosition(TheFielder);
        float fNetOpeness = LikelyToScore(TheFielder);
        float fPositionWeighting = g_pGame->m_pGameTweaks->fShotChipMeterGPositionWeight;
        float fNetWeighting = g_pGame->m_pGameTweaks->fShotChipMeterNetOpenWeight;
        float fTotalSum = 0.0f;
        float fTotalWeight = 0.0f;

        fTotalSum += fPositionScore * fPositionWeighting;
        fTotalWeight += fPositionWeighting;
        fTotalSum += fNetOpeness * fNetWeighting;
        fTotalWeight += fNetWeighting;

        fPositionScore = 0.0f;
        if (fTotalWeight > 0.0f)
        {
            fPositionScore = fTotalSum / fTotalWeight;
        }

        fPositionScore = (fPositionScore >= 0.0f) ? fPositionScore : 0.0f;
        fPositionScore = (fPositionScore <= 1.0f) ? fPositionScore : 1.0f;

        Goalie* pGoalieOutOfNet;
        if (TheFielder != NULL)
        {
            cTeam* pOtherTeam = (TheFielder != NULL) ? TheFielder->m_pTeam->GetOtherTeam() : NULL;
            pGoalieOutOfNet = pOtherTeam->GetGoalie();
        }
        else
        {
            pGoalieOutOfNet = NULL;
        }

        float fOutOfNetScore = OutOfNet(pGoalieOutOfNet);

        Goalie* pGoalieStunned;
        if (TheFielder != NULL)
        {
            cTeam* pOtherTeam = (TheFielder != NULL) ? TheFielder->m_pTeam->GetOtherTeam() : NULL;
            pGoalieStunned = pOtherTeam->GetGoalie();
        }
        else
        {
            pGoalieStunned = NULL;
        }

        {
            float fTrueConfidence = Stunned(pGoalieStunned);
            float fFalseConfidence = 1.0f - fTrueConfidence;
            float fMinVal2 = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fMaxVal2 = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
            float fBranchRatio = fMinVal2 / fMaxVal2;

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM(&fConfidence);

                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
                {
                    fConfidence = (float)(double)fConfidence * fBranchRatio;
                }

                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    float fOpenToNet = OpenToTheirNet(TheFielder);
                    FuzzyVariant fvResult(WeightedScore2(fOutOfNetScore, 0.6f, fOpenToNet, 0.4f));
                    bestValue = fvResult;
                }
            }
        }

        float fTrueConfidence3 = CloseToTheirGoalie((cPlayer*)TheFielder);
        float fFalseConfidence3 = 1.0f - fTrueConfidence3;
        float fMinVal3 = (fTrueConfidence3 <= fFalseConfidence3) ? fTrueConfidence3 : fFalseConfidence3;
        float fMaxVal3 = (fTrueConfidence3 >= fFalseConfidence3) ? fTrueConfidence3 : fFalseConfidence3;
        float fBranchRatio3 = fMinVal3 / fMaxVal3;

        if (fTrueConfidence3 > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence3) ? fConfidence : fTrueConfidence3;
            if ((fConfidence < fTrueConfidence3) && (fTrueConfidence3 < 0.5f))
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio3;
            }

            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                FuzzyVariant fvResult(WeightedScore2(fOutOfNetScore, 0.6f, fPositionScore, 0.4f));
                bestValue = fvResult;
            }
        }

        if (fFalseConfidence3 > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);

            fConfidence = (fConfidence <= fFalseConfidence3) ? fConfidence : fFalseConfidence3;
            if ((fConfidence < fFalseConfidence3) && (fFalseConfidence3 < 0.5f))
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio3;
            }

            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                float fNearToNet = NearToTheirNet((cPlayer*)TheFielder);
                FuzzyVariant fvResult(WeightedScore3(fPositionScore, 0.5f, fOutOfNetScore, 0.3f, fNearToNet, 0.2f));
                bestValue = fvResult;
            }
        }
    }

    bestValue.Confidence = fBestConfidence;
    ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);

    return bestValue;
}

/**
 * Offset/Address/Size: 0x57C4 | 0x8006F994 | size: 0x1A30
 */
FuzzyVariant Fuzzy::GetBestPassReceiveAction(cFielder* TheFielder)
{

    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    SCRIPT_QUESTION_KEY_REF(fielderFunction, GetBestPassReceiveAction, (cPlayer*)TheFielder);

    ScriptQuestionCache* const& cache = ScriptQuestionCache::Instance();
    if (cache->Lookup(hash, bestValue, NULL))
    {
        bestValue.Confidence = bestValue.Confidence;
        ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
        return bestValue;
    }

    float fTrueConfidence = nlMinEquals(ReceivingPass(TheFielder), FuzzyNot(UserControlledT(g_pScriptCurrentTeam)));

    float fFalseConfidence1 = 1.0f - fTrueConfidence;
    float fBranchRatio1 = min_float(fTrueConfidence, fFalseConfidence1) / max_float(fTrueConfidence, fFalseConfidence1);

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
        {
            fConfidence = (float)(double)fConfidence * fBranchRatio1;
        }

        float fCaptain = Captain(TheFielder);
        float fOffZone = InOffensiveZone((cPlayer*)TheFielder);
        float fWideOpen = WideOpen(TheFielder);
        float fThreshold = WeightedScore3(1.0f - fWideOpen, 0.7f, fOffZone, 0.2f, fCaptain, 0.1f);
        fTrueConfidence = ReceivingVolleyPassDelayed((cPlayer*)TheFielder);
        fTrueConfidence = (fTrueConfidence <= fThreshold) ? fTrueConfidence : fThreshold;

        float fFalseConfidence2 = 1.0f - fTrueConfidence;
        float fBranchRatio2 = min_float(fTrueConfidence, fFalseConfidence2) / max_float(fTrueConfidence, fFalseConfidence2);

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM2(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio2;
            }

            FuzzyVariant powerupToUse = GetPowerupToUseForPassReceiveDefence(TheFielder);

            float fPowerupConfidence = powerupToUse.Confidence;
            fTrueConfidence = fPowerupConfidence;
            float fFalseConfidence3 = 1.0f - fTrueConfidence;
            float fBranchRatio3 = min_float(fTrueConfidence, fFalseConfidence3) / max_float(fTrueConfidence, fFalseConfidence3);

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM3(&fConfidence);

                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                {
                    fConfidence = (float)(double)fConfidence * fBranchRatio3;
                }

                FuzzyVariant returnAction(18);
                returnAction.ExtraData = (Variant&)powerupToUse;

                SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                returnAction.SelectionChance = CalcSelectChance(pSkillTweaks->Off_PassReceivePowerupChance, Aggressive(TheFielder));

                if (fConfidence > 0.0f)
                {
                    fBestConfidence = fConfidence;
                    bestValue = returnAction;
                }
            }
        }

        cPlayer* pClosestOpponent = TheFielder->GetClosestOpponentFielder(NULL);

        fTrueConfidence = nlMinEqualsThree(OnScreen((cPlayer*)pClosestOpponent),
            NearTo((cPlayer*)TheFielder, (cPlayer*)pClosestOpponent),
            nlMaxEqualsThree(ClosingTo((cPlayer*)TheFielder, (cPlayer*)pClosestOpponent),
                InDanger(TheFielder).GetFloat(),
                FGREATER(NearToBall((cPlayer*)pClosestOpponent), NearToBall((cPlayer*)TheFielder))));
        float fFalseConfidence4 = 1.0f - fTrueConfidence;
        float fBranchRatio4 = min_float(fTrueConfidence, fFalseConfidence4) / max_float(fTrueConfidence, fFalseConfidence4);

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM4(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio4;
            }

            fTrueConfidence = nlMinEqualsFour(OnTheGround((cPlayer*)TheFielder),
                1.0f - BallOwner((cPlayer*)TheFielder),
                WeightedScore2(InDefensiveZone((cPlayer*)TheFielder), 0.65f, ClosingTo((cPlayer*)TheFielder, (cPlayer*)pClosestOpponent), 0.35f),
                nlMaxEquals(CloseTo((cPlayer*)TheFielder, (cPlayer*)pClosestOpponent),
                    AtIdealDistanceForTackling((cPlayer*)TheFielder, (cPlayer*)pClosestOpponent)));
            float fFalseConfidence5 = 1.0f - fTrueConfidence;
            float fBranchRatio5 = min_float(fTrueConfidence, fFalseConfidence5) / max_float(fTrueConfidence, fFalseConfidence5);

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM5(&fConfidence);

                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                {
                    fConfidence = (float)(double)fConfidence * fBranchRatio5;
                }

                FuzzyVariant returnAction(5);
                returnAction.ExtraData = Variant(FT_PLAYER, (cPlayer*)pClosestOpponent);

                SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                returnAction.SelectionChance = CalcSelectChance(pSkillTweaks->Off_PassReceiveHitChance, Aggressive(TheFielder));

                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    bestValue = returnAction;
                }
            }
        }

        FuzzyVariant oneTimerScore = ShouldIAttemptOneTimer(TheFielder);

        fTrueConfidence = FGREATER(PassReceiveCloseToDone(TheFielder), 0.0f);
        float fFalseConfidence6 = 1.0f - fTrueConfidence;
        float fBranchRatio6 = min_float(fTrueConfidence, fFalseConfidence6) / max_float(fTrueConfidence, fFalseConfidence6);

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM6(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio6;
            }

            fTrueConfidence = oneTimerScore.mData.f;
            float fFalseConfidence7 = 1.0f - fTrueConfidence;
            float fBranchRatio7 = min_float(fTrueConfidence, fFalseConfidence7) / max_float(fTrueConfidence, fFalseConfidence7);

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM7(&fConfidence);

                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                {
                    fConfidence = (float)(double)fConfidence * fBranchRatio7;
                }

                FuzzyVariant shotAction(24);
                shotAction.ExtraData = Variant(FT_BOOL, false);
                shotAction.SelectionChance = oneTimerScore.SelectionChance;

                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    bestValue = shotAction;
                }
            }

            float fChipHalf = 0.5f;
            fTrueConfidence = WeightedScore2(oneTimerScore.mData.f, fChipHalf, GoodToChipShot(TheFielder).GetFloat(), fChipHalf);
            float fFalseConfidence8 = 1.0f - fTrueConfidence;
            float fBranchRatio8 = min_float(fTrueConfidence, fFalseConfidence8) / max_float(fTrueConfidence, fFalseConfidence8);

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM8(&fConfidence);

                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                {
                    fConfidence = (float)(double)fConfidence * fBranchRatio8;
                }

                FuzzyVariant shotAction(24);
                shotAction.ExtraData = Variant(FT_BOOL, true);

                SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                shotAction.SelectionChance = CalcSelectChance(pSkillTweaks->Off_ChipShotChance, Shooter(TheFielder));

                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    bestValue = shotAction;
                }
            }
        }

        fTrueConfidence = FGREATER(PassReceiveCloseToDone(TheFielder), 0.3f);
        float fFalseConfidence9 = 1.0f - fTrueConfidence;
        float fBranchRatio9 = min_float(fTrueConfidence, fFalseConfidence9) / max_float(fTrueConfidence, fFalseConfidence9);

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM9(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio9;
            }

            FuzzyVariant bestPassTargetFielder = GetBestPassTarget((cPlayer*)TheFielder);

            FuzzyVariant passAction(13);
            passAction.ExtraData = (Variant&)bestPassTargetFielder;

            SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            passAction.SelectionChance = CalcSelectChance(pSkillTweaks->Off_OneTouchGroundPassChance, Passer(TheFielder));

            fTrueConfidence = ReceivingVolleyPass((cPlayer*)TheFielder);
            float fVolleyFalse = 1.0f - fTrueConfidence;
            float fVolleyRatio = min_float(fTrueConfidence, fVolleyFalse) / max_float(fTrueConfidence, fVolleyFalse);

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM10(&fConfidence);

                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                {
                    fConfidence = (float)(double)fConfidence * fVolleyRatio;
                }

                float fPassConf = nlMinEqualsFour(
                    FGREATER(bestPassTargetFielder.Confidence, 0.0f),
                    1.0f - NearTo(bestPassTargetFielder.GetPlayer(), (cPlayer*)TheFielder),
                    FLESS(oneTimerScore.GetFloat(), 0.5f),
                    nlMaxEqualsThree(1.0f - WideOpen(TheFielder),
                        FGREATER(InDangerDelayed(TheFielder).GetFloat(), 0.0f),
                        nlMinEquals(1.0f - FarTo((cPlayer*)pClosestOpponent, (cPlayer*)TheFielder),
                            ClosingTo((cPlayer*)pClosestOpponent, (cPlayer*)TheFielder))));

                fTrueConfidence = fPassConf;
                float fFalseConfidence10 = 1.0f - fTrueConfidence;
                float fBranchRatio11 = min_float(fTrueConfidence, fFalseConfidence10) / max_float(fTrueConfidence, fFalseConfidence10);

                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM11(&fConfidence);

                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    {
                        fConfidence = (float)(double)fConfidence * fBranchRatio11;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = passAction;
                    }
                }
            }

            if (fVolleyFalse > 0.0f)
            {
                SaveConfidence PushDOM12(&fConfidence);

                fConfidence = (fConfidence <= fVolleyFalse) ? fConfidence : fVolleyFalse;
                if (fConfidence < fVolleyFalse && fVolleyFalse < 0.5f)
                {
                    fConfidence = (float)(double)fConfidence * fVolleyRatio;
                }

                float fNotFarClosing2 = nlMinEquals(1.0f - FarTo((cPlayer*)pClosestOpponent, (cPlayer*)TheFielder),
                    ClosingTo((cPlayer*)pClosestOpponent, (cPlayer*)TheFielder));
                float fDanger2 = nlMaxEquals(InDangerDelayed(TheFielder).GetFloat(), fNotFarClosing2);
                fTrueConfidence = nlMinEqualsThree(
                    FGREATER(bestPassTargetFielder.Confidence, 0.3f),
                    1.0f - NearTo(bestPassTargetFielder.GetPlayer(), (cPlayer*)TheFielder),
                    fDanger2);
                float fFalseConfidence11 = 1.0f - fTrueConfidence;
                float fBranchRatio12 = min_float(fTrueConfidence, fFalseConfidence11) / max_float(fTrueConfidence, fFalseConfidence11);

                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM13(&fConfidence);

                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    {
                        fConfidence = (float)(double)fConfidence * fBranchRatio12;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = passAction;
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
 * Offset/Address/Size: 0x3FD0 | 0x8006E1A0 | size: 0x17F4
 */
FuzzyVariant Fuzzy::GetBestLooseBallAction(cFielder* TheFielder)
{
    FORCE_DONT_INLINE;

    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    SCRIPT_QUESTION_KEY_REF(fielderFunction, GetBestLooseBallAction, (cPlayer*)TheFielder);

    ScriptQuestionCache* const& cache = ScriptQuestionCache::Instance();
    if (cache->Lookup(hash, bestValue, NULL))
    {
        ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
        return bestValue;
    }

    float fDifficulty = NormalizeVal(Difficult(g_pScriptCurrentTeam), 1.0f, 0.0f);
    float fTrueConfidence = 1.0f - UserControlledT(g_pScriptCurrentTeam);
    float fFalseConfidence = 1.0f - fTrueConfidence;
    float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fBranchRatio = fMinVal / fMaxVal;

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
        {
            fConfidence = (float)(double)fConfidence * fBranchRatio;
        }

        float fCanSlide = TheFielder->CanISlideAttack(g_pScriptBall->GetPosition(), g_pScriptBall->GetVelocity(), NULL) ? 1.0f : 0.0f;

        float fNotOpenMax = nlMaxFour(Pressured(TheFielder), 1.0f - Open(TheFielder), 1.0f - Ownerless(g_pScriptBall), OnMushrooms(TheFielder));

        fCanSlide = nlMinThree(fCanSlide,
            max_float(1.0f - FacingSideline(TheFielder), OnMushrooms(TheFielder)),
            fNotOpenMax);

        fTrueConfidence = fCanSlide;
        fFalseConfidence = 1.0f - fTrueConfidence;
        if (fTrueConfidence <= fFalseConfidence)
        {
            fMinVal = fTrueConfidence;
        }
        else
        {
            fMinVal = fFalseConfidence;
        }
        fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        fBranchRatio = fMinVal / fMaxVal;

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM2(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio;
            }

            SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            float fSelectChance = CalcSelectChance(pSkillTweaks->Def_SlideAttackChance, Aggressive(TheFielder));
            float fStallingGreater = FGREATER(Stalling(g_pScriptOtherTeam), fDifficulty);
            if (fStallingGreater >= fSelectChance)
            {
                fSelectChance = fStallingGreater;
            }

            FuzzyVariant returnAction(15);
            returnAction.SelectionChance = fSelectChance;

            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                bestValue = returnAction;
            }
        }

        cTeam* otherTeam = TheFielder ? ((cPlayer*)TheFielder)->m_pTeam->GetOtherTeam() : NULL;
        FuzzyVariant otherSBC = Fuzzy::GetStrategicBallCarrier(otherTeam);

        float fTrueConfidence3 = nlMinFour(
            1.0f - FarToBall((cPlayer*)TheFielder),
            OnScreen(otherSBC.mData.pPlayer),
            1.0f - SeparatingFrom((cPlayer*)TheFielder, otherSBC.mData.pPlayer),
            nlMaxEquals(AtIdealDistanceForTackling((cPlayer*)TheFielder, otherSBC.mData.pPlayer),
                CloseTo((cPlayer*)TheFielder, otherSBC.mData.pPlayer)));

        float fFalseConfidence3 = 1.0f - fTrueConfidence3;
        float fBranchRatio3 = min_float(fTrueConfidence3, fFalseConfidence3)
                            / max_float(fTrueConfidence3, fFalseConfidence3);

        if (fTrueConfidence3 > 0.0f)
        {
            SaveConfidence PushDOM3(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence3) ? fConfidence : fTrueConfidence3;
            if (fConfidence < fTrueConfidence3 && fTrueConfidence3 < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio3;
            }

            SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            float fSelectChance = CalcSelectChance(pSkillTweaks->Loose_HeavyAttackChance, Aggressive(TheFielder));
            float fStallingGreater = FGREATER(Stalling(g_pScriptOtherTeam), fDifficulty);
            if (fStallingGreater >= fSelectChance)
            {
                fSelectChance = fStallingGreater;
            }

            FuzzyVariant hitAction(5);
            hitAction.ExtraData = *(Variant*)&otherSBC;
            hitAction.SelectionChance = fSelectChance;

            if (BallOwner(otherSBC.mData.pPlayer))
            {
                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    bestValue = hitAction;
                }
            }
            else
            {
                if (WindingUpForShot((cFielder*)otherSBC.mData.pPlayer))
                {
                    float fLosing = Losing(TheFielder ? ((cPlayer*)TheFielder)->m_pTeam : NULL);
                    float fTimeNearly = TimeNearlyOver(g_pGame);
                    if (fTimeNearly <= fLosing)
                    {
                        fLosing = fTimeNearly;
                    }

                    float fRandom = RandomChance(0.5f);
                    fRandom = (fRandom >= fLosing) ? fRandom : fLosing;

                    float fHalf = 0.5f;
                    float fTrueConfidence4 = fRandom * fHalf + fHalf;
                    float fFalseConfidence4 = 1.0f - fTrueConfidence4;
                    float fMinVal4 = (fTrueConfidence4 <= fFalseConfidence4) ? fTrueConfidence4 : fFalseConfidence4;
                    float fMaxVal4 = (fTrueConfidence4 >= fFalseConfidence4) ? fTrueConfidence4 : fFalseConfidence4;
                    float fBranchRatio4 = fMinVal4 / fMaxVal4;

                    if (fTrueConfidence4 > 0.0f)
                    {
                        SaveConfidence PushDOM4(&fConfidence);

                        fConfidence = (fConfidence <= fTrueConfidence4) ? fConfidence : fTrueConfidence4;
                        if (fConfidence < fTrueConfidence4 && fTrueConfidence4 < 0.5f)
                        {
                            fConfidence = (float)(double)fConfidence * fBranchRatio4;
                        }

                        if (fConfidence > fBestConfidence)
                        {
                            fBestConfidence = fConfidence;
                            bestValue = hitAction;
                        }
                    }
                }
                else
                {
                    float fChasing = nlMaxEquals(ChasingBall(otherSBC.mData.pPlayer), ReceivingPassDelayed((cFielder*)otherSBC.mData.pPlayer));

                    if (fChasing)
                    {
                        cTeam* fielderTeam = TheFielder ? ((cPlayer*)TheFielder)->m_pTeam : NULL;
                        float fLosing = Losing(fielderTeam);
                        float fTimeNearly = TimeNearlyOver(g_pGame);
                        if (fTimeNearly <= fLosing)
                        {
                            fLosing = fTimeNearly;
                        }

                        float fRandom = RandomChance(0.4f);
                        if (fRandom >= fLosing)
                        {
                            fLosing = fRandom;
                        }

                        float fDefZone = InDefensiveZoneOfPlayer(g_pScriptBall, (cPlayer*)TheFielder);
                        cPlayer* pOtherPlayer = otherSBC.GetPlayer();
                        float fGreater = FGREATER(NearToBall(pOtherPlayer), NearToBall((cPlayer*)TheFielder));

                        if (fGreater >= fDefZone)
                        {
                            fDefZone = fGreater;
                        }

                        float fAbleToIntercept = AbleToInterceptBall(otherSBC.mData.pPlayer);
                        float fDefWeight = 0.3f;
                        float fLoseWeight = 0.4f;
                        float fWeightedDefZone = fDefZone * fDefWeight;
                        float fTrueConfidence5 = fWeightedDefZone + fAbleToIntercept * fDefWeight + fLosing * fLoseWeight;
                        float fFalseConfidence5 = 1.0f - fTrueConfidence5;
                        float fMinVal5 = (fTrueConfidence5 <= fFalseConfidence5) ? fTrueConfidence5 : fFalseConfidence5;
                        float fMaxVal5 = (fTrueConfidence5 >= fFalseConfidence5) ? fTrueConfidence5 : fFalseConfidence5;
                        float fBranchRatio5 = fMinVal5 / fMaxVal5;

                        if (fTrueConfidence5 > 0.0f)
                        {
                            SaveConfidence PushDOM5(&fConfidence);

                            fConfidence = (fConfidence <= fTrueConfidence5) ? fConfidence : fTrueConfidence5;
                            if (fConfidence < fTrueConfidence5 && fTrueConfidence5 < 0.5f)
                            {
                                fConfidence = (float)(double)fConfidence * fBranchRatio5;
                            }

                            if (fConfidence > fBestConfidence)
                            {
                                fBestConfidence = fConfidence;
                                bestValue = hitAction;
                            }
                        }
                    }
                }
            }
        }

        FuzzyVariant powerupToUse = Fuzzy::GetPowerupToUseForPassReceiveDefence(TheFielder);

        FuzzyVariant returnAction2(18);
        returnAction2.ExtraData = *(Variant*)&powerupToUse;

        SkillTweaks* pSkillTweaks2 = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
        returnAction2.SelectionChance = CalcSelectChance(pSkillTweaks2->Off_PassReceivePowerupChance, Aggressive(TheFielder));
        float fPowerupConfidence = powerupToUse.Confidence;

        if (powerupToUse.mData.i == 7)
        {
            fTrueConfidence = 1.0f;
        }
        else
        {
            fTrueConfidence = 0.0f;
        }
        fTrueConfidence = (fTrueConfidence <= fPowerupConfidence) ? fTrueConfidence : fPowerupConfidence;

        fFalseConfidence = 1.0f - fTrueConfidence;
        fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        fBranchRatio = fMinVal / fMaxVal;

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM6(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio;
            }

            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                bestValue = returnAction2;
            }
        }

        if (fFalseConfidence > 0.0f)
        {
            SaveConfidence PushDOM7(&fConfidence);

            fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
            if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio;
            }

            float fNotDefZone = 1.0f - InDefensiveZone(otherSBC.mData.pPlayer);

            float fNotBestConf = nlMinFour(powerupToUse.Confidence, FuzzyNot(fBestConfidence), 1.0f - CloseTo((cPlayer*)TheFielder, otherSBC.mData.pPlayer), fNotDefZone);

            float fFalseConfidence7 = 1.0f - fNotBestConf;
            float fMinVal7 = (fNotBestConf <= fFalseConfidence7) ? fNotBestConf : fFalseConfidence7;
            float fMaxVal7 = (fNotBestConf >= fFalseConfidence7) ? fNotBestConf : fFalseConfidence7;
            float fBranchRatio7 = fMinVal7 / fMaxVal7;

            if (fNotBestConf > 0.0f)
            {
                SaveConfidence PushDOM7b(&fConfidence);

                fConfidence = (fConfidence <= fNotBestConf) ? fConfidence : fNotBestConf;
                if (fConfidence < fNotBestConf && fNotBestConf < 0.5f)
                {
                    fConfidence = (float)(double)fConfidence * fBranchRatio7;
                }

                float fTrueConfidence8 = nlMaxFour((powerupToUse.mData.i == 8) ? 1.0f : 0.0f,
                    nlMinThree(InOffensiveZone((cPlayer*)TheFielder), Captain(TheFielder), AbleToInterceptBall((cPlayer*)TheFielder)),
                    nlMinEquals(InDefensiveZone((cPlayer*)TheFielder), GonnaGetBall(TheFielder ? ((cPlayer*)TheFielder)->m_pTeam->GetOtherTeam() : NULL)),
                    nlMinEquals(WindingUpForShot((cFielder*)otherSBC.mData.pPlayer), Marking(TheFielder, otherSBC.mData.pPlayer)));
                float fFalseConfidence8 = 1.0f - fTrueConfidence8;
                float fMinVal8 = (fTrueConfidence8 <= fFalseConfidence8) ? fTrueConfidence8 : fFalseConfidence8;
                float fMaxVal8 = (fTrueConfidence8 >= fFalseConfidence8) ? fTrueConfidence8 : fFalseConfidence8;
                float fBranchRatio8 = fMinVal8 / fMaxVal8;

                if (fTrueConfidence8 > 0.0f)
                {
                    SaveConfidence PushDOM8(&fConfidence);

                    fConfidence = (fConfidence <= fTrueConfidence8) ? fConfidence : fTrueConfidence8;
                    if (fConfidence < fTrueConfidence8 && fTrueConfidence8 < 0.5f)
                    {
                        fConfidence = (float)(double)fConfidence * fBranchRatio8;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = returnAction2;
                    }
                }
            }
        }

        FuzzyVariant oneTimerScore = Fuzzy::ShouldIAttemptOneTimer(TheFielder);

        float fCanShoot = TheFielder->CanLooseBallShoot() ? 1.0f : 0.0f;
        if (oneTimerScore.mData.f <= fCanShoot)
        {
            fTrueConfidence = oneTimerScore.mData.f;
        }
        else
        {
            fTrueConfidence = fCanShoot;
        }

        float fFalseConfidence9 = 1.0f - fTrueConfidence;
        float fMinVal9 = (fTrueConfidence <= fFalseConfidence9) ? fTrueConfidence : fFalseConfidence9;
        float fMaxVal9 = (fTrueConfidence >= fFalseConfidence9) ? fTrueConfidence : fFalseConfidence9;
        float fBranchRatio9 = fMinVal9 / fMaxVal9;

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM9(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio9;
            }

            FuzzyVariant shotAction(14);
            shotAction.ExtraData = Variant(FT_BOOL, false);
            shotAction.SelectionChance = oneTimerScore.SelectionChance;

            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                bestValue = shotAction;
            }
        }

        float fChipHalf = 0.5f;
        float fTrueConfidence10 = WeightedScore2(oneTimerScore.mData.f, fChipHalf, Fuzzy::GoodToChipShot(TheFielder).GetFloat(), fChipHalf);
        float fCanShoot2 = TheFielder->CanLooseBallShoot() ? 1.0f : 0.0f;
        if (fCanShoot2 <= fTrueConfidence10)
        {
            fTrueConfidence = fCanShoot2;
        }
        else
        {
            fTrueConfidence = fTrueConfidence10;
        }

        float fFalseConfidence10 = 1.0f - fTrueConfidence;
        float fMinVal10 = (fTrueConfidence <= fFalseConfidence10) ? fTrueConfidence : fFalseConfidence10;
        float fMaxVal10 = (fTrueConfidence >= fFalseConfidence10) ? fTrueConfidence : fFalseConfidence10;
        float fBranchRatio10 = fMinVal10 / fMaxVal10;

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOMa(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio10;
            }

            FuzzyVariant shotAction2(14);
            shotAction2.ExtraData = Variant(FT_BOOL, true);

            SkillTweaks* pSkillTweaks3 = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            shotAction2.SelectionChance = CalcSelectChance(pSkillTweaks3->Off_ChipShotChance, Shooter(TheFielder));

            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                bestValue = shotAction2;
            }
        }

        FuzzyVariant bestPassTargetFielder = Fuzzy::GetBestLooseBallPassTarget(TheFielder);

        float fCanPass = nlMinFour(TheFielder->CanLooseBallPass() ? 1.0f : 0.0f,
            FGREATER(bestPassTargetFielder.Confidence, 0.3f),
            FuzzyNot(CloseTo(bestPassTargetFielder.GetPlayer(), (cPlayer*)TheFielder)),
            nlMaxEquals(Fuzzy::InDangerDelayed(TheFielder).mData.f,
                nlMinEquals(NearTo(otherSBC.mData.pPlayer, (cPlayer*)TheFielder),
                    ClosingTo(otherSBC.mData.pPlayer, (cPlayer*)TheFielder))));

        float fFalseConfidence11 = 1.0f - fCanPass;
        float fBranchRatio11 = min_float(fCanPass, fFalseConfidence11) / max_float(fCanPass, fFalseConfidence11);

        if (fCanPass > 0.0f)
        {
            SaveConfidence PushDOMb(&fConfidence);

            fConfidence = (fConfidence <= fCanPass) ? fConfidence : fCanPass;
            if (fConfidence < fCanPass && fCanPass < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio11;
            }

            FuzzyVariant passAction(13);
            passAction.ExtraData = *(Variant*)&bestPassTargetFielder;

            SkillTweaks* pSkillTweaks4 = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            passAction.SelectionChance = CalcSelectChance(pSkillTweaks4->Loose_GroundPassChance, Passer(TheFielder));

            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                bestValue = passAction;
            }
        }
    }

    bestValue.Confidence = fBestConfidence;
    ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
    return bestValue;
}

static inline float GetWindupBaseConfidence(cFielder* TheFielder)
{
    float likelyToScore = LikelyToScore(TheFielder);
    Goalie* goalie;
    if (TheFielder != NULL)
    {
        cTeam* otherTeam;
        if (TheFielder != NULL)
        {
            otherTeam = TheFielder->m_pTeam->GetOtherTeam();
        }
        else
        {
            otherTeam = NULL;
        }
        goalie = otherTeam->GetGoalie();
    }
    else
    {
        goalie = NULL;
    }
    float goalieStunned = Stunned(goalie);
    return (goalieStunned >= likelyToScore) ? goalieStunned : likelyToScore;
}

static inline float GetWindupDekeConfidence(float attacked, float needDeke, float notRepeating, float notCloseGoalie, float notCloseSideline)
{
    attacked = (attacked >= needDeke) ? attacked : needDeke;
    notCloseGoalie = (notCloseGoalie <= notCloseSideline) ? notCloseGoalie : notCloseSideline;
    notRepeating = (notRepeating <= notCloseGoalie) ? notRepeating : notCloseGoalie;
    attacked = (attacked <= notRepeating) ? attacked : notRepeating;
    return attacked;
}

/**
 * Offset/Address/Size: 0x2CB4 | 0x8006CE84 | size: 0x131C
 */
FuzzyVariant Fuzzy::GetBestWindupShotAction(cFielder* TheFielder)
{
    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    SCRIPT_QUESTION_KEY_REF(fielderFunction, GetBestWindupShotAction, (cPlayer*)TheFielder);

    ScriptQuestionCache* const lookupCache = ScriptQuestionCache::Instance();
    if (lookupCache->Lookup(hash, bestValue, NULL))
    {
        fBestConfidence = bestValue.Confidence;
        bestValue.Confidence = fBestConfidence;
        ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
        return bestValue;
    }

    float fTrueConfidence = GetWindupBaseConfidence(TheFielder);

    float fFalseConfidence = 1.0f - fTrueConfidence;
    float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fBranchRatio = fMinVal / fMaxVal;

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
        if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
        {
            double d = fConfidence;
            fConfidence = (float)d * fBranchRatio;
        }

        if (fConfidence > 0.0f)
        {
            fBestConfidence = fConfidence;
            bestValue = FuzzyVariant(14);
        }
    }

    {
        fTrueConfidence = 1.0f - Invincible(TheFielder);
        float fFalseConfidence = 1.0f - fTrueConfidence;
        float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
        float fBranchRatio = fMinVal / fMaxVal;

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
            {
                double d = fConfidence;
                fConfidence = (float)d * fBranchRatio;
            }

            {
                float fCaptain = Captain(TheFielder);
                float fOpenToTheirNet = OpenToTheirNet(TheFielder);
                float fClosedToTheirNet = 1.0f - fOpenToTheirNet;
                float fOpen = Open(TheFielder);
                float fEdgeWeight = 0.2f;
                float fCenterWeight = 0.6f;
                fTrueConfidence = (1.0f - fOpen) * fCenterWeight + fClosedToTheirNet * fEdgeWeight + fCaptain * fEdgeWeight;
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMinVal / fMaxVal;

                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);

                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
                    {
                        double d = fConfidence;
                        fConfidence = (float)d * fBranchRatio;
                    }

                    {
                        FuzzyVariant powerupToUse = Fuzzy::GetPowerupToUseForWindupDefence(TheFielder);

                        float fPowerupConfidence = powerupToUse.Confidence;
                        fTrueConfidence = fPowerupConfidence;
                        float fFalseConfidence = 1.0f - fTrueConfidence;
                        float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                        float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                        float fBranchRatio = fMinVal / fMaxVal;

                        if (fTrueConfidence > 0.0f)
                        {
                            SaveConfidence PushDOM(&fConfidence);

                            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                            if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
                            {
                                double d = fConfidence;
                                fConfidence = (float)d * fBranchRatio;
                            }

                            FuzzyVariant returnAction(18);
                            returnAction.ExtraData = (Variant&)powerupToUse;

                            SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                            returnAction.SelectionChance = CalcSelectChance(pSkillTweaks->Off_WindupPowerupChance, Aggressive(TheFielder));

                            if (fConfidence > fBestConfidence)
                            {
                                fBestConfidence = fConfidence;
                                bestValue = returnAction;
                            }
                        }
                    }
                }
            }

            {
                fTrueConfidence = Fuzzy::InDangerDelayed(TheFielder).mData.f;
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                float fBranchRatio = fMinVal / fMaxVal;

                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);

                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                    if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
                    {
                        double d = fConfidence;
                        fConfidence = (float)d * fBranchRatio;
                    }

                    {
                        FuzzyVariant bestPassTargetFielder = Fuzzy::GetBestPassTarget((cPlayer*)TheFielder);

                        fTrueConfidence = FGREATER(bestPassTargetFielder.Confidence, 0.3f);
                        float fFalseConfidence = 1.0f - fTrueConfidence;
                        float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                        float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                        float fBranchRatio = fMinVal / fMaxVal;

                        if (fTrueConfidence > 0.0f)
                        {
                            SaveConfidence PushDOM(&fConfidence);

                            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                            if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
                            {
                                double d = fConfidence;
                                fConfidence = (float)d * fBranchRatio;
                            }

                            FuzzyVariant passAction(13);
                            passAction.ExtraData = (Variant&)bestPassTargetFielder;

                            SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                            passAction.SelectionChance = CalcSelectChance(pSkillTweaks->Off_WindupPassChance, Passer(TheFielder));

                            if (fConfidence > fBestConfidence)
                            {
                                fBestConfidence = fConfidence;
                                bestValue = passAction;
                            }
                        }
                    }

                    {
                        fTrueConfidence = GetWindupDekeConfidence(Attacked(TheFielder), FLESS(Open(TheFielder), 0.5f), 1.0f - RepeatingLastDesire(TheFielder, (eScriptFielderDesire)2), 1.0f - CloseToTheirGoalie((cPlayer*)TheFielder), 1.0f - CloseToSideline(TheFielder));
                        float fFalseConfidence = 1.0f - fTrueConfidence;
                        float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                        float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                        float fBranchRatio = fMinVal / fMaxVal;

                        if (fTrueConfidence > 0.0f)
                        {
                            SaveConfidence PushDOM(&fConfidence);

                            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                            if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
                            {
                                double d = fConfidence;
                                fConfidence = (float)d * fBranchRatio;
                            }

                            FuzzyVariant returnAction(2);

                            SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
                            returnAction.SelectionChance = CalcSelectChance(pSkillTweaks->Off_WindupDekeChance, Deker(TheFielder));

                            if (fConfidence > fBestConfidence)
                            {
                                fBestConfidence = fConfidence;
                                bestValue = returnAction;
                            }
                        }
                    }

                    {
                        float fNearToTheirGoalie = NearToTheirGoalie((cPlayer*)TheFielder);
                        fTrueConfidence = fConfidence;
                        fTrueConfidence = (fTrueConfidence >= fNearToTheirGoalie) ? fTrueConfidence : fNearToTheirGoalie;

                        float fNoBestConfidence = FGREATER(1.0f - fBestConfidence, 0.5f);
                        fNoBestConfidence = (fNoBestConfidence <= fTrueConfidence) ? fNoBestConfidence : fTrueConfidence;

                        fTrueConfidence = fNoBestConfidence;
                        float fFalseConfidence = 1.0f - fTrueConfidence;
                        float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                        float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
                        float fBranchRatio = fMinVal / fMaxVal;

                        if (fTrueConfidence > 0.0f)
                        {
                            SaveConfidence PushDOM(&fConfidence);

                            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
                            if ((fConfidence < fTrueConfidence) && (fTrueConfidence < 0.5f))
                            {
                                double d = fConfidence;
                                fConfidence = (float)d * fBranchRatio;
                            }

                            if (fConfidence > fBestConfidence)
                            {
                                fBestConfidence = fConfidence;
                                bestValue = FuzzyVariant(14);
                            }
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
 * Offset/Address/Size: 0x1A48 | 0x8006BC18 | size: 0x126C
 */
FuzzyVariant Fuzzy::GetPowerupToUseForPassReceiveDefence(cFielder* TheFielder)
{

    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    SCRIPT_QUESTION_KEY(fielderFunction, GetPowerupToUseForPassReceiveDefence, (cPlayer*)TheFielder);

    float fTrueConfidence = nlMinFour(
        FuzzyNot(Fuzzy::GoalieAndGonnaPickupBall(
            TheFielder != NULL
                ? ((TheFielder != NULL) ? TheFielder->m_pTeam : NULL)->GetGoalie()
                : NULL)),
        FuzzyNot(Fuzzy::GoalieAndGonnaPickupBall(
            TheFielder != NULL
                ? ((TheFielder != NULL) ? TheFielder->m_pTeam->GetOtherTeam() : NULL)->GetGoalie()
                : NULL)),
        FuzzyNot(UserControlledT((TheFielder != NULL) ? TheFielder->m_pTeam : NULL)),
        OnScreen((cPlayer*)TheFielder));

    float fFalseConfidence = 1.0f - fTrueConfidence;
    float fBranchRatio = min_float(fTrueConfidence, fFalseConfidence)
                       / max_float(fTrueConfidence, fFalseConfidence);

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
        {
            double d = fConfidence;
            fConfidence = (float)d * fBranchRatio;
        }

        float fLikelyConfidence = LikelyToUsePowerup(TheFielder, 0);
        float fLikelyFalseConfidence = 1.0f - fLikelyConfidence;
        float fLikelyBranchRatio = min_float(fLikelyConfidence, fLikelyFalseConfidence)
                                 / max_float(fLikelyConfidence, fLikelyFalseConfidence);

        if (fLikelyConfidence > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);

            fConfidence = (fConfidence <= fLikelyConfidence) ? fConfidence : fLikelyConfidence;

            if (fConfidence < fLikelyConfidence && fLikelyConfidence < 0.5f)
            {
                double d = fConfidence;
                fConfidence = (float)d * fLikelyBranchRatio;
            }

            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                bestValue = FuzzyVariant(0);
            }
        }

        {
            float fLikelyConfidence = LikelyToUsePowerup(TheFielder, 1);
            float fLikelyFalseConfidence = 1.0f - fLikelyConfidence;
            float fLikelyBranchRatio = min_float(fLikelyConfidence, fLikelyFalseConfidence)
                                     / max_float(fLikelyConfidence, fLikelyFalseConfidence);

            if (fLikelyConfidence > 0.0f)
            {
                SaveConfidence PushDOM(&fConfidence);

                fConfidence = (fConfidence <= fLikelyConfidence) ? fConfidence : fLikelyConfidence;

                if (fConfidence < fLikelyConfidence && fLikelyConfidence < 0.5f)
                {
                    double d = fConfidence;
                    fConfidence = (float)d * fLikelyBranchRatio;
                }

                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    bestValue = FuzzyVariant(1);
                }
            }
        }

        {
            float fLikelyConfidence = LikelyToUsePowerup(TheFielder, 2);
            float fLikelyFalseConfidence = 1.0f - fLikelyConfidence;
            float fLikelyBranchRatio = min_float(fLikelyConfidence, fLikelyFalseConfidence)
                                     / max_float(fLikelyConfidence, fLikelyFalseConfidence);

            if (fLikelyConfidence > 0.0f)
            {
                SaveConfidence PushDOM(&fConfidence);

                fConfidence = (fConfidence <= fLikelyConfidence) ? fConfidence : fLikelyConfidence;

                if (fConfidence < fLikelyConfidence && fLikelyConfidence < 0.5f)
                {
                    double d = fConfidence;
                    fConfidence = (float)d * fLikelyBranchRatio;
                }

                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    bestValue = FuzzyVariant(2);
                }
            }
        }

        {
            float fLikelyConfidence = LikelyToUsePowerup(TheFielder, 3);
            float fLikelyFalseConfidence = 1.0f - fLikelyConfidence;
            float fLikelyBranchRatio = min_float(fLikelyConfidence, fLikelyFalseConfidence)
                                     / max_float(fLikelyConfidence, fLikelyFalseConfidence);

            if (fLikelyConfidence > 0.0f)
            {
                SaveConfidence PushDOM(&fConfidence);

                fConfidence = (fConfidence <= fLikelyConfidence) ? fConfidence : fLikelyConfidence;

                if (fConfidence < fLikelyConfidence && fLikelyConfidence < 0.5f)
                {
                    double d = fConfidence;
                    fConfidence = (float)d * fLikelyBranchRatio;
                }

                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    bestValue = FuzzyVariant(3);
                }
            }
        }

        {
            float fLikelyConfidence = LikelyToUsePowerup(TheFielder, 4);
            float fLikelyFalseConfidence = 1.0f - fLikelyConfidence;
            float fLikelyBranchRatio = min_float(fLikelyConfidence, fLikelyFalseConfidence)
                                     / max_float(fLikelyConfidence, fLikelyFalseConfidence);

            if (fLikelyConfidence > 0.0f)
            {
                SaveConfidence PushDOM(&fConfidence);

                fConfidence = (fConfidence <= fLikelyConfidence) ? fConfidence : fLikelyConfidence;

                if (fConfidence < fLikelyConfidence && fLikelyConfidence < 0.5f)
                {
                    double d = fConfidence;
                    fConfidence = (float)d * fLikelyBranchRatio;
                }

                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    bestValue = FuzzyVariant(4);
                }
            }
        }

        {
            float fLikelyConfidence = LikelyToUsePowerup(TheFielder, 5);
            float fLikelyFalseConfidence = 1.0f - fLikelyConfidence;
            float fLikelyBranchRatio = min_float(fLikelyConfidence, fLikelyFalseConfidence)
                                     / max_float(fLikelyConfidence, fLikelyFalseConfidence);

            if (fLikelyConfidence > 0.0f)
            {
                SaveConfidence PushDOM(&fConfidence);

                fConfidence = (fConfidence <= fLikelyConfidence) ? fConfidence : fLikelyConfidence;

                if (fConfidence < fLikelyConfidence && fLikelyConfidence < 0.5f)
                {
                    double d = fConfidence;
                    fConfidence = (float)d * fLikelyBranchRatio;
                }

                if (fConfidence > fBestConfidence)
                {
                    fBestConfidence = fConfidence;
                    bestValue = FuzzyVariant(5);
                }
            }
        }

        {
            fTrueConfidence = nlMinThree(ChasingBall((cPlayer*)TheFielder),
                FuzzyNot(NearToBall((cPlayer*)TheFielder)),
                FuzzyNot(High(g_pBall)));

            float fFalseConfidence = 1.0f - fTrueConfidence;
            float fBranchRatio = min_float(fTrueConfidence, fFalseConfidence)
                               / max_float(fTrueConfidence, fFalseConfidence);

            if (fTrueConfidence > 0.0f)
            {
                SaveConfidence PushDOM(&fConfidence);

                fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                {
                    double d = fConfidence;
                    fConfidence = (float)d * fBranchRatio;
                }

                fTrueConfidence = 1.0f - OnMushrooms(g_pScriptCurrentFielder);
                float fFalseConfidence = 1.0f - fTrueConfidence;
                float fBranchRatio = min_float(fTrueConfidence, fFalseConfidence)
                                   / max_float(fTrueConfidence, fFalseConfidence);

                if (fTrueConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);

                    fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

                    if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
                    {
                        double d = fConfidence;
                        fConfidence = (float)d * fBranchRatio;
                    }

                    {
                        float fLikelyConfidence = LikelyToUsePowerup(TheFielder, 7);
                        float fLikelyFalseConfidence = 1.0f - fLikelyConfidence;
                        float fLikelyBranchRatio = min_float(fLikelyConfidence, fLikelyFalseConfidence)
                                                 / max_float(fLikelyConfidence, fLikelyFalseConfidence);

                        if (fLikelyConfidence > 0.0f)
                        {
                            SaveConfidence PushDOM(&fConfidence);

                            fConfidence = (fConfidence <= fLikelyConfidence) ? fConfidence : fLikelyConfidence;

                            if (fConfidence < fLikelyConfidence && fLikelyConfidence < 0.5f)
                            {
                                double d = fConfidence;
                                fConfidence = (float)d * fLikelyBranchRatio;
                            }

                            if (fConfidence > fBestConfidence)
                            {
                                fBestConfidence = fConfidence;
                                bestValue = FuzzyVariant(7);
                            }
                        }
                    }
                }
            }
        }

        fTrueConfidence = nlMinThree(nlMaxEquals(BallOwner((cPlayer*)TheFielder), ReceivingPass(TheFielder)),
            Captain(TheFielder),
            FuzzyNot(InDefensiveZone((cPlayer*)TheFielder)));

        float fFalseConfidence = 1.0f - fTrueConfidence;
        float fBranchRatio = min_float(fTrueConfidence, fFalseConfidence)
                           / max_float(fTrueConfidence, fFalseConfidence);

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);

            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                double d = fConfidence;
                fConfidence = (float)d * fBranchRatio;
            }

            {
                float fLikelyConfidence = LikelyToUsePowerup(TheFielder, 8);
                float fLikelyFalseConfidence = 1.0f - fLikelyConfidence;
                float fLikelyBranchRatio = min_float(fLikelyConfidence, fLikelyFalseConfidence)
                                         / max_float(fLikelyConfidence, fLikelyFalseConfidence);

                if (fLikelyConfidence > 0.0f)
                {
                    SaveConfidence PushDOM(&fConfidence);

                    fConfidence = (fConfidence <= fLikelyConfidence) ? fConfidence : fLikelyConfidence;

                    if (fConfidence < fLikelyConfidence && fLikelyConfidence < 0.5f)
                    {
                        double d = fConfidence;
                        fConfidence = (float)d * fLikelyBranchRatio;
                    }

                    if (fConfidence > fBestConfidence)
                    {
                        fBestConfidence = fConfidence;
                        bestValue = FuzzyVariant(8);
                    }
                }
            }
        }
    }

    bestValue.Confidence = fBestConfidence;
    return bestValue;
}

/**
 * Offset/Address/Size: 0x1620 | 0x8006B7F0 | size: 0x428
 */
FuzzyVariant Fuzzy::GetPowerupToUseForWindupDefence(cFielder* TheFielder)
{
    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    SCRIPT_QUESTION_KEY_REF(fielderFunction, GetPowerupToUseForWindupDefence, (cPlayer*)TheFielder);

    FuzzyVariant usePowerup = Fuzzy::GetPowerupToUseForPassReceiveDefence(TheFielder);

    float fTrueConfidence = usePowerup.Confidence;
    float fFalseConfidence = 1.0f - fTrueConfidence;

    float minC = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float maxC = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fBranchRatio = minC / maxC;

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);

        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;

        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
        {
            double d = fConfidence;
            fConfidence = (float)d * fBranchRatio;
        }

        if (fConfidence > 0.0f)
        {
            fBestConfidence = fConfidence;
            bestValue = usePowerup;
        }
    }

    bestValue.Confidence = fBestConfidence;
    return bestValue;
}

/**
 * Offset/Address/Size: 0xE64 | 0x8006B034 | size: 0x7BC
 */
FuzzyVariant Fuzzy::InDanger(cFielder* TheFielder)
{
    cFielder* const fielder = TheFielder;
    FuzzyVariant bestValue;
    SCRIPT_QUESTION_KEY(fielderFunction, InDanger, (cPlayer*)fielder);
    ScriptQuestionCache* const* cache = ScriptQuestionCache::InstanceStorage();
    if ((*cache)->Lookup(hash, bestValue, NULL))
    {
        bestValue.Confidence = bestValue.Confidence;
        ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
        return bestValue;
    }

    float fBestConfidence = nlMaxFive(Attacked(fielder),
        Pressured(fielder),
        StuckOnSidelines(fielder),
        AvoidingPowerups(fielder),
        FGREATER(1.0f - Open(fielder), 0.35f));

    FuzzyVariant fvResult(fBestConfidence);
    bestValue = fvResult;
    bestValue.Confidence = 1.0f;

    ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
    return bestValue;
}

namespace Fuzzy
{
const nlVector2 g_vInDangerDelayedMin = { 0.8f, 0.2f };
const nlVector2 g_vInDangerDelayedMax = { 1.0f, 1.0f };
} // namespace Fuzzy

/**
 * Offset/Address/Size: 0x3B4 | 0x8006A584 | size: 0xAB0
 */
FuzzyVariant Fuzzy::InDangerDelayed(cFielder* TheFielder)
{
    FuzzyVariant bestValue;
    float fConfidence = 1.0f;
    float fBestConfidence = 0.0f;

    SCRIPT_QUESTION_KEY(fielderFunction, InDangerDelayed, (cPlayer*)TheFielder);

    ScriptQuestionCache* const* lookupCache = ScriptQuestionCache::InstanceStorage();
    if ((*lookupCache)->Lookup(hash, bestValue, NULL))
    {
        bestValue.Confidence = bestValue.Confidence;
        ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
        return bestValue;
    }

    float fTrueConfidence = nlMaxEquals(
        StuckOnSidelines(TheFielder), AvoidingPowerups(TheFielder));

    float fFalseConfidence = 1.0f - fTrueConfidence;
    float fMinVal = (fTrueConfidence <= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fMaxVal = (fTrueConfidence >= fFalseConfidence) ? fTrueConfidence : fFalseConfidence;
    float fBranchRatio = fMinVal / fMaxVal;

    if (fTrueConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);
        fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
        if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
        {
            fConfidence = (float)(double)fConfidence * fBranchRatio;
        }
        if (fConfidence > 0.0f)
        {
            fBestConfidence = fConfidence;
            FuzzyVariant fvResult(fConfidence);
            bestValue = fvResult;
        }
    }

    if (fFalseConfidence > 0.0f)
    {
        SaveConfidence PushDOM(&fConfidence);
        fConfidence = (fConfidence <= fFalseConfidence) ? fConfidence : fFalseConfidence;
        if (fConfidence < fFalseConfidence && fFalseConfidence < 0.5f)
        {
            fConfidence = (float)(double)fConfidence * fBranchRatio;
        }

        fTrueConfidence = nlMaxThree(Attacked(TheFielder),
            Pressured(TheFielder),
            FGREATER(1.0f - Open(TheFielder), 0.2f));

        float fFalseConfidence2 = 1.0f - fTrueConfidence;
        float fMinVal2 = (fTrueConfidence <= fFalseConfidence2) ? fTrueConfidence : fFalseConfidence2;
        float fMaxVal2 = (fTrueConfidence >= fFalseConfidence2) ? fTrueConfidence : fFalseConfidence2;
        float fBranchRatio2 = fMinVal2 / fMaxVal2;

        if (fTrueConfidence > 0.0f)
        {
            SaveConfidence PushDOM(&fConfidence);
            fConfidence = (fConfidence <= fTrueConfidence) ? fConfidence : fTrueConfidence;
            if (fConfidence < fTrueConfidence && fTrueConfidence < 0.5f)
            {
                fConfidence = (float)(double)fConfidence * fBranchRatio2;
            }
            SkillTweaks* pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            float fMin = Interpolate(g_vInDangerDelayedMin.x, g_vInDangerDelayedMin.y, pSkillTweaks->Off_Reaction);
            pSkillTweaks = SkillTweaks::GetSkillTweaks(g_pCurrentlyUpdatingTeam->m_nSide);
            float fMax = Interpolate(g_vInDangerDelayedMax.x, g_vInDangerDelayedMax.y, pSkillTweaks->Off_Reaction);

            cTeam* pTeam = TheFielder ? TheFielder->m_pTeam : NULL;
            if (Difficult(pTeam) == 0.0f)
            {
                fMin = 0.9f;
            }

            float fScore = NormalizeVal(fConfidence, fMin, fMax);
            if (fConfidence > fBestConfidence)
            {
                fBestConfidence = fConfidence;
                FuzzyVariant fvResult(fScore);
                bestValue = fvResult;
            }
        }
    }

    bestValue.Confidence = fBestConfidence;
    ScriptQuestionCache::Instance()->AddToCache(hash, bestValue, NULL);
    return bestValue;
}

/**
 * Offset/Address/Size: 0x0 | 0x8006A1D0 | size: 0x3B4
 */
FuzzyVariant Fuzzy::GoalieAndGonnaPickupBall(cPlayer* ThePlayer)
{
    FuzzyVariant bestValue;

    SCRIPT_QUESTION_KEY(playerFunction, GoalieAndGonnaPickupBall, (cPlayer*)ThePlayer);

    FuzzyVariant fvResult(nlMinFour(
        GoalieType(ThePlayer),
        CloseToBall(ThePlayer),
        ClosingTo(ThePlayer, g_pScriptBall),
        AbleToInterceptBall(ThePlayer)));

    bestValue = fvResult;
    bestValue.Confidence = 1.0f;

    return bestValue;
}
