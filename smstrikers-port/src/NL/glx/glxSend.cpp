#include "NL/glx/glxSend.h"
#include "port/texfilter.h"
#include "port/aspect.h"
#include "port/probeobj.h"
#include "dolphin/os.h"
#include "port/endian.h"
#include <stdlib.h>
extern "C" int port_region_owns(const void*);  // src/platform/memalloc.cpp

#include "dolphin/gx/GXGeometry.h"
#include "dolphin/gx/GXLighting.h"
#include "dolphin/gx/GXEnum.h"
#include "dolphin/gx/GXTev.h"
#include "dolphin/gx/GXTransform.h"
#include "dolphin/mtx.h"
#include "NL/gl/glConstant.h"
#include "NL/gl/glLightUserData.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glPlat.h"
#include "NL/gl/gl.h"
#include "NL/gl/glState.h"
#include "NL/gl/glUserData.h"
#include "NL/gl/glView.h"
#include "NL/glx/glxGX.h"
#include "NL/glx/glxMatrix.h"
#include "NL/glx/glxTexture.h"
#include "NL/glx/glxDisplayList.h"
#include "dolphin/gx/GXDispList.h"
#include "dolphin/gx/GXBump.h"
#include "dolphin/gx/GXCull.h"
#include "dolphin/gx/GXTexture.h"
#include "NL/nlColour.h"
#include "NL/platvmath.h"
#include "types.h"
#include "dolphin/gx/GXVert.h"
#include <string.h>

// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
bool g_bAllowLighting = true;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
bool g_bAllowSpecular = true;
static nlColour nlBlack = { { 0, 0, 0, 0xFF } };
static nlColour nlWhite = { { 0xFF, 0xFF, 0xFF, 0xFF } };
static bool glx_AlwaysReloadLights = true;
static bool glx_CompiledDraw = true;
static u8 glx_InvXpose = 1;
static bool glx_AllowUncompiledDraws = true;
static bool g_bFastSkinPath = true;
static bool g_bMtxSkinMath = true;
static u32 glx_program = (u32)-1;

static eGLView prev_view;
static bool glx_IsCoPlanarView;
static u32 glx_texdirty;
static u8 glx_normals;
static bool glx_envdiffuse;
static bool glx_mobilediffuse;
static bool glx_constantcolour;
static bool glx_viewport;
static bool glx_CoPlanar;
static u32 glx_texconfig;
static u32 glx_NumIndices;
static u32 glx_DirtyFlags;
static bool glx_translucent;
static bool glx_norasterizedalpha;
static s32 glx_RasterizedAlphaStage;
static s32 glx_RasterizedAlphaArg;
static s32 glx_GlossMapStage;
static s32 glx_GlossMapCoord;
static bool glx_NoFog;
static u32 gx_vtxfmt;
static bool glx_allowSpecular;
static bool glx_ReloadPointLights;
static bool glx_ReloadSpecLights;
static u32 glx_prevLightMask;
static u32 glx_prevSpecMask;
static GXColor rshadow_colour[2];
static nlColour world_ambient;
static f32 glx_IndDivisor;
static _GXTevScale glx_tevscale;
// PORT: retail never assigned this, so it was 0, GX_ANISO_1, on every texture; -1 means "not asked yet".
static int glx_aniso = -1;
static u8 glx_InvXposeChar;

static void glud_Specular(void*);

static u32 prog_2d_unlit = glGetProgram("2d unlit");
static u32 prog_2d_movie = glGetProgram("2d movie");
static u32 prog_3d_unlit = glGetProgram("3d unlit");
static u32 prog_3d_unlit_2x = glGetProgram("3d unlit 2x");
static u32 prog_3d_pointlit = glGetProgram("3d pointlit");
static u32 prog_3d_pointlit_dirt = glGetProgram("3d pointlit dirt");
static u32 prog_3d_crowd = glGetProgram("3d crowd");
static u32 prog_3d_crowd_lit = glGetProgram("3d crowd lit");

static nlVector4 water_Scale = { 1.0f, 1.0f, 0.0f, 0.0f };
static nlVector4 water_Trans = { 0.0f, 0.0f, 0.0f, 0.0f };
static nlVector4 water_Follow = { 0.0f, 0.0f, 0.0f, 0.0f };
static f32 glx_konstlevel[4] = { -1.0f, -1.0f, -1.0f, -1.0f };

static nlMatrix4 mproj;
static nlMatrix4 mview;
static nlMatrix4 modelview;
static Mtx gx_mview;
static Mtx44 gx_proj;
static Mtx gx_modelview;
static GXTexObj glx_texobj[6];
static GXTlutObj glx_tlutobj[6];
static uintptr_t glx_texture[6];

struct GLScissorUserData
{
    u16 xOrig;
    u16 yOrig;
    u16 wd;
    u16 ht;
};

static GXAttr gx_texattr[6];
static GLViewportUserData g_viewport;

static void glx_SwitchTextureState(const glModelPacket*);
static unsigned long glx_SwitchTexConfig(const glModelPacket*);
static void glx_DrawPacket(const glModelPacket*);
static void glx_SwitchUserData(const glModelPacket*);
static void glx_LoadLight(GLLightUserData*, _GXLightID);
static void GetConstants();
static void glud_Skin(void*, const glModelPacket*);
static void glud_Light(void*);
static void glx_SwitchStreams(const glModelPacket*);
static void glx_SwitchRaster(const glModelPacket*);

struct GLSkinUserData
{
    int reg;
    float mat[12];
};

struct LightData
{
    u32 numLights;
    GLLightUserData* lights;
};

static inline GXColor makeColor(f32 r, f32 g, f32 b, f32 a)
{
    GXColor c;
    c.r = (u8)(r * 255.0f);
    c.g = (u8)(g * 255.0f);
    c.b = (u8)(b * 255.0f);
    c.a = (u8)(a * 255.0f);
    return c;
}

static inline nlColour getWorldAmbient()
{
    nlColour colour = { 0, 0, 0, 0 };
    nlColourSet(colour, world_ambient.c[0], world_ambient.c[1], world_ambient.c[2], world_ambient.c[3]);
    return colour;
}

/**
 * Offset/Address/Size: 0x4E94 | 0x801BE994 | size: 0x310
 */
static void GetConstants()
{
    nlVector4 vMult;
    nlVector4 vTexel;
    Mtx crowdMatrix;
    GXColor shadow0;
    GXColor shadow1;
    GXColor ambient;

    {
        const nlVector4& v = glConstantGet("shadow/pass0_colour");
        shadow0 = makeColor(v.x, v.y, v.z, v.w);
        rshadow_colour[0] = shadow0;
    }

    {
        const nlVector4& v = glConstantGet("shadow/pass1_colour");
        shadow1 = makeColor(v.x, v.y, v.z, v.w);
        rshadow_colour[1] = shadow1;
    }

    {
        const nlVector4& v = glConstantGet("lighting/ambient_colour");
        ambient = makeColor(v.x, v.y, v.z, v.w);
        world_ambient.c[0] = ambient.r;
        world_ambient.c[1] = ambient.g;
        world_ambient.c[2] = ambient.b;
        world_ambient.c[3] = ambient.a;
    }

    glConstantGet("water/scale", water_Scale);
    glConstantGet("water/trans", water_Trans);
    glConstantGet("water/follow", water_Follow);

    {
        const nlVector4& warbleDivisor = glConstantGet("warble/divisor");
        glx_IndDivisor = warbleDivisor.x;
    }

    vMult = glConstantGet("lighting/range");
    if (vMult.x == 1.0f)
    {
        glx_tevscale = (_GXTevScale)0;
    }
    else
    {
        glx_tevscale = (_GXTevScale)1;
    }

    if (glConstantGet("texture/density", vTexel))
    {
        glx_SetGridMode(vTexel.x == 1.0f);
    }

    {
        const nlVector4& crowdFrame = glConstantGet("crowd/frame");
        f32 crowdOffsetV = crowdFrame.x;
        PSMTXIdentity(crowdMatrix);
        crowdMatrix[1][3] = crowdOffsetV;
        GXLoadTexMtxImm(crowdMatrix, 0x36, (_GXTexMtxType)1);
    }
}

static inline void glx_SetVtxAttr()
{
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR1, GX_CLR_RGBA, GX_RGBA8, 0);

    for (s32 j = 0; j < 6; j++)
    {
        GXSetVtxAttrFmt(GX_VTXFMT0, (GXAttr)(GX_VA_TEX0 + j), GX_TEX_ST, GX_S16, 10);
    }
}

/**
 * Offset/Address/Size: 0x4C94 | 0x801BE794 | size: 0x200
 */
void glx_SendReset()
{
    prev_view = GLV_Num;
    glx_SetVtxAttr();

    glx_texdirty = 0;

    for (s32 i = 0; i < 6; i++)
    {
        memset(&glx_texobj[i], 0, sizeof(GXTexObj));
        memset(&glx_tlutobj[i], 0, sizeof(GXTlutObj));
        glx_texture[i] = 0;
    }

    gx_vtxfmt = 0;

    GetConstants();

    nlColour ambient = getWorldAmbient();
    gxSetChanAmbColour(0, ambient);
    gxSetChanMatColour(0, nlWhite);
    gxSetChanAmbColour(1, nlBlack);
    gxSetChanMatColour(1, nlWhite);

    GXSetTevSwapModeTable(GX_TEV_SWAP3, GX_CH_RED, GX_CH_RED, GX_CH_RED, GX_CH_RED);

    if (glx_prevSpecMask != 0)
    {
        gxSetNumChans(1);
        GXSetChanCtrl(GX_COLOR1, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, glx_prevSpecMask, GX_DF_NONE, GX_AF_NONE);
    }

    glx_DirtyFlags = 0;
    glx_ReloadPointLights = true;
    glx_ReloadSpecLights = true;
    glx_prevLightMask = 0;
    glx_prevSpecMask = 0;
    glx_allowSpecular = 0;
    glx_envdiffuse = false;
    glx_mobilediffuse = false;
    glx_constantcolour = false;
    glx_viewport = false;
    glx_CoPlanar = false;

    gxSetCoPlanar(false);

    glx_translucent = false;
    glx_norasterizedalpha = false;
    glx_RasterizedAlphaStage = -1;
    glx_RasterizedAlphaArg = -1;
    glx_GlossMapStage = -1;
    glx_GlossMapCoord = -1;
    glx_NoFog = false;
}

/**
 * Offset/Address/Size: 0x4C70 | 0x801BE770 | size: 0x24
 */
void glx_SendEnd()
{
    glx_SwitchUserData(nullptr);
}

extern const u32 glv_MatrixChanged;

/**
 * Offset/Address/Size: 0x2B1C | 0x801BC61C | size: 0x2154
 */
static unsigned long glx_SwitchTexConfig(const glModelPacket* p)
{
    int i;
    int bit;
    int texnum;
    unsigned long extra;
    GXAttr attr;

#define SET_TEV_ORDER(stage, coord, map, chan) \
    GXSetTevOrder((GXTevStageID)(stage), (GXTexCoordID)(coord), (GXTexMapID)(map), (GXChannelID)(chan))
#define SET_TEV_COLOR_IN(stage, a, b, c, d) \
    GXSetTevColorIn((GXTevStageID)(stage), (GXTevColorArg)(a), (GXTevColorArg)(b), (GXTevColorArg)(c), (GXTevColorArg)(d))
#define SET_TEV_ALPHA_IN(stage, a, b, c, d) \
    gxSetTevAlphaIn((int)(stage), (_GXTevAlphaArg)(a), (_GXTevAlphaArg)(b), (_GXTevAlphaArg)(c), (_GXTevAlphaArg)(d))
#define SET_TEV_KCOLOR_SEL(stage, sel) \
    GXSetTevKColorSel((GXTevStageID)(stage), (GXTevKColorSel)(sel))
#define SET_TEV_KALPHA_SEL(stage, sel) \
    GXSetTevKAlphaSel((GXTevStageID)(stage), (GXTevKAlphaSel)(sel))
#define SET_TEX_GEN(stage, type, src, mtx) \
    gxSetTexCoordGen((int)(stage), (_GXTexGenType)(type), (_GXTexGenSrc)(src), (u32)(mtx))

    glx_texconfig = p->state.texconfig;
    extra = 0x40;

    if (glx_texconfig & 0x10)
    {
        if (glx_normals == 0)
        {
            extra = glv_MatrixChanged | 0x40;
            glx_normals = 1;
        }
        glx_allowSpecular = 1;
    }
    else
    {
        glx_allowSpecular = 0;
    }

    i = (int)gx_vtxfmt;
    i++;
    gx_texattr[0] = GX_VA_NULL;
    gx_texattr[1] = GX_VA_NULL;
    gx_texattr[2] = GX_VA_NULL;
    gx_texattr[3] = GX_VA_NULL;
    gx_texattr[4] = GX_VA_NULL;
    gx_texattr[5] = GX_VA_NULL;
    if (i >= 1)
        i = 0;

    gx_vtxfmt = (_GXVtxFmt)i;
    glx_NumIndices = 0;
    for (bit = texnum = 0; bit < 6; bit++)
    {
        if (glx_texconfig & (1 << bit))
        {
            attr = (GXAttr)(texnum + 13);
            gx_texattr[bit] = attr;
            texnum++;
        }
    }

    gxSetTevColourOp(0, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)0);
    gxSetTevColourOp(1, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)0);
    gxSetTevColourOp(2, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)0);
    gxSetTevColourOp(3, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)0);

    gxSetTevAlphaOp(0, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)0);
    gxSetTevAlphaOp(1, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)0);
    gxSetTevAlphaOp(2, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)0);
    gxSetTevAlphaOp(3, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)0);

    SET_TEX_GEN(0, 1, 4, 0x3C);
    SET_TEX_GEN(1, 1, 5, 0x3C);
    SET_TEX_GEN(2, 1, 6, 0x3C);
    SET_TEX_GEN(3, 1, 7, 0x3C);

    glx_RasterizedAlphaStage = -1;
    glx_RasterizedAlphaArg = -1;
    glx_GlossMapStage = -1;
    glx_GlossMapCoord = -1;

    if (glx_program == prog_3d_crowd || glx_program == prog_3d_crowd_lit)
    {
        switch (glx_texconfig)
        {
        case 0x01:
            gxSetNumTexGens(1);
            gxSetNumTevStages(1);
            SET_TEV_ORDER(0, 0, 0, 4);
            SET_TEX_GEN(0, 1, 4, 0x36);
            SET_TEV_COLOR_IN(0, 15, 10, 8, 15);
            SET_TEV_ALPHA_IN(0, 7, 5, 4, 7);
            glx_RasterizedAlphaStage = 0;
            glx_RasterizedAlphaArg = 1;
            break;
        case 0x21:
            gxSetNumTexGens(2);
            gxSetNumTevStages(2);
            SET_TEV_ORDER(0, 1, 1, 4);
            SET_TEV_ORDER(1, 0, 0, 0xFF);
            SET_TEX_GEN(0, 1, 4, 0x36);
            SET_TEX_GEN(1, 10, 19, 0x3C);
            gxSetTevColourOp(1, (_GXTevOp)0, (_GXTevBias)0, glx_tevscale, true, (_GXTevRegID)0);
            SET_TEV_COLOR_IN(0, 15, 12, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 0, 8, 15);
            SET_TEV_ALPHA_IN(0, 7, 6, 6, 7);
            SET_TEV_ALPHA_IN(1, 7, 6, 4, 7);
            glx_RasterizedAlphaStage = 1;
            glx_RasterizedAlphaArg = 1;
            break;
        }
    }
    else
    {
        switch (glx_texconfig)
        {
        case 0x00:
            gxSetNumTexGens(0);
            gxSetNumTevStages(1);
            SET_TEV_ORDER(0, 0xFF, 0xFF, 4);
            SET_TEV_COLOR_IN(0, 15, 10, 12, 15);
            SET_TEV_ALPHA_IN(0, 7, 5, 6, 7);
            glx_RasterizedAlphaStage = 0;
            glx_RasterizedAlphaArg = 1;
            break;
        case 0x01:
            gxSetNumTexGens(1);
            gxSetNumTevStages(1);
            SET_TEV_ORDER(0, 0, 0, 4);
            if (glx_program == prog_3d_unlit_2x)
            {
                gxSetTevColourOp(0, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)1, true, (_GXTevRegID)1);
            }
            SET_TEV_COLOR_IN(0, 15, 10, 8, 15);
            SET_TEV_ALPHA_IN(0, 7, 5, 4, 7);
            glx_RasterizedAlphaStage = 0;
            glx_RasterizedAlphaArg = 1;
            break;
        case 0x03:
            gxSetNumTexGens(2);
            gxSetNumTevStages(3);
            SET_TEV_ORDER(0, 0, 0, 4);
            SET_TEV_ORDER(1, 1, 1, 0xFF);
            SET_TEV_ORDER(2, 0xFF, 0xFF, 4);
            SET_TEV_KCOLOR_SEL(0, 0x0C);
            SET_TEV_KCOLOR_SEL(1, 0x0C);
            SET_TEV_KALPHA_SEL(0, 0x1C);
            SET_TEV_KALPHA_SEL(1, 0x1C);
            SET_TEV_COLOR_IN(0, 15, 8, 14, 15);
            SET_TEV_COLOR_IN(1, 8, 15, 14, 0);
            SET_TEV_COLOR_IN(2, 15, 0, 10, 15);
            SET_TEV_ALPHA_IN(0, 7, 4, 6, 7);
            SET_TEV_ALPHA_IN(1, 4, 7, 6, 0);
            SET_TEV_ALPHA_IN(2, 7, 0, 5, 7);
            glx_RasterizedAlphaStage = 2;
            glx_RasterizedAlphaArg = 2;
            break;
        case 0x07:
            if (glx_program == prog_2d_movie)
            {
                gxSetNumTexGens(3);
                gxSetNumTevStages(4);
                SET_TEV_ORDER(0, 1, 1, 0xFF);
                SET_TEV_ORDER(1, 1, 2, 0xFF);
                SET_TEV_ORDER(2, 0, 0, 0xFF);
                SET_TEV_ORDER(3, 0xFF, 0xFF, 0xFF);
                SET_TEV_COLOR_IN(0, 15, 8, 14, 2);
                gxSetTevColourOp(0, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, false, (_GXTevRegID)0);
                SET_TEV_ALPHA_IN(0, 7, 4, 6, 1);
                gxSetTevAlphaOp(0, (_GXTevOp)1, (_GXTevBias)0, (_GXTevScale)0, false, (_GXTevRegID)0);
                SET_TEV_KCOLOR_SEL(0, 0x0C);
                SET_TEV_KALPHA_SEL(0, 0x1C);
                SET_TEV_COLOR_IN(1, 15, 8, 14, 0);
                gxSetTevColourOp(1, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)1, false, (_GXTevRegID)0);
                SET_TEV_ALPHA_IN(1, 7, 4, 6, 0);
                gxSetTevAlphaOp(1, (_GXTevOp)1, (_GXTevBias)0, (_GXTevScale)0, false, (_GXTevRegID)0);
                SET_TEV_KCOLOR_SEL(1, 0x0D);
                SET_TEV_KALPHA_SEL(1, 0x1D);
                SET_TEV_COLOR_IN(2, 15, 8, 12, 0);
                gxSetTevColourOp(2, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)0);
                SET_TEV_ALPHA_IN(2, 4, 7, 7, 0);
                gxSetTevAlphaOp(2, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)0);
                SET_TEV_COLOR_IN(3, 1, 0, 14, 15);
                gxSetTevColourOp(3, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)0);
                SET_TEV_ALPHA_IN(3, 7, 7, 7, 7);
                gxSetTevAlphaOp(3, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)0);
                SET_TEV_KCOLOR_SEL(3, 0x0E);
                {
                    GXColorS10 movieColour = { (s16)0xFFA6, 0, (s16)0xFF8E, 0x87 };
                    GXSetTevColorS10((GXTevRegID)1, movieColour);
                }
            }
            else
            {
                gxSetNumTexGens(3);
                gxSetNumTevStages(6);
                SET_TEV_ORDER(0, 0, 0, 4);
                SET_TEV_ORDER(1, 1, 1, 0xFF);
                SET_TEV_ORDER(2, 0xFF, 0xFF, 4);
                SET_TEV_ORDER(3, 2, 2, 0xFF);
                SET_TEV_ORDER(4, 0xFF, 0xFF, 0xFF);
                SET_TEV_KCOLOR_SEL(0, 0x0C);
                SET_TEV_KCOLOR_SEL(1, 0x0C);
                SET_TEV_KCOLOR_SEL(3, 0x0D);
                SET_TEV_KCOLOR_SEL(4, 0x0E);
                SET_TEV_KALPHA_SEL(0, 0x1C);
                SET_TEV_KALPHA_SEL(1, 0x1C);
                SET_TEV_KALPHA_SEL(3, 0x1D);
                SET_TEV_KALPHA_SEL(4, 0x1E);
                gxSetTevColourOp(2, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)1);
                SET_TEV_COLOR_IN(0, 15, 8, 14, 15);
                SET_TEV_COLOR_IN(1, 8, 15, 14, 0);
                SET_TEV_COLOR_IN(2, 15, 0, 10, 15);
                SET_TEV_COLOR_IN(3, 15, 14, 8, 15);
                SET_TEV_COLOR_IN(4, 15, 12, 0, 14);
                SET_TEV_COLOR_IN(5, 15, 2, 0, 15);
                SET_TEV_ALPHA_IN(0, 7, 4, 6, 7);
                SET_TEV_ALPHA_IN(1, 4, 7, 6, 0);
                SET_TEV_ALPHA_IN(2, 7, 0, 5, 7);
                SET_TEV_ALPHA_IN(3, 7, 7, 7, 0);
                SET_TEV_ALPHA_IN(4, 7, 7, 7, 0);
                SET_TEV_ALPHA_IN(5, 7, 7, 7, 0);
                glx_RasterizedAlphaStage = 2;
                glx_RasterizedAlphaArg = 2;
            }
            break;
        case 0x11:
            gxSetNumTexGens(2);
            gxSetNumTevStages(2);
            SET_TEV_ORDER(0, 0, 0, 4);
            SET_TEV_ORDER(1, 1, 1, 5);
            SET_TEV_COLOR_IN(0, 15, 10, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 10, 8, 0);
            SET_TEV_ALPHA_IN(0, 7, 5, 4, 7);
            SET_TEV_ALPHA_IN(1, 7, 7, 7, 0);
            glx_RasterizedAlphaStage = 0;
            glx_RasterizedAlphaArg = 1;
            glx_GlossMapStage = 1;
            glx_GlossMapCoord = 1;
            break;
        case 0x13:
            gxSetNumTexGens(3);
            gxSetNumTevStages(4);
            SET_TEV_ORDER(0, 0, 0, 4);
            SET_TEV_ORDER(1, 1, 1, 0xFF);
            SET_TEV_ORDER(2, 0xFF, 0xFF, 4);
            SET_TEV_ORDER(3, 2, 2, 5);
            SET_TEV_KCOLOR_SEL(0, 0x0C);
            SET_TEV_KCOLOR_SEL(1, 0x0C);
            SET_TEV_KALPHA_SEL(0, 0x1C);
            SET_TEV_KALPHA_SEL(1, 0x1C);
            SET_TEV_COLOR_IN(0, 15, 8, 14, 15);
            SET_TEV_COLOR_IN(1, 8, 15, 14, 0);
            SET_TEV_COLOR_IN(2, 15, 0, 10, 15);
            SET_TEV_COLOR_IN(3, 15, 10, 8, 0);
            SET_TEV_ALPHA_IN(0, 7, 5, 4, 7);
            SET_TEV_ALPHA_IN(1, 7, 7, 7, 0);
            SET_TEV_ALPHA_IN(2, 7, 7, 7, 0);
            SET_TEV_ALPHA_IN(3, 7, 7, 7, 0);
            glx_RasterizedAlphaStage = 0;
            glx_RasterizedAlphaArg = 1;
            glx_GlossMapStage = 3;
            glx_GlossMapCoord = 2;
            break;
        case 0x05:
            gxSetNumTexGens(2);
            gxSetNumTevStages(2);
            SET_TEV_ORDER(0, 0, 0, 4);
            SET_TEV_ORDER(1, 1, 1, 0xFF);
            SET_TEV_COLOR_IN(0, 15, 10, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 0, 8, 15);
            SET_TEV_ALPHA_IN(0, 7, 5, 4, 7);
            SET_TEV_ALPHA_IN(1, 7, 7, 7, 0);
            glx_RasterizedAlphaStage = 0;
            glx_RasterizedAlphaArg = 1;
            break;
        case 0x15:
            gxSetNumTexGens(3);
            gxSetNumTevStages(3);
            SET_TEV_ORDER(0, 0, 0, 4);
            SET_TEV_ORDER(1, 1, 1, 0xFF);
            SET_TEV_ORDER(2, 2, 2, 5);
            SET_TEV_COLOR_IN(0, 15, 10, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 0, 8, 15);
            SET_TEV_COLOR_IN(2, 15, 10, 8, 0);
            SET_TEV_ALPHA_IN(0, 7, 5, 4, 7);
            SET_TEV_ALPHA_IN(1, 7, 7, 7, 0);
            SET_TEV_ALPHA_IN(1, 7, 7, 7, 0);
            glx_RasterizedAlphaStage = 0;
            glx_RasterizedAlphaArg = 1;
            glx_GlossMapStage = 2;
            glx_GlossMapCoord = 2;
            break;
        case 0x09:
            gxSetNumTexGens(2);
            gxSetNumTevStages(2);
            SET_TEV_ORDER(0, 0, 0, 4);
            SET_TEV_ORDER(1, 1, 1, 0xFF);
            SET_TEV_COLOR_IN(0, 15, 10, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 0, 12, 8);
            SET_TEV_ALPHA_IN(0, 7, 5, 4, 7);
            SET_TEV_ALPHA_IN(1, 7, 7, 7, 0);
            glx_RasterizedAlphaStage = 0;
            glx_RasterizedAlphaArg = 1;
            break;
        case 0x19:
            gxSetNumTexGens(3);
            gxSetNumTevStages(3);
            SET_TEV_ORDER(0, 0, 0, 4);
            SET_TEV_ORDER(1, 1, 1, 0xFF);
            SET_TEV_ORDER(2, 2, 2, 5);
            SET_TEV_COLOR_IN(0, 15, 10, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 0, 12, 8);
            SET_TEV_COLOR_IN(2, 15, 10, 8, 0);
            SET_TEV_ALPHA_IN(0, 7, 5, 4, 7);
            SET_TEV_ALPHA_IN(1, 7, 7, 7, 0);
            SET_TEV_ALPHA_IN(2, 7, 7, 7, 0);
            glx_RasterizedAlphaStage = 0;
            glx_RasterizedAlphaArg = 1;
            glx_GlossMapStage = 2;
            glx_GlossMapCoord = 2;
            break;
        case 0x0D:
            gxSetNumTexGens(3);
            gxSetNumTevStages(3);
            SET_TEV_ORDER(0, 0, 0, 4);
            SET_TEV_ORDER(1, 1, 1, 0xFF);
            SET_TEV_ORDER(2, 2, 2, 0xFF);
            SET_TEV_COLOR_IN(0, 15, 10, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 0, 8, 15);
            SET_TEV_COLOR_IN(2, 15, 0, 12, 8);
            SET_TEV_ALPHA_IN(0, 7, 5, 4, 7);
            SET_TEV_ALPHA_IN(1, 7, 7, 7, 0);
            SET_TEV_ALPHA_IN(2, 7, 7, 7, 0);
            glx_RasterizedAlphaStage = 0;
            glx_RasterizedAlphaArg = 1;
            break;
        case 0x21:
            gxSetNumTexGens(2);
            gxSetNumTevStages(2);
            SET_TEV_ORDER(0, 1, 1, 4);
            SET_TEV_ORDER(1, 0, 0, 0xFF);
            SET_TEX_GEN(1, 10, 19, 0x3C);
            gxSetTevColourOp(1, (_GXTevOp)0, (_GXTevBias)0, glx_tevscale, true, (_GXTevRegID)0);
            SET_TEV_COLOR_IN(0, 15, 12, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 0, 8, 15);
            SET_TEV_ALPHA_IN(0, 7, 6, 6, 7);
            SET_TEV_ALPHA_IN(1, 7, 6, 4, 7);
            glx_RasterizedAlphaStage = 1;
            glx_RasterizedAlphaArg = 1;
            break;
        case 0x29:
            gxSetNumTexGens(3);
            gxSetNumTevStages(3);
            SET_TEV_ORDER(0, 2, 2, 4);
            SET_TEV_ORDER(1, 0, 0, 0xFF);
            SET_TEV_ORDER(2, 1, 1, 0xFF);
            SET_TEX_GEN(2, 10, 19, 0x3C);
            gxSetTevColourOp(1, (_GXTevOp)0, (_GXTevBias)0, glx_tevscale, true, (_GXTevRegID)0);
            SET_TEV_COLOR_IN(0, 15, 12, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 0, 8, 15);
            SET_TEV_COLOR_IN(2, 15, 12, 8, 0);
            SET_TEV_ALPHA_IN(0, 7, 6, 6, 7);
            SET_TEV_ALPHA_IN(1, 7, 6, 4, 7);
            SET_TEV_ALPHA_IN(2, 7, 6, 6, 0);
            glx_RasterizedAlphaStage = 1;
            glx_RasterizedAlphaArg = 1;
            break;
        case 0x25:
            gxSetNumTexGens(3);
            gxSetNumTevStages(3);
            SET_TEV_ORDER(0, 2, 2, 4);
            SET_TEV_ORDER(1, 0, 0, 0xFF);
            SET_TEV_ORDER(2, 1, 1, 0xFF);
            SET_TEX_GEN(2, 10, 19, 0x3C);
            gxSetTevColourOp(2, (_GXTevOp)0, (_GXTevBias)0, glx_tevscale, true, (_GXTevRegID)0);
            SET_TEV_COLOR_IN(0, 15, 12, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 0, 8, 15);
            SET_TEV_COLOR_IN(2, 15, 0, 8, 15);
            SET_TEV_ALPHA_IN(0, 7, 6, 6, 7);
            SET_TEV_ALPHA_IN(1, 7, 6, 4, 7);
            SET_TEV_ALPHA_IN(1, 7, 7, 7, 0);
            glx_RasterizedAlphaStage = 1;
            glx_RasterizedAlphaArg = 1;
            break;
        case 0x23:
            if (glx_program == prog_3d_pointlit_dirt)
            {
                gxSetNumTexGens(3);
                gxSetNumTevStages(4);
                SET_TEV_ORDER(0, 2, 2, 4);
                SET_TEV_ORDER(1, 1, 1, 0xFF);
                SET_TEV_ORDER(2, 0, 0, 0xFF);
                SET_TEV_ORDER(3, 0xFF, 0xFF, 0xFF);
                SET_TEX_GEN(2, 10, 19, 0x3C);
                gxSetTevColourOp(0, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)1);
                gxSetTevColourOp(3, (_GXTevOp)0, (_GXTevBias)0, glx_tevscale, true, (_GXTevRegID)0);
                SET_TEV_KCOLOR_SEL(1, 0x0C);
                SET_TEV_KCOLOR_SEL(2, 0x0C);
                SET_TEV_COLOR_IN(0, 15, 12, 8, 15);
                SET_TEV_COLOR_IN(1, 15, 12, 8, 14);
                SET_TEV_COLOR_IN(2, 15, 8, 0, 15);
                SET_TEV_COLOR_IN(3, 15, 0, 2, 15);
                SET_TEV_ALPHA_IN(0, 7, 7, 7, 7);
                SET_TEV_ALPHA_IN(1, 7, 7, 7, 7);
                SET_TEV_ALPHA_IN(2, 7, 7, 7, 4);
                SET_TEV_ALPHA_IN(3, 7, 7, 7, 0);
            }
            else
            {
                gxSetNumTexGens(3);
                gxSetNumTevStages(4);
                SET_TEV_ORDER(0, 2, 2, 4);
                SET_TEV_ORDER(1, 0, 0, 0xFF);
                SET_TEV_ORDER(2, 1, 1, 0xFF);
                SET_TEV_ORDER(3, 0xFF, 0xFF, 0xFF);
                SET_TEX_GEN(2, 10, 19, 0x3C);
                gxSetTevColourOp(0, (_GXTevOp)0, (_GXTevBias)0, glx_tevscale, true, (_GXTevRegID)1);
                SET_TEV_KCOLOR_SEL(1, 0x0C);
                SET_TEV_KCOLOR_SEL(2, 0x0C);
                SET_TEV_KALPHA_SEL(1, 0x1C);
                SET_TEV_KALPHA_SEL(2, 0x1C);
                SET_TEV_COLOR_IN(0, 15, 12, 8, 15);
                SET_TEV_COLOR_IN(1, 15, 8, 14, 15);
                SET_TEV_COLOR_IN(2, 8, 15, 14, 0);
                SET_TEV_COLOR_IN(3, 15, 0, 2, 15);
                SET_TEV_ALPHA_IN(0, 7, 7, 7, 7);
                SET_TEV_ALPHA_IN(1, 7, 4, 6, 7);
                SET_TEV_ALPHA_IN(2, 4, 7, 6, 0);
                SET_TEV_ALPHA_IN(3, 7, 7, 7, 0);
            }
            break;
        case 0x27:
            gxSetNumTexGens(4);
            gxSetNumTevStages(7);
            SET_TEV_ORDER(0, 3, 3, 4);
            SET_TEV_ORDER(1, 0, 0, 0xFF);
            SET_TEV_ORDER(2, 1, 1, 0xFF);
            SET_TEV_ORDER(3, 0xFF, 0xFF, 0xFF);
            SET_TEV_ORDER(4, 2, 2, 0xFF);
            SET_TEV_ORDER(5, 0xFF, 0xFF, 0xFF);
            SET_TEX_GEN(3, 10, 19, 0x3C);
            gxSetTevColourOp(0, (_GXTevOp)0, (_GXTevBias)0, glx_tevscale, true, (_GXTevRegID)1);
            SET_TEV_KCOLOR_SEL(1, 0x0C);
            SET_TEV_KCOLOR_SEL(2, 0x0C);
            SET_TEV_KCOLOR_SEL(4, 0x0D);
            SET_TEV_KCOLOR_SEL(5, 0x0E);
            SET_TEV_KALPHA_SEL(1, 0x1C);
            SET_TEV_KALPHA_SEL(2, 0x1C);
            SET_TEV_KALPHA_SEL(4, 0x1D);
            SET_TEV_KALPHA_SEL(5, 0x1E);
            gxSetTevColourOp(3, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)1);
            SET_TEV_COLOR_IN(0, 15, 12, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 8, 14, 15);
            SET_TEV_COLOR_IN(2, 8, 15, 14, 0);
            SET_TEV_COLOR_IN(3, 15, 0, 2, 15);
            SET_TEV_COLOR_IN(4, 15, 14, 8, 15);
            SET_TEV_COLOR_IN(5, 15, 12, 0, 14);
            SET_TEV_COLOR_IN(6, 15, 2, 0, 15);
            SET_TEV_ALPHA_IN(0, 7, 7, 7, 7);
            SET_TEV_ALPHA_IN(1, 7, 4, 6, 7);
            SET_TEV_ALPHA_IN(2, 4, 7, 6, 0);
            SET_TEV_ALPHA_IN(3, 7, 0, 5, 7);
            SET_TEV_ALPHA_IN(4, 7, 7, 7, 0);
            SET_TEV_ALPHA_IN(5, 7, 7, 7, 0);
            SET_TEV_ALPHA_IN(6, 7, 7, 7, 0);
            glx_RasterizedAlphaStage = 3;
            glx_RasterizedAlphaArg = 2;
            break;
        case 0x2B:
            gxSetNumTexGens(4);
            gxSetNumTevStages(5);
            SET_TEV_ORDER(0, 3, 3, 4);
            SET_TEV_ORDER(1, 0, 0, 0xFF);
            SET_TEV_ORDER(2, 1, 1, 0xFF);
            SET_TEV_ORDER(3, 0xFF, 0xFF, 0xFF);
            SET_TEV_ORDER(4, 2, 2, 0xFF);
            SET_TEX_GEN(3, 10, 19, 0x3C);
            gxSetTevColourOp(0, (_GXTevOp)0, (_GXTevBias)0, glx_tevscale, true, (_GXTevRegID)1);
            SET_TEV_KCOLOR_SEL(1, 0x0C);
            SET_TEV_KCOLOR_SEL(2, 0x0C);
            SET_TEV_KALPHA_SEL(1, 0x1C);
            SET_TEV_KALPHA_SEL(2, 0x1C);
            SET_TEV_COLOR_IN(0, 15, 12, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 8, 14, 15);
            SET_TEV_COLOR_IN(2, 8, 15, 14, 0);
            SET_TEV_COLOR_IN(3, 15, 0, 2, 15);
            SET_TEV_COLOR_IN(4, 15, 0, 12, 8);
            SET_TEV_ALPHA_IN(0, 7, 7, 7, 7);
            SET_TEV_ALPHA_IN(1, 7, 4, 6, 7);
            SET_TEV_ALPHA_IN(2, 4, 7, 6, 0);
            SET_TEV_ALPHA_IN(3, 7, 7, 7, 0);
            SET_TEV_ALPHA_IN(4, 7, 7, 7, 0);
            break;
        case 0x31:
            gxSetNumTexGens(3);
            gxSetNumTevStages(4);
            SET_TEV_ORDER(0, 2, 2, 4);
            SET_TEV_ORDER(1, 0, 0, 0xFF);
            SET_TEV_ORDER(2, 1, 1, 5);
            SET_TEV_ORDER(3, 0xFF, 0xFF, 0xFF);
            SET_TEV_KCOLOR_SEL(3, 0x0F);
            SET_TEV_KALPHA_SEL(3, 0x1F);
            gxSetTevColourOp(1, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)1);
            gxSetTevAlphaOp(1, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)1);
            SET_TEX_GEN(2, 10, 19, 0x3C);
            gxSetTevColourOp(0, (_GXTevOp)0, (_GXTevBias)0, glx_tevscale, true, (_GXTevRegID)0);
            SET_TEV_COLOR_IN(0, 15, 12, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 0, 8, 15);
            SET_TEV_COLOR_IN(2, 15, 10, 8, 15);
            SET_TEV_COLOR_IN(3, 15, 14, 0, 2);
            SET_TEV_ALPHA_IN(0, 7, 6, 6, 7);
            SET_TEV_ALPHA_IN(1, 7, 6, 4, 7);
            SET_TEV_ALPHA_IN(2, 7, 7, 7, 7);
            SET_TEV_ALPHA_IN(3, 7, 6, 0, 1);
            glx_RasterizedAlphaStage = 1;
            glx_RasterizedAlphaArg = 1;
            glx_GlossMapStage = 2;
            glx_GlossMapCoord = 1;
            break;
        case 0x39:
            gxSetNumTexGens(4);
            gxSetNumTevStages(5);
            SET_TEV_ORDER(0, 3, 3, 4);
            SET_TEV_ORDER(1, 0, 0, 0xFF);
            SET_TEV_ORDER(2, 2, 2, 5);
            SET_TEV_ORDER(3, 1, 1, 0xFF);
            SET_TEV_ORDER(4, 0xFF, 0xFF, 0xFF);
            SET_TEV_KCOLOR_SEL(4, 0x0F);
            SET_TEV_KALPHA_SEL(4, 0x1F);
            gxSetTevColourOp(1, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)1);
            gxSetTevAlphaOp(1, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)1);
            SET_TEX_GEN(3, 10, 19, 0x3C);
            gxSetTevColourOp(0, (_GXTevOp)0, (_GXTevBias)0, glx_tevscale, true, (_GXTevRegID)0);
            SET_TEV_COLOR_IN(0, 15, 12, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 0, 8, 15);
            SET_TEV_COLOR_IN(2, 15, 10, 8, 15);
            SET_TEV_COLOR_IN(3, 15, 0, 8, 15);
            SET_TEV_COLOR_IN(4, 15, 14, 0, 2);
            SET_TEV_ALPHA_IN(0, 7, 6, 6, 7);
            SET_TEV_ALPHA_IN(1, 7, 6, 4, 7);
            SET_TEV_ALPHA_IN(2, 7, 7, 7, 7);
            SET_TEV_ALPHA_IN(3, 7, 7, 7, 7);
            SET_TEV_ALPHA_IN(4, 7, 6, 0, 1);
            glx_GlossMapStage = 2;
            glx_GlossMapCoord = 2;
            break;
        case 0x35:
            gxSetNumTexGens(4);
            gxSetNumTevStages(5);
            SET_TEV_ORDER(0, 3, 3, 4);
            SET_TEV_ORDER(1, 0, 0, 0xFF);
            SET_TEV_ORDER(2, 1, 1, 0xFF);
            SET_TEV_ORDER(3, 2, 2, 5);
            SET_TEV_ORDER(4, 0xFF, 0xFF, 0xFF);
            SET_TEV_KCOLOR_SEL(4, 0x0F);
            SET_TEV_KALPHA_SEL(4, 0x1F);
            gxSetTevColourOp(1, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)1);
            gxSetTevAlphaOp(1, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)1);
            gxSetTevColourOp(2, (_GXTevOp)0, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)1);
            SET_TEX_GEN(3, 10, 19, 0x3C);
            gxSetTevColourOp(0, (_GXTevOp)0, (_GXTevBias)0, glx_tevscale, true, (_GXTevRegID)0);
            SET_TEV_COLOR_IN(0, 15, 12, 8, 15);
            SET_TEV_COLOR_IN(1, 15, 0, 8, 15);
            SET_TEV_COLOR_IN(2, 15, 2, 8, 15);
            SET_TEV_COLOR_IN(3, 15, 10, 8, 15);
            SET_TEV_COLOR_IN(4, 15, 14, 0, 2);
            SET_TEV_ALPHA_IN(0, 7, 6, 6, 7);
            SET_TEV_ALPHA_IN(1, 7, 6, 4, 7);
            SET_TEV_ALPHA_IN(2, 7, 6, 6, 7);
            SET_TEV_ALPHA_IN(3, 7, 7, 7, 7);
            SET_TEV_ALPHA_IN(4, 7, 6, 0, 1);
            glx_GlossMapStage = 3;
            glx_GlossMapCoord = 2;
            break;
        case 0x02:
        case 0x04:
        case 0x06:
        case 0x08:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0E:
        case 0x0F:
        case 0x10:
        case 0x12:
        case 0x14:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
        case 0x1F:
        case 0x20:
        case 0x22:
        case 0x24:
        case 0x26:
        case 0x28:
        case 0x2A:
        case 0x2C:
        case 0x2D:
        case 0x2E:
        case 0x2F:
        case 0x30:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x36:
        case 0x37:
        case 0x38:
            break;
        default:
            break;
        }
    }

#undef SET_TEV_ORDER
#undef SET_TEV_COLOR_IN
#undef SET_TEV_ALPHA_IN
#undef SET_TEV_KCOLOR_SEL
#undef SET_TEV_KALPHA_SEL
#undef SET_TEX_GEN

    return extra;
}

static inline void setGreyKColor(GXTevKColorID stage, u8 value)
{
    GXColor colour = { 0, 0, 0, 0 };
    colour.r = value;
    colour.g = value;
    colour.b = value;
    colour.a = value;
    GXSetTevKColor(stage, colour);
}

/**
 * Offset/Address/Size: 0x2690 | 0x801BC190 | size: 0x48C
 */
static void glx_SwitchTextureState(const glModelPacket* p)
{
    int bit;
    int texnum;
    GXTexWrapMode mode[2];
    eGLTextureMode tmode;
    eGLTextureFilter filter;

    glUnHandleizeTextureState(p->state.texturestate);

    if (glx_program == prog_2d_movie)
    {
        GXColor c0 = { 0x00, 0x00, 0xE2, 0x58 };
        GXSetTevKColor(GX_KCOLOR0, c0);
        GXColor c1 = { 0xB3, 0x00, 0x00, 0xB6 };
        GXSetTevKColor(GX_KCOLOR1, c1);
        GXColor c2 = { 0xFF, 0x00, 0xFF, 0x80 };
        GXSetTevKColor(GX_KCOLOR2, c2);
        glx_konstlevel[0] = -1.0f;
        glx_konstlevel[1] = -1.0f;
        glx_konstlevel[2] = -1.0f;
        glx_konstlevel[3] = -1.0f;
    }
    else
    {
        u8 raw;
        f32 level;
        raw = (u8)glGetTextureState(GLTS_DiffuseLevel);
        level = (f32)raw * (1.0f / 63.0f);
        if (level != glx_konstlevel[0])
        {
            int val = (int)(255.5f * level);
            setGreyKColor(GX_KCOLOR0, (u8)val);
            glx_konstlevel[0] = level;
        }

        raw = (u8)glGetTextureState(GLTS_ShadowLevel);
        level = (f32)raw * (1.0f / 63.0f);
        if (level != glx_konstlevel[1])
        {
            int val = (int)(255.5f * level);
            setGreyKColor(GX_KCOLOR1, (u8)val);
            glx_konstlevel[1] = level;
        }

        raw = (u8)glGetTextureState(GLTS_ShadowLevel);
        {
            f32 shadowLevel = (f32)raw * (1.0f / 63.0f);
            level = 1.0f - shadowLevel;
        }
        if (level != glx_konstlevel[2])
        {
            int val = (int)(255.5f * level);
            setGreyKColor(GX_KCOLOR2, (u8)val);
            glx_konstlevel[2] = level;
        }

        raw = (u8)glGetTextureState(GLTS_GlossLevel);
        level = (f32)raw * (1.0f / 63.0f);
        if (level != glx_konstlevel[3])
        {
            int val = (int)(255.5f * level);
            setGreyKColor(GX_KCOLOR3, (u8)val);
            glx_konstlevel[3] = level;
        }
    }

    for (bit = texnum = 0; bit < 6; bit++)
    {
        if (glx_texconfig & (1 << bit))
        {
            if (bit == 5)
            {
                mode[1] = GX_CLAMP;
                mode[0] = GX_CLAMP;
            }
            else
            {
                tmode = (eGLTextureMode)glGetTextureState((eGLTextureState)bit);
                switch (tmode)
                {
                case GLTM_WrapWrap:
                    mode[0] = GX_REPEAT;
                    mode[1] = GX_REPEAT;
                    break;
                case GLTM_WrapClamp:
                    mode[0] = GX_REPEAT;
                    mode[1] = GX_CLAMP;
                    break;
                case GLTM_ClampWrap:
                    mode[0] = GX_CLAMP;
                    mode[1] = GX_REPEAT;
                    break;
                case GLTM_ClampClamp:
                    mode[0] = GX_CLAMP;
                    mode[1] = GX_CLAMP;
                    break;
                default:
                    break;
                }
            }

            GXInitTexObjWrapMode(&glx_texobj[texnum], mode[0], mode[1]);

            filter = (eGLTextureFilter)glGetTextureState((eGLTextureState)(bit + 6));
            switch (filter)
            {
            case GLTF_Linear:
            {
                PlatTexture* tex = (PlatTexture*)glx_texture[texnum];
                if (tex == NULL || tex->m_Levels == 1)   // PORT: see above
                    GXInitTexObjFilter(&glx_texobj[texnum], GX_LINEAR, GX_LINEAR);
                else if (tex->m_Format == GXTex_CI8)
                    GXInitTexObjFilter(&glx_texobj[texnum], GX_LIN_MIP_NEAR, GX_LINEAR);
                else
                {
                    static GXAnisotropy aniso[] = { GX_ANISO_1, GX_ANISO_2, GX_ANISO_4 };
                    // PORT: the one line this control was missing.
                    if (glx_aniso < 0)
                        glx_aniso = PortTextureAnisoIndex();
                    GXInitTexObjFilter(&glx_texobj[texnum], GX_LIN_MIP_LIN, GX_LINEAR);
                    GXInitTexObjMaxAniso(&glx_texobj[texnum], aniso[glx_aniso]);
                }
                break;
            }
            case GLTF_Point:
            {
                PlatTexture* tex = (PlatTexture*)glx_texture[texnum];
                GXTexFilter minFilt;
                if (tex == NULL || tex->m_Levels == 1)   // PORT: see above
                    minFilt = GX_NEAR;
                else
                    minFilt = GX_NEAR_MIP_NEAR;
                GXInitTexObjFilter(&glx_texobj[texnum], minFilt, GX_NEAR);
                break;
            }
            }

            glx_texdirty |= (1 << texnum);
            texnum++;
        }
    }
}

static const u32 glv_TexConfigChanged = 0x80;

static inline void glx_SwitchTexture(const glModelPacket* p)
{
    static u32 errorTextures[2] = {
        glGetTexture("global/white"),
        glGetTexture("global/magenta"),
    };
    int bit;
    int texnum;
    PlatTexture* pTex;
    uintptr_t texhandle;

    for (bit = texnum = 0; bit < 6; bit++)
    {
        if (glx_texconfig & (1 << bit))
        {
            texhandle = p->state.texture[bit];
            pTex = glx_GetTex(texhandle, false, prev_view != GLV_Debug);
            // PORT: keep what the packet asked for, the fallback below overwrites texhandle.
            const unsigned long wanted = texhandle;
            const PlatTexture* wantedTex = pTex;
            if (pTex == NULL || pTex->m_bMissingTexture)
            {
                // PORT: this is the quiet one. The fallback usually resolves, so the frame draws the error texture and nothing says why.
                static const bool bProbe = getenv("STRIKERS_PROBE_TEX") != NULL;
                if (bProbe)
                {
                    static unsigned long fbSeen[64];
                    static int nFbSeen = 0;
                    int iFb;
                    bool bNewFb = true;
                    for (iFb = 0; iFb < nFbSeen; iFb++)
                    {
                        if (fbSeen[iFb] == wanted)
                            bNewFb = false;
                    }
                    if (bNewFb && nFbSeen < 64)
                    {
                        fbSeen[nFbSeen++] = wanted;
                        OSReport("glx_SwitchTexture: 0x%08lx -> error texture "
                                 "(%s; owned=%d tag=0x%08x)\n",
                                 (unsigned long)wanted,
                                 pTex == NULL ? "not in inventory"
                                              : "header says missing",
                                 port_region_owns((const void*)wanted),
                                 port_region_owns((const void*)wanted)
                                     ? *(const u32*)wanted : 0u);
                    }
                }
                texhandle = errorTextures[(glGetCurrentFrame() & 4) >> 2];
                pTex = glx_GetTex(texhandle, true, false);
            }

            // PORT: a texture with no pixel data is as unusable as none at all, glx_CreatePlatTexture leaves the GXTexObj zeroed.
            if (pTex != NULL && pTex->m_SwizzledData == NULL)
                pTex = NULL;

            if (pTex == NULL)
            {
                // PORT: even the missing-texture fallback is unresolved.
                static unsigned long seen[16];
                static int nSeen = 0;
                int iSeen;
                bool bNew = true;
                for (iSeen = 0; iSeen < nSeen; iSeen++)
                {
                    if (seen[iSeen] == wanted)
                        bNew = false;
                }
                if (bNew && nSeen < 16)
                {
                    seen[nSeen++] = wanted;
                    OSReport("glx_SwitchTexture: wanted texture 0x%08lx "
                             "(%s), fallback 0x%08lx also unusable; "
                             "skipping stage %d\n",
                             (unsigned long)wanted,
                             wantedTex == NULL ? "not in inventory"
                                 : (wantedTex->m_bMissingTexture
                                        ? "marked missing"
                                        : "no pixel data"),
                             (unsigned long)texhandle, texnum);
                }
                texnum++;
                continue;
            }

            memcpy(&glx_texobj[texnum], &pTex->m_TexObj, sizeof(GXTexObj));
            if (pTex->m_nPaletteEntries != 0)
            {
                memcpy(&glx_tlutobj[texnum], &pTex->m_TlutObj, sizeof(GXTlutObj));
            }

            glx_texture[texnum] = (uintptr_t)pTex;
            glx_texdirty |= 1 << texnum;
            texnum++;
        }
    }

    glx_SwitchTextureState(p);
}

/**
 * Offset/Address/Size: 0x2434 | 0x801BBF34 | size: 0x25C
 */
static void glx_SwitchRaster(const glModelPacket* p)
{
    static _GXCompare gx_DepthFunc[] = {
        GX_ALWAYS,
        GX_LEQUAL,
        GX_EQUAL,
        GX_LESS,
    };
    static _GXCompare gx_AlphaTest[] = {
        GX_ALWAYS,
        GX_GREATER,
    };
    static _GXCullMode gx_Culling[] = {
        GX_CULL_NONE,
        GX_CULL_FRONT,
        GX_CULL_BACK,
        GX_CULL_ALL,
    };

    unsigned long DepthTest;
    unsigned long DepthWrite;
    int DepthFunc;
    int AlphaTest;
    unsigned long AlphaTestRef;
    unsigned long AlphaBlend;
    int Culling;
    int ColourWrite;

    glUnHandleizeRasterState(p->state.raster);

    DepthTest = glGetRasterState(GLS_DepthTest);
    DepthWrite = glGetRasterState(GLS_DepthWrite);
    DepthFunc = glGetRasterState(GLS_DepthFunc);
    gxSetZMode((bool)DepthTest, gx_DepthFunc[DepthFunc], (bool)DepthWrite);

    AlphaTest = glGetRasterState(GLS_AlphaTest);
    AlphaTestRef = glGetRasterState(GLS_AlphaTestRef);
    gxSetAlphaCompare(gx_AlphaTest[AlphaTest], (unsigned char)AlphaTestRef);

    if (AlphaTest != 0)
    {
        gxSetZCompLoc(false);
    }
    else
    {
        gxSetZCompLoc(true);
    }

    AlphaBlend = glGetRasterState(GLS_AlphaBlend);

    switch (AlphaBlend)
    {
    case 0:
        gxSetBlendMode(false, (_GXBlendFactor)1, (_GXBlendFactor)0, false);
        break;
    case 1:
        gxSetBlendMode(true, (_GXBlendFactor)4, (_GXBlendFactor)5, false);
        break;
    case 2:
        gxSetBlendMode(true, (_GXBlendFactor)1, (_GXBlendFactor)1, false);
        break;
    case 3:
        gxSetBlendMode(true, (_GXBlendFactor)4, (_GXBlendFactor)1, false);
        break;
    case 4:
        gxSetBlendMode(true, (_GXBlendFactor)2, (_GXBlendFactor)0, false);
        break;
    case 5:
        gxSetBlendMode(true, (_GXBlendFactor)3, (_GXBlendFactor)1, false);
        break;
    case 6:
        gxSetBlendMode(true, (_GXBlendFactor)1, (_GXBlendFactor)0, false);
        break;
    case 7:
        gxSetBlendMode(true, (_GXBlendFactor)2, (_GXBlendFactor)0, true);
        break;
    }

    Culling = glGetRasterState(GLS_Culling);
    gxSetCullMode(gx_Culling[Culling]);

    ColourWrite = glGetRasterState(GLS_ColourWrite);
    switch (ColourWrite)
    {
    case 0:
        gxSetColourUpdate(false);
        gxSetAlphaUpdate(false);
        break;
    case 1:
        gxSetColourUpdate(true);
        gxSetAlphaUpdate(false);
        break;
    case 2:
        gxSetColourUpdate(false);
        gxSetAlphaUpdate(true);
        break;
    case 3:
        gxSetColourUpdate(true);
        if (prev_view == GLV_ShadowTexture)
        {
            gxSetAlphaUpdate(true);
        }
        else
        {
            gxSetAlphaUpdate(false);
        }
        break;
    }
}

/**
 * Offset/Address/Size: 0x22A0 | 0x801BBDA0 | size: 0x194
 */
static void glx_SwitchStreams(const glModelPacket* pPacket)
{
    static u32 gx_streams[] = {
        9,
        10,
        11,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        26,
        26,
        26,
        26,
        26,
        26,
    };

    glModelStream* stream = pPacket->streams;
    glModelStream* end = stream + pPacket->numStreams;

    GXClearVtxDesc();
    glx_NumIndices = 0;

    while (stream < end)
    {
        if (stream->id == 12)
        {
            GXSetVtxDesc(GX_VA_PNMTXIDX, GX_DIRECT);
            glx_NumIndices++;
            stream++;
            continue;
        }

        s32 attr = gx_streams[stream->id];
        if (attr == 0xFF)
        {
            attr = gx_texattr[stream->id - 3];
        }

        if (attr != 0xFF)
        {
            if (stream->address == 0)
            {
                GXSetVtxDesc((GXAttr)attr, GX_DIRECT);
            }
            else   // PORT: the bound is the stream's own array length, not this packet's vertex count, packets share a model-wide pool.
            {
                // PORT: The fallback below is the wrong bound; report anyone still relying on it.
                if (stream->dataSize == 0 && getenv("STRIKERS_PROBE_ARRAY"))
                {
                    OSReport("[array] dataSize=0 streamId=%d stride=%d "
                             "numVertices=%d beData=%d\n",
                             (int)stream->id, (int)stream->stride,
                             (int)pPacket->numVertices, (int)stream->beData);
                }
                GXSETARRAY((GXAttr)attr, (void*)stream->address,
                           stream->dataSize != 0
                               ? stream->dataSize
                               : (u32)stream->stride * pPacket->numVertices,
                           stream->stride, stream->beData == 0);
                GXSetVtxDesc((GXAttr)attr, GX_INDEX16);
                glx_NumIndices++;
            }
        }

        if (stream->id == 1)
        {
            if (stream->stride == 12)
            {
                GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
            }
            else
            {
                GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_S8, 6);
            }
        }

        if (stream->id == 0)
        {
            if (stream->stride == 12)
            {
                GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
            }
            else
            {
                GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 8);
            }
        }

        stream++;
    }
}

/**
 * Offset/Address/Size: 0x1F6C | 0x801BBA6C | size: 0x334
 */
static void glx_LoadLight(GLLightUserData* pLight, _GXLightID lightId)
{
    static float refMult = 0.666f;
    static float refBright = 0.5f;
    static int refFunc = 2;
    static GXDistAttnFn dist_func[3] = {
        GX_DA_GENTLE,
        GX_DA_MEDIUM,
        GX_DA_STEEP,
    };

    GXLightObj light;
    nlVector3 viewPos;
    GXColor colour;
    nlVector3 viewDir;
    nlVector3 worldDir;

    colour.r = nlFloatColourToByte(pLight->colour.c[0] * pLight->intensity);
    colour.g = nlFloatColourToByte(pLight->colour.c[1] * pLight->intensity);
    colour.b = nlFloatColourToByte(pLight->colour.c[2] * pLight->intensity);
    colour.a = nlFloatColourToByte(pLight->colour.c[3] * pLight->intensity);

    GXInitLightColor(&light, colour);

    if (0.0f == pLight->outerRadius)
    {
        nlVector3 origin = {
            0.0f,
            0.0f,
            0.0f,
        };

        float worldY = pLight->worldPosition.y - origin.y;
        float worldX = pLight->worldPosition.x - origin.x;
        float worldZ = pLight->worldPosition.z - origin.z;

        worldDir.x = worldX;
        worldDir.y = worldY;
        worldDir.z = worldZ;

        {
            float lengthSq = worldDir.GetLengthSq3D();
            float recipLength = nlRecipSqrt(lengthSq, true);

            nlVec3Scale(worldDir, recipLength);
        }

        nlMultDirVectorMatrix(viewDir, worldDir, mview);

        nlVec3Scale(viewDir, 1048576.0f);

        GXInitLightPos(&light, viewDir.x, viewDir.y, viewDir.z);
        GXInitLightAttnA(&light, 1.0f, 0.0f, 0.0f);
        GXInitLightDistAttn(&light, 1048576.0f, 1.0f, GX_DA_OFF);
    }
    else
    {
        nlMultPosVectorMatrix(viewPos, pLight->worldPosition, mview);
        GXInitLightPos(&light, viewPos.x, viewPos.y, viewPos.z);
        GXInitLightAttnA(&light, 1.0f, 0.0f, 0.0f);
        GXInitLightDistAttn(&light, refMult * pLight->outerRadius, refBright, dist_func[refFunc]);
    }

    GXLoadLightObjImm(&light, lightId);
}

static inline void glx_LoadDirectionalLight(GLDirectionalLightUserData* pLight, GXLightID lightID)
{
    GXLightObj light;
    GXColor colour;
    nlVector3 viewDir;

    colour.r = nlFloatColourToByte(pLight->colour.c[0]);
    colour.g = nlFloatColourToByte(pLight->colour.c[1]);
    colour.b = nlFloatColourToByte(pLight->colour.c[2]);
    colour.a = nlFloatColourToByte(pLight->colour.c[3]);

    GXInitLightColor(&light, colour);

    nlMultDirVectorMatrix(viewDir, pLight->direction, mview);
    nlVec3Scale(viewDir, 1048576.0f);
    GXInitLightPos(&light, viewDir.x, viewDir.y, viewDir.z);

    GXInitLightAttnA(&light, 1.0f, 0.0f, 0.0f);
    GXInitLightDistAttn(&light, 1048576.0f, 1.0f, GX_DA_OFF);
    GXLoadLightObjImm(&light, lightID);
}

static inline void glx_LoadSpecular(GLSpecularUserData* pLight, GXLightID lightID)
{
    static float SpecularFudge = 1.25f;
    GXLightObj light;
    nlVector3 worldDir;
    nlVector3 viewDir;
    GXColor colour;

    {
        float recipLength = nlRecipSqrt(
            pLight->worldDirection.x * pLight->worldDirection.x + pLight->worldDirection.y * pLight->worldDirection.y + pLight->worldDirection.z * pLight->worldDirection.z,
            false);
        nlVec3Set(worldDir, recipLength * pLight->worldDirection.x, recipLength * pLight->worldDirection.y, recipLength * pLight->worldDirection.z);
    }

    nlMultDirVectorMatrix(viewDir, worldDir, mview);

    {
        float recipLength = nlRecipSqrt(
            viewDir.x * viewDir.x + viewDir.y * viewDir.y + viewDir.z * viewDir.z,
            false);

        nlVec3Set(viewDir, recipLength * viewDir.x, recipLength * viewDir.y, recipLength * viewDir.z);
    }

    colour.r = nlFloatColourToByte(pLight->colour.c[0] * pLight->intensity * SpecularFudge);
    colour.g = nlFloatColourToByte(pLight->colour.c[1] * pLight->intensity * SpecularFudge);
    colour.b = nlFloatColourToByte(pLight->colour.c[2] * pLight->intensity * SpecularFudge);
    colour.a = nlFloatColourToByte(pLight->colour.c[3] * pLight->intensity * SpecularFudge);

    GXInitLightColor(&light, colour);
    GXInitSpecularDir(&light, viewDir.x, viewDir.y, viewDir.z);

    {
        float half = 0.5f;
        float halfExponent = pLight->exponent * half;
        GXInitLightAttn(&light, 0.0f, 0.0f, 1.0f, halfExponent, 0.0f, 1.0f - halfExponent);
    }

    GXLoadLightObjImm(&light, lightID);
}

static inline void glud_Ambient(void* pData)
{
    nlFloatColour* pColour = (nlFloatColour*)pData;
    nlColour colour;
    colour.c[0] = (u8)(255.0f * pColour->c[0]);
    colour.c[1] = (u8)(255.0f * pColour->c[1]);
    colour.c[2] = (u8)(255.0f * pColour->c[2]);
    colour.c[3] = (u8)(255.0f * pColour->c[3]);
    gxSetChanAmbColour(0, colour);
}

static inline void glud_Diffuse(void* pData)
{
    nlFloatColour* pColour = (nlFloatColour*)pData;
    nlColour colour;
    colour.c[0] = (u8)(255.0f * pColour->c[0]);
    colour.c[1] = (u8)(255.0f * pColour->c[1]);
    colour.c[2] = (u8)(255.0f * pColour->c[2]);
    colour.c[3] = (u8)(255.0f * pColour->c[3]);
    gxSetChanMatColour(0, colour);

    u32 lightMask = glx_prevLightMask;
    if (lightMask)
    {
        if (g_bAllowLighting)
        {
            glx_prevLightMask = lightMask;
            GXSetChanCtrl(GX_COLOR0, GX_TRUE, GX_SRC_REG, GX_SRC_REG, lightMask, GX_DF_CLAMP, GX_AF_SPOT);
        }
    }
}

#pragma dont_inline on
/**
 * Offset/Address/Size: 0x1EAC | 0x801BB9AC | size: 0xC0
 */
static void glud_Light(void* pUserData)
{
    static u32 gxLights[4] = { 1, 2, 4, 8 };

    LightData* lightData = (LightData*)pUserData;
    u32 lightMask;
    s32 light_id;
    GLLightUserData* pLight;
    GLLightUserData* pEndLight;

    if (lightData->numLights != 0)
    {
        pLight = (GLLightUserData*)((u8*)pUserData + 4);
        light_id = 0;
        lightMask = 0;
        pEndLight = &pLight[lightData->numLights];
        while (pLight < pEndLight)
        {
            if (light_id >= 4)
            {
                break;
            }
            lightMask |= gxLights[light_id];
            if (glx_ReloadPointLights != 0)
            {
                glx_LoadLight(pLight, (GXLightID)gxLights[light_id]);
            }
            pLight++;
            light_id += 1;
        }

        glx_ReloadPointLights = 0;
        if (g_bAllowLighting != 0)
        {
            glx_prevLightMask = lightMask;
            GXSetChanCtrl(GX_COLOR0, GX_TRUE, GX_SRC_REG, GX_SRC_VTX, lightMask, GX_DF_CLAMP, GX_AF_SPOT);
        }
    }
}
#pragma dont_inline reset

static inline void glud_DirectionalLight(void* pData)
{
    static GXLightID gxLights[4] = { GX_LIGHT0, GX_LIGHT1, GX_LIGHT2, GX_LIGHT3 };

    // PORT: the count is a u32, glUserAlloc sizes the block as count * sizeof(record) + 4.
    const u32* p32 = (const u32*)pData;
    unsigned long numLights = *p32;
    unsigned long lightMask;
    int index;
    GLDirectionalLightUserData* pLight;
    GLDirectionalLightUserData* pEndLight;

    glx_ReloadPointLights = true;
    if (numLights != 0)
    {
        pLight = (GLDirectionalLightUserData*)((u8*)pData + 4);
        index = 0;
        lightMask = 0;
        pEndLight = &pLight[numLights];

        while (pLight < pEndLight)
        {
            if (index >= 4)
            {
                break;
            }

            lightMask |= gxLights[index];
            glx_LoadDirectionalLight(pLight++, (GXLightID)gxLights[index++]);
        }

        if (g_bAllowLighting != 0)
        {
            glx_prevLightMask = lightMask;
            GXSetChanCtrl(GX_COLOR0, GX_TRUE, GX_SRC_REG, GX_SRC_VTX, lightMask, GX_DF_CLAMP, GX_AF_SPOT);
        }
    }
}

/**
 * Offset/Address/Size: 0x1B8C | 0x801BB68C | size: 0x320
 */
static void glud_Specular(void* pData)
{
    static u32 gxLights[4] = { GX_LIGHT4, GX_LIGHT5, GX_LIGHT6, GX_LIGHT7 };

    // PORT: the count is a u32, glUserAlloc sizes the block as count * sizeof(record) + 4.
    const u32* p32 = (const u32*)pData;
    unsigned long numLights = *p32;
    unsigned long lightMask;
    int index;
    GLSpecularUserData* pLight;
    GLSpecularUserData* pEndLight;

    if (numLights != 0)
    {
        pLight = (GLSpecularUserData*)((unsigned char*)pData + 4);
        index = 0;
        lightMask = 0;
        pEndLight = &pLight[numLights];

        while (pLight < pEndLight)
        {
            if (index >= 4)
            {
                break;
            }

            GXLightID lightID = (GXLightID)gxLights[index];
            lightMask |= lightID;

            if (glx_ReloadSpecLights)
            {
                glx_LoadSpecular(pLight, lightID);
            }

            pLight++;
            index += 1;
        }

        glx_ReloadSpecLights = 0;
        if (g_bAllowSpecular)
        {
            glx_prevSpecMask = lightMask;
            gxSetNumChans(2);
            GXSetChanCtrl(GX_COLOR1, GX_TRUE, GX_SRC_REG, GX_SRC_REG, lightMask, GX_DF_NONE, GX_AF_SPEC);
        }
    }
}

static inline void glx_TextureSwapMode(bool bSwap)
{
    static bool bEnabled = false;

    if (bSwap)
    {
        GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP3);
        bEnabled = true;
    }
    else if (bEnabled)
    {
        GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
        gxSetTevAlphaOp(0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
        gxSetTevAlphaOp(1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
        bEnabled = false;
    }
}

static inline void glud_ShadowVolume(void* pData)
{
    if (*(s32*)pData == 3)
    {
        static GXColor c0 = {
            64,
            64,
            64,
            64,
        };

        glx_DirtyFlags = 0x80;
        glx_TextureSwapMode(true);
        gxSetNumTevStages(3);

        gxSetTevAlphaOp(0, (_GXTevOp)0xE, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)0);
        gxSetTevAlphaOp(1, (_GXTevOp)1, (_GXTevBias)0, (_GXTevScale)0, true, (_GXTevRegID)0);

        GXSetTevColorIn((GXTevStageID)0, (GXTevColorArg)0xF, (GXTevColorArg)0xF, (GXTevColorArg)0xF, (GXTevColorArg)4);
        GXSetTevColorIn((GXTevStageID)1, (GXTevColorArg)0xF, (GXTevColorArg)0xF, (GXTevColorArg)0xF, (GXTevColorArg)0);
        GXSetTevColorIn((GXTevStageID)2, (GXTevColorArg)0xF, (GXTevColorArg)0xF, (GXTevColorArg)0xF, (GXTevColorArg)0);

        gxSetTevAlphaIn(0, (_GXTevAlphaArg)4, (_GXTevAlphaArg)1, (_GXTevAlphaArg)6, (_GXTevAlphaArg)7);
        gxSetTevAlphaIn(1, (_GXTevAlphaArg)7, (_GXTevAlphaArg)6, (_GXTevAlphaArg)0, (_GXTevAlphaArg)6);
        gxSetTevAlphaIn(2, (_GXTevAlphaArg)7, (_GXTevAlphaArg)0, (_GXTevAlphaArg)2, (_GXTevAlphaArg)7);

        GXSetTevColor((GXTevRegID)1, c0);
        GXSetTevColor((GXTevRegID)2, rshadow_colour[prev_view == GLV_ShadowBlend1]);
    }
}

static inline void glud_EnvDiffuse(bool bOn)
{
    Mtx texs, text, invMat, envMat;

    if (bOn)
    {
        PSMTXScale(texs, 0.5f, -0.5f, 0.0f);
        PSMTXTrans(text, 0.5f, 0.5f, 1.0f);
        PSMTXConcat(text, texs, envMat);
        GXLoadTexMtxImm(envMat, 0x5B, GX_MTX3x4);

        PSMTXInvXpose(gx_modelview, invMat);
        GXLoadTexMtxImm(invMat, 0x39, GX_MTX3x4);

        u32 value = glx_GlossMapCoord;
        GXTevStageID stage = (GXTevStageID)glx_GlossMapStage;
        GXSetTexCoordGen2((GXTexCoordID)value, GX_TG_MTX3x4, GX_TG_NRM, 0x39, GX_TRUE, 0x5B);
        GXSetTevOrder(stage, (GXTexCoordID)value, (GXTexMapID)value, GX_COLOR0A0);

        if (glx_texconfig & 0x20)
        {
            GXSetTevColorIn(
                stage, (GXTevColorArg)0xF, (GXTevColorArg)0xB, (GXTevColorArg)8, (GXTevColorArg)0xF);
        }
        else
        {
            GXSetTevColorIn(
                stage, (GXTevColorArg)0xF, (GXTevColorArg)0xB, (GXTevColorArg)8, (GXTevColorArg)0);
        }
    }
    else
    {
        if (glx_GlossMapStage >= 0)
        {
            gxSetTexCoordGen(
                glx_GlossMapStage, GX_TG_MTX2x4, (GXTexGenSrc)(glx_GlossMapStage + 4), GX_IDENTITY);
            glx_DirtyFlags |= glv_TexConfigChanged;
        }
    }
}

static inline void glud_MobileDiffuse(bool bOn)
{
    Mtx texmtx;
    Mtx texs;
    Mtx text;

    if (bOn)
    {
        gxSetTexCoordGen(0, GX_TG_MTX3x4, GX_TG_NRM, 0x39);
        memcpy(texmtx, gx_modelview, sizeof(Mtx));
        PSMTXScale(texs, 0.5f, -0.5f, 0.0f);
        PSMTXTrans(text, 0.5f, 0.5f, 1.0f);
        PSMTXConcat(texs, texmtx, texmtx);
        PSMTXConcat(text, texmtx, texmtx);

        static int n = 5;
        float fTransScale = 1.0f / (float)n;
        u32 frame = glGetCurrentFrame();
        u32 frameDiv = frame / n;
        u32 frameMod = frame - frameDiv * n;
        float fTrans = fTransScale * frameMod;
        fTrans = 2.0f * fTrans - 1.0f;
        texmtx[0][2] = fTrans;
        texmtx[1][2] = fTrans;

        GXLoadTexMtxImm(texmtx, 0x39, GX_MTX3x4);
    }
    else
    {
        gxSetTexCoordGen(0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    }
}

/**
 * Offset/Address/Size: 0x19BC | 0x801BB4BC | size: 0x1D0
 */
static void glud_Skin(void* pData, const glModelPacket* pPacket)
{
    float mSkinConcat[3][4];
    float mNormFast[3][4];
    nlMatrix4 nlMat;
    float modelViewMtx[3][4];
    float gxMat[3][4];
    float mNorm[3][4];
    u32 numMatrices;
    u32 i;
    int slot;
    GLSkinUserData* pSkin;
    u8 bInvXpose;

    numMatrices = *(u32*)pData;
    pSkin = (GLSkinUserData*)((u8*)pData + 4);

    if (prev_view == GLV_Characters)
        bInvXpose = glx_InvXposeChar;
    else
        bInvXpose = glx_InvXpose;

    if (g_bFastSkinPath && g_bMtxSkinMath && !glx_IsCoPlanarView && pPacket->state.matrix == glGetIdentityMatrix())
    {
        for (i = 0; i < numMatrices; i++, pSkin++)
        {
            PSMTXConcat(gx_mview, *(const Mtx*)pSkin->mat, mSkinConcat);
            slot = pSkin->reg + 99;
            GXLoadPosMtxImm(mSkinConcat, (u32)slot);
            if (bInvXpose)
            {
                PSMTXInvXpose(mSkinConcat, mNormFast);
                GXLoadNrmMtxImm(mNormFast, (u32)slot);
            }
            else
            {
                GXLoadNrmMtxImm(mSkinConcat, (u32)slot);
            }
        }
    }
    else
    {
        for (i = 0; i < numMatrices; i++, pSkin++)
        {
            if (g_bMtxSkinMath && !glx_IsCoPlanarView)
            {
                glxCopyMatrix(gxMat, *(nlMatrix4*)pPacket->state.matrix);
                PSMTXConcat(gxMat, gx_mview, modelViewMtx);
                PSMTXConcat(modelViewMtx, *(const Mtx*)pSkin->mat, mSkinConcat);
            }
            else
            {
                nlMultMatrices(nlMat, *(nlMatrix4*)pPacket->state.matrix, mview);
                glxCopyMatrix(modelViewMtx, nlMat);
                PSMTXConcat(modelViewMtx, *(const Mtx*)pSkin->mat, mSkinConcat);
            }
            slot = pSkin->reg + 99;
            GXLoadPosMtxImm(mSkinConcat, (u32)slot);
            if (bInvXpose)
            {
                PSMTXInvXpose(mSkinConcat, mNorm);
                GXLoadNrmMtxImm(mNorm, (u32)slot);
            }
            else
            {
                GXLoadNrmMtxImm(mSkinConcat, (u32)slot);
            }
        }
    }
}

static inline void glud_ConstantColour(void* pData)
{
    GXSetTevColor(GX_TEVREG2, *(GXColor*)pData);
}

static inline void glud_Translucent(void* pData)
{
    int alpha = (int)(*(float*)pData * 255.5f);
    if (alpha < 0)
    {
        alpha = 0;
    }
    if (alpha > 255)
    {
        alpha = 255;
    }

    GXColor c = {
        (u8)alpha,
        (u8)alpha,
        (u8)alpha,
        (u8)alpha,
    };
    GXSetTevColor((GXTevRegID)3, c);
    glx_translucent = true;
}

static inline void glud_NoRasterizedAlpha()
{
    GXColor c = {
        255,
        255,
        255,
        255,
    };
    GXSetTevColor((GXTevRegID)3, c);
    glx_norasterizedalpha = true;
}

static inline void glud_Viewport(void* pData)
{
    GLViewportUserData* pViewport = (GLViewportUserData*)pData;
    g_viewport.x = pViewport->x;
    g_viewport.y = pViewport->y;
    g_viewport.w = pViewport->w;
    g_viewport.h = pViewport->h;
    g_viewport.view = pViewport->view;
    g_viewport.projection = pViewport->projection;
}

static inline void AdjustViewport(bool bOn)
{
    s32 x, y, w, h;
    nlMatrix4 mProj;
    nlMatrix4 mView;
    Mtx44 proj;
    Mtx view;

    if (bOn)
    {
        x = g_viewport.x;
        y = g_viewport.y;
        w = g_viewport.w;
        h = g_viewport.h;

        memcpy(&mProj, (const void*)g_viewport.projection, sizeof(nlMatrix4));
        memcpy(&mView, (const void*)g_viewport.view, sizeof(nlMatrix4));
        glxCopyMatrix(proj, mProj);
        glxCopyMatrix(view, mView);
        GXLoadPosMtxImm(view, 0);

        _GXProjectionType type;
        if (-1.0f == mProj.e[14])
        {
            type = (_GXProjectionType)0;
        }
        else
        {
            type = (_GXProjectionType)1;
        }
        GXSetProjection(proj, type);
        GXSetCurrentMtx(0);
        GXSetViewport((float)x, (float)y, (float)w, (float)h, 0.0f, 1.0f);
        GXSetScissor(x, y, w, h);
    }
    else
    {
        GXLoadPosMtxImm(gx_modelview, 0);

        _GXProjectionType type;
        if (-1.0f == mproj.e2[3][2])
        {
            type = (_GXProjectionType)0;
        }
        else
        {
            type = (_GXProjectionType)1;
        }
        GXSetProjection(gx_proj, type);
        GXSetCurrentMtx(0);
        // PORT: the frame is PortLogicalFrameWidth() wide, not 640.
        GXSetViewport(0.0f, 0.0f, (f32)PortLogicalFrameWidth(), 448.0f, 0.0f, 1.0f);
        GXSetScissor(0, 0, PortLogicalFrameWidth(), 0x1C0);
    }
}

static inline void glud_Scissor(const GLScissorUserData* pScissor)
{
    u32 xOrig;
    u32 yOrig;
    u32 wd;
    u32 ht;

    if (pScissor == NULL)
    {
        xOrig = 0;
        yOrig = xOrig;
        wd = PortLogicalFrameWidth();   // PORT: not 640
        ht = 448;
    }
    else
    {
        xOrig = pScissor->xOrig;
        yOrig = pScissor->yOrig;
        wd = pScissor->wd;
        ht = pScissor->ht;
    }
    GXSetScissor(xOrig, yOrig, wd, ht);
}

inline void EnableTranslucent(bool enable)
{
    static _GXTevAlphaArg argSaved = (_GXTevAlphaArg)5;
    if (glx_RasterizedAlphaStage >= 0 && glx_RasterizedAlphaArg >= 0)
    {
        if (enable)
        {
            argSaved = (_GXTevAlphaArg)gxSetTevAlphaIn(
                glx_RasterizedAlphaStage,
                glx_RasterizedAlphaArg,
                (glx_texconfig & 0x20) ? (_GXTevAlphaArg)3 : (_GXTevAlphaArg)3);
        }
        else
        {
            gxSetTevAlphaIn(glx_RasterizedAlphaStage, glx_RasterizedAlphaArg, argSaved);
        }
    }
}

inline void EnableNoRasterizedAlpha(bool enable)
{
    static _GXTevAlphaArg argSaved = (_GXTevAlphaArg)5;
    if (glx_RasterizedAlphaStage >= 0 && glx_RasterizedAlphaArg >= 0)
    {
        if (enable)
        {
            argSaved = (_GXTevAlphaArg)gxSetTevAlphaIn(
                glx_RasterizedAlphaStage, glx_RasterizedAlphaArg, (_GXTevAlphaArg)3);
        }
        else
        {
            gxSetTevAlphaIn(glx_RasterizedAlphaStage, glx_RasterizedAlphaArg, argSaved);
        }
    }
}

static inline void setWorldAmbient()
{
    nlColour ambient = getWorldAmbient();
    gxSetChanAmbColour(0, ambient);
}

/**
 * Offset/Address/Size: 0x1058 | 0x801BAB58 | size: 0x964
 */
static void glx_SwitchUserData(const glModelPacket* p)
{
    static bool bDeferredEnvDiffuse = true;
    int i;
    uintptr_t* pTable;   // PORT: void*[GLUD_Num].
    GLViewportUserData* pViewport;
    void* pData;

    if (glx_AlwaysReloadLights)
    {
        glx_ReloadPointLights = true;
        glx_ReloadSpecLights = true;
    }

    setWorldAmbient();

    if (glx_prevLightMask)
    {
        GXSetChanCtrl(GX_COLOR0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, (GXLightID)glx_prevLightMask, GX_DF_NONE, GX_AF_NONE);
        glx_prevLightMask = 0;
    }

    if (glx_prevSpecMask)
    {
        gxSetNumChans(1);
        GXSetChanCtrl(GX_COLOR1, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, (GXLightID)glx_prevSpecMask, GX_DF_NONE, GX_AF_NONE);
        glx_prevSpecMask = 0;
    }

    if (bDeferredEnvDiffuse)
    {
        glx_envdiffuse = false;
    }
    else if (glx_GlossMapStage >= 0)
    {
        gxSetTexCoordGen(glx_GlossMapStage, GX_TG_MTX2x4, (_GXTexGenSrc)(glx_GlossMapStage + 4), GX_IDENTITY);
        glx_DirtyFlags |= glv_TexConfigChanged;
    }

    glx_mobilediffuse = false;
    glx_constantcolour = false;
    glx_viewport = false;
    glx_translucent = false;
    glx_norasterizedalpha = false;
    glx_NoFog = false;
    GXSetScissor(0, 0, PortLogicalFrameWidth(), 448);   // PORT: not 640
    glx_CoPlanar = false;

    if (p == NULL)
    {
        return;
    }

    pTable = (uintptr_t*)p->userData;
    if (pTable == NULL)
    {
        return;
    }

    if (prev_view == GLV_ShadowTexture)
    {
        if (pTable[GLUD_Skin] != 0)
        {
            if (pTable[GLUD_Viewport] != 0)
            {
                pViewport = (GLViewportUserData*)glUserGetData((void*)pTable[GLUD_Viewport]);
                memcpy(&mview, (void*)pViewport->view, sizeof(nlMatrix4));
                glxCopyMatrix(gx_mview, mview);
            }
        }
    }

    for (i = 0; i < GLUD_Num; i++, pTable++)
    {
        if (*pTable == 0)
        {
            continue;
        }
        pData = glUserGetData((void*)*pTable);
        switch (i)
        {
        case GLUD_CoPlanar:
            glx_CoPlanar = true;
            break;

        case GLUD_NoFog:
            glx_NoFog = true;
            break;

        case GLUD_Ambient:
            glud_Ambient(pData);
            break;

        case GLUD_Diffuse:
            glud_Diffuse(pData);
            break;

        case GLUD_Light:
            glud_Light(pData);
            break;

        case GLUD_DirectionalLight:
            glud_DirectionalLight(pData);
            break;

        case GLUD_Specular:
            if (glx_allowSpecular)
            {
                glud_Specular(pData);
            }
            break;

        case GLUD_ShadowVolume:
            glud_ShadowVolume(pData);
            break;

        case GLUD_Translucent:
            glud_Translucent(pData);
            break;

        case GLUD_NoRasterizedAlpha:
            glud_NoRasterizedAlpha();
            break;

        case GLUD_Scissor:
            glud_Scissor((GLScissorUserData*)pData);
            break;

        case GLUD_EnvDiffuse:
            if (glx_allowSpecular)
            {
                if (bDeferredEnvDiffuse)
                {
                    glx_envdiffuse = true;
                }
                else
                {
                    glud_EnvDiffuse(true);
                }
            }
            break;

        case GLUD_MobileDiffuse:
            glx_mobilediffuse = true;
            break;

        case GLUD_Skin:
            glud_Skin(pData, p);
            break;

        case GLUD_ConstantColour:
            glx_constantcolour = true;
            glud_ConstantColour(pData);
            break;

        case GLUD_Viewport:
            glx_viewport = true;
            glud_Viewport(pData);
            break;

        default:
            break;
        }
    }
}
static const u32 ColourTargetTexture = glGetTexture("target/colour");

static inline void force_LoadTexture(int stage, uintptr_t handle)
{
    PlatTexture* pTex = glx_GetTex(handle, true, true);
    glx_texture[stage] = (uintptr_t)pTex;
    memcpy(&glx_texobj[stage], &pTex->m_TexObj, sizeof(GXTexObj));
    GXInitTexObjWrapMode(&glx_texobj[stage], (GXTexWrapMode)0, (GXTexWrapMode)0);
    GXInitTexObjFilter(&glx_texobj[stage], (GXTexFilter)1, (GXTexFilter)1);
    glx_texdirty |= 1 << stage;
}

static inline void _Indirect(bool bOn)
{
    static float indMtx[2][3] = {
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
    };

    if (bOn)
    {
        force_LoadTexture(1, glGetTexture("target/offset"));
        GXSetNumIndStages(1);
        GXSetIndTexOrder((GXIndTexStageID)0, (GXTexCoordID)0, (GXTexMapID)1);
        GXSetIndTexCoordScale((GXIndTexStageID)0, (GXIndTexScale)1, (GXIndTexScale)1);
        GXSetTevIndWarp((GXTevStageID)0, (GXIndTexStageID)0, 1, 0, (GXIndTexMtxID)1);

        float scale = 1.0f / glx_IndDivisor;
        indMtx[0][0] = scale;
        indMtx[1][1] = scale;
        GXSetIndTexMtx((GXIndTexMtxID)1, indMtx, 1);
    }
    else
    {
        GXSetNumIndStages(0);
        GXSetTevDirect((GXTevStageID)0);
    }
}

/**
 * Offset/Address/Size: 0x538 | 0x801BA038 | size: 0xB20
 */
static void glx_DrawPacket(const glModelPacket* packet)
{
    static _GXPrimitive primitives[6] = {
        GX_TRIANGLES,
        GX_TRIANGLESTRIP,
        GX_TRIANGLEFAN,
        GX_QUADS,
        GX_LINES,
        GX_LINESTRIP,
    };

    glModelPacket* p;
    u32 i, j, mask;
    u8 bIndirect;
    u8 bFogWasDisabled;
    _GXTlut tlutID;

    p = (glModelPacket*)packet;
    bFogWasDisabled = false;
    bIndirect = false;

    if ((prev_view == GLV_WarbleBlend) && (p->state.texture[0] == ColourTargetTexture))
    {
        _Indirect(true);
        bIndirect = true;
    }

    if (glx_texdirty != 0)
    {
        for (i = 0; i < 6; i++)
        {
            mask = (1 << i);
            if (glx_texdirty & mask)
            {
                PlatTexture* tex = (PlatTexture*)glx_texture[i];
                if (tex == NULL)
                {
                    // PORT: nothing resolved for this stage, see the guard in glx_SwitchTexture.
                    glx_texdirty &= ~mask;
                    continue;
                }
                if (tex->m_nPaletteEntries != 0)
                {
                    GXInitTexObjTlut(&glx_texobj[i], (GXTlut)i);
                    GXLoadTlut(&glx_tlutobj[i], (GXTlut)i);
                }

                GXLoadTexObj(&glx_texobj[i], (GXTexMapID)i);
                glx_texdirty &= ~mask;
            }
        }
    }

    gxSetCoPlanar(glx_CoPlanar);

    if (glx_NoFog && glx_GetFog())
    {
        bFogWasDisabled = true;
        glx_Fog(false);
    }

    if (glx_envdiffuse)
    {
        glud_EnvDiffuse(true);
    }
    else if (glx_mobilediffuse)
    {
        glud_MobileDiffuse(true);
    }

    if (glx_norasterizedalpha)
    {
        EnableNoRasterizedAlpha(true);
    }
    else if (glx_translucent)
    {
        EnableTranslucent(true);
    }
    else if (glx_constantcolour)
    {
        if (glx_texconfig == 1)
        {
            GXSetTevColorIn((GXTevStageID)0, (GXTevColorArg)15, (GXTevColorArg)6, (GXTevColorArg)8, (GXTevColorArg)15);
            gxSetTevAlphaIn(0, (_GXTevAlphaArg)7, (_GXTevAlphaArg)3, (_GXTevAlphaArg)4, (_GXTevAlphaArg)7);
        }
        else if (glx_texconfig == 0x21)
        {
            GXSetTevColorIn((GXTevStageID)1, (GXTevColorArg)15, (GXTevColorArg)6, (GXTevColorArg)8, (GXTevColorArg)15);
        }
    }

    if (glx_viewport)
    {
        AdjustViewport(true);
    }

    static const bool bDrawProbe = getenv("STRIKERS_PROBE_DRAW") != NULL;
    if (bDrawProbe)
    {
        // Per-frame tally alongside the per-packet dump: how many packets were submitted.
        static unsigned long lastFrame = 0;
        static unsigned long nThisFrame = 0;
        static unsigned long nTextured = 0;
        const unsigned long frame = glGetCurrentFrame();
        if (frame != lastFrame)
        {
            if (nThisFrame != 0)
                OSReport("frame %lu: %lu packets drawn, %lu with a texture\n",
                         lastFrame, nThisFrame, nTextured);
            lastFrame = frame;
            nThisFrame = 0;
            nTextured = 0;
        }
        nThisFrame++;
        if (glx_texture[0] != 0)
            nTextured++;

        static int nProbed = 0;
        if (nProbed < 8)
        {
            nProbed++;
            OSReport("packet %d: %u verts, prim %u, %u streams, idxBuf %p\n",
                     nProbed, (unsigned)p->numVertices, (unsigned)p->primType,
                     (unsigned)p->numStreams, (void*)p->indexBuffer);
            OSReport("    matrix handle %p  owned=%d  identity=%p\n",
                     (void*)p->state.matrix,
                     port_region_owns((const void*)p->state.matrix),
                     (void*)glGetIdentityMatrix());
            const glModelStream* st = p->streams;
            for (unsigned s = 0; s < p->numStreams && st != NULL; s++)
            {
                OSReport("    stream %u: id %u stride %u addr %p",
                         s, (unsigned)st[s].id, (unsigned)st[s].stride,
                         (void*)st[s].address);
                OSReport("\n");
                if (st[s].address != 0 && st[s].stride >= 12)
                {
                    // Both readings, because which one is right is the question: data loaded from a model file is big-endian.
                    for (unsigned v = 0; v < p->numVertices && v < 4; v++)
                    {
                        const unsigned char* q =
                            (const unsigned char*)st[s].address + v * st[s].stride;
                        float nx, ny, nz;
                        memcpy(&nx, q, 4);
                        memcpy(&ny, q + 4, 4);
                        memcpy(&nz, q + 8, 4);
                        OSReport("        v%u  be = %g %g %g   le = %g %g %g\n", v,
                                 (double)port_bef32(q), (double)port_bef32(q + 4),
                                 (double)port_bef32(q + 8),
                                 (double)nx, (double)ny, (double)nz);
                    }
                }
            }
        }
    }

    if (p->indexBuffer == 0)
    {
        GXBegin(primitives[p->primType], (_GXVtxFmt)gx_vtxfmt, p->numVertices);

        for (i = 0; i < p->numVertices; i++)
        {
            for (j = 0; j < glx_NumIndices; j++)
            {
                GXPosition1x16((u16)i);   // PORT: Aurora's FIFO
            }
        }
        GXEnd();   // PORT: a no-op on console, required by Aurora
    }
    else
    {
        if (glx_NumIndices == 0)
        {
            GXCallDisplayList(dlGetDisplayList(p->indexBuffer), dlGetSize(p->indexBuffer));
        }
        else if (glx_CompiledDraw && (glx_NumIndices == p->numStreams) && dlIsDisplayList(p->indexBuffer))
        {
            GXCallDisplayList(dlGetDisplayList(p->indexBuffer), dlGetSize(p->indexBuffer));
        }
        else if (glx_AllowUncompiledDraws && glGetRasterState(p->state.raster, (eGLState)8) != 1)
        {
            if (dlIsDisplayList(p->indexBuffer))
            {
                DisplayList* dl = dlGetStruct(p->indexBuffer);
                GXBegin(primitives[p->primType], (_GXVtxFmt)gx_vtxfmt, p->numVertices);
                for (j = 0; j < p->numVertices; j++)
                {
                    for (i = 0; i < glx_NumIndices; i++)
                    {
                        u16* ptr;
                        if (((u16*)&dl->indices)[1] != 0)
                        {
                            u16 ns = ((u16*)&dl->indices)[0];
                            s32 stride = (ns - 1) * 2 + 1;
                            s32 offset = stride * j;
                            u8* ptr8 = (u8*)dl->list + offset;
                            ptr = (u16*)ptr8;
                            ptr8 = (u8*)ptr;
                            ptr8 += 4;
                            ptr = (u16*)ptr8;
                        }
                        else
                        {
                            u16 ns = ((u16*)&dl->indices)[0];
                            s32 stride = ns * 2;
                            s32 offset = j * stride;
                            u8* ptr8 = (u8*)dl->list + offset;
                            ptr = (u16*)ptr8;
                            ptr8 = (u8*)ptr;
                            ptr8 += 3;
                            ptr = (u16*)ptr8;
                        }
                        // PORT: the display list is big-endian; *ptr would read it host-order and GXPosition1x16 swaps again.
                        GXPosition1x16((u16)(((const u8*)ptr)[0] << 8
                                             | ((const u8*)ptr)[1]));
                    }
                }
                GXEnd();   // PORT: see above
            }
            else
            {
                u16* idxPtr = (u16*)p->indexBuffer;
                GXBegin(primitives[p->primType], (_GXVtxFmt)gx_vtxfmt, p->numVertices);
                for (i = 0; i < p->numVertices; i++)
                {
                    for (j = 0; j < glx_NumIndices; j++)
                    {
                        GXPosition1x16(idxPtr[i]);   // PORT: Aurora's FIFO
                    }
                }
                GXEnd();   // PORT: see above
            }
        }
    }

    if (bIndirect)
    {
        _Indirect(false);
    }

    if (bFogWasDisabled)
    {
        glx_Fog(true);
    }

    if (glx_envdiffuse)
    {
        glud_EnvDiffuse(false);
    }
    else if (glx_mobilediffuse)
    {
        glud_MobileDiffuse(false);
    }

    if (glx_norasterizedalpha)
    {
        EnableNoRasterizedAlpha(false);
    }
    else if (glx_translucent)
    {
        EnableTranslucent(false);
    }
    else if (glx_constantcolour)
    {
        if (glx_texconfig == 1)
        {
            GXSetTevColorIn((GXTevStageID)0, (GXTevColorArg)15, (GXTevColorArg)10, (GXTevColorArg)8, (GXTevColorArg)15);
            gxSetTevAlphaIn(0, (_GXTevAlphaArg)7, (_GXTevAlphaArg)5, (_GXTevAlphaArg)4, (_GXTevAlphaArg)7);
        }
        else
        {
            GXSetTevColorIn((GXTevStageID)1, (GXTevColorArg)15, (GXTevColorArg)10, (GXTevColorArg)8, (GXTevColorArg)15);
        }
    }

    if (glx_viewport)
    {
        AdjustViewport(false);
    }

    GXSetCurrentMtx(0);
}

static inline void glx_SwitchProgram(const glModelPacket* p)
{
    unsigned long program = p->state.program;

    if (glx_program == prog_2d_movie && program != prog_2d_movie)
    {
        GXSetTevKAlphaSel(GX_TEVSTAGE1, GX_TEV_KASEL_1);
    }

    glx_normals = program == prog_3d_pointlit || program == prog_3d_pointlit_dirt || program == prog_3d_crowd_lit;
    glx_program = program;
}

static inline void glx_SwitchViews(eGLView view)
{
    if (view != prev_view)
    {
        prev_view = view;
        glx_IsCoPlanarView = view == GLV_CoPlanar0 || view == GLV_CoPlanar;
        glViewGetProjectionMatrix(view, mproj);
        glViewGetViewMatrix(view, mview);
        glxCopyMatrix(gx_mview, mview);
        glxCopyMatrix(gx_proj, mproj);

        GXProjectionType type;
        if (mproj.e2[3][2] == -1.0f)
        {
            type = GX_PERSPECTIVE;
        }
        else
        {
            type = GX_ORTHOGRAPHIC;
        }
        GXSetProjection(gx_proj, type);
        glx_SwitchUserData(NULL);

        glx_ReloadPointLights = true;
        glx_ReloadSpecLights = true;
        nlColour ambient = getWorldAmbient();
        gxSetChanAmbColour(0, ambient);
        gxSetChanMatColour(0, nlWhite);
        gxSetChanAmbColour(1, nlBlack);
        gxSetChanMatColour(1, nlWhite);
    }
}

static inline void glx_SwitchMatrix(const glModelPacket* p)
{
    uintptr_t matrix = p->state.matrix;
    Mtx mNorm;

    // PORT: an unreplaced handle is still a file offset.
    if (matrix == glGetIdentityMatrix() || !port_region_owns((const void*)matrix))
    {
        if (matrix != glGetIdentityMatrix())
        {
            static bool bReported = false;
            if (!bReported)
            {
                bReported = true;
                OSReport("glx_SwitchMatrix: matrix handle %lu is not a GLMatrix; "
                         "using identity\n", (unsigned long)matrix);
            }
        }
        modelview = mview;
    }
    else
    {
        nlMultMatrices(modelview, *(const nlMatrix4*)matrix, mview);
    }
    glxCopyMatrix(gx_modelview, modelview);
    GXLoadPosMtxImm(gx_modelview, 0);
    if (glx_normals)
    {
        if (glx_InvXpose)
        {
            PSMTXInvXpose(gx_modelview, mNorm);
            GXLoadNrmMtxImm(mNorm, 0);
        }
        else
        {
            GXLoadNrmMtxImm(gx_modelview, 0);
        }
    }
    GXSetCurrentMtx(0);
}

/**
 * Offset/Address/Size: 0x0 | 0x801B9B00 | size: 0x538
 */
void glx_SendFrame_cb(eGLView view, unsigned long flags, const glModelPacket* p)
{
    if (p != NULL)
    {
        if (glx_DirtyFlags != 0)
        {
            glx_TextureSwapMode(false);
            flags |= glx_DirtyFlags;
            glx_DirtyFlags = 0;
        }
    }

    if (flags & 0x7FF)
    {
        if (flags & 0x83)
        {
            if (flags & 1)
            {
                glx_SwitchViews(view);
            }

            if (flags & 2)
            {
                glx_SwitchProgram(p);
            }

            if (flags & 0x80)
            {
                flags |= glx_SwitchTexConfig(p);
            }
        }

        if (flags & 0x100)
        {
            glx_SwitchUserData(p);
        }

        if (flags & 0x14)
        {
            glx_SwitchTexture(p);
        }

        if (flags & 0x08)
        {
            glx_SwitchRaster(p);
        }

        if (flags & 0x20)
        {
            glx_SwitchMatrix(p);
        }

        if (flags & 0x40)
        {
            glx_SwitchStreams(p);
        }
    }

    {
        static const bool bProbeObj = getenv("STRIKERS_PROBE_OBJ") != NULL;
        const char* pkName = NULL;
        int pkIdx = -1;
        if (bProbeObj && p != NULL && (flags & 0x800) &&
            (pkIdx = PortProbeObjIndex(p, (const void*)p->streams,
                                       &pkName)) >= 0 &&
            PortProbeObjShouldLog(glGetCurrentFrame()))
        {
            unsigned int amb[2], mat[2], nchan;
            gxProbeChanState(amb, mat, &nchan);
            unsigned int udmask = 0;
            const unsigned long* tbl = (const unsigned long*)p->userData;
            if (tbl != NULL)
                for (int k = 0; k < GLUD_Num; k++)
                    if (tbl[k] != 0)
                        udmask |= 1u << k;
            OSReport("[probeobj] f=%lu v=%d %s[%d] fl=%03lx prog=%u cfg=%02x "
                     "rast=%08x usk=%08x mset=%u tex0=%p ud=%05x\n",
                     (unsigned long)glGetCurrentFrame(), (int)view, pkName,
                     pkIdx, (unsigned long)flags, (unsigned)p->state.program,
                     (unsigned)p->state.texconfig, (unsigned)p->state.raster,
                     (unsigned)p->state.userStateKey, (unsigned)p->materialset,
                     (void*)p->state.texture[0], udmask);
            OSReport("[probeobj]   amb0=%08x mat0=%08x amb1=%08x mat1=%08x "
                     "nch=%u lm=%lx sm=%lx env=%d cc=%d tr=%d nra=%d nofog=%d "
                     "mob=%d cop=%d\n",
                     amb[0], mat[0], amb[1], mat[1], nchan,
                     (unsigned long)glx_prevLightMask,
                     (unsigned long)glx_prevSpecMask,
                     (int)glx_envdiffuse, (int)glx_constantcolour,
                     (int)glx_translucent, (int)glx_norasterizedalpha,
                     (int)glx_NoFog, (int)glx_mobilediffuse,
                     (int)glx_CoPlanar);
            for (unsigned sIdx = 0; sIdx < p->numStreams; sIdx++)
            {
                const glModelStream* st = &p->streams[sIdx];
                OSReport("[probeobj]   stream id=%u stride=%u be=%u size=%u\n",
                         (unsigned)st->id, (unsigned)st->stride,
                         (unsigned)st->beData, (unsigned)st->dataSize);
                if (st->id == 1 && st->address != 0)
                {
                    const signed char* nb = (const signed char*)st->address;
                    OSReport("[probeobj]   nrm0 be=%u first=%d,%d,%d %d,%d,%d\n",
                             (unsigned)st->beData, nb[0], nb[1], nb[2],
                             nb[3], nb[4], nb[5]);
                }
                if ((st->id == 3 || st->id == 4) && st->address != 0 &&
                    st->stride == 4 && p->indexBuffer == 0)
                {
                    // Direct (non-indexed) UVs: min/max over this packet's own vertex range would need the index buffer.
                }
                if (st->id == 2 && st->address != 0 && st->stride == 4)
                {
                    const unsigned char* cb = (const unsigned char*)st->address;
                    unsigned nClr = st->dataSize / 4;
                    unsigned char mn[4] = { 255, 255, 255, 255 };
                    unsigned char mx[4] = { 0, 0, 0, 0 };
                    for (unsigned ci = 0; ci < nClr && ci < 4096; ci++)
                        for (unsigned ch = 0; ch < 4; ch++)
                        {
                            unsigned char b = cb[ci * 4 + ch];
                            if (b < mn[ch]) mn[ch] = b;
                            if (b > mx[ch]) mx[ch] = b;
                        }
                    OSReport("[probeobj]   clr0 n=%u first=%02x%02x%02x%02x "
                             "%02x%02x%02x%02x min=%02x%02x%02x%02x "
                             "max=%02x%02x%02x%02x\n",
                             nClr, cb[0], cb[1], cb[2], cb[3],
                             nClr > 1 ? cb[4] : 0, nClr > 1 ? cb[5] : 0,
                             nClr > 1 ? cb[6] : 0, nClr > 1 ? cb[7] : 0,
                             mn[0], mn[1], mn[2], mn[3],
                             mx[0], mx[1], mx[2], mx[3]);
                }
            }
            const PlatTexture* tx0 = (const PlatTexture*)glx_texture[0];
            if (tx0 != NULL && tx0->m_Format == 2 && tx0->m_SwizzledData != NULL)
            {
                static int nDumped = 0;
                if (nDumped < 16)
                {
                    nDumped++;
                    PortProbeDumpCMPR("tile", pkIdx, tx0->m_SwizzledData,
                                      tx0->m_Width, tx0->m_Height);
                }
                // Mean colour of the top mip, decoded as big-endian RGB565, what the texels are, as distinct from how they render.
                const unsigned char* px = (const unsigned char*)tx0->m_SwizzledData;
                unsigned long sr = 0, sg = 0, sb = 0;
                unsigned nTx = (unsigned)tx0->m_Width * tx0->m_Height;
                if (nTx > 4096) nTx = 4096;
                for (unsigned ti = 0; ti < nTx; ti++)
                {
                    unsigned v = ((unsigned)px[ti * 2] << 8) | px[ti * 2 + 1];
                    sr += (v >> 11) & 0x1F;
                    sg += (v >> 5) & 0x3F;
                    sb += v & 0x1F;
                }
                OSReport("[probeobj]   tex0 mip0 avg565 be r=%lu g=%lu b=%lu "
                         "of31/63/31 first=%02x%02x%02x%02x\n",
                         sr / nTx, sg / nTx, sb / nTx,
                         px[0], px[1], px[2], px[3]);
            }
            if (tx0 != NULL)
                OSReport("[probeobj]   tex0 fmt=%d %ux%u lv=%u pal=%d bits=%u,%u,%u,%u\n",
                         (int)tx0->m_Format, (unsigned)tx0->m_Width,
                         (unsigned)tx0->m_Height, (unsigned)tx0->m_Levels,
                         (int)tx0->m_nPaletteEntries,
                         (unsigned)tx0->m_Bits[0], (unsigned)tx0->m_Bits[1],
                         (unsigned)tx0->m_Bits[2], (unsigned)tx0->m_Bits[3]);
            // Every bound texture slot, in TEV map order: the SRTG light ramp is the highest active bit.
            for (int bit2 = 0, tn2 = 0; bit2 < 6; bit2++)
            {
                if (!(p->state.texconfig & (1u << bit2)))
                    continue;
                const PlatTexture* tp = (const PlatTexture*)glx_texture[tn2];
                unsigned long h2 = (unsigned long)p->state.texture[bit2];
                if (tp != NULL)
                {
                    const unsigned char* d2 =
                        (const unsigned char*)tp->m_SwizzledData;
                    OSReport("[probeobj]   texmap%d (bit%d) h=%08lx tp=%p "
                             "fmt=%d %ux%u lv=%u pal=%d data=%02x%02x%02x%02x"
                             "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                             tn2, bit2, h2, (const void*)tp, (int)tp->m_Format,
                             (unsigned)tp->m_Width, (unsigned)tp->m_Height,
                             (unsigned)tp->m_Levels, (int)tp->m_nPaletteEntries,
                             d2 ? d2[0] : 0, d2 ? d2[1] : 0, d2 ? d2[2] : 0,
                             d2 ? d2[3] : 0, d2 ? d2[4] : 0, d2 ? d2[5] : 0,
                             d2 ? d2[6] : 0, d2 ? d2[7] : 0, d2 ? d2[8] : 0,
                             d2 ? d2[9] : 0, d2 ? d2[10] : 0, d2 ? d2[11] : 0,
                             d2 ? d2[12] : 0, d2 ? d2[13] : 0, d2 ? d2[14] : 0,
                             d2 ? d2[15] : 0);
                    if (tp->m_Format == 2 && tp->m_SwizzledData != NULL)
                    {
                        static int nSlotDump = 0;
                        if (nSlotDump < 24)
                        {
                            nSlotDump++;
                            char stag[16];
                            stag[0] = 's'; stag[1] = 'l'; stag[2] = 'o';
                            stag[3] = 't'; stag[4] = (char)('0' + tn2);
                            stag[5] = '\0';
                            PortProbeDumpCMPR(stag, pkIdx, tp->m_SwizzledData,
                                              tp->m_Width, tp->m_Height);
                        }
                    }
                    if (tp->m_nPaletteEntries > 0 && tp->m_PaletteData != NULL)
                    {
                        const unsigned short* pp = tp->m_PaletteData;
                        OSReport("[probeobj]     pal[0..7]=%04x %04x %04x %04x "
                                 "%04x %04x %04x %04x\n",
                                 pp[0], pp[1], pp[2], pp[3],
                                 pp[4], pp[5], pp[6], pp[7]);
                    }
                }
                else
                {
                    OSReport("[probeobj]   texmap%d (bit%d) h=%08lx UNBOUND\n",
                             tn2, bit2, h2);
                }
                tn2++;
            }
            if (tbl != NULL)
            {
                for (int k = 0; k < GLUD_Num; k++)
                {
                    if (tbl[k] == 0)
                        continue;
                    const void* pd = glUserGetData((void*)tbl[k]);
                    if (k == GLUD_Ambient || k == GLUD_Diffuse)
                    {
                        const nlFloatColour* fc = (const nlFloatColour*)pd;
                        OSReport("[probeobj]   ud%d %s = %.3f %.3f %.3f %.3f\n",
                                 k, k == GLUD_Ambient ? "amb" : "dif",
                                 fc->c[0], fc->c[1], fc->c[2], fc->c[3]);
                    }
                    else if (k == GLUD_Light)
                    {
                        unsigned n = *(const u32*)pd;
                        const GLLightUserData* L =
                            (const GLLightUserData*)((const u8*)pd + 4);
                        OSReport("[probeobj]   ud%d nlights=%u pos=%.2f,%.2f,%.2f "
                                 "col=%.2f,%.2f,%.2f int=%.2f inr=%.2f outr=%.2f\n",
                                 k, n, L->worldPosition.x, L->worldPosition.y,
                                 L->worldPosition.z, L->colour.c[0], L->colour.c[1],
                                 L->colour.c[2], L->intensity, L->innerRadius,
                                 L->outerRadius);
                    }
                    else if (k == GLUD_DirectionalLight || k == GLUD_Specular)
                    {
                        OSReport("[probeobj]   ud%d nlights=%u\n", k,
                                 (unsigned)*(const u32*)pd);
                    }
                    else if (k == GLUD_ConstantColour)
                    {
                        OSReport("[probeobj]   ud%d const=%08x\n", k,
                                 *(const u32*)pd);
                    }
                    else if (k == GLUD_Translucent)
                    {
                        OSReport("[probeobj]   ud%d transl=%.3f\n", k,
                                 (double)*(const float*)pd);
                    }
                }
            }
        }
    }

    if (flags & 0x800)
    {
        glx_DrawPacket(p);
    }
}

const u32 glv_MatrixChanged = 0x20;
