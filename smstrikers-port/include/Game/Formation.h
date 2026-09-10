#ifndef _FORMATION_H_
#define _FORMATION_H_

#include "NL/nlMath.h"
#include "NL/nlTimer.h"

class cTeam;
class cFielder;
class FormationManager;
class cPlayer;

enum eFormationType
{
    FTYPE_DEFENSIVE = 0,
    FTYPE_OFFENSIVE = 1,
    FTYPE_BALLPOSITION = 2,
    NUM_FORMATION_TYPES = 3,
};

enum eFormationSet
{
    FSET_NONE = -1,
    FSET_OFFENSIVE_DEFENSIVE = 0,
    FSET_BALL_PASSIVE = 1,
    FSET_BALL_MODERATE = 2,
    FSET_BALL_AGGRESIVE = 3,
    NUM_FORMATION_SETS = 4,
};
enum eFormation
{
    FORMATION_NONE = -1,
    FORMATION_OFF_DEF_KICKOFF_NEUTRAL = 0,
    FORMATION_OFF_DEF_KICKOFF_ADVANTAGE = 1,
    FORMATION_OFF_DEF_31 = 2,
    FORMATION_OFF_DEF_121_DIAMOND = 3,
    FORMATION_OFF_DEF_22_SQUARE = 4,
    FORMATION_OFF_DEF_22_TRAPEZE = 5,
    FORMATION_BAL_PAS_OFFENSIVE_RIGHT = 6,
    FORMATION_BAL_PAS_OFFENSIVE_LEFT = 7,
    FORMATION_BAL_PAS_OFFENSIVE_CENTER = 8,
    FORMATION_BAL_PAS_DEFENSIVE_RIGHT = 9,
    FORMATION_BAL_PAS_DEFENSIVE_CENTER = 10,
    FORMATION_BAL_PAS_DEFENSIVE_LEFT = 11,
    FORMATION_BAL_PAS_CENTER_RIGHT = 12,
    FORMATION_BAL_PAS_CENTER_LEFT = 13,
    FORMATION_BAL_PAS_MIDOFFENSIVE_RIGHT = 14,
    FORMATION_BAL_PAS_MIDOFFENSIVE_LEFT = 15,
    FORMATION_BAL_PAS_MIDOFFENSIVE_CENTRE = 16,
    FORMATION_BAL_MOD_OFFENSIVE_RIGHT = 17,
    FORMATION_BAL_MOD_OFFENSIVE_LEFT = 18,
    FORMATION_BAL_MOD_OFFENSIVE_CENTER = 19,
    FORMATION_BAL_MOD_DEFENSIVE_RIGHT = 20,
    FORMATION_BAL_MOD_DEFENSIVE_CENTER = 21,
    FORMATION_BAL_MOD_DEFENSIVE_LEFT = 22,
    FORMATION_BAL_MOD_CENTER_LEFT = 23,
    FORMATION_BAL_MOD_CENTER_RIGHT = 24,
    FORMATION_BAL_MOD_MIDOFFENSIVE_RIGHT = 25,
    FORMATION_BAL_MOD_MIDOFFENSIVE_LEFT = 26,
    FORMATION_BAL_MOD_MIDOFFENSIVE_CENTRE = 27,
    FORMATION_BAL_MOD_CENTER = 28,
    FORMATION_BAL_AGG_OFFENSIVE_RIGHT = 29,
    FORMATION_BAL_AGG_OFFENSIVE_LEFT = 30,
    FORMATION_BAL_AGG_OFFENSIVE_CENTER = 31,
    FORMATION_BAL_AGG_DEFENSIVE_RIGHT = 32,
    FORMATION_BAL_AGG_DEFENSIVE_CENTER = 33,
    FORMATION_BAL_AGG_DEFENSIVE_LEFT = 34,
    FORMATION_BAL_AGG_CENTER_LEFT = 35,
    FORMATION_BAL_AGG_CENTER_RIGHT = 36,
    FORMATION_BAL_AGG_MIDOFFENSIVE_RIGHT = 37,
    FORMATION_BAL_AGG_MIDOFFENSIVE_LEFT = 38,
    FORMATION_BAL_AGG_MIDOFFENSIVE_CENTRE = 39,
    FORMATION_BAL_AGG_MIDDEFENSIVE_CENTRE = 40,
    NUM_FORMATIONS = 41,
};

class FormationSet;
class FormationSpec;

class FormationEval
{
public:
    FormationEval();
    FormationEval(FormationManager* pMgr, eFormationType type, const FormationSpec* spec);
    virtual ~FormationEval() { }
    virtual void CalculateDesiredLocation(nlVector3& destPosition, cFielder* pFielder, bool bExtrapolate);
    virtual cPlayer* GetKeyPlayer();
    virtual void GetKeyPositions(cFielder* pFielder, nlVector3& v3KeyAIPosition, nlVector3* pKeyFormationAIPosition, bool bExtrapolate);
    virtual float GetWeight();
    virtual float IsFielderInPosition(cFielder* pFielder, nlVector3 v3Pos, bool bExtended);
    virtual void SortPlayers(const nlVector2* v2Center);
    virtual void Update(float fDeltaT);
    static FormationEval* Create(FormationManager* pManager, eFormationType formType, eFormationSet formSetID, eFormation formID);
    void AssignPositionsToFielders(unsigned int* pFielderPosAssignments, float (*fFielderToPositionDistance)[4]);
    static void AILocToFieldLoc(nlVector3& dest, const nlVector3& ai_location, int nTeamID);
    static void FieldLocToAILoc(nlVector3& dest, const nlVector3& field_location, int nTeamID);

    /* 0x04 */ eFormationType m_eFormationType;
    /* 0x08 */ const FormationSpec* m_pFormationSpec;
    /* 0x0C */ cPlayer* m_pKeyPlayer;
    /* 0x10 */ FormationManager* m_pFormationManager;
    /* 0x14 */ Timer m_SortTimer;
    /* 0x18 */ unsigned int m_iFielderFormationPos[4];
}; // total size: 0x28

class FormationBallPosition : public FormationEval
{
public:
    FormationBallPosition(FormationManager* pMgr, eFormationType type, const FormationSet* set);
    FormationBallPosition(FormationManager* pMgr, eFormationType type, const FormationSpec* spec);
    ~FormationBallPosition();
    void CalculateDesiredLocation(nlVector3& destPosition, cFielder* pFielder, bool bExtrapolate);
    float GetWeight();
    float GetBlendFactor();
    void CalcBallPosition(nlVector2& v2DestAIBallPos);
    void Update(float fDeltaT);
    bool SelectClosestBallFormations(const nlVector2& v2AIBallLoc);

    /* 0x28 */ const FormationSet* m_pFormationSet;
    /* 0x2C */ FormationBallPosition* m_pNextClosestFormation;
}; // total size: 0x30

class FormationOffensive : public FormationEval
{
public:
    FormationOffensive(FormationManager* pMgr, eFormationType type, const FormationSpec* spec);
    float IsFielderInPosition(cFielder* pFielder, nlVector3 v3DesiredPosition, bool bInPosition);
    float GetWeight();
};

class FormationDefensive : public FormationEval
{
public:
    FormationDefensive(FormationManager* pMgr, eFormationType type, const FormationSpec* spec);
    float IsFielderInPosition(cFielder* pFielder, nlVector3 v3DesiredPosition, bool bInPosition);
    float GetWeight();
};

struct CachedPosition
{
    /* 0x0 */ bool bCacheIsValid;
    /* 0x1 */ bool bInPosition;
    /* 0x4 */ nlVector3 vPosition;
}; // total size: 0x10

class FormationManager
{
public:
    FormationManager(cTeam* pTeam);
    ~FormationManager();

    static void LoadFormationSets();
    static void UnloadFormationSets();
    static FormationSpec* GetFormationSpec(eFormation specType);
    void Update(float dt);
    void ChooseNewFormations();
    void SetNewFormationEval(eFormationType formType, eFormation formation);
    void SetNewFormationEval(eFormationType formType, eFormationSet formSet);
    bool CalculateFielderPosition(nlVector3& v3DestPosition, cFielder* pFielder, bool bInPosition, float fBallPosFormationWeight);

    /* 0x00 */ cTeam* m_pTeam;
    /* 0x04 */ FormationEval* m_pFormations[3];
    /* 0x10 */ nlVector2 m_v2AIFielderCenter;
    /* 0x18 */ Timer m_tSelectFormationsTimer;
    /* 0x1C */ CachedPosition m_CachedPositions[4];

    static FormationSet* m_FormationSetArray;
    static int m_NumFormationSets;
}; // total size: 0x5C

#endif // _FORMATION_H_
