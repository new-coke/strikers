#include "Game/AI/Powerups.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/AiUtil.h"
#include "Game/AI/Scripts/ScriptQuestions.h"
#include "Game/Render/Bowser.h"
#include "Game/Render/ChainChomp.h"
#include "Game/Render/NPCManager.h"
#include "Game/BasicStadium.h"
#include "Game/CharacterTriggers.h"
#include "Game/GameInfo.h"
#include "Game/Physics/PhysicsSphere.h"
#include "Game/Physics/PhysicsShell.h"
#include "Game/Physics/PhysicsBanana.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Physics/PhysicsCharacter.h"
#include "Game/Ball.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/Effects/EffectsGroup.h"
#include "Game/Game.h"
#include "Game/WorldManager.h"
#include "Game/Sys/audio.h"
#include "Game/Audio/WorldAudio.h"
#include "NL/nlMath.h"
#include "NL/nlFormat.h"
#include "NL/nlSlotPool.h"
#include "NL/nlString.h"
#include "NL/nlDebug.h"
#include "math.h"

static int gBobombAnticipationVoiceID = -1;
static f32 CANT_COLLIDE = *(f32*)__float_max;
unsigned long uPowerupTexID[NUM_POWER_UPS] = {
    nlStringLowerHash("fe/shell_green"),
    nlStringLowerHash("fe/shell_red"),
    nlStringLowerHash("fe/shell_spike"),
    nlStringLowerHash("fe/shell_blue"),
    nlStringLowerHash("fe/banana"),
    nlStringLowerHash("fe/babomb"),
    nlStringLowerHash("fe/chomp"),
    nlStringLowerHash("fe/mushroom"),
    nlStringLowerHash("fe/star"),
};
static PowerupSounds powerupSounds[9] = {
    // POWER_UP_GREEN_SHELL
    { 0x60, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x8A, 0x5F },
    // POWER_UP_RED_SHELL
    { 0x60, 0x65, 0x66, 0x67, 0x68, 0x69, 0x8A, 0x5F },
    // POWER_UP_SPINY_SHELL
    { 0x60, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0xFFFFFFFF, 0x5F },
    // POWER_UP_FREEZE_SHELL
    { 0x60, 0x71, 0x72, 0x73, 0x74, 0x75, 0x8A, 0x5F },
    // POWER_UP_BANANA
    { 0x60, 0x80, 0xFFFFFFFF, 0x82, 0x81, 0x81, 0x8A, 0x82 },
    // POWER_UP_BOBOMB
    { 0x60, 0x89, 0x88, 0x8B, 0xFFFFFFFF, 0xFFFFFFFF, 0x8A, 0x8A },
    // POWER_UP_CHAIN_CHOMP
    { 0x60, 0x8D, 0x8E, 0x8F, 0xFFFFFFFF, 0x90, 0xFFFFFFFF, 0xFFFFFFFF },
    // POWER_UP_MUSHROOM
    { 0x60, 0x7D, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7E },
    // POWER_UP_STAR
    { 0x60, 0x84, 0xFFFFFFFF, 0x85, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x86 },
};
PowerupBase* g_pPowerups[25] = { 0 };

namespace
{
struct Pair
{
    /* 0x0 */ unsigned long hashId;
    /* 0x4 */ const PowerupBase* powerup;
}; // total size: 0x8

struct PowerupRegistry
{
    PowerupRegistry()
    {
        for (int i = 0; i < NUM_POWERUP_REGISTRY_ENTRIES; i++)
        {
            registry[i].hashId = 0;
        }
    }

    enum
    {
        NUM_POWERUP_REGISTRY_ENTRIES = 25
    };

    /* 0x0 */ Pair registry[NUM_POWERUP_REGISTRY_ENTRIES];
}; // total size: 0xC8

PowerupModelPool powerupModelPool;
PowerupRegistry powerupRegistry;

const char* uFREEZE_SHELL_STREAK_TEXTURE = "global/blueshellstreak";
const char* uSPINY_SHELL_STREAK_TEXTURE = "global/redshellstreak";
const char* uGREEN_SHELL_STREAK_TEXTURE = "global/greenshellstreak";
const char* uRED_SHELL_STREAK_TEXTURE = "global/redshellstreak";
const char* uBOBOMB_STREAK_TEXTURE = "global/bobombstreak";

} // namespace

static const nlVector3 v3Zero = { 0.0f, 0.0f, 0.0f };
static u8 gbAlwaysSurround;

SlotPool<FreezeShell> FreezeShell::m_FreezeShellSlotPool(16, 16);
SlotPool<GreenShell> GreenShell::m_GreenShellSlotPool(16, 16);
SlotPool<SpinyShell> SpinyShell::m_SpinyShellSlotPool(16, 16);
SlotPool<RedShell> RedShell::m_RedShellSlotPool(16, 16);
SlotPool<Banana> Banana::m_BananaSlotPool(16, 16);
SlotPool<Bobomb> Bobomb::m_BobombSlotPool(16, 16);

namespace
{
const unsigned long uFREEZE_SHELL_MASTER_OBJECT = nlStringLowerHash("gameplay/blueshell");
const unsigned long uSPINY_SHELL_MASTER_OBJECT = nlStringLowerHash("gameplay/spikeshell");
const unsigned long uGREEN_SHELL_MASTER_OBJECT = nlStringLowerHash("gameplay/greenshell");
const unsigned long uRED_SHELL_MASTER_OBJECT = nlStringLowerHash("gameplay/redshell");
const unsigned long uBANANA_MASTER_OBJECT = nlStringLowerHash("gameplay/banana");
const unsigned long uBOBOMB_MASTER_OBJECT = nlStringLowerHash("gameplay/bobomb");
const char* uBANANA_STREAK_TEXTURE;
} // namespace

//  */
// void FormatImpl<BasicString<char, Detail::TempStringAllocator>>::operator%<int>(const int&)
// {
// }

//  */
// void Format<BasicString<char, Detail::TempStringAllocator>, int>(const BasicString<char, Detail::TempStringAllocator>&, const int&)
// {
// }

/**
 * Offset/Address/Size: 0x5EBC | 0x800607A8 | size: 0x1A0
 */
cFielder* FindPowerupTarget(cFielder* pThrower, Bowser* pBowser)
{
    float fBestScore = 99999.9f;
    cFielder* pBestCandidate = NULL;
    cTeam* pTeam;
    unsigned short aDirection;
    int i;
    float fTempScore;
    cFielder* pCandidate;

    if (pThrower != NULL)
    {
        pTeam = pThrower->m_pTeam->GetOtherTeam();
        aDirection = pThrower->m_aActualFacingDirection;
    }
    else
    {
        int team = nlRandom(2, &nlDefaultSeed);
        pTeam = g_pTeams[team];
        aDirection = pBowser->maFacingDirection;
    }

    for (i = 0; i < 4; i++)
    {
        fTempScore = 99999.9f;
        pCandidate = pTeam->GetFielder(i);

        if (!pCandidate->IsFallenDown(0.0f) && !pCandidate->IsFrozen())
        {
            if (pThrower != NULL)
            {
                if (pThrower->m_pController != NULL)
                {
                    float fMag = pThrower->m_pController->GetMovementStickMagnitude();
                    if (fMag)
                    {
                        aDirection = pThrower->m_pController->GetMovementStickDirection();
                    }
                }
                fTempScore = pThrower->DoFlashLight(
                    pCandidate->m_v3Position, aDirection, g_pGame->m_pGameTweaks->fAngleWeighting, 0.0f, 9999.0f);
            }
            else
            {
                fTempScore = cPlayer::DoFlashLight(
                    pBowser->mv3Position, pCandidate->m_v3Position, aDirection, g_pGame->m_pGameTweaks->fAngleWeighting, 0.0f, 9999.0f);
            }
        }

        if (g_pBall->GetOwnerFielder() == pCandidate || g_pBall->GetPassTargetFielder() == pCandidate)
        {
            fTempScore = 0.0f;
        }

        if (fTempScore < fBestScore)
        {
            pBestCandidate = pCandidate;
            fBestScore = fTempScore;
        }
    }

    if (pBestCandidate == NULL)
    {
        pBestCandidate = pTeam->GetStriker();
    }

    return pBestCandidate;
}

/**
 * Offset/Address/Size: 0x5998 | 0x80060284 | size: 0x524
 */
void PowerupThrowPosition(int nThrowOrder, eThrowStyle eStyle, PowerupBase* pNewPowerup, PowerupBase* pFirstPowerup)
{
    f32 fPowerupOffSet = 2.0f * ((PhysicsSphere*)pFirstPowerup->m_pPhysicsObject)->GetRadius();
    fPowerupOffSet += 0.5f;

    switch (eStyle)
    {
    case THROW_HORIZONTAL_LINE:
    {
        nlVector3 v3StartPosition;
        nlVector3 v3VelocityDirection;
        nlVector3 v3PerpToVelocity;

        pNewPowerup->m_v3Velocity = pFirstPowerup->m_v3Velocity;
        pNewPowerup->m_pPhysicsObject->SetLinearVelocity(pFirstPowerup->m_v3Velocity);

        v3VelocityDirection = pFirstPowerup->m_v3Velocity;
        v3VelocityDirection.z = 0.0f;
        f32 invLen = nlRecipSqrt(v3VelocityDirection.x * v3VelocityDirection.x + v3VelocityDirection.y * v3VelocityDirection.y + v3VelocityDirection.z * v3VelocityDirection.z, true);
        nlVec3Set(v3VelocityDirection, invLen * v3VelocityDirection.x, invLen * v3VelocityDirection.y, invLen * v3VelocityDirection.z);

        if (nThrowOrder % 2 == 0)
        {
            RotateVectorZAxis(v3PerpToVelocity, v3VelocityDirection, 0x4000);
        }
        else
        {
            RotateVectorZAxis(v3PerpToVelocity, v3VelocityDirection, 0xC000);
        }

        fPowerupOffSet *= (f32)((nThrowOrder + 1) / 2);
        nlVec3Set(v3PerpToVelocity, fPowerupOffSet * v3PerpToVelocity.x, fPowerupOffSet * v3PerpToVelocity.y, fPowerupOffSet * v3PerpToVelocity.z);
        nlVec3Set(v3StartPosition, pFirstPowerup->m_v3Position.x + v3PerpToVelocity.x, pFirstPowerup->m_v3Position.y + v3PerpToVelocity.y, pFirstPowerup->m_v3Position.z + v3PerpToVelocity.z);

        pNewPowerup->m_v3Position = v3StartPosition;
        pNewPowerup->m_pPhysicsObject->SetPosition(pNewPowerup->m_v3Position, PhysicsObject::WORLD_COORDINATES);
        break;
    }
    case THROW_ARROW:
    {
        nlVector3 v3StartPosition;
        nlVector3 v3VelocityDirection;
        nlVector3 v3PerpToVelocity;

        pNewPowerup->m_v3Velocity = pFirstPowerup->m_v3Velocity;
        pNewPowerup->m_pPhysicsObject->SetLinearVelocity(pFirstPowerup->m_v3Velocity);

        v3VelocityDirection = pFirstPowerup->m_v3Velocity;
        v3VelocityDirection.z = 0.0f;
        f32 invLen = nlRecipSqrt(v3VelocityDirection.x * v3VelocityDirection.x + v3VelocityDirection.y * v3VelocityDirection.y + v3VelocityDirection.z * v3VelocityDirection.z, true);
        nlVec3Set(v3VelocityDirection, invLen * v3VelocityDirection.x, invLen * v3VelocityDirection.y, invLen * v3VelocityDirection.z);

        if (nThrowOrder % 2 == 0)
        {
            RotateVectorZAxis(v3PerpToVelocity, v3VelocityDirection, 0x4000);
        }
        else
        {
            RotateVectorZAxis(v3PerpToVelocity, v3VelocityDirection, 0xC000);
        }

        fPowerupOffSet *= (f32)((nThrowOrder + 1) / 2);

        nlVec3Set(v3PerpToVelocity, fPowerupOffSet * v3PerpToVelocity.x, fPowerupOffSet * v3PerpToVelocity.y, fPowerupOffSet * v3PerpToVelocity.z);
        nlVec3Set(v3VelocityDirection, fPowerupOffSet * v3VelocityDirection.x, fPowerupOffSet * v3VelocityDirection.y, fPowerupOffSet * v3VelocityDirection.z);

        RotateVectorZAxis(v3VelocityDirection, v3VelocityDirection, 0x8000);

        nlVec3Set(v3StartPosition,
            pFirstPowerup->m_v3Position.x + (v3PerpToVelocity.x + v3VelocityDirection.x),
            pFirstPowerup->m_v3Position.y + (v3PerpToVelocity.y + v3VelocityDirection.y),
            pFirstPowerup->m_v3Position.z + (v3PerpToVelocity.z + v3VelocityDirection.z));

        pNewPowerup->m_v3Position = v3StartPosition;
        pNewPowerup->m_pPhysicsObject->SetPosition(pNewPowerup->m_v3Position, PhysicsObject::WORLD_COORDINATES);
        break;
    }
    case THROW_SURROUND:
    {
        pNewPowerup->m_v3Position = pFirstPowerup->m_v3Position;
        pNewPowerup->m_pPhysicsObject->SetPosition(pNewPowerup->m_v3Position, PhysicsObject::WORLD_COORDINATES);

        nlVector3 v3CurrentVelocity;
        v3CurrentVelocity = pFirstPowerup->m_v3Velocity;

        s16 nFlipAngle = (s16)(((nThrowOrder + 1) / 2) * 0x3333);
        if (nThrowOrder % 2 != 0)
        {
            nFlipAngle = -nFlipAngle;
        }
        RotateVectorZAxis(v3CurrentVelocity, v3CurrentVelocity, (u16)nFlipAngle);

        pNewPowerup->m_v3Velocity = v3CurrentVelocity;
        pNewPowerup->m_pPhysicsObject->SetLinearVelocity(v3CurrentVelocity);
        break;
    }
    case THROW_SPREAD:
    {
        pNewPowerup->m_v3Position = pFirstPowerup->m_v3Position;
        pNewPowerup->m_pPhysicsObject->SetPosition(pNewPowerup->m_v3Position, PhysicsObject::WORLD_COORDINATES);

        nlVector3 v3CurrentVelocity;
        v3CurrentVelocity = pFirstPowerup->m_v3Velocity;

        s16 nFlipAngle = (s16)(((nThrowOrder + 1) / 2) * 0x1999);
        if (nThrowOrder % 2 != 0)
        {
            nFlipAngle = -nFlipAngle;
        }
        RotateVectorZAxis(v3CurrentVelocity, v3CurrentVelocity, (u16)nFlipAngle);

        pNewPowerup->m_v3Velocity = v3CurrentVelocity;
        pNewPowerup->m_pPhysicsObject->SetLinearVelocity(v3CurrentVelocity);
        break;
    }
    default:
        break;
    }
}

/**
 * Offset/Address/Size: 0x4F00 | 0x8005F7EC | size: 0xA98
 */
u8 PowerupCreateAndThrow(cFielder* pThrower, ePowerUpType eType, int nnumOfPowerups, Bowser* pBowser)
{
    eThrowStyle eStyle;
    float fMediumChance;
    float fBigChance;
    float fExplodeChance;
    float fRandom;
    float fArrowChance;
    float fSpreadChance;
    float fSurroundChance;
    float fHorizChance;
    PowerupBase* pPowerup;
    cTeam* pTargetTeam;
    ePowerupSize eSize;
    bool bExplode;
    cFielder* pTargetFielders[4];
    int j;
    u8 bFoundLocation;
    float fBananaRadius;
    float fBobombRadius;
    float fGreenShellRadius;
    float fFreezeShellRadius;
    float fRedShellRadius;
    float fSpinyShellRadius;
    eStyle = THROW_ARROW;
    eSize = POWERUPSIZE_SMALL;
    fMediumChance = 0.0f;
    fBigChance = 0.0f;
    fExplodeChance = 0.0f;

    switch (eType)
    {
    case POWER_UP_BANANA:
        fMediumChance = g_pGame->m_pGameTweaks->fBananaMediumChance;
        fBigChance = fMediumChance + g_pGame->m_pGameTweaks->fBananaBigChance;
        fExplodeChance = g_pGame->m_pGameTweaks->fBananaExplodeChance;
        break;
    case POWER_UP_GREEN_SHELL:
    case POWER_UP_RED_SHELL:
    case POWER_UP_SPINY_SHELL:
    case POWER_UP_FREEZE_SHELL:
        fMediumChance = g_pGame->m_pGameTweaks->fShellMediumChance;
        fBigChance = fMediumChance + g_pGame->m_pGameTweaks->fShellBigChance;
        fExplodeChance = g_pGame->m_pGameTweaks->fShellExplodeChance;
        break;
    case POWER_UP_BOBOMB:
        fMediumChance = g_pGame->m_pGameTweaks->fBobombMediumChance;
        fBigChance = fMediumChance + g_pGame->m_pGameTweaks->fBobombBigChance;
        fExplodeChance = g_pGame->m_pGameTweaks->fBobombMineChance;
        break;
    default:
        break;
    }

    if (pThrower != NULL)
    {
        fBigChance += ((FielderTweaks*)pThrower->m_pTweaks)->fChanceForBig;
    }
    else if (pBowser != NULL)
    {
        fBigChance += nlRandomf(g_pGame->m_pGameTweaks->fBowserBigPowerupBoost, &nlDefaultSeed);
    }

    if (pBowser == NULL)
    {
        if ((nlSingleton<GameInfoManager>::Instance()->GetCustomPowerups() == CP_GIANT) && (nnumOfPowerups == 1))
        {
            fMediumChance = 0.0f;
            fBigChance = 1.0f;
        }
        else if (nlSingleton<GameInfoManager>::Instance()->GetCustomPowerups() == CP_EXPLOSIVE)
        {
            fExplodeChance = 1.0f;
        }
    }

    bExplode = false;

    if (nnumOfPowerups > 1)
    {
        if (eType == POWER_UP_RED_SHELL)
        {
            eStyle = THROW_SURROUND;
        }
        else
        {
            fRandom = nlRandomf(1.0f, &nlDefaultSeed);
            fArrowChance = g_pGame->m_pGameTweaks->fPowerupArrowThrowChance;
            fSpreadChance = fArrowChance + g_pGame->m_pGameTweaks->fPowerupSpreadThrowChance;
            fSurroundChance = fSpreadChance + g_pGame->m_pGameTweaks->fPowerupSurroundThrowChance;
            fHorizChance = fSurroundChance + g_pGame->m_pGameTweaks->fPowerupHorizontalLineThrowChance;

            if (fRandom < fArrowChance)
            {
                eStyle = THROW_ARROW;
            }
            else if ((fRandom < fSpreadChance) && (eType != POWER_UP_BANANA) && (eType != POWER_UP_SPINY_SHELL))
            {
                eStyle = THROW_SPREAD;
            }
            else if ((fRandom < fSurroundChance) && (eType != POWER_UP_BANANA) && (eType != POWER_UP_SPINY_SHELL))
            {
                eStyle = THROW_SURROUND;
            }
            else if (fRandom < fHorizChance)
            {
                eStyle = THROW_HORIZONTAL_LINE;
            }
        }

        if (gbAlwaysSurround)
        {
            eStyle = THROW_SURROUND;
        }
    }
    else
    {
        fRandom = nlRandomf(1.0f, &nlDefaultSeed);
        if (fRandom < fExplodeChance)
        {
            bExplode = true;
        }
        else if (pThrower == NULL)
        {
            bExplode = true;
        }

        fRandom = nlRandomf(1.0f, &nlDefaultSeed);
        if (fRandom < fMediumChance)
        {
            eSize = POWERUPSIZE_MEDIUM;
        }
        else if (fRandom < fBigChance)
        {
            eSize = POWERUPSIZE_LARGE;
        }

        if (pThrower != NULL)
        {
            cFielder* pFielder = pThrower;
            if (pThrower->IsCaptain())
            {
                pFielder = pThrower->m_pTeam->GetFielder(1);
            }

            switch (pFielder->m_eCharacterClass)
            {
            case BIRDO:
                if (eType == POWER_UP_FREEZE_SHELL)
                {
                    bExplode = true;
                }
                break;
            case TOAD:
                if (eType == POWER_UP_RED_SHELL)
                {
                    bExplode = true;
                }
                break;
            case KOOPA:
                if (eType == POWER_UP_GREEN_SHELL)
                {
                    bExplode = true;
                }
                break;
            case HAMMERBROS:
                if (eType == POWER_UP_SPINY_SHELL)
                {
                    bExplode = true;
                }
                break;
            default:
                break;
            }
        }
    }
    PowerupBase* pFirstPowerup = NULL;
    cFielder* pTarget;

    if (pThrower != NULL)
    {
        pTarget = pThrower->m_pPowerupTarget;
        pTargetTeam = pThrower->m_pTeam->GetOtherTeam();
    }
    else
    {
        cFielder* pBowserTarget = pBowser->mpTarget;
        pTargetTeam = pBowserTarget->m_pTeam;
        pTarget = pBowserTarget;

        if ((eSize == POWERUPSIZE_SMALL) && (nnumOfPowerups == 1))
        {
            eSize = POWERUPSIZE_MEDIUM;
        }
    }

    for (int a = 0; a < 4; a++)
    {
        pTargetFielders[a] = pTargetTeam->GetFielder(a);
    }

    for (j = 0; j < nnumOfPowerups; j++)
    {
        bFoundLocation = false;

        for (int i = 0; i < 25; i++)
        {
            if (bFoundLocation)
            {
                continue;
            }

            if (g_pPowerups[i] != NULL)
            {
                continue;
            }

            switch (eType)
            {
            case POWER_UP_BANANA:
            {
                switch (pThrower->m_eCharacterClass)
                {
                case DONKEYKONG:
                    eSize = POWERUPSIZE_MEDIUM;
                    break;
                default:
                    if (pThrower->m_pTeam->GetCaptain()->m_eCharacterClass == DONKEYKONG)
                    {
                        eSize = POWERUPSIZE_MEDIUM;
                    }
                    break;
                }

                switch (eSize)
                {
                case POWERUPSIZE_LARGE:
                    fBananaRadius = g_pGame->m_pGameTweaks->fBananaBigRadius;
                    break;
                case POWERUPSIZE_MEDIUM:
                    fBananaRadius = g_pGame->m_pGameTweaks->fBananaMediumRadius;
                    break;
                case POWERUPSIZE_SMALL:
                    fBananaRadius = g_pGame->m_pGameTweaks->fBananaSmallRadius;
                    break;
                }

                Banana* pBanana = NULL;
                if (Banana::m_BananaSlotPool.m_FreeList == NULL)
                {
                    SlotPoolBase::BaseAddNewBlock(&Banana::m_BananaSlotPool, sizeof(Banana));
                }

                if (Banana::m_BananaSlotPool.m_FreeList != NULL)
                {
                    pBanana = (Banana*)Banana::m_BananaSlotPool.m_FreeList;
                    Banana::m_BananaSlotPool.m_FreeList = Banana::m_BananaSlotPool.m_FreeList->next;
                }

                new (pBanana) Banana(pTarget, i, fBananaRadius, eSize, bExplode);

                pPowerup = pBanana;
                break;
            }
            case POWER_UP_BOBOMB:
            {
                switch (eSize)
                {
                case POWERUPSIZE_LARGE:
                    fBobombRadius = g_pGame->m_pGameTweaks->fBobombBigRadius;
                    break;
                case POWERUPSIZE_MEDIUM:
                    fBobombRadius = g_pGame->m_pGameTweaks->fBobombMediumRadius;
                    break;
                case POWERUPSIZE_SMALL:
                    fBobombRadius = g_pGame->m_pGameTweaks->fBobombSmallRadius;
                    break;
                }

                Bobomb* pBobomb = new Bobomb(pTarget, i, fBobombRadius, eSize, true);

                pPowerup = pBobomb;
                break;
            }
            case POWER_UP_GREEN_SHELL:
            {
                switch (eSize)
                {
                case POWERUPSIZE_LARGE:
                    fGreenShellRadius = g_pGame->m_pGameTweaks->fShellBigRadius;
                    break;
                case POWERUPSIZE_MEDIUM:
                    fGreenShellRadius = g_pGame->m_pGameTweaks->fShellMediumRadius;
                    break;
                case POWERUPSIZE_SMALL:
                    fGreenShellRadius = g_pGame->m_pGameTweaks->fShellSmallRadius;
                    break;
                }

                pPowerup = NULL;
                if (GreenShell::m_GreenShellSlotPool.m_FreeList == NULL)
                {
                    SlotPoolBase::BaseAddNewBlock(&GreenShell::m_GreenShellSlotPool, sizeof(GreenShell));
                }

                if (GreenShell::m_GreenShellSlotPool.m_FreeList != NULL)
                {
                    pPowerup = (GreenShell*)GreenShell::m_GreenShellSlotPool.m_FreeList;
                    GreenShell::m_GreenShellSlotPool.m_FreeList = GreenShell::m_GreenShellSlotPool.m_FreeList->next;
                }

                new (pPowerup) GreenShell(pTarget, i, fGreenShellRadius, eSize, bExplode);
                break;
            }
            case POWER_UP_FREEZE_SHELL:
            {
                switch (eSize)
                {
                case POWERUPSIZE_LARGE:
                    fFreezeShellRadius = g_pGame->m_pGameTweaks->fShellBigRadius;
                    break;
                case POWERUPSIZE_MEDIUM:
                    fFreezeShellRadius = g_pGame->m_pGameTweaks->fShellMediumRadius;
                    break;
                case POWERUPSIZE_SMALL:
                    fFreezeShellRadius = g_pGame->m_pGameTweaks->fShellSmallRadius;
                    break;
                }

                pPowerup = NULL;
                if (FreezeShell::m_FreezeShellSlotPool.m_FreeList == NULL)
                {
                    SlotPoolBase::BaseAddNewBlock(&FreezeShell::m_FreezeShellSlotPool, sizeof(FreezeShell));
                }

                if (FreezeShell::m_FreezeShellSlotPool.m_FreeList != NULL)
                {
                    pPowerup = (FreezeShell*)FreezeShell::m_FreezeShellSlotPool.m_FreeList;
                    FreezeShell::m_FreezeShellSlotPool.m_FreeList = FreezeShell::m_FreezeShellSlotPool.m_FreeList->next;
                }

                new (pPowerup) FreezeShell(pTarget, i, fFreezeShellRadius, eSize, bExplode);
                break;
            }
            case POWER_UP_RED_SHELL:
            {
                switch (eSize)
                {
                case POWERUPSIZE_LARGE:
                    fRedShellRadius = g_pGame->m_pGameTweaks->fShellBigRadius;
                    break;
                case POWERUPSIZE_MEDIUM:
                    fRedShellRadius = g_pGame->m_pGameTweaks->fShellMediumRadius;
                    break;
                case POWERUPSIZE_SMALL:
                    fRedShellRadius = g_pGame->m_pGameTweaks->fShellSmallRadius;
                    break;
                }

                pPowerup = NULL;
                if (RedShell::m_RedShellSlotPool.m_FreeList == NULL)
                {
                    SlotPoolBase::BaseAddNewBlock(&RedShell::m_RedShellSlotPool, sizeof(RedShell));
                }

                if (RedShell::m_RedShellSlotPool.m_FreeList != NULL)
                {
                    pPowerup = (RedShell*)RedShell::m_RedShellSlotPool.m_FreeList;
                    RedShell::m_RedShellSlotPool.m_FreeList = RedShell::m_RedShellSlotPool.m_FreeList->next;
                }

                new (pPowerup) RedShell(pTarget, i, fRedShellRadius, eSize, bExplode);
                break;
            }
            case POWER_UP_SPINY_SHELL:
            {
                switch (eSize)
                {
                case POWERUPSIZE_LARGE:
                    fSpinyShellRadius = g_pGame->m_pGameTweaks->fShellBigRadius;
                    break;
                case POWERUPSIZE_MEDIUM:
                    fSpinyShellRadius = g_pGame->m_pGameTweaks->fShellMediumRadius;
                    break;
                case POWERUPSIZE_SMALL:
                    fSpinyShellRadius = g_pGame->m_pGameTweaks->fShellSmallRadius;
                    break;
                }

                pPowerup = NULL;
                if (SpinyShell::m_SpinyShellSlotPool.m_FreeList == NULL)
                {
                    SlotPoolBase::BaseAddNewBlock(&SpinyShell::m_SpinyShellSlotPool, sizeof(SpinyShell));
                }

                if (SpinyShell::m_SpinyShellSlotPool.m_FreeList != NULL)
                {
                    pPowerup = (SpinyShell*)SpinyShell::m_SpinyShellSlotPool.m_FreeList;
                    SpinyShell::m_SpinyShellSlotPool.m_FreeList = SpinyShell::m_SpinyShellSlotPool.m_FreeList->next;
                }

                new (pPowerup) SpinyShell(pTarget, i, fSpinyShellRadius, eSize, bExplode);
                break;
            }
            default:
                break;
            }

            pPowerup->Init(pThrower, pBowser);

            if (pFirstPowerup == NULL)
            {
                pPowerup->ThrowAt(pThrower, pBowser);
                pFirstPowerup = pPowerup;
            }
            else
            {
                PowerupThrowPosition(j, eStyle, pPowerup, pFirstPowerup);

                if (pPowerup->m_eType == POWER_UP_RED_SHELL)
                {
                    for (int i = 0; i < 4; i++)
                    {
                        if (pFirstPowerup->m_pTarget == pTargetFielders[i])
                        {
                            pTargetFielders[i] = NULL;
                        }

                        if (pTargetFielders[i] != NULL)
                        {
                            pPowerup->m_pTarget = pTargetFielders[i];
                            pTargetFielders[i] = NULL;
                            break;
                        }
                    }
                }
            }

            g_pPowerups[i] = pPowerup;
            bFoundLocation = true;
        }
    }

    if (pThrower != NULL)
    {
        Event* pEvent = g_pEventManager->CreateValidEvent(0x1D, 0x24);
        PowerupUsedEventData* pData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) PowerupUsedEventData();

        pData->Type = eType;
        pData->Thrower = pThrower;
        pData->Target = pTarget;
    }

    return 1;
}
static inline PowerupBase* FindPowerUpImpl(unsigned long hashOfDrawable)
{
    const Pair* entry = powerupRegistry.registry;
    for (int i = 0; i < 25; ++i)
    {
        if (hashOfDrawable == entry->hashId)
        {
            return const_cast<PowerupBase*>(powerupRegistry.registry[i].powerup);
        }
        ++entry;
    }
    return nullptr;
}

/**
 * Offset/Address/Size: 0x4EB4 | 0x8005F7A0 | size: 0x4C
 */
PowerupBase* FindPowerUp(unsigned long hashOfDrawable)
{
    return FindPowerUpImpl(hashOfDrawable);
}

//  */
/**
 * Offset/Address/Size: 0x4C00 | 0x8005F4EC | size: 0x2B4
 */
void PowerupModelPool::Initialize(int type, unsigned long objHashName)
{
    DrawableObject* obj;
    int i;
    obj = WorldManager::s_World->FindDrawableObject(objHashName);
    i = 0;

    obj->m_uObjectFlags &= ~1;
    obj->m_uObjectFlags |= 0x80;

    for (; i < 25; i++)
    {
        mObjs[type][i] = obj->Clone();

        BasicString<char, Detail::TempStringAllocator> name = Format(BasicString<char, Detail::TempStringAllocator>("powerup_generated_{0}"), mNum);

        mObjs[type][i]->m_uHashID = nlStringLowerHash(name.c_str());
        mObjs[type][i]->m_uObjectFlags &= ~1;

        WorldManager::s_World->AddDrawableObject(mObjs[type][i]->GetHashID(), mObjs[type][i]);

        mFree[type][i] = 1;
        mNum++;
    }
}

/**
 * Offset/Address/Size: 0x4B68 | 0x8005F454 | size: 0x98
 */
void InitializePowerups()
{
    powerupModelPool.mNum = 0;
    powerupModelPool.Initialize(POWER_UP_FREEZE_SHELL, uFREEZE_SHELL_MASTER_OBJECT);
    powerupModelPool.Initialize(POWER_UP_SPINY_SHELL, uSPINY_SHELL_MASTER_OBJECT);
    powerupModelPool.Initialize(POWER_UP_GREEN_SHELL, uGREEN_SHELL_MASTER_OBJECT);
    powerupModelPool.Initialize(POWER_UP_RED_SHELL, uRED_SHELL_MASTER_OBJECT);
    powerupModelPool.Initialize(POWER_UP_BANANA, uBANANA_MASTER_OBJECT);
    powerupModelPool.Initialize(POWER_UP_BOBOMB, uBOBOMB_MASTER_OBJECT);
}

/**
 * Offset/Address/Size: 0x4AEC | 0x8005F3D8 | size: 0x7C
 */
void CompactPowerups()
{
    SlotPoolBase::BaseFreeBlocks(&GreenShell::m_GreenShellSlotPool, sizeof(GreenShell));
    SlotPoolBase::BaseFreeBlocks(&RedShell::m_RedShellSlotPool, sizeof(RedShell));
    SlotPoolBase::BaseFreeBlocks(&SpinyShell::m_SpinyShellSlotPool, sizeof(SpinyShell));
    SlotPoolBase::BaseFreeBlocks(&FreezeShell::m_FreezeShellSlotPool, sizeof(FreezeShell));
    SlotPoolBase::BaseFreeBlocks(&Banana::m_BananaSlotPool, sizeof(Banana));
    SlotPoolBase::BaseFreeBlocks(&Bobomb::m_BobombSlotPool, sizeof(Bobomb));
}

/**
 * Offset/Address/Size: 0x465C | 0x8005EF48 | size: 0x490
 */
PowerupBase::PowerupBase(cFielder* pTarget, ePowerUpType eType, float fRadius, ePowerupSize eSize, bool bExplode, int nIndex)
    : m_bShouldDestroy(false)
    , m_pDrawableObj(NULL)
    , m_pTarget(pTarget)
    , m_eType(eType)
{
    m_aOrientation = 0;
    m_scale = 1.0f;
    m_szStreakTexture = NULL;
    m_fBlurWidth = 0.0f;
    m_fBlurLength = 0.0f;
    m_uVoiceID = 0;
    m_pBlurHandler = NULL;
    m_nIndex = nIndex;
    meSize = eSize;
    mbExploder = bExplode;

    switch (eType)
    {
    case POWER_UP_GREEN_SHELL:
    {
        PhysicsShell* pShell = (PhysicsShell*)nlMalloc(sizeof(PhysicsShell), 8, false);
        pShell = new (pShell) PhysicsShell(fRadius);
        m_pPhysicsObject = pShell;
        PhysicsShell* pObj = (PhysicsShell*)m_pPhysicsObject;
        pObj->m_pPowerupObject = this;
        pObj->m_pTriggerCallbackFunc = (void (*)(PhysicsObject*, PhysicsObject*, nlVector3&, void*))CollisionCallback;
        pObj->m_pCallbackParam = this;
        m_szStreakTexture = uGREEN_SHELL_STREAK_TEXTURE;
        mtActiveTimer.SetSeconds(g_pGame->m_pGameTweaks->fGreenShellActiveTime);
        m_fBlurWidth = 2.0f * (fRadius / 3.0f);
        m_fBlurLength = (f32)(2.0 * fRadius);
        break;
    }
    case POWER_UP_RED_SHELL:
    {
        PhysicsShell* pShell = (PhysicsShell*)nlMalloc(sizeof(PhysicsShell), 8, false);
        pShell = new (pShell) PhysicsShell(fRadius);
        m_pPhysicsObject = pShell;
        PhysicsShell* pObj = (PhysicsShell*)m_pPhysicsObject;
        pObj->m_pPowerupObject = this;
        pObj->m_pTriggerCallbackFunc = (void (*)(PhysicsObject*, PhysicsObject*, nlVector3&, void*))CollisionCallback;
        pObj->m_pCallbackParam = this;
        m_szStreakTexture = uRED_SHELL_STREAK_TEXTURE;
        mtActiveTimer.SetSeconds(g_pGame->m_pGameTweaks->fRedShellActiveTime);
        m_fBlurWidth = 2.0f * (fRadius / 3.0f);
        m_fBlurLength = (f32)(2.0 * fRadius);
        break;
    }
    case POWER_UP_SPINY_SHELL:
    {
        PhysicsShell* pShell = (PhysicsShell*)nlMalloc(sizeof(PhysicsShell), 8, false);
        pShell = new (pShell) PhysicsShell(fRadius);
        m_pPhysicsObject = pShell;
        PhysicsShell* pObj = (PhysicsShell*)m_pPhysicsObject;
        pObj->m_pPowerupObject = this;
        pObj->m_pTriggerCallbackFunc = (void (*)(PhysicsObject*, PhysicsObject*, nlVector3&, void*))CollisionCallback;
        pObj->m_pCallbackParam = this;
        m_szStreakTexture = uSPINY_SHELL_STREAK_TEXTURE;
        mtActiveTimer.SetSeconds(g_pGame->m_pGameTweaks->fSpinyShellActiveTime);
        m_fBlurWidth = 2.0f * (fRadius / 3.0f);
        m_fBlurLength = (f32)(2.0 * fRadius);
        break;
    }
    case POWER_UP_FREEZE_SHELL:
    {
        PhysicsShell* pShell = (PhysicsShell*)nlMalloc(sizeof(PhysicsShell), 8, false);
        pShell = new (pShell) PhysicsShell(fRadius);
        m_pPhysicsObject = pShell;
        PhysicsShell* pObj = (PhysicsShell*)m_pPhysicsObject;
        pObj->m_pPowerupObject = this;
        pObj->m_pTriggerCallbackFunc = (void (*)(PhysicsObject*, PhysicsObject*, nlVector3&, void*))CollisionCallback;
        pObj->m_pCallbackParam = this;
        m_szStreakTexture = uFREEZE_SHELL_STREAK_TEXTURE;
        mtActiveTimer.SetSeconds(g_pGame->m_pGameTweaks->fFreezeShellActiveTime);
        m_fBlurWidth = 2.0f * (fRadius / 3.0f);
        m_fBlurLength = (f32)(2.0 * fRadius);
        break;
    }
    case POWER_UP_BANANA:
    {
        PhysicsBanana* pBanana = (PhysicsBanana*)nlMalloc(sizeof(PhysicsBanana), 8, false);
        pBanana = new (pBanana) PhysicsBanana(fRadius);
        m_pPhysicsObject = pBanana;
        PhysicsBanana* pObj = (PhysicsBanana*)m_pPhysicsObject;
        pObj->m_pPowerupObject = this;
        pObj->m_pTriggerCallbackFunc = (void (*)(PhysicsObject*, PhysicsObject*, nlVector3&, void*))CollisionCallback;
        pObj->m_pCallbackParam = this;
        m_szStreakTexture = uBANANA_STREAK_TEXTURE;
        mtActiveTimer.SetSeconds(g_pGame->m_pGameTweaks->fBananaActiveTime);
        m_fBlurWidth = 0.0f;
        m_fBlurLength = 0.0f;
        break;
    }
    case POWER_UP_BOBOMB:
    {
        PhysicsBanana* pBobomb = (PhysicsBanana*)nlMalloc(sizeof(PhysicsBanana), 8, false);
        pBobomb = new (pBobomb) PhysicsBanana(fRadius);
        m_pPhysicsObject = pBobomb;
        PhysicsBanana* pObj = (PhysicsBanana*)m_pPhysicsObject;
        pObj->m_pPowerupObject = this;
        pObj->m_pTriggerCallbackFunc = (void (*)(PhysicsObject*, PhysicsObject*, nlVector3&, void*))CollisionCallback;
        pObj->m_pCallbackParam = this;
        m_szStreakTexture = uBOBOMB_STREAK_TEXTURE;
        mtActiveTimer.SetSeconds(g_pGame->m_pGameTweaks->fBobombActiveTime);
        m_fBlurWidth = 0.5f * fRadius;
        m_fBlurLength = (f32)(2.0 * fRadius);
        break;
    }
    }

    m_aOrientation = nlRandom(65000, &nlDefaultSeed);

    if (eType != POWER_UP_BANANA && eType != POWER_UP_RED_SHELL)
    {
        mtNoHitTimer.SetSeconds(0.4f);
    }
    else
    {
        mtNoHitTimer.SetSeconds(1.0f);
    }

    m_v3Position.x = 0.0f;
    m_v3Position.y = 0.0f;
    m_v3Position.z = fRadius;
    m_v3PrevPosition = m_v3Position;
    m_v3Velocity = v3Zero;

    m_pPhysicsObject->SetPosition(m_v3Position, PhysicsObject::WORLD_COORDINATES);
    m_pPhysicsObject->SetLinearVelocity(m_v3Velocity);
    m_pPhysicsObject->EnableCollisions();
}

static inline void ReleasePowerupModel(ePowerUpType type, DrawableObject* pDrawable)
{
    int i;

    for (i = 0; i < 25; i++)
    {
        if (pDrawable == powerupModelPool.mObjs[type][i])
        {
            pDrawable->m_uObjectFlags &= ~1;
            powerupModelPool.mFree[type][i] = true;
            break;
        }
    }
}

/**
 * Offset/Address/Size: 0x4540 | 0x8005EE2C | size: 0x11C
 */
PowerupBase::~PowerupBase()
{
    uintptr_t voiceID = m_uVoiceID;

    if (voiceID != 0)
    {
        if (voiceID != (uintptr_t)Audio::GetSndIDError())
        {
            Audio::gPowerupSFX.StopEmitter((SFXEmitter*)m_uVoiceID, 0);
            m_uVoiceID = 0;
        }
    }

    ReleasePowerupModel(m_eType, m_pDrawableObj);

    delete m_pPhysicsObject;
}

/**
 * Offset/Address/Size: 0x451C | 0x8005EE08 | size: 0x24
 */
float PowerupBase::GetRadius() const
{
    return ((PhysicsSphere*)m_pPhysicsObject)->GetRadius();
}

/**
 * Offset/Address/Size: 0x4404 | 0x8005ECF0 | size: 0x118
 */
void PowerupBase::Update(float dt)
{
    nlPolar polar;

    m_v3PrevPosition = m_v3Position;
    m_pPhysicsObject->GetPosition(&m_v3Position);
    m_pPhysicsObject->GetLinearVelocity(&m_v3Velocity);

    if (m_v3Position.z < ((PhysicsSphere*)m_pPhysicsObject)->GetRadius())
    {
        m_v3Position.z = ((PhysicsSphere*)m_pPhysicsObject)->GetRadius();
        m_pPhysicsObject->SetPosition(m_v3Position, PhysicsObject::WORLD_COORDINATES);
    }

    if (m_pBlurHandler != nullptr)
    {
        m_pBlurHandler->AddViewOrientedPoint(m_v3Position, m_v3Velocity);
    }

    DecrementTimers(dt);

    UpdateTransform();

    if (m_pBlurHandler != nullptr)
    {
        nlCartesianToPolar(polar, m_v3Velocity.x, m_v3Velocity.y);
        if (polar.r < 0.5f)
        {
            m_pBlurHandler->Die(0.5f);
            m_pBlurHandler = nullptr;
        }
    }
}

/**
 * Offset/Address/Size: 0x3DFC | 0x8005E6E8 | size: 0x608
 */
int PowerupBase::AwardPowerup(cTeam* pTeam)
{
    unsigned char bEmptySpot;
    int i;
    int nDifference;
    int nChanceForChainChomp;
    cTeam* pOtherTeam;

    if (g_pGame->mIsPure)
    {
        return -1;
    }

    if (!nlSingleton<GameInfoManager>::Instance()->GetGameplayOptions().PowerUps)
    {
        return -1;
    }

    bEmptySpot = false;
    for (i = 0; i < 2; i++)
    {
        if (pTeam->GetPowerUpByIndex(i).eType == POWER_UP_NONE)
        {
            bEmptySpot = true;
        }
    }

    if (!bEmptySpot)
    {
        return -1;
    }

    nDifference = pTeam->m_nScore - pTeam->GetOtherTeam()->m_nScore;

    if ((u32)(nDifference < 0 ? -nDifference : nDifference) <= (u32)g_pGame->m_pGameTweaks->nScoreDifferenceMinimum)
    {
        nDifference = 0;
    }
    else
    {
        if (nDifference < -g_pGame->m_pGameTweaks->nScoreDifferenceMaximum)
        {
            nDifference = -g_pGame->m_pGameTweaks->nScoreDifferenceMaximum;
        }
        else if (nDifference > g_pGame->m_pGameTweaks->nScoreDifferenceMaximum)
        {
            nDifference = g_pGame->m_pGameTweaks->nScoreDifferenceMaximum;
        }
    }

    if (nDifference < 0)
    {
        nDifference *= nDifference;
        nDifference = -nDifference;
    }
    else
    {
        nDifference *= nDifference;
    }

    nChanceForChainChomp = g_pGame->m_pGameTweaks->nChanceForChainChomp - nDifference;

    pOtherTeam = pTeam->GetOtherTeam();
    for (int i = 0; i < 2; i++)
    {
        if (pOtherTeam->GetPowerUpByIndex(i).eType == POWER_UP_CHAIN_CHOMP)
        {
            nChanceForChainChomp = 0;
        }
        if (pTeam->GetPowerUpByIndex(i).eType == POWER_UP_CHAIN_CHOMP)
        {
            nChanceForChainChomp = 0;
        }
    }

    if (!BasicStadium::GetCurrentStadium()->mpNPCManager->mpChainChomp->IsHidden() || BasicStadium::GetCurrentStadium()->mpNPCManager->mpBowser->meBowserState != BOWSER_STATE_HIDDEN)
    {
        nChanceForChainChomp = 0;
    }

    cFielder* pCaptain = pTeam->GetCaptain();
    cFielder* pSideKick = pTeam->GetFielder(1);

    int nChanceForStar = ((nChanceForChainChomp) > 0 ? (nChanceForChainChomp) : 0);
    nChanceForStar = nChanceForStar + g_pGame->m_pGameTweaks->nChanceForStar - nDifference;

    if (nDifference >= -1)
    {
        nChanceForChainChomp = 0;
        nChanceForStar = 0;
    }

    ePowerUpType powerUpType;
    int nChanceForSpinyShell, nChanceForRedShell, nChanceForBanana, nChanceForBoBomb, nChanceForMushroom, nChanceForGreenShell, nChanceForFreezeShell;

    pOtherTeam = (cTeam*)pCaptain->m_pTweaks;
    nChanceForSpinyShell = g_pGame->m_pGameTweaks->nChanceForSpinyShell + ((FielderTweaks*)pOtherTeam)->nChanceForSpinyShell + ((FielderTweaks*)pSideKick->m_pTweaks)->nChanceForSpinyShell + ((nChanceForStar) > 0 ? (nChanceForStar) : 0) - nDifference;

    nChanceForRedShell = ((nChanceForSpinyShell) > 0 ? (nChanceForSpinyShell) : 0);
    nChanceForRedShell = nChanceForRedShell + (((FielderTweaks*)pSideKick->m_pTweaks)->nChanceForRedShell + (g_pGame->m_pGameTweaks->nChanceForRedShell + ((FielderTweaks*)pCaptain->m_pTweaks)->nChanceForRedShell)) - nDifference;

    nChanceForBanana = ((nChanceForRedShell) > 0 ? (nChanceForRedShell) : 0);
    nChanceForBanana = nChanceForBanana + (((FielderTweaks*)pSideKick->m_pTweaks)->nChanceForBanana + (g_pGame->m_pGameTweaks->nChanceForBanana + ((FielderTweaks*)pCaptain->m_pTweaks)->nChanceForBanana)) + nDifference;

    nChanceForBoBomb = ((nChanceForBanana) > 0 ? (nChanceForBanana) : 0);
    nChanceForBoBomb = nChanceForBoBomb
                     + (((FielderTweaks*)pSideKick->m_pTweaks)->nChanceForBoBomb
                         + (g_pGame->m_pGameTweaks->nChanceForBoBomb
                             + ((FielderTweaks*)pCaptain->m_pTweaks)->nChanceForBoBomb));

    nChanceForMushroom = ((nChanceForBoBomb) > 0 ? (nChanceForBoBomb) : 0);
    nChanceForMushroom = nChanceForMushroom + (((FielderTweaks*)pSideKick->m_pTweaks)->nChanceForMushroom + (g_pGame->m_pGameTweaks->nChanceForMushroom + ((FielderTweaks*)pCaptain->m_pTweaks)->nChanceForMushroom)) + nDifference;

    nChanceForGreenShell = g_pGame->m_pGameTweaks->nChanceForGreenShell + ((FielderTweaks*)pCaptain->m_pTweaks)->nChanceForGreenShell + ((FielderTweaks*)pSideKick->m_pTweaks)->nChanceForGreenShell + ((nChanceForMushroom) > 0 ? (nChanceForMushroom) : 0) + nDifference;

    nChanceForFreezeShell = ((nChanceForGreenShell) > 0 ? (nChanceForGreenShell) : 0);
    nChanceForFreezeShell = nChanceForFreezeShell + (((FielderTweaks*)pSideKick->m_pTweaks)->nChanceForFreezeShell + (g_pGame->m_pGameTweaks->nChanceForFreezeShell + ((FielderTweaks*)pCaptain->m_pTweaks)->nChanceForFreezeShell)) + nDifference;

    int nChance = nlRandom(nChanceForFreezeShell, &nlDefaultSeed);

    switch (nlSingleton<GameInfoManager>::Instance()->GetCustomPowerups())
    {
    case CP_FREEZING:
        nChanceForChainChomp = nChanceForStar = nChanceForSpinyShell = nChanceForRedShell = nChanceForBanana = nChanceForBoBomb = nChanceForMushroom = nChanceForGreenShell = 0;
        powerUpType = POWER_UP_FREEZE_SHELL;
        break;
    case CP_GIANT:
        nChanceForStar = nChanceForMushroom = nChanceForBanana = 0;
        powerUpType = POWER_UP_GREEN_SHELL;
        break;
    case CP_SHELLS:
        nChanceForChainChomp = nChanceForStar = nChanceForBanana = nChanceForBoBomb = nChanceForMushroom = 0;
        powerUpType = POWER_UP_GREEN_SHELL;
        break;
    case CP_ENCHANCEMENT:
        nChanceForChainChomp = nChanceForSpinyShell = nChanceForRedShell = nChanceForBanana = nChanceForBoBomb = nChanceForGreenShell = nChanceForFreezeShell = 0;
        powerUpType = POWER_UP_MUSHROOM;
        break;
    case CP_EXPLOSIVE:
        nChanceForChainChomp = nChanceForStar = nChanceForSpinyShell = nChanceForRedShell = nChanceForBanana = nChanceForMushroom = nChanceForGreenShell = nChanceForFreezeShell = 0;
        powerUpType = POWER_UP_BOBOMB;
        break;
    default:
        powerUpType = POWER_UP_MUSHROOM;
        break;
    }

    if (nChance < nChanceForChainChomp)
    {
        powerUpType = POWER_UP_CHAIN_CHOMP;
    }
    else if (nChance < nChanceForStar)
    {
        powerUpType = POWER_UP_STAR;
    }
    else if (nChance < nChanceForSpinyShell)
    {
        powerUpType = POWER_UP_SPINY_SHELL;
    }
    else if (nChance < nChanceForRedShell)
    {
        powerUpType = POWER_UP_RED_SHELL;
    }
    else if (nChance < nChanceForBanana)
    {
        powerUpType = POWER_UP_BANANA;
    }
    else if (nChance < nChanceForBoBomb)
    {
        powerUpType = POWER_UP_BOBOMB;
    }
    else if (nChance < nChanceForMushroom)
    {
        powerUpType = POWER_UP_MUSHROOM;
    }
    else if (nChance < nChanceForGreenShell)
    {
        powerUpType = POWER_UP_GREEN_SHELL;
    }
    else if (nChance <= nChanceForFreezeShell)
    {
        powerUpType = POWER_UP_FREEZE_SHELL;
    }

    float fMultiplesBonus;
    int nNumOfPowerups;
    float fRandom;
    float fFiveChance;

    nNumOfPowerups = 1;
    fMultiplesBonus = ((FielderTweaks*)pCaptain->m_pTweaks)->fChanceForMultiples / 2.0f;
    fRandom = nlRandomf(1.0f, &nlDefaultSeed);

    switch (powerUpType)
    {
    case POWER_UP_GREEN_SHELL:
    case POWER_UP_FREEZE_SHELL:
    {
        float fFiveChance = fMultiplesBonus + g_pGame->m_pGameTweaks->fShellFiveChance;
        if (fMultiplesBonus + (fFiveChance + g_pGame->m_pGameTweaks->fShellThreeChance) > fRandom)
        {
            nNumOfPowerups = 3;
        }
        else if (fFiveChance > fRandom)
        {
            nNumOfPowerups = 5;
        }
        break;
    }
    case POWER_UP_RED_SHELL:
    case POWER_UP_SPINY_SHELL:
        pOtherTeam = (cTeam*)g_pGame->m_pGameTweaks;
        if (((FielderTweaks*)pCaptain->m_pTweaks)->fChanceForMultiples + (((GameTweaks*)pOtherTeam)->fShellFiveChance + ((GameTweaks*)pOtherTeam)->fShellThreeChance) > fRandom)
        {
            nNumOfPowerups = 3;
        }
        break;
    case POWER_UP_BOBOMB:
        fFiveChance = fMultiplesBonus + g_pGame->m_pGameTweaks->fBobombFiveChance;
        if (fMultiplesBonus + (fFiveChance + g_pGame->m_pGameTweaks->fBobombThreeChance) > fRandom)
        {
            nNumOfPowerups = 3;
        }
        else if (fFiveChance > fRandom)
        {
            nNumOfPowerups = 5;
        }
        break;
    case POWER_UP_BANANA:
        fFiveChance = fMultiplesBonus + g_pGame->m_pGameTweaks->fBananaFiveChance;
        if (fMultiplesBonus + (fFiveChance + g_pGame->m_pGameTweaks->fBananaThreeChance) > fRandom)
        {
            nNumOfPowerups = 3;
        }
        else
        {
            nNumOfPowerups = 5;
        }
        break;
    default:
        break;
    }

    if (nlSingleton<GameInfoManager>::Instance()->GetCustomPowerups() == CP_GIANT)
    {
        nNumOfPowerups = 1;
    }

    pTeam->SetCurrentPowerUp(powerUpType, nNumOfPowerups);
    return (int)powerUpType;
}
/**
 * Offset/Address/Size: 0x38B4 | 0x8005E1A0 | size: 0x548
 */
void PowerupBase::CollisionCallback(PhysicsObject* pObjA, PhysicsObject* pObjB, const nlVector3& v3Pos, void* pParam)
{
    PowerupBase* pObj = (PowerupBase*)pParam;
    cCharacter* pCharacter = NULL;
    cPlayer* pPlayerTarget = NULL;

    int type = pObjB->GetObjectType();
    switch (type)
    {
    case 0x0D:
    case 0x0E:
        pCharacter = ((PhysicsCharacter*)pObjB->m_parentObject)->m_pAICharacter;
        break;
    case 0x0F:
    {
        cBall* pBall = ((PhysicsAIBall*)pObjB)->m_pAIBall;
        cFielder* pFielder = (cFielder*)pBall->m_pOwner;
        if (pFielder != NULL)
        {
            pCharacter = (cCharacter*)pFielder;
            if (((cCharacter*)pFielder)->m_eClassType != FIELDER)
                break;
            if (pBall->GetOwnerFielder()->IsBallAwayFromCarrier())
            {
                Event* pEvent = g_pEventManager->CreateValidEvent(0x2D, 0x24);
                CollisionBallShellData* pData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) CollisionBallShellData();
                pData->v3CollisionVelocity = pObj->m_v3Velocity;
            }
        }
        else
        {
            bool bPerfectShot = (pBall->m_tShotTimer.m_uPackedTime != 0) && pBall->m_unk_0xA4;
            if (bPerfectShot)
                EmitBallWallHit("perfect_shot_catch");
            else if (pObj->m_pTarget != NULL)
                EmitBallWallHit("goalie_catch");
            pObj->m_pTarget = NULL;
            pBall->ClearShotInProgress();
            pBall->ClearPassTarget();
            pBall->m_unk_0xA6 = false;
            pBall->mpDamageTarget = NULL;
        }
        break;
    }
    case 0x13:
        if (pObj->meSize > ((PhysicsShell*)pObjB)->m_pPowerupObject->meSize)
        {
            ((PhysicsShell*)pObjB)->m_pPowerupObject->m_bShouldDestroy = true;
        }
        else if (pObj->meSize == ((PhysicsShell*)pObjB)->m_pPowerupObject->meSize)
        {
            if (pObj->m_eType == POWER_UP_BANANA)
            {
                pObj->m_bShouldDestroy = true;
            }
            else if (pObj->m_eType == POWER_UP_SPINY_SHELL && ((PhysicsShell*)pObjB)->m_pPowerupObject->m_eType == POWER_UP_SPINY_SHELL)
            {
                ((PhysicsShell*)pObjB)->m_pPowerupObject->m_bShouldDestroy = true;
                pObj->m_bShouldDestroy = true;
            }
            else if (pObj->m_eType == POWER_UP_SPINY_SHELL)
            {
                ((PhysicsShell*)pObjB)->m_pPowerupObject->m_bShouldDestroy = true;
            }
            else if (((PhysicsShell*)pObjB)->m_pPowerupObject->m_eType == POWER_UP_SPINY_SHELL)
            {
                pObj->m_bShouldDestroy = true;
            }
            else
            {
                ((PhysicsShell*)pObjB)->m_pPowerupObject->m_bShouldDestroy = true;
                pObj->m_bShouldDestroy = true;
            }
        }
        else
        {
            pObj->m_bShouldDestroy = true;
        }
        break;
    case 0x14:
    {
        PowerupBase* pOther = ((PhysicsBanana*)pObjB)->m_pPowerupObject;
        if (pObj->meSize > pOther->meSize)
        {
            pOther->m_bShouldDestroy = true;
        }
        else if (pObj->meSize == pOther->meSize)
        {
            pOther->m_bShouldDestroy = true;
            if (pObj->m_eType != POWER_UP_SPINY_SHELL)
                pObj->m_bShouldDestroy = true;
        }
        else
        {
            pObj->m_bShouldDestroy = true;
        }
        break;
    }
    default:
        if (pObj->m_eType == POWER_UP_BOBOMB && !((Bobomb*)pObj)->mbIsMine && ((PhysicsShell*)pObjA)->mbIsInNet)
            pObj->m_bShouldDestroy = true;
        break;
    }

    if (pCharacter != NULL)
    {
        if (pCharacter->m_eClassType == GOALIE)
        {
            pObj->m_bShouldDestroy = true;
        }
        else
        {
            pPlayerTarget = (cPlayer*)pCharacter;
            if (((cFielder*)pCharacter)->mbWasHitByPowerupThisFrame)
                return;
            ((cFielder*)pCharacter)->mbWasHitByPowerupThisFrame = true;

            {
                ePowerUpType eType = pObj->m_eType;
                if (eType == POWER_UP_BANANA)
                {
                    Event* pEvent = g_pEventManager->CreateValidEvent(0x2B, 0x30);
                    CollisionPlayerBananaData* pData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) CollisionPlayerBananaData();
                    pData->pPlayer = (cFielder*)pCharacter;
                    pData->pThrower = pObj->m_pThrower;
                    pData->nThrowerPadID = pObj->m_nThrowerPadID;
                    pData->v3CollisionLocation = v3Pos;
                    pObj->m_bShouldDestroy = true;
                }
                else if (eType != POWER_UP_BOBOMB)
                {
                    if (eType != POWER_UP_FREEZE_SHELL)
                    {
                        Event* pEvent = g_pEventManager->CreateValidEvent(0x29, 0x40);
                        CollisionPlayerShellData* pData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) CollisionPlayerShellData();
                        pData->pPlayer = (cFielder*)pCharacter;
                        pData->eSize = (int)pObj->meSize;
                        pData->pThrower = pObj->m_pThrower;
                        pData->nThrowerPadID = (u8)pObj->m_nThrowerPadID;
                        if (pObj->mbExploder)
                        {
                            pData->bIsExploder = true;
                        }
                        else
                        {
                            pData->bIsExploder = false;
                        }
                        pData->v3CollisionLocation = v3Pos;
                        pData->v3CollisionVelocity = pObj->m_v3Velocity;
                    }
                    else
                    {
                        Event* pEvent = g_pEventManager->CreateValidEvent(0x2A, 0x28);
                        CollisionPlayerFreezeData* pData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) CollisionPlayerFreezeData();
                        pData->pPlayer = (cFielder*)pCharacter;
                        pData->eSize = (int)pObj->meSize;
                        pData->pThrower = pObj->m_pThrower;
                        pData->nThrowerPadID = pObj->m_nThrowerPadID;
                    }
                    if (pObj->meSize != POWERUPSIZE_LARGE)
                    {
                        pObj->m_bShouldDestroy = true;
                    }
                    else
                    {
                        nlVector3 v3NewVelocity = pObj->m_v3Velocity;
                        nlPolar aSpeedOut;
                        nlCartesianToPolar(aSpeedOut, v3NewVelocity.x, v3NewVelocity.y);
                        v3NewVelocity.z = 0.5f * aSpeedOut.r;
                        pObj->m_v3Velocity = v3NewVelocity;
                        pObj->m_pPhysicsObject->SetLinearVelocity(v3NewVelocity);
                        pObj->m_pTarget = NULL;
                    }
                }
                else
                {
                    pObj->m_bShouldDestroy = true;
                }
            }

            if (pObj->m_eType == POWER_UP_SPINY_SHELL)
                pObj->m_bShouldDestroy = false;
            if (((cFielder*)pCharacter)->IsInvincible())
                pObj->m_bShouldDestroy = true;
        }
    }

    if (pPlayerTarget != NULL)
    {
        Event* pEvent = g_pEventManager->CreateValidEvent(0x1E, 0x24);
        PowerupHitPlayerEventData* pData = new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) PowerupHitPlayerEventData();
        pData->Type = pObj->m_eType;
        pData->Thrower = (cPlayer*)pObj->m_pThrower;
        pData->Target = pPlayerTarget;
    }
}

/**
 * Offset/Address/Size: 0x3374 | 0x8005DC60 | size: 0x540
 */
void PowerupBase::ThrowAt(cFielder* pThrower, Bowser* pBowser)
{
    ePowerUpType type;
    unsigned long soundID;
    PhysicsObject* pPhysObj;

    {
        uintptr_t voiceID = m_uVoiceID;
        if (voiceID != 0)
        {
            if (voiceID != (uintptr_t)Audio::GetSndIDError())
            {
                Audio::gPowerupSFX.StopEmitter((SFXEmitter*)m_uVoiceID, 0);
                m_uVoiceID = 0;
            }
        }
    }

    type = m_eType;
    pPhysObj = m_pPhysicsObject;

    if (type >= NUM_POWER_UPS)
    {
        Audio::GetSndIDError();
    }
    else if (!Audio::IsInited())
    {
        Audio::GetSndIDError();
    }
    else
    {
        Audio::SoundAttributes attributes;
        attributes.Init();
        soundID = powerupSounds[type].sndActivate;
        if (soundID != 0xFFFFFFFF)
        {
            attributes.SetSoundType(soundID, true);
            if (type == POWER_UP_BOBOMB)
            {
                attributes.UsePhysObj(pPhysObj);
                attributes.mf_ReturnEmitterOnPlay = true;
            }
            else
            {
                attributes.UseStationaryPosVector(pPhysObj->GetPosition());
            }
            Audio::gPowerupSFX.GetSFXVol(soundID);
            Audio::gPowerupSFX.Play(attributes);
        }
    }

    if (m_pTarget->m_pTeam == g_pTeams[1])
    {
        Audio::gCrowdSFX.PlayRandomReaction(Audio::cWorldSFX::CROWD_REACTION_YEAH_SMALL, 100.0f, -1.0f, 0, 0.0f);
    }
    else
    {
        Audio::gCrowdSFX.PlayRandomReaction(Audio::cWorldSFX::CROWD_REACTION_OH_SMALL, 100.0f, -1.0f, 0, 0.0f);
    }

    ePowerUpType type2 = m_eType;
    if (type2 == POWER_UP_CHAIN_CHOMP && !g_pGame->mbCaptainShotToScoreOn)
    {
        PhysicsObject* pPhysObj2 = m_pPhysicsObject;
        uintptr_t errorCode; // PORT: may hold an SFXEmitter*
        if (type2 >= NUM_POWER_UPS)
        {
            errorCode = Audio::GetSndIDError();
        }
        else if (!Audio::IsInited())
        {
            errorCode = Audio::GetSndIDError();
        }
        else
        {
            Audio::SoundAttributes attributes2;
            attributes2.Init();
            unsigned long soundID2 = powerupSounds[type2].sndInEffect;
            if (soundID2 == 0xFFFFFFFF)
            {
                errorCode = (uintptr_t)-1;
            }
            else
            {
                attributes2.SetSoundType(soundID2, true);
                if (type2 == POWER_UP_BOBOMB)
                {
                    attributes2.UsePhysObj(pPhysObj2);
                    attributes2.mf_ReturnEmitterOnPlay = true;
                }
                else
                {
                    attributes2.UsePhysObj(pPhysObj2);
                    attributes2.mf_ReturnEmitterOnPlay = true;
                }
                Audio::gPowerupSFX.GetSFXVol(soundID2);
                errorCode = Audio::gPowerupSFX.Play(attributes2);
            }
        }
        m_uVoiceID = errorCode;
    }

    nlVector3 v3TargetPos = m_pTarget->m_v3Position;
    nlVector3 v3TargetVel = m_pTarget->m_v3Velocity;

    float fSpeed = 0.0f;
    switch (m_eType)
    {
    case POWER_UP_FREEZE_SHELL:
        fSpeed += g_pGame->m_pGameTweaks->fFreezeShellSpeed;
        break;
    case POWER_UP_GREEN_SHELL:
        fSpeed += g_pGame->m_pGameTweaks->fGreenShellSpeed;
        break;
    case POWER_UP_RED_SHELL:
        fSpeed += g_pGame->m_pGameTweaks->fRedShellSpeed;
        break;
    case POWER_UP_SPINY_SHELL:
        fSpeed += g_pGame->m_pGameTweaks->fSpinyShellSpeed;
        break;
    case POWER_UP_BANANA:
        if (pThrower != NULL)
        {
            s16 fFacingDelta = pThrower->GetFacingDeltaToPosition(m_pTarget->m_v3Position);
            float fMinSpeed = g_pGame->m_pGameTweaks->fBananaSpeed;
            float fMaxSpeed = fMinSpeed + pThrower->m_fActualSpeed;
            u16 absDelta = (fFacingDelta < 0) ? -fFacingDelta : fFacingDelta;
            fSpeed = InterpolateRangeClamped(fMinSpeed, fMaxSpeed, 32000.0f, 12500.0f, (float)absDelta);
        }
        else
        {
            fSpeed += g_pGame->m_pGameTweaks->fBananaSpeed;
        }
        break;
    default:
        break;
    }

    if (m_eType != POWER_UP_BANANA)
    {
        switch (meSize)
        {
        case POWERUPSIZE_SMALL:
        case POWERUPSIZE_MEDIUM:
            break;
        case POWERUPSIZE_LARGE:
            fSpeed -= 1.0f;
            break;
        }
    }

    nlVector3 v3Direction;
    nlVec3Set(v3Direction,
        v3TargetPos.x - m_v3Position.x,
        v3TargetPos.y - m_v3Position.y,
        v3TargetPos.z - m_v3Position.z);
    float fInvDistance = nlRecipSqrt(v3Direction.GetLengthSq3D(), true);
    nlVec3Scale(v3Direction, fInvDistance);

    int nNumSolutions;
    float pSolutions[2];
    CalcInterceptXY(m_v3Position, fSpeed, 0.0f, v3TargetPos, v3TargetVel, nNumSolutions, pSolutions);

    if (nNumSolutions != 0)
    {
        float t;
        if (nNumSolutions == 2)
        {
            t = (pSolutions[0] < pSolutions[1]) ? pSolutions[0] : pSolutions[1];
        }
        else
        {
            t = pSolutions[0];
        }

        nlVector3 v3ShellVelocity;
        float predictedX = v3TargetVel.x * t + v3TargetPos.x;
        float predictedY = v3TargetVel.y * t + v3TargetPos.y;
        v3ShellVelocity.x = (predictedX - m_v3Position.x) / t;
        v3ShellVelocity.y = (predictedY - m_v3Position.y) / t;
        v3ShellVelocity.z = 0.0f;
        m_v3Velocity = v3ShellVelocity;
        m_pPhysicsObject->SetLinearVelocity(v3ShellVelocity);
    }
    else
    {
        nlVector3 v3Velocity;
        nlVec3Scale(v3Velocity, v3Direction, fSpeed);
        v3Velocity.z = 0.0f;
        m_v3Velocity = v3Velocity;
        m_pPhysicsObject->SetLinearVelocity(v3Velocity);
    }
}

static inline void ApplyPowerupVolume(Audio::SoundAttributes& attributes, unsigned long soundID, float volume)
{
    float defaultVolume = Audio::gPowerupSFX.GetSFXVol(soundID);
    if (volume != 100.0f)
    {
        attributes.mf_Volume = volume * defaultVolume;
    }
}

void PowerupBase::DecrementTimers(float fDeltaT)
{
    mtActiveTimer.Countdown(fDeltaT, 0.0f);
    mtNoHitTimer.Countdown(fDeltaT, 0.0f);
}

static inline void UnregisterPowerup(unsigned long hashID)
{
    for (int i = 0; i < 25; i++)
    {
        if (hashID == powerupRegistry.registry[i].hashId)
        {
            powerupRegistry.registry[i].hashId = 0;
            return;
        }
    }

    nlBreak();
}

/**
 * Offset/Address/Size: 0x28DC | 0x8005D1C8 | size: 0xA98
 */
void PowerupBase::Destroy(bool bSilent)
{
    void FireCameraRumbleFilter(float, float);
    struct CollisionBobombDataLayout
    {
        void* vtable;
        nlVector3 v3ExplosionLocation;
        float fExplosionRadius;
        cFielder* pThrower;
        int nThrowerPadID;
        bool bIsFreezeBomb;
    };

    EffectsGroup* pExplosionGroup;
    EffectsGroup* pGroundGroup;

    if (!bSilent && (m_eType == POWER_UP_BOBOMB || mbExploder))
    {
        if (!g_pGame->mbCaptainShotToScoreOn)
        {
            PowerupSound bobombExplosions[2] = { PWRUP_SOUND_HIT, PWRUP_SOUND_EXPLODE };
            switch (meSize)
            {
            case POWERUPSIZE_LARGE:
            {
                pExplosionGroup = fxGetGroup("bobomb_explode_big");
                pGroundGroup = fxGetGroup("bobomb_explode_ground_big");
                if (bSilent)
                    break;

                PowerupSound pwrSnd;
                unsigned long sndType;
                PhysicsObject* pPhysObj = m_pPhysicsObject;
                float fVol = g_pGame->m_pGameTweaks->fPowerupLargeSizeVolCoeff;
                pwrSnd = bobombExplosions[nlRandom(2, &nlDefaultSeed)];

                if (!Audio::IsInited())
                {
                    Audio::GetSndIDError();
                }
                else
                {
                    Audio::SoundAttributes attrs;
                    attrs.Init();

                    switch (pwrSnd)
                    {
                    case PWRUP_SOUND_ACQUIRE:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndAcquire;
                        break;
                    case PWRUP_SOUND_ACTIVATE:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndActivate;
                        break;
                    case PWRUP_SOUND_IN_EFFECT:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndInEffect;
                        break;
                    case PWRUP_SOUND_HIT:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndHit;
                        break;
                    case PWRUP_SOUND_BOUNCE_WALL:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndBounceWall;
                        break;
                    case PWRUP_SOUND_BOUNCE_GROUND:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndBounceGround;
                        break;
                    case PWRUP_SOUND_EXPLODE:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndExplode;
                        break;
                    case PWRUP_SOUND_END:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndEnd;
                        break;
                    }

                    if (sndType != 0xFFFFFFFF)
                    {
                        if (pwrSnd == PWRUP_SOUND_ACQUIRE)
                        {
                            attrs.SetSoundType(sndType, false);
                        }
                        else
                        {
                            attrs.SetSoundType(sndType, true);
                            if ((pwrSnd == PWRUP_SOUND_IN_EFFECT) || (pwrSnd == PWRUP_SOUND_ACTIVATE))
                            {
                                attrs.UsePhysObj(pPhysObj);
                                attrs.mf_ReturnEmitterOnPlay = true;
                            }
                            else
                            {
                                attrs.UseStationaryPosVector(pPhysObj->GetPosition());
                            }
                        }

                        ApplyPowerupVolume(attrs, sndType, fVol);
                        Audio::gPowerupSFX.Play(attrs);
                    }
                }
                break;
            }
            case POWERUPSIZE_MEDIUM:
            {
                pExplosionGroup = fxGetGroup("bobomb_explode_med");
                pGroundGroup = fxGetGroup("bobomb_explode_ground_med");
                if (bSilent)
                    break;

                PhysicsObject* pPhysObj = m_pPhysicsObject;
                unsigned long sndType;
                float fVol = g_pGame->m_pGameTweaks->fPowerupMedSizeVolCoeff;
                PowerupSound pwrSnd = bobombExplosions[nlRandom(2, &nlDefaultSeed)];

                if (!Audio::IsInited())
                {
                    Audio::GetSndIDError();
                }
                else
                {
                    Audio::SoundAttributes attrs;
                    attrs.Init();

                    switch (pwrSnd)
                    {
                    case PWRUP_SOUND_ACQUIRE:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndAcquire;
                        break;
                    case PWRUP_SOUND_ACTIVATE:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndActivate;
                        break;
                    case PWRUP_SOUND_IN_EFFECT:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndInEffect;
                        break;
                    case PWRUP_SOUND_HIT:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndHit;
                        break;
                    case PWRUP_SOUND_BOUNCE_WALL:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndBounceWall;
                        break;
                    case PWRUP_SOUND_BOUNCE_GROUND:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndBounceGround;
                        break;
                    case PWRUP_SOUND_EXPLODE:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndExplode;
                        break;
                    case PWRUP_SOUND_END:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndEnd;
                        break;
                    }

                    if (sndType != 0xFFFFFFFF)
                    {
                        if (pwrSnd == PWRUP_SOUND_ACQUIRE)
                        {
                            attrs.SetSoundType(sndType, false);
                        }
                        else
                        {
                            attrs.SetSoundType(sndType, true);
                            if ((pwrSnd == PWRUP_SOUND_IN_EFFECT) || (pwrSnd == PWRUP_SOUND_ACTIVATE))
                            {
                                attrs.UsePhysObj(pPhysObj);
                                attrs.mf_ReturnEmitterOnPlay = true;
                            }
                            else
                            {
                                attrs.UseStationaryPosVector(pPhysObj->GetPosition());
                            }
                        }

                        ApplyPowerupVolume(attrs, sndType, fVol);
                        Audio::gPowerupSFX.Play(attrs);
                    }
                }
                break;
            }
            case POWERUPSIZE_SMALL:
            {
                pExplosionGroup = fxGetGroup("bobomb_explode_small");
                pGroundGroup = fxGetGroup("bobomb_explode_ground_small");
                if (bSilent)
                    break;

                PhysicsObject* pPhysObj = m_pPhysicsObject;
                unsigned long sndType;
                float fVol = g_pGame->m_pGameTweaks->fPowerupSmallSizeVolCoeff;
                PowerupSound pwrSnd = bobombExplosions[nlRandom(2, &nlDefaultSeed)];

                if (!Audio::IsInited())
                {
                    Audio::GetSndIDError();
                }
                else
                {
                    Audio::SoundAttributes attrs;
                    attrs.Init();

                    switch (pwrSnd)
                    {
                    case PWRUP_SOUND_ACQUIRE:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndAcquire;
                        break;
                    case PWRUP_SOUND_ACTIVATE:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndActivate;
                        break;
                    case PWRUP_SOUND_IN_EFFECT:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndInEffect;
                        break;
                    case PWRUP_SOUND_HIT:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndHit;
                        break;
                    case PWRUP_SOUND_BOUNCE_WALL:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndBounceWall;
                        break;
                    case PWRUP_SOUND_BOUNCE_GROUND:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndBounceGround;
                        break;
                    case PWRUP_SOUND_EXPLODE:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndExplode;
                        break;
                    case PWRUP_SOUND_END:
                        sndType = powerupSounds[POWER_UP_BOBOMB].sndEnd;
                        break;
                    }

                    if (sndType != 0xFFFFFFFF)
                    {
                        if (pwrSnd == PWRUP_SOUND_ACQUIRE)
                        {
                            attrs.SetSoundType(sndType, false);
                        }
                        else
                        {
                            attrs.SetSoundType(sndType, true);
                            if ((pwrSnd == PWRUP_SOUND_IN_EFFECT) || (pwrSnd == PWRUP_SOUND_ACTIVATE))
                            {
                                attrs.UsePhysObj(pPhysObj);
                                attrs.mf_ReturnEmitterOnPlay = true;
                            }
                            else
                            {
                                attrs.UseStationaryPosVector(pPhysObj->GetPosition());
                            }
                        }

                        ApplyPowerupVolume(attrs, sndType, fVol);
                        Audio::gPowerupSFX.Play(attrs);
                    }
                }
                break;
            }
            }

            Audio::gStadGenSFX.Play((Audio::eWorldSFX)0xCE, 100.0f, -1.0f, true, 100.0f);
            EmissionController* pControl = EmissionManager::Create(pExplosionGroup, 0);
            pControl->SetPosition(m_pPhysicsObject->GetPosition());
            if ((m_v3Position.z - ((PhysicsSphere*)m_pPhysicsObject)->GetRadius()) < 1.0f)
            {
                EmissionController* pControl = EmissionManager::Create(pGroundGroup, 0);
                pControl->SetPosition(m_pPhysicsObject->GetPosition());
            }
            FireCameraRumbleFilter(0.0f, 0.2f);
        }

        Event* pEvent = g_pEventManager->CreateValidEvent(0x2C, 0x34);
        CollisionBobombDataLayout* pEventData = (CollisionBobombDataLayout*)new (/* PORT: m_data is at 0x18 here. */ (u8*)&pEvent->m_data) CollisionBobombData();
        pEventData->v3ExplosionLocation = m_v3Position;
        pEventData->fExplosionRadius = g_pGame->GetGameTweaks()->fPowerupExplosionRadius * (float)meSize
                                     + g_pGame->GetGameTweaks()->fPowerupExplosionRadius;
        pEventData->pThrower = m_pThrower;
        pEventData->nThrowerPadID = m_nThrowerPadID;
        pEventData->bIsFreezeBomb = (m_eType == POWER_UP_FREEZE_SHELL);
    }
    else if (!bSilent)
    {
        switch (meSize)
        {
        case POWERUPSIZE_LARGE:
        {
            float fVol = g_pGame->m_pGameTweaks->fPowerupLargeSizeVolCoeff;
            ePowerUpType type = m_eType;
            PhysicsObject* pPhysObj = m_pPhysicsObject;

            if (type >= NUM_POWER_UPS)
            {
                Audio::GetSndIDError();
            }
            else if (!Audio::IsInited())
            {
                Audio::GetSndIDError();
            }
            else
            {
                Audio::SoundAttributes attrs;
                attrs.Init();

                unsigned long sndType = powerupSounds[type].sndEnd;
                if (sndType != 0xFFFFFFFF)
                {
                    attrs.SetSoundType(sndType, true);
                    if (type == POWER_UP_BOBOMB)
                    {
                        attrs.UseStationaryPosVector(pPhysObj->GetPosition());
                    }
                    else
                    {
                        attrs.UseStationaryPosVector(pPhysObj->GetPosition());
                    }

                    ApplyPowerupVolume(attrs, sndType, fVol);

                    Audio::gPowerupSFX.Play(attrs);
                }
            }
            break;
        }
        case POWERUPSIZE_MEDIUM:
        {
            float fVol = g_pGame->m_pGameTweaks->fPowerupMedSizeVolCoeff;
            ePowerUpType type = m_eType;
            PhysicsObject* pPhysObj = m_pPhysicsObject;

            if (type >= NUM_POWER_UPS)
            {
                Audio::GetSndIDError();
            }
            else if (!Audio::IsInited())
            {
                Audio::GetSndIDError();
            }
            else
            {
                Audio::SoundAttributes attrs;
                attrs.Init();

                unsigned long sndType = powerupSounds[type].sndEnd;
                if (sndType != 0xFFFFFFFF)
                {
                    attrs.SetSoundType(sndType, true);
                    if (type == POWER_UP_BOBOMB)
                    {
                        attrs.UseStationaryPosVector(pPhysObj->GetPosition());
                    }
                    else
                    {
                        attrs.UseStationaryPosVector(pPhysObj->GetPosition());
                    }

                    ApplyPowerupVolume(attrs, sndType, fVol);

                    Audio::gPowerupSFX.Play(attrs);
                }
            }
            break;
        }
        case POWERUPSIZE_SMALL:
        {
            float fVol = g_pGame->m_pGameTweaks->fPowerupSmallSizeVolCoeff;
            ePowerUpType type = m_eType;
            PhysicsObject* pPhysObj = m_pPhysicsObject;

            if (type >= NUM_POWER_UPS)
            {
                Audio::GetSndIDError();
            }
            else if (!Audio::IsInited())
            {
                Audio::GetSndIDError();
            }
            else
            {
                Audio::SoundAttributes attrs;
                attrs.Init();

                unsigned long sndType = powerupSounds[type].sndEnd;
                if (sndType != 0xFFFFFFFF)
                {
                    attrs.SetSoundType(sndType, true);
                    if (type == POWER_UP_BOBOMB)
                    {
                        attrs.UseStationaryPosVector(pPhysObj->GetPosition());
                    }
                    else
                    {
                        attrs.UseStationaryPosVector(pPhysObj->GetPosition());
                    }

                    ApplyPowerupVolume(attrs, sndType, fVol);

                    Audio::gPowerupSFX.Play(attrs);
                }
            }
            break;
        }
        default:
            break;
        }
    }

    uintptr_t voiceID = m_uVoiceID;
    if (voiceID != 0)
    {
        if (voiceID != (uintptr_t)Audio::GetSndIDError())
        {
            Audio::gPowerupSFX.StopEmitter((SFXEmitter*)m_uVoiceID, 0);
            m_uVoiceID = 0;
        }
    }

    g_pPowerups[m_nIndex] = NULL;
    m_pTarget = NULL;

    if (m_pBlurHandler != NULL)
    {
        m_pBlurHandler->Die(0.5f);
        m_pBlurHandler = NULL;
    }

    UnregisterPowerup(m_pDrawableObj->GetHashID());

    delete this;
}
/**
 * Offset/Address/Size: 0x27C0 | 0x8005D0AC | size: 0x11C
 */
void PowerupBase::PreThrow(cFielder* pFielder, Bowser* pBowser)
{
    nlVector3 pos;

    if (pFielder != NULL)
    {
        if (pFielder->m_nPowerupAnimID == 0x5F || pFielder->m_nPowerupAnimID == 0x61)
        {
            pos = pFielder->GetJointPosition(pFielder->m_nLeftHandJointIndex);
        }
        else
        {
            pos = pFielder->GetJointPosition(pFielder->m_nRightHandJointIndex);
        }
    }
    else
    {
        nlVector3 localPt = { 2.5f, 0.0f, 1.2f };
        GetWorldPoint(pos, localPt, pBowser->mv3Position, pBowser->maFacingDirection);
    }

    m_v3Position = pos;
    m_pPhysicsObject->SetPosition(m_v3Position, PhysicsObject::WORLD_COORDINATES);
    m_v3Velocity = v3Zero;
    m_pPhysicsObject->SetLinearVelocity(v3Zero);
}

/**
 * Offset/Address/Size: 0x24CC | 0x8005CDB8 | size: 0x2F4
 */
void PowerupBase::UpdateTransform()
{
    nlPolar pDirectionalSpeed;
    float fSpeedNormalized;
    float fActualRadius;
    float fNormalRadius;

    nlCartesianToPolar(pDirectionalSpeed, m_v3Velocity.x, m_v3Velocity.y);
    fSpeedNormalized = NormalizeVal(pDirectionalSpeed.r, 0.0f, g_pGame->m_pGameTweaks->fGreenShellSpeed);

    {
        float z = 0.0f;
        m_aOrientation += (int)(1875.0f * fSpeedNormalized + z);
    }

    switch (m_eType)
    {
    case POWER_UP_BANANA:
        switch (meSize)
        {
        case POWERUPSIZE_LARGE:
            fActualRadius = g_pGame->m_pGameTweaks->fBananaBigRadius;
            break;
        case POWERUPSIZE_MEDIUM:
            fActualRadius = g_pGame->m_pGameTweaks->fBananaMediumRadius;
            break;
        case POWERUPSIZE_SMALL:
            fActualRadius = g_pGame->m_pGameTweaks->fBananaSmallRadius;
            break;
        }
        fNormalRadius = g_pGame->m_pGameTweaks->fBananaSmallRadius;
        break;

    case POWER_UP_BOBOMB:
        switch (meSize)
        {
        case POWERUPSIZE_LARGE:
            fActualRadius = g_pGame->m_pGameTweaks->fBobombBigRadius;
            break;
        case POWERUPSIZE_MEDIUM:
            fActualRadius = g_pGame->m_pGameTweaks->fBobombMediumRadius;
            break;
        case POWERUPSIZE_SMALL:
            fActualRadius = g_pGame->m_pGameTweaks->fBobombSmallRadius;
            break;
        }
        fNormalRadius = g_pGame->m_pGameTweaks->fBobombSmallRadius;
        break;

    default:
        switch (meSize)
        {
        case POWERUPSIZE_LARGE:
            fActualRadius = g_pGame->m_pGameTweaks->fShellBigRadius;
            break;
        case POWERUPSIZE_MEDIUM:
            fActualRadius = g_pGame->m_pGameTweaks->fShellMediumRadius;
            break;
        case POWERUPSIZE_SMALL:
            fActualRadius = g_pGame->m_pGameTweaks->fShellSmallRadius;
            break;
        }
        fNormalRadius = g_pGame->m_pGameTweaks->fShellSmallRadius;
        break;
    }

    m_scale = fActualRadius / fNormalRadius;
    ((PhysicsSphere*)m_pPhysicsObject)->SetRadius(fActualRadius);

    if (m_eType != POWER_UP_BANANA && m_eType != POWER_UP_RED_SHELL)
    {
        if (mtNoHitTimer.m_uPackedTime != 0)
        {
            m_scale = InterpolateRangeClamped(0.33f, m_scale, 0.4f, 0.0f, mtNoHitTimer.GetSeconds());
            ((PhysicsSphere*)m_pPhysicsObject)->SetRadius(m_scale * fNormalRadius);

            if (m_pBlurHandler != NULL && m_scale <= 1.0f)
            {
                m_pBlurHandler->m_fLineWidth = m_scale * m_fBlurWidth;
            }
        }
    }
    else
    {
        float fBananaTimer = mtNoHitTimer.GetSeconds() - 0.6f;
        if (fBananaTimer > 0.0f)
        {
            m_scale = InterpolateRangeClamped(0.33f, m_scale, 0.4f, 0.0f, fBananaTimer);
            if (m_pBlurHandler != NULL && m_scale <= 1.0f)
            {
                m_pBlurHandler->m_fLineWidth = m_scale * m_fBlurWidth;
            }
        }
    }

    if (m_szStreakTexture != NULL && m_pBlurHandler == NULL)
    {
        m_pBlurHandler = BlurManager::GetNewHandler(m_szStreakTexture, m_scale * m_fBlurWidth, 15, true);
    }
}

static inline void RegisterPowerup(unsigned long hashID, PowerupBase* powerup)
{
    int j = 0;
    for (; j < 25; j++)
    {
        if (powerupRegistry.registry[j].hashId == 0)
        {
            powerupRegistry.registry[j].hashId = hashID;
            powerupRegistry.registry[j].powerup = powerup;
            return;
        }
    }
    nlBreak();
}

static inline DrawableObject* AcquirePowerupModel(int type)
{
    for (int i = 0; i < 25; i++)
    {
        if (powerupModelPool.mFree[type][i])
        {
            powerupModelPool.mFree[type][i] = false;
            return powerupModelPool.mObjs[type][i];
        }
    }

    return NULL;
}

/**
 * Offset/Address/Size: 0x2360 | 0x8005CC4C | size: 0x16C
 */
void PowerupBase::Init(cFielder* pFielder, Bowser* pBowser)
{
    m_pDrawableObj = AcquirePowerupModel(m_eType);

    {
        DrawableObject* pD = m_pDrawableObj;
        pD->m_uObjectFlags |= 0x100;
        unsigned long hashID = m_pDrawableObj->GetHashID();
        RegisterPowerup(hashID, this);
    }

    PreThrow(pFielder, pBowser);

    m_pThrower = pFielder;

    if (pFielder != NULL)
    {
        s32 padID;
        bool bHasPad = pFielder->GetGlobalPad() != NULL;
        padID = bHasPad ? pFielder->GetGlobalPad()->m_padIndex : -1;
        m_nThrowerPadID = padID;
    }
}

void PowerupBase::SpeedManagement()
{
    nlPolar aShell;
    nlVector2 v2NewVelocity;
    nlVector3 v3NewVelocity;

    if (mtNoHitTimer.m_uPackedTime == 0)
    {
        nlCartesianToPolar(aShell, m_v3Velocity.x, m_v3Velocity.y);
        if (aShell.r < 3.0f)
        {
            m_bShouldDestroy = true;
        }
        else if (aShell.r > 20.0f)
        {
            v2NewVelocity = *(const nlVector2*)&m_v3Velocity;
            f32 velX = v2NewVelocity.x;
            f32 velY = v2NewVelocity.y;
            f32 sqX = velX * velX;
            f32 sqY = velY * velY;
            f32 recipLen = nlRecipSqrt(sqX + sqY, true);
            v2NewVelocity.x = recipLen * velX;
            v2NewVelocity.y = recipLen * velY;
            f32 scaledY = 19.0f * v2NewVelocity.y;
            f32 scaledX = 19.0f * v2NewVelocity.x;
            v2NewVelocity.x = scaledX;
            v2NewVelocity.y = scaledY;
            v3NewVelocity.y = v2NewVelocity.y;
            v3NewVelocity.x = v2NewVelocity.x;
            v3NewVelocity.z = m_v3Velocity.z;
            m_v3Velocity = v3NewVelocity;
            m_pPhysicsObject->SetLinearVelocity(v3NewVelocity);
        }
    }
}

unsigned long PowerupBase::GetSoundType(ePowerUpType type, PowerupBase::PowerupSound powerupSnd)
{
    unsigned long sndType;

    switch (powerupSnd)
    {
    case PWRUP_SOUND_ACQUIRE:
        sndType = powerupSounds[type].sndAcquire;
        break;
    case PWRUP_SOUND_ACTIVATE:
        sndType = powerupSounds[type].sndActivate;
        break;
    case PWRUP_SOUND_IN_EFFECT:
        sndType = powerupSounds[type].sndInEffect;
        break;
    case PWRUP_SOUND_HIT:
        sndType = powerupSounds[type].sndHit;
        break;
    case PWRUP_SOUND_BOUNCE_WALL:
        sndType = powerupSounds[type].sndBounceWall;
        break;
    case PWRUP_SOUND_BOUNCE_GROUND:
        sndType = powerupSounds[type].sndBounceGround;
        break;
    case PWRUP_SOUND_EXPLODE:
        sndType = powerupSounds[type].sndExplode;
        break;
    case PWRUP_SOUND_END:
        sndType = powerupSounds[type].sndEnd;
        break;
    }

    return sndType;
}

//  */
// PhysicsShell::~PhysicsShell()
// {
// }

/**
 * Offset/Address/Size: 0x2050 | 0x8005C93C | size: 0x2A0
 */
unsigned long PowerupBase::PlayPowerupSound(ePowerUpType type, PowerupBase::PowerupSound powerupSnd, const nlVector3& v3Pos, float fVol)
{
    PhysicsShell dummyShell(1.0f);
    dummyShell.SetPosition(v3Pos, PhysicsObject::WORLD_COORDINATES);
    return PlayPowerupSound(type, powerupSnd, &dummyShell, fVol);
}

/**
 * Offset/Address/Size: 0x1E00 | 0x8005C6EC | size: 0x250
 */
unsigned long PowerupBase::PlayPowerupSound(ePowerUpType type, PowerupBase::PowerupSound powerupSnd, PhysicsObject* pPhysObj, float fVol)
{
    Audio::SoundAttributes sndAtr;
    unsigned long sndType;
    float fDefaultVol;

    if (type >= NUM_POWER_UPS)
    {
        return Audio::GetSndIDError();
    }

    if (!Audio::IsInited())
    {
        return Audio::GetSndIDError();
    }

    sndAtr.Init();

    sndType = GetSoundType(type, powerupSnd);

    if (sndType == 0xFFFFFFFF)
    {
        return -1;
    }

    if (powerupSnd == PWRUP_SOUND_ACQUIRE)
    {
        sndAtr.SetSoundType(sndType, false);
    }
    else
    {
        sndAtr.SetSoundType(sndType, true);

        if (type == POWER_UP_BOBOMB)
        {
            if ((powerupSnd == PWRUP_SOUND_IN_EFFECT) || (powerupSnd == PWRUP_SOUND_ACTIVATE))
            {
                sndAtr.UsePhysObj(pPhysObj);
                sndAtr.mf_ReturnEmitterOnPlay = 1;
            }
            else
            {
                sndAtr.UseStationaryPosVector(pPhysObj->GetPosition());
            }
        }
        else
        {
            if (powerupSnd == PWRUP_SOUND_IN_EFFECT)
            {
                sndAtr.UsePhysObj(pPhysObj);
                sndAtr.mf_ReturnEmitterOnPlay = 1;
            }
            else
            {
                sndAtr.UseStationaryPosVector(pPhysObj->GetPosition());
            }
        }
    }

    fDefaultVol = Audio::gPowerupSFX.GetSFXVol(sndType);

    if (100.0f != fVol)
    {
        sndAtr.mf_Volume = fVol * fDefaultVol;
    }

    return Audio::gPowerupSFX.Play(sndAtr);
}

/**
 * Offset/Address/Size: 0x1DD0 | 0x8005C6BC | size: 0x30
 */
void PowerupBase::StopPowerupInEffectSound(SFXEmitter* pSFXEmitter)
{
    Audio::gPowerupSFX.StopEmitter(pSFXEmitter, 0);
}

GreenShell::GreenShell(cFielder* pTarget, int nIndex, float fRadius, ePowerupSize eSize, bool bExplode)
    : PowerupBase(pTarget, POWER_UP_GREEN_SHELL, fRadius, eSize, bExplode, nIndex)
{
}

/**
 * Offset/Address/Size: 0x1C98 | 0x8005C584 | size: 0x138
 */
GreenShell::~GreenShell()
{
}

/**
 * Offset/Address/Size: 0x1A80 | 0x8005C36C | size: 0x218
 */
void GreenShell::Update(float dt)
{
    nlPolar polar;

    m_v3PrevPosition = m_v3Position;
    m_pPhysicsObject->GetPosition(&m_v3Position);
    m_pPhysicsObject->GetLinearVelocity(&m_v3Velocity);

    if (m_v3Position.z < ((PhysicsSphere*)m_pPhysicsObject)->GetRadius())
    {
        m_v3Position.z = ((PhysicsSphere*)m_pPhysicsObject)->GetRadius();
        m_pPhysicsObject->SetPosition(m_v3Position, PhysicsObject::WORLD_COORDINATES);
    }

    if (m_pBlurHandler != NULL)
    {
        m_pBlurHandler->AddViewOrientedPoint(m_v3Position, m_v3Velocity);
    }

    DecrementTimers(dt);

    UpdateTransform();

    if (m_pBlurHandler != NULL)
    {
        nlCartesianToPolar(polar, m_v3Velocity.x, m_v3Velocity.y);
        if (polar.r < 0.5f)
        {
            m_pBlurHandler->Die(0.5f);
            m_pBlurHandler = NULL;
        }
    }

    SpeedManagement();

    if (m_bShouldDestroy)
    {
        m_pDrawableObj->m_uObjectFlags &= ~1;
        Destroy(false);
    }
}

/**
 * Offset/Address/Size: 0x19AC | 0x8005C298 | size: 0xD4
 */
void GreenShell::Destroy(bool bSilent)
{
    if (!bSilent && !g_pGame->mbCaptainShotToScoreOn)
    {
        EffectsGroup* pEffectsGroup;
        switch (meSize)
        {
        case POWERUPSIZE_LARGE:
            pEffectsGroup = fxGetGroup("green_shell_explode_big");
            break;
        case POWERUPSIZE_MEDIUM:
            pEffectsGroup = fxGetGroup("green_shell_explode_med");
            break;
        case POWERUPSIZE_SMALL:
            pEffectsGroup = fxGetGroup("green_shell_explode");
            break;
        }

        EmissionController* pController = EmissionManager::Create(pEffectsGroup, 0);
        pController->SetPosition(m_pPhysicsObject->GetPosition());
    }

    PowerupBase::Destroy(bSilent);
}

RedShell::RedShell(cFielder* pTarget, int nIndex, float fRadius, ePowerupSize eSize, bool bExplode)
    : PowerupBase(pTarget, POWER_UP_RED_SHELL, fRadius, eSize, bExplode, nIndex)
{
}

/**
 * Offset/Address/Size: 0x1874 | 0x8005C160 | size: 0x138
 */
RedShell::~RedShell()
{
}

/**
 * Offset/Address/Size: 0x1628 | 0x8005BF14 | size: 0x24C
 */
void RedShell::Update(float dt)
{
    nlPolar polar;

    m_v3PrevPosition = m_v3Position;
    m_pPhysicsObject->GetPosition(&m_v3Position);
    m_pPhysicsObject->GetLinearVelocity(&m_v3Velocity);

    if (m_v3Position.z < ((PhysicsSphere*)m_pPhysicsObject)->GetRadius())
    {
        m_v3Position.z = ((PhysicsSphere*)m_pPhysicsObject)->GetRadius();
        m_pPhysicsObject->SetPosition(m_v3Position, PhysicsObject::WORLD_COORDINATES);
    }

    if (m_pBlurHandler != NULL)
    {
        m_pBlurHandler->AddViewOrientedPoint(m_v3Position, m_v3Velocity);
    }

    DecrementTimers(dt);

    UpdateTransform();

    if (m_pBlurHandler != NULL)
    {
        nlCartesianToPolar(polar, m_v3Velocity.x, m_v3Velocity.y);
        if (polar.r < 0.5f)
        {
            m_pBlurHandler->Die(0.5f);
            m_pBlurHandler = NULL;
        }
    }

    SpeedManagement();

    if (mtActiveTimer.m_uPackedTime == 0)
    {
        m_pTarget = NULL;
    }
    else if (mtNoHitTimer.GetSeconds() < 0.8f)
    {
        SeekTarget();
    }

    if (m_bShouldDestroy)
    {
        m_pDrawableObj->m_uObjectFlags &= ~1u;
        Destroy(false);
    }
}

/**
 * Offset/Address/Size: 0x1554 | 0x8005BE40 | size: 0xD4
 */
void RedShell::Destroy(bool bSilent)
{
    if (!bSilent && !g_pGame->mbCaptainShotToScoreOn)
    {
        EffectsGroup* pEffectsGroup;
        switch (meSize)
        {
        case POWERUPSIZE_LARGE:
            pEffectsGroup = fxGetGroup("red_shell_explode_big");
            break;
        case POWERUPSIZE_MEDIUM:
            pEffectsGroup = fxGetGroup("red_shell_explode_med");
            break;
        case POWERUPSIZE_SMALL:
            pEffectsGroup = fxGetGroup("red_shell_explode");
            break;
        }

        EmissionController* pController = EmissionManager::Create(pEffectsGroup, 0);
        pController->SetPosition(m_pPhysicsObject->GetPosition());
    }

    PowerupBase::Destroy(bSilent);
}

/**
 * Offset/Address/Size: 0x13C8 | 0x8005BCB4 | size: 0x18C
 */
void RedShell::SeekTarget()
{
    float fCurrSpeed;
    float dx, dy;
    float newVelY, newVelX;
    nlVector3 v3NewVelocity;

    cFielder* target = m_pTarget;
    if (target == NULL)
    {
        return;
    }

    const nlVector3& targetPos = ((cCharacter*)target)->m_v3Position;
    dy = targetPos.x - m_v3Position.x;
    dx = targetPos.y - m_v3Position.y;

    float distSq = dy * dy + dx * dx;
    nlSqrt(distSq, true);

    float invDist = 1.0f / nlSqrt(distSq, true);
    dx = invDist * dx;
    dy = invDist * dy;

    float velX = m_v3Velocity.y;
    float velY = m_v3Velocity.x;
    fCurrSpeed = nlGetLength2D(velY, velX);

    float turnRate = 8.5f;
    float steerY = turnRate * dy;
    float steerX = turnRate * dx;
    newVelY = steerY + m_v3Velocity.x;
    newVelX = steerX + m_v3Velocity.y;

    float newSpeed = nlSqrt(newVelY * newVelY + newVelX * newVelX, true);
    float invNewSpeed = 1.0f / newSpeed;
    float normNewVelY = invNewSpeed * newVelY;
    float normNewVelX = invNewSpeed * newVelX;

    float dot = normNewVelY * dy + normNewVelX * dx;
    if (dot < 0.0f)
    {
        m_pTarget = NULL;
    }
    else
    {
        nlVec3Set(v3NewVelocity, fCurrSpeed * normNewVelY, fCurrSpeed * normNewVelX, 0.0f);
        m_v3Velocity = v3NewVelocity;
        m_pPhysicsObject->SetLinearVelocity(v3NewVelocity);
        m_pPhysicsObject->SetLinearVelocity(m_v3Velocity);
    }
}

Banana::Banana(cFielder* pTarget, int nIndex, float fRadius, ePowerupSize eSize, bool bExplode)
    : PowerupBase(pTarget, POWER_UP_BANANA, fRadius, eSize, bExplode, nIndex)
{
}

/**
 * Offset/Address/Size: 0x1290 | 0x8005BB7C | size: 0x138
 */
Banana::~Banana()
{
}

/**
 * Offset/Address/Size: 0x1130 | 0x8005BA1C | size: 0x160
 */
void Banana::Update(float dt)
{
    nlPolar polar;

    m_v3PrevPosition = m_v3Position;
    m_pPhysicsObject->GetPosition(&m_v3Position);
    m_pPhysicsObject->GetLinearVelocity(&m_v3Velocity);

    if (m_v3Position.z < ((PhysicsSphere*)m_pPhysicsObject)->GetRadius())
    {
        m_v3Position.z = ((PhysicsSphere*)m_pPhysicsObject)->GetRadius();
        m_pPhysicsObject->SetPosition(m_v3Position, PhysicsObject::WORLD_COORDINATES);
    }

    if (m_pBlurHandler != nullptr)
    {
        m_pBlurHandler->AddViewOrientedPoint(m_v3Position, m_v3Velocity);
    }

    DecrementTimers(dt);

    UpdateTransform();

    if (m_pBlurHandler != nullptr)
    {
        nlCartesianToPolar(polar, m_v3Velocity.x, m_v3Velocity.y);
        if (polar.r < 0.5f)
        {
            m_pBlurHandler->Die(0.5f);
            m_pBlurHandler = nullptr;
        }
    }

    if (mtActiveTimer.m_uPackedTime == 0)
    {
        m_bShouldDestroy = true;
    }

    if (m_bShouldDestroy)
    {
        m_pDrawableObj->m_uObjectFlags &= ~1;
        Destroy(false);
    }
}

/**
 * Offset/Address/Size: 0x105C | 0x8005B948 | size: 0xD4
 */
void Banana::Destroy(bool bSilent)
{
    if (!bSilent && !g_pGame->mbCaptainShotToScoreOn)
    {
        EffectsGroup* pEffectsGroup;
        switch (meSize)
        {
        case POWERUPSIZE_LARGE:
            pEffectsGroup = fxGetGroup("banana_explode_big");
            break;
        case POWERUPSIZE_MEDIUM:
            pEffectsGroup = fxGetGroup("banana_explode_med");
            break;
        case POWERUPSIZE_SMALL:
            pEffectsGroup = fxGetGroup("banana_explode");
            break;
        }

        EmissionController* pController = EmissionManager::Create(pEffectsGroup, 0);
        pController->SetPosition(m_pPhysicsObject->GetPosition());
    }

    PowerupBase::Destroy(bSilent);
}

SpinyShell::SpinyShell(cFielder* pTarget, int nIndex, float fRadius, ePowerupSize eSize, bool bExplode)
    : PowerupBase(pTarget, POWER_UP_SPINY_SHELL, fRadius, eSize, bExplode, nIndex)
{
}

/**
 * Offset/Address/Size: 0xF24 | 0x8005B810 | size: 0x138
 */
SpinyShell::~SpinyShell()
{
}

/**
 * Offset/Address/Size: 0xD0C | 0x8005B5F8 | size: 0x218
 */
void SpinyShell::Update(float dt)
{
    nlPolar polar;

    m_v3PrevPosition = m_v3Position;
    m_pPhysicsObject->GetPosition(&m_v3Position);
    m_pPhysicsObject->GetLinearVelocity(&m_v3Velocity);

    if (m_v3Position.z < ((PhysicsSphere*)m_pPhysicsObject)->GetRadius())
    {
        m_v3Position.z = ((PhysicsSphere*)m_pPhysicsObject)->GetRadius();
        m_pPhysicsObject->SetPosition(m_v3Position, PhysicsObject::WORLD_COORDINATES);
    }

    if (m_pBlurHandler != NULL)
    {
        m_pBlurHandler->AddViewOrientedPoint(m_v3Position, m_v3Velocity);
    }

    DecrementTimers(dt);

    UpdateTransform();

    if (m_pBlurHandler != NULL)
    {
        nlCartesianToPolar(polar, m_v3Velocity.x, m_v3Velocity.y);
        if (polar.r < 0.5f)
        {
            m_pBlurHandler->Die(0.5f);
            m_pBlurHandler = NULL;
        }
    }

    SpeedManagement();

    if (m_bShouldDestroy)
    {
        m_pDrawableObj->m_uObjectFlags &= ~1u;
        Destroy(false);
    }
}

/**
 * Offset/Address/Size: 0xC38 | 0x8005B524 | size: 0xD4
 */
void SpinyShell::Destroy(bool bSilent)
{
    if (!bSilent && !g_pGame->mbCaptainShotToScoreOn)
    {
        EffectsGroup* pEffectsGroup;
        switch (meSize)
        {
        case POWERUPSIZE_LARGE:
            pEffectsGroup = fxGetGroup("spiny_shell_explode_big");
            break;
        case POWERUPSIZE_MEDIUM:
            pEffectsGroup = fxGetGroup("spiny_shell_explode_med");
            break;
        case POWERUPSIZE_SMALL:
            pEffectsGroup = fxGetGroup("spiny_shell_explode");
            break;
        }

        EmissionController* pController = EmissionManager::Create(pEffectsGroup, 0);
        pController->SetPosition(m_pPhysicsObject->GetPosition());
    }

    PowerupBase::Destroy(bSilent);
}

FreezeShell::FreezeShell(cFielder* pTarget, int nIndex, float fRadius, ePowerupSize eSize, bool bExplode)
    : PowerupBase(pTarget, POWER_UP_FREEZE_SHELL, fRadius, eSize, bExplode, nIndex)
{
}

/**
 * Offset/Address/Size: 0xB00 | 0x8005B3EC | size: 0x138
 */
FreezeShell::~FreezeShell()
{
}

/**
 * Offset/Address/Size: 0x8E8 | 0x8005B1D4 | size: 0x218
 */
void FreezeShell::Update(float fDeltaT)
{
    nlPolar polar;

    m_v3PrevPosition = m_v3Position;
    m_pPhysicsObject->GetPosition(&m_v3Position);
    m_pPhysicsObject->GetLinearVelocity(&m_v3Velocity);

    if (m_v3Position.z < ((PhysicsSphere*)m_pPhysicsObject)->GetRadius())
    {
        m_v3Position.z = ((PhysicsSphere*)m_pPhysicsObject)->GetRadius();
        m_pPhysicsObject->SetPosition(m_v3Position, PhysicsObject::WORLD_COORDINATES);
    }

    if (m_pBlurHandler != NULL)
    {
        m_pBlurHandler->AddViewOrientedPoint(m_v3Position, m_v3Velocity);
    }

    DecrementTimers(fDeltaT);

    UpdateTransform();

    if (m_pBlurHandler != NULL)
    {
        nlCartesianToPolar(polar, m_v3Velocity.x, m_v3Velocity.y);
        if (polar.r < 0.5f)
        {
            m_pBlurHandler->Die(0.5f);
            m_pBlurHandler = NULL;
        }
    }

    SpeedManagement();

    if (m_bShouldDestroy)
    {
        m_pDrawableObj->m_uObjectFlags &= ~1u;
        Destroy(false);
    }
}

/**
 * Offset/Address/Size: 0x814 | 0x8005B100 | size: 0xD4
 */
void FreezeShell::Destroy(bool bSilent)
{
    if (!bSilent && !g_pGame->mbCaptainShotToScoreOn)
    {
        EffectsGroup* pEffectsGroup;
        switch (meSize)
        {
        case POWERUPSIZE_LARGE:
            pEffectsGroup = fxGetGroup("freeze_shell_explode_big");
            break;
        case POWERUPSIZE_MEDIUM:
            pEffectsGroup = fxGetGroup("freeze_shell_explode_med");
            break;
        case POWERUPSIZE_SMALL:
            pEffectsGroup = fxGetGroup("freeze_shell_explode");
            break;
        }

        EmissionController* pController = EmissionManager::Create(pEffectsGroup, 0);
        pController->SetPosition(m_pPhysicsObject->GetPosition());
    }

    PowerupBase::Destroy(bSilent);
}

Bobomb::Bobomb(cFielder* pTarget, int nIndex, float fRadius, ePowerupSize eSize, bool bExplode)
    : PowerupBase(pTarget, POWER_UP_BOBOMB, fRadius, eSize, bExplode, nIndex)
{
    pMovementEmitter = NULL;

    GameTweaks* pGameTweaks = g_pGame->m_pGameTweaks;
    if (nlRandomf(1.0f, &nlDefaultSeed) < pGameTweaks->fBobombMineChance)
    {
        mbIsMine = true;
    }
    else
    {
        mbIsMine = false;
    }
}

/**
 * Offset/Address/Size: 0x6DC | 0x8005AFC8 | size: 0x138
 */
Bobomb::~Bobomb()
{
}

/**
 * Offset/Address/Size: 0x428 | 0x8005AD14 | size: 0x2B4
 */
void Bobomb::Update(float dt)
{
    ePowerUpType type;
    PhysicsObject* pPhysObj;
    u32 soundID;
    nlPolar polar;
    nlVector3 pos;
    EmissionController* pController;
    Audio::SoundAttributes attributes;

    m_v3PrevPosition = m_v3Position;
    m_pPhysicsObject->GetPosition(&m_v3Position);
    m_pPhysicsObject->GetLinearVelocity(&m_v3Velocity);

    if (m_v3Position.z < ((PhysicsSphere*)m_pPhysicsObject)->GetRadius())
    {
        m_v3Position.z = ((PhysicsSphere*)m_pPhysicsObject)->GetRadius();
        m_pPhysicsObject->SetPosition(m_v3Position, PhysicsObject::WORLD_COORDINATES);
    }

    if (m_pBlurHandler != nullptr)
    {
        m_pBlurHandler->AddViewOrientedPoint(m_v3Position, m_v3Velocity);
    }

    DecrementTimers(dt);

    UpdateTransform();

    if (m_pBlurHandler != nullptr)
    {
        nlCartesianToPolar(polar, m_v3Velocity.x, m_v3Velocity.y);
        if (polar.r < 0.5f)
        {
            m_pBlurHandler->Die(0.5f);
            m_pBlurHandler = nullptr;
        }
    }

    if ((m_uVoiceID == 0) && (!g_pGame->mbCaptainShotToScoreOn))
    {
        uintptr_t errorCode; // PORT: may hold an SFXEmitter*

        type = m_eType;
        pPhysObj = m_pPhysicsObject;

        if (type >= NUM_POWER_UPS)
        {
            errorCode = Audio::GetSndIDError();
        }
        else if (!Audio::IsInited())
        {
            errorCode = Audio::GetSndIDError();
        }
        else
        {
            attributes.Init();
            soundID = powerupSounds[type].sndInEffect;

            if (soundID == 0xFFFFFFFF)
            {
                errorCode = -1;
            }
            else
            {
                attributes.SetSoundType(soundID, true);
                if (type == POWER_UP_BOBOMB)
                {
                    attributes.UsePhysObj(pPhysObj);
                    attributes.mf_ReturnEmitterOnPlay = true;
                }
                else
                {
                    attributes.UsePhysObj(pPhysObj);
                    attributes.mf_ReturnEmitterOnPlay = true;
                }

                Audio::gPowerupSFX.GetSFXVol(soundID);
                errorCode = Audio::gPowerupSFX.Play(attributes);
            }
        }

        m_uVoiceID = errorCode;
    }

    pController = EmissionManager::Create(fxGetGroup("bobomb_tick"), 0);
    pos = m_pPhysicsObject->GetPosition();
    pos.z += ((PhysicsSphere*)m_pPhysicsObject)->GetRadius();
    pController->SetPosition(pos);

    if (mtActiveTimer.m_uPackedTime == 0)
    {
        m_bShouldDestroy = true;
    }

    if (m_bShouldDestroy)
    {
        m_pDrawableObj->m_uObjectFlags &= ~1;
        Destroy(false);
    }
}

/**
 * Offset/Address/Size: 0x80 | 0x8005A96C | size: 0x3A8
 */
void Bobomb::ThrowAt(cFielder* pThrower, Bowser* pBowser)
{
    if (gBobombAnticipationVoiceID != -1 || !Audio::IsSFXPlaying(gBobombAnticipationVoiceID))
        gBobombAnticipationVoiceID = Audio::gCrowdSFX.Play((Audio::eWorldSFX)0x9E, 100.0f, -1.0f, true, 100.0f);

    if (m_pTarget->m_pTeam == g_pTeams[1])
    {
        Audio::gCrowdSFX.PlayRandomReaction(Audio::cWorldSFX::CROWD_REACTION_YEAH_SMALL, 100.0f, -1.0f, 0, 0.0f);
    }
    else
    {
        Audio::gCrowdSFX.PlayRandomReaction(Audio::cWorldSFX::CROWD_REACTION_OH_SMALL, 100.0f, -1.0f, 0, 0.0f);
    }

    int nNumSolutions;
    float pSolutions[2];
    nlVector3 v3TargetPos;
    nlVector3 v3TargetVel;

    v3TargetPos = m_pTarget->m_v3Position;
    v3TargetVel = m_pTarget->m_v3Velocity;

    CalcInterceptXY(m_v3Position, g_pGame->m_pGameTweaks->fBobombSpeed, 0.0f, v3TargetPos, v3TargetVel, nNumSolutions, pSolutions);

    float dx, dy, dz;
    float t;

    if (nNumSolutions != 0)
    {
        if (nNumSolutions == 2)
        {
            t = (pSolutions[0] < pSolutions[1]) ? pSolutions[0] : pSolutions[1];
        }
        else
        {
            t = pSolutions[0];
        }

        if (t > g_pGame->m_pGameTweaks->fMinBobombMoveSFXTime)
        {
            ePowerUpType type = m_eType;
            PhysicsObject* pPhysObj = m_pPhysicsObject;
            // PORT: Play returns the emitter here and it is cast straight back to SFXEmitter* below.
            uintptr_t errorCode;

            if (type >= NUM_POWER_UPS)
            {
                errorCode = Audio::GetSndIDError();
            }
            else if (!Audio::IsInited())
            {
                errorCode = Audio::GetSndIDError();
            }
            else
            {
                Audio::SoundAttributes attributes;
                unsigned long soundID;

                attributes.Init();
                soundID = powerupSounds[type].sndActivate;

                if (soundID == 0xFFFFFFFF)
                {
                    errorCode = -1;
                }
                else
                {
                    attributes.SetSoundType(soundID, true);

                    if (type == POWER_UP_BOBOMB)
                    {
                        attributes.UsePhysObj(pPhysObj);
                        attributes.mf_ReturnEmitterOnPlay = true;
                    }
                    else
                    {
                        attributes.UseStationaryPosVector(pPhysObj->GetPosition());
                    }

                    Audio::gPowerupSFX.GetSFXVol(soundID);
                    errorCode = Audio::gPowerupSFX.Play(attributes);
                }
            }

            pMovementEmitter = (SFXEmitter*)errorCode;
        }

        nlVector3 v3BobombVelocity;
        float targetX = v3TargetVel.x * t + v3TargetPos.x;
        float targetY = v3TargetVel.y * t + v3TargetPos.y;

        v3BobombVelocity.x = (targetX - m_v3Position.x) / t;
        v3BobombVelocity.y = (targetY - m_v3Position.y) / t;
        v3BobombVelocity.z = -(t * (0.5f * m_pPhysicsObject->m_gravity));

        cGame* game = g_pGame;
        GameTweaks* gameTweaks = game->m_pGameTweaks;

        if (v3BobombVelocity.z > game->m_pGameTweaks->fBobombMaxZSpeed)
        {
            v3BobombVelocity.z = gameTweaks->fBobombMaxZSpeed;
        }

        m_v3Velocity = v3BobombVelocity;
        m_pPhysicsObject->SetLinearVelocity(v3BobombVelocity);
    }
    else
    {
        dy = v3TargetPos.y - m_v3Position.y;
        dx = v3TargetPos.x - m_v3Position.x;
        dz = v3TargetPos.z - m_v3Position.z;
        float invDist = nlRecipSqrt((dx * dx) + (dy * dy) + (dz * dz), true);
        float speed = g_pGame->m_pGameTweaks->fBobombSpeed;
        nlVector3 v3BobombVelocity;

        v3BobombVelocity.x = speed * (invDist * dx);
        v3BobombVelocity.y = speed * (invDist * dy);
        v3BobombVelocity.z = speed * (invDist * dz);
        v3BobombVelocity.z = g_pGame->m_pGameTweaks->fBobombMaxZSpeed;

        m_v3Velocity = v3BobombVelocity;
        m_pPhysicsObject->SetLinearVelocity(v3BobombVelocity);
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8005A8EC | size: 0x80
 */
void Bobomb::Destroy(bool bSilent)
{
    if (gBobombAnticipationVoiceID != -1)
    {
        Audio::StopSFX(gBobombAnticipationVoiceID);
        gBobombAnticipationVoiceID = -1;
    }

    if (pMovementEmitter != nullptr)
    {
        Audio::gPowerupSFX.StopEmitter(pMovementEmitter, 0);
        pMovementEmitter = nullptr;
    }

    PowerupBase::Destroy(bSilent);
}
