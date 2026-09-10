#ifndef _PHYSICSGOALIE_H_
#define _PHYSICSGOALIE_H_

#include "NL/nlMath.h"
#include "Game/Physics/PhysicsCharacter.h"

class PhysicsGoalie : public PhysicsCharacter
{
public:
    PhysicsGoalie(float radius, float height)
        : PhysicsCharacter(radius, height)
    {
    }
    virtual void PostUpdate();

    static bool IsBallNearGoalie(float goalieRadius, float ballRadius, const nlVector3& goaliePos, const nlVector3& ballPrevPosition);
    static bool BigBallSweepTest(float goalieRadius, float ballRadius, const nlVector3& goaliePos, const nlVector3& ballPrevPosition, const nlVector3& ballCurrentPosition);
    bool SweepTestForBallContact(const nlVector3& ballPrevPosition, const nlVector3& ballCurrentPosition, const nlVector3& velocity, float ballRadius, nlVector3& positionWhenHit, nlVector3& contactNormal) const;
    bool SweepTestEveryBone(float ballRadius, const nlVector3& ballPrevPosition, const nlVector3& ballCurrentPosition, nlVector3& contactNormal, nlVector3& positionWhenHit) const;
    void CollideGoalieWithPost();
};

#endif // _PHYSICSGOALIE_H_
