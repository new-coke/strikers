#ifndef _AVOIDCONTROLLER_H_
#define _AVOIDCONTROLLER_H_

#include "NL/nlMath.h"
#include "Game/AI/Fielder.h"
#include "Game/Player.h"
#include "Game/AI/Powerups.h"
#include "Game/AI/ScriptAction.h"
#include "Game/Field.h"

enum eAvoidableThings
{
    AVOID_NOTHING = 0,
    AVOID_FIELDERS = 1,
    AVOID_GOALIES = 2,
    AVOID_POWERUPS = 4,
    AVOID_SIDELINES = 8,
    AVOID_BOWSER = 16,
    AVOID_EVERYTHING = 31,
    NUM_AVOIDABLES = 6,
};

class AvoidController
{
public:
    AvoidController(cFielder* fielder);
    void SetThingsToAvoid(int thingsToAvoid);
    void UseMinimumAvoidance(cPlayer* player);
    nlVector3& GetLastRepulsionVector(eAvoidableThings things);
    void Update(float fDeltaT);
    bool CalcFielderRepulsionVector(nlVector3& v3OutRepulsion);

private:
    bool CalcGoalieRepulsionVector(nlVector3& v3OutRepulsion);
    bool CalcBowserRepulsionVector(nlVector3& v3OutRepulsion);

public:
    bool CalcPowerupRepulsionVector(nlVector3& v3OutRepulsion);
    bool CalcDesiredVelocityToAvoidSideline(nlVector2& vNewDesiredVelDir, const nlVector2& vCurrentDesiredVelDir, const nlVector2& vCurrentVelDir, const nlVector2& vSidelinePos, const nlVector2& vSidelineNormal);
    bool CalcDesiredVelocityToAvoidCorner(nlVector2& vNewDesiredVelDir, const sCornerSegment& corner, const nlVector2& vCurrentDesiredVelDir, const nlVector2& vCurrentVelDir);
    bool AvoidSidelines();
    void CalcDesiredAndCurrentVelocities(nlVector2* vOutDesiredVel, nlVector2* vOutCurrentVel, bool bNormalized);
    void ApplyRepulsionVector(nlVector3 v3Repulsion);

    /* 0x00 */ cFielder* m_pFielder;
    /* 0x04 */ const FielderTweaks* m_pFTweaks;
    /* 0x08 */ int m_ThingsToAvoid;
    /* 0x0C */ int m_CurrentlyAvoiding;
    /* 0x10 */ bool m_UseMinimumAvoidance;
    /* 0x14 */ cPlayer* m_pIgnoreThisPlayer;
    /* 0x18 */ bool m_VeryCloseToSideline;
    /* 0x19 */ bool m_SidelineUnavoidable;
    /* 0x1C */ nlVector2 m_SidelineNormal;
    /* 0x24 */ nlVector2 m_SidelineDirection;
    /* 0x2C */ nlVector3 m_LastRepulsionVector[6];
}; // total size: 0x74

#endif // _AVOIDCONTROLLER_H_
