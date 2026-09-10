#ifndef _DECISIONENTITY_H_
#define _DECISIONENTITY_H_

#include "NL/nlList.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/FuzzyVariant.h"
#include "types.h"

// Forward declarations
class ScriptAction;
struct sDesireParams;
struct sPlayParams;

enum eDecisionEntity
{
    DECISION_ENTITY_STRATEGY = 0,
    DECISION_ENTITY_PLAY = 1,
    NUM_DECISION_ENTITIES = 2,
};

class cDecisionEntity
{
public:
    cDecisionEntity() { };
    cDecisionEntity(eDecisionEntity type, unsigned long id, FuzzyVariant (*dtf)(cDecisionEntity*), FuzzyVariant (*af)(cDecisionEntity*));
    cDecisionEntity(const cDecisionEntity& other)
        : m_type(other.m_type)
        , m_id(other.m_id)
        , m_pDTF(other.m_pDTF)
        , m_pAF(other.m_pAF)
        , m_pLastQueuedAction(other.m_pLastQueuedAction)
        , m_LastSelectedAction(other.m_LastSelectedAction)
        , m_lQueuedActions(other.m_lQueuedActions)
        , m_iNumDTFCalls(other.m_iNumDTFCalls)
    {
    }

    float CallDTF(cFielder* pFielder);
    bool DoAbort(cFielder* pFielder);
    sDesireParams& GetLastDesireParams();
    sPlayParams& GetLastPlayParams();
    ScriptAction* QueueActionSetDesire(int eDesireType, float fConfidence, float fDuration, FuzzyVariant param1, FuzzyVariant param2);
    ScriptAction* QueueActionSetPlay(int ePlayType, float fConfidence, float fDuration);
    ScriptAction* FindDesireAction(int eDesireType, FuzzyVariant param1, FuzzyVariant param2);
    bool SelectAction(eScriptActionSelection actionSelection, float fDeltaT);

    /* 0x00 */ eDecisionEntity m_type;                         // offset 0x0, size 0x4
    /* 0x04 */ unsigned long m_id;                             // offset 0x4, size 0x4
    /* 0x08 */ FuzzyVariant (*m_pDTF)(class cDecisionEntity*); // offset 0x8, size 0x4
    /* 0x0C */ FuzzyVariant (*m_pAF)(class cDecisionEntity*);  // offset 0xC, size 0x4
    /* 0x10 */ ScriptAction* m_pLastQueuedAction;              // offset 0x10, size 0x4
    /* 0x14 */ ScriptAction m_LastSelectedAction;              // offset 0x14, size 0x80
    /* 0x94 */ nlList<ScriptAction> m_lQueuedActions;          // offset 0x94, size 0x8
    /* 0x9C */ unsigned long m_iNumDTFCalls;                   // offset 0x9C, size 0x4
}; // total size: 0xA0

cDecisionEntity* GetDecisionEntity(eDecisionEntity type, unsigned long id);

#endif // _DECISIONENTITY_H_
