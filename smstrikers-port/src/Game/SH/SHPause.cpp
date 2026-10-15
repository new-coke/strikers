#include "Game/SH/SHPause.h"
#include "Game/OverlayManager.h"
#include "Game/SH/SHLessonSelect.h"

#include "Game/FE/FEAudio.h"
#include "Game/FE/Overlay/OverlayHandlerSummary.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feManager.h"
#include "Game/FE/feSceneManager.h"
#include "Game/FE/fePopupMenu.h"
#include "Game/FE/tlTextInstance.h"
#include "Game/GameInfo.h"
#include "Game/Game.h"
#include "NL/gl/gl.h"
#include "NL/glx/glxSwap.h"
#include "NL/nlLocalization.h"
#include "NL/nlPrint.h"
#include "NL/nlTask.h"
#include "port/config.h"

#include <cstdio>

extern FEInput* g_pFEInput;
extern nlColour MenuHighliteColour;

extern bool g_bRenderWorld;
extern bool g_bFrameStatsOnScreen;
extern unsigned char g_bShadowBlobs;
extern unsigned char g_bClipToFrustum;
extern unsigned char g_bWhiteDiffuse;
extern unsigned char g_TexDetail;
extern unsigned char g_TexShadow;
extern unsigned char g_TexSelfIllum;
extern unsigned char g_TexGloss;
extern bool g_bShadowVolumes;
extern bool g_bEnableDrawableModel;
extern bool g_bDrawPlanarShadows;
extern bool g_bBallGlow;
extern bool g_bEnableDrawableSkinModel;
extern unsigned char g_hudVisible;
extern bool g_bAllowLighting;
extern bool g_bAllowSpecular;

extern "C" unsigned int aurora_vita_debug_runtime_flags(void) noexcept;
extern "C" unsigned int aurora_vita_debug_runtime_capabilities(void) noexcept;
extern "C" void aurora_vita_debug_set_runtime_flags(unsigned int flags) noexcept;
extern "C" int aurora_vita_debug_shader_runtime_compile(void) noexcept;
extern "C" void aurora_vita_debug_set_shader_runtime_compile(int enabled) noexcept;
extern "C" int PortDebugRenderViewEnabled(unsigned int view);
extern "C" void PortDebugRenderViewToggle(unsigned int view);
extern "C" unsigned int PortDebugRenderViewLastUs(unsigned int view);


static char sPauseOutSlide[] = "out";
eFEINPUT_PAD PauseMenuScene::mControllingInput = FE_ALL_PADS;
float PauseMenuScene::mDelayBeforeUnpause = 0.1f;
u32 PauseMenuScene::mLastTaskManagerState;
s32 PauseMenuScene::mLastSelectedIndex;
bool PauseMenuScene::mDebugMenuRequested;
s32 PauseMenuScene::mDebugPage;

namespace DoubleHighlite
{
void OpenItem(TLComponentInstance*);
}

#include "NL/nlBind.h"

typedef Detail::MemFunImpl<void, void (PauseMenuScene::*)()> MemFunImpl_Pause_v_t;
typedef Detail::MemFunImpl<void, void (PauseMenuScene::*)(TLComponentInstance*)> MemFunImpl_Pause_p_t;
typedef BindExp1<void, MemFunImpl_Pause_v_t, PauseMenuScene*> BindExp1_Pause_t;
typedef BindExp2<void, MemFunImpl_Pause_p_t, PauseMenuScene*, Placeholder<0> > BindExp2_Pause_t;

namespace
{
enum DebugRuntimeBits : unsigned int
{
    DR_STATIC_GEOMETRY = 1u << 0,
    DR_STREAMED_VERTEX = 1u << 1,
    DR_LIT_VERTEX = 1u << 2,
    DR_DYNAMIC_TEX_MTX = 1u << 3,
    DR_BUMP_VERTEX = 1u << 4,
    DR_PRIMITIVE_EXPAND = 1u << 5,
    DR_STATIC_STABLE_ONLY = 1u << 6,
};

static const int kDebugBasePageCount = 10;
static const char* const kDebugPageNames[kDebugBasePageCount] = {
    "RENDER", "SHADOWS", "SHADERS", "EFFECTS",
    "VIEWS A", "VIEWS B", "VIEWS C", "POST FX",
    "GXM", "ADVANCED"
};
static unsigned short sDebugMenuText[6][64];

static int DebugPageCount() { return 8; }

static const char* DebugPageName(int page)
{
    return page < kDebugBasePageCount ? kDebugPageNames[page] : "FRAGMENT SHADERS";
}

struct DebugViewEntry
{
    eGLView view;
    const char* label;
};

static const DebugViewEntry kDebugViewPages[4][4] = {
    {
        { GLV_Characters, "CHARACTERS" },
        { GLV_Shadowed, "SHADOWED" },
        { GLV_WorldShadowed, "WORLD SHADOWED" },
        { GLV_Unshadowed, "UNSHADOWED" },
    },
    {
        { GLV_Skybox, "SKYBOX" },
        { GLV_ShadowTexture, "SHADOW TEXTURE" },
        { GLV_Shadow0, "SHADOW 0" },
        { GLV_Shadow1, "SHADOW 1" },
    },
    {
        { GLV_Particles, "PARTICLES" },
        { GLV_LingeringParticles, "LINGERING PARTICLES" },
        { GLV_CoPlanar, "COPLANAR" },
        { GLV_UnsortedPerspective, "UNSORTED PERSPECT" },
    },
    {
        { GLV_Warble, "WARBLE" },
        { GLV_DepthOfField, "DEPTH OF FIELD" },
        { GLV_ElectricFence, "ELECTRIC FENCE" },
        { GLV_CameraSpace, "CAMERA SPACE" },
    },
};

static const char* OnOff(bool value)
{
    return value ? "ON" : "OFF";
}

static void ToWide(const char* src, unsigned short* dst, unsigned int capacity)
{
    if (capacity == 0)
        return;
    unsigned int i = 0;
    while (src[i] != '\0' && i + 1 < capacity)
    {
        dst[i] = (unsigned short)(unsigned char)src[i];
        ++i;
    }
    dst[i] = 0;
}

static void FormatDebugLabel(int page, int row, char* out, unsigned int outSize)
{
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
    const unsigned int flags=0,caps=0;
#else
    const unsigned int flags = aurora_vita_debug_runtime_flags();
    const unsigned int caps = aurora_vita_debug_runtime_capabilities();
#endif
    const char* label = "-";
    const char* value = "";

    if (row == 4)
    {
        const int prev = (page + DebugPageCount() - 1) % DebugPageCount();
        std::snprintf(out, outSize, "< PREV  [%s]", DebugPageName(prev));
        return;
    }
    if (row == 5)
    {
        const int next = (page + 1) % DebugPageCount();
        std::snprintf(out, outSize, "NEXT  [%s] >", DebugPageName(next));
        return;
    }


    if (page >= 4 && page <= 7 && row >= 0 && row < 4)
    {
        const DebugViewEntry& entry = kDebugViewPages[page - 4][row];
        const unsigned int us = PortDebugRenderViewLastUs((unsigned int)entry.view);
        std::snprintf(
            out,
            outSize,
            "%s : %s  %u.%u ms",
            entry.label,
            OnOff(PortDebugRenderViewEnabled((unsigned int)entry.view) != 0),
            us / 1000u,
            (us % 1000u) / 100u);
        return;
    }

    switch (page)
    {
    case 0:
        if (row == 0) { label = "WORLD"; value = OnOff(g_bRenderWorld); }
        if (row == 1) { label = "HUD"; value = OnOff(g_hudVisible != 0); }
        if (row == 2) { label = "STATIC MODELS"; value = OnOff(g_bEnableDrawableModel); }
        if (row == 3) { label = "SKINNED MODELS"; value = OnOff(g_bEnableDrawableSkinModel); }
        break;
    case 1:
        if (row == 0) { label = "BLOB SHADOWS"; value = OnOff(g_bShadowBlobs != 0); }
        if (row == 1) { label = "PLANAR SHADOWS"; value = OnOff(g_bDrawPlanarShadows); }
        if (row == 2) { label = "SHADOW VOLUMES"; value = OnOff(g_bShadowVolumes); }
        if (row == 3) { label = "SHADOW TEX STAGE"; value = OnOff(g_TexShadow != 0); }
        break;
    case 2:
        if (row == 0) { label = "LIGHTING"; value = OnOff(g_bAllowLighting); }
        if (row == 1) { label = "SPECULAR"; value = OnOff(g_bAllowSpecular); }
        if (row == 2) { label = "DETAIL STAGE"; value = OnOff(g_TexDetail != 0); }
        if (row == 3) { label = "GLOSS STAGE"; value = OnOff(g_TexGloss != 0); }
        break;
    case 3:
        if (row == 0) { label = "SELF ILLUMINATION"; value = OnOff(g_TexSelfIllum != 0); }
        if (row == 1) { label = "BALL GLOW"; value = OnOff(g_bBallGlow); }
        if (row == 2) { label = "WHITE DIFFUSE"; value = OnOff(g_bWhiteDiffuse != 0); }
        if (row == 3) { label = "FRUSTUM CULLING"; value = OnOff(g_bClipToFrustum != 0); }
        break;
#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
    case 8:
        if (row == 0) { label = "STATIC GEOMETRY GPU"; value = (caps & DR_STATIC_GEOMETRY) ? OnOff((flags & DR_STATIC_GEOMETRY) != 0) : "N/A"; }
        if (row == 1) { label = "STREAMED GPU VERTEX"; value = "LOCKED"; }
        if (row == 2) { label = "LIT FIXED VERTEX"; value = OnOff((flags & DR_LIT_VERTEX) != 0); }
        if (row == 3) { label = "PRIMITIVE EXPANSION"; value = OnOff((flags & DR_PRIMITIVE_EXPAND) != 0); }
        break;
    case 9:
        if (row == 0) { label = "DYNAMIC TEX MATRIX"; value = OnOff((flags & DR_DYNAMIC_TEX_MTX) != 0); }
        if (row == 1) { label = "BUMP FIXED VERTEX"; value = OnOff((flags & DR_BUMP_VERTEX) != 0); }
        if (row == 2) { label = "STATIC STABLE ONLY"; value = OnOff((flags & DR_STATIC_STABLE_ONLY) != 0); }
        if (row == 3) { label = "RUNTIME SHADER COMPILE"; value = OnOff(aurora_vita_debug_shader_runtime_compile() != 0); }
        break;
#endif
    default:
        break;
    }

    std::snprintf(out, outSize, "%s : %s", label, value);
}

static void ToggleDebugOption(int page, int row)
{
#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
    unsigned int flags = aurora_vita_debug_runtime_flags();
#endif
    switch (page)
    {
    case 0:
        if (row == 0) g_bRenderWorld = !g_bRenderWorld;
        if (row == 1) g_hudVisible = g_hudVisible ? 0 : 1;
        if (row == 2) g_bEnableDrawableModel = !g_bEnableDrawableModel;
        if (row == 3) g_bEnableDrawableSkinModel = !g_bEnableDrawableSkinModel;
        break;
    case 1:
        if (row == 0) g_bShadowBlobs = g_bShadowBlobs ? 0 : 1;
        if (row == 1) g_bDrawPlanarShadows = !g_bDrawPlanarShadows;
        if (row == 2) g_bShadowVolumes = !g_bShadowVolumes;
        if (row == 3) g_TexShadow = g_TexShadow ? 0 : 1;
        break;
    case 2:
        if (row == 0) g_bAllowLighting = !g_bAllowLighting;
        if (row == 1) g_bAllowSpecular = !g_bAllowSpecular;
        if (row == 2) g_TexDetail = g_TexDetail ? 0 : 1;
        if (row == 3) g_TexGloss = g_TexGloss ? 0 : 1;
        break;
    case 3:
        if (row == 0) g_TexSelfIllum = g_TexSelfIllum ? 0 : 1;
        if (row == 1) g_bBallGlow = !g_bBallGlow;
        if (row == 2) g_bWhiteDiffuse = g_bWhiteDiffuse ? 0 : 1;
        if (row == 3) g_bClipToFrustum = g_bClipToFrustum ? 0 : 1;
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        if (row >= 0 && row < 4)
        {
            const DebugViewEntry& entry = kDebugViewPages[page - 4][row];
            PortDebugRenderViewToggle((unsigned int)entry.view);
        }
        break;
#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
    case 8:
        if (row == 0 && (aurora_vita_debug_runtime_capabilities() & DR_STATIC_GEOMETRY)) flags ^= DR_STATIC_GEOMETRY;
        if (row == 2) flags ^= DR_LIT_VERTEX;
        if (row == 3) flags ^= DR_PRIMITIVE_EXPAND;
        aurora_vita_debug_set_runtime_flags(flags);
        break;
    case 9:
        if (row == 0) flags ^= DR_DYNAMIC_TEX_MTX;
        if (row == 1) flags ^= DR_BUMP_VERTEX;
        if (row == 2) flags ^= DR_STATIC_STABLE_ONLY;
        if (row < 3) aurora_vita_debug_set_runtime_flags(flags);
        if (row == 3) aurora_vita_debug_set_shader_runtime_compile(!aurora_vita_debug_shader_runtime_compile());
        break;
#endif
    default:
        break;
    }
}
}

/**
 * Offset/Address/Size: 0x225C | 0x800AF754 | size: 0xDC
 */
PauseMenuScene::PauseMenuScene(PauseMenuScene::ScreenContext context)
    : BaseSceneHandler()
    , mContext(context)
    , mGameIsOver(false)
    , mQuitDelay(0.0f)
    , mQuittingController(FE_ALL_PADS)
    , mMenuItems()
    , mTransitionTo(TT_IN)
    , mIsInTransition(false)
    , mStartAnimAtEnd(false)
    , mButtons()
    , mButtons2()
{
    if (mDebugMenuRequested)
    {
        mContext = SC_DEBUG;
        mDebugMenuRequested = false;
    }
    mDelayBeforeUnpause = 0.1f;
}

void PauseMenuScene::OpenDebugMenu()
{
    if (FrontEnd::m_bInPauseMenuState)
        return;
    mDebugPage = 0;
    mLastSelectedIndex = 0;
    mDebugMenuRequested = true;
    FrontEnd::EnterMenuState(FrontEnd::MET_PAUSE);
}

bool PauseMenuScene::DebugMenuRequested()
{
    return mDebugMenuRequested;
}

/**
 * Offset/Address/Size: 0x21AC | 0x800AF6A4 | size: 0xB0
 */
PauseMenuScene::~PauseMenuScene()
{
}

/**
 * Offset/Address/Size: 0x2158 | 0x800AF650 | size: 0x54
 */
void PauseMenuScene::OnSelectRESUME(TLComponentInstance* instance)
{
    TransitionOut(TT_OUT);
    g_pFEInput->Reset();
    FEAudio::PlayAnimAudioEvent("sfx_screen_back", false);
    FEAudio::PlayAnimAudioEvent("sfx_pause_resume", false);
    mLastSelectedIndex = 0;
}

/**
 * Offset/Address/Size: 0x18CC | 0x800AEDC4 | size: 0x88C
 */
void PauseMenuScene::OnSelectQUIT(TLComponentInstance* instance)
{
    FEPopupMenu* popup;

    if (FrontEnd::m_bGameOver)
    {
        OverlayManager::Instance()->Pop();
        OverlayManager::Instance()->Pop();
        OverlayManager::Instance()->Push(OVERLAY_BRAG, SCREEN_FORWARD, false);
    }
    else
    {
        popup = (FEPopupMenu*)OverlayManager::Instance()->Push(OVERLAY_POPUP, SCREEN_NOTHING, false);
        popup->mControlInput = mQuittingController;

        if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
        {
            popup->Create(
                POPUP_INGAME_QUIT_STRIKERS_101,
                Bind<void>(MemFun<PauseMenuScene, void>(&PauseMenuScene::OnSelectPopupYESFORFEIT), this),
                Bind<void>(MemFun<PauseMenuScene, void>(&PauseMenuScene::OnSelectPopupNOFORFEIT), this));
        }
        else if (nlSingleton<GameInfoManager>::Instance()->mCurrentMode == GameInfoManager::GM_FRIENDLY || g_pGame->m_eGameState == GS_END_GAME)
        {
            popup->Create(
                POPUP_INGAME_QUIT_MATCH,
                Bind<void>(MemFun<PauseMenuScene, void>(&PauseMenuScene::OnSelectPopupYESFORFEIT), this),
                Bind<void>(MemFun<PauseMenuScene, void>(&PauseMenuScene::OnSelectPopupNOFORFEIT), this));
        }
        else if (nlSingleton<GameInfoManager>::Instance()->IsInCupMode()
                 || (nlSingleton<GameInfoManager>::Instance()->IsInTournamentMode()
                     && nlSingleton<GameInfoManager>::Instance()->GetPlayingSide((unsigned short)mQuittingController) != -1))
        {
            popup->Create(
                POPUP_INGAME_FORFEIT_MATCH,
                Bind<void>(MemFun<PauseMenuScene, void>(&PauseMenuScene::OnSelectPopupYESFORFEIT), this),
                Bind<void>(MemFun<PauseMenuScene, void>(&PauseMenuScene::OnSelectPopupNOFORFEIT), this));
        }
        else
        {
            popup->Create(
                POPUP_NO_FORFEIT,
                Bind<void>(MemFun<PauseMenuScene, void>(&PauseMenuScene::OnSelectPopupNOFORFEIT), this));
        }
    }
}

/**
 * Offset/Address/Size: 0x1890 | 0x800AED88 | size: 0x3C
 */
void PauseMenuScene::OnSelectCHOOSESIDES(TLComponentInstance* instance)
{
    OverlayManager::Instance()->Push(IGSCENE_CHOOSE_SIDES, SCREEN_FORWARD, true);
}

/**
 * Offset/Address/Size: 0x1854 | 0x800AED4C | size: 0x3C
 */
void PauseMenuScene::OnSelectAUDIOOPTIONS(TLComponentInstance* instance)
{
    OverlayManager::Instance()->Push(IGSCENE_PAUSE_AUDIO, SCREEN_FORWARD, true);
}

/**
 * Offset/Address/Size: 0x1818 | 0x800AED10 | size: 0x3C
 */
void PauseMenuScene::OnSelectVISUALOPTIONS(TLComponentInstance* instance)
{
    OverlayManager::Instance()->Push(IGSCENE_PAUSE_VISUAL, SCREEN_FORWARD, true);
}

/**
 * Offset/Address/Size: 0x17CC | 0x800AECC4 | size: 0x4C
 */
void PauseMenuScene::OnSelectSTATISTICS(TLComponentInstance* instance)
{
    SummaryOverlay* scene = (SummaryOverlay*)OverlayManager::Instance()->Push(OVERLAY_SUMMARY_PAUSE, SCREEN_FORWARD, true);
    scene->m_controllingInput = mControllingInput;
    scene->mButtonState = ButtonComponent::BS_B_ONLY;
}

void PauseMenuScene::OnSelectBRAGGING(TLComponentInstance* instance)
{
}

/**
 * Offset/Address/Size: 0x17C8 | 0x800AECC0 | size: 0x4
 */
void PauseMenuScene::OnSelectPopupNOFORFEIT()
{
}

/**
 * Offset/Address/Size: 0x1684 | 0x800AEB7C | size: 0x144
 */
void PauseMenuScene::OnSelectPopupYESFORFEIT()
{
    GameInfoManager* gameInfoManager;
    s32 quittingSide;

    gameInfoManager = nlSingleton<GameInfoManager>::s_pInstance;

    if (gameInfoManager->mIsInStrikers101Mode)
    {
        mQuitDelay = 1.0f;
        return;
    }

    if (g_pGame->m_eGameState != GS_END_GAME)
    {
        gameInfoManager = nlSingleton<GameInfoManager>::s_pInstance;
        quittingSide = -1;

        if (gameInfoManager->IsInCupMode())
        {
            eTeamID userTeam = gameInfoManager->GetUserSelectedCupTeam();
            if (userTeam == gameInfoManager->GetTeam(0))
            {
                quittingSide = 0;
            }
            else if (userTeam == gameInfoManager->GetTeam(1))
            {
                quittingSide = 1;
            }
        }
        else if (gameInfoManager->IsInTournamentMode())
        {
            quittingSide = gameInfoManager->GetPlayingSide(mQuittingController);
        }

        if (gameInfoManager->IsInCupOrTournamentMode())
        {
            if (quittingSide == 0)
            {
                nlSingleton<StatsTracker>::Instance()->TrackWinner(0);
                gameInfoManager->SetResultsOfLastUserGame((eUserGameResult)0xD);
            }
            else if (quittingSide == 1)
            {
                nlSingleton<StatsTracker>::Instance()->TrackWinner(1);
                gameInfoManager->SetResultsOfLastUserGame((eUserGameResult)0xE);
            }
        }
    }

    mQuitDelay = 1.0f;
}

/**
 * Offset/Address/Size: 0x1640 | 0x800AEB38 | size: 0x44
 */
void PauseMenuScene::OnSelectLESSONS(TLComponentInstance* instance)
{
    LessonSelectScene* scene = (LessonSelectScene*)OverlayManager::Instance()->Push(IGSCENE_LESSON_SELECT, SCREEN_FORWARD, true);
    scene->mStartAnimAtEnd = true;
}

void PauseMenuScene::StartDelayedQuit()
{
    mQuitDelay = 1.0f;
}

/**
 * Offset/Address/Size: 0x84C | 0x800ADD44 | size: 0xDF4
 */
void PauseMenuScene::SceneCreated()
{
    extern bool g_e3_Build;

    typedef Detail::MemFunImpl<void, void (PauseMenuScene::*)(TLComponentInstance*)> PauseMemFun;
    typedef BindExp2<void, PauseMemFun, PauseMenuScene*, Placeholder<0> > PauseBind;
    typedef MenuItem<TLComponentInstance>::Callback MenuCallback;

    FEAudio::EnableSounds(false);

    switch (mContext)
    {
    case SC_REGULAR_PAUSE:
    {
        FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

        static void (PauseMenuScene::* PauseMenuCBs[6])(TLComponentInstance*) = {
            &PauseMenuScene::OnSelectRESUME,
            &PauseMenuScene::OnSelectCHOOSESIDES,
            &PauseMenuScene::OnSelectAUDIOOPTIONS,
            &PauseMenuScene::OnSelectVISUALOPTIONS,
            &PauseMenuScene::OnSelectSTATISTICS,
            &PauseMenuScene::OnSelectQUIT,
        };

        static char* MENU_NAMES[6]
            = { "MENU ITEM1", "MENU ITEM2", "MENU ITEM3", "MENU ITEM6", "MENU ITEM4", "MENU ITEM5" };

        static const bool E3_BUILD_IS_DISABLED_OPTIONS[6] = { false, false, true, true, false, false };

        int i;
        for (i = 0; i < 6; i++)
        {
            TLInstance* instance = FEFinder<TLInstance, 4>::Find<TLSlide>(
                presentation->m_currentSlide,
                InlineHasher(nlStringLowerHash("Layer")),
                InlineHasher(nlStringLowerHash(MENU_NAMES[i])));
            TLComponentInstance* compinstance = (TLComponentInstance*)instance;

            MenuItem<TLComponentInstance>* menuItem = mMenuItems.AddItem(compinstance);

            void (PauseMenuScene::*openCB)(TLComponentInstance*) = &PauseMenuScene::OpenItem;
            {
                MenuCallback openFunc(Bind<void>(MemFun<PauseMenuScene, void, TLComponentInstance*>(openCB), this, placeholder0));
                menuItem->SetCallback(ON_HIGHLIGHT, openFunc);
            }

            {
                MenuCallback closeFunc(Bind<void>(MemFun<PauseMenuScene, void, TLComponentInstance*>(&PauseMenuScene::CloseItem), this, placeholder0));
                menuItem->SetCallback(ON_UNHIGHLIGHT, closeFunc);
            }

            if (PauseMenuCBs[i])
            {
                MenuCallback applyFunc(Bind<void>(MemFun<PauseMenuScene, void, TLComponentInstance*>(PauseMenuCBs[i]), this, placeholder0));
                menuItem->SetCallback(ON_APPLY, applyFunc);
            }

            (void)FindComponent(compinstance->GetActiveSlide(), "highlite");

            if (i == mLastSelectedIndex)
            {
                menuItem->RunCallback(ON_HIGHLIGHT);
            }
            else
            {
                menuItem->RunCallback(ON_UNHIGHLIGHT);

                TLSlide* slide = compinstance->GetActiveSlide();
                compinstance->Update(1.0f + (slide->m_start + slide->m_duration));
            }

            if (i == 5)
            {
                SetQuitTextForJapanese(compinstance);
            }

            if (g_e3_Build)
            {
                menuItem->SetDisabledFlag(E3_BUILD_IS_DISABLED_OPTIONS[i]);
            }
        }

        mMenuItems.SetFlag(1);
        mMenuItems.SetActiveItemIndex(mLastSelectedIndex);
        mMenuItems.RunCallbackOnCurrent(ON_HIGHLIGHT);
        break;
    }
    case SC_101_PAUSE:
    {
        FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();

        static void (PauseMenuScene::* PauseMenuCBs[3])(TLComponentInstance*) = {
            &PauseMenuScene::OnSelectLESSONS,
            &PauseMenuScene::OnSelectRESUME,
            &PauseMenuScene::OnSelectQUIT,
        };

        static char* MENU_NAMES[3] = { "MENU ITEM1", "MENU ITEM2", "MENU ITEM3" };

        int i;
        for (i = 0; i < 3; i++)
        {
            TLInstance* instance = FEFinder<TLInstance, 4>::Find<TLSlide>(
                presentation->m_currentSlide,
                InlineHasher(nlStringLowerHash("Layer")),
                InlineHasher(nlStringLowerHash(MENU_NAMES[i])));
            TLComponentInstance* compinstance = (TLComponentInstance*)instance;

            MenuItem<TLComponentInstance>* menuItem = mMenuItems.AddItem(compinstance);

            {
                MenuCallback openFunc(DoubleHighlite::OpenItem);
                menuItem->SetCallback(ON_HIGHLIGHT, openFunc);
            }

            {
                MenuCallback closeFunc(DoubleHighlite::CloseItem);
                menuItem->SetCallback(ON_UNHIGHLIGHT, closeFunc);
            }

            if (PauseMenuCBs[i])
            {
                MenuCallback applyFunc(Bind<void>(MemFun<PauseMenuScene, void, TLComponentInstance*>(PauseMenuCBs[i]), this, placeholder0));
                menuItem->SetCallback(ON_APPLY, applyFunc);
            }

            (void)FindComponent(compinstance->GetActiveSlide(), "highlite");

            if (i == mLastSelectedIndex)
            {
                menuItem->RunCallback(ON_HIGHLIGHT);
            }
            else
            {
                menuItem->RunCallback(ON_UNHIGHLIGHT);

                TLSlide* slide = compinstance->GetActiveSlide();
                compinstance->Update(1.0f + (slide->m_start + slide->m_duration));
            }
        }

        mMenuItems.SetFlag(1);
        mMenuItems.SetActiveItemIndex(mLastSelectedIndex);
        mMenuItems.RunCallbackOnCurrent(ON_HIGHLIGHT);
        break;
    }
    case SC_DEBUG:
    {
        FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
        static char* MENU_NAMES[6]
            = { "MENU ITEM1", "MENU ITEM2", "MENU ITEM3", "MENU ITEM6", "MENU ITEM4", "MENU ITEM5" };

        for (int i = 0; i < 6; ++i)
        {
            TLInstance* instance = FEFinder<TLInstance, 4>::Find<TLSlide>(
                presentation->m_currentSlide,
                InlineHasher(nlStringLowerHash("Layer")),
                InlineHasher(nlStringLowerHash(MENU_NAMES[i])));
            TLComponentInstance* compinstance = (TLComponentInstance*)instance;
            MenuItem<TLComponentInstance>* menuItem = mMenuItems.AddItem(compinstance);

            menuItem->SetCallback(
                ON_HIGHLIGHT,
                MenuCallback(Bind<void>(MemFun<PauseMenuScene, void, TLComponentInstance*>(
                    &PauseMenuScene::OpenItem), this, placeholder0)));
            menuItem->SetCallback(
                ON_UNHIGHLIGHT,
                MenuCallback(Bind<void>(MemFun<PauseMenuScene, void, TLComponentInstance*>(
                    &PauseMenuScene::CloseItem), this, placeholder0)));
            menuItem->SetCallback(
                ON_APPLY,
                MenuCallback(Bind<void>(MemFun<PauseMenuScene, void, TLComponentInstance*>(
                    &PauseMenuScene::OnSelectDEBUG), this, placeholder0)));

            if (i == 0)
                menuItem->RunCallback(ON_HIGHLIGHT);
            else
                menuItem->RunCallback(ON_UNHIGHLIGHT);
        }

        mMenuItems.SetFlag(1);
        mMenuItems.SetActiveItemIndex(0);
        RefreshDebugMenuLabels();
        break;
    }
    default:
        break;
    }

    TLComponentInstance* buttonComponent = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));
    mButtons.mButtonInstance = buttonComponent;
    mButtons.SetState(ButtonComponent::BS_A_AND_B);

    buttonComponent = FEFinder<TLComponentInstance, 4>::Find<FEPresentation>(
        m_pFEPresentation,
        InlineHasher(nlStringLowerHash("menu in2")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));
    mButtons2.mButtonInstance = buttonComponent;
    mButtons2.SetState(ButtonComponent::BS_A_AND_B);

    EnableAutoPressed();
    FEAudio::EnableSounds(true);
}

void PauseMenuScene::RefreshDebugMenuLabels()
{
    if (mContext != SC_DEBUG)
        return;

    const int count = mMenuItems.GetNumItemsAdded();
    for (int i = 0; i < count && i < 6; ++i)
    {
        TLComponentInstance* instance = mMenuItems.GetMenuItem(i)->GetType();
        if (instance == NULL || instance->GetActiveSlide() == NULL)
            continue;

        TLTextInstance* text = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            instance->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("pauseresume")));
        if (text == NULL)
            continue;

        char label[64];
        FormatDebugLabel(mDebugPage, i, label, sizeof(label));
        ToWide(label, sDebugMenuText[i], 64);
        text->SetString(sDebugMenuText[i]);
    }
}

void PauseMenuScene::OnSelectDEBUG(TLComponentInstance* instance)
{
    (void)instance;
    const int row = mMenuItems.GetActiveItemIndex();
    if (row == 4)
    {
        mDebugPage = (mDebugPage + DebugPageCount() - 1) % DebugPageCount();
    }
    else if (row == 5)
    {
        mDebugPage = (mDebugPage + 1) % DebugPageCount();
    }
    else
    {
        ToggleDebugOption(mDebugPage, row);
    }
    RefreshDebugMenuLabels();
}

/**
 * Offset/Address/Size: 0x268 | 0x800AD760 | size: 0x5E4
 */
void PauseMenuScene::Update(float fDeltaT)
{
    if (mQuitDelay > 0.0f)
    {
        mQuitDelay = mQuitDelay - fDeltaT;
        if (!nlSingleton<OverlayManager>::Instance()->IsOnStack(OVERLAY_POPUP))
        {
            glxSwapSetBlack(true);
        }
        if (mQuitDelay <= 0.0f)
        {
            mQuitDelay = 0.0f;
            FrontEnd::ReturnToFE();
        }
        return;
    }

    if (mStartAnimAtEnd)
    {
        if (m_pFEPresentation->m_currentSlide != NULL)
        {
            m_pFEPresentation->m_fadeDuration = 999.9f;
            mStartAnimAtEnd = false;
        }
    }

    BaseSceneHandler::Update(fDeltaT);
    mButtons.CentreButtons();
    mButtons2.CentreButtons();

    if (mIsInTransition)
    {
        TLSlide* slide = m_pFEPresentation->m_currentSlide;
        f32 currentTime = slide->m_time;
        f32 endTime = slide->m_start + slide->m_duration;
        if (!(currentTime >= endTime))
            return;

        switch (mTransitionTo)
        {
        case TT_OUT:
            FrontEnd::ExitMenuState();
            break;
        default:
            break;
        }
        mIsInTransition = false;
        mTransitionTo = (TransitionType)0;
        return;
    }

    u8* connState;
    u8 goToChooseSides = 0;
    int i = 0;
    connState = &FrontEnd::m_ctrlConnectedState[0];

    for (; i < 4; i++)
    {
        bool curConnected = g_pFEInput->IsConnected((eFEINPUT_PAD)i);

        if (!g_pFEInput->IsConnected((eFEINPUT_PAD)i))
        {
            if (nlSingleton<GameInfoManager>::Instance()->GetPlayingSide((unsigned short)i) != -1)
            {
                if (!goToChooseSides)
                {
                    OverlayManager* overlayManager = nlSingleton<OverlayManager>::s_pInstance;
                    while ((overlayManager = nlSingleton<OverlayManager>::s_pInstance)->GetCurrentScene() != (BaseSceneHandler*)this)
                    {
                        overlayManager->Pop();
                        nlSingleton<FESceneManager>::Instance()->ForceImmediateStackProcessing();
                    }
                    overlayManager->Push(IGSCENE_CHOOSE_SIDES, SCREEN_FORWARD, true);
                }
                goToChooseSides = 1;
            }
        }

        *connState = curConnected;
        connState++;
    }

    if (goToChooseSides)
        return;

    mDelayBeforeUnpause = mDelayBeforeUnpause - fDeltaT;
    if (mDelayBeforeUnpause > 0.0f)
        return;

    mDelayBeforeUnpause = 0.0f;

    if (m_pFEPresentation->m_currentSlide == NULL)
        return;

    if (g_pFEInput->IsAutoPressed(mControllingInput, 0xd, true, NULL))
    {
        mMenuItems.PreviousItem();
        return;
    }

    if (g_pFEInput->IsAutoPressed(mControllingInput, 0xe, true, NULL))
    {
        mMenuItems.NextItem();
        return;
    }

    if (g_pFEInput->JustPressed(mControllingInput, 0x100, false, &mQuittingController))
    {
        switch (mMenuItems.RunCallbackOnCurrent(ON_APPLY))
        {
        case RES_OK:
            mLastSelectedIndex = mMenuItems.GetActiveItemIndex();
            FEAudio::PlayAnimAudioEvent("sfx_accept", false);
            break;
        case RES_ITEM_DISABLED:
            FEAudio::PlayAnimAudioEvent("sfx_deny", false);
            break;
        default:
            break;
        }
        return;
    }

    if (!g_pFEInput->JustPressed(mControllingInput, 0x200, false, NULL))
    {
        if (!g_pFEInput->JustPressed(mControllingInput, 0x1000, false, NULL))
            return;
        if (FrontEnd::m_bGameOver)
            return;
    }

    if (!FrontEnd::m_bGameOver)
    {
        OnSelectRESUME(NULL);
    }
    else
    {
        FrontEnd::ExitMenuState();
        FEAudio::PlayAnimAudioEvent("sfx_back", false);
    }
}

void PauseMenuScene::SetupForNewGame()
{
    nlTaskManager* taskManager = nlTaskManager::m_pInstance;
    mLastTaskManagerState = taskManager->m_CurrState;
    taskManager->m_Locked = false;
    nlTaskManager::SetNextState(1);
}

/**
 * Offset/Address/Size: 0xD0 | 0x800AD5C8 | size: 0x198
 */
void PauseMenuScene::TransitionOut(PauseMenuScene::TransitionType newtype)
{
    mIsInTransition = true;
    mTransitionTo = newtype;

    if (mTransitionTo == TT_OUT)
    {
        FEPresentation* presentation = m_pFEScene->m_pFEPackage->GetPresentation();
        presentation->SetActiveSlide("menu in2");
        presentation->Update(0.0f);

        int i;
        for (i = 0; i < mMenuItems.GetNumItemsAdded(); i++)
        {

            char menuname[64];
            nlSNPrintf(menuname, sizeof(menuname), "MENU ITEM%d", i + 1);

            TLInstance* instance = FEFinder<TLInstance, 4>::Find<TLSlide>(
                presentation->m_currentSlide,
                InlineHasher(nlStringLowerHash("Layer")),
                InlineHasher(nlStringLowerHash(menuname)));

            TLComponentInstance* compinstance = (TLComponentInstance*)instance;

            if (i == mMenuItems.GetActiveItemIndex())
            {
                compinstance->SetActiveSlide(sPauseOutSlide);
                compinstance->Update(0.0f);

                TLComponentInstance* highlite = (TLComponentInstance*)FindComponent(compinstance->GetActiveSlide(), "highlite");
                highlite->SetActiveSlide(sPauseOutSlide);
                highlite->Update(0.0f);
                highlite->SetAssetColour(MenuHighliteColour);
            }
            else
            {
                compinstance->SetActiveSlide(sPauseOutSlide);
                compinstance->Update(0.0f);

                TLComponentInstance* highlite = (TLComponentInstance*)FindComponent(compinstance->GetActiveSlide(), "highlite");
                highlite->m_bVisible = false;
            }

            if (i + 1 == 5)
            {
                SetQuitTextForJapanese(compinstance);
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x800AD4F8 | size: 0xD0
 */
void PauseMenuScene::OpenItem(TLComponentInstance* instance)
{
    DoubleHighlite::OpenItem(instance);

    if (mContext == SC_DEBUG)
    {
        RefreshDebugMenuLabels();
        return;
    }

    if (mMenuItems.GetMenuItem()->IsDisabled())
    {
        TLTextInstance* text = FEFinder<TLTextInstance, 3>::Find(
            instance->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("pauseresume")),
            InlineHasher(0),
            InlineHasher(0),
            InlineHasher(0),
            InlineHasher(0),
            InlineHasher(0));

        text->m_LocStrId = 0x38202C30;
        text->m_OverloadFlags |= 0x8;
    }

    if (mMenuItems.GetActiveItemIndex() == 5)
    {
        SetQuitTextForJapanese(instance);
    }
}

void PauseMenuScene::CloseItem(TLComponentInstance* instance)
{
    DoubleHighlite::CloseItem(instance);

    if (mContext == SC_DEBUG)
    {
        RefreshDebugMenuLabels();
        return;
    }

    if (mMenuItems.GetActiveItemIndex() == 5)
    {
        SetQuitTextForJapanese(instance);
    }
}

void PauseMenuScene::SetQuitTextForJapanese(TLComponentInstance* instance)
{
    if (g_pLocalization->m_CurrentLanguage == nlLocalization::LangJapanese
        && nlSingleton<GameInfoManager>::Instance()->IsInCupOrTournamentMode())
    {
        TLTextInstance* text = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
            instance->GetActiveSlide(),
            InlineHasher(nlStringLowerHash("pauseresume")));

        if (text != NULL)
        {
            text->m_LocStrId = 0x2718546B;
            text->m_OverloadFlags |= 0x8;
        }
    }
}
