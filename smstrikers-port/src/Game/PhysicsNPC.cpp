#include "Game/Physics/PhysicsNPC.h"
#include "Game/Physics/PhysicsCharacter.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Physics/PhysicsShell.h"
#include "Game/Physics/PhysicsBanana.h"
#include "Game/Physics/PhysicsFakeBall.h"
#include "Game/Render/SkinAnimatedMovableNPC.h"
#include "Game/Render/ChainChomp.h"

#include "Game/Player.h"
#include "Game/Ball.h"
#include "Game/Powerups.h"

class cFielder;

extern CollisionSpace* g_CollisionSpace;

/**
 * Offset/Address/Size: 0x440 | 0x8013B8F8 | size: 0x68
 */
PhysicsNPC::PhysicsNPC(float radius)
    : PhysicsSphere(g_CollisionSpace, (PhysicsWorld*)0, radius)
{
    mpTriggerCallbackFunc = NULL;
    mpAINPC = NULL;

    SetCollide(0x00);
    SetCategory(0xFF);
}

/**
 * Offset/Address/Size: 0x438 | 0x8013B8F0 | size: 0x8
 */
void PhysicsNPC::SetCallbackFunction(CallbackFn cb)
{
    mpTriggerCallbackFunc = cb;
}

/**
 * Offset/Address/Size: 0xC0 | 0x8013B578 | size: 0x378
 */
ContactType PhysicsNPC::Contact(PhysicsObject* obj, dContact* info, int what)
{
    nlVector3 myPos;
    GetPosition(&myPos);
    cFielder* pFielder;
    cBall* ball;
    switch (obj->GetObjectType())
    {
    case 0x04:
    case 0x0D:
    case 0x0E:
    {
        if (((PhysicsCharacter*)obj->m_parentObject)->m_pAICharacter->m_eClassType == FIELDER)
        {
            pFielder = (cFielder*)((PhysicsCharacter*)obj->m_parentObject)->m_pAICharacter;
            u8 bDoCallback = 1;
            if (((SkinAnimatedNPC*)mpAINPC)->GetSkinAnimatedNPC_Type() == SkinAnimatedNPC_CHAIN_CHOMP)
            {
                if (!((cPlayer*)pFielder)->IsOnSameTeam((cPlayer*)((ChainChomp*)mpAINPC)->mpTarget))
                {
                    bDoCallback = 0;
                }
            }
            if (bDoCallback)
            {
                if (mpTriggerCallbackFunc != NULL)
                {
                    nlVector3 v3Pos;
                    nlVec3Set(v3Pos, info->geom.pos[0], info->geom.pos[1], info->geom.pos[2]);
                    mpTriggerCallbackFunc(this, obj, v3Pos);
                    break;
                }
            }
            return NO_CONTACT;
        }
        else if (((PhysicsCharacter*)obj->m_parentObject)->m_pAICharacter->m_eClassType == GOALIE)
        {
            if (((SkinAnimatedNPC*)mpAINPC)->GetSkinAnimatedNPC_Type() == SkinAnimatedNPC_BOWSER)
            {
                return ONE_WAY_CONTACT_OTHER;
            }
        }
        break;
    }
    case 0x0F:
    {
        ball = ((PhysicsAIBall*)obj)->m_pAIBall;
        ball->m_unk_0xA6 = false;
        ball->mpDamageTarget = NULL;
        if (ball->m_pOwner != NULL)
        {
            if (ball->m_pOwner->m_eClassType != FIELDER)
                break;
            if (((SkinAnimatedNPC*)mpAINPC)->GetSkinAnimatedNPC_Type() != SkinAnimatedNPC_CHAIN_CHOMP)
                break;
            if (ball->m_pOwner->IsOnSameTeam((cPlayer*)((ChainChomp*)mpAINPC)->mpTarget))
            {
                if (mpTriggerCallbackFunc != NULL)
                {
                    nlVector3 v3Pos;
                    nlVec3Set(v3Pos, info->geom.pos[0], info->geom.pos[1], info->geom.pos[2]);
                    mpTriggerCallbackFunc(this, obj, v3Pos);
                }
                break;
            }
            else
            {
                return NO_CONTACT;
            }
        }
        else
        {
            bool cannotCollide = false;
            if (((PhysicsAIBall*)obj)->m_pAIBall->m_tShotTimer.m_uPackedTime != 0 && ((PhysicsAIBall*)obj)->m_pAIBall->mbCanDamage)
            {
                cannotCollide = true;
            }
            if (cannotCollide)
            {
                return NO_CONTACT;
            }
            if (mpTriggerCallbackFunc != NULL)
            {
                nlVector3 v3Pos;
                nlVec3Set(v3Pos, info->geom.pos[0], info->geom.pos[1], info->geom.pos[2]);
                mpTriggerCallbackFunc(this, obj, v3Pos);
            }
            ((PhysicsBall*)obj)->m_bUseMagnusEffect = false;
            FakeBallWorld::InvalidateBallCache();
            return ONE_WAY_CONTACT_OTHER;
        }
    }
    case 0x13:
    {
        PhysicsShell* pShell = (PhysicsShell*)obj;
        if (pShell->m_pPowerupObject->mtNoHitTimer.m_uPackedTime != 0)
        {
            if (((SkinAnimatedNPC*)mpAINPC)->GetSkinAnimatedNPC_Type() == SkinAnimatedNPC_BOWSER)
            {
                if (pShell->m_pPowerupObject->m_pThrower == NULL)
                {
                    return NO_CONTACT;
                }
            }
        }
        if (mpTriggerCallbackFunc != NULL)
        {
            nlVector3 v3Pos;
            nlVec3Set(v3Pos, info->geom.pos[0], info->geom.pos[1], info->geom.pos[2]);
            mpTriggerCallbackFunc(this, obj, v3Pos);
        }
        break;
    }
    case 0x14:
    {
        PhysicsBanana* pBanana = (PhysicsBanana*)obj;
        if (pBanana->m_pPowerupObject->mtNoHitTimer.m_uPackedTime != 0)
        {
            if (((SkinAnimatedNPC*)mpAINPC)->GetSkinAnimatedNPC_Type() == SkinAnimatedNPC_BOWSER)
            {
                if (pBanana->m_pPowerupObject->m_pThrower == NULL)
                {
                    return NO_CONTACT;
                }
            }
        }
        if (mpTriggerCallbackFunc != NULL)
        {
            nlVector3 v3Pos;
            nlVec3Set(v3Pos, info->geom.pos[0], info->geom.pos[1], info->geom.pos[2]);
            mpTriggerCallbackFunc(this, obj, v3Pos);
        }
        break;
    }
    default:
        break;
    }
    if (((SkinAnimatedNPC*)mpAINPC)->GetSkinAnimatedNPC_Type() == SkinAnimatedNPC_BOWSER)
    {
        return ONE_WAY_CONTACT_OTHER;
    }
    return NO_CONTACT;
}

/**
 * Offset/Address/Size: 0x70 | 0x8013B528 | size: 0x50
 */
bool PhysicsNPC::SetContactInfo(dContact* contact, PhysicsObject* other, bool first)
{
    if (first)
    {
        SetDefaultContactInfo(contact);
    }

    contact->surface.bounce = 0.01f;
    contact->surface.bounce_vel = 0.0f;
    contact->surface.mu = 5.0f;

    return true;
}
