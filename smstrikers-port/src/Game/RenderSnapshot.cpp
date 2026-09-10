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

extern bool g_GoalLightEnabled;
float g_AllActorsHidden;

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
    for (int i = 0; i < 10; i++)
    {
        mCharacters[i].Grab(*g_pCharacters[i]);
    }

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

        for (int i = 0; i < 10; i++)
        {
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
    mFrameBlendPercent = blendFactors[0];
    mValid = true;

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
