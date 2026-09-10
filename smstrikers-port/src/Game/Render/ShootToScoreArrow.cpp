#include "Game/Render/ShootToScoreArrow.h"

#include "NL/gl/glDraw2.h"
#include "NL/gl/glState.h"

const unsigned long UnlitProgram = glGetProgram("3d unlit");
const unsigned long LitProgram = glGetProgram("3d pointlit");
const unsigned long LightTexture = glGetTexture("global/lightramp");
const unsigned long BlackTexture = glGetTexture("global/black");
const unsigned long WhiteTexture = glGetTexture("global/white");

static bool useSubtractiveDarkening = false;

/**
 * Offset/Address/Size: 0x0 | 0x8015FFE0 | size: 0x1C8
 */
void WorldDarkening::UpdateAndRender(float deltaTime)
{
    glPoly2 poly;

    mPos += mRate * deltaTime;

    if (mRate > 0.0f)
    {
        if (mPos > mTo)
        {
            mPos = mTo;
        }
    }
    if (mRate < 0.0f)
    {
        if (mPos < mTo)
        {
            mPos = mTo;
        }
    }

    if (mPos <= 0.0f)
    {
        mActive = false;
        return;
    }

    mActive = true;

    glSetDefaultState(true);
    glSetRasterState(GLS_DepthTest, 0);
    if (useSubtractiveDarkening != 0)
    {
        glSetRasterState(GLS_AlphaBlend, 7);
    }
    else
    {
        glSetRasterState(GLS_AlphaBlend, 1);
    }

    glSetCurrentTexture(glGetTexture("global/white"), GLTT_Diffuse);
    glSetCurrentTextureState(glHandleizeTextureState());
    glSetRasterState(GLS_DepthTest, 0);
    glSetCurrentRasterState(glHandleizeRasterState());

    // PORT: the frame, not 640x480.
    poly.SetupRectangle(0.0f, 0.0f, glGetOrthographicWidth(),
                        glGetOrthographicHeight(), -1.0f);

    s32 darkenAmount = 255.0f * mPos;

    if (useSubtractiveDarkening != 0)
    {
        SetPolyColour(poly, darkenAmount, darkenAmount, darkenAmount, 255);
    }
    else
    {
        SetPolyColour(poly, 0, 0, 0, darkenAmount);
    }

    poly.Attach(GLV_BigBlackPolygon, 0, NULL, -1);
    glSetDefaultState(false);
}

/**
 * Offset/Address/Size: 0x1C8 | 0x801601A8 | size: 0xC
 */
void WorldDarkening::Fade(float rate, float to)
{
    this->mRate = rate;
    this->mTo = to;
}

/**
 * Offset/Address/Size: 0x1D4 | 0x801601B4 | size: 0x40
 */
WorldDarkening& WorldDarkening::Instance()
{
    static WorldDarkening instance;
    return instance;
}
