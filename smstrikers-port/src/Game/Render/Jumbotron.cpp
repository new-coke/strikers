#include "Game/Render/Jumbotron.h"

#include "NL/gl/gl.h"
#include "NL/gl/glState.h"
#include "NL/nlDebug.h"
#include "NL/nlPrint.h"
#include "NL/nlString.h"
#include "NL/nlFileGC.h"
#include "NL/gc/gcSwizzler.h"
#include "NL/gl/glMemory.h"
#include "dolphin/os/OSThread.h"

extern bool g_e3_Build;

Jumbotron Jumbotron::instance;

void* g_TrophyTextureLocationInMemory;
static u32 g_nMaxNumFrames;

const float StateTimeScale[4] = {
    1.0f,
    1.0f,
    2.0f,
    4.0f
};

/**
 * Offset/Address/Size: 0x53C | 0x80163D24 | size: 0x158
 */
void Jumbotron::Initialize()
{
    unsigned long dataInc;
    unsigned long dataOffset;

    m_State = JState_Nothing;
    m_AnimationClass = Jumbo_Nothing;
    m_fTime = 0.0f;
    m_fFramerate = 0.13333334f;
    m_CurrentTexture = -1;
    m_szPrefix[0] = 0;
    m_szTexture[0] = 0;
    m_nCurrentFrame = -1;
    m_nNumFrames = 0;

    dataInc = GCTextureSize(GXTex_CMPR, 0x80, 0x80, 5, -1);
    u32 texSize = 0x1E000;
    u32 minSize = ((dataInc + 0x20) << 4) + 0x120;
    if (minSize >= 0x1E000)
    {
        texSize = minSize;
    }

    m_BundleLoadBase = glResourceAlloc(texSize, GLM_TextureData);
    g_TrophyTextureLocationInMemory = m_BundleLoadBase;

    unsigned long dataStep = dataInc + 0x20;
    dataOffset = 0x120;

    for (int frame = 0; frame < 16; frame++)
    {
        nlSNPrintf(m_szTexture, 0x80, "kickoff/frame%03d", frame);
        PlatTexture* platTex = glx_CreatePlatTexture();
        platTex->CreateWithMemory(0x80, 0x80, GXTex_CMPR, 5, (void*)((u8*)m_BundleLoadBase + dataOffset + 0x20));
        nlZeroMemory(platTex->m_SwizzledData,
            GCTextureSize(platTex->m_Format, platTex->m_Width, platTex->m_Height, platTex->m_Levels, -1));

        platTex->Prepare();

        glx_AddTex(glGetTexture(m_szTexture), platTex);

        dataOffset += dataStep;
    }

    m_nMaxNumFrames = 16;
    g_nMaxNumFrames = 16;
}

/**
 * Offset/Address/Size: 0x538 | 0x80163D20 | size: 0x4
 */
void Jumbotron::Uninitialize()
{
    // Empty
}

/**
 * Offset/Address/Size: 0x45C | 0x80163C44 | size: 0xDC
 */
void Jumbotron::Reset()
{
    if (m_State == JState_PlayingButWaiting)
    {
        if (m_State == JState_Loading || m_State == JState_PlayingButWaiting)
        {
            while (nlAsyncReadsPending(NULL))
            {
                OSYieldThread();
                nlServiceFileSystem();
            }
        }
        m_State = JState_Ready;
    }

    switch (m_State)
    {
    case 2:
    case 0:
        break;
    case 4:
        m_State = JState_Ready;
        m_CurrentTexture = -1;
        break;
    default:
        nlBreak();
        break;
    }

    m_State = JState_Nothing;
    m_AnimationClass = Jumbo_Nothing;
    m_fTime = 0.0f;
    m_fFramerate = 0.13333334f;
    m_CurrentTexture = -1;
    m_szPrefix[0] = 0;
    m_szTexture[0] = 0;
    m_nCurrentFrame = -1;
    m_nNumFrames = 0;
}

/**
 * Offset/Address/Size: 0x3DC | 0x80163BC4 | size: 0x80
 */
static void BundleLoad_cb(void*, unsigned long, void*)
{
    Jumbotron* j = &Jumbotron::instance;

    j->m_nCurrentFrame = 0;
    j->m_nNumFrames = j->m_nMaxNumFrames;

    nlStrNCpy<char>(j->m_szTexture, "kickoff/frame000", 0x80);

    j->m_CurrentTexture = -1;

    if (j->m_State == JState_PlayingButWaiting)
    {
        j->m_State = JState_Playing;
    }
    else
    {
        j->m_State = JState_Ready;
    }
}

/**
 * Offset/Address/Size: 0x234 | 0x80163A1C | size: 0x1A8
 */
void Jumbotron::BeginLoad()
{
    if (m_State != JState_Nothing)
    {
        eJumboType tempAnimationClass = m_AnimationClass;
        if (m_State == JState_PlayingButWaiting)
        {
            if (m_State == JState_Loading || m_State == JState_PlayingButWaiting)
            {
                while (nlAsyncReadsPending(NULL))
                {
                    OSYieldThread();
                    nlServiceFileSystem();
                }
            }
            m_State = JState_Ready;
        }

        switch (m_State)
        { /* irregular */
        case 2:
        case 0:
            break;
        case 4:
            m_State = JState_Ready;
            m_CurrentTexture = -1;
            break;
        default:
            nlBreak();
            break;
        }

        m_State = JState_Nothing;
        m_AnimationClass = Jumbo_Nothing;
        m_fTime = 0.0f;
        m_fFramerate = 0.13333334f;
        m_CurrentTexture = -1;
        m_szPrefix[0] = 0;
        m_szTexture[0] = 0;
        m_nCurrentFrame = -1;
        m_nNumFrames = 0;
        m_AnimationClass = tempAnimationClass;
    }

    m_State = JState_Loading;
    const char* var_r4 = NULL;
    m_fTime = 0.0f;
    if (g_e3_Build != 0)
    {
        if (m_AnimationClass == Jumbo_Kickoff)
        {
            var_r4 = "kickoff";
        }
        else
        {
            var_r4 = "goal";
        }
    }
    else
    {
        switch (m_AnimationClass)
        {
        case Jumbo_Kickoff:
            var_r4 = "kickoff";
            break;
        case Jumbo_Goal:
            var_r4 = "goal";
            break;
        case Jumbo_Wins:
            var_r4 = "wins";
            break;
        }
    }

    char szTextureBundle[0x80];
    nlStrNCpy<char>(m_szPrefix, var_r4, 0x40);
    nlSNPrintf(szTextureBundle, 0x80, "jumbotron/%s.glt", m_szPrefix);
    glBeginLoadTextureBundle(szTextureBundle, BundleLoad_cb, m_BundleLoadBase);
}

/**
 * Offset/Address/Size: 0x1E8 | 0x801639D0 | size: 0x4C
 */
void Jumbotron::WaitForLoad()
{
    if (m_State == JState_Loading || m_State == JState_PlayingButWaiting)
    {
        while (nlAsyncReadsPending(NULL))
        {
            OSYieldThread();
            nlServiceFileSystem();
        }
    }
}

/**
 * Offset/Address/Size: 0xE4 | 0x801638CC | size: 0x104
 */
void Jumbotron::BeginPlaying()
{
    if (m_State == JState_Nothing)
    {
        if (m_State == JState_PlayingButWaiting)
        {
            if (m_State == JState_Loading || m_State == JState_PlayingButWaiting)
            {
                while (nlAsyncReadsPending(NULL))
                {
                    OSYieldThread();
                    nlServiceFileSystem();
                }
            }
            m_State = JState_Ready;
        }
        switch (m_State)
        {
        case 2:
        case 0:
            break;
        case 4:
            m_State = JState_Ready;
            m_CurrentTexture = -1;
            break;
        default:
            nlBreak();
            break;
        }
        m_State = JState_Nothing;
        m_AnimationClass = Jumbo_Nothing;
        m_fTime = 0.0f;
        m_fFramerate = 0.13333334f;
        m_CurrentTexture = -1;
        m_szPrefix[0] = 0;
        m_szTexture[0] = 0;
        m_nCurrentFrame = -1;
        m_nNumFrames = 0;
        return;
    }

    if (m_State == JState_Loading)
    {
        m_State = JState_PlayingButWaiting;
        return;
    }

    m_State = JState_Playing;
}

/**
 * Offset/Address/Size: 0xD0 | 0x801638B8 | size: 0x14
 */
void Jumbotron::StopPlaying()
{
    m_State = JState_Ready;
    m_CurrentTexture = -1;
}

/**
 * Offset/Address/Size: 0x0 | 0x801637E8 | size: 0xD0
 */
void Jumbotron::Update(float dt)
{
    if (m_State == JState_Playing)
    {
        m_fTime += dt * StateTimeScale[m_AnimationClass];
        if (m_CurrentTexture == (u32)-1)
        {
            m_CurrentTexture = glGetTexture(m_szTexture);
        }
        if (m_fTime > m_fFramerate)
        {
            m_fTime = 0.0f;
            s32 currentFrame = m_nCurrentFrame + 1;
            m_nCurrentFrame = currentFrame;
            if (currentFrame >= m_nNumFrames)
            {
                m_nCurrentFrame = 0;
            }
            nlSNPrintf(m_szTexture, 0x80, "kickoff/frame%03d", m_nCurrentFrame);
            m_CurrentTexture = glGetTexture(m_szTexture);
        }
    }
}

// Force .data string-literal order to match target. With -inline deferred, this
// end-of-file stub emits first, so its references fix the @NNN allocation order:
// "kickoff/frame%03d", "kickoff/frame000", "jumbotron/%s.glt".
static void Jumbotron_stub()
{
    nlSNPrintf(NULL, 0, "kickoff/frame%03d");
    nlStrNCpy<char>((char*)NULL, "kickoff/frame000", 0);
    nlSNPrintf(NULL, 0, "jumbotron/%s.glt");
}
