#include "Game/Physics/PhysicsShell.h"
#include "Game/Game.h"
#include "Game/AI/Powerups.h"
#include "Game/Field.h"
#include "Game/Net.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Physics/PhysicsBanana.h"
#include "Game/Physics/PhysicsCharacter.h"
#include "Game/Physics/PhysicsNPC.h"
#include "Game/Render/NetMesh.h"
#include "Game/Render/SkinAnimatedMovableNPC.h"
#include "Game/Effects/EffectsGroup.h"
#include "Game/Effects/EmissionController.h"
#include "Game/Effects/EmissionManager.h"

extern CollisionSpace* g_CollisionSpace;
extern PhysicsWorld* g_PhysicsWorld;

/**
 * Offset/Address/Size: 0x9A4 | 0x8013C30C | size: 0x7C
 */
PhysicsShell::PhysicsShell(float radius)
    : PhysicsSphere(g_CollisionSpace, g_PhysicsWorld, radius)
{

    m_pTriggerCallbackFunc = 0;
    m_pCallbackParam = 0;
    m_pPowerupObject = 0;
    mbIsInNet = false;
    m_bIsSupportedByGround = false;

    SetCollide(0xef);
    SetCategory(0x20);
    m_gravity = -32.f;
}

/**
 * Offset/Address/Size: 0x10C | 0x8013BA74 | size: 0x898
 */
ContactType PhysicsShell::Contact(PhysicsObject* obj, dContact* info, int numContacts)
{
    nlVector3 myPos;
    bool bWasRicochet;
    cFielder* pFielder;
    ContactType eType;

    GetPosition(&myPos);
    bWasRicochet = false;
    eType = TWO_WAY_CONTACT;

    switch (obj->GetObjectType())
    {
    case 0x0D:
    case 0x0E:
    {
        if (((PhysicsCharacter*)obj->m_parentObject)->m_pAICharacter->m_eClassType == FIELDER)
        {
            pFielder = (cFielder*)((PhysicsCharacter*)obj->m_parentObject)->m_pAICharacter;
            if (m_pPowerupObject->mtNoHitTimer.m_uPackedTime != 0 && m_pPowerupObject->m_pThrower == pFielder)
            {
                return NO_CONTACT;
            }
            if (pFielder->IsFrozen())
            {
                eType = ONE_WAY_CONTACT_THIS;
                bWasRicochet = true;
            }

            if (!bWasRicochet && m_pTriggerCallbackFunc != NULL)
            {
                nlVector3 v3Pos;
                nlVec3Set(v3Pos, info->geom.pos[0], info->geom.pos[1], info->geom.pos[2]);
                m_pTriggerCallbackFunc(this, obj, v3Pos, m_pCallbackParam);
            }

            if (m_pPowerupObject->meSize == POWERUPSIZE_LARGE)
            {
                return NO_CONTACT;
            }

            if (m_pPowerupObject->m_eType == POWER_UP_SPINY_SHELL)
            {
                if (pFielder->IsFrozen() || pFielder->IsInvincible())
                {
                    eType = ONE_WAY_CONTACT_THIS;
                }
                else
                {
                    return NO_CONTACT;
                }
            }
        }
        else
        {
            bWasRicochet = true;
            Event* event = g_pEventManager->CreateValidEvent(0x23, 0x2C);
            CollisionPowerupGroundData* eventData = new (&event->m_data) CollisionPowerupGroundData();
            GetPosition(&eventData->position);
            eventData->eType = m_pPowerupObject->m_eType;
        }
        break;
    }

    case 0x0F:
    {
        numContacts = eType;
        cPlayer* owner = ((PhysicsAIBall*)obj)->m_pAIBall->m_pOwner;
        if (owner != NULL)
        {
            if (owner->m_eClassType == FIELDER)
            {
                if (m_pPowerupObject->mtNoHitTimer.m_uPackedTime != 0 && m_pPowerupObject->m_pThrower == (cFielder*)owner)
                {
                    return NO_CONTACT;
                }

                if (((cFielder*)owner)->IsBallAwayFromCarrier())
                {
                    if (m_pTriggerCallbackFunc != NULL)
                    {
                        nlVector3 v3Pos;
                        nlVec3Set(v3Pos, info->geom.pos[0], info->geom.pos[1], info->geom.pos[2]);
                        m_pTriggerCallbackFunc(this, obj, v3Pos, m_pCallbackParam);
                    }
                    return NO_CONTACT;
                }
            }
            else
            {
                bWasRicochet = true;
            }

            if (m_pPowerupObject->m_eType == POWER_UP_SPINY_SHELL)
            {
                numContacts = ONE_WAY_CONTACT_OTHER;
            }
        }
        else
        {
            bool canDamage = false;
            if (((PhysicsAIBall*)obj)->m_pAIBall->m_tShotTimer.m_uPackedTime != 0 && ((PhysicsAIBall*)obj)->m_pAIBall->mbCanDamage)
            {
                canDamage = true;
            }

            if (canDamage)
            {
                m_pPowerupObject->m_bShouldDestroy = true;
                return NO_CONTACT;
            }

            if (m_pPowerupObject->meSize != POWERUPSIZE_SMALL)
            {
                numContacts = ONE_WAY_CONTACT_OTHER;
            }
        }

        if (!bWasRicochet && m_pTriggerCallbackFunc != NULL)
        {
            nlVector3 v3Pos;
            nlVec3Set(v3Pos, info->geom.pos[0], info->geom.pos[1], info->geom.pos[2]);
            m_pTriggerCallbackFunc(this, obj, v3Pos, m_pCallbackParam);
        }

        if (numContacts == ONE_WAY_CONTACT_OTHER)
        {
            return ONE_WAY_CONTACT_OTHER;
        }
        break;
    }

    case 0x13:
    {
        if (m_pPowerupObject->mtNoHitTimer.m_uPackedTime != 0)
        {
            if (((PhysicsShell*)obj)->m_pPowerupObject->m_pThrower == m_pPowerupObject->m_pThrower)
            {
                return NO_CONTACT;
            }
        }

        if (m_pTriggerCallbackFunc != NULL)
        {
            nlVector3 v3Pos;
            nlVec3Set(v3Pos, info->geom.pos[0], info->geom.pos[1], info->geom.pos[2]);
            m_pTriggerCallbackFunc(this, obj, v3Pos, m_pCallbackParam);
        }

        ePowerupSize otherShellSize = ((PhysicsShell*)obj)->m_pPowerupObject->meSize;
        ePowerupSize myShellSize = m_pPowerupObject->meSize;
        if (myShellSize > otherShellSize)
        {
            return NO_CONTACT;
        }
        break;
    }

    case 0x14:
    {
        if (m_pPowerupObject->mtNoHitTimer.m_uPackedTime != 0)
        {
            if (((PhysicsBanana*)obj)->m_pPowerupObject->m_pThrower == m_pPowerupObject->m_pThrower)
            {
                return NO_CONTACT;
            }
        }

        if (m_pTriggerCallbackFunc != NULL)
        {
            nlVector3 v3Pos;
            nlVec3Set(v3Pos, info->geom.pos[0], info->geom.pos[1], info->geom.pos[2]);
            m_pTriggerCallbackFunc(this, obj, v3Pos, m_pCallbackParam);
        }

        if (((PhysicsBanana*)obj)->m_pPowerupObject->meSize != POWERUPSIZE_LARGE)
        {
            return NO_CONTACT;
        }
        break;
    }

    case 0x1A:
    {
        if (m_pPowerupObject->mtNoHitTimer.m_uPackedTime != 0)
        {
            if (((SkinAnimatedNPC*)((PhysicsNPC*)obj)->mpAINPC)->GetSkinAnimatedNPC_Type() == SkinAnimatedNPC_BOWSER)
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
        if (obj->GetObjectType() == 0x11)
        {
            for (int i = 0; i < numContacts; i++)
            {
                if (info[i].geom.pos[2] <= myPos.z && info[i].geom.normal[2] > 0.9f)
                {
                    if (!m_bIsSupportedByGround)
                    {
                        m_bIsSupportedByGround = true;
                    }

                    if (!mbIsInNet && m_pPowerupObject->mtNoHitTimer.m_uPackedTime == 0)
                    {
                        nlVector3 v3IncidentVel;
                        GetLinearVelocity(&v3IncidentVel);
                        if (v3IncidentVel.z < -1.0f)
                        {
                            Event* event = g_pEventManager->CreateValidEvent(0x22, 0x2C);
                            CollisionPowerupGroundData* eventData = new (&event->m_data) CollisionPowerupGroundData();
                            GetPosition(&eventData->position);
                            eventData->fVecZComponent = v3IncidentVel.z;
                            eventData->eType = m_pPowerupObject->m_eType;
                        }
                    }
                    break;
                }
            }
        }

        nlVector3 v3PowerupPosition;
        GetPosition(&v3PowerupPosition);
        float fPowerupRadius = GetRadius();

        if (mbIsInNet)
        {
            float fNetWidth = cNet::m_fNetWidth;
            double fAbsPowerupY = __fabs(v3PowerupPosition.y);
            float fNetLimitY = 0.5f * fNetWidth - fPowerupRadius;
            if ((float)fAbsPowerupY > fNetLimitY)
            {
                m_pPowerupObject->m_bShouldDestroy = true;
                return NO_CONTACT;
            }

            float fMaxX;
            if (v3PowerupPosition.x > 0.0f)
            {
                NetMesh* pMesh = NetMesh::spPositiveXNetMesh;
                if (v3PowerupPosition.x > 0.0f)
                {
                    fMaxX = pMesh->mfMaxX;
                }
                else
                {
                    fMaxX = pMesh->mfMinX;
                }
            }
            else
            {
                NetMesh* pMesh = NetMesh::spNegativeXNetMesh;
                if (v3PowerupPosition.x > 0.0f)
                {
                    fMaxX = pMesh->mfMaxX;
                }
                else
                {
                    fMaxX = pMesh->mfMinX;
                }
            }

            if ((float)fabs(v3PowerupPosition.x) > (float)fabs(fMaxX) - 2.0f * fPowerupRadius)
            {
                m_pPowerupObject->m_bShouldDestroy = true;
                return NO_CONTACT;
            }

            double fPowerupZ = v3PowerupPosition.z;
            float fNetHeight = cNet::m_fNetHeight;
            double fAbsPowerupZ = __fabs(fPowerupZ);
            float fNetLimit = fNetHeight - fPowerupRadius;
            if ((float)fAbsPowerupZ > fNetLimit)
            {
                m_pPowerupObject->m_bShouldDestroy = true;
                return NO_CONTACT;
            }
        }

        float fNetWidth = cNet::m_fNetWidth;
        double fAbsPowerupY = __fabs(v3PowerupPosition.y);
        float fNetLimitY = 0.5f * fNetWidth - fPowerupRadius;
        if ((float)fAbsPowerupY < fNetLimitY)
        {
            if ((float)fabs(v3PowerupPosition.x) > cField::GetGoalLineX(1u) - fPowerupRadius)
            {
                double fPowerupZ = v3PowerupPosition.z;
                float fNetHeight = cNet::m_fNetHeight;
                double fAbsPowerupZ = __fabs(fPowerupZ);
                float fNetLimit = fNetHeight - fPowerupRadius;
                if ((float)fAbsPowerupZ < fNetLimit)
                {
                    mbIsInNet = true;
                    return NO_CONTACT;
                }
            }
        }

        for (int i = 0; i < numContacts; i++)
        {
            if (info[i].geom.normal[2] < 0.08f)
            {
                if (m_pPowerupObject->mtActiveTimer.m_uPackedTime == 0)
                {
                    m_pPowerupObject->m_bShouldDestroy = true;
                }
                else if (!mbIsInNet)
                {
                    bWasRicochet = true;
                }
            }
        }
        break;
    }
    }

    if (bWasRicochet)
    {
        if (obj->GetObjectType() == 0x19)
        {
            Event* event = g_pEventManager->CreateValidEvent(0x21, 0x3C);
            CollisionPowerupWallData* eventData = new (&event->m_data) CollisionPowerupWallData();
            eventData->eSize = m_pPowerupObject->meSize;
            eventData->eType = m_pPowerupObject->m_eType;
            nlVec3Set(eventData->position, info->geom.pos[0], info->geom.pos[1], info->geom.pos[2]);
            nlVec3Set(eventData->normal, info->geom.normal[0], info->geom.normal[1], info->geom.normal[2]);
        }

        if (obj->GetObjectType() != 0x0F || (obj->GetObjectType() == 0x0F && m_pPowerupObject->m_pTarget != NULL))
        {
            EffectsGroup* pGroup = fxGetGroup("shell_ricochet");
            EmissionController* pControl = EmissionManager::Create(pGroup, 0);

            nlVector3 v3Position;
            nlVector3 v3Velocity;
            nlVector3 v3Direction = { 0.0f, 0.0f, 1.0f };
            GetPosition(&v3Position);
            GetLinearVelocity(&v3Velocity);

            pControl->SetPosition(v3Position);
            pControl->SetDirection(v3Direction);
            pControl->SetVelocity(v3Velocity);
        }

        m_pPowerupObject->m_pTarget = NULL;
    }

    return eType;
}

/**
 * Offset/Address/Size: 0x94 | 0x8013B9FC | size: 0x78
 */
void PhysicsShell::PostUpdate()
{
    PhysicsObject::PostUpdate();

    nlVector3 velocity;
    GetLinearVelocity(&velocity);

    nlVector3& pos = GetPosition();
    if (pos.z > 20.0f && velocity.z > 0.0f)
    {
        velocity.z *= 0.9f;
        SetLinearVelocity(velocity);
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8013B968 | size: 0x94
 */
bool PhysicsShell::SetContactInfo(dContact* contact, PhysicsObject* other, bool first)
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
        contact->surface.bounce = g_pGame->m_pGameTweaks->fShellBounce;
    }

    contact->surface.bounce_vel = 0.0f;
    contact->surface.mu = 0.005f;

    return true;
}
