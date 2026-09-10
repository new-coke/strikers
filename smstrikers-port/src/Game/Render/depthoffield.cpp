#include "Game/Render/depthoffield.h"

#include "NL/nlColour.h"
#include "NL/gl/glState.h"
#include "NL/gl/glConstant.h"
#include "NL/gl/glDraw2.h"
#include "NL/gl/glDraw3.h"

DepthOfFieldManager DepthOfFieldManager::instance;
static const u32 DOFTexture = glGetTexture("target/dof");

/**
 * Offset/Address/Size: 0x208 | 0x80163794 | size: 0x4
 */
void DepthOfFieldManager::Initialize()
{
    // EMPTY
}

/**
 * Offset/Address/Size: 0x1FC | 0x80163788 | size: 0xC
 */
void DepthOfFieldManager::TurnOn()
{
    m_bOn = true;
}

/**
 * Offset/Address/Size: 0x1F0 | 0x8016377C | size: 0xC
 */
void DepthOfFieldManager::TurnOff()
{
    m_bOn = false;
}

/**
 * Offset/Address/Size: 0x0 | 0x8016358C | size: 0x1F0
 */
void DepthOfFieldManager::Update()
{
    nlVector4 vRange = { 0, 0, 0, 0 };
    vRange.x = m_fDistanceFromCamera;
    vRange.y = (m_bOn != 0) ? 1.0f : 0.0f;
    glConstantSet("dof/range", vRange);

    if (m_bOn == 0)
        return;

    nlVector4 v4Depth = glConstantGet("dof/depth");

    int alpha = (int)255.5f * (int)0;
    {
        float f = 255.5f * m_fIntensity;
        int a = (int)f;
        if (a < 0)
            a = 0;
        if (a > 255)
            a = 255;
        alpha = a;
    }

    glSetDefaultState(0);
    glSetTextureState(GLTS_DiffuseWrap, 3);

    nlColour c;
    if (m_bDebugView)
    {
        unsigned long tex = glGetTexture("global/white");
        glSetCurrentTexture(tex, GLTT_Diffuse);
        glSetRasterState(GLS_AlphaBlend, 1);
        c.c[0] = 0xFF;
        c.c[1] = 0xC8;
        c.c[2] = 0xC8;
        c.c[3] = 0x80;
    }
    else
    {
        unsigned long tex = glGetTexture("target/dof");
        glSetCurrentTexture(tex, GLTT_Diffuse);
        c.c[0] = 0xFF;
        c.c[1] = 0xFF;
        c.c[2] = 0xFF;
        c.c[3] = (unsigned char)alpha;
        if (alpha != 0xFF)
        {
            glSetRasterState(GLS_AlphaBlend, 1);
        }
    }

    glSetRasterState(GLS_DepthTest, 1);
    glSetCurrentRasterState(glHandleizeRasterState());
    glSetCurrentTextureState(glHandleizeTextureState());

    glPoly2 p;
    // PORT: the frame, not 640x480.
    p.SetupRectangle(0.0f, 0.0f, glGetOrthographicWidth(),
                     glGetOrthographicHeight(), v4Depth.x);
    p.SetColour(c);
    p.Attach(GLV_DepthOfField, 0, 0, (unsigned long)-1);
}
