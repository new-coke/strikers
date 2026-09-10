#include "Game/Physics/PhysicsBanana.h"

#include "Game/Game.h"
#include "Game/AI/Powerups.h"
#include "Game/Character.h"
#include "Game/Player.h"
#include "Game/Physics/PhysicsCharacter.h"
#include "Game/Physics/PhysicsAIBall.h"   // m_pAIBall
#include "Game/Physics/PhysicsNPC.h"      // mpAINPC
#include "Game/Render/SkinAnimatedNPC.h" // the NPC type enum
#include "Game/Ball.h"                    // cBall members

#include "NL/nlMemory.h"

extern CollisionSpace* g_CollisionSpace;
extern PhysicsWorld* g_PhysicsWorld;

/**
 * Offset/Address/Size: 0x75C | 0x80136190 | size: 0x78
 */
PhysicsBanana::PhysicsBanana(float radius)
    : PhysicsSphere(g_CollisionSpace, g_PhysicsWorld, radius)
{
    m_pTriggerCallbackFunc = NULL;
    m_pCallbackParam = NULL;
    m_pPowerupObject = NULL;
    m_bIsSupportedByGround = false;

    SetCollide(0xEF);
    SetCategory(0x20);

    m_gravity = -32.0f;
}

static const nlVector3 v3Zero = { 0.0f, 0.0f, 0.0f };

/**
 * Offset/Address/Size: 0x648 | 0x8013607C | size: 0x114
 */
void PhysicsBanana::PreUpdate()
{
    PhysicsObject::PreUpdate();
    nlVector3 linVel = GetLinearVelocity();

    float len = nlSqrt((linVel.x * linVel.x) + (linVel.y * linVel.y) + (linVel.z * linVel.z), true);

    if ((m_bIsSupportedByGround) || ((len > 15.0f) && (m_pPowerupObject->m_eType == POWER_UP_BANANA)))
    {
        if (((linVel.x * linVel.x) + (linVel.y * linVel.y) + (linVel.z * linVel.z)) > 0.1f)
        {
            float dumping = -g_pGame->m_pGameTweaks->fBananaResistance / len;
            linVel.x = dumping * linVel.x;
            linVel.y = dumping * linVel.y;
            linVel.z = dumping * linVel.z;
            AddForceAtCentreOfMass(linVel);
            return;
        }

        SetLinearVelocity(v3Zero);
    }
}

/**
 * Offset/Address/Size: 0x63C | 0x80136070 | size: 0xC
 */
void PhysicsBanana::PreCollide()
{
    m_bIsSupportedByGround = false;
}

/**
 * Offset/Address/Size: 0xFC | 0x80135B30 | size: 0x540
 */
ContactType PhysicsBanana::Contact(PhysicsObject* other, dContact* contact, int numContacts)
{
    nlVector3 bananaPos;
    GetPosition(&bananaPos);

    if (other->GetObjectType() == 0x11)
    {
        for (int i = 0; i < numContacts; i++)
        {
            if (contact[i].geom.pos[2] <= bananaPos.z && contact[i].geom.normal[2] > 0.9f)
            {
                if (!m_bIsSupportedByGround)
                {
                    m_bIsSupportedByGround = true;
                }

                nlVector3 linVel;
                GetLinearVelocity(&linVel);

                if (linVel.z < -1.0f)
                {
                    Event* event = g_pEventManager->CreateValidEvent(0x22, 0x2C);
                    CollisionPowerupGroundData* eventData = new (&event->m_data) CollisionPowerupGroundData();
                    GetPosition(&eventData->position);
                    eventData->fVecZComponent = linVel.z;
                    eventData->eType = m_pPowerupObject->m_eType;
                }

                if (m_pTriggerCallbackFunc != NULL)
                {
                    nlVector3 contactPos;
                    nlVec3Set(contactPos, contact->geom.pos[0], contact->geom.pos[1], contact->geom.pos[2]);
                    m_pTriggerCallbackFunc(this, other, contactPos, m_pCallbackParam);
                }
                break;
            }
        }
    }

    bool hasWallContact = false;
    switch (other->GetObjectType())
    {
    case 0x0D:
    case 0x0E:
    {
        if (((PhysicsCharacter*)other->m_parentObject)->m_pAICharacter->m_eClassType == FIELDER)
        {
            if (m_pPowerupObject->mtNoHitTimer.m_uPackedTime != 0 && m_pPowerupObject->m_pThrower == (cFielder*)((PhysicsCharacter*)other->m_parentObject)->m_pAICharacter)
            {
                return NO_CONTACT;
            }
            if (m_pTriggerCallbackFunc != NULL)
            {
                nlVector3 contactPos;
                nlVec3Set(contactPos, contact->geom.pos[0], contact->geom.pos[1], contact->geom.pos[2]);
                m_pTriggerCallbackFunc(this, other, contactPos, m_pCallbackParam);
            }
            return NO_CONTACT;
        }
        if (m_pTriggerCallbackFunc != NULL)
        {
            nlVector3 contactPos;
            nlVec3Set(contactPos, contact->geom.pos[0], contact->geom.pos[1], contact->geom.pos[2]);
            m_pTriggerCallbackFunc(this, other, contactPos, m_pCallbackParam);
        }
        break;
    }
    case 0x0F:
    {
        // PORT: was four console byte offsets walked off raw pointers.
        cBall* pBall = ((PhysicsAIBall*)other)->m_pAIBall;
        cCharacter* character = (cCharacter*)pBall->m_pOwner;
        if (character != NULL && character->m_eClassType == FIELDER)
        {
            if (m_pPowerupObject->mtNoHitTimer.m_uPackedTime != 0 && m_pPowerupObject->m_pThrower == (cFielder*)character)
            {
                return NO_CONTACT;
            }
            if (((cFielder*)character)->IsBallAwayFromCarrier())
            {
                return NO_CONTACT;
            }
        }
        {
            // PORT: the console read the shot timer's first word as a pointer and tested it for zero; m_uPackedTime is that word.
            bool hasBall = false;
            if (pBall->m_tShotTimer.m_uPackedTime != 0 && pBall->mbCanDamage)
            {
                hasBall = true;
            }
            if (!hasBall)
            {
                if (m_pTriggerCallbackFunc != NULL)
                {
                    nlVector3 contactPos;
                    nlVec3Set(contactPos, contact->geom.pos[0], contact->geom.pos[1], contact->geom.pos[2]);
                    m_pTriggerCallbackFunc(this, other, contactPos, m_pCallbackParam);
                }
                break;
            }
            m_pPowerupObject->m_bShouldDestroy = true;
            return NO_CONTACT;
        }
    }
    case 0x13:
    {
        if (m_pPowerupObject->mtNoHitTimer.m_uPackedTime != 0)
        {
            PowerupBase* otherPowerup = ((PhysicsBanana*)other)->m_pPowerupObject;
            if (otherPowerup->m_pThrower == m_pPowerupObject->m_pThrower)
            {
                return NO_CONTACT;
            }
        }
        if (m_pTriggerCallbackFunc != NULL)
        {
            nlVector3 contactPos;
            nlVec3Set(contactPos, contact->geom.pos[0], contact->geom.pos[1], contact->geom.pos[2]);
            m_pTriggerCallbackFunc(this, other, contactPos, m_pCallbackParam);
        }
        return NO_CONTACT;
    }
    case 0x14:
    {
        if (m_pPowerupObject->mtNoHitTimer.m_uPackedTime != 0)
        {
            PowerupBase* otherPowerup = ((PhysicsBanana*)other)->m_pPowerupObject;
            if (otherPowerup->m_pThrower == m_pPowerupObject->m_pThrower)
            {
                return NO_CONTACT;
            }
        }
        if (m_pTriggerCallbackFunc != NULL)
        {
            nlVector3 contactPos;
            nlVec3Set(contactPos, contact->geom.pos[0], contact->geom.pos[1], contact->geom.pos[2]);
            m_pTriggerCallbackFunc(this, other, contactPos, m_pCallbackParam);
        }
        return NO_CONTACT;
    }
    case 0x1A:
    {
        if (m_pPowerupObject->mtNoHitTimer.m_uPackedTime != 0)
        {
            // PORT: was `*(PhysicsObject**)((u8*)other + 0x30)` and a GetObjectType() call on the result. 0x30 is PhysicsNPC::mpAINPC on console.
            if (((SkinAnimatedNPC*)((PhysicsNPC*)other)->mpAINPC)
                    ->GetSkinAnimatedNPC_Type() == SkinAnimatedNPC_BOWSER)
            {
                if (m_pPowerupObject->m_pThrower == NULL)
                {
                    return NO_CONTACT;
                }
            }
        }
        break;
    }
    default:
    {
        for (int i = 0; i < numContacts; i++)
        {
            if (contact[i].geom.normal[2] < 0.08f)
            {
                hasWallContact = true;
            }
        }
        break;
    }
    }

    nlVector3& linVel = GetLinearVelocity();
    float velSq = (linVel.x * linVel.x) + (linVel.y * linVel.y) + (linVel.z * linVel.z);

    if (hasWallContact && velSq > 1.0f)
    {
        if (other->GetObjectType() == 0x19)
        {
            Event* event = g_pEventManager->CreateValidEvent(0x21, 0x3C);
            CollisionPowerupWallData* eventData = new (&event->m_data) CollisionPowerupWallData();

            eventData->eSize = m_pPowerupObject->meSize;
            eventData->eType = m_pPowerupObject->m_eType;

            nlVec3Set(eventData->position, contact->geom.pos[0], contact->geom.pos[1], contact->geom.pos[2]);
            nlVec3Set(eventData->normal, contact->geom.normal[0], contact->geom.normal[1], contact->geom.normal[2]);
        }
    }

    return TWO_WAY_CONTACT;
}

/**
 * Offset/Address/Size: 0x70 | 0x80135AA4 | size: 0x8C
 */
bool PhysicsBanana::SetContactInfo(dContact* contact, PhysicsObject* other, bool first)
{
    if (first)
    {
        SetDefaultContactInfo(contact);
    }

    if (other->GetObjectType() == 0x11)
    {
        contact->surface.bounce = g_pGame->m_pGameTweaks->fShellBounceGround;
    }
    else
    {
        contact->surface.bounce = 0.01f;
    }
    contact->surface.bounce_vel = 0.0f;
    contact->surface.mu = 10.0f;

    return true;
}

/**
 * Offset/Address/Size: 0x0 | 0x80135A34 | size: 0x70
 */
PhysicsBanana::~PhysicsBanana()
{
}
