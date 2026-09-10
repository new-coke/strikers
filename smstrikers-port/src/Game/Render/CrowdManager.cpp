#include "Game/Render/CrowdManager.h"

#include "Game/NisPlayer.h"
#include "NL/gl/glConstant.h"
#include "NL/gl/glMemory.h"
#include "NL/gl/glState.h"
#include "NL/gl/glTexture.h"
#include "NL/gc/gcSwizzler.h"
#include "NL/glx/glxTexture.h"
#include "NL/nlMath.h"
#include "NL/nlPrint.h"
#include "NL/nlString.h"

static void CrowdBundleLoad_cb(void*, unsigned long, void*);

CrowdManager CrowdManager::instance;

/**
 * Offset/Address/Size: 0x978 | 0x801647F4 | size: 0x10C
 */
void CrowdManager::Initialize()
{
    m_State = Crowd_Idle;
    m_fTime = 0.0f;
    m_fAnimScale = 1.0f;
    m_CurrentTexture = -1;
    m_nCurrentFrame = -1;
    m_nNumFrames = 8;

    nlStrNCpy(m_szTexture, "idle/idle", 64);
    m_TextureHandle = glGetTexture(m_szTexture);

    GCTextureSize(GXTex_CMPR, 256, 512, 6, -1); // Result discarded
    m_BundleLoadBase = (uintptr_t)glResourceAlloc(0x155A0, GLM_TextureData);

    PlatTexture* platTex = glx_CreatePlatTexture();
    platTex->CreateWithMemory(256, 512, GXTex_CMPR, 6, (void*)(m_BundleLoadBase + 0x60));

    nlZeroMemory(platTex->m_SwizzledData, GCTextureSize(platTex->m_Format, platTex->m_Width, platTex->m_Height, platTex->m_Levels, -1));

    platTex->Prepare();
    glx_AddTex(m_TextureHandle, platTex);

    m_szStadium[0] = '\0';
}

/**
 * Offset/Address/Size: 0x974 | 0x801647F0 | size: 0x4
 */
void CrowdManager::Uninitialize()
{
}

inline void CrowdManager::SetFrameConstant()
{
    float frameValue = (float)(u32)m_nCurrentFrame / 8.0f;
    nlVector4 frameVector = { frameValue, frameValue, frameValue, frameValue };
    glConstantSet("crowd/frame", frameVector);
}

/**
 * Offset/Address/Size: 0x938 | 0x801647B4 | size: 0x3C
 */
void CrowdManager::SetStadium(const char* stadium)
{
    if (stadium == nullptr)
    {
        m_szStadium[0] = '\0';
    }
    else
    {
        nlStrNCpy(m_szStadium, stadium, 64);
    }
}

/**
 * Offset/Address/Size: 0x918 | 0x80164794 | size: 0x20
 */
u32 CrowdManager::GetTextureHandle(unsigned long id) const
{
    if (m_CurrentTexture == 0xFFFFFFFF)
    {
        return -1;
    }
    return m_TextureHandle;
}

/**
 * Offset/Address/Size: 0x79C | 0x80164618 | size: 0x17C
 */
void CrowdManager::Replay(LoadFrame& frame)
{
    int replayState = 0;
    Replayable<1, LoadFrame, int>(frame, replayState);

    if (replayState != (int)m_State)
    {
        m_State = (eCrowdState)replayState;
        const char* szBundle = NULL;
        m_fTime = 0.0f;

        switch (m_State)
        {
        case Crowd_Idle:
            szBundle = "idle";
            break;
        case Crowd_Happy:
            szBundle = "idle";
            break;
        case Crowd_Excited:
            szBundle = "happy";
            break;
        default:
            break;
        }

        char szFilename[64];
        if (m_szStadium[0] == '\0')
        {
            nlSNPrintf(szFilename, 64, "crowd/%s.glt", szBundle);
        }
        else
        {
            nlSNPrintf(szFilename, 64, "crowd/%s_%s.glt", m_szStadium, szBundle);
        }

        glBeginLoadTextureBundle(szFilename, CrowdBundleLoad_cb, (void*)m_BundleLoadBase);

        m_nCurrentFrame = 0;
        nlStrNCpy(m_szTexture, "idle/idle", 64);
        SetFrameConstant();
    }
}

/**
 * Offset/Address/Size: 0x76C | 0x801645E8 | size: 0x30
 */
void CrowdManager::Replay(SaveFrame& frame)
{
    int state = m_State;
    Replayable<1, SaveFrame, int>(frame, state);
}

/**
 * Offset/Address/Size: 0x70C | 0x80164588 | size: 0x60
 */
static void CrowdBundleLoad_cb(void*, unsigned long, void*)
{
    if (!glTextureLoad(glGetTexture("idle/idle")))
        return;

    f32 animScale;
    switch (CrowdManager::instance.m_State)
    {
    case Crowd_Happy:
        animScale = 1.625f;
        break;
    default:
        animScale = 1.0f;
        break;
    }
    CrowdManager::instance.m_fAnimScale = animScale;
}
/**
 * Offset/Address/Size: 0x5A0 | 0x8016441C | size: 0x16C
 */
void CrowdManager::SetState(eCrowdState state, bool force)
{
    if (state == m_State && !force)
    {
        return;
    }

    m_State = state;
    const char* szBundle = NULL;
    m_fTime = 0.0f;

    switch (m_State)
    {
    case Crowd_Idle:
        szBundle = "idle";
        break;
    case Crowd_Happy:
        szBundle = "idle";
        break;
    case Crowd_Excited:
        szBundle = "happy";
        break;
    default:
        break;
    }

    char szFilename[64];
    if (m_szStadium[0] == '\0')
    {
        nlSNPrintf(szFilename, 64, "crowd/%s.glt", szBundle);
    }
    else
    {
        nlSNPrintf(szFilename, 64, "crowd/%s_%s.glt", m_szStadium, szBundle);
    }

    glBeginLoadTextureBundle(szFilename, CrowdBundleLoad_cb, (void*)m_BundleLoadBase);

    m_nCurrentFrame = 0;
    nlStrNCpy(m_szTexture, "idle/idle", 64);
    SetFrameConstant();
}

/**
 * Offset/Address/Size: 0x480 | 0x801642FC | size: 0x120
 */
void CrowdManager::Update(float deltaTime)
{
    float adjustedDeltaTime = deltaTime;

    if (NisPlayer::Instance()->WorldIsFrozen())
    {
        adjustedDeltaTime = 0.0f;
    }

    m_fTime += m_fAnimScale * adjustedDeltaTime;
    if (m_CurrentTexture == 0xFFFFFFFF)
    {
        m_CurrentTexture = glGetTexture(m_szTexture);
    }

    if (m_fTime > 0.13333334f)
    {
        m_fTime = 0.0f;

        int currentFrame = m_nCurrentFrame + 1;
        m_nCurrentFrame = currentFrame;
        if (currentFrame >= m_nNumFrames)
        {
            m_nCurrentFrame = 0;
        }

        SetFrameConstant();
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80163E7C | size: 0x480
 */
void CrowdManager::EventHandler(Event* event)
{
    s32 eventID = (s32)event->m_uEventID;

    switch (eventID)
    {
    case 10:
        SetState(Crowd_Idle, false);
        break;
    case 9:
        SetState(Crowd_Idle, true);
        break;
    case 5:
    {
        EventData* gsd;
        s32 id = port_event_data_id(&event->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            gsd = NULL;
        }
        else
        {
            id = port_event_data_id(&event->m_data);
            if (id != 0x18A)
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                gsd = NULL;
            }
            else
            {
                gsd = &event->m_data;
            }
        }

        if (gsd != NULL)
        {
            SetState(Crowd_Excited, false);
        }
        break;
    }
    }
}
