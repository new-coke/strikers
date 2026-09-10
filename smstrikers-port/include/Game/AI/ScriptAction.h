#ifndef _SCRIPTACTION_H_
#define _SCRIPTACTION_H_

#include "NL/nlSlotPool.h"
#include "Game/AI/FuzzyVariant.h"

// Forward declarations
class cDecisionEntity;

enum eScriptActionSelection
{
    SAS_BEST_CONFIDENCE = 0,
    SAS_BEST_CHANCE = 1,
    SAS_BEST_CONFIDENCE_TIMES_CHANCE = 2,
    SAS_BEST_CONFIDENCE_MIN_CHANCE_THRESHOLD = 3,
    SAS_WORST_CONFIDENCE = 4,
    NUM_SCRIPT_ACTION_SELECTION = 5,
};

enum eScriptActionType
{
    SAT_NONE = -1,
    SAT_SET_PLAY = 0,
    SAT_SET_DESIRE = 1,
    NUM_SCRIPT_ACTIONS = 2,
};

enum eFielderDesireState
{
    FIELDERDESIRE_END_OF_LIST = -1,
    FIELDERDESIRE_NEED_DESIRE = 0,
    FIELDERDESIRE_CUT_AND_BREAK = 1,
    FIELDERDESIRE_DEKE = 2,
    FIELDERDESIRE_GET_IN_POSITION = 3,
    FIELDERDESIRE_GET_OPEN = 4,
    FIELDERDESIRE_HIT = 5,
    FIELDERDESIRE_INTERCEPT_BALL = 6,
    FIELDERDESIRE_MARK = 7,
    FIELDERDESIRE_PROTECT_BALL = 8,
    FIELDERDESIRE_RUN_TO_NET = 9,
    FIELDERDESIRE_RUN_UPFIELD = 10,
    FIELDERDESIRE_RUN_DOWNFIELD = 11,
    FIELDERDESIRE_RUN_TO_LOCATION = 12,
    FIELDERDESIRE_PASS = 13,
    FIELDERDESIRE_SHOOT = 14,
    FIELDERDESIRE_SLIDE_ATTACK = 15,
    FIELDERDESIRE_SUPPORT_BALL_DEFENSIVE = 16,
    FIELDERDESIRE_SUPPORT_BALL_OFFENSIVE = 17,
    FIELDERDESIRE_USE_POWERUP = 18,
    FIELDERDESIRE_WINDUP_PASS = 19,
    FIELDERDESIRE_WINDUP_SHOT = 20,
    FIELDERDESIRE_WAIT_FOR_THOUGHT_CAP = 21,
    FIELDERDESIRE_USER_CONTROLLED = 22,
    FIELDERDESIRE_FINISH_ACTION = 23,
    FIELDERDESIRE_ONETIMER = 24,
    FIELDERDESIRE_POST_WHISTLE = 25,
    FIELDERDESIRE_RECEIVE_PASS_FROM_IDLE = 26,
    FIELDERDESIRE_RECEIVE_PASS_FROM_RUN = 27,
    FIELDERDESIRE_WAIT = 28,
    NUM_FIELDERDESIRES = 29,
};

struct sPlayParams
{
    /* 0x0 */ int ePlayType;
    /* 0x4 */ float fDuration;

    sPlayParams() { }
    sPlayParams(const sPlayParams& other)
        : ePlayType(other.ePlayType)
        , fDuration(other.fDuration)
    {
    }
}; // total size: 0x8

struct sDesireParams
{
    /* 0x00 */ float fDuration;                 // size 0x4
    /* 0x04 */ eFielderDesireState eDesireType; // size 0x4
    /* 0x08 */ FuzzyVariant opt1;               // size 0x30
    /* 0x38 */ FuzzyVariant opt2;               // size 0x30

    sDesireParams()
    {
        fDuration = 0.0f;
        eDesireType = FIELDERDESIRE_NEED_DESIRE;
        opt1 = fvNotSet;
        opt2 = fvNotSet;
    }
}; // total size: 0x68

class ScriptAction
{
public:
    static SlotPool<ScriptAction> m_ScriptActionSlotPool;

    ScriptAction() { }
    ScriptAction(eScriptActionType action_type, float fConfidence);
    float CalcSelectionChance();
    bool RollChanceDice();

public:
    /* 0x00 */ ScriptAction* next;            // size 0x4
    /* 0x04 */ eScriptActionType m_eType;     // size 0x4
    /* 0x08 */ float m_fConfidence;           // size 0x4
    /* 0x0C */ float m_fSelectionChance;      // size 0x4
    /* 0x10 */ sDesireParams m_sDesireParams; // size 0x68
    /* 0x78 */ sPlayParams m_sPlayParams;     // size 0x8
}; // total size: 0x80

#endif // _SCRIPTACTION_H_
