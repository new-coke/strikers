#include "Game/Transitions/ColourBlendScreenTransition.h"

#include "NL/nlColour.h"
#include "NL/gl/glState.h"
#include "NL/gl/glDraw2.h"

#include "strtold.h"

/**
 * Offset/Address/Size: 0x380 | 0x80205044 | size: 0x10
 */
void ColourBlendScreenTransition::Update(float dt)
{
    m_fCurrentTime += dt;
}

/**
 * Offset/Address/Size: 0x1D8 | 0x80204E9C | size: 0x1A8
 */
void ColourBlendScreenTransition::Render(eGLView view)
{
    class glPoly2 poly;
    struct nlColour colour;
    class nlVector4 v4Clr;

    float temp_f8;
    float temp_f9;
    nlVector4 tmp = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (m_fLength > 0.0001f)
    {
        temp_f9 = m_fCurrentTime / m_fLength;
        temp_f8 = 1.0f - temp_f9;
        nlVec4Set(tmp,
            (temp_f8 * m_RGBAstart.e[0]) + (temp_f9 * m_RGBAend.e[0]),
            (temp_f8 * m_RGBAstart.e[1]) + (temp_f9 * m_RGBAend.e[1]),
            (temp_f8 * m_RGBAstart.e[2]) + (temp_f9 * m_RGBAend.e[2]),
            (temp_f8 * m_RGBAstart.e[3]) + (temp_f9 * m_RGBAend.e[3]));
    }

    v4Clr = tmp;
    nlColourSet(colour,
        (u8)(255.0f * v4Clr.x),
        (u8)(255.0f * v4Clr.y),
        (u8)(255.0f * v4Clr.z),
        (u8)(255.0f * v4Clr.w));

    glSetDefaultState(false);
    glSetCurrentTexture(m_nTexture, GLTT_Diffuse);
    glSetRasterState(GLS_AlphaBlend, 1);
    glSetCurrentRasterState(glHandleizeRasterState());
    glSetCurrentTextureState(glHandleizeTextureState());

    poly.FullCoverage(colour, 0.0f);
    poly.Attach(view, 0, NULL, 0xFFFFFFFF);
}

/**
 * Offset/Address/Size: 0x0 | 0x80204CC4 | size: 0x1D8
 */
ColourBlendScreenTransition* ColourBlendScreenTransition::GetFromParser(SimpleParser* parser)
{
    class nlVector4 v4StartColour;
    class nlVector4 v4EndColour;
    uintptr_t texture;
    char* pToken;

    static u32 defaultTexture = glHash("global/white");

    texture = defaultTexture;

    float fLength = atof(parser->NextTokenOnLine(true));

    v4StartColour.x = atof(parser->NextTokenOnLine(true));
    v4StartColour.y = atof(parser->NextTokenOnLine(true));
    v4StartColour.z = atof(parser->NextTokenOnLine(true));
    v4StartColour.w = atof(parser->NextTokenOnLine(true));

    v4EndColour.x = atof(parser->NextTokenOnLine(true));
    v4EndColour.y = atof(parser->NextTokenOnLine(true));
    v4EndColour.z = atof(parser->NextTokenOnLine(true));
    v4EndColour.w = atof(parser->NextTokenOnLine(true));

    pToken = parser->NextTokenOnLine(true);
    if (pToken != NULL)
    {
        texture = glHash(pToken);
    }

    ColourBlendScreenTransition* transition = new (nlMalloc(sizeof(ColourBlendScreenTransition), 8, 0)) ColourBlendScreenTransition(fLength, v4StartColour, v4EndColour, texture);
    return transition;
}
