#include "PhotoFlashEffect.h"

#include "NL/nlColour.h"
#include "NL/gl/glState.h"
#include "NL/gl/glDraw2.h"

static s32 sNumFlashFadeFrames = 8;

s32 PhotoFlash::sNumFramesSinceFlash = 999;

// PORT: seconds since Flash(); the fade is timed, not frame-counted.
static float sSecondsSinceFlash = 999.0f;

/**
 * Offset/Address/Size: 0xC4 | 0x8016B024 | size: 0xC
 */
void PhotoFlash::Flash()
{
    sNumFramesSinceFlash = 0;
    sSecondsSinceFlash = 0.0f;
}

/**
 * Offset/Address/Size: 0x0 | 0x8016AF60 | size: 0xC4
 */
void PhotoFlash::Render(float dt)
{
    glPoly2 poly;
    u8 value;
    // PORT: the console's fade was sNumFlashFadeFrames fields of wall time, and the frame rate here is not the field rate.
    const float fadeSeconds = (float)sNumFlashFadeFrames / 60.0f;
    if (sSecondsSinceFlash < fadeSeconds)
    {
        glSetDefaultState(false);
        glSetRasterState(GLS_AlphaBlend, 2);
        glSetCurrentRasterState(glHandleizeRasterState());

        value = (u8)(255.0f * (1.0f - sSecondsSinceFlash / fadeSeconds));

        nlColour c = { 0, 0, 0, 0 };
        c.c[0] = value;
        c.c[1] = value;
        c.c[2] = value;
        c.c[3] = 255;

        poly.SetColour(c);
        // PORT: the frame, not 640x480.
        poly.SetupRectangle(0.0f, 0.0f, glGetOrthographicWidth(),
                            glGetOrthographicHeight(), 0.0f);
        glAttachPoly2(GLV_FrontEnd, 1, &poly, 0, 0);
    }

    sSecondsSinceFlash += dt;
    ++sNumFramesSinceFlash;
}
