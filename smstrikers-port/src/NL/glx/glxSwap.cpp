#include "NL/glx/glxSwap.h"
#include "NL/nlDebug.h"
#include "NL/nlPrint.h"
#include "NL/nlEndian.h"
#include "NL/nlMemory.h"
#include "NL/nlConfig.h"

#include "dolphin/gx.h"
#include "dolphin/vi.h"
#include "dolphin/os/OSThread.h"
#include "dolphin/PPCArch.h"
#include "NL/glx/glxGX.h"
#include "NL/glx/glxTexture.h"
#include "NL/gl/glPlat.h"
#include "NL/gl/glConstant.h"
#include "Game/ResetTask.h"

#include "direct_io.h"

static u8 glx_bAllowDrawSync = true;
static int LoadWaitSeconds = 2;

u8 glx_ScreenShot;
static int _shotno;
u8 glx_MovieOutput;
u8 glx_ResetCaptureFrame;
u8 glx_bLoadingIndicator;
s32 glx_nBlitXor;
static void* glx_FrameBuffer[2];
static int glx_nBuffer;
static int nFirstFrame;
u8 bInRetrace;

enum GLXSwapMode
{
    GLSwap_Simple = 0,
    GLSwap_Hitz = 1,
    GLSwap_Num = 2,
};
GLXSwapMode glx_SwapMode;
static int glx_nLoadFrame;
static int glx_nLoadWaitFrames;
static int loadCounter;
static int nSelected;
static bool glx_bLoadOtherPosition;
static u8 bSaved;
static u8 bSavedState;
static u32 count0;

static u16 _ImageData[32][32] = {
#include "NL/glx/_ImageData.inc"
};

static u16 _SelectedImageData[32][32] = {
#include "NL/glx/_SelectedImageData.inc"
};

typedef BasicString<char, Detail::TempStringAllocator> TempString;

static void DrawLoadingIndicator();
static void BlitImage(int, int, float, float, bool);
static void hitz_Post(bool);
static void hitz_Pre(bool);
static void hitz_SwapBuffers();
static void hitz_AdvanceFrame();
static void simple_Post(bool);
static void simple_Pre(bool);
static void vi_post_cb(u32);
static void vi_pre_cb(u32);
static void loading_indicator();
static void glx_ScreenCapture(bool);

/**
 * Offset/Address/Size: 0x0 | 0x801BED50 | size: 0x118
 */
static void DrawLoadingIndicator()
{
    u32 targetFPS = glx_GetTargetFPS();
    int num_ticks;
    int offset_x;
    int offset_y;
    float scale_x;
    float scale_y;
    int y;

    if (targetFPS == 50)
    {
        offset_x = 0x120;
        y = 0x1B4;
        if (glx_bLoadOtherPosition)
        {
            y = 0x1A2;
        }
        scale_x = 1.0f;
        scale_y = 1.0f;
        offset_y = y;
        num_ticks = 0x10;
    }
    else
    {
        offset_x = 0x120;
        y = 0x17E;
        if (glx_bLoadOtherPosition)
        {
            y = 0x16E;
        }
        scale_x = 1.0f;
        scale_y = 1.0f;
        offset_y = y;
        num_ticks = 0x13;
    }

    int width = (int)(24.0f * scale_x);
    if (width & 1)
    {
        width++;
    }

    for (int i = 0; i < 3; i++)
    {
        BlitImage(offset_x, offset_y, scale_x, scale_y, !(nSelected - i));
        offset_x += width;
    }

    loadCounter++;
    if (loadCounter >= num_ticks)
    {
        loadCounter = 0;
        nSelected++;
        if (nSelected >= 3)
        {
            nSelected = 0;
        }
    }
}

static inline void StorePixel(
    void* fb, int x, int y, const int yuv[3], const int yuv_p[3], const int yuv_pp[3])
{
    u8* fbyte = (u8*)fb + y * 0x500 + (x << 1);
    fbyte[0] = (u8)yuv[0];

    if ((x & 1) != 0)
    {
        fbyte[-1] = (u8)(0.25f * (float)yuv_pp[1] + 0.5f * (float)yuv_p[1] + 0.25f * (float)yuv[1]);
        fbyte[1] = (u8)(0.25f * (float)yuv_pp[2] + 0.5f * (float)yuv_p[2] + 0.25f * (float)yuv[2]);
    }
}

static inline int DecodeLoadingPixel(u16 pixel, int yuv[3])
{
    int y = (pixel & 0xF000) >> 12;
    y |= y << 4;
    int u = (pixel & 0x0F00) >> 8;
    int v = (pixel & 0x00F0) >> 4;
    u |= u << 4;
    v |= v << 4;
    int a = pixel & 0xF;
    a |= a << 4;
    yuv[0] = y;
    yuv[1] = u;
    yuv[2] = v;
    return a;
}

/**
 * Offset/Address/Size: 0x118 | 0x801BEE68 | size: 0x2D0
 */
static void BlitImage(int offset_x, int offset_y, float scale_x, float scale_y, bool bSelectedImage)
{
    float limit = 32.0f;
    float xinc = limit / (float)(int)(limit * scale_x);
    float yinc = limit / (float)(int)(limit * scale_y);
    float xpos, ypos;
    int yuv[3], yuv_p[3], yuv_pp[3];
    int ix, x, y;

    for (y = 0, ypos = 0.0f; ypos < limit; y++, ypos += yinc)
    {
        int srcRow = (int)ypos;

        for (x = 0, xpos = 0.0f; xpos < limit; x++, xpos += xinc)
        {
            ix = (int)xpos;
            int pixel;
            if (bSelectedImage)
            {
                pixel = _SelectedImageData[srcRow][ix];
            }
            else
            {
                pixel = _ImageData[srcRow][ix];
            }

            int alpha = DecodeLoadingPixel(pixel, yuv);
            if (alpha > 0)
            {
                StorePixel(glx_FrameBuffer[glx_nBuffer ^ glx_nBlitXor], offset_x + x, offset_y + y, yuv, yuv_p, yuv_pp);
            }

            if (x == 0)
            {
                yuv_p[1] = yuv[1];
            }
            yuv_pp[1] = yuv_p[1];
            yuv_p[1] = yuv[1];

            if (x == 0)
            {
                yuv_p[2] = yuv[2];
            }
            yuv_pp[2] = yuv_p[2];
            yuv_p[2] = yuv[2];
        }

        DCStoreRangeNoSync(
            (u8*)glx_FrameBuffer[glx_nBuffer ^ glx_nBlitXor] + (offset_y + y) * 0x500 + (offset_x << 1),
            (u32)(x << 1));
    }

    PPCSync();
}

static void PutPixel(void* fb, int x, int y, const int yuv[3], const int yuv_p[3], const int yuv_pp[3])
{
    u8* fbyte = (u8*)fb + y * 0x500 + (x << 1);
    fbyte[0] = (u8)yuv[0];

    if ((x & 1) != 0)
    {
        fbyte[-1] = (u8)(0.25f * (float)yuv_pp[1] + 0.5f * (float)yuv_p[1] + 0.25f * (float)yuv[1]);
        fbyte[1] = (u8)(0.25f * (float)yuv_pp[2] + 0.5f * (float)yuv_p[2] + 0.25f * (float)yuv[2]);
    }
}

static u16 GetPixelFromTexture(PlatTexture* pTex, int x, int y)
{
    u16* pData = (u16*)pTex->m_SwizzledData;
    return pData[((y / 4) * (pTex->m_Width >> 2) + (x / 4)) * 16 + ((y & 3) << 2) + (x & 3)];
}

static void RGB5A32RGB32(u8* pRGB, u16 rgb16)
{
    if (rgb16 & 0x8000)
    {
        pRGB[0] = ((rgb16 >> 10) & 0x1F) * 255 / 31;
        pRGB[1] = ((rgb16 >> 5) & 0x1F) * 255 / 31;
        pRGB[2] = (rgb16 & 0x1F) * 255 / 31;
        pRGB[3] = 0xFF;
    }
    else
    {
        pRGB[3] = ((rgb16 >> 12) & 0x7) * 255u / 7;
        pRGB[0] = ((rgb16 >> 8) & 0xF) * 255u / 15;
        pRGB[1] = ((rgb16 >> 4) & 0xF) * 255u / 15;
        pRGB[2] = (rgb16 & 0xF) * 255u / 15;
    }
}

static void RGB2YUV(u8* pYUV, const u8* pRGB)
{
    float r = pRGB[0];
    float g = pRGB[1];
    float b = pRGB[2];
    float y = 0.299 * r + 0.587 * g + 0.114 * b;
    float u = 128.0 - 0.169 * r - 0.331 * g + 0.5f * b;
    float v = 128.0 + 0.5f * r - 0.419 * g - 0.081 * b;
    int iy = (int)(y + 0.5f);
    int iu = (int)(u + 0.5f);
    int iv = (int)(v + 0.5f);

    iy = iy < 0 ? 0 : (iy > 255 ? 255 : iy);
    iu = iu < 0 ? 0 : (iu > 255 ? 255 : iu);
    iv = iv < 0 ? 0 : (iv > 255 ? 255 : iv);

    pYUV[0] = iy;
    pYUV[1] = iu;
    pYUV[2] = iv;
}

/**
 * Offset/Address/Size: 0x3E8 | 0x801BF138 | size: 0xC0
 */
static void hitz_Post(bool bSend)
{
    if (glx_ResetCaptureFrame != 0)
    {
        _shotno = 0;
    }
    if (glx_ScreenShot != 0)
    {
        glx_ScreenCapture(false);
        glx_ScreenShot = 0;
    }
    if (glx_MovieOutput != 0)
    {
        glx_ScreenCapture(true);
    }

    GXColor clearColor = { 0, 0, 0, 0 };
    GXSetCopyClear(clearColor, 0xFFFFFF);
    gxSetZMode(true, GX_LEQUAL, true);
    gxSetColourUpdate(true);
    gxSetAlphaUpdate(true);
    GXCopyDisp(glx_FrameBuffer[glx_nBuffer], true);
    GXSetDrawDone();
    GXFlush();
    count0 = VIGetRetraceCount();
}

/**
 * Offset/Address/Size: 0x4A8 | 0x801BF1F8 | size: 0xD4
 */
static void hitz_Pre(bool)
{
    GXWaitDrawDone();
    u32 retraceCount = VIGetRetraceCount();
    float value = glConstantGet("glxswap/vwait").x;

    if (value == 2.0f)
    {
        hitz_AdvanceFrame();
        VIWaitForRetrace();
        u32 newRetraceCount = VIGetRetraceCount();
        if (newRetraceCount < count0 + 2)
        {
            VIWaitForRetrace();
        }
    }
    else if (value == 1.0f)
    {
        hitz_AdvanceFrame();
        VIWaitForRetrace();
    }
    else if (count0 == retraceCount)
    {
        hitz_AdvanceFrame();
        VIWaitForRetrace();
    }
    else if (value == 0.5f)
    {
        while (bInRetrace != 0)
        {
            OSYieldThread();
        }
        hitz_AdvanceFrame();
    }
    else
    {
        hitz_AdvanceFrame();
    }

    hitz_SwapBuffers();
}

/**
 * Offset/Address/Size: 0x57C | 0x801BF2CC | size: 0x10
 */
static void hitz_SwapBuffers()
{
    glx_nBuffer ^= 1;
}

/**
 * Offset/Address/Size: 0x58C | 0x801BF2DC | size: 0x54
 */
static void hitz_AdvanceFrame()
{
    VISetNextFrameBuffer(glx_FrameBuffer[glx_nBuffer]);
    if (nFirstFrame > 0)
    {
        nFirstFrame--;
        if (nFirstFrame == 0)
        {
            VISetBlack(false);
        }
    }
    VIFlush();
}

/**
 * Offset/Address/Size: 0x5E0 | 0x801BF330 | size: 0xA0
 */
static void simple_Post(bool bSend)
{
    gxSetZMode(true, GX_LEQUAL, true);
    gxSetColourUpdate(true);
    gxSetAlphaUpdate(true);
    GXCopyDisp(glx_FrameBuffer[glx_nBuffer], true);
    GXDrawDone();
    VISetNextFrameBuffer(glx_FrameBuffer[glx_nBuffer]);
    if (nFirstFrame > 0)
    {
        nFirstFrame--;
        if (nFirstFrame == 0)
        {
            VISetBlack(false);
        }
    }
    VIFlush();
    VIWaitForRetrace();
    glx_nBuffer ^= 1;
}

/**
 * Offset/Address/Size: 0x680 | 0x801BF3D0 | size: 0x4
 */
static void simple_Pre(bool)
{
}

/**
 * Offset/Address/Size: 0x684 | 0x801BF3D4 | size: 0x58
 */
void glxSwapPost(bool bSend)
{
    if (glx_bLoadingIndicator == false)
    {
        switch (glx_SwapMode)
        {
        case GLSwap_Simple:
            simple_Post(bSend);
            return;
        case GLSwap_Hitz:
            hitz_Post(bSend);
            return;
        default:
            nlBreak();
            break;
        }
    }
}

/**
 * Offset/Address/Size: 0x6DC | 0x801BF42C | size: 0x58
 */
void glxSwapPre(bool bSend)
{
    if (glx_bLoadingIndicator == false)
    {
        switch (glx_SwapMode)
        {
        case GLSwap_Simple:
            simple_Pre(bSend);
            return;
        case GLSwap_Hitz:
            hitz_Pre(bSend);
            return;
        default:
            nlBreak();
            break;
        }
    }
}

/**
 * Offset/Address/Size: 0x734 | 0x801BF484 | size: 0x260
 */
void glxInitSwap(void* fb0, void* fb1)
{
    glx_FrameBuffer[0] = fb0;
    glx_FrameBuffer[1] = fb1;
    glx_nBuffer = 0;
    nFirstFrame = 3;
    glx_bAllowDrawSync = 1;

    TempString swapString = Config::Global().Get<TempString>("swapmode", TempString("hitz"));

    glx_SwapMode = (swapString == "simple") ? GLSwap_Simple : GLSwap_Hitz;
    switch (glx_SwapMode)
    {
    case GLSwap_Hitz:
        GXSetDrawDone();
        GXFlush();
        VISetPreRetraceCallback(vi_pre_cb);
        VISetPostRetraceCallback(vi_post_cb);
        break;
    case GLSwap_Simple:
        break;
    default:
        nlBreak();
        break;
    }
}

/**
 * Offset/Address/Size: 0x994 | 0x801BF6E4 | size: 0x28
 */
void glxSwapWaitDrawDone()
{
    GXSetDrawDone();
    GXFlush();
    GXWaitDrawDone();
}

/**
 * Offset/Address/Size: 0x9BC | 0x801BF70C | size: 0x50
 */
static void vi_post_cb(u32)
{
    HandleSoftReset();
    if (glx_bLoadingIndicator != false)
    {
        glx_nLoadFrame++;
        if (glx_nLoadFrame >= glx_nLoadWaitFrames)
        {
            loading_indicator();
        }
    }
    bInRetrace = 0;
}

/**
 * Offset/Address/Size: 0xA0C | 0x801BF75C | size: 0xC
 */
static void vi_pre_cb(u32)
{
    bInRetrace = 1;
}

/**
 * Offset/Address/Size: 0xA18 | 0x801BF768 | size: 0x88
 */
static void loading_indicator()
{
    if (nFirstFrame == 0)
    {
        DrawLoadingIndicator();
        VISetNextFrameBuffer(glx_FrameBuffer[glx_nBuffer]);
        VIFlush();
        glx_nBuffer ^= 1;
    }

    if (nFirstFrame > 0)
    {
        nFirstFrame--;
        if (nFirstFrame == 0)
        {
            glx_ClearXFB(glx_FrameBuffer[0]);
            glx_ClearXFB(glx_FrameBuffer[1]);
            VISetBlack(false);
            VIFlush();
        }
    }
}

/**
 * Offset/Address/Size: 0xAA0 | 0x801BF7F0 | size: 0x54
 */
void glxLoadRestoreState()
{
    bSaved = 0;
    glx_bLoadingIndicator = bSavedState;
    if (glx_bLoadingIndicator)
    {
        glxSwapLoading(true, false);
        glx_ClearXFB(glx_FrameBuffer[0]);
        glx_ClearXFB(glx_FrameBuffer[1]);
    }
}

/**
 * Offset/Address/Size: 0xAF4 | 0x801BF844 | size: 0x40
 */
void glxLoadSaveState()
{
    bSaved = 1;
    bSavedState = glx_bLoadingIndicator;
    if (glx_bLoadingIndicator)
    {
        glxSwapLoading(false, false);
    }
}

/**
 * Offset/Address/Size: 0xB34 | 0x801BF884 | size: 0x4C
 */
void glxSwapLoading(bool bBegin, bool bOtherPosition)
{
    u32 loadWaitFrames;
    u32 targetFPS;
    u32 lws;

    glx_bLoadOtherPosition = bOtherPosition;
    glx_bLoadingIndicator = bBegin;
    glx_nLoadFrame = 0;
    targetFPS = glx_GetTargetFPS();
    lws = LoadWaitSeconds;
    loadWaitFrames = lws * targetFPS;
    loadCounter = 0;
    nSelected = 0;
    LoadWaitSeconds = 0;
    glx_nLoadWaitFrames = loadWaitFrames;
}

static u8 glxSwapAllowDrawSync()
{
    return glx_bAllowDrawSync;
}

/**
 * Offset/Address/Size: 0xB80 | 0x801BF8D0 | size: 0x14
 */
void* glxGetBackBuffer()
{
    return glx_FrameBuffer[glx_nBuffer];
}

/**
 * Offset/Address/Size: 0xB94 | 0x801BF8E4 | size: 0x18
 */
void* glxGetDisplayedBuffer()
{
    return glx_FrameBuffer[glx_nBuffer ^ 1];
}

#pragma pack(push)
#pragma pack(1)
struct TargaHeader
{
    /* 0x00 */ unsigned char imageIDLength;
    /* 0x01 */ unsigned char colorMapType;
    /* 0x02 */ unsigned char imageType;
    /* 0x03 */ unsigned short firstEntry;
    /* 0x05 */ unsigned short mapLength;
    /* 0x07 */ unsigned char paletteBitsPerPixel;
    /* 0x08 */ unsigned short xOrigin;
    /* 0x0A */ unsigned short yOrigin;
    /* 0x0C */ unsigned short width;
    /* 0x0E */ unsigned short height;
    /* 0x10 */ unsigned char bitsPerPixel;
    /* 0x11 */ unsigned char imageDescriptor;
}; // total size: 0x12
#pragma pack(pop)

/**
 * Offset/Address/Size: 0xBAC | 0x801BF8FC | size: 0x1B0
 */
static void glx_ScreenCapture(bool isMovie)
{
    char filename[0x40];
    FILE* file;
    TargaHeader header;
    u32 argbColor;
    union
    {
        u32 word;
        u8 bytes[4];
    } colorBytes;
    s32 pixelOffset;
    u8* imageData;
    s32 y, x;

    if (isMovie != 0)
    {
        nlSNPrintf(filename, 0x40, "../shot%03d.tga", _shotno);
    }
    else
    {
        nlSNPrintf(filename, 0x40, "shot%03d.tga", _shotno);
    }

    _shotno++;
    file = fopen(filename, "wb");

    if (file != NULL)
    {
        header.imageIDLength = 0;
        header.colorMapType = 0;
        header.imageType = 2;
        header.firstEntry = 0;
        header.mapLength = 0;
        header.paletteBitsPerPixel = 0;
        header.xOrigin = 0;
        header.yOrigin = 0;
        header.width = 0x280;
        header.height = 0x1C0;
        header.bitsPerPixel = 0x18;
        header.imageDescriptor = 0x20;

        nlSwapEndian(header.width, &(header.width));
        nlSwapEndian(header.height, &(header.height));

        imageData = new (8, false) u8[0xD2000];
        GXDrawDone();

        for (y = 0; y < 0x1C0; y++)
        {
            for (x = 0; x < 0x280; x++)
            {
                GXPeekARGB((u16)x, (u16)y, &argbColor);

                pixelOffset = (y * 0x280 + x) * 3;
                colorBytes.word = argbColor;
                imageData[pixelOffset] = colorBytes.bytes[3];
                imageData[pixelOffset + 1] = colorBytes.bytes[2];
                imageData[pixelOffset + 2] = colorBytes.bytes[1];
            }
        }

        fwrite(&header, 1, sizeof(TargaHeader), file);
        fwrite(imageData, 3, 0x46000, file);
        fclose(file);
        delete[] imageData;
    }
}

/**
 * Offset/Address/Size: 0xD5C | 0x801BFAAC | size: 0x48
 */
void glxSwapSetBlack(bool black)
{
    if (black)
    {
        VISetBlack(1);
        nFirstFrame = 3;
    }
    else
    {
        VISetBlack(0);
        nFirstFrame = 0;
    }
}
