#include <stdio.h>
#include <stdlib.h>
#include "port/input.h"
#include "Game/Physics/PhysicsCharacter.h"
#include "Game/Physics/PhysicsColumn.h"
#include "Game/Physics/PhysicsFakeBall.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Ball.h"
#include "Game/Net.h"
#include "Game/AI/Fielder.h"
#include "Game/Sys/eventman.h"
#include "NL/nlMemory.h"
#include "NL/nlBasicString.h"
#include "NL/nlSingleton.h"
#include "Game/GameInfo.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/MathHelpers.h"

extern CollisionSpace* g_CollisionSpace;
extern PhysicsWorld* g_PhysicsWorld;
extern cBall* g_pBall;

static bool sbDoDKBallStuckHack = true;
static float sfBallStuckHackShoveMagnitude = 10.0f;

// PPC `rlwimi dest, src, shift, bit, bit`: rotate `src` left by `shift` and insert it into `dest` under the one-bit mask at PPC bit `bit` (bit 0 = MSB).
#define SET_BIT(dest, src, shift, bit)                                         \
    ((dest) = ((dest) & ~(0x80000000u >> (bit))) |                             \
              ((((unsigned int)(src) << (shift)) |                             \
                ((unsigned int)(src) >> ((32 - (shift)) & 31))) &              \
               (0x80000000u >> (bit))))
/**
 * Offset/Address/Size: 0x10D0 | 0x801372E8 | size: 0xFC
 */
PhysicsCharacter::PhysicsCharacter(float radius, float heightScale)
    : PhysicsCharacterBase(g_CollisionSpace, g_PhysicsWorld, radius + (heightScale / 2.0f))
{
    // PORT: was three bit pokes through `this + 0x80`, which is where this word lives on console and is m_parentObject here.
    m_CanCollideWithWall = 1;
    m_CanCollideWithBall = 1;
    m_CanCollidedWithGoalLine = 1;

    m_nDKBallStuckHackCounter = 0;
    m_pAICharacter = NULL;

    m_gravity = 0.0f;
    SetMass(100.0f);

    PhysicsColumn* column = (PhysicsColumn*)nlMalloc(sizeof(PhysicsColumn), 8, false);
    column = new (column) PhysicsColumn(g_CollisionSpace, g_PhysicsWorld, radius);
    m_pPlayerPlayerColumn = column;
    m_pPlayerPlayerColumn->SetCategory(0x40);
    m_pPlayerPlayerColumn->SetCollide(0x5F);
    AddObject(m_pPlayerPlayerColumn);
}

/**
 * Offset/Address/Size: 0x10AC | 0x801372C4 | size: 0x24
 */
void PhysicsCharacter::GetRadius(float* radius)
{
    m_pPlayerPlayerColumn->GetRadius(radius);
}

/**
 * Offset/Address/Size: 0x103C | 0x80137254 | size: 0x70
 */
bool PhysicsCharacter::SetContactInfo(dContact* contact, PhysicsObject* other, bool first)
{
    bool result = BaseSetContactInfo(contact, other, first);
    int objectType = other->GetObjectType();
    if (objectType == 0xF || objectType == 0x10)
    {
        contact->surface.bounce = 0.2f;
    }
    return result;
}

/**
 * Offset/Address/Size: 0xB24 | 0x80136D3C | size: 0x518
 */
ContactType PhysicsCharacter::Contact(PhysicsObject* pOther, dContact* contacts,
    int numContacts, PhysicsObject* pOtherOrig)
{
    int objectType = pOther->GetObjectType();
    {
        static const bool bProbe = getenv("STRIKERS_PROBE_CHARDRAW") != NULL;
        if (bProbe)
        {
            for (int ci = 0; ci < numContacts; ci++)
            {
                if (contacts[ci].geom.depth <= 0.33)
                    continue;
                fprintf(stderr, "[contact] frame %lu: depth %.3f vs objType 0x%X "
                        "at %.1f,%.1f,%.1f normal %.2f,%.2f,%.2f\n",
                        PortInputFrame(), (double)contacts[ci].geom.depth, objectType,
                        (double)contacts[ci].geom.pos[0], (double)contacts[ci].geom.pos[1],
                        (double)contacts[ci].geom.pos[2],
                        (double)contacts[ci].geom.normal[0],
                        (double)contacts[ci].geom.normal[1],
                        (double)contacts[ci].geom.normal[2]);
            }
        }
    }
    if (objectType == 0x12)
        return NO_CONTACT;
    if (objectType == 0x7)
        return NO_CONTACT;
    if (!m_CanCollidedWithGoalLine && objectType == 0x19)
    {
        nlVector3 pos;
        GetPosition(&pos);
        float radius;
        m_pPlayerPlayerColumn->GetRadius(&radius);
        float netWidth = cNet::m_fNetWidth;
        float half = 0.5f;
        if (fabsf(pos.y) < netWidth * half - radius)
            return NO_CONTACT;
    }
    if (objectType == 0x19 || objectType == 0x5)
    {
        for (int i = 0; i < numContacts; i++)
        {
            if (contacts[i].geom.normal[2] < 0.08f)
            {
                Event* event = g_pEventManager->CreateValidEvent(0x1F, 0x34);
                CollisionPlayerWallData* wallData = new (&event->m_data) CollisionPlayerWallData();
                wallData->pPlayer = (cPlayer*)m_pAICharacter;
                nlVec3Set(wallData->contactPoint, contacts->geom.pos[0], contacts->geom.pos[1], contacts->geom.pos[2]);
                nlVec3Set(wallData->wallNormal, contacts->geom.normal[0], contacts->geom.normal[1], contacts->geom.normal[2]);
            }
        }
        if (!m_CanCollideWithWall)
            return NO_CONTACT;
    }
    if (objectType == 0xF)
    {
        if (contacts->geom.normal[2] > contacts->geom.normal[0] && contacts->geom.normal[2] > contacts->geom.normal[1])
            m_IsOnTopOfBall = 1;
        if (!m_CanCollideWithBall)
            return NO_CONTACT;
        cBall* ball = ((PhysicsAIBall*)pOther)->m_pAIBall;
        bool canDamage = false;
        if (ball->m_tShotTimer.m_uPackedTime != 0 && ball->mbCanDamage)
            canDamage = true;
        if (canDamage)
            return NO_CONTACT;
        if (!m_HasCollidedWithBall)
        {
            Event* event = g_pEventManager->CreateValidEvent(0x27, 0x30);
            CollisionPlayerBallData* ballData = new (&event->m_data) CollisionPlayerBallData();
            ballData->pPlayer = (cPlayer*)m_pAICharacter;
            ballData->pBall = ((PhysicsAIBall*)pOther)->m_pAIBall;
            ballData->velocity = pOther->GetLinearVelocity();
            ballData->boneID = GetBoneIDForSubObject(pOtherOrig);
            m_HasCollidedWithBall = 1;
        }
        if (pOther->m_parentObject == NULL)
            FakeBallWorld::InvalidateBallCache();
        if (ball->m_pOwner != NULL)
        {
            cFielder* fielder = (cFielder*)m_pAICharacter;
            if (fielder->IsFallenDown(0.0f))
            {
                if (fielder->IsCharacterInAir(true))
                    return NO_CONTACT;
                if (fielder->m_eAnimID != 0x74 && fielder->m_eAnimID != 0x75)
                    return ONE_WAY_CONTACT_THIS;
                return ONE_WAY_CONTACT_OTHER;
            }
        }
        return ONE_WAY_CONTACT_OTHER;
    }
    ContactType contactType = TWO_WAY_CONTACT;
    if (objectType == 0x4)
    {
        PhysicsCharacter* otherPhysChar = (PhysicsCharacter*)pOther->m_parentObject;
        if (m_pAICharacter->m_eClassType == FIELDER && otherPhysChar->m_pAICharacter->m_eClassType == FIELDER)
        {
            cFielder* fielder = (cFielder*)m_pAICharacter;
            cFielder* otherFielder = (cFielder*)otherPhysChar->m_pAICharacter;
            if (fielder->IsCharacterInAir(true))
            {
                if (!otherFielder->IsCharacterInAir(true))
                    return NO_CONTACT;
            }
            else if (otherFielder->IsCharacterInAir(true))
            {
                if (!fielder->IsCharacterInAir(true))
                    return NO_CONTACT;
            }
            else
            {
                if (fielder->IsFrozen())
                    return ONE_WAY_CONTACT_OTHER;
                if (otherFielder->IsFrozen())
                    return ONE_WAY_CONTACT_THIS;
                if (fielder->IsFallenDown(0.0f))
                {
                    if (!otherFielder->IsHitting())
                    {
                        if (fielder->m_eAnimID != 0x74 && fielder->m_eAnimID != 0x75)
                            return ONE_WAY_CONTACT_THIS;
                        return ONE_WAY_CONTACT_OTHER;
                    }
                    return TWO_WAY_CONTACT;
                }
                if (otherFielder->IsFallenDown(0.0f))
                {
                    if (!fielder->IsHitting())
                    {
                        if (otherFielder->m_eAnimID != 0x74 && otherFielder->m_eAnimID != 0x75)
                            return ONE_WAY_CONTACT_OTHER;
                        return ONE_WAY_CONTACT_THIS;
                    }
                    return TWO_WAY_CONTACT;
                }
                if (fielder->IsInvincible())
                    contactType = ONE_WAY_CONTACT_OTHER;
                else if (otherFielder->IsInvincible())
                    contactType = ONE_WAY_CONTACT_THIS;
            }
        }
        {
            Event* event = g_pEventManager->CreateValidEvent(0x25, 0x38);
            CollisionPlayerPlayerData* playerData = new (&event->m_data) CollisionPlayerPlayerData();
            playerData->player1 = (cPlayer*)m_pAICharacter;
            playerData->player2 = (cPlayer*)otherPhysChar->m_pAICharacter;
            playerData->velocity1 = m_pAICharacter->m_v3Velocity;
            playerData->velocity2 = otherPhysChar->m_pAICharacter->m_v3Velocity;
        }
    }
    return contactType;
}

/**
 * Offset/Address/Size: 0xA8C | 0x80136CA4 | size: 0x98
 */
void PhysicsCharacter::PreCollide()
{
    BasePreCollide();
    m_HasCollidedWithBall = 0;

    nlVector3 force = { 0.f, 0.0f, 0.0f };
    force.y = -sfBallStuckHackShoveMagnitude;

    if (sbDoDKBallStuckHack && m_nDKBallStuckHackCounter > 5)
    {
        g_pBall->m_pPhysicsBall->AddForceAtCentreOfMass(force);
    }
    m_IsOnTopOfBall = 0;
}

/**
 * Offset/Address/Size: 0x708 | 0x80136920 | size: 0x384
 */
PhysicsBoneID PhysicsCharacter::ResolvePhysicsBoneIDFromName(const char* name)
{
    if (strcmpi("PHYSBONE_R_ARM", name) == 0)
    {
        return PHYSBONE_FIELDER_R_ARM;
    }

    if (strcmpi("PHYSBONE_L_ARM", name) == 0)
    {
        return PHYSBONE_FIELDER_L_ARM;
    }

    if (strcmpi("PHYSBONE_R_LEG", name) == 0)
    {
        return PHYSBONE_FIELDER_R_LEG;
    }

    if (strcmpi("PHYSBONE_L_LEG", name) == 0)
    {
        return PHYSBONE_FIELDER_L_LEG;
    }

    if (strcmpi("PHYSBONE_HEAD", name) == 0)
    {
        return PHYSBONE_FIELDER_HEAD;
    }

    if (strcmpi("PhySphere_Lhand", name) == 0)
    {
        return PHYSBONE_GOALIE_L_HAND;
    }

    if (strcmpi("PhySphere_Lwrist", name) == 0)
    {
        return PHYSBONE_GOALIE_L_WRIST;
    }

    if (strcmpi("PhySphere_Lforearm", name) == 0)
    {
        return PHYSBONE_GOALIE_L_FOREARM;
    }

    if (strcmpi("PhySphere_Lbicep", name) == 0)
    {
        return PHYSBONE_GOALIE_L_BICEP;
    }

    if (strcmpi("PhySphere_Lshoulder", name) == 0)
    {
        return PHYSBONE_GOALIE_L_SHOULDER;
    }

    if (strcmpi("PhySphere_Lthigh", name) == 0)
    {
        return PHYSBONE_GOALIE_L_THIGH;
    }

    if (strcmpi("PhySphere_Lthighlower", name) == 0)
    {
        return PHYSBONE_GOALIE_L_THIGHLOWER;
    }

    if (strcmpi("PhySphere_Lcalfupper", name) == 0)
    {
        return PHYSBONE_GOALIE_L_CALFUPPER;
    }

    if (strcmpi("PhySphere_Lheel", name) == 0)
    {
        return PHYSBONE_GOALIE_L_HEEL;
    }

    if (strcmpi("PhySphere_Ltoe", name) == 0)
    {
        return PHYSBONE_GOALIE_L_TOE;
    }

    if (strcmpi("PhySphere_Rhand", name) == 0)
    {
        return PHYSBONE_GOALIE_R_HAND;
    }

    if (strcmpi("PhySphere_Rwrist", name) == 0)
    {
        return PHYSBONE_GOALIE_R_WRIST;
    }

    if (strcmpi("PhySphere_Rforearm", name) == 0)
    {
        return PHYSBONE_GOALIE_R_FOREARM;
    }

    if (strcmpi("PhySphere_Rbicep", name) == 0)
    {
        return PHYSBONE_GOALIE_R_BICEP;
    }

    if (strcmpi("PhySphere_Rshoulder", name) == 0)
    {
        return PHYSBONE_GOALIE_R_SHOULDER;
    }

    if (strcmpi("PhySphere_Rthigh", name) == 0)
    {
        return PHYSBONE_GOALIE_R_THIGH;
    }

    if (strcmpi("PhySphere_Rthighlower", name) == 0)
    {
        return PHYSBONE_GOALIE_R_THIGHLOWER;
    }

    if (strcmpi("PhySphere_Rcalfupper", name) == 0)
    {
        return PHYSBONE_GOALIE_R_CALFUPPER;
    }

    if (strcmpi("PhySphere_Rheel", name) == 0)
    {
        return PHYSBONE_GOALIE_R_HEEL;
    }

    if (strcmpi("PhySphere_Rtoe", name) == 0)
    {
        return PHYSBONE_GOALIE_R_TOE;
    }

    if (strcmpi("PhySphere_head", name) == 0)
    {
        return PHYSBONE_GOALIE_HEAD;
    }

    return strcmpi("PhySphere_stomach", name) != 0 ? PHYSBONE_UNKNOWN : PHYSBONE_GOALIE_STOMACH;
}

/**
 * Offset/Address/Size: 0x100 | 0x80136318 | size: 0x608
 */
void PhysicsCharacter::PostUpdate()
{
    nlVector3 characterPosition;

    PhysicsObject::PostUpdate();
    GetPosition(&characterPosition);

    SetCharacterPositionXY(characterPosition);

    if (m_HasCollidedWithBall)
    {
        cBall* const pBall = g_pBall;
        if (pBall->m_unk_0xA6)
        {
            cCharacter* pCharacter = m_pAICharacter;
            if (pCharacter->m_eClassType == FIELDER)
            {
                cFielder* pFldr = (cFielder*)pCharacter;
                nlVector3 v3PrevOwnerToBall;
                nlVec3Sub(v3PrevOwnerToBall, pBall->m_v3Position, pBall->m_pPrevOwner->m_v3Position);
                u16 angle = RadToAng16(nlATan2f(v3PrevOwnerToBall.y, v3PrevOwnerToBall.x));

                if (!pFldr->IsInvincible())
                {
                    if (pFldr->m_pBall)
                    {
                        pFldr->ReleaseBall();
                    }

                    pBall->ClearBallEffects();

                    bool goalieCaught = false;
                    if (pBall->m_tShotTimer.m_uPackedTime != 0)
                    {
                        if (pBall->m_unk_0xA4)
                        {
                            goalieCaught = true;
                        }
                    }

                    if (goalieCaught)
                    {
                        EmitGoalieCatch(pFldr, "perfect_shot_catch", false);
                    }
                    else
                    {
                        cPlayer* pPrevOwner = g_pBall->m_pPrevOwner;
                        if (pPrevOwner != NULL && pPrevOwner->m_eClassType == GOALIE)
                        {
                            cTeam* otherTeam = pPrevOwner->m_pTeam->GetOtherTeam();
                            eTeamID teamID = nlSingleton<GameInfoManager>::Instance()->GetTeam((short)otherTeam->m_nSide);
                            BasicString<char, Detail::TempStringAllocator> effectName(GetTeamName(teamID));
                            effectName.AppendInPlace("_shoot_to_score_catch");
                            EmitGoalieCatch(pFldr, effectName.c_str(), false);
                        }
                    }

                    if (pFldr->InitActionHitReact(pBall->m_pPrevOwner, angle, true))
                    {
                        pFldr->PlayAttackReactionSounds(100.0f);
                    }
                }
                else
                {
                    pFldr->SetNoPickUpTime(0.3f);

                    pBall->ClearBallEffects();

                    bool goalieCaught = false;
                    if (pBall->m_tShotTimer.m_uPackedTime != 0)
                    {
                        if (pBall->m_unk_0xA4)
                        {
                            goalieCaught = true;
                        }
                    }

                    if (goalieCaught)
                    {
                        EmitGoalieCatch(pFldr, "perfect_shot_catch", false);
                    }
                    else
                    {
                        eTeamID teamID = nlSingleton<GameInfoManager>::Instance()->GetTeam((short)pFldr->m_pTeam->m_nSide);
                        BasicString<char, Detail::TempStringAllocator> effectName(GetTeamName(teamID));
                        effectName.AppendInPlace("_shoot_to_score_catch");
                        EmitGoalieCatch(pFldr, effectName.c_str(), true);
                    }
                }

                pBall->m_tNoPickupTimer.SetSeconds(0.0f);
                pBall->m_unk_0xA6 = false;
                pBall->mpDamageTarget = NULL;

                float fShotAlignment = nlVec3DotProduct(v3PrevOwnerToBall, pBall->GetVelocity());

                nlVector3 v3BallVel;
                if (fShotAlignment > 0.0f)
                {
                    nlVec3Scale(v3BallVel, pBall->GetVelocity(), -0.1f);

                    nlVector3 v3CharacterToBall;
                    nlVec3Sub(v3CharacterToBall, pBall->m_v3Position, characterPosition);
                    float fBounceAlignment = nlVec3DotProduct(v3CharacterToBall, v3BallVel);

                    if (fBounceAlignment < 0.0f)
                    {
                        pBall->SetPosition(pBall->m_v3PrevPosition);
                    }
                }
                else
                {
                    nlVec3Scale(v3BallVel, pBall->GetVelocity(), 0.1f);
                }

                v3BallVel.z += 4.0f + nlRandomf(3.0f, &nlDefaultSeed);
                v3BallVel.y += -4.0f + nlRandomf(8.0f, &nlDefaultSeed);

                nlVector3 v3BallSpin;
                pBall->m_pPhysicsBall->GetAngularVelocity(&v3BallSpin);
                pBall->SetVelocity(v3BallVel, SPINTYPE_PARAMETER, &v3BallSpin);
            }
        }
    }

    if (m_IsOnTopOfBall)
    {
        m_nDKBallStuckHackCounter++;
    }
    else
    {
        m_nDKBallStuckHackCounter = 0;
    }
}

/**
 * Offset/Address/Size: 0xBC | 0x801362D4 | size: 0x44
 */
void PhysicsCharacter::GetCharacterPositionXY(nlVector3* pos)
{
    float z = pos->z;
    GetPosition(pos);
    pos->z = z;
}

/**
 * Offset/Address/Size: 0x80 | 0x80136298 | size: 0x3C
 */
void PhysicsCharacter::SetCharacterPositionXY(const nlVector3& pos)
{
    nlVector3 v;
    v.x = pos.x;
    v.y = pos.y;
    v.z = 0.0f;
    SetCharacterPosition(v);
}

/**
 * Offset/Address/Size: 0x3C | 0x80136254 | size: 0x44
 */
void PhysicsCharacter::GetCharacterVelocityXY(nlVector3* vel)
{
    float z = vel->z;
    GetLinearVelocity(vel);
    vel->z = z;
}

/**
 * Offset/Address/Size: 0x0 | 0x80136218 | size: 0x3C
 */
void PhysicsCharacter::SetCharacterVelocityXY(const nlVector3& vel)
{
    nlVector3 v;
    v.x = vel.x;
    v.y = vel.y;
    v.z = 0.0f;
    SetLinearVelocity(v);
}
