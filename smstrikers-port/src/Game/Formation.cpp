#include "Game/Formation.h"
#include "stdlib.h"
extern "C" void OSReport(const char*, ...);

#include "Game/MathHelpers.h"
#include "Game/AI/AiUtil.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/FuzzyVariant.h"
#include "Game/AI/Scripts/FormationScript.h"
#include "Game/AI/Scripts/ScriptQuestions.h"
#include "Game/Game.h"
#include "Game/Team.h"
#include "Game/FormationDefines.h"
#include "Game/Sys/debug.h"
#include "NL/nlMath.h"
#include "NL/nlPrint.h"
#include "NL/nlString.h"

static const nlVector3 v3Zero = { 0.0f, 0.0f, 0.0f };
static const float g_fFormationCaptainPosBonus[4] = { 0.55f, 0.5f, 0.0f, 0.0f };

struct FormationPositionThresholds
{
    float fInRadius;
    float fOutUpField;
    float fOutDownField;
    float fOutLateral;
};

static const FormationPositionThresholds g_aDefensiveFormationThresholds[4] = {
    { 0.6f, 1.0f, 4.0f, 2.0f },
    { 0.6f, 1.0f, 4.0f, 2.0f },
    { 0.6f, 3.0f, 1.0f, 2.0f },
    { 0.6f, 3.0f, 1.0f, 2.0f },
};

int FormationManager::m_NumFormationSets = 0;
FormationSet* FormationManager::m_FormationSetArray = nullptr;

static inline void ClearFormationEvals(FormationEval** pp);

/**
 * Offset/Address/Size: 0x29F4 | 0x8003AC44 | size: 0x60
 */
FormationManager::FormationManager(cTeam* pTeam)
    : m_pTeam(pTeam)
{
    m_pFormations[0] = nullptr;
    m_pFormations[1] = nullptr;
    m_pFormations[2] = nullptr;
    m_CachedPositions[0].bCacheIsValid = false;
    m_CachedPositions[1].bCacheIsValid = false;
    m_CachedPositions[2].bCacheIsValid = false;
    m_CachedPositions[3].bCacheIsValid = false;
    m_tSelectFormationsTimer.m_uPackedTime = 0;
}

/**
 * Offset/Address/Size: 0x2974 | 0x8003ABC4 | size: 0x80
 */
FormationManager::~FormationManager()
{
    ClearFormationEvals((FormationEval**)this);
}

static inline void ClearFormationEvaluators()
{
    if (g_pTeams[0] != nullptr && g_pTeams[1] != nullptr)
    {
        FormationEval** pp;
        s32 j;
        for (s32 i = 0; i < 2; i++)
        {
            pp = (FormationEval**)g_pTeams[i]->m_pFormationManager;
            for (j = 0; j < 3; j++)
            {
                FormationEval* pEval = pp[1];
                delete pEval;
                *++pp = nullptr;
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x281C | 0x8003AA6C | size: 0x158
 */
void FormationManager::LoadFormationSets()
{
    int numSets = 0;
    FormationSet* pFormationSets = FormationSet::LoadFormationSets("FormationSets.ini", numSets);

    if (numSets == 0)
    {
        if (m_FormationSetArray != nullptr)
        {
            tDebugPrintManager::Print(DC_AI, "Reload of FormationSets.ini failed, but we\'ve got some loaded already.");
        }
        return;
    }

    if (g_pGame != nullptr)
    {
        ClearFormationEvaluators();
    }

    delete[] m_FormationSetArray;
    m_FormationSetArray = pFormationSets;
    m_NumFormationSets = numSets;

    if (g_pGame != nullptr)
    {
        if (g_pTeams[0] != nullptr && g_pTeams[1] != nullptr)
        {
            for (s32 i_team = 0; i_team < 2; i_team++)
            {
                g_pTeams[i_team]->m_pFormationManager->ChooseNewFormations();
            }
        }
    }
}

static inline void ClearFormationEvals(FormationEval** pp)
{
    for (s32 j = 0; j < 3; j++)
    {
        FormationEval* pEval = pp[1];
        delete pEval;
        *++pp = nullptr;
    }
}

/**
 * Offset/Address/Size: 0x275C | 0x8003A9AC | size: 0xC0
 */
void FormationManager::UnloadFormationSets()
{
    s32 i;

    if (g_pGame != nullptr)
    {
        if (g_pTeams[0] != nullptr && g_pTeams[1] != nullptr)
        {
            for (i = 0; i < 2; i++)
            {
                ClearFormationEvals((FormationEval**)g_pTeams[i]->m_pFormationManager);
            }
        }
    }

    delete[] m_FormationSetArray;
    m_FormationSetArray = nullptr;
    m_NumFormationSets = 0;
}

/**
 * Offset/Address/Size: 0x26E4 | 0x8003A934 | size: 0x78
 */
FormationSpec* FormationManager::GetFormationSpec(eFormation specType)
{
    FormationSpec* pSpec = nullptr;
    int i = 0;

    while (i < m_NumFormationSets)
    {
        pSpec = m_FormationSetArray[i].GetFormationSpecFromID(specType);
        if (pSpec != nullptr)
        {
            break;
        }
        i++;
    }

    return pSpec;
}

/**
 * Offset/Address/Size: 0x2558 | 0x8003A7A8 | size: 0x18C
 */
void FormationManager::Update(float dt)
{
    if (!g_pGame->IsGameplayOrOvertime())
    {
        return;
    }

    m_tSelectFormationsTimer.Countdown(dt, 0.0f);

    if (m_tSelectFormationsTimer.m_uPackedTime == 0)
    {
        ChooseNewFormations();
        float randomTime = nlRandomf(0.2f, &nlDefaultSeed);
        m_tSelectFormationsTimer.SetSeconds(10.0f + randomTime);
    }

    m_v2AIFielderCenter.x = 0.0f;
    m_v2AIFielderCenter.y = 0.0f;

    for (int i = 0; i < 4; i++)
    {
        cFielder* pFielder = m_pTeam->GetFielder(i);
        float newX = m_v2AIFielderCenter.x + pFielder->m_v3Position.x;
        float newY = m_v2AIFielderCenter.y + pFielder->m_v3Position.y;
        m_v2AIFielderCenter.x = newX;
        m_v2AIFielderCenter.y = newY;
    }

    float cx = m_v2AIFielderCenter.x * 0.25f;
    float cy = m_v2AIFielderCenter.y * 0.25f;
    m_v2AIFielderCenter.x = cx;
    m_v2AIFielderCenter.y = cy;

    if (m_pTeam->m_nSide == AWAY)
    {
        m_v2AIFielderCenter.x = -m_v2AIFielderCenter.x;
        m_v2AIFielderCenter.y = -m_v2AIFielderCenter.y;
    }

    for (int i = 0; i < 3; i++)
    {
        if (m_pFormations[i] != nullptr)
        {
            m_pFormations[i]->Update(dt);
        }
    }

    m_CachedPositions[0].bCacheIsValid = false;
    m_CachedPositions[1].bCacheIsValid = false;
    m_CachedPositions[2].bCacheIsValid = false;
    m_CachedPositions[3].bCacheIsValid = false;
}

/**
 * Offset/Address/Size: 0x24B8 | 0x8003A708 | size: 0xA0
 */
void FormationManager::ChooseNewFormations()
{
    s32 defensiveFormation;
    s32 offensiveFormation;
    s32 ballFormationSet;

    defensiveFormation = Fuzzy::GetBestDefensiveFormation(m_pTeam).mData.i;
    offensiveFormation = Fuzzy::GetBestOffensiveFormation(m_pTeam).mData.i;
    ballFormationSet = Fuzzy::GetBestBallFormationSet(m_pTeam).mData.i;

    SetNewFormationEval(FTYPE_DEFENSIVE, (eFormation)defensiveFormation);
    SetNewFormationEval(FTYPE_OFFENSIVE, (eFormation)offensiveFormation);
    SetNewFormationEval(FTYPE_BALLPOSITION, (eFormationSet)ballFormationSet);
}

/**
 * Offset/Address/Size: 0x23DC | 0x8003A62C | size: 0xDC
 */
void FormationManager::SetNewFormationEval(eFormationType formType, eFormation formation)
{
    FormationEval* pFormation;

    if (formation == FORMATION_NONE)
    {
        pFormation = m_pFormations[formType];
        delete pFormation;
        m_pFormations[formType] = nullptr;
    }
    else
    {
        pFormation = m_pFormations[formType];
        if (pFormation != nullptr)
        {
            if ((s32)formation == pFormation->m_pFormationSpec->m_ID)
            {
                return;
            }
        }
        delete pFormation;
        m_pFormations[formType] = FormationEval::Create(this, formType, FSET_NONE, formation);
    }
}

/**
 * Offset/Address/Size: 0x2300 | 0x8003A550 | size: 0xDC
 */
void FormationManager::SetNewFormationEval(eFormationType formType, eFormationSet formSet)
{
    FormationEval* pFormation;

    if (formSet == FSET_NONE)
    {
        pFormation = m_pFormations[formType];
        delete pFormation;
        m_pFormations[formType] = nullptr;
    }
    else
    {
        pFormation = m_pFormations[formType];
        if (pFormation != nullptr)
        {
            const FormationSet* pFormationSet = static_cast<FormationBallPosition*>(pFormation)->m_pFormationSet;
            if ((s32)formSet == pFormationSet->m_ID)
            {
                return;
            }
        }
        delete pFormation;
        m_pFormations[formType] = FormationEval::Create(this, formType, formSet, FORMATION_NONE);
    }
}

/**
 * Offset/Address/Size: 0x202C | 0x8003A27C | size: 0x2D4
 */
bool FormationManager::CalculateFielderPosition(nlVector3& v3DestPosition, cFielder* pFielder, bool bInPosition, float fBallPosFormationWeight)
{
    float fFielderInPosition;
    float fWeights[3];
    nlVector3 v3FutureDesiredPosition;
    nlVector3 v3FormationPosition[2][3];

    int id = pFielder->m_ID;
    if (m_CachedPositions[id].bCacheIsValid)
    {
        v3DestPosition = m_CachedPositions[id].vPosition;
        return m_CachedPositions[id].bInPosition;
    }

    v3FutureDesiredPosition = v3Zero;
    fFielderInPosition = 0.0f;

    for (int i = 0; i < 3; i++)
    {
        FormationEval* pFormation = m_pFormations[i];
        if (pFormation != nullptr && pFormation->m_pFormationSpec != nullptr)
        {
            fWeights[i] = pFormation->GetWeight();
            pFormation->CalculateDesiredLocation(v3FormationPosition[1][i], pFielder, true);
            pFormation->CalculateDesiredLocation(v3FormationPosition[0][i], pFielder, false);
        }
        else
        {
            v3FormationPosition[0][i] = v3Zero;
            fWeights[i] = 0.0f;
            v3FormationPosition[1][i] = v3Zero;
        }
    }

    if (fWeights[2] >= 0.5f)
    {
        fWeights[2] = 1.0f;
    }

    float remainingWeight;
    float weight0;
    float scaledBallWeight = fWeights[2] * fBallPosFormationWeight;
    weight0 = fWeights[0];
    remainingWeight = 1.0f - scaledBallWeight;
    fWeights[2] = scaledBallWeight;
    fWeights[0] = weight0 * remainingWeight;
    fWeights[1] *= remainingWeight;

    for (int i = 0; i < 3; i++)
    {
        FormationEval* pFormation = m_pFormations[i];
        if (pFormation != nullptr && pFormation->m_pFormationSpec != nullptr)
        {
            nlVector3 pos = v3FormationPosition[0][i];
            float weight = fWeights[i];
            float x = v3FutureDesiredPosition.x + weight * v3FormationPosition[1][i].x;
            float z = v3FutureDesiredPosition.z + weight * v3FormationPosition[1][i].z;
            float y = v3FutureDesiredPosition.y + weight * v3FormationPosition[1][i].y;
            v3FutureDesiredPosition.x = x;
            v3FutureDesiredPosition.z = z;
            v3FutureDesiredPosition.y = y;
            fFielderInPosition += fWeights[i] * pFormation->IsFielderInPosition(pFielder, pos, bInPosition);
        }
    }

    v3DestPosition = v3FutureDesiredPosition;
    bInPosition = fFielderInPosition >= 0.3f;

    m_CachedPositions[id].vPosition = v3DestPosition;
    m_CachedPositions[id].bInPosition = bInPosition;
    m_CachedPositions[id].bCacheIsValid = true;

    return bInPosition;
}

FormationEval::FormationEval(FormationManager* pMgr, eFormationType type, const FormationSpec* spec)
{
    m_SortTimer.m_uPackedTime = 0;
    m_pFormationManager = pMgr;
    m_pKeyPlayer = NULL;
    m_pFormationSpec = spec;
    m_eFormationType = type;
    m_iFielderFormationPos[0] = 0;
    m_iFielderFormationPos[1] = 1;
    m_iFielderFormationPos[2] = 2;
    m_iFielderFormationPos[3] = 3;
}

FormationDefensive::FormationDefensive(FormationManager* pMgr, eFormationType type, const FormationSpec* spec)
    : FormationEval(pMgr, type, spec)
{
}

FormationOffensive::FormationOffensive(FormationManager* pMgr, eFormationType type, const FormationSpec* spec)
    : FormationEval(pMgr, type, spec)
{
}

FormationBallPosition::FormationBallPosition(FormationManager* pMgr, eFormationType type, const FormationSet* set)
    : FormationEval(pMgr, type, NULL)
{
    m_pNextClosestFormation = NULL;
    m_pFormationSet = set;
}

FormationBallPosition::FormationBallPosition(FormationManager* pMgr, eFormationType type, const FormationSpec* spec)
    : FormationEval(pMgr, type, spec)
{
    m_pNextClosestFormation = NULL;
    m_pFormationSet = NULL;
}

static inline const FormationSpec* GetFormationSpecInline(int id)
{
    const FormationSpec* spec = NULL;
    int i;
    for (i = 0; i < FormationManager::m_NumFormationSets; i++)
    {
        spec = FormationManager::m_FormationSetArray[i].GetFormationSpecFromID(id);
        if (spec != NULL)
        {
            break;
        }
    }
    return spec;
}

/**
 * Offset/Address/Size: 0x1E00 | 0x8003A050 | size: 0x22C
 */
FormationEval* FormationEval::Create(FormationManager* pManager, eFormationType formType, eFormationSet formSetID, eFormation formID)
{
    FormationEval* pNewFormation = NULL;
    FormationSet* formSet = NULL;
    const FormationSpec* formDef = NULL;

    if (formSetID != FSET_NONE)
    {
        formSet = &FormationManager::m_FormationSetArray[formSetID];
    }

    if (formType != (eFormationType)-1)
    {
        formDef = GetFormationSpecInline(formID);
    }

    switch (formType)
    {
    case FTYPE_DEFENSIVE:
        pNewFormation = new (nlMalloc(sizeof(FormationDefensive), 8, false)) FormationDefensive(pManager, formType, formDef);
        break;
    case FTYPE_OFFENSIVE:
        pNewFormation = new (nlMalloc(sizeof(FormationOffensive), 8, false)) FormationOffensive(pManager, formType, formDef);
        break;
    case FTYPE_BALLPOSITION:
        pNewFormation = new (nlMalloc(sizeof(FormationBallPosition), 8, false)) FormationBallPosition(pManager, formType, formSet);
        break;
    }

    return pNewFormation;
}

/**
 * Offset/Address/Size: 0x1D40 | 0x80039F90 | size: 0xC0
 */
void FormationEval::Update(float fDeltaT)
{
    m_pKeyPlayer = NULL;

    char buff[32];
    nlSNPrintf(buff, 31, "FormationEvalUpdate%d_%d", m_eFormationType, m_pFormationManager->m_pTeam->m_nSide);
    nlStringHash(buff);

    if (m_SortTimer.Countdown(fDeltaT, 0.0f))
    {
        SortPlayers(NULL);
        float randomTime = nlRandomf(0.2f, &nlDefaultSeed);
        m_SortTimer.SetSeconds(0.4f + randomTime);
    }
}

/**
 * Offset/Address/Size: 0x1D24 | 0x80039F74 | size: 0x1C
 */
float FormationEval::GetWeight()
{
    if (m_pFormationSpec != NULL)
    {
        return 1.0f;
    }
    return 0.0f;
}

void FormationEval::FieldLocToAILoc(nlVector3& dest, const nlVector3& field_location, int nTeamID)
{
    if (nTeamID == 0)
    {
        dest = field_location;
        return;
    }
    nlVec3Set(dest, -field_location.x, -field_location.y, 0.0f);
}

void FormationEval::AILocToFieldLoc(nlVector3& dest, const nlVector3& ai_location, int nTeamID)
{
    if (nTeamID == 0)
    {
        dest = ai_location;
        return;
    }
    nlVec3Set(dest, -ai_location.x, -ai_location.y, 0.0f);
}

/**
 * Offset/Address/Size: 0x1A14 | 0x80039C64 | size: 0x310
 */
void FormationEval::AssignPositionsToFielders(unsigned int* pFielderPosAssignments, float (*fFielderToPositionDistance)[4])
{
    float fBestDistance = 100000000000.0f;
    cPlayer* pKeyPlayer = GetKeyPlayer();
    cTeam* pTeam = m_pFormationManager->m_pTeam;
    int aiAssignedPos[4];

    for (aiAssignedPos[0] = 0; aiAssignedPos[0] < 4; aiAssignedPos[0]++)
    {
        float fDistance = fFielderToPositionDistance[0][aiAssignedPos[0]];

        for (aiAssignedPos[1] = 0; aiAssignedPos[1] < 4; aiAssignedPos[1]++)
        {
            if (aiAssignedPos[1] == aiAssignedPos[0])
            {
                continue;
            }

            float fDistancePos1 = fDistance;
            fDistance += fFielderToPositionDistance[1][aiAssignedPos[1]];

            for (aiAssignedPos[2] = 0; aiAssignedPos[2] < 4; aiAssignedPos[2]++)
            {
                if (aiAssignedPos[2] == aiAssignedPos[1])
                {
                    continue;
                }

                if (aiAssignedPos[2] == aiAssignedPos[0])
                {
                    continue;
                }

                float fDistancePos2 = fDistance;
                fDistance += fFielderToPositionDistance[2][aiAssignedPos[2]];

                for (aiAssignedPos[3] = 0; aiAssignedPos[3] < 4; aiAssignedPos[3]++)
                {
                    if (aiAssignedPos[3] == aiAssignedPos[2])
                    {
                        continue;
                    }

                    if (aiAssignedPos[3] == aiAssignedPos[1])
                    {
                        continue;
                    }

                    if (aiAssignedPos[3] == aiAssignedPos[0])
                    {
                        continue;
                    }

                    float fDistancePos3 = fDistance;
                    fDistance += fFielderToPositionDistance[3][aiAssignedPos[3]];

                    float fCaptainPosScore = 0.0f;
                    float fCaptainPosCount = fCaptainPosScore;
                    int* piAssignedPos = aiAssignedPos;

                    for (int iFielder = 0; iFielder < 4; iFielder++)
                    {
                        if (iFielder == pKeyPlayer->m_ID)
                        {
                            if (*piAssignedPos == m_pFormationSpec->m_iKeyIndex)
                            {
                                fCaptainPosScore += 0.5f;
                                fCaptainPosCount += 1.0f;
                            }
                        }

                        cFielder* pFielder = pTeam->GetFielder(iFielder);
                        if (pFielder->IsCaptain() && pFielder->GetGlobalPad() == NULL)
                        {
                            for (int iPos = 0; iPos < 4; iPos++)
                            {
                                if (iPos == *piAssignedPos)
                                {
                                    if (g_fFormationCaptainPosBonus[iPos] > 0.0f)
                                    {
                                        fCaptainPosCount += 1.0f;
                                        fCaptainPosScore += 1.0f - g_fFormationCaptainPosBonus[iPos];
                                        break;
                                    }
                                }
                            }
                        }

                        piAssignedPos++;
                    }

                    float fCaptainPosMult = 0.0f;
                    if (fCaptainPosCount > 0.0f)
                    {
                        fCaptainPosMult = fCaptainPosScore / fCaptainPosCount;
                    }

                    if (fCaptainPosMult > 0.0f)
                    {
                        fDistance *= fCaptainPosMult;
                    }

                    if (fDistance < fBestDistance)
                    {
                        fBestDistance = fDistance;
                        pFielderPosAssignments[0] = aiAssignedPos[0];
                        pFielderPosAssignments[1] = aiAssignedPos[1];
                        pFielderPosAssignments[2] = aiAssignedPos[2];
                        pFielderPosAssignments[3] = aiAssignedPos[3];
                    }

                    fDistance = fDistancePos3;
                }

                fDistance = fDistancePos2;
            }

            fDistance = fDistancePos1;
        }
    }
}

static inline void SubtractVector(nlVector2& result, const nlVector2& value)
{
    result.y -= value.y;
    result.x -= value.x;
}

static inline cTeam* GetFormationTeam(FormationManager* manager)
{
    return manager->m_pTeam;
}

/**
 * Offset/Address/Size: 0x1798 | 0x800399E8 | size: 0x27C
 */
void FormationEval::SortPlayers(const nlVector2* v2Center)
{
    float fFielderToPositionDistance[4][4];
    nlVector2 av2FormationPositions[4];
    nlVector3 av3FielderAILocs[4];
    nlVector2* pFormPositions;
    int i;
    nlVector2 v2CenterOfPlayers;
    int i_fielder;
    int i_pos;

    if (m_pFormationSpec == NULL)
    {
        return;
    }

    cTeam* team = GetFormationTeam(m_pFormationManager);
    memset(fFielderToPositionDistance, 0, sizeof(fFielderToPositionDistance));

    for (i = 0; i < 4; i++)
    {
        cFielder* pFielder = team->GetFielder(i);
        if (pFielder->GetGlobalPad() != NULL)
        {
            f32 fx = 0.4f * pFielder->m_v3Velocity.x + pFielder->m_v3Position.x;
            f32 fz = 0.4f * pFielder->m_v3Velocity.z + pFielder->m_v3Position.z;
            f32 fy = 0.4f * pFielder->m_v3Velocity.y + pFielder->m_v3Position.y;
            av3FielderAILocs[i].x = fx;
            av3FielderAILocs[i].y = fy;
            av3FielderAILocs[i].z = fz;
        }
        else
        {
            f32 fx = 0.15f * pFielder->m_v3Velocity.x + pFielder->m_v3Position.x;
            f32 fz = 0.15f * pFielder->m_v3Velocity.z + pFielder->m_v3Position.z;
            f32 fy = 0.15f * pFielder->m_v3Velocity.y + pFielder->m_v3Position.y;
            av3FielderAILocs[i].x = fx;
            av3FielderAILocs[i].y = fy;
            av3FielderAILocs[i].z = fz;
        }

        FieldLocToAILoc(av3FielderAILocs[i], av3FielderAILocs[i], team->m_nSide);
    }

    if (v2Center != NULL)
    {
        v2CenterOfPlayers = *v2Center;
    }
    else
    {
        v2CenterOfPlayers = m_pFormationManager->m_v2AIFielderCenter;
        SubtractVector(v2CenterOfPlayers, m_pFormationSpec->m_v2Center);
    }

    for (i = 0; i < 4; i++)
    {
        nlVec2Set(av2FormationPositions[i],
            m_pFormationSpec->m_Positions[i].m_Location.x + v2CenterOfPlayers.x,
            m_pFormationSpec->m_Positions[i].m_Location.y + v2CenterOfPlayers.y);
    }

    pFormPositions = av2FormationPositions;
    GetKeyPlayer();

    for (i_fielder = 0; i_fielder < 4; i_fielder++)
    {
        team->GetFielder(i_fielder);
        for (i_pos = 0; i_pos < 4; i_pos++)
        {
            float dy = pFormPositions[i_pos].y - av3FielderAILocs[i_fielder].y;
            float dx = pFormPositions[i_pos].x - av3FielderAILocs[i_fielder].x;
            fFielderToPositionDistance[i_fielder][i_pos] = nlSqrt(dx * dx + dy * dy, true);
        }
    }

    AssignPositionsToFielders(m_iFielderFormationPos, fFielderToPositionDistance);
}

/**
 * Offset/Address/Size: 0x1630 | 0x80039880 | size: 0x168
 */
cPlayer* FormationEval::GetKeyPlayer()
{
    if (m_pKeyPlayer != NULL)
    {
        return m_pKeyPlayer;
    }

    cTeam* team = m_pFormationManager->m_pTeam;
    cPlayer* otherCarrier;
    cPlayer* pKeyPlayer;

    cPlayer* strategicBallCarrier = Fuzzy::GetStrategicBallCarrier(team).mData.pPlayer;
    pKeyPlayer = strategicBallCarrier;

    if (pKeyPlayer == NULL)
    {
        pKeyPlayer = g_pBall->GetOwnerGoalie();
    }

    if (pKeyPlayer == NULL)
    {
        otherCarrier = Fuzzy::GetStrategicBallCarrier(team->GetOtherTeam()).mData.pPlayer;

        if (otherCarrier != NULL)
        {
            for (int i_fielder = 0; i_fielder < 4; i_fielder++)
            {
                cFielder* pFielder = team->GetFielder(i_fielder);
                if (pFielder->m_pMark == (cFielder*)otherCarrier)
                {
                    pKeyPlayer = pFielder;
                    break;
                }
            }
        }
        else
        {
            pKeyPlayer = Fuzzy::GetBestBallInterceptor(team).mData.pPlayer;
        }
    }

    if (pKeyPlayer == NULL)
    {
        for (int i_fielder = 0; i_fielder < 4; i_fielder++)
        {
            if (team->GetFielder(i_fielder)->GetGlobalPad() != NULL)
            {
                pKeyPlayer = team->GetFielder(i_fielder);
                break;
            }
        }
    }

    if (pKeyPlayer == NULL)
    {
        if (m_eFormationType == FTYPE_DEFENSIVE)
        {
            pKeyPlayer = team->GetRearMostFielder();
        }
        else if (m_eFormationType == FTYPE_OFFENSIVE)
        {
            pKeyPlayer = team->GetFrontMostFielder();
        }
        else
        {
            pKeyPlayer = team->GetStriker();
        }
    }

    m_pKeyPlayer = pKeyPlayer;
    return pKeyPlayer;
}

/**
 * Offset/Address/Size: 0x1488 | 0x800396D8 | size: 0x1A8
 */
void FormationEval::GetKeyPositions(cFielder* pFielder, nlVector3& v3KeyAIPosition, nlVector3* pKeyFormationAIPosition, bool bExtrapolate)
{
    cPlayer* keyPlayer = GetKeyPlayer();

    if (keyPlayer == pFielder)
    {
        FormationManager* mgr = m_pFormationManager;
        f32 x = mgr->m_v2AIFielderCenter.x;
        f32 y = mgr->m_v2AIFielderCenter.y;
        v3KeyAIPosition.x = x;
        v3KeyAIPosition.y = y;
        v3KeyAIPosition.z = 0.0f;

        if (pKeyFormationAIPosition != NULL)
        {
            const FormationSpec* spec = m_pFormationSpec;
            f32 cx = spec->m_v2Center.x;
            f32 cy = spec->m_v2Center.y;
            pKeyFormationAIPosition->x = cx;
            pKeyFormationAIPosition->y = cy;
            pKeyFormationAIPosition->z = 0.0f;
        }
    }
    else
    {
        if (bExtrapolate)
        {
            f32 t = 0.25f;
            f32 rz = keyPlayer->m_v3Position.z + t * keyPlayer->m_v3Velocity.z;
            f32 rx = keyPlayer->m_v3Position.x + t * keyPlayer->m_v3Velocity.x;
            f32 ry = keyPlayer->m_v3Position.y + t * keyPlayer->m_v3Velocity.y;
            v3KeyAIPosition.x = rx;
            v3KeyAIPosition.y = ry;
            v3KeyAIPosition.z = rz;
        }
        else
        {
            v3KeyAIPosition = keyPlayer->m_v3Position;
        }

        if (pFielder->m_pTeam->m_nSide != 0)
        {
            f32 nx = -v3KeyAIPosition.x;
            f32 ny = -v3KeyAIPosition.y;
            v3KeyAIPosition.x = nx;
            v3KeyAIPosition.y = ny;
            v3KeyAIPosition.z = 0.0f;
        }

        if (pKeyFormationAIPosition != NULL)
        {
            if (m_pFormationSpec->m_iKeyIndex > -1)
            {
                nlVector2& keyLoc = m_pFormationSpec->GetKeyLocation();
                f32 lx = keyLoc.x;
                f32 ly = keyLoc.y;
                pKeyFormationAIPosition->x = lx;
                pKeyFormationAIPosition->y = ly;
                pKeyFormationAIPosition->z = 0.0f;
            }
            else
            {
                pKeyFormationAIPosition->x = 2.0f;
                pKeyFormationAIPosition->y = 0.0f;
                pKeyFormationAIPosition->z = 0.0f;

                if (keyPlayer->m_eClassType != GOALIE)
                {
                    u32 posIndex = m_iFielderFormationPos[keyPlayer->m_ID];
                    const FormationPos* pPos = &m_pFormationSpec->m_Positions[posIndex];
                    f32 px = pPos->m_Location.x;
                    f32 py = pPos->m_Location.y;
                    pKeyFormationAIPosition->x = px;
                    pKeyFormationAIPosition->y = py;
                    pKeyFormationAIPosition->z = 0.0f;
                }
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x1330 | 0x80039580 | size: 0x158
 */
void FormationEval::CalculateDesiredLocation(nlVector3& destPosition, cFielder* pFielder, bool bExtrapolate)
{
    nlVector3 v3KeyAIPosition;
    nlVector3 v3KeyFormationAIPosition;
    nlVector2 v2FormationMin;
    nlVector2 v2FormationMax;

    GetKeyPositions(pFielder, v3KeyAIPosition, &v3KeyFormationAIPosition, bExtrapolate);

    m_pFormationSpec->CalculateExtents(v2FormationMin, v2FormationMax, *(nlVector2*)&v3KeyFormationAIPosition);

    u32 posIndex = m_iFielderFormationPos[pFielder->m_ID];
    const FormationSpec* pSpec = m_pFormationSpec;

    const FormationPos* pPos = &pSpec->m_Positions[posIndex];
    nlVector2 offset;
    nlVec2Sub(offset, pPos->m_Location, *(nlVector2*)&v3KeyFormationAIPosition);

    f32 clampX = nlMinEquals(nlMaxEquals(v3KeyAIPosition.x, v2FormationMin.x), v2FormationMax.x);

    destPosition.x = offset.x + clampX;

    f32 clampY = nlMinEquals(nlMaxEquals(v3KeyAIPosition.y, v2FormationMin.y), v2FormationMax.y);

    destPosition.y = offset.y + clampY;
    f32 zero = 0.0f;
    destPosition.z = zero;

    if (pFielder->m_pTeam->m_nSide != 0)
    {
        nlVec3Set(destPosition, -destPosition.x, -destPosition.y, zero);
    }
}

/**
 * Offset/Address/Size: 0x1150 | 0x800393A0 | size: 0x1E0
 */
float FormationEval::IsFielderInPosition(cFielder* pFielder, nlVector3 v3Pos, bool bExtended)
{
    if (m_pFormationSpec == NULL)
    {
        return 0.0f;
    }

    nlVector3 v3DesiredPos;
    GetKeyPositions(pFielder, v3DesiredPos, NULL, false);

    if (pFielder->m_pTeam->m_nSide != 0)
    {
        f32 negY = -v3DesiredPos.y;
        f32 negX = -v3DesiredPos.x;
        v3DesiredPos.z = 0.0f;
        v3DesiredPos.x = negX;
        v3DesiredPos.y = negY;
    }

    f32 dx = pFielder->m_v3Position.x - v3DesiredPos.x;
    f32 dy = pFielder->m_v3Position.y - v3DesiredPos.y;
    f32 distToDesired = nlSqrt(dx * dx + dy * dy, true);

    f32 dx2 = pFielder->m_v3Position.x - v3Pos.x;
    f32 dy2 = pFielder->m_v3Position.y - v3Pos.y;
    f32 distToTarget = nlSqrt(dx2 * dx2 + dy2 * dy2, true);

    f32 normalizedDist = NormalizeVal(distToDesired, g_pGame->m_pGameTweaks->vGetInPositionKeyFielderDist);

    f32 inDist = Interpolate(
        g_pGame->m_pGameTweaks->vGetInPositionInRadius.x,
        g_pGame->m_pGameTweaks->vGetInPositionInRadius.y,
        normalizedDist);

    f32 outDist = Interpolate(
        g_pGame->m_pGameTweaks->vGetInPositionOutRadius.x,
        g_pGame->m_pGameTweaks->vGetInPositionOutRadius.y,
        normalizedDist);

    f32 result = 0.0f;
    if (distToTarget <= inDist)
    {
        result = 1.0f;
    }
    else if (bExtended && distToTarget <= outDist)
    {
        result = nlMinEquals(nlMaxEquals(1.0f - distToTarget / outDist, 0.0f), 1.0f);
    }

    return result;
}

static inline float CalcScore(float fUpScore, float fDownScore, float fLateralScore)
{
    float fUpPos = 0.0f;
    if (fUpScore >= 0.0f)
        fUpPos = fUpScore;
    float fDownPos = (fDownScore >= 0.0f) ? fDownScore : 0.0f;
    float fLateralPos = (fLateralScore >= 0.0f) ? fLateralScore : 0.0f;
    float fScore = (fUpPos + fDownPos + fLateralPos) * 0.5f;
    fScore = (fScore >= 0.0f) ? fScore : 0.0f;
    fScore = (fScore <= 1.0f) ? fScore : 1.0f;
    return 1.0f - fScore;
}

/**
 * Offset/Address/Size: 0xE70 | 0x800390C0 | size: 0x2E0
 */
float FormationDefensive::IsFielderInPosition(cFielder* pFielder, nlVector3 v3DesiredPosition, bool bInPosition)
{
    nlVector3 v3KeyPosition;
    GetKeyPositions(pFielder, v3KeyPosition, NULL, false);

    if (pFielder->m_pTeam->m_nSide != 0)
    {
        float negY = -v3KeyPosition.y;
        float negX = -v3KeyPosition.x;
        v3KeyPosition.z = 0.0f;
        v3KeyPosition.x = negX;
        v3KeyPosition.y = negY;
    }

    float dx = pFielder->m_v3Position.x - v3KeyPosition.x;
    float dy = pFielder->m_v3Position.y - v3KeyPosition.y;
    float fPercent = nlSqrt(dx * dx + dy * dy, true);

    fPercent = NormalizeVal(fPercent, g_pGame->m_pGameTweaks->vGetInPositionKeyFielderDist);

    nlVector3 v3FielderPos = pFielder->m_v3Position;
    nlVector3 v3NetLocation = pFielder->GetAIOffNetLocation(NULL);
    const FormationPositionThresholds* pPositionThresholds = &g_aDefensiveFormationThresholds[pFielder->m_eRole];

    float distY = v3FielderPos.y - v3DesiredPosition.y;
    float distX = v3FielderPos.x - v3DesiredPosition.x;
    float dist = nlSqrt(distX * distX + distY * distY, true);

    float fInPosition = 0.0f;

    if (dist <= pPositionThresholds->fInRadius)
    {
        fInPosition = 1.0f;
    }
    else
    {
        float fNearZero = (float)((float)fabs(fPercent - fInPosition) <= 0.0001f);
        if (fNearZero != fInPosition)
        {
            if (pFielder == GetKeyPlayer())
                fInPosition = 1.0f;
            else
                fInPosition = 0.0f;
        }
        else if (bInPosition)
        {
            float fUpScore = ((v3DesiredPosition.x - v3FielderPos.x) * AIsgn(v3NetLocation.x))
                           / (fPercent * pPositionThresholds->fOutUpField);

            float fDownScore = ((v3FielderPos.x - v3DesiredPosition.x) * AIsgn(v3NetLocation.x))
                             / (fPercent * pPositionThresholds->fOutDownField);

            float fLateralScore = (float)fabs(v3FielderPos.y - v3DesiredPosition.y)
                                / (fPercent * pPositionThresholds->fOutLateral);

            fInPosition = CalcScore(fUpScore, fDownScore, fLateralScore);
        }
    }

    return fInPosition;
}

/**
 * Offset/Address/Size: 0xE40 | 0x80039090 | size: 0x30
 */
float FormationDefensive::GetWeight()
{
    return NormalizeVal(m_pFormationManager->m_v2AIFielderCenter.x, 12.361266f, -12.361266f);
}

static inline float fMax(float a, float b)
{
    if (a >= b)
        return a;
    return b;
}

static inline float fMin(float a, float b)
{
    if (a <= b)
        return a;
    return b;
}

/**
 * Offset/Address/Size: 0xB60 | 0x80038DB0 | size: 0x2E0
 */
float FormationOffensive::IsFielderInPosition(cFielder* pFielder, nlVector3 v3DesiredPosition, bool bInPosition)
{
    nlVector3 v3KeyPosition;
    GetKeyPositions(pFielder, v3KeyPosition, NULL, false);

    if (pFielder->m_pTeam->m_nSide != 0)
    {
        float negY = -v3KeyPosition.y;
        float negX = -v3KeyPosition.x;
        v3KeyPosition.z = 0.0f;
        v3KeyPosition.x = negX;
        v3KeyPosition.y = negY;
    }

    float dx = pFielder->m_v3Position.x - v3KeyPosition.x;
    float dy = pFielder->m_v3Position.y - v3KeyPosition.y;
    float fPercent = nlSqrt(dx * dx + dy * dy, true);

    fPercent = NormalizeVal(fPercent, g_pGame->m_pGameTweaks->vGetInPositionKeyFielderDist);

    nlVector3 v3FielderPos = pFielder->m_v3Position;
    nlVector3 v3NetLocation = pFielder->GetAIOffNetLocation(NULL);
    const FormationPositionThresholds* pPositionThresholds = &g_aDefensiveFormationThresholds[pFielder->m_eRole];

    float distY = v3FielderPos.y - v3DesiredPosition.y;
    float distX = v3FielderPos.x - v3DesiredPosition.x;
    float dist = nlSqrt(distX * distX + distY * distY, true);

    float fInPosition = 0.0f;

    if (dist <= pPositionThresholds->fInRadius)
    {
        fInPosition = 1.0f;
    }
    else
    {
        float fNearZero = (float)((float)fabs(fPercent - fInPosition) <= 0.0001f);
        if (fNearZero != fInPosition)
        {
            if (pFielder == GetKeyPlayer())
                fInPosition = 1.0f;
            else
                fInPosition = 0.0f;
        }
        else if (bInPosition)
        {
            float fUpScore = ((v3DesiredPosition.x - v3FielderPos.x) * AIsgn(v3NetLocation.x))
                           / (fPercent * pPositionThresholds->fOutUpField);

            float fDownScore = ((v3FielderPos.x - v3DesiredPosition.x) * AIsgn(v3NetLocation.x))
                             / (fPercent * pPositionThresholds->fOutDownField);

            float fLateralScore = (float)fabs(v3FielderPos.y - v3DesiredPosition.y)
                                / (fPercent * pPositionThresholds->fOutLateral);

            fInPosition = CalcScore(fUpScore, fDownScore, fLateralScore);
        }
    }

    return fInPosition;
}

/**
 * Offset/Address/Size: 0xB30 | 0x80038D80 | size: 0x30
 */
float FormationOffensive::GetWeight()
{
    return NormalizeVal(m_pFormationManager->m_v2AIFielderCenter.x, -12.361266f, 12.361266f);
}

/**
 * Offset/Address/Size: 0xA98 | 0x80038CE8 | size: 0x98
 */
FormationBallPosition::~FormationBallPosition()
{
    if (m_pNextClosestFormation != NULL)
    {
        delete m_pNextClosestFormation;
        m_pNextClosestFormation = NULL;
    }
}

/**
 * Offset/Address/Size: 0x930 | 0x80038B80 | size: 0x168
 */
void FormationBallPosition::Update(float fDeltaT)
{
    nlVector3 v3AIBallLoc;

    m_pKeyPlayer = NULL;

    if (m_SortTimer.Countdown(fDeltaT, 0.0f))
    {
        if (g_pBall->m_pOwner != NULL)
        {
            v3AIBallLoc = g_pBall->m_pOwner->m_v3Position;
        }
        else if (g_pBall->m_pPassTarget != NULL)
        {
            v3AIBallLoc = g_pBall->m_pPassTarget->m_v3Position;
        }
        else
        {
            v3AIBallLoc = g_pBall->m_v3Position;
        }

        if (m_pFormationManager->m_pTeam->m_nSide != 0)
        {
            float negY = -v3AIBallLoc.y;
            float negX = -v3AIBallLoc.x;
            v3AIBallLoc.z = 0.0f;
            v3AIBallLoc.x = negX;
            v3AIBallLoc.y = negY;
        }

        nlVector2 vAIBallLoc = *(const nlVector2*)&v3AIBallLoc;

        SelectClosestBallFormations(vAIBallLoc);

        if (m_pFormationSpec != NULL)
        {
            SortPlayers((const nlVector2*)&v3Zero);
        }

        if (m_pNextClosestFormation != NULL)
        {
            m_pNextClosestFormation->SortPlayers((const nlVector2*)&v3Zero);
        }

        m_SortTimer.SetSeconds(0.4f + nlRandomf(0.2f, &nlDefaultSeed));
    }
}

/**
 * Offset/Address/Size: 0x618 | 0x80038868 | size: 0x318
 */
bool FormationBallPosition::SelectClosestBallFormations(const nlVector2& v2AIBallLoc)
{
    FormationSpec* pClosest[2] = { NULL, NULL };
    float fDist[2] = { 1000000000.0f, 1000000000.0f };
    s32 i = 0;
    float ballX, ballY;
    ballY = v2AIBallLoc.y;
    ballX = v2AIBallLoc.x;

    for (; i < m_pFormationSet->m_NumFormationDefs; i++)
    {
        FormationSpec* pSpec = m_pFormationSet->GetFormationSpec(i);
        nlVector2& keyLoc = pSpec->GetKeyLocation();
        float dx, dy;
        dy = ballY - keyLoc.y;
        dx = ballX - keyLoc.x;
        float dist = nlSqrt(dx * dx + dy * dy, true);

        if (dist < fDist[0])
        {
            fDist[1] = fDist[0];
            pClosest[1] = pClosest[0];
            pClosest[0] = pSpec;
            fDist[0] = dist;
        }
        else if (dist < fDist[1])
        {
            pClosest[1] = pSpec;
            fDist[1] = dist;
        }
    }

    bool bChanged = false;

    FormationSpec* pClosestFormation = pClosest[0];
    // PORT: nothing selected, empty set, or NaN distances. Either way the read below is of address 8.
    if (pClosestFormation == NULL)
    {
        if (getenv("STRIKERS_PROBE_AI") != NULL)
        {
            static int nReported = 0;
            if (nReported < 8)
            {
                nReported++;
                OSReport("SelectClosestBallFormations: nothing selected, "
                         "%d defs, ball (%g, %g), current %p\n",
                         (int)m_pFormationSet->m_NumFormationDefs,
                         (double)v2AIBallLoc.x, (double)v2AIBallLoc.y,
                         (void*)m_pFormationSpec);
            }
        }
        return false;
    }

    if (pClosestFormation != m_pFormationSpec)
    {
        float fInDist = pClosestFormation->m_InRadius;
        if (m_pFormationSpec == NULL)
        {
            fInDist = pClosestFormation->m_OutRadius;
        }

        nlVector2& keyLoc = pClosestFormation->GetKeyLocation();
        float dy = v2AIBallLoc.y - keyLoc.y;
        float dx = v2AIBallLoc.x - keyLoc.x;
        float dist = nlSqrt(dx * dx + dy * dy, true);

        if (dist <= fInDist)
        {
            m_pFormationSpec = pClosestFormation;
            bChanged = true;
        }
    }

    do
    {
        if (m_pNextClosestFormation != NULL)
        {
            FormationSpec* pNextSpec = (FormationSpec*)m_pNextClosestFormation->m_pFormationSpec;
            float fOutDist = pNextSpec->m_OutRadius;
            nlVector2& keyLoc = pNextSpec->GetKeyLocation();
            float dy = v2AIBallLoc.y - keyLoc.y;
            float dx = v2AIBallLoc.x - keyLoc.x;
            float dist = nlSqrt(dx * dx + dy * dy, true);

            if (dist > fOutDist)
            {
                delete m_pNextClosestFormation;
                m_pNextClosestFormation = NULL;
                break;
            }
        }

        if (pClosest[1] != NULL)
        {
            FormationSpec* pCurrentNextSpec;
            if (m_pNextClosestFormation != NULL)
            {
                pCurrentNextSpec = (FormationSpec*)m_pNextClosestFormation->m_pFormationSpec;
            }
            else
            {
                pCurrentNextSpec = NULL;
            }

            if (pCurrentNextSpec != m_pFormationSpec)
            {
                float fInDist = pClosest[1]->m_InRadius;
                if (pCurrentNextSpec == NULL)
                {
                    fInDist = pClosest[1]->m_OutRadius;
                }

                nlVector2& keyLoc = pClosest[1]->GetKeyLocation();
                float dy = v2AIBallLoc.y - keyLoc.y;
                float dx = v2AIBallLoc.x - keyLoc.x;
                float dist = nlSqrt(dx * dx + dy * dy, true);

                if (!(dist <= fInDist))
                {
                    break;
                }
            }

            if (m_pNextClosestFormation == NULL)
            {
                m_pNextClosestFormation = new (nlMalloc(sizeof(FormationBallPosition), 8, false)) FormationBallPosition(m_pFormationManager, FTYPE_BALLPOSITION, pClosest[1]);
                bChanged = true;
            }
            else if (m_pNextClosestFormation->m_pFormationSpec != pClosest[1])
            {
                m_pNextClosestFormation->m_pFormationSpec = pClosest[1];
                bChanged = true;
            }
        }
    } while (false);

    return bChanged;
}

/**
 * Offset/Address/Size: 0x2C0 | 0x80038510 | size: 0x358
 */
void FormationBallPosition::CalculateDesiredLocation(nlVector3& destPosition, cFielder* pFielder, bool bExtrapolate)
{
    nlVector3 v3NextPosition;

    {
        nlVector3 v3KeyFormationAIPosition;
        nlVector3 v3KeyAIPosition;
        nlVector2 v2FormationMax;
        nlVector2 v2FormationMin;

        if (m_pFormationSpec == NULL)
        {
            destPosition = pFielder->m_v3Position;
            return;
        }

        GetKeyPositions(pFielder, v3KeyAIPosition, &v3KeyFormationAIPosition, bExtrapolate);

        m_pFormationSpec->CalculateExtents(v2FormationMin, v2FormationMax, *(nlVector2*)&v3KeyFormationAIPosition);

        u32 posIndex = m_iFielderFormationPos[pFielder->m_ID];
        const FormationSpec* pSpec = m_pFormationSpec;

        const FormationPos* pPos = &pSpec->m_Positions[posIndex];
        nlVector2 offset;
        nlVec2Sub(offset, pPos->m_Location, *(nlVector2*)&v3KeyFormationAIPosition);

        f32 clampX = nlMinEquals(nlMaxEquals(v3KeyAIPosition.x, v2FormationMin.x), v2FormationMax.x);

        destPosition.x = offset.x + clampX;

        f32 clampY = nlMinEquals(nlMaxEquals(v3KeyAIPosition.y, v2FormationMin.y), v2FormationMax.y);

        f32 zero = 0.0f;
        destPosition.y = offset.y + clampY;
        destPosition.z = zero;

        if (pFielder->m_pTeam->m_nSide != 0)
        {
            nlVec3Set(destPosition, -destPosition.x, -destPosition.y, zero);
        }
    }

    if (m_pNextClosestFormation != NULL && m_pNextClosestFormation->m_pFormationSpec != NULL)
    {
        m_pNextClosestFormation->CalculateDesiredLocation(v3NextPosition, pFielder, bExtrapolate);

        f32 blend = 1.0f;

        if (m_pNextClosestFormation != NULL)
        {
            const FormationSpec* pNextSpec = m_pNextClosestFormation->m_pFormationSpec;
            if (pNextSpec != NULL)
            {
                const FormationSpec* pSpec = m_pFormationSpec;
                nlVector3 v3AIBallLoc;

                if (g_pBall->m_pOwner != NULL)
                {
                    v3AIBallLoc = g_pBall->m_pOwner->m_v3Position;
                }
                else if (g_pBall->m_pPassTarget != NULL)
                {
                    v3AIBallLoc = g_pBall->m_pPassTarget->m_v3Position;
                }
                else
                {
                    v3AIBallLoc = g_pBall->m_v3Position;
                }

                if (m_pFormationManager->m_pTeam->m_nSide != 0)
                {
                    f32 negY = -v3AIBallLoc.y;
                    f32 negX = -v3AIBallLoc.x;
                    v3AIBallLoc.z = 0.0f;
                    v3AIBallLoc.x = negX;
                    v3AIBallLoc.y = negY;
                }

                nlVector2 vAIBallLoc = *(const nlVector2*)&v3AIBallLoc;
                nlVector2& keyLoc = pSpec->GetKeyLocation();
                f32 dyA = vAIBallLoc.y - keyLoc.y;
                f32 dxA = vAIBallLoc.x - keyLoc.x;
                f32 dist = nlSqrt(dxA * dxA + dyA * dyA, true);

                nlVector2& nextKeyLoc = pNextSpec->GetKeyLocation();
                f32 dyB = vAIBallLoc.y - nextKeyLoc.y;
                f32 dxB = vAIBallLoc.x - nextKeyLoc.x;
                f32 nextDist = nlSqrt(dxB * dxB + dyB * dyB, true);

                blend = nextDist / (dist + nextDist);
            }
        }

        f32 invBlend = 1.0f - blend;
        destPosition.x = invBlend * v3NextPosition.x + blend * destPosition.x;
        destPosition.y = invBlend * v3NextPosition.y + blend * destPosition.y;
        destPosition.z = invBlend * v3NextPosition.z + blend * destPosition.z;
    }
}

void FormationBallPosition::CalcBallPosition(nlVector2& v2DestAIBallPos)
{
    nlVector3 v3AIBallLoc;

    if (g_pBall->m_pOwner != NULL)
    {
        v3AIBallLoc = g_pBall->m_pOwner->m_v3Position;
    }
    else if (g_pBall->m_pPassTarget != NULL)
    {
        v3AIBallLoc = g_pBall->m_pPassTarget->m_v3Position;
    }
    else
    {
        v3AIBallLoc = g_pBall->m_v3Position;
    }

    if (m_pFormationManager->m_pTeam->m_nSide != 0)
    {
        float negY = -v3AIBallLoc.y;
        float negX = -v3AIBallLoc.x;
        v3AIBallLoc.z = 0.0f;
        v3AIBallLoc.x = negX;
        v3AIBallLoc.y = negY;
    }

    v2DestAIBallPos = *(const nlVector2*)&v3AIBallLoc;
}

float FormationBallPosition::GetBlendFactor()
{
    float fBlendFactor = 1.0f;

    if (m_pNextClosestFormation != NULL)
    {
        const FormationSpec* pNextSpec = m_pNextClosestFormation->m_pFormationSpec;
        if (pNextSpec != NULL)
        {
            const FormationSpec* pSpec = m_pFormationSpec;
            nlVector2 vAIBallPos;
            CalcBallPosition(vAIBallPos);

            nlVector2& keyLoc = pSpec->GetKeyLocation();
            float dy = vAIBallPos.y - keyLoc.y;
            float dx = vAIBallPos.x - keyLoc.x;
            float dist = nlSqrt(dx * dx + dy * dy, true);

            nlVector2& nextKeyLoc = pNextSpec->GetKeyLocation();
            float nextDy = vAIBallPos.y - nextKeyLoc.y;
            float nextDx = vAIBallPos.x - nextKeyLoc.x;
            float nextDist = nlSqrt(nextDx * nextDx + nextDy * nextDy, true);

            fBlendFactor = nextDist / (dist + nextDist);
        }
    }

    return fBlendFactor;
}

/**
 * Offset/Address/Size: 0x0 | 0x80038250 | size: 0x2C0
 */
float FormationBallPosition::GetWeight()
{
    nlVector2 vAIBallPos;
    CalcBallPosition(vAIBallPos);

    nlVector2& keyLoc = m_pFormationSpec->GetKeyLocation();
    float dy = vAIBallPos.y - keyLoc.y;
    float dx = vAIBallPos.x - keyLoc.x;
    float dist = nlSqrt(dx * dx + dy * dy, true);
    float fWeight = NormalizeVal(dist, m_pFormationSpec->m_OutRadius, 0.0f);

    if (m_pNextClosestFormation != NULL)
    {
        const FormationSpec* pNextSpec = m_pNextClosestFormation->m_pFormationSpec;
        if (pNextSpec != NULL)
        {
            float fBlendFactor = GetBlendFactor();
            float fNextClosestWeight = m_pNextClosestFormation->GetWeight();
            fWeight = fWeight * fBlendFactor + fNextClosestWeight * (1.0f - fBlendFactor);
        }
    }

    return fWeight;
}
