#include "Game/RenderSnapshot.h"

#include "Game/Drawable/DrawableNetMesh.h"
#include "Game/Render/SidelineExplodable.h"
#include "Game/BasicStadium.h"
#include "Game/Render/NPCManager.h"
#include "Game/Camera/CameraMan.h"
#include "Game/CharacterTemplate.h"
#include "Game/Render/NetMesh.h"
#include "Game/Physics/PhysicsNet.h"
#include "Game/Goalie.h"
#include "Game/Render/SkinAnimatedMovableNPC.h"
#include "Game/GL/GLInventory.h"
#include "Game/GL/GLTextureAnim.h"
#include "NL/gl/glState.h"
#include "Game/Render/CrowdManager.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/Character.h"
#if defined(PORT_VITA)
#include "port/native_jobs.hpp"
#include <cstdlib>
#endif

extern bool g_GoalLightEnabled;
float g_AllActorsHidden;

#if defined(PORT_VITA)
namespace
{
bool VitaGameParallelEnabled()
{
    static const bool enabled = [] {
        const char* value = std::getenv("STRIKERS_VITA_GAME_PARALLEL");
        return value == nullptr || value[0] != '0';
    }();
    return enabled;
}

struct CharacterBlendJob
{
    RenderSnapshot* dst;
    const RenderSnapshot* lhs;
    const RenderSnapshot* rhs;
    const float* factors;
    unsigned int alreadyBlendedMask;
};

bool BlendCharacters(void* opaque, size_t begin, size_t end, uint32_t) noexcept
{
    CharacterBlendJob& job = *static_cast<CharacterBlendJob*>(opaque);
    for (size_t i = begin; i < end; ++i)
    {
        if ((job.alreadyBlendedMask & (1u << i)) != 0)
            continue;
        job.dst->mCharacters[i].Blend(job.factors, job.lhs->mCharacters[i], job.rhs->mCharacters[i]);
    }
    return true;
}

struct SnapshotObjectBlendJob
{
    RenderSnapshot* dst;
    const RenderSnapshot* lhs;
    const RenderSnapshot* rhs;
    const float* factors;
};

bool BlendSnapshotObjects(void* opaque, size_t begin, size_t end, uint32_t) noexcept
{
    SnapshotObjectBlendJob& job = *static_cast<SnapshotObjectBlendJob*>(opaque);
    for (size_t item = begin; item < end; ++item)
    {
        if (item < 150)
        {
            job.dst->mPowerups[item].Blend(job.factors, job.lhs->mPowerups[item], job.rhs->mPowerups[item]);
        }
        else
        {
            const size_t fragment = item - 150;
            job.dst->mExplosionFragments[fragment].Blend(
                job.factors, job.lhs->mExplosionFragments[fragment], job.rhs->mExplosionFragments[fragment]);
        }
    }
    return true;
}

struct CharacterGrabJob
{
    RenderSnapshot* dst;
    unsigned int serialMask;
};

bool GrabCharacters(void* opaque, size_t begin, size_t end, uint32_t) noexcept
{
    CharacterGrabJob& job = *static_cast<CharacterGrabJob*>(opaque);
    for (size_t i = begin; i < end; ++i)
    {
        if ((job.serialMask & (1u << i)) != 0)
            continue;
        job.dst->mCharacters[i].Grab(*g_pCharacters[i]);
    }
    return true;
}

struct CharacterSkinPoseJob
{
    const RenderSnapshot* snapshot;
    unsigned int serialMask;
};

bool PoseCharacterSkins(void* opaque, size_t begin, size_t end, uint32_t) noexcept
{
    CharacterSkinPoseJob& job = *static_cast<CharacterSkinPoseJob*>(opaque);
    for (size_t i = begin; i < end; ++i)
    {
        if ((job.serialMask & (1u << i)) != 0 || !job.snapshot->mCharacters[i].mVisible)
            continue;
        cCharacter* character = g_pCharacters[i];
        if (character != nullptr)
            character->PoseSkinMesh(job.snapshot->mCharacters[i].mPoseAccumulator);
    }
    return true;
}
}
#endif

/**
 * Offset/Address/Size: 0x844 | 0x80113518 | size: 0x13C
 */
void RenderSnapshot::Initialize()
{
    DrawableNetMesh* pNetMesh = new (nlMalloc(sizeof(DrawableNetMesh), 8, false)) DrawableNetMesh(true);
    mpNetMeshPositiveX = pNetMesh;

    pNetMesh = new (nlMalloc(sizeof(DrawableNetMesh), 8, false)) DrawableNetMesh(false);
    mpNetMeshNegativeX = pNetMesh;

    for (int i = 0; i < 20; i++)
    {
        mExplosionFragments[i].mID = i;
    }

    mNumExplodables = SidelineExplodableManager::GetNumExplodables();
    mpExplodableVisibilityRecords = (unsigned char*)nlMalloc(mNumExplodables, 8, false);

    mValid = false;
}

/**
 * Offset/Address/Size: 0x7AC | 0x80113480 | size: 0x98
 */
void RenderSnapshot::Free()
{
    for (int i = 0; i < 10; i++)
    {
        mCharacters[i].Free();
    }

    mChainChomp.Free();
    mBowser.Free();

    delete mpNetMeshPositiveX;
    delete mpNetMeshNegativeX;
    delete[] mpExplodableVisibilityRecords;

    mpNetMeshPositiveX = nullptr;
    mpNetMeshNegativeX = nullptr;
    mpExplodableVisibilityRecords = nullptr;
}

/**
 * Offset/Address/Size: 0x624 | 0x801132F8 | size: 0x188
 */
void RenderSnapshot::Grab()
{
    mNumNisExtraBalls = 0;
#if defined(PORT_VITA)
    if (VitaGameParallelEnabled() && port::jobs::worker_threads() != 0)
    {
        unsigned int serialMask = 0;
        for (int i = 0; i < 10; ++i)
        {
            // DrawableCharacter::Grab lazily allocates the snapshot pose. Keep
            // first-use allocation on CPU0; steady-state copies are disjoint.
            if (mCharacters[i].mPoseAccumulator == nullptr)
            {
                mCharacters[i].Grab(*g_pCharacters[i]);
                serialMask |= 1u << i;
            }
        }
        CharacterGrabJob job = { this, serialMask };
        (void)port::jobs::parallel_for(10, port::jobs::game_execution_lanes()>=4 ? 2 : 3, GrabCharacters, &job);
    }
    else
    {
#endif
    for (int i = 0; i < 10; i++)
    {
        mCharacters[i].Grab(*g_pCharacters[i]);
    }
#if defined(PORT_VITA)
    }
#endif

    for (int i = 0; i < 150; i++)
    {
        mPowerups[i].Grab(i);
    }

    for (int i = 0; i < 20; i++)
    {
        mExplosionFragments[i].Grab();
    }

    if (mpExplodableVisibilityRecords != nullptr)
    {
        SidelineExplodableManager::GetVisibilityOfExplodableModels((bool*)mpExplodableVisibilityRecords, mNumExplodables);
    }

    {
        const BasicStadium* stadium = BasicStadium::GetCurrentStadium();
        mChainChomp.Grab(*(stadium->mpNPCManager->mpChainChomp));
    }

    {
        const BasicStadium* stadium = BasicStadium::GetCurrentStadium();
        mBowser.Grab(*(stadium->mpNPCManager->mpBowser));
    }

    mBall.Grab();

    mGoalLight = g_GoalLightEnabled;

    if (NetMesh::s_bAnimatedNetMeshEnabled)
    {
        if (mpNetMeshPositiveX != nullptr)
        {
            PhysicsNet* physNet = PhysicsNet::spPhysNetPositiveX;
            mpNetMeshPositiveX->Grab(*physNet->mpNetMesh);
        }
        if (mpNetMeshNegativeX != nullptr)
        {
            PhysicsNet* physNet = PhysicsNet::spPhysNetNegativeX;
            mpNetMeshNegativeX->Grab(*physNet->mpNetMesh);
        }

        mDoGoalieNetTestPosX = Goalie::mbPosGoalieNetCheck;
        mDoGoalieNetTestNegX = Goalie::mbNegGoalieNetCheck;
    }

    int stackSize = cCameraManager::m_UpVectorStackSize;
    mCameraUp = cCameraManager::m_UpVectorStack[stackSize];

    mValid = true;
}

/**
 * Retail size: 0x84 (erased)
 */
DrawablePowerup& RenderSnapshot::GetPowerup(const PowerupBase& base) const
{
    for (int i = 0; i < 150; i++)
    {
        if (mPowerups[i].GetPowerup(i) == &base)
        {
            return const_cast<DrawablePowerup&>(mPowerups[i]);
        }
    }

    return const_cast<DrawablePowerup&>(mPowerups[0]);
}

/**
 * Offset/Address/Size: 0x51C | 0x801131F0 | size: 0x108
 */
int RenderSnapshot::NumDrawableObjects() const
{
    int ret = 11;

    if (mChainChomp.mVisible)
        ret = 12;
    if (mBowser.mVisible)
        ret++;

    for (int i = 0; i < 150; i++)
    {
        if (mPowerups[i].mVisible)
            ret++;
    }

    return ret;
}

/**
 * Offset/Address/Size: 0x3DC | 0x801130B0 | size: 0x140
 */
nlVector3 RenderSnapshot::GetPositionForDrawableObject(int i) const
{
    int j;

    if (i == 0)
    {
        return mBall.mPosition;
    }

    int charIndex = i - 1;
    if (charIndex < 10)
    {
        const nlVector3* pos = &mCharacters[charIndex].mPosition;
        return *pos;
    }

    int remaining = i - 11;

    if (remaining == 0)
    {
        if (mChainChomp.mVisible)
        {
            return mChainChomp.mPosition;
        }
    }
    else
    {
        remaining--;
    }

    if (remaining == 0)
    {
        if (mBowser.mVisible)
        {
            return mBowser.mPosition;
        }
    }
    else
    {
        remaining--;
    }

    for (j = 0, i = 0; i < 150; i++, j++)
    {
        if (mPowerups[i].mVisible)
        {
            if (remaining == 0)
            {
                const nlVector3* pos = &mPowerups[j].mPosition;
                return *pos;
            }
            remaining--;
        }
    }

    nlVector3 defaultPos = { 0.0f, 0.0f, 0.0f };
    return defaultPos;
}

/**
 * Offset/Address/Size: 0x3D0 | 0x801130A4 | size: 0xC
 */
void RenderSnapshot::Invalidate()
{
    mValid = false;
}

/**
 * Offset/Address/Size: 0x21C | 0x80112EF0 | size: 0x1B4
 */
void RenderSnapshot::Render(float deltaTime) const
{
    float hideTime = 0.0f;
    bool bAllActorsHidden = false;

    if (g_AllActorsHidden > 0.0f)
    {
        g_AllActorsHidden -= deltaTime;
        bAllActorsHidden = true;
    }

    if (!mValid)
        return;

    if (!bAllActorsHidden)
    {
        {
            const BasicStadium* stadium = BasicStadium::GetCurrentStadium();
            mChainChomp.Render(*(stadium->mpNPCManager->mpChainChomp));
        }

        {
            const BasicStadium* stadium = BasicStadium::GetCurrentStadium();
            mBowser.Render(*(stadium->mpNPCManager->mpBowser));
        }

#if defined(PORT_VITA)
        const bool parallelSkin = VitaGameParallelEnabled() && port::jobs::worker_threads() != 0;
        if (parallelSkin)
        {
            // ShaderSkinMesh::Pose mutates only the character-owned pose matrix
            // tree and morph weights. Warm each concrete mesh once on CPU0 so
            // no legacy allocator/tree insertion can happen on a helper core.
            static GLSkinMesh* warmedMesh[10] = {};
            unsigned int serialMask = 0;
            for (int i = 0; i < 10; ++i)
            {
                if (!mCharacters[i].mVisible || g_pCharacters[i] == nullptr)
                    continue;
                GLSkinMesh* mesh = g_pCharacters[i]->GetSkinMesh();
                if (mesh != warmedMesh[i])
                {
                    g_pCharacters[i]->PoseSkinMesh(mCharacters[i].mPoseAccumulator);
                    warmedMesh[i] = mesh;
                    serialMask |= 1u << i;
                }
            }

            CharacterSkinPoseJob poseJob = { this, serialMask };
            (void)port::jobs::parallel_for(10, port::jobs::game_execution_lanes()>=4 ? 2 : 3, PoseCharacterSkins, &poseJob);

            // GX state emission remains strictly ordered on CPU0. Only the
            // matrix/morph preparation above is parallel.
            for (int i = 0; i < 10; ++i)
                mCharacters[i].Render(*g_pCharacters[i], false);
        }
        else
#endif
        {
            for (int i = 0; i < 10; i++)
                mCharacters[i].Render(*g_pCharacters[i]);
        }

        for (int i = 0; i < 150; i++)
        {
            mPowerups[i].Render(i);
        }

        for (int i = 0; i < 20; i++)
        {
            mExplosionFragments[i].Render();
        }

        mBall.Render();
        for (unsigned i = 0; i < mNumNisExtraBalls; ++i)
            mNisExtraBalls[i].Render();
    }

    mpNetMeshPositiveX->Render();
    mpNetMeshNegativeX->Render();
    SidelineExplodableManager::SetVisibilityOfUnexplodedModels((bool*)mpExplodableVisibilityRecords, mNumExplodables);

    int stackSize = cCameraManager::m_UpVectorStackSize;
    cCameraManager::m_UpVectorStack[stackSize] = mCameraUp;

    static u32 texanim = glGetTexture("wario_stadium/goallight.ifl");

    GLTextureAnim* pTexAnim = glInventory.GetTextureAnim(texanim);
    if (pTexAnim != nullptr)
    {
        pTexAnim->m_isStopped = !mGoalLight;
    }
}

/**
 * Offset/Address/Size: 0x218 | 0x80112EEC | size: 0x4
 */
void RenderSnapshot::RenderDebugInfo(const RenderSnapshot& previous, const RenderSnapshot& current, float fBlend) const
{
}

/**
 * Offset/Address/Size: 0xC | 0x80112CE0 | size: 0x20C
 */
void RenderSnapshot::Blend(const float* blendFactors, const RenderSnapshot& lhs, const RenderSnapshot& rhs)
{
    mNumNisExtraBalls = 0;
    mFrameBlendPercent = blendFactors[0];
    mValid = true;

#if defined(PORT_VITA)
    if (VitaGameParallelEnabled() && port::jobs::worker_threads() != 0)
    {
        // DrawableCharacter::Blend lazily allocates its pose accumulator. Keep
        // those rare first allocations on CPU0 because the legacy allocator was
        // never designed as a general multi-threaded heap. Once allocated, each
        // character owns disjoint destination pose/matrix storage and can blend
        // independently on CPU0/CPU2.
        unsigned int serialCharacterMask = 0;
        for (int i = 0; i < 10; ++i)
        {
            if (mCharacters[i].mPoseAccumulator == nullptr)
            {
                mCharacters[i].Blend(blendFactors, lhs.mCharacters[i], rhs.mCharacters[i]);
                serialCharacterMask |= 1u << i;
            }
        }

        CharacterBlendJob characterJob = { this, &lhs, &rhs, blendFactors, serialCharacterMask };
        (void)port::jobs::parallel_for(10, port::jobs::game_execution_lanes()>=4 ? 2 : 3, BlendCharacters, &characterJob);

        // Powerups and explosion fragments are pure value interpolation with
        // disjoint destinations. Batch them into one job so the barrier cost is
        // amortized over 170 objects.
        SnapshotObjectBlendJob objectJob = { this, &lhs, &rhs, blendFactors };
        (void)port::jobs::parallel_for(170, 48, BlendSnapshotObjects, &objectJob);
    }
    else
    {
#endif
    for (int i = 0; i < 10; i++)
    {
        mCharacters[i].Blend(blendFactors, lhs.mCharacters[i], rhs.mCharacters[i]);
    }

    for (int i = 0; i < 150; i++)
    {
        mPowerups[i].Blend(blendFactors, lhs.mPowerups[i], rhs.mPowerups[i]);
    }

    for (int i = 0; i < 20; i++)
    {
        mExplosionFragments[i].Blend(blendFactors, lhs.mExplosionFragments[i], rhs.mExplosionFragments[i]);
    }
#if defined(PORT_VITA)
    }
#endif

    mChainChomp.Blend(blendFactors, lhs.mChainChomp, rhs.mChainChomp);
    mBowser.Blend(blendFactors, lhs.mBowser, rhs.mBowser);
    mBall.Blend(blendFactors, lhs.mBall, rhs.mBall);

    mpNetMeshPositiveX->Blend(blendFactors[0], *lhs.mpNetMeshPositiveX, *rhs.mpNetMeshPositiveX);
    mpNetMeshNegativeX->Blend(blendFactors[0], *lhs.mpNetMeshNegativeX, *rhs.mpNetMeshNegativeX);

    float blend = blendFactors[0];
    mCameraUp.x = (1.0f - blend) * lhs.mCameraUp.x + blend * rhs.mCameraUp.x;
    mCameraUp.y = (1.0f - blend) * lhs.mCameraUp.y + blend * rhs.mCameraUp.y;
    mCameraUp.z = (1.0f - blend) * lhs.mCameraUp.z + blend * rhs.mCameraUp.z;

    mGoalLight = rhs.mGoalLight;
    mDoGoalieNetTestPosX = rhs.mDoGoalieNetTestPosX;
    mDoGoalieNetTestNegX = rhs.mDoGoalieNetTestNegX;

    for (int i = 0; i < mNumExplodables; i++)
    {
        unsigned char visible = 0;
        if (lhs.mpExplodableVisibilityRecords[i] || rhs.mpExplodableVisibilityRecords[i])
        {
            visible = 1;
        }
        mpExplodableVisibilityRecords[i] = visible;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80112CD4 | size: 0xC
 */
RenderSnapshot& RenderSnapshot::GetMutable()
{
    mValid = true;
    return *this;
}

template void RenderSnapshot::Replay<SaveFrame>(SaveFrame&);
template void RenderSnapshot::Replay<LoadFrame>(LoadFrame&);
