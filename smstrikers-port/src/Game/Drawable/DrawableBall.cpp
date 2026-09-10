#include "Game/Drawable/DrawableBall.h"
#include "Game/Drawable/DrawableCharacter.h"
#include "Game/Drawable/DrawableModel.h"
#include "Game/RenderSnapshot.h"
#include "Game/Ball.h"
#include "Game/CharacterTemplate.h"
#include "Game/Player.h"
#include "NL/gl/glModel.h"
#include "NL/gl/glState.h"
#include "NL/gl/glUserData.h"

static int g_nMotionBlurDivs = 5;
static float g_fMotionBlurAlpha0 = 0.1f;
static float g_fMotionBlurAlphaScale = 0.22f;

static float g_fBallBlur;

namespace
{
extern "C"
{
    extern const unsigned long eOC_NO_LIGHT;
}
} // namespace

static glModel* BallLightingCB(glModel* pModel, eGLView& view, unsigned long& uLayer);
static glModel* BallBlurCB(glModel* pModel, eGLView& view, unsigned long& uLayer);

/**
 * Offset/Address/Size: 0x0 | 0x8011DD50 | size: 0x80
 */
void DrawableBall::EvaluateFrom(DrawableCharacter& character)
{
    mPosition = character.GetBallPosition();
    mOrientation = character.GetBallOrientation();
}

/**
 * Offset/Address/Size: 0x80 | 0x8011DDD0 | size: 0x140
 */
void DrawableBall::Blend(const float* blendFactors, const DrawableBall& lhs, const DrawableBall& rhs)
{
    mPrevOrientation = mOrientation;
    nlQuatNLerp(mOrientation, lhs.mOrientation, rhs.mOrientation, *blendFactors);

    const f32 t = *blendFactors;

    mPosition.x = (1.0f - t) * lhs.mPosition.x + t * rhs.mPosition.x;
    mPosition.y = (1.0f - t) * lhs.mPosition.y + t * rhs.mPosition.y;
    mPosition.z = (1.0f - t) * lhs.mPosition.z + t * rhs.mPosition.z;

    mVelocity.x = (1.0f - t) * lhs.mVelocity.x + t * rhs.mVelocity.x;
    mVelocity.y = (1.0f - t) * lhs.mVelocity.y + t * rhs.mVelocity.y;
    mVelocity.z = (1.0f - t) * lhs.mVelocity.z + t * rhs.mVelocity.z;

    mVisible = lhs.mVisible && rhs.mVisible;

    mOwnerIndex = rhs.mOwnerIndex;
    mPrevOwnerIndex = rhs.mPrevOwnerIndex;
    mPassTargetIndex = rhs.mPassTargetIndex;
}

inline void DrawableBall::RenderMotionBlur(DrawableObject& obj) const
{
    glModel* (*savedBlurCB)(glModel*, eGLView&, unsigned long&);
    u8 savedBallShadowDisabled;
    float t;
    nlQuaternion q;
    nlMatrix4 mView;
    nlMatrix4 mWorld;
    nlMatrix4 mSaved;

    glViewGetViewMatrix(GLV_Unshadowed, mView);

    const float blurOffsetScale = 0.0078125f;
    nlVector3 blurOffset;
    nlVec3Set(blurOffset,
        blurOffsetScale * mView.m13,
        blurOffsetScale * mView.m23,
        blurOffsetScale * mView.m33);

    savedBallShadowDisabled = DrawableModel::GetBallShadowDisabled();
    DrawableModel::SetBallShadowDisabled(1);

    mSaved = obj.GetWorldMatrix();
    savedBlurCB = obj.GetCallback();
    obj.m_CB = BallBlurCB;

    int i;
    unsigned long uSavedFlags = obj.m_uObjectCreationFlags;
    obj.m_uObjectCreationFlags = uSavedFlags | eOC_NO_LIGHT;

    for (i = 0; i < g_nMotionBlurDivs; i++)
    {
        t = (float)i / (float)g_nMotionBlurDivs;
        g_fBallBlur = g_fMotionBlurAlphaScale * (1.0f - t) + g_fMotionBlurAlpha0;

        nlQuatNLerp(q, mPrevOrientation, mOrientation, t);
        obj.m_orientation = q;
        obj.m_worldMatrixUpToDate = 0;

        mWorld = obj.GetWorldMatrix();
        mWorld.m41 += blurOffset.x;
        mWorld.m42 += blurOffset.y;
        mWorld.m43 += blurOffset.z;
        obj.m_worldMatrix = mWorld;

        obj.Draw();
    }

    DrawableModel::SetBallShadowDisabled(savedBallShadowDisabled);
    obj.m_worldMatrix = mSaved;
    obj.m_CB = savedBlurCB;
    obj.m_uObjectCreationFlags = uSavedFlags;
}

void DrawableBall::RenderLighting(DrawableObject& obj) const
{
    u8 savedBallShadowDisabled = DrawableModel::GetBallShadowDisabled();
    DrawableModel::SetBallShadowDisabled(1);

    glModel* (*savedLightingCB)(glModel*, eGLView&, unsigned long&) = obj.GetCallback();
    obj.m_CB = BallLightingCB;

    unsigned long uSavedFlags = obj.m_uObjectCreationFlags;
    // PORT: BallLightingCB does `uLayer += 2`, and DrawModel hands it m_uRenderLayer itself rather than a copy.
    unsigned long uSavedLayer = obj.m_uRenderLayer;
    obj.m_uObjectCreationFlags &= ~0x80;
    obj.Draw();

    DrawableModel::SetBallShadowDisabled(savedBallShadowDisabled);
    obj.m_CB = savedLightingCB;
    obj.m_uObjectCreationFlags = uSavedFlags;
    obj.m_uRenderLayer = uSavedLayer;
}

namespace
{
extern "C"
{
    const unsigned long eOC_NO_LIGHT = 0x80;
}
} // namespace

/**
 * Offset/Address/Size: 0x1C0 | 0x8011DF10 | size: 0x47C
 */
void DrawableBall::Render() const
{
    DrawableObject* drawable = g_pBall->GetDrawableBall();
    if (mVisible)
    {
        drawable->m_uObjectFlags |= 1;
    }
    else
    {
        drawable->m_uObjectFlags &= ~1;
    }
    if (!mVisible)
    {
        return;
    }

    DrawableObject* pDrawableBall = g_pBall->GetDrawableBall();
    pDrawableBall->m_orientation = mOrientation;
    pDrawableBall->m_worldMatrixUpToDate = 0;
    pDrawableBall->m_translation = mPosition;
    pDrawableBall->m_worldMatrixUpToDate = 0;

    unsigned long uSavedFlags = pDrawableBall->m_uObjectCreationFlags;
    pDrawableBall->m_uObjectCreationFlags &= ~0x80;
    pDrawableBall->Draw();
    pDrawableBall->m_uObjectCreationFlags = uSavedFlags;

    RenderMotionBlur(*pDrawableBall);
    RenderLighting(*pDrawableBall);
}

/**
 * Offset/Address/Size: 0x63C | 0x8011E38C | size: 0xB4
 */
static glModel* BallLightingCB(glModel* pModel, eGLView& view, unsigned long& uLayer)
{
    u32 tex;
    glModelPacket* pPacket;

    static u32 WhiteTexture = glGetTexture("global/white");

    pPacket = pModel->packets;
    tex = WhiteTexture;
    while (pPacket < &pModel->packets[pModel->numPackets])
    {
        pPacket->state.texture[0] = tex;
        glSetRasterState(pPacket->state.raster, GLS_AlphaBlend, 4);
        pPacket++;
    }

    uLayer += 2;
    return pModel;
}

/**
 * Offset/Address/Size: 0x6F0 | 0x8011E440 | size: 0x10C
 */
glModel* BallBlurCB(glModel* pModel, eGLView& view, unsigned long& uLayer)
{
    glModelPacket* pPacket;
    if (g_fBallBlur == 0.0f)
    {
        return nullptr;
    }

    void* pUserDataHandle = glUserAlloc(GLUD_ConstantColour, 4, false);
    u8* pColorData = (u8*)glUserGetData(pUserDataHandle);

    const float alphaFloat = 255.0f * g_fBallBlur;

    pColorData[0] = 0xC8;
    pColorData[1] = 0xC8;
    pColorData[2] = 0xC8;
    pColorData[3] = (u8)(int)alphaFloat;

    pPacket = pModel->packets;
    while (pPacket < &pModel->packets[pModel->numPackets])
    {
        if (g_fBallBlur != 1.0f)
        {
            glSetRasterState(pPacket->state.raster, GLS_AlphaBlend, 1);
            glSetRasterState(pPacket->state.raster, GLS_DepthWrite, 0);
            glUserAttach(pUserDataHandle, pPacket, false);
        }
        pPacket++;
    }

    return pModel;
}

/**
 * Offset/Address/Size: 0x7FC | 0x8011E54C | size: 0xCC
 */
void DrawableBall::Grab()
{
    mOrientation = g_pBall->m_qOrientation;
    mPosition = g_pBall->m_v3Position;
    mVelocity = g_pBall->m_v3Velocity;
    mOwnerIndex = GetCharacterIndex(g_pBall->m_pOwner);
    mPrevOwnerIndex = GetCharacterIndex(g_pBall->m_pPrevOwner);
    mPassTargetIndex = GetCharacterIndex(g_pBall->m_pPassTarget);
    mLastTouchIndex = GetCharacterIndex(g_pBall->m_pLastTouch);
    mVisible = true;
}

/**
 * Offset/Address/Size: 0x8C8 | 0x8011E618 | size: 0x24
 */
DrawableCharacter* DrawableBall::IndexToPlayer(int index) const
{
    if (index == -1)
    {
        return nullptr;
    }
    return &mRenderSnapshot->mCharacters[index];
}

/**
 * Offset/Address/Size: 0xA24 | 0x8011E774 | size: 0x138
 */
template <>
void DrawableBall::Replay<SaveFrame>(SaveFrame& frame)
{
    Replayable<1, SaveFrame, FloatCompressor<-127, 127, 7> >(frame, FloatCompressor<-127, 127, 7>(mPosition.x));
    Replayable<1, SaveFrame, FloatCompressor<-127, 127, 7> >(frame, FloatCompressor<-127, 127, 7>(mPosition.y));
    Replayable<1, SaveFrame, FloatCompressor<-127, 127, 7> >(frame, FloatCompressor<-127, 127, 7>(mPosition.z));

    Replayable<1, SaveFrame, FloatCompressor<-1, 1, 13> >(frame, FloatCompressor<-1, 1, 13>(mOrientation.x));
    Replayable<1, SaveFrame, FloatCompressor<-1, 1, 13> >(frame, FloatCompressor<-1, 1, 13>(mOrientation.y));
    Replayable<1, SaveFrame, FloatCompressor<-1, 1, 13> >(frame, FloatCompressor<-1, 1, 13>(mOrientation.z));
    Replayable<1, SaveFrame, FloatCompressor<-1, 1, 13> >(frame, FloatCompressor<-1, 1, 13>(mOrientation.w));

    Replayable<1, SaveFrame, FloatCompressor<-127, 127, 5> >(frame, FloatCompressor<-127, 127, 5>(mVelocity.x));
    Replayable<1, SaveFrame, FloatCompressor<-127, 127, 5> >(frame, FloatCompressor<-127, 127, 5>(mVelocity.y));
    Replayable<1, SaveFrame, FloatCompressor<-127, 127, 5> >(frame, FloatCompressor<-127, 127, 5>(mVelocity.z));

    Replayable<1, SaveFrame, bool>(frame, mVisible);
    Replayable<1, SaveFrame, char>(frame, (char&)mOwnerIndex);
    Replayable<1, SaveFrame, char>(frame, (char&)mPrevOwnerIndex);
    Replayable<1, SaveFrame, char>(frame, (char&)mPassTargetIndex);
    Replayable<1, SaveFrame, char>(frame, (char&)mLastTouchIndex);
}

/**
 * Offset/Address/Size: 0x8EC | 0x8011E63C | size: 0x138
 */
template <>
void DrawableBall::Replay<LoadFrame>(LoadFrame& frame)
{
    Replayable<1, LoadFrame, FloatCompressor<-127, 127, 7> >(frame, FloatCompressor<-127, 127, 7>(mPosition.x));
    Replayable<1, LoadFrame, FloatCompressor<-127, 127, 7> >(frame, FloatCompressor<-127, 127, 7>(mPosition.y));
    Replayable<1, LoadFrame, FloatCompressor<-127, 127, 7> >(frame, FloatCompressor<-127, 127, 7>(mPosition.z));

    Replayable<1, LoadFrame, FloatCompressor<-1, 1, 13> >(frame, FloatCompressor<-1, 1, 13>(mOrientation.x));
    Replayable<1, LoadFrame, FloatCompressor<-1, 1, 13> >(frame, FloatCompressor<-1, 1, 13>(mOrientation.y));
    Replayable<1, LoadFrame, FloatCompressor<-1, 1, 13> >(frame, FloatCompressor<-1, 1, 13>(mOrientation.z));
    Replayable<1, LoadFrame, FloatCompressor<-1, 1, 13> >(frame, FloatCompressor<-1, 1, 13>(mOrientation.w));

    Replayable<1, LoadFrame, FloatCompressor<-127, 127, 5> >(frame, FloatCompressor<-127, 127, 5>(mVelocity.x));
    Replayable<1, LoadFrame, FloatCompressor<-127, 127, 5> >(frame, FloatCompressor<-127, 127, 5>(mVelocity.y));
    Replayable<1, LoadFrame, FloatCompressor<-127, 127, 5> >(frame, FloatCompressor<-127, 127, 5>(mVelocity.z));

    Replayable<1, LoadFrame, bool>(frame, mVisible);
    Replayable<1, LoadFrame, char>(frame, (char&)mOwnerIndex);
    Replayable<1, LoadFrame, char>(frame, (char&)mPrevOwnerIndex);
    Replayable<1, LoadFrame, char>(frame, (char&)mPassTargetIndex);
    Replayable<1, LoadFrame, char>(frame, (char&)mLastTouchIndex);
}
