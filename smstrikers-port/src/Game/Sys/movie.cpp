#include "Game/Sys/movie.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/Audio/AudioStream.h"
#include "Game/Sys/debug.h"
#include "NL/gc/gcSwizzler.h"
#include "NL/gl/glMemory.h"
#include "NL/gl/glState.h"
#include "NL/glx/glxTarget.h"
#include "NL/glx/glxTexture.h"
#include "NL/nlMemory.h"
#include "NL/nlString.h"
#include "dolphin/thp/THPInfo.h"
#include "port/audio.h"   // PORT: is anything pulling?

extern "C"
{
    void GXSetDrawDone(void);
    void GXFlush(void);
    void GXWaitDrawDone(void);
    void GXInvalidateTexAll(void);
    int THPSimpleDecode(int);
    void THPSimpleAudioStart(void);
    int THPSimpleGetTotalFrame(void);
    int THPSimpleGetCurrentFrame(void);
    void THPSimpleAudioStop(void);
    void THPSimpleLoadStop(void);
    void THPSimpleClose(void);
    void THPSimpleQuit(void);
    int THPSimpleInit(int);
    int THPSimpleOpen(const char*);
    void THPSimpleGetVideoInfo(void*);
    unsigned long THPSimpleCalcNeedMemory(unsigned long, unsigned long);
    void THPSimpleSetBuffer(unsigned char*);
    int THPSimplePreLoad(int);
}
void nlServiceFileSystem(void);

static bool g_bActive;
static unsigned char* buffer;
static bool bMustFreeBuffer;
static bool start;
static bool g_bMovieMustStop;
static unsigned long long resourceMarker;
static void* g_GrabTextureData;
static unsigned long g_GrabTextureSize;
static const unsigned long GrabTextureHandle = glGetTexture("target/grab_texture");
static PlatTexture* pTex[4] = { 0 };
static THPVideoInfo videoInfo;

/**
 * Offset/Address/Size: 0x28C | 0x801CBA48 | size: 0x4F0
 */
bool MovieStart(const char* szFilename, bool bSound, bool bLoopMovie)
{
    unsigned long bufCount;
    unsigned long size;

    if (g_bActive)
    {
        return false;
    }

    g_bMovieMustStop = false;

    char fileName[256];
    nlStrNCpy(fileName, szFilename, 256);
    nlToLower(fileName);

    unsigned char bIntroMovie = strstr(fileName, "intromovie") != NULL;

    bufCount = 8;
    if (bIntroMovie)
        bufCount = 16;
    size = 3;
    if (bIntroMovie)
        size = 6;

    // PORT: 3 is retail's, mixing the movie under MusyX in the AI DMA callback, and it needs both a callback to chain and an open audio device; 1 decodes video only.
    THPSimpleInit(bSound && PortAudioDeviceOpen() ? 3 : 1);

    if (!THPSimpleOpen(fileName))
    {
        THPSimpleQuit();
        return false;
    }

    THPSimpleGetVideoInfo(&videoInfo);
    resourceMarker = glResourceMark();

    pTex[0] = glx_CreatePlatTexture();
    pTex[0]->Create(videoInfo.mXSize, videoInfo.mYSize, (eGXTextureFormat)4, 1, false, false);
    glx_AddTex(glGetTexture("movie"), pTex[0]);

    pTex[1] = glx_CreatePlatTexture();
    pTex[1]->Create(videoInfo.mXSize / 2, videoInfo.mYSize / 2, (eGXTextureFormat)4, 1, false, false);
    glx_AddTex(glGetTexture("movie_u"), pTex[1]);

    pTex[2] = glx_CreatePlatTexture();
    pTex[2]->Create(videoInfo.mXSize / 2, videoInfo.mYSize / 2, (eGXTextureFormat)4, 1, false, false);
    glx_AddTex(glGetTexture("movie_v"), pTex[2]);

    u32 texSize = GCTextureSize(pTex[0]->m_Format, pTex[0]->m_Width, pTex[0]->m_Height, pTex[0]->m_Levels, (unsigned long)-1);
    memset(pTex[0]->m_SwizzledData, 0x10, texSize);

    texSize = GCTextureSize(pTex[1]->m_Format, pTex[1]->m_Width, pTex[1]->m_Height, pTex[1]->m_Levels, (unsigned long)-1);
    memset(pTex[1]->m_SwizzledData, 0x80, texSize);

    texSize = GCTextureSize(pTex[2]->m_Format, pTex[2]->m_Width, pTex[2]->m_Height, pTex[2]->m_Levels, (unsigned long)-1);
    memset(pTex[2]->m_SwizzledData, 0x80, texSize);

    pTex[0]->Prepare();
    pTex[1]->Prepare();
    pTex[2]->Prepare();

    GXInvalidateTexAll();

    size = THPSimpleCalcNeedMemory(bufCount, size);

    if (glx_GetSharedMemorySize() >= size)
    {
        buffer = (unsigned char*)glx_GetSharedMemory();
        bMustFreeBuffer = false;

        PlatTexture* tex = glx_GetTex(GrabTextureHandle, true, true);
        g_GrabTextureSize = GCTextureSize(tex->m_Format, tex->m_Width, tex->m_Height, tex->m_Levels, (unsigned long)-1);

        if (nlVirtualLargestBlock() >= g_GrabTextureSize + 0x2000)
        {
            g_GrabTextureData = nlVirtualAlloc(g_GrabTextureSize, false);
            memcpy(g_GrabTextureData, tex->m_SwizzledData, g_GrabTextureSize);
        }
    }
    else
    {
        buffer = (unsigned char*)nlMalloc(size, 32, false);
        bMustFreeBuffer = true;
    }

    THPSimpleSetBuffer(buffer);

    if (nlTaskManager::m_pInstance->m_CurrState != 4)
    {
        if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
        {
            AudioStreamTrack::TrackManagerBase* trackMgr = g_pTrackManager;
            // PORT: null when audio is disabled, nothing to ask.
            AudioStreamTrack::StreamTrack* track = trackMgr != NULL ? trackMgr->GetTrack(nlStringLowerHash("Announcer")) : NULL;
            // PORT: null when audio is disabled, so the track is null too.
            if (track != NULL) track->Pause(0, false);
        }
    }

    if (!THPSimplePreLoad(bLoopMovie != false))
    {
        g_bActive = true;

        if (glx_GetSharedLock())
        {
            glx_UnlockSharedMemory();
        }

        int lastFrame = THPSimpleGetTotalFrame() - 1;
        int currentFrame = (int)(unsigned int)THPSimpleGetCurrentFrame();
        if ((unsigned int)currentFrame != (unsigned int)lastFrame)
        {
            tDebugPrintManager::Print(DC_RENDER, "MOVIE did not finish playback.\n");
        }

        THPSimpleAudioStop();
        THPSimpleLoadStop();
        THPSimpleClose();
        THPSimpleQuit();

        if (bMustFreeBuffer && buffer != NULL)
        {
            nlFree(buffer);
        }

        buffer = NULL;
        pTex[0] = NULL;
        pTex[1] = NULL;
        pTex[2] = NULL;
        pTex[3] = NULL;

        glResourceRelease(resourceMarker);

        if (nlTaskManager::m_pInstance->m_CurrState != 4)
        {
            if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
            {
                AudioStreamTrack::TrackManagerBase* trackMgr = g_pTrackManager;
                // PORT: null when audio is disabled, nothing to ask.
                AudioStreamTrack::StreamTrack* track = trackMgr != NULL ? trackMgr->GetTrack(nlStringLowerHash("Announcer")) : NULL;
                // PORT: null when audio is disabled, so the track is null too.
                if (track != NULL) track->Resume();
            }
        }

        if (g_GrabTextureData != NULL)
        {
            PlatTexture* tex = glx_GetTex(GrabTextureHandle, true, true);
            memcpy(tex->m_SwizzledData, g_GrabTextureData, g_GrabTextureSize);
            tex->Prepare();
            nlVirtualFree(g_GrabTextureData);
            g_GrabTextureData = NULL;
        }

        g_bActive = false;
        return false;
    }

    start = true;
    g_bActive = true;
    return true;
}

/**
 * Offset/Address/Size: 0x124 | 0x801CB8E0 | size: 0x168
 */
bool MovieStop()
{
    if (!g_bActive)
    {
        return false;
    }

    if (glx_GetSharedLock())
    {
        glx_UnlockSharedMemory();
    }

    int lastFrame = THPSimpleGetTotalFrame() - 1;
    int currentFrame = (int)(unsigned int)THPSimpleGetCurrentFrame();
    if ((unsigned int)currentFrame != (unsigned int)lastFrame)
    {
        tDebugPrintManager::Print(DC_RENDER, "MOVIE did not finish playback.\n");
    }

    THPSimpleAudioStop();
    THPSimpleLoadStop();
    THPSimpleClose();
    THPSimpleQuit();

    if (bMustFreeBuffer && buffer != NULL)
    {
        nlFree(buffer);
    }

    buffer = NULL;
    pTex[0] = NULL;
    pTex[1] = NULL;
    pTex[2] = NULL;
    pTex[3] = NULL;

    glResourceRelease(resourceMarker);

    if (nlTaskManager::m_pInstance->m_CurrState != 4)
    {
        if (GameInfoManager::GetInstance()->mIsInStrikers101Mode)
        {
            AudioStreamTrack::TrackManagerBase* trackMgr = g_pTrackManager;
            // PORT: null when audio is disabled, nothing to ask.
            AudioStreamTrack::StreamTrack* track = trackMgr != NULL ? trackMgr->GetTrack(nlStringLowerHash("Announcer")) : NULL;
            // PORT: null when audio is disabled, so the track is null too.
            if (track != NULL) track->Resume();
        }
    }

    if (g_GrabTextureData != NULL)
    {
        PlatTexture* tex = glx_GetTex(GrabTextureHandle, true, true);
        memcpy(tex->m_SwizzledData, g_GrabTextureData, g_GrabTextureSize);
        tex->Prepare();
        nlVirtualFree(g_GrabTextureData);
        g_GrabTextureData = NULL;
    }

    g_bActive = false;
    return true;
}

/**
 * Offset/Address/Size: 0x3C | 0x801CB7F8 | size: 0xE8
 */
bool MoviePlay()
{
    if (!g_bActive)
    {
        return false;
    }

    if (start)
    {
        if (buffer == (unsigned char*)glx_GetSharedMemory())
        {
            glx_LockSharedMemory();
        }
    }

    if (g_bMovieMustStop)
    {
        return false;
    }

    GXSetDrawDone();
    GXFlush();
    nlServiceFileSystem();
    OSYieldThread();
    GXWaitDrawDone();

    int error = THPSimpleDecode(0);
    if (error == 1 || error == 2)
    {
        return false;
    }

    GXInvalidateTexAll();
    pTex[0]->Prepare();
    pTex[1]->Prepare();
    pTex[2]->Prepare();

    if (start)
    {
        THPSimpleAudioStart();
        start = false;
    }

    return true;
}

/**
 * Offset/Address/Size: 0x0 | 0x801CB7BC | size: 0x3C
 */
bool IsMoviePlayingInStrikers101()
{
    if (nlTaskManager::m_pInstance->m_CurrState != 4
        && nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode
        && g_bActive)
    {
        return true;
    }
    return false;
}
