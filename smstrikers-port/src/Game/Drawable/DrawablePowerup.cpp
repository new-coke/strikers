#include "Game/Drawable/DrawablePowerup.h"
#include "Game/Drawable/DrawableModel.h"
#include "Game/WorldManager.h"
#include "Game/AI/Powerups.h"
#include "NL/gl/glDraw3.h"
#include "NL/gl/glState.h"
#include "NL/nlString.h"

namespace
{
char* GetName(int idx)
{
    static char powerupLookup[] = "powerup_generated_xxx";

    if (idx < 10)
    {
        powerupLookup[18] = idx + '0';
        powerupLookup[19] = '\0';
        powerupLookup[20] = '\0';
    }
    else if (idx < 100)
    {
        powerupLookup[18] = (idx / 10) + '0';
        powerupLookup[19] = (idx % 10) + '0';
        powerupLookup[20] = '\0';
    }
    else
    {
        powerupLookup[18] = (idx / 100) + '0';
        powerupLookup[19] = ((idx % 100) / 10) + '0';
        powerupLookup[20] = (idx % 10) + '0';
    }

    return powerupLookup;
}
} // namespace

/**
 * Offset/Address/Size: 0x504 | 0x8011F178 | size: 0x1A0
 */
static void DrawShadow(float radius, float x, float y, float z)
{
    float one = 1.0f;
    float zero = 0.0f;
    float half_dim;
    float frac = z / 10.0f;

    if (frac < zero)
    {
        frac = zero;
    }

    if (frac > one)
    {
        frac = one;
    }

    half_dim = one - frac;
    float shadowRadius = (float)(half_dim * (1.75 * radius) + frac * (4.0f * radius));
    int alpha = (int)(150.0f * half_dim + 48.0f * frac);

    if (alpha < 0)
    {
        alpha = 0;
    }

    if (alpha > 0xFF)
    {
        alpha = 0xFF;
    }

    glQuad3 quad;
    quad.m_pos[0].x = x - shadowRadius;
    quad.m_pos[0].y = y - shadowRadius;
    quad.m_pos[0].z = 0.015625f;
    quad.m_pos[1].x = x - shadowRadius;
    quad.m_pos[1].y = y + shadowRadius;
    quad.m_pos[1].z = 0.015625f;
    quad.m_pos[2].x = x + shadowRadius;
    quad.m_pos[2].y = y + shadowRadius;
    quad.m_pos[2].z = 0.015625f;
    quad.m_pos[3].x = x + shadowRadius;
    quad.m_pos[3].y = y - shadowRadius;
    quad.m_pos[3].z = 0.015625f;

    quad.m_uv[0].x = 1.0f;
    quad.m_uv[0].y = 1.0f;
    quad.m_uv[1].x = 0.0f;
    quad.m_uv[1].y = 1.0f;
    quad.m_uv[2].x = 0.0f;
    quad.m_uv[2].y = 0.0f;
    quad.m_uv[3].x = 1.0f;
    quad.m_uv[3].y = 0.0f;

    unsigned char colour[4];
    colour[0] = 0xFF;
    colour[1] = 0xFF;
    colour[2] = 0xFF;
    colour[3] = (unsigned char)alpha;

    // PORT: was a pack into `unsigned long` and four word writes.
    for (int colourIdx = 0; colourIdx < 4; colourIdx++)
    {
        quad.m_colour[colourIdx].c[0] = colour[0];
        quad.m_colour[colourIdx].c[1] = colour[1];
        quad.m_colour[colourIdx].c[2] = colour[2];
        quad.m_colour[colourIdx].c[3] = colour[3];
    }

    glSetDefaultState(true);
    glSetRasterState(GLS_AlphaBlend, 1);
    glSetRasterState(GLS_Culling, 0);
    glSetRasterState(GLS_DepthWrite, 0);
    glSetCurrentRasterState(glHandleizeRasterState());

    unsigned long texture = glGetTexture("global/ball_shadow");
    glSetCurrentTexture(texture, GLTT_Diffuse);
    glSetTextureState(GLTS_DiffuseWrap, 3);
    glSetCurrentTextureState(glHandleizeTextureState());

    quad.Attach(GLV_Unshadowed, 0, true);
}

/**
 * Retail size: 0x128 (erased)
 */
PowerupBase* DrawablePowerup::GetPowerup(int idx) const
{
    return FindPowerUp(nlStringLowerHash(GetName(idx)));
}

/**
 * Offset/Address/Size: 0x374 | 0x8011EFE8 | size: 0x190
 */
void DrawablePowerup::Grab(int idx)
{
    PowerupBase* powerup = FindPowerUp(nlStringLowerHash(GetName(idx)));
    if (powerup != NULL)
    {
        mType = powerup->m_eType;
        mVisible = true;
        mRadius = powerup->GetRadius();
        mOrientation = powerup->m_aOrientation;
        mPosition = powerup->m_v3Position;
        mScale = powerup->m_scale;
    }
    else
    {
        mVisible = false;
    }
}

/**
 * Offset/Address/Size: 0xF8 | 0x8011ED6C | size: 0x27C
 */
void DrawablePowerup::Render(int idx) const
{
    World* world = WorldManager::s_World;
    DrawableObject* obj = world->FindDrawableObject(nlStringLowerHash(GetName(idx)));

    if (obj == NULL)
    {
        return;
    }

    nlQuaternion quat;
    float angle = 0.0000958738f * (float)mOrientation;
    const nlVector3 axis = { 0.0f, 0.0f, 1.0f };
    nlMakeQuat(quat, axis, angle);

    if (mVisible)
    {
        obj->m_uObjectFlags |= 1;
    }
    else
    {
        obj->m_uObjectFlags &= ~1;
    }

    obj->m_orientation = quat;
    obj->m_worldMatrixUpToDate = false;
    obj->m_translation = mPosition;
    obj->m_worldMatrixUpToDate = false;
    obj->m_scale = mScale;
    obj->m_worldMatrixUpToDate = false;

    if (!mVisible)
    {
        return;
    }

    obj->Draw();
    DrawableModel* model = obj->AsDrawableModel();

    if (model == NULL)
    {
        DrawShadow(mRadius, mPosition.x, mPosition.y, mPosition.z);
    }
    else
    {
        model->DrawPlanarShadow();
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8011EC74 | size: 0xF8
 */
void DrawablePowerup::Blend(const float* blendFactors, const DrawablePowerup& lhs, const DrawablePowerup& rhs)
{
    mVisible = lhs.mVisible && rhs.mVisible;
    if (!mVisible)
        return;

    float* factors = (float*)blendFactors;

    mType = lhs.mType;
    mScale = (1.0f - factors[2]) * lhs.mScale + factors[2] * rhs.mScale;
    mRadius = lhs.mRadius;
    mOrientation = lhs.mOrientation + (s16)((s16)(rhs.mOrientation - lhs.mOrientation) * blendFactors[2]);
    nlVecLerp(mPosition, lhs.mPosition, rhs.mPosition, blendFactors[2]);
}

template void DrawablePowerup::Replay<SaveFrame>(SaveFrame&);
template void DrawablePowerup::Replay<LoadFrame>(LoadFrame&);
