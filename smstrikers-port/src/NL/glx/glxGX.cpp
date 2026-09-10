#include "NL/glx/glxGX.h"
#include "dolphin/gx.h"

GXTevAlphaArg gx_alphaArg[2][32];

static bool gx_dither;
static bool gx_colourupdate;
static bool gx_alphaupdate;
static bool gx_zcomploc;
static bool gx_ztest;
static bool save_ztest;
static GXCompare gx_zfunc;
static GXCompare save_zfunc;
static bool gx_zwrite;
static bool save_zwrite;
static s32 /*GXCompare*/ gx_alphafunc;
static u8 gx_alpharef;
static u8 /*GXBlendMode*/ gx_blend;
static u8 /*GXLogicOp*/ gx_blendSubtract;
static GXBlendFactor gx_srcfactor;
static GXBlendFactor gx_dstfactor;
static GXCullMode gx_cullmode;
static u32 gx_currentmtx;
static u32 gx_numChans;
static u32 gx_numTEV;
static u32 gx_numGens;
static nlColour gx_matColour[2];
static nlColour gx_ambColour[2];
static bool gx_coplanar = false;

/**
 * Offset/Address/Size: 0x0 | 0x801C14E8 | size: 0x48
 */
bool gxSetCoPlanar(bool coplanar)
{
    bool prev = gx_coplanar;
    if ((u8)gx_coplanar != coplanar)
    {
        gx_coplanar = coplanar;
        GXSetCoPlanar(coplanar);
    }
    return prev;
}

static inline void SetGXChanAmbColour(int chan, const nlColour& color)
{
    nlColour c;
    c = color;
    GXSetChanAmbColor((GXChannelID)chan, *(GXColor*)&c);
}

/**
 * Offset/Address/Size: 0x48 | 0x801C1530 | size: 0x54
 */
nlColour gxSetChanAmbColour(int chan, const nlColour& color)
{
    nlColour* pAmb = &gx_ambColour[chan];
    nlColour prev = *pAmb;
    if (prev != color)
    {
        *(volatile u32*)pAmb = *(volatile u32*)&color;
        SetGXChanAmbColour(chan, color);
    }
    return prev;
}

static inline void SetGXChanMatColour(int chan, const nlColour& color)
{
    nlColour c;
    c = color;
    GXSetChanMatColor((GXChannelID)chan, *(GXColor*)&c);
}

/**
 * Offset/Address/Size: 0x9C | 0x801C1584 | size: 0x54
 */
nlColour gxSetChanMatColour(int chan, const nlColour& color)
{
    nlColour* pMat = &gx_matColour[chan];
    nlColour prev = *pMat;
    if (prev != color)
    {
        *(volatile u32*)pMat = *(volatile u32*)&color;
        SetGXChanMatColour(chan, color);
    }
    return prev;
}

/**
 * Offset/Address/Size: 0xF0 | 0x801C15D8 | size: 0x28
 */
void gxSetTexCoordGen(int dst_coord, _GXTexGenType func, _GXTexGenSrc src_param, unsigned long arg)
{
    GXSetTexCoordGen2((GXTexCoordID)dst_coord, func, src_param, arg, false, 125);
}

/**
 * Offset/Address/Size: 0x118 | 0x801C1600 | size: 0x30
 */
void gxSetTevAlphaOp(int stage, _GXTevOp op, _GXTevBias bias, _GXTevScale scale, bool clamp, _GXTevRegID out_reg)
{
    GXSetTevAlphaOp((GXTevStageID)stage, op, bias, scale, (u32)(-clamp | clamp) >> 0x1FU, out_reg);
}

/**
 * Offset/Address/Size: 0x148 | 0x801C1630 | size: 0x30
 */
void gxSetTevColourOp(int stage, _GXTevOp op, _GXTevBias bias, _GXTevScale scale, bool clamp, _GXTevRegID out_reg)
{
    GXSetTevColorOp((GXTevStageID)stage, op, bias, scale, (u32)(-clamp | clamp) >> 0x1FU, out_reg);
}

/**
 * Offset/Address/Size: 0x178 | 0x801C1660 | size: 0x50
 */
u32 gxSetNumTexGens(unsigned long numGens)
{
    u32 prev = gx_numGens;
    if (numGens != gx_numGens)
    {
        GXSetNumTexGens(numGens);
        gx_numGens = numGens;
    }
    return prev;
}

/**
 * Offset/Address/Size: 0x1C8 | 0x801C16B0 | size: 0x50
 */
u32 gxSetNumTevStages(unsigned long numTEV)
{
    u32 prev = gx_numTEV;
    if (numTEV != gx_numTEV)
    {
        GXSetNumTevStages(numTEV);
        gx_numTEV = numTEV;
    }
    return prev;
}

/**
 * Offset/Address/Size: 0x218 | 0x801C1700 | size: 0x50
 */
u32 gxSetNumChans(unsigned long numChans)
{
    u32 prev = gx_numChans;
    if (numChans != gx_numChans)
    {
        GXSetNumChans(numChans);
        gx_numChans = numChans;
    }
    return prev;
}

/**
 * Offset/Address/Size: 0x268 | 0x801C1750 | size: 0x48
 */
_GXCullMode gxSetCullMode(_GXCullMode mode)
{
    _GXCullMode prev = gx_cullmode;
    if (mode != prev)
    {
        GXSetCullMode(mode);
        gx_cullmode = mode;
    }
    return prev;
}

/**
 * Offset/Address/Size: 0x2B0 | 0x801C1798 | size: 0xDC
 */
void gxSetBlendMode(bool bBlend, _GXBlendFactor src_factor, _GXBlendFactor dst_factor, bool bSubtract)
{
    if ((bBlend != (u8)gx_blend) || (src_factor != (s32)gx_srcfactor) || (dst_factor != (s32)gx_dstfactor) || (bSubtract != (u8)gx_blendSubtract))
    {
        if ((bSubtract != 0) && (bBlend != 0))
        {
            GXSetBlendMode(GX_BM_SUBTRACT, src_factor, dst_factor, GX_LO_CLEAR);
        }
        else
        {
            GXSetBlendMode((GXBlendMode)((u32)(-(s32)bBlend | bBlend) >> 0x1FU), src_factor, dst_factor, GX_LO_CLEAR);
        }
        gx_blend = (GXBlendMode)bBlend;
        gx_blendSubtract = (GXLogicOp)bSubtract;
        gx_srcfactor = src_factor;
        gx_dstfactor = dst_factor;
    }
}

/**
 * Offset/Address/Size: 0x38C | 0x801C1874 | size: 0x70
 */
void gxSetAlphaCompare(_GXCompare func, u8 ref)
{
    if (gx_alphafunc != func || gx_alpharef != ref)
    {
        GXSetAlphaCompare(func, ref, GX_AOP_AND, func, ref);
        gx_alphafunc = func;
        gx_alpharef = ref;
    }
}

/**
 * Offset/Address/Size: 0x3FC | 0x801C18E4 | size: 0x40
 */
void gxRestoreZMode()
{
    gx_ztest = save_ztest;
    gx_zfunc = save_zfunc;
    gx_zwrite = save_zwrite;
    GXSetZMode((GXBool)gx_ztest, gx_zfunc, (GXBool)gx_zwrite);
}

/**
 * Offset/Address/Size: 0x43C | 0x801C1924 | size: 0x1C
 */
void gxSaveZMode()
{
    save_ztest = gx_ztest;
    save_zfunc = gx_zfunc;
    save_zwrite = gx_zwrite;
}

/**
 * Offset/Address/Size: 0x458 | 0x801C1940 | size: 0x88
 */
void gxSetZMode(bool bTest, _GXCompare func, bool bWrite)
{
    if ((bTest != (u8)gx_ztest) || (func != (s32)gx_zfunc) || (bWrite != (u8)gx_zwrite))
    {
        GXSetZMode(bTest, func, bWrite);
        gx_ztest = bTest;
        gx_zfunc = func;
        gx_zwrite = bWrite;
    }
}

/**
 * Offset/Address/Size: 0x4E0 | 0x801C19C8 | size: 0x5C
 */
bool gxSetZCompLoc(bool bBefore)
{
    bool bPrev = gx_zcomploc;
    if (bBefore != (u8)gx_zcomploc)
    {
        GXSetZCompLoc((u32)(-(s32)bBefore | bBefore) >> 0x1FU);
        gx_zcomploc = bBefore;
    }
    return bPrev;
}

/**
 * Offset/Address/Size: 0x53C | 0x801C1A24 | size: 0x5C
 */
bool gxSetAlphaUpdate(bool bOn)
{
    bool bPrev = gx_alphaupdate;
    if (bOn != gx_alphaupdate)
    {
        GXSetAlphaUpdate(bOn != 0);
        gx_alphaupdate = bOn;
    }
    return bPrev;
}

/**
 * Offset/Address/Size: 0x598 | 0x801C1A80 | size: 0x5C
 */
bool gxSetColourUpdate(bool bOn)
{
    bool bPrev = gx_colourupdate;
    if (bOn != gx_colourupdate)
    {
        GXSetColorUpdate(bOn != 0);
        gx_colourupdate = bOn;
    }
    return bPrev;
}

/**
 * Offset/Address/Size: 0x5F4 | 0x801C1ADC | size: 0x5C
 */
uint gxSetDither(bool dither)
{
    u8 cur = gx_dither;
    uint prev = cur;
    if (dither != cur)
    {
        GXSetDither(dither != 0);
        gx_dither = dither;
    }
    return prev;
}

/**
 * Offset/Address/Size: 0x650 | 0x801C1B38 | size: 0x58
 */
uint gxSetTevAlphaIn(int stage, int component, _GXTevAlphaArg arg)
{
    s32 componentOffset = component * 4;
    u8* pStageArgs = ((u8*)&gx_alphaArg) + (stage * 0x10);
    s32 prev = *(GXTevAlphaArg*)(pStageArgs + componentOffset);
    *(GXTevAlphaArg*)(pStageArgs + componentOffset) = arg;
    GXSetTevAlphaIn((GXTevStageID)stage, ((GXTevAlphaArg*)pStageArgs)[0], ((GXTevAlphaArg*)pStageArgs)[1], ((GXTevAlphaArg*)pStageArgs)[2], ((GXTevAlphaArg*)pStageArgs)[3]);
    return (uint)prev;
}

/**
 * Offset/Address/Size: 0x6A8 | 0x801C1B90 | size: 0x5C
 */
void gxSetTevAlphaIn(int stage, _GXTevAlphaArg a, _GXTevAlphaArg b, _GXTevAlphaArg c, _GXTevAlphaArg d)
{
    GXSetTevAlphaIn((GXTevStageID)stage, a, b, c, d);
    u32 idx = stage * 4;
    gx_alphaArg[0][idx] = a;
    gx_alphaArg[0][idx + 1] = b;
    gx_alphaArg[0][idx + 2] = c;
    gx_alphaArg[0][idx + 3] = d;
}

/**
 * Offset/Address/Size: 0x704 | 0x801C1BEC | size: 0x270
 */
void gxInit()
{
    for (int i = 0; i < 2; i++)
    {
        gx_alphaArg[i][0] = GX_CA_A0;
        gx_alphaArg[i][1] = GX_CA_A0;
        gx_alphaArg[i][2] = GX_CA_A0;
        gx_alphaArg[i][3] = GX_CA_A0;
        gx_alphaArg[i][4] = GX_CA_A0;
        gx_alphaArg[i][5] = GX_CA_A0;
        gx_alphaArg[i][6] = GX_CA_A0;
        gx_alphaArg[i][7] = GX_CA_A0;
        gx_alphaArg[i][8] = GX_CA_A0;
        gx_alphaArg[i][9] = GX_CA_A0;
        gx_alphaArg[i][10] = GX_CA_A0;
        gx_alphaArg[i][11] = GX_CA_A0;
        gx_alphaArg[i][12] = GX_CA_A0;
        gx_alphaArg[i][13] = GX_CA_A0;
        gx_alphaArg[i][14] = GX_CA_A0;
        gx_alphaArg[i][15] = GX_CA_A0;
        gx_alphaArg[i][16] = GX_CA_A0;
        gx_alphaArg[i][17] = GX_CA_A0;
        gx_alphaArg[i][18] = GX_CA_A0;
        gx_alphaArg[i][19] = GX_CA_A0;
        gx_alphaArg[i][20] = GX_CA_A0;
        gx_alphaArg[i][21] = GX_CA_A0;
        gx_alphaArg[i][22] = GX_CA_A0;
        gx_alphaArg[i][23] = GX_CA_A0;
        gx_alphaArg[i][24] = GX_CA_A0;
        gx_alphaArg[i][25] = GX_CA_A0;
        gx_alphaArg[i][26] = GX_CA_A0;
        gx_alphaArg[i][27] = GX_CA_A0;
        gx_alphaArg[i][28] = GX_CA_A0;
        gx_alphaArg[i][29] = GX_CA_A0;
        gx_alphaArg[i][30] = GX_CA_A0;
        gx_alphaArg[i][31] = GX_CA_A0;
    }

    GXSetDither(1);
    gx_dither = 1;
    GXSetColorUpdate(1);
    gx_colourupdate = 1;
    GXSetAlphaUpdate(1);
    gx_alphaupdate = 1;
    GXSetZCompLoc(1);
    gx_zcomploc = 1;

    GXSetZMode(true, GX_LEQUAL, true);
    gx_ztest = true;
    gx_zfunc = GX_LEQUAL;
    gx_zwrite = true;

    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    gx_alphafunc = GX_ALWAYS;
    gx_alpharef = 0;

    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    gx_blend = GX_BM_NONE;
    gx_blendSubtract = GX_LO_CLEAR;
    gx_srcfactor = GX_BL_ONE;
    gx_dstfactor = GX_BL_ZERO;

    GXSetCullMode(GX_CULL_BACK);
    gx_cullmode = GX_CULL_BACK;

    GXSetCurrentMtx(0);
    gx_currentmtx = 0;

    GXSetNumChans(1);
    gx_numChans = 1;

    GXSetNumTevStages(1);
    gx_numTEV = 1;

    GXSetNumTexGens(0);
    gx_numGens = 0;

    // White color (RGBA 255,255,255,255)
    GXColor white = { 255, 255, 255, 255 };
    // Black with full alpha (RGBA 0,0,0,255)
    GXColor black = { 0, 0, 0, 255 };

    GXSetChanMatColor(GX_COLOR0, white);
    GXSetChanMatColor(GX_COLOR1, white);
    GXSetChanAmbColor(GX_COLOR0, black);
    GXSetChanAmbColor(GX_COLOR1, black);

    gx_matColour[0].c[0] = 255;
    gx_matColour[0].c[1] = 255;
    gx_matColour[0].c[2] = 255;
    gx_matColour[0].c[3] = 255;
    gx_matColour[1] = gx_matColour[0];

    gx_ambColour[0].c[0] = 0;
    gx_ambColour[0].c[1] = 0;
    gx_ambColour[0].c[2] = 0;
    gx_ambColour[0].c[3] = 255;
    gx_ambColour[1] = gx_ambColour[0];

    GXSetCoPlanar(false);
    gx_coplanar = false;
}

extern "C" void gxProbeChanState(unsigned int amb[2], unsigned int mat[2],
                                 unsigned int* numChans)
{
    for (int i = 0; i < 2; i++)
    {
        amb[i] = *(const u32*)&gx_ambColour[i];
        mat[i] = *(const u32*)&gx_matColour[i];
    }
    *numChans = gx_numChans;
}
