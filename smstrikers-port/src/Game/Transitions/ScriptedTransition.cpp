#include "Game/Transitions/ScriptedTransition.h"

#include "stdlib.h"
#include "strtold.h"

#include "NL/gl/glState.h"
#include "NL/gl/glStruct.h"
#include "NL/nlString.h"

inline float parseFloat(const char* str, float defaultValue = 0.0f)
{
    if (str != nullptr)
    {
        return atof(str);
    }
    return defaultValue;
}

namespace TransitionModifiers
{

class ScaleModel : public TransitionModifierInterface
{
public:
    ScaleModel()
    {
        m_v2StartScale.x = 1.0f;
        m_v2StartScale.y = 1.0f;
        m_v2EndScale.x = 1.0f;
        m_v2EndScale.y = 1.0f;
    }

    /**
     * Offset/Address/Size: 0x1870 | 0x8020819C | size: 0x5C
     */
    virtual ~ScaleModel() { }

    /**
     * Offset/Address/Size: 0x18CC | 0x802081F8 | size: 0xBC
     */
    virtual void InitializeFromParser(SimpleParser* parser)
    {
        m_v2StartScale.x = atof(parser->NextTokenOnLine(true));
        m_v2StartScale.y = atof(parser->NextTokenOnLine(true));
        f32 val = m_v2StartScale.x;
        char* token = parser->NextTokenOnLine(true);
        if (token != nullptr)
        {
            val = atof(token);
        }
        m_v2EndScale.x = val;

        val = m_v2StartScale.y;
        token = parser->NextTokenOnLine(true);
        if (token != nullptr)
        {
            val = atof(token);
        }
        m_v2EndScale.y = val;
    }

    /**
     * Offset/Address/Size: 0x1988 | 0x802082B4 | size: 0x8C
     */
    virtual void ApplyModifier(glPoly2& poly, float time)
    {
        float x = ((1.0f - time) * m_v2StartScale.x) + (time * m_v2EndScale.x);
        float y = ((1.0f - time) * m_v2StartScale.y) + (time * m_v2EndScale.y);

        poly.m_pos[0].x *= x;
        poly.m_pos[0].y *= y;
        poly.m_pos[1].x *= x;
        poly.m_pos[1].y *= y;
        poly.m_pos[2].x *= x;
        poly.m_pos[2].y *= y;
        poly.m_pos[3].x *= x;
        poly.m_pos[3].y *= y;
    }

    /* 0x4 */ nlVector2 m_v2StartScale;
    /* 0xC */ nlVector2 m_v2EndScale;
}; // total size: 0x14

class ColourBlend : public TransitionModifierInterface
{
public:
    ColourBlend()
    {
        nlColourSet(m_cStartColour, 0xFF, 0xFF, 0xFF, 0xFF);
        nlColourSet(m_cEndColour, 0xFF, 0xFF, 0xFF, 0xFF);
    }

    /**
     * Offset/Address/Size: 0x1638 | 0x80207F64 | size: 0x5C
     */
    virtual ~ColourBlend() { }

    /**
     * Offset/Address/Size: 0x1694 | 0x80207FC0 | size: 0x11C
     */
    virtual void InitializeFromParser(SimpleParser* parser)
    {
        m_cStartColour.c[0] = atoi(parser->NextTokenOnLine(true));
        m_cStartColour.c[1] = atoi(parser->NextTokenOnLine(true));
        m_cStartColour.c[2] = atoi(parser->NextTokenOnLine(true));
        m_cStartColour.c[3] = atoi(parser->NextTokenOnLine(true));

        unsigned char val = m_cStartColour.c[0];
        char* token = parser->NextTokenOnLine(true);
        if (token != nullptr)
        {
            val = atoi(token);
        }
        m_cEndColour.c[0] = val;

        val = m_cStartColour.c[1];
        token = parser->NextTokenOnLine(true);
        if (token != nullptr)
        {
            val = atoi(token);
        }
        m_cEndColour.c[1] = val;

        val = m_cStartColour.c[2];
        token = parser->NextTokenOnLine(true);
        if (token != nullptr)
        {
            val = atoi(token);
        }
        m_cEndColour.c[2] = val;

        val = m_cStartColour.c[3];
        token = parser->NextTokenOnLine(true);
        if (token != nullptr)
        {
            val = atoi(token);
        }
        m_cEndColour.c[3] = val;
    }

    /**
     * Offset/Address/Size: 0x17B0 | 0x802080DC | size: 0xC0
     */
    virtual void ApplyModifier(glPoly2& poly, float time)
    {
        nlColour col;
        s32 temp_r6;
        u32 temp_r3;

        temp_r3 = 255.0f * time;
        temp_r6 = 0xFF - temp_r3;

        col.c[0] = ((temp_r6 * m_cStartColour.c[0]) + (temp_r3 * m_cEndColour.c[0])) >> 8;
        col.c[1] = ((temp_r6 * m_cStartColour.c[1]) + (temp_r3 * m_cEndColour.c[1])) >> 8;
        col.c[2] = ((temp_r6 * m_cStartColour.c[2]) + (temp_r3 * m_cEndColour.c[2])) >> 8;
        col.c[3] = ((temp_r6 * m_cStartColour.c[3]) + (temp_r3 * m_cEndColour.c[3])) >> 8;
        poly.SetColour(col);
    }

    /* 0x4 */ nlColour m_cStartColour;
    /* 0x8 */ nlColour m_cEndColour;
}; // total size: 0xC

class ScaleTexture : public TransitionModifierInterface
{
public:
    ScaleTexture()
    {
        m_v2StartShift.x = 1.0f;
        m_v2StartShift.y = 1.0f;
        m_v2EndShift.x = 1.0f;
        m_v2EndShift.y = 1.0f;
    }

    /**
     * Offset/Address/Size: 0x1494 | 0x80207DC0 | size: 0x5C
     */
    virtual ~ScaleTexture() { }

    /**
     * Offset/Address/Size: 0x14F0 | 0x80207E1C | size: 0xBC
     */
    virtual void InitializeFromParser(SimpleParser* parser)
    {
        m_v2StartShift.x = atof(parser->NextTokenOnLine(true));
        m_v2StartShift.y = atof(parser->NextTokenOnLine(true));
        f32 val = m_v2StartShift.x;
        char* token = parser->NextTokenOnLine(true);
        if (token != nullptr)
        {
            val = atof(token);
        }
        m_v2EndShift.x = val;

        val = m_v2StartShift.y;
        token = parser->NextTokenOnLine(true);
        if (token != nullptr)
        {
            val = atof(token);
        }
        m_v2EndShift.y = val;
    }

    /**
     * Offset/Address/Size: 0x15AC | 0x80207ED8 | size: 0x8C
     */
    virtual void ApplyModifier(glPoly2& poly, float time)
    {
        float x = ((1.0f - time) * m_v2StartShift.x) + (time * m_v2EndShift.x);
        float y = ((1.0f - time) * m_v2StartShift.y) + (time * m_v2EndShift.y);

        poly.m_uv[0].x *= x;
        poly.m_uv[0].y *= y;
        poly.m_uv[1].x *= x;
        poly.m_uv[1].y *= y;
        poly.m_uv[2].x *= x;
        poly.m_uv[2].y *= y;
        poly.m_uv[3].x *= x;
        poly.m_uv[3].y *= y;
    }

    /* 0x4 */ nlVector2 m_v2StartShift;
    /* 0xC */ nlVector2 m_v2EndShift;
}; // total size: 0x14

class TranslateModel : public TransitionModifierInterface
{
public:
    TranslateModel()
    {
        m_v2StartShift.x = 0.0f;
        m_v2StartShift.y = 0.0f;
        m_v2EndShift.x = 0.0f;
        m_v2EndShift.y = 0.0f;
    }

    /**
     * Offset/Address/Size: 0x12F0 | 0x80207C1C | size: 0x5C
     */
    virtual ~TranslateModel() { }

    /**
     * Offset/Address/Size: 0x134C | 0x80207C78 | size: 0xBC
     */
    virtual void InitializeFromParser(SimpleParser* parser)
    {
        m_v2StartShift.x = atof(parser->NextTokenOnLine(true));
        m_v2StartShift.y = atof(parser->NextTokenOnLine(true));
        f32 val = m_v2StartShift.x;
        char* token = parser->NextTokenOnLine(true);
        if (token != nullptr)
        {
            val = atof(token);
        }
        m_v2EndShift.x = val;

        val = m_v2StartShift.y;
        token = parser->NextTokenOnLine(true);
        if (token != nullptr)
        {
            val = atof(token);
        }
        m_v2EndShift.y = val;
    }

    /**
     * Offset/Address/Size: 0x1408 | 0x80207D34 | size: 0x8C
     */
    virtual void ApplyModifier(glPoly2& poly, float time)
    {
        float x = ((1.0f - time) * m_v2StartShift.x) + (time * m_v2EndShift.x);
        float y = ((1.0f - time) * m_v2StartShift.y) + (time * m_v2EndShift.y);

        poly.m_pos[0].x += x;
        poly.m_pos[0].y += y;
        poly.m_pos[1].x += x;
        poly.m_pos[1].y += y;
        poly.m_pos[2].x += x;
        poly.m_pos[2].y += y;
        poly.m_pos[3].x += x;
        poly.m_pos[3].y += y;
    }

    /* 0x4 */ nlVector2 m_v2StartShift;
    /* 0xC */ nlVector2 m_v2EndShift;
}; // total size: 0x14

class TranslateTexture : public TransitionModifierInterface
{
public:
    TranslateTexture()
    {
        m_v2StartShift.x = 0.0f;
        m_v2StartShift.y = 0.0f;
        m_v2EndShift.x = 0.0f;
        m_v2EndShift.y = 0.0f;
    }

    /**
     * Offset/Address/Size: 0x114C | 0x80207A78 | size: 0x5C
     */
    virtual ~TranslateTexture() { }

    /**
     * Offset/Address/Size: 0x11A8 | 0x80207AD4 | size: 0xBC
     */
    virtual void InitializeFromParser(SimpleParser* parser)
    {
        m_v2StartShift.x = atof(parser->NextTokenOnLine(true));
        m_v2StartShift.y = atof(parser->NextTokenOnLine(true));
        f32 val = m_v2StartShift.x;
        char* token = parser->NextTokenOnLine(true);
        if (token != nullptr)
        {
            val = atof(token);
        }
        m_v2EndShift.x = val;

        val = m_v2StartShift.y;
        token = parser->NextTokenOnLine(true);
        if (token != nullptr)
        {
            val = atof(token);
        }
        m_v2EndShift.y = val;
    }

    /**
     * Offset/Address/Size: 0x1264 | 0x80207B90 | size: 0x8C
     */
    virtual void ApplyModifier(glPoly2& poly, float time)
    {
        float x = ((1.0f - time) * m_v2StartShift.x) + (time * m_v2EndShift.x);
        float y = ((1.0f - time) * m_v2StartShift.y) + (time * m_v2EndShift.y);

        poly.m_uv[0].x += x;
        poly.m_uv[0].y += y;
        poly.m_uv[1].x += x;
        poly.m_uv[1].y += y;
        poly.m_uv[2].x += x;
        poly.m_uv[2].y += y;
        poly.m_uv[3].x += x;
        poly.m_uv[3].y += y;
    }

    /* 0x04 */ nlVector2 m_v2StartShift;
    /* 0x0C */ nlVector2 m_v2EndShift;
}; // total size: 0x14

class RotateModel : public TransitionModifierInterface
{
public:
    RotateModel()
    {
        m_angleStart = 0.0f;
        m_angleEnd = 0.0f;
        m_v3Axis.x = 0.0f;
        m_v3Axis.y = 0.0f;
        m_v3Axis.z = 1.0f;
    }

    /**
     * Offset/Address/Size: 0xF40 | 0x8020786C | size: 0x5C
     */
    virtual ~RotateModel() { }

    /**
     * Offset/Address/Size: 0xF9C | 0x802078C8 | size: 0xC8
     */
    virtual void InitializeFromParser(SimpleParser* parser)
    {
        f32 angleEnd;

        m_v3Axis.x = atof(parser->NextTokenOnLine(true));
        m_v3Axis.y = atof(parser->NextTokenOnLine(true));
        m_v3Axis.z = atof(parser->NextTokenOnLine(true));
        m_angleStart = atof(parser->NextTokenOnLine(true));

        angleEnd = m_angleStart;
        char* token = parser->NextTokenOnLine(true);
        if (token != nullptr)
        {
            angleEnd = atof(token);
        }
        m_angleEnd = angleEnd;
    }

    /**
     * Offset/Address/Size: 0x1064 | 0x80207990 | size: 0xE8
     */
    virtual void ApplyModifier(glPoly2& poly, float time)
    {
        nlMatrix4 m4;
        nlMatrix3 m3;
        nlQuaternion quat;

        nlMakeQuat(quat, m_v3Axis, (3.1415927f * ((m_angleStart * (1.0f - time)) + (time * m_angleEnd))) / 180.0f);
        nlQuatToMatrix(m4, quat);

        m3.e[0] = m4.m11;
        m3.e[1] = m4.m12;
        m3.e[3] = m4.m21;
        m3.e[4] = m4.m22;
        m3.e[7] = 0.0f;
        m3.e[6] = 0.0f;
        m3.e[5] = 0.0f;
        m3.e[2] = 0.0f;
        m3.e[8] = 1.0f;

        for (int i = 0; i < 4; i++)
        {
            nlVector2 temp;
            nlMultVectorMatrix(temp, poly.m_pos[i], m3);
            poly.m_pos[i] = temp;
        }
    }

    /* 0x04 */ nlVector3 m_v3Axis;
    /* 0x10 */ float m_angleStart;
    /* 0x14 */ float m_angleEnd;
}; // total size: 0x18

class RotateTexture : public TransitionModifierInterface
{
public:
    RotateTexture()
    {
        m_angleStart = 0.0f;
        m_angleEnd = 0.0f;
        m_v3Axis.x = 0.0f;
        m_v3Axis.y = 0.0f;
        m_v3Axis.z = 1.0f;
    }

    /**
     * Offset/Address/Size: 0xD34 | 0x80207660 | size: 0x5C
     */
    virtual ~RotateTexture() { }

    /**
     * Offset/Address/Size: 0xD90 | 0x802076BC | size: 0xC8
     */
    virtual void InitializeFromParser(SimpleParser* parser)
    {
        f32 angleEnd;

        m_v3Axis.x = atof(parser->NextTokenOnLine(true));
        m_v3Axis.y = atof(parser->NextTokenOnLine(true));
        m_v3Axis.z = atof(parser->NextTokenOnLine(true));
        m_angleStart = atof(parser->NextTokenOnLine(true));

        angleEnd = m_angleStart;
        char* token = parser->NextTokenOnLine(true);
        if (token != nullptr)
        {
            angleEnd = atof(token);
        }
        m_angleEnd = angleEnd;
    }

    /**
     * Offset/Address/Size: 0xE58 | 0x80207784 | size: 0xE8
     */
    virtual void ApplyModifier(glPoly2& poly, float time)
    {
        nlMatrix4 m4;
        nlMatrix3 m3;
        nlQuaternion quat;

        nlMakeQuat(quat, m_v3Axis, (3.1415927f * ((m_angleStart * (1.0f - time)) + (time * m_angleEnd))) / 180.0f);
        nlQuatToMatrix(m4, quat);

        m3.e[0] = m4.m11;
        m3.e[1] = m4.m12;
        m3.e[3] = m4.m21;
        m3.e[4] = m4.m22;
        m3.e[7] = 0.0f;
        m3.e[6] = 0.0f;
        m3.e[5] = 0.0f;
        m3.e[2] = 0.0f;
        m3.e[8] = 1.0f;

        for (int i = 0; i < 4; i++)
        {
            nlVector2 temp;
            nlMultVectorMatrix(temp, poly.m_pos[i], m3);
            poly.m_uv[i] = temp;
        }
    }

    /* 0x04 */ nlVector3 m_v3Axis;
    /* 0x10 */ float m_angleStart;
    /* 0x14 */ float m_angleEnd;
}; // total size: 0x18

class ScreenBlur : public TransitionModifierInterface
{
public:
    ScreenBlur()
    {
        m_fStartBlend = 1.0f;
        m_fEndBlend = 1.0f;
    }

    /**
     * Offset/Address/Size: 0xC04 | 0x80207530 | size: 0x5C
     */
    virtual ~ScreenBlur() { }

    /**
     * Offset/Address/Size: 0xC60 | 0x8020758C | size: 0x84
     */
    virtual void InitializeFromParser(SimpleParser* parser)
    {
        m_fStartBlend = parseFloat(parser->NextTokenOnLine(true), 1.0f);
        m_fEndBlend = parseFloat(parser->NextTokenOnLine(true), 1.0f);
    }

    /**
     * Offset/Address/Size: 0xCE4 | 0x80207610 | size: 0x4
     */
    virtual void ApplyModifier(glPoly2& poly, float time) { }

    /**
     * Offset/Address/Size: 0xCE8 | 0x80207614 | size: 0x4C
     */
    virtual void Cleanup()
    {
        glViewSetFilter(GLV_ScreenBlur, GLFilter_None);
        glViewSetFilterSource(GLV_ScreenBlur, GLTG_Main);
        glViewSetFilter(GLV_ScreenBlur2, GLFilter_None);
        glViewSetFilterSource(GLV_ScreenBlur2, GLTG_GlowConv);
    }

    /* 0x4 */ float m_fStartBlend;
    /* 0x8 */ float m_fEndBlend;
}; // total size: 0xC

class ScreenGrab : public TransitionModifierInterface
{
public:
    ScreenGrab()
    {
        m_bDoGrab = true;
        m_nTexture = glHash("target/backbuffer");
    }

    /**
     * Offset/Address/Size: 0xB04 | 0x80207430 | size: 0x5C
     */
    virtual ~ScreenGrab() { }

    /**
     * Offset/Address/Size: 0xB60 | 0x8020748C | size: 0x4
     */
    virtual void InitializeFromParser(SimpleParser* parser) { }

    /**
     * Offset/Address/Size: 0xB64 | 0x80207490 | size: 0x64
     */
    virtual void ApplyModifier(glPoly2& poly, float time)
    {
        if (m_bDoGrab)
        {
            glViewSetFilter(GLV_ScreenGrab, GLFilter_Blt);
            glViewSetFilterSource(GLV_ScreenGrab, GLTG_Main);
            m_bDoGrab = false;
        }
        else
        {
            glViewSetFilter(GLV_ScreenGrab, GLFilter_None);
        }
    }

    /**
     * Offset/Address/Size: 0xBC8 | 0x802074F4 | size: 0x3C
     */
    virtual void Cleanup()
    {
        glViewSetFilter(GLV_ScreenGrab, GLFilter_None);
        m_bDoGrab = true;
    }

    /* 0x4 */ bool m_bDoGrab;
    /* 0x8 */ u32 m_nTexture;
}; // total size: 0xC

class ToScreenCoordinates : public TransitionModifierInterface
{
public:
    ToScreenCoordinates()
    {
        float temp_f30 = 0.5f * glGetOrthographicWidth();
        float temp_f31 = 0.5f * glGetOrthographicHeight();
        float temp_f1 = -glGetScreenInfo()->PixelCentre;

        m_m3Position.SetIdentity();
        m_m3UV.SetIdentity();

        m_m3Position.m11 = temp_f30;
        m_m3Position.m22 = temp_f31;
        m_m3Position.m31 = temp_f30 - temp_f1;
        m_m3Position.m32 = temp_f31 - temp_f1;

        m_m3UV.m32 = 0.5f;
        m_m3UV.m31 = 0.5f;
        m_m3UV.m22 = 0.5f;
        m_m3UV.m11 = 0.5f;
    };

    /**
     * Offset/Address/Size: 0xA14 | 0x80207340 | size: 0x5C
     */
    virtual ~ToScreenCoordinates() { }

    /**
     * Offset/Address/Size: 0xA70 | 0x8020739C | size: 0x4
     */
    virtual void InitializeFromParser(SimpleParser* parser) { }

    /**
     * Offset/Address/Size: 0xA74 | 0x802073A0 | size: 0x90
     */
    virtual void ApplyModifier(glPoly2& poly, float time)
    {
        for (int i = 0; i < 4; i++)
        {
            nlVector2 temp;
            nlMultVectorMatrix(temp, poly.m_pos[i], m_m3Position);
            poly.m_pos[i] = temp;

            nlMultVectorMatrix(temp, poly.m_uv[i], m_m3UV);
            poly.m_uv[i] = temp;
        }
    }

    /* 0x04 */ nlMatrix3 m_m3Position;
    /* 0x28 */ nlMatrix3 m_m3UV;
}; // total size: 0x4C

}; // namespace TransitionModifiers

/**
 * Offset/Address/Size: 0x9A0 | 0x802072CC | size: 0x74
 */
ScriptedScreenTransition::ScriptedScreenTransition()
{
    m_pModifiers = nullptr;
    m_nModifiers = 0;
    m_fLength = 0.0f;
    m_fCurrentTime = 0.0f;
    m_nTexture = glHash("global/white");
    m_eTimeLine = TIME_LINEAR;
}

/**
 * Offset/Address/Size: 0x8D0 | 0x802071FC | size: 0xD0
 */
ScriptedScreenTransition::~ScriptedScreenTransition()
{
    if (m_pModifiers != nullptr)
    {
        for (int i = 0; i < m_nModifiers; i++)
        {
            delete m_pModifiers[i];
            m_pModifiers[i] = nullptr;
        }

        delete[] m_pModifiers;
        m_pModifiers = nullptr;
    }
}

/**
 * Offset/Address/Size: 0x8C0 | 0x802071EC | size: 0x10
 */
void ScriptedScreenTransition::Update(float dt)
{
    m_fCurrentTime += dt;
}

/**
 * Offset/Address/Size: 0x708 | 0x80207034 | size: 0x1B8
 */
void ScriptedScreenTransition::Render(eGLView view)
{
    glPoly2 poly;                                 // r1+0xC
    nlColour colour = { 0xFF, 0xFF, 0xFF, 0xFF }; // r1+0x8

    nlVec2Set(poly.m_pos[0], -1.0f, -1.0f);
    nlVec2Set(poly.m_pos[1], -1.0f, 1.0f);
    nlVec2Set(poly.m_pos[2], 1.0f, 1.0f);
    nlVec2Set(poly.m_pos[3], 1.0f, -1.0f);

    poly.depth = -1.0f;

    nlVec2Set(poly.m_uv[0], -1.0f, -1.0f);
    nlVec2Set(poly.m_uv[1], -1.0f, 1.0f);
    nlVec2Set(poly.m_uv[2], 1.0f, 1.0f);
    nlVec2Set(poly.m_uv[3], 1.0f, -1.0f);

    *(u32*)&poly.m_colour[0] = *(u32*)&colour; // sp4C (0x4C) -> offset 0x40
    *(u32*)&poly.m_colour[1] = *(u32*)&colour; // sp50 (0x50) -> offset 0x44
    *(u32*)&poly.m_colour[2] = *(u32*)&colour; // sp54 (0x54) -> offset 0x48
    *(u32*)&poly.m_colour[3] = *(u32*)&colour; // sp58 (0x58) -> offset 0x4C

    // *(u32*)&colour = 0xFFFFFFFF;
    float normalizedTime;
    if (m_fLength > 0.00001) // @532 = 0.0f
    {
        normalizedTime = m_fCurrentTime / m_fLength;
    }
    else
    {
        normalizedTime = 0.0f; // @333 = 0.0f
    }

    float finalTime;
    switch (m_eTimeLine)
    {
    case TIME_ACCEL:
        finalTime = normalizedTime * normalizedTime;
        break;
    case TIME_DECEL:
        finalTime = nlSqrt(normalizedTime, true);
        break;
    case TIME_LINEAR:
        finalTime = normalizedTime;
        break;
    default:
        finalTime = normalizedTime;
        break;
    }

    glSetDefaultState(false);
    glSetCurrentTexture(m_nTexture, GLTT_Diffuse);
    glSetTextureState(GLTS_DiffuseWrap, 3);
    glSetRasterState(GLS_AlphaBlend, 1);

    for (int i = 0; i < m_nModifiers; i++)
    {
        m_pModifiers[i]->ApplyModifier(poly, finalTime);
    }

    glSetCurrentRasterState(glHandleizeRasterState());
    glSetCurrentTextureState(glHandleizeTextureState());
    poly.Attach(view, 0, nullptr, 0xFFFFFFFF);
}

/**
 * Offset/Address/Size: 0x698 | 0x80206FC4 | size: 0x70
 */
void ScriptedScreenTransition::Cancel()
{
    for (int i = 0; i < m_nModifiers; i++)
    {
        m_pModifiers[i]->Cleanup();
    }
}

/**
 * Offset/Address/Size: 0x3B8 | 0x80206CE4 | size: 0x2E0
 */
void ScriptedScreenTransition::InitializeFromParser(SimpleParser* parser)
{
    TransitionModifierInterface* pModifiers[25]; // r1+0x8
    m_nModifiers = 0;

    for (char* pToken = parser->NextToken(true); pToken != nullptr; pToken = parser->NextToken(true))
    {
        if (nlStrCmp<char>(pToken, "end") == 0)
            break;

        if (nlStrCmp<char>(pToken, "length") == 0)
        {
            m_fLength = atof(parser->NextTokenOnLine(true));
        }
        else if (nlStrCmp<char>(pToken, "texture") == 0)
        {
            m_nTexture = glHash(parser->NextTokenOnLine(true));
        }
        else if (nlStrCmp<char>(pToken, "time") == 0)
        {
            char* temp_r3 = parser->NextTokenOnLine(true);
            if (nlStrCmp<char>(temp_r3, "linear") == 0)
            {
                m_eTimeLine = TIME_LINEAR;
            }
            else if (nlStrCmp<char>(temp_r3, "accelerate") == 0)
            {
                m_eTimeLine = TIME_ACCEL;
            }
            else if (nlStrCmp<char>(temp_r3, "decelarate") == 0)
            {
                m_eTimeLine = TIME_DECEL;
            }
        }
        else
        {
            TransitionModifierInterface* pModifier = GetModifierFromName(pToken);
            pModifier->InitializeFromParser(parser);
            pModifiers[m_nModifiers] = pModifier;
            m_nModifiers += 1;
        }
    }

    pModifiers[m_nModifiers] = new (nlMalloc(sizeof(TransitionModifiers::ToScreenCoordinates), 8, false)) TransitionModifiers::ToScreenCoordinates();
    m_nModifiers += 1;

    m_pModifiers = (TransitionModifierInterface**)nlMalloc(m_nModifiers * sizeof(TransitionModifierInterface*), 8, 0);
    memcpy(m_pModifiers, pModifiers, m_nModifiers * sizeof(TransitionModifierInterface*));
}

/**
 * Offset/Address/Size: 0x0 | 0x8020692C | size: 0x3B8
 */
TransitionModifierInterface* ScriptedScreenTransition::GetModifierFromName(char* pName)
{
    if (nlStrCmp<char>(pName, "modelscale") == 0)
    {
        return new (nlMalloc(sizeof(TransitionModifiers::ScaleModel), 8, false)) TransitionModifiers::ScaleModel();
    }

    if (nlStrCmp<char>(pName, "colour") == 0)
    {
        return new (nlMalloc(sizeof(TransitionModifiers::ColourBlend), 8, false)) TransitionModifiers::ColourBlend();
    }

    if (nlStrCmp<char>(pName, "texturescale") == 0)
    {
        return new (nlMalloc(sizeof(TransitionModifiers::ScaleTexture), 8, false)) TransitionModifiers::ScaleTexture();
    }

    if (nlStrCmp<char>(pName, "modelshift") == 0)
    {
        return new (nlMalloc(sizeof(TransitionModifiers::TranslateModel), 8, false)) TransitionModifiers::TranslateModel();
    }

    if (nlStrCmp<char>(pName, "textureshift") == 0)
    {
        return new (nlMalloc(sizeof(TransitionModifiers::TranslateTexture), 8, false)) TransitionModifiers::TranslateTexture();
    }
    if (nlStrCmp<char>(pName, "modelrotate") == 0)
    {
        return new (nlMalloc(sizeof(TransitionModifiers::RotateModel), 8, false)) TransitionModifiers::RotateModel();
    }

    if (nlStrCmp<char>(pName, "texturerotate") == 0)
    {
        return new (nlMalloc(sizeof(TransitionModifiers::RotateTexture), 8, false)) TransitionModifiers::RotateTexture();
    }

    if (nlStrCmp<char>(pName, "screenblur") == 0)
    {
        return new (nlMalloc(sizeof(TransitionModifiers::ScreenBlur), 8, false)) TransitionModifiers::ScreenBlur();
    }

    if (nlStrCmp<char>(pName, "screengrab") == 0)
    {
        return new (nlMalloc(sizeof(TransitionModifiers::ScreenGrab), 8, false)) TransitionModifiers::ScreenGrab();
    }
    return NULL;
}
