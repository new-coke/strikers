#include "Game/TransitionTask.h"
#include "Game/Font/fontmanager.h"
#include "Game/BaseGameSceneManager.h"
#include "Game/OverlayManager.h"
#include "Game/FE/feSceneManager.h"
#include "Game/FE/feResourceManager.h"
#include "Game/FE/feManager.h"
#include "Game/GameInfo.h"
#include "Game/DB/StatsTracker.h"
#include "Game/Transitions/ScreenTransitionManager.h"
#include "Game/Sys/eventman.h"
#include "Game/Ball.h"
#include "Game/Physics/Physics.h"
#include "Game/Physics/PhysicsFakeBall.h"
#include "Game/Render/FlareHandler.h"
#include "Game/Render/GraphicsLoader.h"
#include "Game/NisPlayer.h"
#include "Game/ReplayChoreo.h"
#include "Game/ReplayManager.h"
#include "Game/Render/Jumbotron.h"
#include "Game/Render/CrowdManager.h"
#include "Game/Render/NPCLoader.h"
#include "Game/SAnim/pnSAnimController.h"
#include "Game/SAnim/pnBlender.h"
#include "Game/SAnim/pnSingleAxisBlender.h"
#include "Game/SAnim/pnFeather.h"
#include "Game/Render/SidelineExplodable.h"
#include "Game/BeginFrameTask.h"
#include "Game/WorldManager.h"
#include "Game/ObjectBlur.h"
#include "Game/Camera/CameraMan.h"
#include "Game/CameraLoader.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/AI/AILoader.h"
#include "Game/Loader/LoadingManager.h"
#include "Game/World/WorldLoader.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/Audio/AudioScriptEventMgr.h"
#include "Game/Audio/AudioStream.h"
#include "Game/Audio/CrowdMood.h"
#include "Game/Audio/StreamTrack.h"
#include "Game/CharacterTemplate.h"
#include "Game/CharacterTriggers.h"
#include "Game/Sys/PlatStream.h"
#include "Game/Debug/TimeRegions.h"
#include "Game/Render/ElectricFence.h"
#include "Game/Render/Presentation.h"
#include "Game/Game.h"
#include "Game/Goalie.h"
#include "Game/Drawable/DrawableModel.h"
#include "Game/Drawable/DrawableCharacter.h"
#include "Game/AI/Fielder.h"
#include "Game/Render/Wiper.h"
#include "Game/FE/LidOpenMessage.h"
#include "Game/PadActions.h"
#include "NL/gl/glPlat.h"
#include "dolphin/vi/vifuncs.h"
#include "dolphin/os/OSRtc.h"
#include "Game/SH/SHLoading.h"
#include "Game/SH/SHPause.h"
#include "Game/FE/feScene.h"
#include "Game/DB/UserOptions.h"
#include "NL/nlMemory.h"
#include "NL/MemAlloc.h"
#include "NL/nlConfig.h"
#include "NL/platpad.h"
#include "NL/gl/gl.h"
#include "NL/gl/glView.h"
#include "NL/glx/glxSwap.h"
#include "NL/plat/plataudio.h"
#include "NL/gl/glMemory.h"
#include "NL/nlLocalization.h"
#include "Game/GameSceneManager.h"
#include "Game/FE/feMusic.h"
#include "Game/FE/feAnimModelManager.h"
#include "Game/FE/FELoader.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/SH/SHSaveLoad.h"
#include "Game/SH/SHMainMenu.h"
#include "Game/SH/SHCupHub.h"
#include "Game/Sys/debug.h"
#include "dolphin/os.h"
#include "Game/main.h"

int nlSNPrintf(char*, unsigned long, const char*, ...);

TransitionTask* TransitionTask::sm_pGlobalTask;
bool g_bFrameStatsOnDisk;
static unsigned long long s_FontResourceMark = -1;
extern bool gSebringLoadPackageToVirtualMemory;
extern PhysicsLoader ThePhysicsLoader;

void glx_SetFog(int);
bool fxParticleShutdown();
bool fxUnloadGroups();
bool fxUnloadTemplates();
void glResourceRelease(unsigned long long);

extern unsigned char g_JaapAndJacksNastyHackBecauseWeDoNotKnowDifferenceBetweenPausePauseAndPostGamePause;

void PrintAvailableARAMMemory();
void InitializeGameObjectLighting();
void AIEventHandler(Event*, void*);
void InitializeElectricFence();
void InitializeTimeRegions();

extern GraphicsLoader TheGraphicsLoader;
extern FELoader TheFELoader;
extern AudioLoader TheAudioLoader;
extern NPCLoader TheNPCLoader;
extern AILoader TheAILoader;
extern CameraLoader TheCameraLoader;

namespace Detail
{
class SwitchToStartScreenLoader : public Loader
{
public:
    /**
     * Offset/Address/Size: 0x1BF4 | 0x801731C4 | size: 0x28
     */
    virtual bool StartLoad(LoadingManager*)
    {
        FrontEnd::EnterStartScreen(false);
        return true;
    }
};

static SwitchToStartScreenLoader switchToStartScreen;
} // namespace Detail

static inline void MarkFontAsJapanese(const char* fontName)
{
    FontManager* fontManager = nlSingleton<FontManager>::s_pInstance;
    nlFont* font = fontManager->GetFontByHashID(nlStringLowerHash(fontName));
    font->SetIsJapanese(true);
}

/**
 * Offset/Address/Size: 0x1A10 | 0x80172FE0 | size: 0x1E4
 */
static void LoadFonts()
{
    const char* TEXT_FONT_NAME = "fot-rodinprob18";
    const char* HEADING_FONT_NAME = "cepoitalic24";
    char langprefix[4] = "eng";

    switch (g_pLocalization->m_CurrentLanguage)
    {
    case 1:
        langprefix[0] = 'f';
        langprefix[1] = 'r';
        langprefix[2] = 'e';
        break;
    case 2:
        langprefix[0] = 'd';
        langprefix[1] = 'e';
        langprefix[2] = 'u';
        break;
    case 4:
        langprefix[0] = 'i';
        langprefix[1] = 't';
        langprefix[2] = 'a';
        break;
    case 5:
        langprefix[0] = 'j';
        langprefix[1] = 'p';
        langprefix[2] = 'n';
        break;
    case 3:
        langprefix[0] = 's';
        langprefix[1] = 'p';
        langprefix[2] = 'a';
        break;
    case 6:
        langprefix[0] = 'u';
        langprefix[1] = 'k';
        langprefix[2] = 'e';
        break;
    case 8:
        langprefix[0] = 'b';
        langprefix[1] = 'o';
        langprefix[2] = 'b';
        break;
    case 7:
        langprefix[0] = 'l';
        langprefix[1] = 'n';
        langprefix[2] = 'g';
        break;
    case 0:
        break;
    }

    char textfontbundlename[64];
    char textfontfilename[64];
    char headingfontbundlename[64];
    char headingfontfilename[64];

    nlSNPrintf(textfontbundlename, 64, "art/fe/fonts/%sfonttext18.res", langprefix);
    nlSNPrintf(textfontfilename, 64, "fe/fonts/%sfonttext18", langprefix);
    nlSNPrintf(headingfontbundlename, 64, "art/fe/fonts/%sfontheading24.res", langprefix);
    nlSNPrintf(headingfontfilename, 64, "fe/fonts/%sfontheading24", langprefix);

    nlSingleton<FontManager>::Instance()->LoadFont(textfontbundlename, textfontfilename, TEXT_FONT_NAME);
    if (g_pLocalization->m_CurrentLanguage == nlLocalization::LangJapanese)
    {
        MarkFontAsJapanese(TEXT_FONT_NAME);
    }
    nlSingleton<FontManager>::Instance()->LoadFont(headingfontbundlename, headingfontfilename, HEADING_FONT_NAME);
    if (g_pLocalization->m_CurrentLanguage == nlLocalization::LangJapanese)
    {
        MarkFontAsJapanese(HEADING_FONT_NAME);
    }
}

static void LoadFontsJapaneseInGame()
{
    const char* TEXT_FONT_NAME = "fot-rodinprob18";
    const char* HEADING_FONT_NAME = "cepoitalic24";
    char textfontbundlename[64];
    char textfontfilename[64];
    char headingfontbundlename[64];
    char headingfontfilename[64];
    char langprefix[4] = "jpn";

    nlSNPrintf(textfontbundlename, 64, "art/fe/fonts/%sfonttextingame18.res", langprefix);
    nlSNPrintf(textfontfilename, 64, "fe/fonts/%sfonttextingame18", langprefix);
    nlSNPrintf(headingfontbundlename, 64, "art/fe/fonts/%sfontheadingingame24.res", langprefix);
    nlSNPrintf(headingfontfilename, 64, "fe/fonts/%sfontheadingingame24", langprefix);
    nlSingleton<FontManager>::Instance()->LoadFont(textfontbundlename, textfontfilename, TEXT_FONT_NAME);
    MarkFontAsJapanese(TEXT_FONT_NAME);
    nlSingleton<FontManager>::Instance()->LoadFont(headingfontbundlename, headingfontfilename, HEADING_FONT_NAME);
    MarkFontAsJapanese(HEADING_FONT_NAME);
}

static void LoadFontsJapanese101()
{
    const char* TEXT_FONT_NAME = "fot-rodinprob18";
    const char* HEADING_FONT_NAME = "cepoitalic24";
    char textfontbundlename[64];
    char textfontfilename[64];
    char headingfontbundlename[64];
    char headingfontfilename[64];
    char langprefix[4] = "jpn";

    nlSNPrintf(textfontbundlename, 64, "art/fe/fonts/%sfonttext10118.res", langprefix);
    nlSNPrintf(textfontfilename, 64, "fe/fonts/%sfonttext10118", langprefix);
    nlSNPrintf(headingfontbundlename, 64, "art/fe/fonts/%sfontheading10124.res", langprefix);
    nlSNPrintf(headingfontfilename, 64, "fe/fonts/%sfontheading10124", langprefix);
    nlSingleton<FontManager>::Instance()->LoadFont(textfontbundlename, textfontfilename, TEXT_FONT_NAME);
    MarkFontAsJapanese(TEXT_FONT_NAME);
    nlSingleton<FontManager>::Instance()->LoadFont(headingfontbundlename, headingfontfilename, HEADING_FONT_NAME);
    MarkFontAsJapanese(HEADING_FONT_NAME);
}

/**
 * Offset/Address/Size: 0x19E0 | 0x80172FB0 | size: 0x30
 */
TransitionTask::TransitionTask()
{
    m_pAIHandler = nullptr;
    m_pGoalieHandler = nullptr;
    m_pLoadingManager = nullptr;
    m_TransitionState = eTS_Unknown;
}

/**
 * Offset/Address/Size: 0x19D8 | 0x80172FA8 | size: 0x8
 */
void TransitionTask::Initialize(LoadingManager& loadingManager)
{
    m_pLoadingManager = &loadingManager;
}

static inline void ClearCharacterEffectsAndResetPowerups()
{
    int i;
    for (i = 0; i < 10; i++)
    {
        cCharacter* pChar = g_pCharacters[i];
        if (pChar != NULL)
        {
            if (pChar->m_eClassType == FIELDER)
            {
                if (((cFielder*)pChar)->IsFrozen())
                {
                    EmitUnFreeze((cPlayer*)pChar);
                }
                ((cFielder*)pChar)->ClearTimers();
                ((cPlayer*)pChar)->ClearPowerupAnimState(false);
            }
            g_pCharacters[i]->ResetEffects();
        }
    }

    g_pGame->ResetPowerups(false);
}

static void EnablePersistentEffects(bool bEnable)
{
    efList* controllers;
    EmissionController* p;

    controllers = EmissionManager::GetContainer();
    if (controllers != NULL)
    {
        p = (EmissionController*)controllers->m_headNode;
        while (p != NULL)
        {
            if (p->m_uUserData == 0xDEADBEEF)
            {
                p->m_bDisabled = !bEnable;
                if (bEnable)
                {
                    SidelineExplodableManager::AssociateEffectWithNearbyFloatingCamera(p);
                }
                else
                {
                    SidelineExplodableManager::UnAssociateEffectWithNearbyFloatingCamera(p);
                }
            }
            p = (EmissionController*)p->m_nextNode;
        }
    }
}

static void WaitForAllScenesValid()
{
    while (!nlSingleton<FESceneManager>::Instance()->AreAllScenesValid())
    {
        nlServiceFileSystem();
        nlSingleton<FESceneManager>::Instance()->Update(0.0f);
        nlSingleton<FEResourceManager>::Instance()->Run(0.0f);
    }
}

static void WaitForFELoadsToFinish(BaseSceneHandler* pFinalScene)
{
    do
    {
        nlServiceFileSystem();
        nlSingleton<FEResourceManager>::Instance()->Run(0.0f);
        nlSingleton<FESceneManager>::Instance()->Update(0.0f);
    } while (!pFinalScene->m_pFEScene->m_bValid);
}

static void ClearCharacterEffectsTexturing()
{
    int i;
    cFielder* pFielder;

    for (i = 0; i < 10; i++)
    {
        if (g_pCharacters[i] != NULL)
        {
            pFielder = (cFielder*)g_pCharacters[i];
            if (pFielder->m_eClassType == FIELDER)
            {
                if (pFielder->IsFrozen())
                {
                    EmitUnFreeze(pFielder);
                }
                pFielder->ClearTimers();
                pFielder->ClearPowerupAnimState(false);
            }
            g_pCharacters[i]->ResetEffects();
        }
    }
}

/**
 * Offset/Address/Size: 0x10B8 | 0x80172688 | size: 0x920
 */
void TransitionTask::StateTransition(unsigned int from, unsigned int to)
{
    bool bLoadingIndicator = false;
    int i;

    if (to == 4 || to == 2)
    {
        glxSwapLoading(true, false);
        bLoadingIndicator = true;
    }

    nlTaskManager::m_pInstance->m_Locked = true;

    bool bNISLighting;
    if (to & 0x100)
    {
        bNISLighting = true;
    }
    else if (to == 1 && (from & 0x100))
    {
        bNISLighting = true;
    }
    else
    {
        bNISLighting = false;
    }
    DrawableCharacter::sCameraRelativeLighting = bNISLighting;

    if (to == 0x80000)
    {
        glxSwapSetBlack(true);
        DisplayLoadingMessageFast();
        glxSwapSetBlack(false);
        glxSwapLoading(true, true);

        DestroyFEFast();
        InitializeFEState();
        glxSwapLoading(false, false);
        nlTaskManager::SetNextState(4);
    }

    if (to == 4)
    {
        UpdateMonkeyState(1);

        if (g_pGame != NULL)
        {
            DestroyGameState();
        }

        if (from == 0x10000)
        {
            AudioLoader::LoadFE(false);
            InitializeFEFast();
        }
        else
        {
            InitializeFEState();
        }
    }

    if (to == 2)
    {
        UpdateMonkeyState(0);

        if (from == 4)
        {
            DestroyFEState();
        }

        if (g_pGame == NULL)
        {
            InitializeGameState();
            AudioLoader::InitCrowdFromStateTransition();
            ReplayManager::Instance()->ResetSnapshots();
        }

        if (from != 1)
        {
            ReplayManager::Instance()->PrepareForRecording();
        }

        if (!g_JaapAndJacksNastyHackBecauseWeDoNotKnowDifferenceBetweenPausePauseAndPostGamePause)
        {
            if ((from & 0x110) || (from == 1 && (nlTaskManager::m_pInstance->m_PrevState & 0x110)))
            {
                NisPlayer::Instance()->Reset();
                Wiper::Instance().Reset();
            }
        }
        else
        {
            g_JaapAndJacksNastyHackBecauseWeDoNotKnowDifferenceBetweenPausePauseAndPostGamePause = false;
        }
    }

    if ((from == 2 && to != 1) || (from == 1 && to != 2))
    {
        if (g_pBall != NULL)
        {
            g_pBall->KillBlurHandler();
        }
    }

    if (nlSingleton<OverlayManager>::s_pInstance != NULL)
    {
        nlSingleton<OverlayManager>::Instance()->HandleStateTransition(from, to);
    }

    if ((to & 0x110) || to == 0x20000)
    {
        EnablePersistentEffects(true);

        if (from != 1 && to != 0x20000)
        {
            {
                Presentation& presentation = Presentation::Instance();
                presentation.mLetterBoxEnabled = true;
            }

            for (i = 0; i < 2; i++)
            {
                g_pTeams[i]->StopGameplayEffectsAndSounds();
            }

            ClearCharacterEffectsTexturing();

            g_pGame->ResetPowerups(false);
        }
    }
    else
    {
        EnablePersistentEffects(false);

        if ((from & 0x110) || (from == 1 && (nlTaskManager::m_pInstance->m_PrevState & 0x110)))
        {
            if (to != 1 && to != 4)
            {
                Presentation& presentation = Presentation::Instance();
                presentation.mLetterBoxEnabled = false;
                presentation.mLetterBoxDuration = 0.0f;

                ClearCharacterEffectsTexturing();

                g_pGame->ResetPowerups(false);
            }
        }
    }

    if (to == 0x10)
    {
        cCameraManager::PushWorldUpVector();
    }
    if (from == 0x10)
    {
        cCameraManager::PopWorldUpVector();
    }

    nlTaskManager::m_pInstance->m_Locked = false;

    if (bLoadingIndicator)
    {
        glxSwapLoading(false, false);
    }
}

/**
 * Offset/Address/Size: 0x888 | 0x80171E58 | size: 0x830
 */
void TransitionTask::InitializeGameState()
{
    m_TransitionState = eTS_Initializing;

    tDebugPrintManager::Print(DC_MEMORY, "-- Memory upon Entering InitializeGameState \n");
    tDebugPrintManager::Print(DC_MEMORY, "Free Memory: %u\n", StandardAllocator.TotalFreeMemory());
    tDebugPrintManager::Print(DC_MEMORY, "Largest Free Block: %u\n", StandardAllocator.LargestFreeBlock());

    if (AudioLoader::IsInited())
    {
        PrintAvailableARAMMemory();
    }

    tDebugPrintManager::Print(DC_MEMORY, "-----------------------------------------\n\n");

    gSebringLoadPackageToVirtualMemory = true;

    if (nlSingleton<GameInfoManager>::Instance()->IsInCupMode())
    {
        nlSingleton<StatsTracker>::Instance()->SimulateRemainingGames();
    }

    nlSingleton<GameInfoManager>::Instance()->OnPreGameState();
    ReplayManager::Instance()->Initialize();
    m_GameResourceMark = glResourceMark();
    InitializeGameObjectLighting();

    m_pAIHandler = g_pEventManager->AddEventHandler(AIEventHandler, NULL, 1);
    m_pGoalieHandler = g_pEventManager->AddEventHandler(Goalie::EventHandler, NULL, 1);

    nlSingleton<StatsTracker>::Instance()->SetBasicGameInfoPointer(
        nlSingleton<GameInfoManager>::Instance()->mGameInfo[nlSingleton<GameInfoManager>::Instance()->mCurrentMode],
        true);
    nlSingleton<StatsTracker>::Instance()->CreateEventHandler();

    Jumbotron::instance.Initialize();
    CrowdManager::instance.Initialize();
    CreateGame();
    TerrainInitialize();

    m_pLoadingManager->QueueLoader(&TheGraphicsLoader);
    m_pLoadingManager->QueueLoader(&TheFELoader);
    TheWorldLoader.mTEMP_FOR_FE = false;
    m_pLoadingManager->QueueLoader(&TheWorldLoader);
    m_pLoadingManager->QueueLoader(&TheAudioLoader);

    AudioLoader::LoadInGame();

    if (!AudioLoader::gbDisableCrowd)
    {
        CrowdMood::Init();
    }

    AudioScriptEventMgr::Init();

    m_pLoadingManager->QueueLoader(&ThePhysicsLoader);
    m_pLoadingManager->QueueLoader(&TheNPCLoader);
    m_pLoadingManager->QueueLoader(&TheAILoader);
    m_pLoadingManager->QueueLoader(&TheCameraLoader);

    AudioLoader::SetupPostPhysicsCameraLoad();
    m_pLoadingManager->QueueLoader(&Detail::switchToStartScreen);

    ReplayManager::Instance()->InitializeSnapshots();

    s_FontResourceMark = glResourceMark();
    g_pLocalization->Load(g_Language, false);

    if (nlSingleton<FontManager>::s_pInstance == NULL)
    {
        nlSingleton<FontManager>::s_pInstance = new (nlMalloc(sizeof(FontManager), 8, false)) FontManager();
    }

    if (g_pLocalization->m_CurrentLanguage == nlLocalization::LangJapanese)
    {
        if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
        {
            LoadFontsJapanese101();
        }
        else
        {
            LoadFontsJapaneseInGame();
        }
    }
    else
    {
        LoadFonts();
    }

    if (nlSingleton<FEResourceManager>::s_pInstance == NULL)
    {
        nlSingleton<FEResourceManager>::s_pInstance = new (nlMalloc(sizeof(FEResourceManager), 8, false)) FEResourceManager();
    }

    nlSingleton<FEResourceManager>::Instance()->Initialize();
    nlSingleton<FEResourceManager>::Instance()->LoadPermanentResourceBundle("art/fe/InGameUI.Res");
    nlSingleton<FEResourceManager>::Instance()->OpenOnDemandResourceBundle("art/fe/InGameUI.Dmn");

    if (nlSingleton<FESceneManager>::s_pInstance == NULL)
    {
        nlSingleton<FESceneManager>::s_pInstance = new (nlMalloc(sizeof(FESceneManager), 8, false)) FESceneManager();
    }

    nlSingleton<FESceneManager>::Instance()->m_uDefaultRenderView = 31;

    if (nlSingleton<OverlayManager>::s_pInstance == NULL)
    {
        nlSingleton<OverlayManager>::s_pInstance = new (nlMalloc(sizeof(OverlayManager), 8, false)) OverlayManager();
    }

    nlSingleton<OverlayManager>::Instance()->Push(OVERLAY_TEXT, (ScreenMovement)0, false);
    BaseSceneHandler* newscene
        = nlSingleton<OverlayManager>::Instance()->Push(OVERLAY_HUD, (ScreenMovement)0, false);
    newscene->SetVisible(false);

    if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
    {
        nlSingleton<OverlayManager>::Instance()->Push(OVERLAY_LESSON_TICKER, (ScreenMovement)0, false);
        Presentation& pres = Presentation::Instance();
        pres.mLetterBoxEnabled = false;
        pres.mLetterBoxDuration = 0.0f;
    }

    BaseSceneHandler* goalOverlay
        = nlSingleton<OverlayManager>::Instance()->Push(OVERLAY_GOAL, (ScreenMovement)0, false);
    goalOverlay->SetVisible(false);

    GameInfoManager::eGameModes mode = nlSingleton<GameInfoManager>::Instance()->mCurrentMode;
    if (!(mode >= GameInfoManager::GM_MUSHROOM_CUP && mode <= GameInfoManager::GM_TOURNAMENT))
    {
        if (mode == GameInfoManager::GM_DEMO)
        {
            nlSingleton<OverlayManager>::Instance()->Push(OVERLAY_DEMO, (ScreenMovement)0, false);
        }
    }

    nlSingleton<OverlayManager>::Instance()->Push((SceneList)0x4D, (ScreenMovement)0, false);

    WaitForAllScenesValid();

    SuperLoadingScene* loadingscene = (SuperLoadingScene*)nlSingleton<OverlayManager>::Instance()->Push(SCENE_SUPER_LOADING, (ScreenMovement)0, false);
    loadingscene->mType = SuperLoadingScene::TT_OUT;
    nlSingleton<FESceneManager>::Instance()->Update(0.0f);

    do
    {
        nlServiceFileSystem();
        nlSingleton<FEResourceManager>::Instance()->Run(0.0f);
    } while (!loadingscene->m_pFEScene->m_bValid);

    PauseMenuScene::mLastSelectedIndex = 0;

    cPlatPad::m_bDisableRumble = !nlSingleton<GameInfoManager>::Instance()->GetGameplayOptions().RumbleEnabled
                              || GetConfigBool(Config::Global(), "no_pad_rumble", false);

    AudioLoader::StopStreaming();

    if (AudioLoader::IsInited())
    {
        PlatAudio::ConfigureStreamBuffers(7);
        // PORT: null when audio is disabled, nothing to command.
        if (g_pTrackManager != NULL) g_pTrackManager->DestroyAllTracks();
        // PORT: null when audio is disabled, nothing to command.
        if (g_pTrackManager != NULL) g_pTrackManager->CreateTrack("Announcer", Audio::MasterVolume::VG_Voice);
        // PORT: null when audio is disabled, nothing to command.
        if (g_pTrackManager != NULL) g_pTrackManager->CreateTrack("Music", Audio::MasterVolume::VG_Music);
        Audio::CreatePriorityStreams();
    }

    InitializeElectricFence();
    BeginFrameTask::s_FramerateLocked = false;

    OSReport("-- Memory upon Exiting InitializeGameState\n");
    OSReport("Free Memory: %u\n", StandardAllocator.TotalFreeMemory());
    OSReport("Largest Free Block: %u\n", StandardAllocator.LargestFreeBlock());
    OSReport("Largest Virtual Free Block: %u\n", nlVirtualLargestBlock());
    OSReport("-----------------------------------------\n\n");

    TakeGameMemSnapshot::ResetTimers();
    InitializeTimeRegions();

    m_TransitionState = eTS_InState;
}

/**
 * Offset/Address/Size: 0x4BC | 0x80171A8C | size: 0x3CC
 */
void TransitionTask::DestroyGameState()
{
    m_TransitionState = eTS_Destroying;

    if (g_bFrameStatsOnDisk)
    {
        WriteFrameRateStatsToFile();
    }

    DestroyTimeRegions();

    WaitForAllScenesValid();

    nlSingleton<OverlayManager>::Instance()->PopEntireStack();
    nlSingleton<FESceneManager>::Instance()->ForceImmediateStackProcessing();

    glxSwapLoading(false, false);

    nlSingleton<OverlayManager>::Instance()->Push((SceneList)0x4E, (ScreenMovement)0, false);

    nlSingleton<FESceneManager>::Instance()->ForceImmediateStackProcessing();

    WaitForAllScenesValid();

    for (int i = 0; i < 2; i++)
    {
        glBeginFrame();
        SetupMatrices();
        nlSingleton<FEResourceManager>::Instance()->Run(1.0f);
        nlSingleton<FESceneManager>::Instance()->Update(1.0f);
        nlSingleton<FESceneManager>::Instance()->RenderActiveScenes();
        glFinish();
        glEndFrame();
        glSendFrame();
    }

    glxSwapLoading(true, false);

    PlatAudio::StopAllStreams();
    AudioScriptEventMgr::Purge();
    FlareHandler::instance.Cleanup();
    NisPlayer::Instance()->Reset();
    ReplayChoreo::Instance().Reset();
    ReplayManager::Instance()->Uninitialize();

    glx_SetFog(-1);

    WaitForAllScenesValid();

    if (nlSingleton<GameInfoManager>::Instance()->mCurrentMode != 0)
    {
        nlSingleton<OverlayManager>::Instance()->DestroyMessengerManager();
    }

    nlSingleton<OverlayManager>::Instance()->PopEntireStack();

    if (nlSingleton<OverlayManager>::s_pInstance != NULL)
    {
        delete nlSingleton<OverlayManager>::s_pInstance;
        nlSingleton<OverlayManager>::s_pInstance = NULL;
    }

    if (nlSingleton<FESceneManager>::s_pInstance != NULL)
    {
        delete nlSingleton<FESceneManager>::s_pInstance;
        nlSingleton<FESceneManager>::s_pInstance = NULL;
    }

    nlSingleton<FEResourceManager>::Instance()->UnloadPermanentResourceBundle();

    if (nlSingleton<FEResourceManager>::s_pInstance != NULL)
    {
        delete nlSingleton<FEResourceManager>::s_pInstance;
        nlSingleton<FEResourceManager>::s_pInstance = NULL;
    }

    if (nlSingleton<FontManager>::s_pInstance != NULL)
    {
        delete nlSingleton<FontManager>::s_pInstance;
        nlSingleton<FontManager>::s_pInstance = NULL;
    }

    nlFree(g_pLocalization->m_pFile);
    glResourceRelease(s_FontResourceMark);

    g_pEventManager->RemoveEventHandler(m_pAIHandler);
    g_pEventManager->RemoveEventHandler(m_pGoalieHandler);
    m_pAIHandler = NULL;
    m_pGoalieHandler = NULL;

    AudioLoader::UnloadInGame();
    BeginFrameTask::s_FramerateLocked = 0;

    DestroyPowerups();
    DestroyCharacters();

    delete g_pBall;
    g_pBall = NULL;

    FakeBallWorld::Destroy();
    cCameraManager::Shutdown();
    EmissionManager::Shutdown();
    fxParticleShutdown();
    fxUnloadGroups();
    fxUnloadTemplates();

    SidelineExplodableManager::DestroyAllActiveFragments(true);
    WorldManager::DestroyWorld();

    SlotPoolBase::BaseFreeBlocks(&SFXPlaySet::m_TrackedSFXSlotPool, sizeof(SFXPlaySet));
    SidelineExplodableManager::CleanUp();
    ThePhysicsLoader.DestroyPhysics();
    FrontEnd::Destroy();

    Jumbotron::instance.Uninitialize();
    CrowdManager::instance.Uninitialize();

    FreeElectricFence();
    DestroyGame();
    BlurManager::Shutdown();
    CleanBoundingBoxCache();

    nlSingleton<StatsTracker>::Instance()->DestroyEventHandler();
    glResourceRelease(m_GameResourceMark);

    nlSingleton<ScreenTransitionManager>::Instance()->CancelAllTransitions();

    gSebringLoadPackageToVirtualMemory = 0;

    CompactSlotPools();

    m_TransitionState = eTS_Unknown;
}

/**
 * Offset/Address/Size: 0x0 | 0x801715D0 | size: 0x4BC
 */
void TransitionTask::InitializeFEState()
{
    m_TransitionState = eTS_Initializing;

    tDebugPrintManager::Print(DC_MEMORY, "-- Memory upon Entering InitializeFEState ----------\n");
    tDebugPrintManager::Print(DC_MEMORY, "Free Memory: %u\n", StandardAllocator.TotalFreeMemory());
    tDebugPrintManager::Print(DC_MEMORY, "Largest Free Block: %u\n", StandardAllocator.LargestFreeBlock());
    tDebugPrintManager::Print(DC_MEMORY, "-----------------------------------------\n\n");

    s_FontResourceMark = glResourceMark();
    g_pLocalization->Load(g_Language, false);
    EnableAutoPressed();

    if (nlSingleton<FontManager>::s_pInstance == NULL)
    {
        nlSingleton<FontManager>::s_pInstance = new (nlMalloc(sizeof(FontManager), 8, false)) FontManager();
    }

    LoadFonts();

    if (nlSingleton<FEResourceManager>::s_pInstance == NULL)
    {
        nlSingleton<FEResourceManager>::s_pInstance = new (nlMalloc(sizeof(FEResourceManager), 8, false)) FEResourceManager();
    }

    nlSingleton<FEResourceManager>::Instance()->Initialize();
    nlSingleton<FEResourceManager>::Instance()->LoadPermanentResourceBundle("art/fe/MainUI.Dmn");

    if (nlSingleton<FESceneManager>::s_pInstance == NULL)
    {
        nlSingleton<FESceneManager>::s_pInstance = new (nlMalloc(sizeof(FESceneManager), 8, false)) FESceneManager();
    }

    nlSingleton<FESceneManager>::Instance()->m_uDefaultRenderView = 31;

    nlSingleton<GameInfoManager>::Instance()->OnPostGameState();

    if (nlSingleton<GameSceneManager>::s_pInstance == NULL)
    {
        nlSingleton<GameSceneManager>::s_pInstance = new (nlMalloc(sizeof(GameSceneManager), 8, false)) GameSceneManager();
    }

    if (nlSingleton<FEAnimModelManager>::s_pInstance == NULL)
    {
        nlSingleton<FEAnimModelManager>::s_pInstance = new (nlMalloc(sizeof(FEAnimModelManager), 8, false)) FEAnimModelManager();
    }

    nlSingleton<FEAnimModelManager>::Instance()->Initialize();

    FEMusic::ResetCurrentFEStreamHash();

    static bool gAlreadyBooted = false;

    if (!gAlreadyBooted)
    {
        gAlreadyBooted = true;
        if (SaveLoadScene::IsIOEnabled())
        {
            SaveLoadScene* scene = (SaveLoadScene*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_SHOULD_LOAD_OR_SAVE, (ScreenMovement)0, false);
            scene->mNextScene = SCENE_LEGAL;
        }
        else
        {
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_LEGAL, (ScreenMovement)0, false);
        }
    }
    else
    {
        AudioLoader::LoadFE(true);

        if (g_e3_Build)
        {
            for (int i = 0; i < 4; i++)
            {
                nlSingleton<GameInfoManager>::Instance()->SetPlayingSide(i, -1);
            }
        }

        if (nlSingleton<GameInfoManager>::Instance()->IsInDemoMode())
        {
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_TITLE, (ScreenMovement)0, false);
            nlSingleton<GameInfoManager>::Instance()->SetMode(GameInfoManager::GM_DEMO);
        }
        else if (nlSingleton<GameInfoManager>::Instance()->IsInCupOrTournamentMode())
        {
            GameInfoManager* pGameInfo = nlSingleton<GameInfoManager>::Instance();
            if (pGameInfo->IsInTournamentMode())
            {
                pGameInfo->IncreaseGameNumber(true);
                while (pGameInfo->GetCurrentRoundNumber() != -5)
                {
                    if (pGameInfo->DetermineNextMatchups(0x1b))
                        break;
                    pGameInfo->IncreaseRoundNumber();
                }

                CupHubScene* scene = (CupHubScene*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_TOURNAMENT_STANDINGS_ANIM, (ScreenMovement)0, false);
                scene->mDoAutoSave = true;
                FEMusic::StartStreamIfDifferent(4);
            }
            else
            {
                pGameInfo->OnPostCupGameState();
                if (nlSingleton<GameInfoManager>::Instance()->IsInRegularCupMode())
                {
                    FEMusic::StartStreamIfDifferent(2);
                }
                else if (nlSingleton<GameInfoManager>::Instance()->IsInSuperCupMode())
                {
                    FEMusic::StartStreamIfDifferent(3);
                }
            }
        }
        else
        {
            if (nlSingleton<GameInfoManager>::Instance()->mGoToChooseCaptains)
            {
                nlSingleton<GameInfoManager>::Instance()->mGoToChooseCaptains = false;
                nlSingleton<GameInfoManager>::Instance()->SetMode(GameInfoManager::GM_FRIENDLY);
                nlSingleton<GameInfoManager>::Instance()->ResetPlayingSides();
                nlSingleton<GameSceneManager>::Instance()->Push(SCENE_CHOOSE_CAPTAINS, (ScreenMovement)0, false);
            }
            else
            {
                nlSingleton<GameSceneManager>::Instance()->Push(SCENE_MAIN_MENU, (ScreenMovement)0, false);
                SHMainMenu::mSnapMenuIntoPosition = true;
                SHMainMenu::mLastMenuItem = 0;
            }
            FEMusic::StartStreamIfDifferent(0);
        }
    }

    tDebugPrintManager::Print(DC_MEMORY, "-- Memory upon Exiting InitializeFEState \n");
    tDebugPrintManager::Print(DC_MEMORY, "Free Memory: %u\n", StandardAllocator.TotalFreeMemory());
    tDebugPrintManager::Print(DC_MEMORY, "Largest Free Block: %u\n", StandardAllocator.LargestFreeBlock());
    tDebugPrintManager::Print(DC_MEMORY, "-----------------------------------------\n\n");

    m_TransitionState = eTS_InState;
}

void TransitionTask::DestroyFEState()
{
    m_TransitionState = eTS_Destroying;

    g_pFEInput->Reset();

    WaitForAllScenesValid();

    nlSingleton<GameSceneManager>::Instance()->PopEntireStack();

    if (nlSingleton<GameSceneManager>::s_pInstance != NULL)
    {
        delete nlSingleton<GameSceneManager>::s_pInstance;
        nlSingleton<GameSceneManager>::s_pInstance = NULL;
    }

    if (nlSingleton<FESceneManager>::s_pInstance != NULL)
    {
        delete nlSingleton<FESceneManager>::s_pInstance;
        nlSingleton<FESceneManager>::s_pInstance = NULL;
    }

    nlSingleton<FEResourceManager>::Instance()->UnloadPermanentResourceBundle();

    if (nlSingleton<FEResourceManager>::s_pInstance != NULL)
    {
        delete nlSingleton<FEResourceManager>::s_pInstance;
        nlSingleton<FEResourceManager>::s_pInstance = NULL;
    }

    if (nlSingleton<FEAnimModelManager>::s_pInstance != NULL)
    {
        delete nlSingleton<FEAnimModelManager>::s_pInstance;
        nlSingleton<FEAnimModelManager>::s_pInstance = NULL;
    }

    if (nlSingleton<FontManager>::s_pInstance != NULL)
    {
        delete nlSingleton<FontManager>::s_pInstance;
        nlSingleton<FontManager>::s_pInstance = NULL;
    }

    AudioLoader::UnloadFE();
    nlFree(g_pLocalization->m_pFile);
    glResourceRelease(s_FontResourceMark);

    CompactSlotPools();

    m_TransitionState = eTS_Unknown;
}

void TransitionTask::InitializeFEFast()
{
    m_TransitionState = eTS_Initializing;

    tDebugPrintManager::Print(DC_MEMORY, "-- Memory upon Entering InitializeFEFast ----------\n");
    tDebugPrintManager::Print(DC_MEMORY, "Free Memory: %u\n", StandardAllocator.TotalFreeMemory());
    tDebugPrintManager::Print(DC_MEMORY, "Largest Free Block: %u\n", StandardAllocator.LargestFreeBlock());
    tDebugPrintManager::Print(DC_MEMORY, "-----------------------------------------\n\n");

    EnableAutoPressed();

    if (nlSingleton<FEResourceManager>::s_pInstance == NULL)
    {
        nlSingleton<FEResourceManager>::s_pInstance = new (nlMalloc(sizeof(FEResourceManager), 8, false)) FEResourceManager();
    }

    nlSingleton<FEResourceManager>::Instance()->Initialize();
    nlSingleton<FEResourceManager>::Instance()->LoadPermanentResourceBundle("art/fe/BootUI.Res");

    if (nlSingleton<FESceneManager>::s_pInstance == NULL)
    {
        nlSingleton<FESceneManager>::s_pInstance = new (nlMalloc(sizeof(FESceneManager), 8, false)) FESceneManager();
    }

    nlSingleton<FESceneManager>::Instance()->m_uDefaultRenderView = 31;

    if (nlSingleton<GameSceneManager>::s_pInstance == NULL)
    {
        nlSingleton<GameSceneManager>::s_pInstance = new (nlMalloc(sizeof(GameSceneManager), 8, false)) GameSceneManager();
    }

    DisplayFirstScreen();

    tDebugPrintManager::Print(DC_MEMORY, "-- Memory upon Exiting InitializeFEFast\n");
    tDebugPrintManager::Print(DC_MEMORY, "Free Memory: %u\n", StandardAllocator.TotalFreeMemory());
    tDebugPrintManager::Print(DC_MEMORY, "Largest Free Block: %u\n", StandardAllocator.LargestFreeBlock());
    tDebugPrintManager::Print(DC_MEMORY, "-----------------------------------------\n\n");

    m_TransitionState = eTS_InState;
}

void TransitionTask::DestroyFEFast()
{
    m_TransitionState = eTS_Destroying;

    g_pFEInput->Reset();

    WaitForAllScenesValid();

    nlSingleton<GameSceneManager>::Instance()->PopEntireStack();

    if (nlSingleton<GameSceneManager>::s_pInstance != NULL)
    {
        delete nlSingleton<GameSceneManager>::s_pInstance;
        nlSingleton<GameSceneManager>::s_pInstance = NULL;
    }

    if (nlSingleton<FESceneManager>::s_pInstance != NULL)
    {
        delete nlSingleton<FESceneManager>::s_pInstance;
        nlSingleton<FESceneManager>::s_pInstance = NULL;
    }

    nlSingleton<FEResourceManager>::Instance()->UnloadPermanentResourceBundle();

    if (nlSingleton<FEResourceManager>::s_pInstance != NULL)
    {
        delete nlSingleton<FEResourceManager>::s_pInstance;
        nlSingleton<FEResourceManager>::s_pInstance = NULL;
    }

    CompactSlotPools();

    m_TransitionState = eTS_Unknown;
}

void TransitionTask::CompactSlotPools()
{
    cPN_SAnimController::m_SAnimControllerSlotPool.FreeBlocks();
    cPN_Blender::m_BlenderSlotPool.FreeBlocks();
    cPN_SingleAxisBlender::m_SingleAxisBlenderSlotPool.FreeBlocks();
    cPN_Feather::m_FeatherSlotPool.FreeBlocks();

    glViewCompact();
}

void TransitionTask::DisplayFirstScreen()
{
    bool isBPressed = g_pFEInput->IsPressed(FE_ALL_PADS, 0x200, false, NULL);

    if (VIGetTvFormat() == 5)
    {
        glx_SetPal50Mode();
    }

    do
    {
        if (VIGetTvFormat() == 0)
        {
            if (VIGetDTVStatus() != 0 && (OSGetProgressiveMode() == 1 || isBPressed))
            {
                nlSingleton<GameSceneManager>::Instance()->Push(SCENE_PROGRESSIVE_SCAN, (ScreenMovement)0, false);
                break;
            }
        }
        else if (VIGetTvFormat() == 1)
        {
            nlSingleton<GameSceneManager>::Instance()->Push(SCENE_EURO_RGB60, (ScreenMovement)0, false);
            break;
        }

        nlSingleton<GameSceneManager>::Instance()->Push(SCENE_HEALTH_WARNING, (ScreenMovement)0, false);
    } while (false);
}
