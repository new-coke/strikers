#include "Game/Drawable/DrawableExplosionFragment.h"

#include "Game/Game.h"
#include "Game/Render/SidelineExplodable.h"
#include "Game/World.h"
#include "Game/WorldManager.h"
#include "Game/Drawable/DrawableObj.h"

/**
 * Offset/Address/Size: 0x1D4 | 0x8011F974 | size: 0xBC
 */
void DrawableExplosionFragment::Grab()
{
    ExplosionFragment* fragment = SidelineExplodableManager::GetFragmentFromHandle(mID);
    if (fragment == NULL)
    {
        mVisible = false;
        return;
    }

    mVisible = true;
    mFragmentModelHash = fragment->mFragmentModelHash;

    mPosition = fragment->ExplosionFragment::GetPosition();

    static nlMatrix4 temp;
    fragment->ExplosionFragment::GetRotation(&temp);
    nlMatrixToQuat(mOrientation, temp);

    if (fragment->mfRemainingLifespan > ExplosionFragment::sfFadeOutTime)
    {
        mOpacity = 1.0f;
    }
    else
    {
        float ratio = fragment->mfRemainingLifespan;
        ratio /= ExplosionFragment::sfFadeOutTime;
        mOpacity = ratio;
    }
}

/**
 * Offset/Address/Size: 0xF8 | 0x8011F898 | size: 0xDC
 */
void DrawableExplosionFragment::Render() const
{
    if (g_pGame->mbCaptainShotToScoreOn == 0)
    {
        DrawableObject* obj = WorldManager::s_World->FindDrawableObject(mFragmentModelHash);
        if (obj != NULL)
        {
            obj->m_translation = mPosition;
            obj->m_worldMatrixUpToDate = false;
            obj->m_orientation = mOrientation;
            obj->m_worldMatrixUpToDate = false;

            obj->m_translucency = mOpacity;
            if (obj->m_translucency < 0.0f)
            {
                obj->m_translucency = 0.0f;
            }
            if (obj->m_translucency > 1.0f)
            {
                obj->m_translucency = 1.0f;
            }

            if (mVisible)
            {
                obj->Draw();
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8011F7A0 | size: 0xF8
 */
void DrawableExplosionFragment::Blend(const float* w, const DrawableExplosionFragment& a, const DrawableExplosionFragment& b)
{
    mVisible = a.mVisible && b.mVisible && (a.mFragmentModelHash == b.mFragmentModelHash);

    if (!mVisible)
        return;

    const float t = w[2];
    const float it = 1.0f - t;

    mPosition.x = it * a.mPosition.x + t * b.mPosition.x;
    mPosition.y = it * a.mPosition.y + t * b.mPosition.y;
    mPosition.z = it * a.mPosition.z + t * b.mPosition.z;

    nlQuatSlerp(mOrientation, a.mOrientation, b.mOrientation, t);

    mFragmentModelHash = a.mFragmentModelHash;
    float opacity = (a.mOpacity + b.mOpacity);
    float factor = 0.5f;
    mOpacity = opacity * factor; // otherwise it does not match the asm
}

template void DrawableExplosionFragment::Replay<SaveFrame>(SaveFrame&);
template void DrawableExplosionFragment::Replay<LoadFrame>(LoadFrame&);
