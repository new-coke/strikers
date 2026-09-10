#include "Game/GameRenderTask.h"

#include "types.h"

#include "Game/BasicStadium.h"
#include "Game/CharacterTemplate.h"
#include "Game/Debug/ShapeRender.h"
#include "Game/FixedUpdateTask.h"
#include "Game/GL/GLInventory.h"
#include "Game/Game.h"
#include "Game/GameObjectLighting.h"
#include "Game/ObjectBlur.h"
#include "Game/Render/CrowdManager.h"
#include "Game/Render/ElectricFence.h"
#include "Game/Render/FlareHandler.h"
#include "Game/Render/Indicators.h"
#include "Game/Render/Jumbotron.h"
#include "Game/Render/Presentation.h"
#include "Game/Render/RenderShadow.h"
#include "Game/Render/ShootToScoreArrow.h"
#include "Game/Render/ShootToScoreMeter.h"
#include "Game/Render/depthoffield.h"
#include "Game/ReplayManager.h"
#include "Game/WorldManager.h"

#include "NL/MemAlloc.h"
#include "NL/nlConfig.h"
#include "NL/nlTask.h"
#include "NL/nlTicker.h"
#include "NL/gl/glConstant.h"
#include "NL/gl/glDraw2.h"
#include "NL/gl/glFont.h"
#include "NL/gl/glModify.h"
#include "NL/gl/glState.h"
#include "NL/gl/glTexture.h"

class PhotoFlash
{
public:
    static void Render(float);
};

// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
u8 g_bWhiteDiffuse;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
u8 g_TexDetail = 1;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
u8 g_TexShadow = 1;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
u8 g_TexSelfIllum = 1;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
u8 g_TexGloss = 1;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
u8 g_bTexelDensity;
static int g_LightMult = 2;
static float g_WarbleDivisor = 32.0f;
bool g_bRenderWorld = true;
static nlVector4 world_ambient = { 0.0f, 0.0f, 0.0f, 0.0f };
static u8 g_bWarblePreview;
static u8 g_bWarbleShow;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
u8 g_bMemoryOnScreen;

static const unsigned long GLTT_Detail_bit = 1UL << 1;
static const unsigned long GLTT_Shadow_bit = 1UL << 2;
static const unsigned long GLTT_SelfIllum_bit = 1UL << 3;
static const unsigned long GLTT_Gloss_bit = 1UL << 4;

static inline void WarbleTest()
{
    glPoly2 p;
    if (!glTextureLoad(glGetTexture("target/warble")))
    {
        return;
    }

    nlColour white = { 0xFF, 0xFF, 0xFF, 0xFF };
    glSetDefaultState(false);
    glSetCurrentTexture(glGetTexture("target/warble"), GLTT_Diffuse);

    float warblePosX = 512.0f;
    float warblePosY = 32.0f;
    float warbleSize = 80.0f;
    p.SetupRectangle(warblePosX, warblePosY, warbleSize, warbleSize, 0.0f);
    p.SetColour(white);

    if (g_bWarblePreview)
    {
        p.Attach((eGLView)0x1A, 0, 0, (u32)-1);
    }

    u32 showTex = glGetTexture(g_bWarbleShow ? "target/offset" : "target/colour");
    glSetCurrentTexture(showTex, GLTT_Diffuse);
    glSetTextureState(GLTS_DiffuseWrap, 3);
    glSetCurrentTextureState(glHandleizeTextureState());

    p.FullCoverage(white, 0.0f);
    p.Attach(GLV_WarbleBlend, 0, 0, (u32)-1);
}

static void inline MemoryOnScreen(unsigned int cutoff)
{
    float x0;
    float x1;
    float y0;
    float y1;

    if (StandardAllocator.TotalFreeMemory() > cutoff)
    {
        return;
    }

    nlColour bg = { 0x3A, 0x6E, 0xA5, 0xFF };
    nlColour fg = { 0xFF, 0xFF, 0xFF, 0xFF };

    glFontVirtualPosToScreenCoordPos(0.0f, -2.0f, x0, y0);
    glFontVirtualPosToScreenCoordPos(0.0f, -1.0f, x1, y1);

    // PORT: the frame, not 640.
    g_ShapeRenderer.DrawRectangle2D(0.0f, y0 - 4.0f,
                                   glGetOrthographicWidth(),
                                   8.0f + (y1 - y0), -0.5f, bg,
                                   GLV_FrontEnd);

    glFontBegin(false);
    u32 virtFree;
    u32 stdFree;
    virtFree = VirtualAllocator.LargestFreeBlock() >> 10;
    stdFree = StandardAllocator.LargestFreeBlock() >> 10;
    u32 totalFree = StandardAllocator.TotalFreeMemory() >> 10;
    glFontPrintf((eGLView)0x21, 0, -2, fg, "%u free, %u block, %u virt (KB)", totalFree, stdFree, virtFree);
    glFontEnd();
}

#pragma inline_max_size(0x10000)
#pragma inline_max_total_size(0x10000)
void GameRenderTask::SetupConstants()
{
    nlVector4 vMult = { 0.0f, 0.0f, 0.0f, 0.0f };
    Config& cfg = Config::Global();
    vMult.x = GetConfigFloat(cfg, "shadow_volumes/colour_r", 255.0f) / 255.0f;

    Config& cfg2 = Config::Global();
    vMult.y = GetConfigFloat(cfg2, "shadow_volumes/colour_g", 255.0f) / 255.0f;

    Config& cfg3 = Config::Global();
    vMult.z = GetConfigFloat(cfg3, "shadow_volumes/colour_b", 255.0f) / 255.0f;

    Config& cfg4 = Config::Global();
    vMult.w = GetConfigFloat(cfg4, "shadow_volumes/colour_a", 64.0f) / 255.0f;

    glConstantSet("shadow/pass0_colour", vMult);
    glConstantSet("shadow/pass1_colour", vMult);
    glConstantSet("lighting/ambient_colour", world_ambient);

    nlVector4 svolColour = { 0.0f, 0.0f, 0.0f, 0.0f };
    svolColour.x = (float)g_LightMult;
    glConstantSet("lighting/range", svolColour);

    nlVector4 vWarble = { 0.0f, 0.0f, 0.0f, 0.0f };
    vWarble.x = g_WarbleDivisor;
    glConstantSet("warble/divisor", vWarble);

    nlVector4 vTexel = { 0.0f, 0.0f, 0.0f, 0.0f };
    vTexel.x = g_bTexelDensity ? 1.0f : 0.0f;
    glConstantSet("texture/density", vTexel);
}

/**
 * Offset/Address/Size: 0x0 | 0x80170BAC | size: 0x9E8
 */
void GameRenderTask::Run(float fDeltaT)
{
    static unsigned int lastTick;
    static s8 init;
    static float prevTime;

    int shadowIndex;
    u32 whiteTexture;
    glPoly2 p;
    float w;
    float h;
    u32 mask;

    if (g_bMemoryOnScreen)
    {
        MemoryOnScreen((u32)-1);
    }

    UpdateGameObjectLighting();

    uintptr_t hasWorld = (uintptr_t)WorldManager::s_World;
    if (((-hasWorld) | hasWorld) >> 31)
    {
        nlVector3 shadowPos = { 0.0f, 0.0f, 0.0f };
        ((BasicStadium*)WorldManager::s_World)->m_pShadowLight = WorldManager::s_World->GetShadowLight(shadowPos, 10000000000.0f);
    }

    for (shadowIndex = 0; shadowIndex < MaxProjectedShadows; shadowIndex++)
    {
        SetCharacterShadowUpdated(shadowIndex, false);
    }

    if (!g_bRenderWorld)
    {
        return;
    }

    if (FixedUpdateTask::mfFrameLockTime > 0.0f)
    {
        FixedUpdateTask::DecrementFrameLock(fDeltaT);
    }

    Presentation::Instance().Update(fDeltaT);

    whiteTexture = glGetTexture("global/white");

    SetupConstants();

    float dt = (nlTaskManager::m_pInstance->m_CurrState == 1) ? 0.0f : fDeltaT;

    Jumbotron::instance.Update(dt);
    CrowdManager::instance.Update(dt);
    FlareHandler::instance.Render();

    for (int charIndex = 0; charIndex < 10; charIndex++)
    {
        if (g_pCharacters[charIndex] != 0)
        {
            g_pCharacters[charIndex]->UpdateBlinking(dt);
        }
    }

    if (g_bWhiteDiffuse)
    {
        gl_ModifyAddMapping(GLMod_DiffuseTex, whiteTexture);
    }

    if (g_bTexelDensity)
    {
        if (glTextureLoad(whiteTexture))
        {
            w = (float)(s32)glTextureGetWidth();
            h = (float)(s32)glTextureGetHeight();
            p.SetupRectangle(24.0f, 24.0f, w, h, 0.0f);

            WorldDarkening::SetPolyColour(p, 0xFF, 0xFF, 0xFF, 0xFF);

            glSetDefaultState(false);
            glSetCurrentTexture(whiteTexture, GLTT_Diffuse);
            glSetTextureState(GLTS_DiffuseWrap, 3);
            glSetCurrentTextureState(glHandleizeTextureState());
            p.Attach((eGLView)0x1A, 0, 0, (u32)-1);
        }
    }

    mask = 0;
    if (!g_TexDetail)
    {
        mask |= GLTT_Detail_bit;
    }
    if (!g_TexShadow)
    {
        mask |= GLTT_Shadow_bit;
    }
    if (!g_TexSelfIllum)
    {
        mask |= GLTT_SelfIllum_bit;
    }
    if (!g_TexGloss)
    {
        mask |= GLTT_Gloss_bit;
    }

    if (mask)
    {
        gl_ModifyAddMapping(GLMod_TextureMask, ~mask);
    }

    WorldManager::RenderWorld();

    if (nlTaskManager::m_pInstance->m_CurrState != 4)
    {
        ReplayManager::Instance()->RenderSnapshotAt(fDeltaT);
    }

    gl_ModifyClearMappings();

    nlTaskManager* taskMgr = nlTaskManager::m_pInstance;
    if (taskMgr->m_CurrState == 2 || (taskMgr->m_CurrState == 1 && taskMgr->m_PrevState == 2))
    {
        glInventory.Update(fDeltaT);

        if (!g_pGame->mbCaptainShotToScoreOn)
        {
            float indicatorDt;
            if (nlTaskManager::m_pInstance->m_CurrState == 2)
            {
                indicatorDt = fDeltaT;
            }
            else
            {
                indicatorDt = 0.0f;
            }
            UpdateAndRenderIndicators(indicatorDt);
        }

        WorldDarkening::Instance().UpdateAndRender(fDeltaT);
        ShootToScoreMeter::instance.UpdateAndRender(fDeltaT);
    }
    else if (taskMgr->m_CurrState == 0x100)
    {
        if (!init)
        {
            lastTick = nlGetTicker();
            init = 1;
        }

        u32 tick = nlGetTicker();
        glInventory.Update(0.001f * nlGetTickerDifference(lastTick, tick));
        lastTick = tick;
    }
    else
    {
        float replayTime = ReplayManager::Instance()->mTime;
        float delta = replayTime - prevTime;
        delta = fabsf(delta);
        glInventory.Update(delta);
        prevTime = ReplayManager::Instance()->mTime;
    }

    DepthOfFieldManager::instance.Update();

    if ((nlTaskManager::m_pInstance->m_CurrState & 0x00020010) == 0)
    {
        BlurManager::Update(fDeltaT);
    }

    WarbleTest();

    PhotoFlash::Render(fDeltaT);
    UpdateElectricFence(fDeltaT);
}
