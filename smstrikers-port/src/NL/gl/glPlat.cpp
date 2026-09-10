#include "NL/nlDebug.h"
#include "NL/nlMemory.h"
#include "NL/nlConfig.h"
#include "NL/gl/glState.h"
#include "NL/gl/glPlat.h"
#include "port/aspect.h"
#include "NL/gl/glConstant.h"
#include "NL/gl/glFont.h"
#include "NL/gl/glDraw2.h"
#include "NL/gl/glTarget.h"
#include "NL/gl/glAppAttach.h"
#include "NL/gl/glRenderList.h"
#include "NL/glx/glxSwap.h"
#include "NL/glx/glxMemory.h"
#include "NL/glx/glxTarget.h"
#include "NL/glx/glxSend.h"
#include "dolphin/vi/vifuncs.h"
#include <dolphin/vi.h>   // PORT: VILockAspectRatio
#include "dolphin/gx/GXTransform.h"
#include "dolphin/gx/GXCull.h"
#include "dolphin/gx/GXFrameBuffer.h"
#include "dolphin/gx/GXPixel.h"
#include "dolphin/gx/GXManage.h"
#include "dolphin/gx/GXPerf.h"
#include "dolphin/os/OSCache.h"
#include "dolphin/os/OSThread.h"
#include "dolphin/os/OSReset.h"
#include "dolphin/vm/VM.h"
#include "Game/Sys/debug.h"

// PAL 480i deflicker render mode (first symbol in .data)
static GXRenderModeObj glPal480IntDf = { VI_TVMODE_PAL_INT,
    640,
    480,
    542,
    40,
    16,
    640,
    542,
    VI_XFBMODE_DF,
    0,
    0,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 8, 8, 10, 12, 10, 8, 8 } };

static GXRenderModeObj glx_rmode;

// Declaration order below mirrors the original TU: the .sdata (initialised)
// and .sbss (zero) symbols are interleaved exactly as MWCC laid them out.
static bool glx_bProgressiveMode = false;
static bool glx_Virt;
static bool glx_Perf;
static bool glx_PerfSync;
static GXPerf0 glx_perf0 = GX_PERF0_TRIANGLES;
static GXPerf1 glx_perf1 = GX_PERF1_VERTICES;
static s32 glx_ViewFence = -1;
static u32 glx_NumVirtMisses = 0;
static u32 glx_VirtLatency = 0;
static bool glx_bFogAdjust = true;
static bool glx_bFog = false;
static u32 glx_FogType = 4;
static bool glx_bFogSky = true;
static f32 glx_FogStart = 5.f;
static f32 glx_FogEnd = 160.f;
static f32 glx_FogIntensity = 1.f;
static GXColor glx_FogColour = { 0xFF, 0xFF, 0xFF, 0xFF };
static s32 prev_VIWidth = 0x00000294;
static s32 glx_VIWidth = 0x00000294;
static u32 glx_FIFOSize = 393216;
static eVideoMode glx_VideoMode;
static void* glx_FIFOMem = nullptr;
static GXFifoObj* glx_FIFO = nullptr;
static f32 glx_CopyDispScaleFactor = 1.f;
static u32 glx_TargetFPS = 60;
static void* glx_FrameBuffer[2];
static u32 total_val0 = 0;
static u32 total_val1 = 0;
static s32 glx_FBSize;

// Performance metric string array
static const char* str_perf0[]
    = { "VERTICES", "CLIP_VTX", "CLIP_CLKS", "XF_WAIT_IN", "XF_WAIT_OUT", "XF_XFRM_CLKS", "XF_LIT_CLKS", "XF_BOT_CLKS", "XF_REGLD_CLKS", "XF_REGRD_CLKS", "CLIP_RATIO", "TRIANGLES", "TRIANGLES_CULLED", "TRIANGLES_PASSED", "TRIANGLES_SCISSORED", "TRIANGLES_0TEX", "TRIANGLES_1TEX", "TRIANGLES_2TEX", "TRIANGLES_3TEX", "TRIANGLES_4TEX", "TRIANGLES_5TEX", "TRIANGLES_6TEX", "TRIANGLES_7TEX", "TRIANGLES_8TEX", "TRIANGLES_0CLR", "TRIANGLES_1CLR", "TRIANGLES_2CLR", "QUAD_0CVG", "QUAD_NON0CVG", "QUAD_1CVG", "QUAD_2CVG", "QUAD_3CVG", "QUAD_4CVG", "AVG_QUAD_CNT", "CLOCKS" };

// Performance counter string array for GPU/texture metrics
static const char* str_perf1[] = { "TEXELS", "TX_IDLE", "TX_REGS", "TX_MEMSTALL", "TC_CHECK1_2", "TC_CHECK3_4", "TC_CHECK5_6", "TC_CHECK7_8", "TC_MISS", "VC_ELEMQ_FULL", "VC_MISSQ_FULL", "VC_MEMREQ_FULL", "VC_STATUS7", "VC_MISSREP_FULL", "VC_STREAMBUF_LOW", "VC_ALL_STALLS", "VERTICES", "FIFO_REQ", "CALL_REQ", "VC_MISS_REQ", "CP_ALL_REQ", "CLOCKS" };

static u32 fogtype[5] = { GX_FOG_PERSP_LIN, GX_FOG_PERSP_EXP, GX_FOG_PERSP_EXP2, GX_FOG_PERSP_REVEXP, GX_FOG_PERSP_REVEXP2 };

static const GXColor glx_CopyClearColour = { 0, 0, 0, 0x40 };

static void glx_SendViews();

// No-fog colour passed via a void helper so the by-value arg copy is an
// inline-expansion temporary that grabs the lowest stack slot (0x18),
static inline void glx_SetFogNone()
{
    GXSetFog(GX_FOG_NONE, 0.0f, 0.0f, 0.0f, 0.0f, glx_FogColour);
}

/**
 * Offset/Address/Size: 0x0 | 0x801B45F4 | size: 0xB0
 */
void glplatViewProjectPoint(eGLView view, const nlVector3& v3world, nlVector3& v3NDC)
{
    nlVector3 v_out;
    nlMatrix4* temp_r31 = glViewGetViewMatrix(view);
    nlMatrix4* temp_r30 = glViewGetProjectionMatrix(view);
    nlMultPosVectorMatrix(v_out, v3world, *temp_r31);
    nlMultPosVectorMatrix(v3NDC, v_out, *temp_r30);
    float wc = 1.f / -v_out.z;
    v3NDC.x = v3NDC.x * wc;
    v3NDC.y = -v3NDC.y * wc;
    v3NDC.z = v3NDC.z * wc;
}

/**
 * Offset/Address/Size: 0xB0 | 0x801B46A4 | size: 0x4
 */
void glplatEndFrame()
{
    // EMPTY
}

/**
 * Offset/Address/Size: 0xB4 | 0x801B46A8 | size: 0x58
 */
void glplatBeginFrame()
{
    if (glx_VIWidth != prev_VIWidth)
    {
        s32 diff = 720 - glx_VIWidth;
        prev_VIWidth = glx_VIWidth;
        glx_rmode.viWidth = (s16)glx_VIWidth;
        glx_rmode.viXOrigin = (s16)(diff / 2);
        VIConfigure(&glx_rmode);
        VIFlush();
    }
}

/**
 * Offset/Address/Size: 0x10C | 0x801B4700 | size: 0x20
 */
void glplatFinish()
{
    glxSwapWaitDrawDone();
}

/**
 * Offset/Address/Size: 0x12C | 0x801B4720 | size: 0x34
 */
void glplatAbortFrame()
{
    glplatFrameAllocNextFrame();
    glx_NumVirtMisses = 0;
    glx_VirtLatency = 0;
    glxSwapWaitDrawDone();
    VIWaitForRetrace();
}

// Sets the perf-overlay rect colour. By-reference helper so the nlColour is an
// inline-expansion temporary (no named local in glx_PerfBG) and lands in the
// lowest stack slot (0x8); the {0,0,0,0} decl-init keeps the .sbss2 zero const.
static inline void glx_SetPerfBGColour(glPoly2& p)
{
    nlColour c = { 0, 0, 0, 0 };
    c.c[0] = 0x3A;
    c.c[1] = 0x6E;
    c.c[2] = 0xA5;
    c.c[3] = 0xFF;
    p.SetColour(c);
}

// Draws the dark background rectangle behind the perf/virt overlay text.
// Inlined (and erased by the linker) in the original; the nlColour is a
// temporary so it lands in the first stack slot (0x8).
static inline void glx_PerfBG(int nLines)
{
    glPoly2 p;
    f32 y1;
    f32 x1;
    f32 y0;
    f32 x0;

    glFontVirtualPosToScreenCoordPos(0.f, 36.f, y1, x1);
    glFontVirtualPosToScreenCoordPos(0.f, (f32)(nLines + 0x24), y0, x0);
    glSetDefaultState(false);
    p.SetupRectangle(0.f, x1 - 2.f, 640.f, 4.f + (x0 - x1), 10000000000.f);

    glx_SetPerfBGColour(p);
    p.Attach((eGLView)0x21, 0, 0, -1);
}

static inline void glx_SendFrame(bool bSend)
{
    s32 nLines;
    s32 lineNo;
    u32 bytesFree;

    nLines = 0;
    if (glx_Perf != false)
    {
        nLines = 1;
    }
    if (glx_Virt != false)
    {
        nLines += 1;
    }

    if (nLines != 0)
    {
        glx_PerfBG(nLines);
        lineNo = 0;
        if ((u8)glx_Perf != 0)
        {
            lineNo = 1;
            static u32 print_val0 = 0;
            static u32 print_val1 = 0;
            static int counter = 0;
            s32 cnt = counter + 1;
            counter = cnt;
            if (cnt >= 0x14)
            {
                u32 tv0 = total_val0;
                u32 tv1 = total_val1;
                tv0 = tv0 / (u32)cnt;
                total_val0 = 0;
                total_val1 = 0;
                counter = 0;
                print_val0 = tv0;
                print_val1 = tv1 / (u32)cnt;
            }
            glFontBegin(false);
            glFontPrintf((eGLView)0x21, 1, 0x24, "%u %s, %u %s", print_val0, str_perf0[glx_perf0], print_val1, str_perf1[glx_perf1]);
            glFontEnd();
        }

        if (glx_Virt)
        {
            static u32 print0 = 0;
            static u32 print1 = 0;
            static u32 print2 = 0;

            if ((u32)glx_NumVirtMisses != 0U)
            {
                print0 = glx_NumVirtMisses;
                print1 = glx_VirtLatency;
                print2 = glGetCurrentFrame();
            }
            bytesFree = nlVirtualLargestBlock();
            glFontBegin(0);
            glFontPrintf((eGLView)0x21, 1, lineNo + 0x24, "%uKB free : %u misses, %u us latency (frame %u)", bytesFree >> 0xAU, print0, print1, print2);
            glFontEnd();
        }
    }
}

/**
 * Offset/Address/Size: 0x160 | 0x801B4754 | size: 0x2FC
 */
void glplatSendFrame()
{
    glxSwapPre(true);
    glx_SendFrame(true);
    glx_SendViews();
    glxSwapPost(true);
    glplatFrameAllocNextFrame();
    glx_NumVirtMisses = 0U;
    glx_VirtLatency = 0;
}

// Begin/end GP performance-metric capture. Both were inlined (and erased by
// the linker) in the original; glx_StopMetrics' val0/val1 temps are what place
// the GXReadGPMetric pairs on glx_SendViews' frame.
static inline void glx_StartMetrics()
{
    if (glx_PerfSync)
    {
        GXDrawDone();
    }
    GXClearGPMetric();
    GXSetGPMetric(glx_perf0, glx_perf1);
}

static inline void glx_StopMetrics()
{
    u32 val0;
    u32 val1;

    if (glx_PerfSync)
    {
        GXDrawDone();
    }
    GXReadGPMetric(&val0, &val1);
    total_val0 += val0;
    total_val1 += val1;
}

// Builds the per-view fog colour. Inlined (and erased by the linker) in the
// original; returning the GXColor by value makes it a temporary on
// glx_SendViews' frame (build slot below the GXSetFog by-value arg copy).
static inline GXColor glx_GetFogColour()
{
    GXColor c;
    s32 r = (s32)(glx_FogIntensity * glx_FogColour.r);
    s32 g = (s32)(glx_FogIntensity * glx_FogColour.g);
    s32 b = (s32)(glx_FogIntensity * glx_FogColour.b);

    c.r = r;
    c.g = g;
    c.b = b;
    c.a = glx_FogColour.a;
    return c;
}

/**
 * Offset/Address/Size: 0x45C | 0x801B4A50 | size: 0x470
 */
static void glx_SendViews()
{
    GLRenderList* renderList;
    nlVector4 dofRange;
    bool isEmpty;
    bool useFog;
    PlatTexture* tex;
    u32 textureHandle;
    u16 fenceMetricPending;
    s32 view;
    nlMatrix4 projection;
    GXFogAdjTable fogAdjTable;

    glx_SendReset();

    renderList = gl_ViewGetRenderList((eGLView)9);
    if (!renderList->IsEmpty())
    {
        glx_UpdateWarble();
    }

    if (glx_ViewFence < 0)
    {
        glx_StartMetrics();
    }

    dofRange = glConstantGet("dof/range");

    for (view = 0; view < 0x22; view++)
    {
        if (!glViewGetEnable((eGLView)view))
        {
            continue;
        }

        renderList = gl_ViewGetRenderList((eGLView)view);
        isEmpty = renderList->IsEmpty();
        if (isEmpty && (glViewGetFilter((eGLView)view) == 0) && (view != 0x19))
        {
            if (view == 0)
            {
                glx_ShadowTextureGrab();
            }
            continue;
        }

        fenceMetricPending = 0;
        do
        {
            if (glx_bFog)
            {
                switch (view)
                {
                case 3:
                case 6:
                case 7:
                case 11:
                    useFog = true;
                    break;
                case 2:
                    useFog = glx_bFogSky;
                    break;
                default:
                    useFog = false;
                    break;
                }

                if (useFog)
                {
                    GXSetFog((GXFogType)fogtype[glx_FogType], glx_FogStart, glx_FogEnd, 0.25f, 130.0f, glx_GetFogColour());

                    if (glx_bFogAdjust)
                    {
                        glViewGetProjectionMatrix((eGLView)view, projection);
                        // PORT: the frame is no longer 640 wide, and a fog table built for the wrong width saturates whole columns.
                        {
                            u32 fogWidth = PortLogicalFrameWidth();
                            GXInitFogAdjTable(&fogAdjTable, fogWidth, projection.e2);
                            GXSetFogRangeAdj(1, (s32)(fogWidth / 2), &fogAdjTable);
                        }
                    }
                    else
                    {
                        GXSetFogRangeAdj(0, 0, 0);
                    }
                    break;
                }
            }

            glx_SetFogNone();
        } while (false);

        if (view == glx_ViewFence)
        {
            gld_ViewName((eGLView)view);
            fenceMetricPending = 0;
            if (glx_Perf)
            {
                glx_StartMetrics();
            }
        }

        switch (view)
        {
        case 5:
            continue;

        case 0x11:
            glx_DOFUpdate(dofRange.x);
            glx_DOFGrab();
            break;

        case 0xF:
            renderList = gl_ViewGetRenderList((eGLView)0xE);
            if (renderList->IsEmpty())
            {
                continue;
            }
            glx_ShadowGrab();
            break;

        case 9:
            glx_ColourGrab();
            break;

        case 10:
            renderList = gl_ViewGetRenderList((eGLView)9);
            if (renderList->IsEmpty())
            {
                continue;
            }
            glx_OffsetGrab();
            break;

        default:
            break;
        }

        if (glViewGetDepthClear((eGLView)view))
        {
            glx_ClearZBuffer();
        }

        gl_ViewIterate((eGLView)view, glx_SendFrame_cb);

        if (view == 0)
        {
            glx_ShadowTextureGrab();
        }

        if ((u16)fenceMetricPending != 0)
        {
            glx_StopMetrics();
        }

        if (glViewGetFilter((eGLView)view) == (eGLFilter)6)
        {
            if (glx_GetSharedLock())
            {
                return;
            }

            textureHandle = glGetTexture("target/grab_texture");
            tex = glx_GetTex(textureHandle, false, true);
            // PORT: the frame is PortLogicalFrameWidth() wide, not 640.
            glGrabFrameBufferToTexture(textureHandle, PortLogicalFrameWidth() / 2, tex->m_Height,
                                       0, 0, PortLogicalFrameWidth(), 0x1C0);
        }
    }

    glx_SendEnd();
    if (glx_ViewFence < 0)
    {
        glx_StopMetrics();
    }
}

/**
 * Offset/Address/Size: 0x8CC | 0x801B4EC0 | size: 0x110
 */
void glx_Fog(bool enable)
{
    if (enable)
    {
        s32 r = (s32)(glx_FogIntensity * glx_FogColour.r);
        s32 g = (s32)(glx_FogIntensity * glx_FogColour.g);
        s32 b = (s32)(glx_FogIntensity * glx_FogColour.b);
        GXColor fogColour;
        fogColour.r = r;
        fogColour.g = g;
        fogColour.b = b;
        fogColour.a = glx_FogColour.a;
        GXSetFog((GXFogType)fogtype[glx_FogType], glx_FogStart, glx_FogEnd, 0.25f, 130.f, fogColour);
    }
    else
    {
        GXSetFog(GX_FOG_NONE, 0.f, 0.f, 0.f, 0.f, glx_FogColour);
    }
}

/**
 * Offset/Address/Size: 0x9DC | 0x801B4FD0 | size: 0x8
 */
bool glx_GetFog()
{
    return glx_bFog;
}

/**
 * Offset/Address/Size: 0x9E4 | 0x801B4FD8 | size: 0x24
 */
bool glplatPostStartup()
{
    glxPostInitTargets();
    return true;
}

void virt_cb(unsigned long faultAddr, unsigned long mainAddr, unsigned long pageIndex, unsigned long elapsed, int wroteBack);

static inline void ClearXFBInline(void* cache)
{
    s32 var_r6 = 0;
    u8* var_r5 = (u8*)cache;

    while (var_r6 < (s32)glx_FBSize)
    {
        *(u32*)var_r5 = 0x10801080;
        var_r6 += 4;
        var_r5 += 4;
    }
    DCFlushRange(cache, glx_FBSize);
}

/**
 * Offset/Address/Size: 0xA08 | 0x801B4FFC | size: 0x524
 */
// PORT: re-derive what is sized from the display aspect once rather than read from it every frame.
static gl_ScreenInfo* s_portScreenInfo;

extern "C" void PortApplyAspectChange(void)
{
    const unsigned int width = PortLogicalFrameWidth();

    glx_rmode.fbWidth = (u16)width;
    VIConfigure(&glx_rmode);
    // PORT: the aspect lock is Aurora's alone; the decomp's dolphin/vi.h does not declare it, and with no swapchain there is no display whose ratio could be locked.
#if defined(PORT_USE_AURORA)
    VILockAspectRatio((int)(PortTargetAspect() * 10000.0f), 10000);
#endif

    if (s_portScreenInfo != NULL)
        s_portScreenInfo->ScreenWidth = (int)width;
}

bool glplatStartup(gl_ScreenInfo* screenInfo)
{
    u32 fbSize;
    u32 totalSize;
    void* fbMem;
    u32* ptr;
    s32 i;
    void* buf1;
    s32 j;
    GXRenderModeObj* rmode;

    if (!glxInitMemory())
    {
        return false;
    }

    if (Config::Global().Exists("gpu fifo"))
    {
        f32 var_f1 = GetConfigFloat(Config::Global(), "gpu fifo", 0.0f);
        glx_FIFOSize = (u32)(1024.0f * (1024.0f * var_f1));
    }

    // PORT: the display aspect decides the frame width now. 640 at 4:3.
    s_portScreenInfo = screenInfo;
    screenInfo->ScreenWidth = (int)PortLogicalFrameWidth();
    screenInfo->ScreenHeight = 448;
    screenInfo->ColourDepth[0] = 6;
    screenInfo->ColourDepth[1] = 6;
    screenInfo->ColourDepth[2] = 6;
    screenInfo->ColourDepth[3] = 6;
    screenInfo->ZDepth = 24;
    screenInfo->StencilDepth = 0;
    screenInfo->PixelCentre = 0.5f;
    screenInfo->FSAA = false;
    glx_CopyDispScaleFactor = 1.0f;

    switch (VIGetTvFormat())
    {
    case 0:
        rmode = &GXNtsc480IntDf;
        glx_VideoMode = VideoMode_NTSC;
        break;
    case 5:
    case 1:
        rmode = &glPal480IntDf;
        glx_VideoMode = VideoMode_PAL;
        break;
    case 2:
        rmode = &GXMpal480IntDf;
        glx_VideoMode = VideoMode_MPAL;
        break;
    default:
        nlBreak();
        break;
    }

    if (((OSGetResetCode() >> 0x1F) != 0) && (OSGetResetCode() == 0x17) && (VIGetDTVStatus() != 0))
    {
        rmode = &GXNtsc480Prog;
        glx_bProgressiveMode = true;
    }
    else
    {
        glx_bProgressiveMode = false;
    }

    GXAdjustForOverscan(rmode, &glx_rmode, 0, 16);

    // PORT: widen the mode after the overscan trim, not before.
    glx_rmode.fbWidth = (u16)PortLogicalFrameWidth();

    if (glx_VideoMode == VideoMode_PAL)
    {
        glx_rmode.efbHeight = 448;
        glx_CopyDispScaleFactor = GXGetYScaleFactor(448, glx_rmode.xfbHeight);
        glx_TargetFPS = 50;
    }
    else
    {
        glx_TargetFPS = 60;
    }

    glx_rmode.viWidth = glx_VIWidth;
    glx_rmode.viXOrigin = (720 - glx_VIWidth) / 2;
    VIConfigure(&glx_rmode);
    VIFlush();
    VIConfigure(&glx_rmode);

    glx_FIFOMem = nlMalloc(glx_FIFOSize, 32, false);
    if (glx_FIFOMem == NULL)
    {
        return false;
    }
    glx_FIFO = GXInit(glx_FIFOMem, glx_FIFOSize);

    // PORT: size the XFB from the console's width.
    fbSize = ((640u + 15) & 0xFFF0) * glx_rmode.xfbHeight * 2;
    if (fbSize < 0x9F600u)
    {
        fbSize = 0x9F600u;
    }
    totalSize = fbSize * 2;
    fbMem = nlMalloc(totalSize, 32, false);
    glx_FrameBuffer[1] = (void*)((u8*)fbMem + fbSize);
    glx_FrameBuffer[0] = fbMem;
    glx_FBSize = fbSize;
    ClearXFBInline(glx_FrameBuffer[0]);

    buf1 = glx_FrameBuffer[1];
    ptr = (u32*)buf1;
    i = 0;
    while (i < glx_FBSize)
    {
        *ptr = 0x10801080;
        i += 4;
        ptr++;
    }
    DCFlushRange(buf1, glx_FBSize);

    tDebugPrintManager::Print(DC_GL, "%uKB used for FB and FIFO\n", totalSize >> 10, glx_FIFOSize >> 10);

    gxInit();

    GXSetViewport(0.0f, 0.0f, (f32)glx_rmode.fbWidth, (f32)glx_rmode.efbHeight, 0.0f, 1.0f);
    GXSetScissor(0, 0, glx_rmode.fbWidth, glx_rmode.efbHeight);
    GXSetDispCopySrc(0, 0, glx_rmode.fbWidth, glx_rmode.efbHeight);
    GXSetDispCopyDst(glx_rmode.fbWidth, glx_rmode.xfbHeight);
    GXSetDispCopyYScale(glx_CopyDispScaleFactor);
    GXSetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
    gxSetDither(true);
    gxSetColourUpdate(true);
    gxSetAlphaUpdate(true);

    GXSetCopyClear(glx_CopyClearColour, 0xFFFFFF);
    GXSetDispCopyGamma(GX_GM_1_0);

    j = 0;
    do
    {
        gxSetTevColourOp(j, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
        gxSetTevAlphaOp(j, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
        j++;
    } while (j < 16);

    GXFlush();
    VISetNextFrameBuffer(glx_FrameBuffer[0]);
    glxSwapSetBlack(true);
    VIFlush();
    VIWaitForRetrace();
    if (glx_rmode.viTVmode & 1)
    {
        VIWaitForRetrace();
    }
    glxInitSwap(glx_FrameBuffer[0], glx_FrameBuffer[1]);
    glxInitTex();
    glxInitTargets();
    VMSetLogStatsCallback((VMLogStatsCallback)virt_cb);
    return true;
}

/**
 * Offset/Address/Size: 0xF2C | 0x801B5520 | size: 0x2C
 */
void glx_SetPal50Mode()
{
    glx_SwitchVideoMode(&glPal480IntDf, VideoMode_PAL);
}

/**
 * Offset/Address/Size: 0xF58 | 0x801B554C | size: 0x2C
 */
void glx_SetRGB60Mode()
{
    glx_SwitchVideoMode(&GXEurgb60Hz480IntDf, VideoMode_PAL60);
}

/**
 * Offset/Address/Size: 0xF84 | 0x801B5578 | size: 0x34
 */
void glx_SetInterlacedMode()
{
    glx_SwitchVideoMode(&GXNtsc480IntDf, VideoMode_NTSC);
    glx_bProgressiveMode = 0;
}

/**
 * Offset/Address/Size: 0xFB8 | 0x801B55AC | size: 0x34
 */
void glx_SetProgressiveMode()
{
    glx_SwitchVideoMode(&GXNtsc480Prog, VideoMode_NTSC);
    glx_bProgressiveMode = 1;
}

/**
 * Offset/Address/Size: 0xFEC | 0x801B55E0 | size: 0x1C
 */
u32 glx_GetResetCode()
{
    return glx_bProgressiveMode ? 0x17 : 0;
}

static inline void glx_SetVIWidth(const s32 currentWidth, const s32 maxWidth)
{
    const s32 diff = maxWidth - currentWidth;
    glx_rmode.viWidth = (s16)currentWidth;
    glx_rmode.viXOrigin = (s16)(((s32)((u32)diff >> 31) + diff) >> 1);
}

/**
 * Offset/Address/Size: 0x1008 | 0x801B55FC | size: 0x18C
 */
void glx_SwitchVideoMode(_GXRenderModeObj* rmode, eVideoMode mode)
{
    glx_VideoMode = mode;
    GXAdjustForOverscan(rmode, &glx_rmode, 0, 0x10);
    if (mode == 1)
    {
        glx_rmode.efbHeight = 448;
        glx_CopyDispScaleFactor = GXGetYScaleFactor(448, glx_rmode.xfbHeight);
        glx_TargetFPS = 50;
    }
    else
    {
        glx_TargetFPS = 60;
        glx_CopyDispScaleFactor = 1.f;
    }
    VISetBlack(1);
    VIFlush();
    VIWaitForRetrace();

    glx_rmode.viWidth = glx_VIWidth;
    glx_rmode.viXOrigin = (720 - glx_VIWidth) / 2;

    VIConfigure(&glx_rmode);
    VIFlush();
    VIConfigure(&glx_rmode);
    VIFlush();
    VIWaitForRetrace();

    for (int i = 0; i < 60; i++)
    {
        OSYieldThread();
        VIWaitForRetrace();
    }

    VISetBlack(0);
    VIFlush();
    VIWaitForRetrace();

    GXSetViewport(0.f, 0.f, (f32)glx_rmode.fbWidth, (f32)glx_rmode.efbHeight, 0.f, 1.f);
    GXSetScissor(0, 0, glx_rmode.fbWidth, glx_rmode.efbHeight);
    GXSetDispCopySrc(0, 0, glx_rmode.fbWidth, glx_rmode.efbHeight);
    GXSetDispCopyDst(glx_rmode.fbWidth, glx_rmode.xfbHeight);
    GXSetDispCopyYScale(glx_CopyDispScaleFactor);
}

/**
 * Offset/Address/Size: 0x1194 | 0x801B5788 | size: 0x8
 */
bool glplatPreStartup()
{
    return true;
}

/**
 * Offset/Address/Size: 0x119C | 0x801B5790 | size: 0x1C
 */
void virt_cb(unsigned long faultAddr, unsigned long mainAddr, unsigned long pageIndex, unsigned long elapsed, int wroteBack)
{
    glx_NumVirtMisses += 1;
    glx_VirtLatency += elapsed;
}

/**
 * Offset/Address/Size: 0x11B8 | 0x801B57AC | size: 0x4C
 */
void glx_ClearXFB(void* cache)
{
    u8* var_r5 = (u8*)cache;
    s32 var_r6 = 0;

    while (var_r6 < (s32)glx_FBSize)
    {
        *(u32*)var_r5 = 0x10801080;
        var_r6 += 4;
        var_r5 += 4;
    }
    DCFlushRange(cache, glx_FBSize);
}

/**
 * Offset/Address/Size: 0x1204 | 0x801B57F8 | size: 0x8
 */
u32 glx_GetTargetFPS()
{
    return glx_TargetFPS;
}

/**
 * Offset/Address/Size: 0x120C | 0x801B5800 | size: 0x8
 */
u32 glx_GetScaledXFBWidth()
{
    return glx_VIWidth;
}

/**
 * Offset/Address/Size: 0x1214 | 0x801B5808 | size: 0x110
 */
void glx_SetFog(int type)
{
    if (type < 0)
    {
        glx_bFog = false;
        return;
    }

    if (type == 0)
    {
        glx_bFog = 1;
        glx_bFogSky = 1;
        glx_FogType = 0;
        glx_FogColour.r = 0xAE;
        glx_FogColour.g = 0x73;
        glx_FogColour.b = 0x55;
        glx_FogIntensity = 1.f;
        glx_FogStart = 5.f;
        glx_FogEnd = 130.f;
        return;
    }
    if (type == 1)
    {
        glx_bFog = 1;
        glx_bFogSky = 0;
        glx_FogType = 4;
        glx_FogColour.r = 0x2C;
        glx_FogColour.g = 0xB9;
        glx_FogColour.b = 0xFF;
        glx_FogIntensity = .5f;
        glx_FogStart = 10.f;
        glx_FogEnd = 125.f;
        return;
    }

    if (type != 2)
    {
        return;
    }

    glx_bFog = 1;
    glx_bFogSky = 1;
    glx_FogType = 2;
    glx_FogColour.r = 0xFA;
    glx_FogColour.g = 0xE6;
    glx_FogColour.b = 0xB9;
    glx_FogIntensity = 1.f;
    glx_FogStart = 45.f;
    glx_FogEnd = 160.f;
}
