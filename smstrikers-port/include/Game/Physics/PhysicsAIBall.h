#ifndef _PHYSICSAIBALL_H_
#define _PHYSICSAIBALL_H_

#include "NL/nlMath.h"
#include "Game/Physics/PhysicsBall.h"

class cBall;

class PhysicsAIBall : public PhysicsBall
{
public:
    PhysicsAIBall(float radius);

    /* 0x0C */ virtual int GetObjectType() const
    {
        return 0xf;
    };
    /* 0x14 */ virtual void PreUpdate();
    /* 0x18 */ virtual void PostUpdate();
    /* 0x20 */ virtual ContactType Contact(PhysicsObject*obj, dContact*info, int numContacts);

    static bool IsBallOutsideNet(const nlVector3&ballPosition);
    static bool DidBallJustEnterNet(const nlVector3&oldPosition, nlVector3 newPosition);
    void CheckIfBallWentThroughGoalPost();
    void CheckIfBallWentThroughGoalie();

    /* 0x40 */ cBall* m_pAIBall;
    /* 0x44 */ nlVector3 m_v3PrevPosition;
    /* 0x50 */ u32 m_goalieContactFramesAgo;
    /* 0x54 */ u32 m_goalPostContactFramesAgo;
    /* 0x58 */ bool mbIsInsideNet;
    /* 0x59 */ bool mbGoalPlaneContact;
    /* 0x5A */ bool mbBallSpeedBelowSweepTestThreshold;
}; // total size: 0x5C

#endif // _PHYSICSAIBALL_H_
