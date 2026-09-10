#include "Game/AI/AIPlay.h"

/**
 * Offset/Address/Size: 0x200 | 0x80005A94 | size: 0x18
 */
AIPlay::AIPlay(cFielder* pFielder, eAIPlay eplayType, float fPlayDuration)
{
    meCurrentPlay = eplayType;
    mpFielder = pFielder;
    mfPlayDuration = fPlayDuration;
    mpPlayDE = NULL;
}

/**
 * Offset/Address/Size: 0x1C4 | 0x80005A58 | size: 0x3C
 */
AIPlay::~AIPlay()
{
}

/**
 * Offset/Address/Size: 0x198 | 0x80005A2C | size: 0x2C
 */
void AIPlay::ClearPlay()
{
    mfPlayDuration = -1.0f;
    if (mfPlayDuration < 0.0f)
    {
        mfPlayDuration = 0.0f;
        meCurrentPlay = AIPLAY_NULL;
        mpPlayDE = NULL;
    }
}

bool AIPlay::IsPlayActive()
{
    return meCurrentPlay != AIPLAY_NULL;
}

void AIPlay::SetDuration(float fNewDuration)
{
    mfPlayDuration = fNewDuration;
    if (mfPlayDuration < 0.0f)
    {
        mfPlayDuration = 0.0f;
        meCurrentPlay = AIPLAY_NULL;
        mpPlayDE = NULL;
    }
}

void AIPlay::SetPlayType(eAIPlay eNewAIPlay)
{
    meCurrentPlay = eNewAIPlay;
    mpPlayDE = GetDecisionEntity(DECISION_ENTITY_PLAY, eNewAIPlay);
}

/**
 * Offset/Address/Size: 0x7C | 0x80005910 | size: 0x11C
 */
void AIPlay::Update(float fDeltaT)
{
    bool bGetNewPlay;
    class cDecisionEntity* decision_entity;
    float fDuration;

    mfPlayDuration -= fDeltaT;
    if (mfPlayDuration < 0.0f)
    {
        mfPlayDuration = 0.0f;
        meCurrentPlay = AIPLAY_NULL;
        mpPlayDE = NULL;
    }

    bGetNewPlay = false;
    if (mpPlayDE != NULL)
    {
        if (mpFielder != NULL)
        {
            bGetNewPlay = mpPlayDE->DoAbort(mpFielder);
        }
    }

    if ((meCurrentPlay == AIPLAY_NULL) || (bGetNewPlay != 0))
    {
        decision_entity = GetDecisionEntity(DECISION_ENTITY_STRATEGY, 0U);
        if ((decision_entity != NULL) && (decision_entity->CallDTF(mpFielder) > 0.0f))
        {
            int id = decision_entity->GetLastPlayParams().ePlayType;
            meCurrentPlay = (eAIPlay)id;
            mpPlayDE = GetDecisionEntity(DECISION_ENTITY_PLAY, id);

            fDuration = decision_entity->GetLastPlayParams().fDuration;
            if (fDuration <= 0.0f)
            {
                fDuration = 1.0f;
            }

            mfPlayDuration = fDuration;
            if (mfPlayDuration < 0.0f)
            {
                mfPlayDuration = 0.0f;
                meCurrentPlay = AIPLAY_NULL;
                mpPlayDE = NULL;
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80005894 | size: 0x7C
 */
void AIPlay::CalculateNewDesire()
{
    float fResult;

    if ((mpPlayDE != NULL) && (mpPlayDE->CallDTF(mpFielder) > 0.0f))
    {
        fResult = mpPlayDE->m_LastSelectedAction.m_fConfidence;
        if (fResult > 0.0f)
        {
            mpFielder->InitDesire(&mpPlayDE->GetLastDesireParams(), fResult);
        }
    }
}
