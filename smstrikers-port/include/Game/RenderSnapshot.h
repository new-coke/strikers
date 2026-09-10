#ifndef _RENDERSNAPSHOT_H_
#define _RENDERSNAPSHOT_H_

#include "types.h"
#include "NL/nlMath.h"

#include "Game/Replay.h"

#include "Game/Drawable/DrawableBall.h"
#include "Game/Drawable/DrawableCharacter.h"
#include "Game/Drawable/DrawablePowerup.h"
#include "Game/Drawable/DrawableExplosionFragment.h"
#include "Game/Drawable/DrawableNetMesh.h"
#include "Game/Physics/PhysicsNet.h"
#include "Game/Render/CrowdManager.h"
#include "Game/Effects/EmissionManager.h"

extern f32 g_fFixedUpdateTick;
class PowerupBase;

class RenderSnapshot
{
public:
    template <typename T>
    void Replay(T&);

    RenderSnapshot()
        : mValid(false)
        , mBall(this)
        , mpNetMeshPositiveX(NULL)
        , mpNetMeshNegativeX(NULL)
        , mFrameBlendPercent(0.0f)
    {
    }

    void Initialize();
    void Free();
    void Grab();
    DrawablePowerup& GetPowerup(const PowerupBase& base) const;
    int NumDrawableObjects() const;
    nlVector3 GetPositionForDrawableObject(int i) const;
    void Invalidate();
    void Render(float deltaTime) const;
    void RenderDebugInfo(const RenderSnapshot& previous, const RenderSnapshot& current, float fBlend) const;
    void Blend(const float* blendFactors, const RenderSnapshot& lhs, const RenderSnapshot& rhs);
    RenderSnapshot& GetMutable();
    inline DrawableCharacter& GetCharacter(int index)
    {
        return mCharacters[index];
    }

    /*  0x0000 */ unsigned int mEvents;
    /*  0x0004 */ bool mValid;
    /*  0x0005 */ bool mGoalLight;
    /*  0x0008 */ DrawableBall mBall;
    /*  0x004C */ DrawableCharacter mCharacters[10];
    /*  0x03BC */ DrawablePowerup mPowerups[150];
    /*  0x167C */ DrawableExplosionFragment mExplosionFragments[20];
    /*  0x19EC */ DrawableCharacter mChainChomp;
    /*  0x1A44 */ DrawableCharacter mBowser;
    /*  0x1A9C */ DrawableNetMesh* mpNetMeshPositiveX;
    /*  0x1AA0 */ DrawableNetMesh* mpNetMeshNegativeX;
    /*  0x1AA4 */ bool mDoGoalieNetTestPosX;
    /*  0x1AA5 */ bool mDoGoalieNetTestNegX;
    /*  0x1AA8 */ unsigned char* mpExplodableVisibilityRecords;
    /*  0x1AAC */ int mNumExplodables;
    /*  0x1AB0 */ nlVector3 mCameraUp;
    /*  0x1ABC */ float mFrameBlendPercent;
}; // total size: 0x1AC0

template <typename T>
struct RenderSnapshotFrameTraits
{
    enum
    {
        IsLoadFrame = false
    };
};

template <>
struct RenderSnapshotFrameTraits<LoadFrame>
{
    enum
    {
        IsLoadFrame = true
    };
};

template <typename T>
void RenderSnapshot::Replay(T& frame)
{
    typedef RenderSnapshotFrameTraits<T> FrameTraits;
    LoadFrame& loadFrame = reinterpret_cast<LoadFrame&>(frame);
    nlVector3* pos;

    for (int i = 0; i < 10; i++)
    {
        Replayable<0, T, DrawableCharacter>(frame, mCharacters[i]);
    }

    for (int i = 0; i < 150; i++)
    {
        Replayable<0, T, DrawablePowerup>(frame, mPowerups[i]);
    }

    for (int i = 0; i < 20; i++)
    {
        Replayable<0, T, DrawableExplosionFragment>(frame, mExplosionFragments[i]);
    }

    for (int i = 0; i < mNumExplodables; i++)
    {
        Replayable<0, T, bool>(frame, reinterpret_cast<bool&>(mpExplodableVisibilityRecords[i]));
    }

    Replayable<0, T, DrawableCharacter>(frame, mChainChomp);
    Replayable<0, T, DrawableCharacter>(frame, mBowser);
    Replayable<0, T, DrawableBall>(frame, mBall);
    Replayable<1, T, nlVector3>(frame, mCameraUp);
    Replayable<1, T, bool>(frame, mGoalLight);

    Replayable<1, T, CrowdManager>(frame, CrowdManager::instance);

    if (!FrameTraits::IsLoadFrame || loadFrame.mReplayNonBlendables == REPLAY_NON_BLENDABLES)
    {
        Replayable<3, T, EmissionManager>(frame, EmissionManager::InstanceForReplayOnly());
    }

    if (NetMesh::s_bAnimatedNetMeshEnabled)
    {
        if (FrameTraits::IsLoadFrame && frame.mInterval == 1)
        {
            static bool doUpdate = false;
            static nlVector3 ballPrevPosition = { 0.0f, 0.0f, 0.0f };

            if (doUpdate)
            {
                doUpdate = false;

                pos = &mBall.mPosition;
                NetMesh::spPositiveXNetMesh->Update(g_fFixedUpdateTick, *pos, ballPrevPosition, mDoGoalieNetTestPosX, nullptr);
                NetMesh::spNegativeXNetMesh->Update(g_fFixedUpdateTick, *pos, ballPrevPosition, mDoGoalieNetTestNegX, nullptr);
                ballPrevPosition = *pos;

                mpNetMeshPositiveX->Grab(*PhysicsNet::spPhysNetPositiveX->mpNetMesh);
                mpNetMeshNegativeX->Grab(*PhysicsNet::spPhysNetNegativeX->mpNetMesh);
            }

            static float previousTime = 0.0f;

            if (loadFrame.mNonBlendableAheadOfFrame > 0.0f)
            {
                if (loadFrame.mNonBlendableAheadOfFrame < previousTime)
                {
                    mpNetMeshPositiveX->Grab(*PhysicsNet::spPhysNetPositiveX->mpNetMesh);
                    mpNetMeshNegativeX->Grab(*PhysicsNet::spPhysNetNegativeX->mpNetMesh);
                    doUpdate = true;
                }
                previousTime = loadFrame.mNonBlendableAheadOfFrame;
            }
        }

        Replayable<1, T, DrawableNetMesh>(frame, *mpNetMeshPositiveX);
        Replayable<1, T, DrawableNetMesh>(frame, *mpNetMeshNegativeX);
    }

    mValid = true;
}

// PORT: `template <>` here made this a specialisation, which stops RenderSnapshot.cpp's explicit instantiation emitting anything.

// PORT: `template <>` here made this a specialisation, which stops RenderSnapshot.cpp's explicit instantiation emitting anything.

#endif // _RENDERSNAPSHOT_H_
