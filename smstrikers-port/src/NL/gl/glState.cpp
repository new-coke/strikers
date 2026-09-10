#include "NL/gl/gl.h"

#include "NL/gl/glState.h"
#include "NL/gl/glMatrix.h"
#include "NL/nlMemory.h"

enum eGLMaterialState
{
    GLMS_Glossiness = 0,
    GLMS_Num = 1
};

class glRasterState
{
public:
    unsigned long SetRasterState(eGLState, unsigned long);

    /* 0x00 */ unsigned long m_State;
}; // total size: 0x4

class glTextureState
{
public:
    unsigned long SetTextureState(eGLTextureState, unsigned long);
    unsigned long GetTextureState(eGLTextureState);

    /* 0x00 */ unsigned long long m_State;
}; // total size: 0x8

class glMaterialState
{
public:
    unsigned long SetMaterialState(eGLMaterialState, unsigned long);

    /* 0x00 */ unsigned long m_State;
}; // total size: 0x4

static glStateBundle _bundle;

static glRasterState _state;
static glTextureState _textureState;
static glMaterialState _materialState;

static unsigned long defaultRasterState = 0;
static unsigned long long defaultTextureState = 0;
static unsigned long defaultMaterialState = 0;

static gl_StateBitfield packed_raster[GLS_Num + 1] = { 0, 1, 0, 1, 0, 2, 0, 1, 0, 8, 0, 3, 0, 2, 0, 2, 0, 1, 0, 1, 0, 0 };
static gl_StateBitfield packed_texture[19] = { 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 6, 0, 6, 0, 6, 0, 6, 0, 6, 0, 6, 0, 0 };
static gl_StateBitfield packed_materials[GLMS_Num + 1] = { 0, 7, 0, 0 };

static inline uintptr_t glSetCurrentTextureInline(uintptr_t texture, eGLTextureType type)
{
    uintptr_t prev = _bundle.texture[type];
    unsigned long mask = 1u << type;

    if (texture == 0xFFFFFFFFu)
    {
        _bundle.texconfig = _bundle.texconfig & ~mask;
    }
    else
    {
        _bundle.texconfig = _bundle.texconfig | mask;
    }

    _bundle.texture[type] = texture;
    return prev;
}

/**
 * Offset/Address/Size: 0x0 | 0x801DBC44 | size: 0x184
 */
void glSetDefaultState(bool setRasterDefaults)
{
    _state.m_State = defaultRasterState;
    _textureState.m_State = defaultTextureState;

    if (setRasterDefaults)
    {
        s32 depthTestBit = packed_raster[0].startBit;
        s32 depthTestNumBits = packed_raster[0].numBits;
        s32 i;

        for (i = depthTestNumBits; i > 0; i--)
        {
        }

        for (i = 0; i < depthTestNumBits; i++)
        {
            if ((1u << i) & 1u)
            {
                _state.m_State = _state.m_State | (1u << depthTestBit);
            }
            else
            {
                _state.m_State = _state.m_State & ~(1u << depthTestBit);
            }
            depthTestBit++;
        }

        {
            s32 depthWriteBit = packed_raster[1].startBit;
            s32 depthWriteNumBits = packed_raster[1].numBits;

            for (i = depthWriteNumBits; i > 0; i--)
            {
            }

            for (i = 0; i < depthWriteNumBits; i++)
            {
                if ((1u << i) & 1u)
                {
                    _state.m_State = _state.m_State | (1u << depthWriteBit);
                }
                else
                {
                    _state.m_State = _state.m_State & ~(1u << depthWriteBit);
                }
                depthWriteBit++;
            }
        }
    }

    _bundle.raster = _state.m_State;
    _bundle.texturestate = _textureState.m_State;
    _bundle.program = (u32)-1;
    _bundle.matrix = glGetIdentityMatrix();

    glSetCurrentTextureInline((u32)-1, GLTT_Diffuse);
    glSetCurrentTextureInline((u32)-1, GLTT_Detail);
    glSetCurrentTextureInline((u32)-1, GLTT_Shadow);
    glSetCurrentTextureInline((u32)-1, GLTT_SelfIllum);
    glSetCurrentTextureInline((u32)-1, GLTT_Gloss);
    glSetCurrentTextureInline((u32)-1, GLTT_BumpLocal);
}

/**
 * Offset/Address/Size: 0x184 | 0x801DBDC8 | size: 0xC
 */
void glUnHandleizeTextureState(unsigned long long state)
{
    _textureState.m_State = state;
}

/**
 * Offset/Address/Size: 0x190 | 0x801DBDD4 | size: 0xC
 */
unsigned long long glHandleizeTextureState()
{
    return _textureState.m_State;
}

/**
 * Offset/Address/Size: 0x19C | 0x801DBDE0 | size: 0x8
 */
void glUnHandleizeRasterState(unsigned long state)
{
    _state.m_State = state;
}

/**
 * Offset/Address/Size: 0x1A4 | 0x801DBDE8 | size: 0x8
 */
unsigned long glHandleizeRasterState()
{
    return _state.m_State;
}

/**
 * Offset/Address/Size: 0x1AC | 0x801DBDF0 | size: 0x14
 */
void glSetTextureStateDefaults()
{
    _textureState.m_State = defaultTextureState;
}

/**
 * Offset/Address/Size: 0x1C0 | 0x801DBE04 | size: 0xC
 */
void glSetRasterStateDefaults()
{
    _state.m_State = defaultRasterState;
}

static inline unsigned long SetTextureStateRefImpl(unsigned long long* pState, eGLTextureState state, unsigned long value)
{
    s32* pNumBits;
    gl_StateBitfield* pInfo = &packed_texture[state];
    s32 numBits;
    s32 j;
    unsigned long long tex = *pState;
    s32 cnt;
    unsigned long out = 0;
    unsigned long long cmp = (unsigned long long)out;
    unsigned long one = 1;
    pNumBits = &pInfo->numBits;
    numBits = *pNumBits;
    cnt = (s32)out;

    for (; cnt < numBits; cnt++)
    {
        unsigned long long mask = 1ULL << (cnt + pInfo->startBit);
        if ((tex & mask) != cmp)
        {
            out |= (one << cnt);
        }
    }

    for (j = 0; j < *pNumBits; j++)
    {
        if (value & (1 << j))
        {
            unsigned long long mask = 1ULL << (j + pInfo->startBit);
            unsigned long hi = ((u32*)pState)[0];
            unsigned long lo = ((u32*)pState)[1];
            lo = lo | (u32)mask;
            ((u32*)pState)[1] = lo;
            lo = hi | (u32)(mask >> 32);
            ((u32*)pState)[0] = lo;
        }
        else
        {
            *pState &= ~(1 << (j + pInfo->startBit));
        }
    }

    return out;
}

/**
 * Offset/Address/Size: 0x1CC | 0x801DBE10 | size: 0x12C
 */
unsigned long glSetTextureState(unsigned long long& texture, eGLTextureState state, unsigned long value)
{
    return SetTextureStateRefImpl(&texture, state, value);
}

static inline unsigned long SetTextureStateImpl(
    glTextureState& textureState,
    gl_StateBitfield* pInfo,
    const unsigned long& value)
{
    s32 numBits = pInfo->numBits;
    s32 bit = pInfo->startBit;
    s32 j;
    unsigned long long tex = textureState.m_State;
    s32 cnt;
    unsigned long out = 0;
    unsigned long long cmp = (unsigned long long)out;
    unsigned long one = 1;
    cnt = (s32)out;

    for (; cnt < numBits; cnt++)
    {
        unsigned long long mask = 1ULL << (cnt + bit);
        if ((tex & mask) != cmp)
        {
            out |= (one << cnt);
        }
    }

    for (j = 0; j < numBits; bit++, j++)
    {
        if (value & (1 << j))
        {
            unsigned long long mask = 1ULL << bit;
            textureState.m_State = textureState.m_State | mask;
        }
        else
        {
            textureState.m_State &= ~(1 << bit);
        }
    }

    return out;
}

static inline unsigned long SetTextureStateImpl(
    glTextureState& textureState,
    eGLTextureState state,
    unsigned long value)
{
    gl_StateBitfield* pInfo = &packed_texture[state];
    return SetTextureStateImpl(textureState, pInfo, value);
}

static inline gl_StateBitfield* GetTextureStateAndInfo(
    glTextureState& textureState,
    eGLTextureState state,
    unsigned long& out)
{
    gl_StateBitfield* pInfo = &packed_texture[state];
    s32 numBits = pInfo->numBits;
    unsigned long long texture = textureState.m_State;
    out = 0;
    s32 cnt = (s32)out;
    unsigned long one = 1;
    unsigned long long cmp = (unsigned long long)out;

    for (; cnt < numBits; cnt++)
    {
        unsigned long long mask = 1ULL << (cnt + pInfo->startBit);
        if ((texture & mask) != cmp)
        {
            out |= (one << cnt);
        }
    }

    return pInfo;
}

unsigned long glTextureState::GetTextureState(
    eGLTextureState state)
{
    unsigned long out;
    GetTextureStateAndInfo(*this, state, out);
    return out;
}

static inline unsigned long SetTextureStateInline(glTextureState& textureState, eGLTextureState state, unsigned long value)
{
    unsigned long out;
    gl_StateBitfield* pInfo = GetTextureStateAndInfo(textureState, state, out);
    s32 numBits = pInfo->numBits;

    for (s32 cnt = 0; cnt < numBits; cnt++)
    {
        if (value & (1 << cnt))
        {
            textureState.m_State |= (1ULL << (cnt + pInfo->startBit));
        }
        else
        {
            textureState.m_State &= ~(1 << (cnt + pInfo->startBit));
        }
    }

    return out;
}

/**
 * Offset/Address/Size: 0x2F8 | 0x801DBF3C | size: 0x118
 */
unsigned long glSetTextureState(eGLTextureState state, unsigned long value)
{
    return SetTextureStateInline(_textureState, state, value);
}

static inline unsigned long GetTextureStateImpl(unsigned long long* pTexture, eGLTextureState texturestate)
{
    gl_StateBitfield* pInfo = &packed_texture[texturestate];
    s32 numBits = pInfo->numBits;
    unsigned long long texture = *pTexture;
    s32 cnt = 0;
    unsigned long out = 0;
    cnt = (s32)out;
    unsigned long one = 1;
    unsigned long long cmp = (unsigned long long)out;

    for (; cnt < numBits; cnt++)
    {
        unsigned long long mask = 1ULL << (cnt + pInfo->startBit);
        if ((texture & mask) != cmp)
        {
            out |= (one << cnt);
        }
    }

    return out;
}

/**
 * Offset/Address/Size: 0x410 | 0x801DC054 | size: 0x9C
 */
unsigned long glGetTextureState(unsigned long long texture, eGLTextureState texturestate)
{
    return GetTextureStateImpl(&texture, texturestate);
}

/**
 * Offset/Address/Size: 0x4AC | 0x801DC0F0 | size: 0x9C
 */
unsigned long glGetTextureState(eGLTextureState texturestate)
{
    return GetTextureStateImpl(&_textureState.m_State, texturestate);
}

static inline unsigned long glExtractRasterBits(unsigned long raster, gl_StateBitfield* p, s32* pn)
{
    unsigned long out = 0;
    for (s32 i = 0; i < *pn; i++)
    {
        if (raster & (1u << (i + p->startBit)))
        {
            out |= (1u << i);
        }
    }
    return out;
}

/**
 * Offset/Address/Size: 0x548 | 0x801DC18C | size: 0xBC
 */
u32 glSetRasterState(u32& raster, eGLState state, unsigned long value)
{
    gl_StateBitfield* p = &packed_raster[state];
    s32* pn = &p->numBits;
    unsigned long out = glExtractRasterBits(raster, p, pn);

    for (s32 i = 0; i < *pn; i++)
    {
        if (value & (1u << i))
        {
            raster |= (1u << (i + p->startBit));
        }
        else
        {
            raster &= ~(1u << (i + p->startBit));
        }
    }

    return out;
}

static inline unsigned long extractStateBits(unsigned long rasterState, gl_StateBitfield* p, s32 numBits)
{
    unsigned long out = 0;
    for (s32 i = 0; i < numBits; i++)
    {
        if (rasterState & (1u << (i + p->startBit)))
        {
            out |= (1u << i);
        }
    }
    return out;
}

static inline unsigned long getRasterState()
{
    return _state.m_State;
}

/**
 * Offset/Address/Size: 0x604 | 0x801DC248 | size: 0xB4
 */
u32 glSetRasterState(eGLState state, unsigned long value)
{
    gl_StateBitfield* p = &packed_raster[state];
    s32 numBits = p->numBits;
    unsigned long out = extractStateBits(getRasterState(), p, numBits);

    for (s32 i = 0; i < numBits; i++)
    {
        if (value & (1u << i))
        {
            _state.m_State = _state.m_State | (1u << (i + p->startBit));
        }
        else
        {
            _state.m_State = _state.m_State & ~(1u << (i + p->startBit));
        }
    }

    return out;
}

/**
 * Offset/Address/Size: 0x6B8 | 0x801DC2FC | size: 0x58
 */
u32 glGetRasterState(unsigned long raster, eGLState state)
{
    gl_StateBitfield* p = &packed_raster[state];
    s32* pn = &p->numBits;
    return glExtractRasterBits(raster, p, pn);
}

/**
 * Offset/Address/Size: 0x710 | 0x801DC354 | size: 0x5C
 */
u32 glGetRasterState(eGLState state)
{
    gl_StateBitfield* p = &packed_raster[state];
    s32* pn = &p->numBits;
    return glExtractRasterBits(_state.m_State, p, pn);
}

/**
 * Offset/Address/Size: 0x76C | 0x801DC3B0 | size: 0x30
 */
u32 glGetTexture(const char* textureName)
{
    if (textureName == 0)
        return -1;

    return glHash(textureName);
}

/**
 * Offset/Address/Size: 0x79C | 0x801DC3E0 | size: 0x30
 */
u32 glGetProgram(const char* programName)
{
    if (programName == NULL)
    {
        return -1;
    }
    return glHash(programName);
}

/**
 * Offset/Address/Size: 0x7CC | 0x801DC410 | size: 0x18
 */
uintptr_t glSetCurrentMatrix(uintptr_t matrix)
{
    uintptr_t prev = _bundle.matrix;
    _bundle.matrix = matrix;
    return prev;
}

/**
 * Offset/Address/Size: 0x7E4 | 0x801DC428 | size: 0x20
 */
u64 glSetCurrentTextureState(unsigned long long state)
{
    unsigned long long prev = _bundle.texturestate;
    _bundle.texturestate = state;
    return prev;
}

/**
 * Offset/Address/Size: 0x804 | 0x801DC448 | size: 0x14
 */
u64 glGetCurrentTextureState()
{
    return _bundle.texturestate;
}

/**
 * Offset/Address/Size: 0x818 | 0x801DC45C | size: 0x18
 */
u32 glSetCurrentRasterState(unsigned long raster)
{
    u32 prev = _bundle.raster;
    _bundle.raster = raster;
    return prev;
}

/**
 * Offset/Address/Size: 0x830 | 0x801DC474 | size: 0x10
 */
u32 glGetCurrentRasterState()
{
    return _bundle.raster;
}

/**
 * Offset/Address/Size: 0x840 | 0x801DC484 | size: 0x18
 */
u32 glSetCurrentProgram(unsigned long program)
{
    u32 prev = _bundle.program;
    _bundle.program = program;
    return prev;
}

static inline uintptr_t GetCurrentTextureImpl(eGLTextureType type)
{
    return _bundle.texture[type];
}

/**
 * Offset/Address/Size: 0x858 | 0x801DC49C | size: 0x50
 */
uintptr_t glSetCurrentTexture(uintptr_t texture, eGLTextureType type)
{
    uintptr_t prev = GetCurrentTextureImpl(type);
    unsigned long mask = 1u << type;

    if (texture == 0xFFFFFFFFu)
    {
        _bundle.texconfig = _bundle.texconfig & ~mask;
    }
    else
    {
        _bundle.texconfig = _bundle.texconfig | mask;
    }

    _bundle.texture[type] = texture;
    return prev;
}

/**
 * Offset/Address/Size: 0x8A8 | 0x801DC4EC | size: 0x30
 */
void glStateRestore(const glStateBundle& state)
{
    memcpy(&_bundle, &state, sizeof(glStateBundle));
}

/**
 * Offset/Address/Size: 0x8D8 | 0x801DC51C | size: 0x2C
 */
void glStateSave(glStateBundle& state)
{
    memcpy(&state, &_bundle, sizeof(glStateBundle));
}

/**
 * Offset/Address/Size: 0x904 | 0x801DC548 | size: 0xC
 */
glStateBundle* gl_GetCurrentStateBundle()
{
    return &_bundle;
}

unsigned long glMaterialState::SetMaterialState(eGLMaterialState state, unsigned long value)
{
    gl_StateBitfield* p = &packed_materials[state];
    s32 startBit = p->startBit;
    s32 numBits = p->numBits;
    unsigned long oldState = m_State;
    unsigned long out = 0;
    for (u32 cnt = 0; (s32)cnt < numBits; cnt++)
    {
        if (oldState & (1u << (cnt + startBit)))
            out |= (1u << cnt);
    }
    numBits = p->numBits;
    for (u32 cnt = 0; (s32)cnt < numBits; cnt++)
    {
        if (value & (1u << cnt))
            m_State = m_State | (1u << startBit);
        else
            m_State = m_State & ~(1u << startBit);
        startBit++;
    }
    return out;
}

unsigned long glRasterState::SetRasterState(eGLState state, unsigned long value)
{
    gl_StateBitfield* p = &packed_raster[state];
    s32 startBit = p->startBit;
    s32 numBits = p->numBits;
    unsigned long oldState = m_State;
    unsigned long out = 0;
    for (u32 cnt = 0; (s32)cnt < numBits; cnt++)
    {
        if (oldState & (1u << (cnt + startBit)))
            out |= (1u << cnt);
    }
    numBits = p->numBits;
    for (u32 cnt = 0; (s32)cnt < numBits; cnt++)
    {
        if (value & (1u << cnt))
            m_State = m_State | (1u << (cnt + startBit));
        else
            m_State = m_State & ~(1u << (cnt + startBit));
    }
    return out;
}

unsigned long glTextureState::SetTextureState(eGLTextureState state, unsigned long value)
{
    gl_StateBitfield* p = &packed_texture[state];
    s32 startBit = p->startBit;
    s32 numBits = p->numBits;
    unsigned long long oldState = m_State;
    unsigned long out = 0;
    for (u32 cnt = 0; (s32)cnt < numBits; cnt++)
    {
        if (oldState & (1ULL << (cnt + startBit)))
            out |= (1u << cnt);
    }
    numBits = p->numBits;
    for (u32 cnt = 0; (s32)cnt < numBits; cnt++)
    {
        if (value & (1u << cnt))
            m_State = m_State | (1ULL << (cnt + startBit));
        else
            m_State = m_State & ~(1 << (cnt + startBit));
    }
    return out;
}

/**
 * Offset/Address/Size: 0x910 | 0x801DC554 | size: 0x1E78
 */
void gl_StateStartup()
{
    s32 start;
    gl_StateBitfield* p;

    _bundle.texconfig = 0;

    start = 0;
    p = &packed_raster[GLS_DepthTest];
    while (p->numBits != 0)
    {
        p->startBit = start;
        start += p->numBits;
        p++;
    }

    start = 0;
    p = &packed_texture[GLTS_DiffuseWrap];
    while (p->numBits != 0)
    {
        p->startBit = start;
        start += p->numBits;
        p++;
    }

    start = 0;
    p = &packed_materials[GLMS_Glossiness];
    while (p->numBits != 0)
    {
        p->startBit = start;
        start += p->numBits;
        p++;
    }

    _state.SetRasterState(GLS_DepthTest, 0);
    _state.SetRasterState(GLS_DepthWrite, 0);
    _state.SetRasterState(GLS_DepthFunc, 1);
    _state.SetRasterState(GLS_AlphaTest, 0);
    _state.SetRasterState(GLS_AlphaTestRef, 0);
    _state.SetRasterState(GLS_AlphaBlend, 0);
    _state.SetRasterState(GLS_Culling, 1);
    _state.SetRasterState(GLS_ColourWrite, 3);
    _state.SetRasterState(GLS_SolidOffset, 0);
    _state.SetRasterState(GLS_FillMode, 0);

    defaultRasterState = _state.m_State;

    _textureState.SetTextureState(GLTS_DiffuseWrap, 0);
    _textureState.SetTextureState(GLTS_DetailWrap, 0);
    _textureState.SetTextureState(GLTS_ShadowWrap, 0);
    _textureState.SetTextureState(GLTS_SelfIllumWrap, 0);
    _textureState.SetTextureState(GLTS_GlossWrap, 0);
    _textureState.SetTextureState(GLTS_BumpLocalWrap, 0);
    _textureState.SetTextureState(GLTS_DiffuseFilter, 0);
    _textureState.SetTextureState(GLTS_DetailFilter, 0);
    _textureState.SetTextureState(GLTS_ShadowFilter, 0);
    _textureState.SetTextureState(GLTS_SelfIllumFilter, 0);
    _textureState.SetTextureState(GLTS_GlossFilter, 0);
    _textureState.SetTextureState(GLTS_BumpLocalFilter, 0);

    SetTextureStateImpl(_textureState, GLTS_DiffuseLevel, 63);
    SetTextureStateImpl(_textureState, GLTS_DetailLevel, 0);
    SetTextureStateImpl(_textureState, GLTS_ShadowLevel, 63);
    SetTextureStateImpl(_textureState, GLTS_SelfIllumLevel, 63);
    SetTextureStateImpl(_textureState, GLTS_GlossLevel, 63);
    SetTextureStateImpl(_textureState, GLTS_BumpLocalLevel, 63);

    defaultTextureState = _textureState.m_State;

    _materialState.SetMaterialState(GLMS_Glossiness, 1);

    defaultMaterialState = _materialState.m_State;
}
