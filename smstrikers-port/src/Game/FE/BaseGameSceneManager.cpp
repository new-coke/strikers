#include "Game/BaseGameSceneManager.h"
#include <stdlib.h>
#include "dolphin/os.h"
#include "NL/nlString.h"
#include "port/overlay.h"
extern "C" void PortNoteSceneEntered(int);
#include "Game/main.h"
#include "Game/SH/SHBackground.h"
#include "Game/SH/SHTitleScreen.h"
#include "Game/SH/SHMainMenu.h"
#include "Game/SH/SHChooseSides.h"
#include "Game/SH/SHChooseCaptains.h"
#include "Game/SH/SHStadiumSelect.h"
#include "Game/SH/SHCupCheater.h"
#include "Game/SH/SHChooseCup.h"
#include "Game/SH/SHCupChooseCaptain.h"
#include "Game/SH/SHCupHub.h"
#include "Game/SH/SHSuperTeam.h"
#include "Game/SH/SHSpoils.h"
#include "Game/SH/SHSaveLoad.h"
#include "Game/SH/SHTournSetParams.h"
#include "Game/SH/SHTournTeamSetup.h"
#include "Game/SH/SHOptions.h"
#include "Game/SH/SHCrossFader.h"
#include "Game/SH/SHCupOptions.h"
#include "Game/SH/SHLoading.h"
#include "Game/SH/SHCupTrophy.h"
#include "Game/SH/SHMilestoneTrophy.h"
#include "Game/SH/SHMoviePlayer.h"
#include "Game/SH/SHQuickGameplayOptions.h"
#include "Game/SH/SHLoadingTransition.h"
#include "Game/SH/SHHealthWarning.h"
#include "Game/SH/SHCredits.h"
#include "Game/SH/SHProgressiveScan.h"
#include "Game/SH/SHPause.h"
#include "Game/SH/SHPauseOptions.h"
#include "Game/SH/SHPausePostGame.h"
#include "Game/SH/SHLesson.h"
#include "Game/SH/SHLessonSelect.h"
#include "Game/FE/feSceneManager.h"
#include "Game/FE/fePopupMenu.h"
#include "Game/FE/feScrollingTicker.h"
#include "Game/FE/BraggingRights.h"
#include "Game/FE/feNSNMessenger.h"
#include "Game/FE/FEAudio.h"
#include "Game/FE/Overlay/OverlayHandlerSummary.h"
#include "Game/FE/Overlay/OverlayHandlerWinner.h"
#include "Game/OverlayHandlerHUD.h"
#include "Game/OverlayHandlerInGameText.h"
#include "Game/OverlayHandlerGoal.h"
#include "Game/OverlayHandlerDemo.h"
#include "Game/OverlayHandlerSTSX2.h"

#include "Game/BaseSceneHandler.h"

#include "NL/nlConfig.h"
#include "types.h"

struct SceneEntry
{
    SceneList mSceneID;
    const char* mFenFileName;
};

struct SceneEntry SceneEntryTable[] = {
    { SCENE_FRIENDLY_BACKGROUND, "art/fe/background.fen" },
    { SCENE_MARIO_BACKGROUND, "art/fe/mariobg.fen" },
    { SCENE_TITLE, "art/fe/start_screen_v2.fen" },
    { SCENE_MAIN_MENU, "art/fe/main_menuv2.fen" },
    { SCENE_CHOOSE_SIDES_FRIENDLY, "art/fe/choose_sides_v3.fen" },
    { SCENE_CHOOSE_SIDES_CUP, "art/fe/choose_sides_v3.fen" },
    { SCENE_CHOOSE_SIDES_SUPER_CUP, "art/fe/choose_sides_v3.fen" },
    { SCENE_CHOOSE_SIDES_TOURNAMENT, "art/fe/choose_sides_v3.fen" },
    { SCENE_CHOOSE_CAPTAINS, "/art/fe/choose_captains_v3.fen" },
    { SCENE_STADIUM_SELECT, "art/fe/choose_stadiums_v2.fen" },
    { SCENE_CUP_CHEATER, "art/fe/cup_cheater.fen" },
    { SCENE_CUP_BACKGROUND, "art/fe/cup_background.fen" },
    { SCENE_SUPER_CUP_BACKGROUND, "art/fe/super_cup_background.fen" },
    { SCENE_CUP_CHOOSE_CUP, "art/fe/choose_cup_v2.fen" },
    { SCENE_SUPER_CUP_CHOOSE_CUP, "art/fe/choose_cup_v2.fen" },
    { SCENE_CUP_CHOOSE_CAPTAIN, "art/fe/cup_choose_captains_v3.fen" },
    { SCENE_SUPER_CUP_CHOOSE_CAPTAIN, "art/fe/cup_choose_captains_v3.fen" },
    { SCENE_CUP_STANDINGS, "art/fe/cup_standings_v2.fen" },
    { SCENE_CUP_STANDINGS_ANIM, "art/fe/cup_standings_v2.fen" },
    { SCENE_CUP_STANDINGS_FINAL_ANIM, "art/fe/cup_standings_v2.fen" },
    { SCENE_SUPER_CUP_STANDINGS, "art/fe/cup_standings_v2.fen" },
    { SCENE_SUPER_CUP_STANDINGS_ANIM, "art/fe/cup_standings_v2.fen" },
    { SCENE_SUPER_CUP_STANDINGS_FINAL_ANIM, "art/fe/cup_standings_v2.fen" },
    { SCENE_TOURNAMENT_STANDINGS, "art/fe/cup_standings_v2.fen" },
    { SCENE_TOURNAMENT_STANDINGS_ANIM, "art/fe/cup_standings_v2.fen" },
    { SCENE_TOURNAMENT_STANDINGS_FINAL_ANIM, "art/fe/cup_standings_v2.fen" },
    { SCENE_CUP_SUPER_TEAM, "art/fe/super_team_reveal_v3.fen" },
    { SCENE_POPUP_MENU, "art/fe/popup_menu.fen" },
    { SCENE_TROPHY_ROOM, "art/fe/spoils_menu_v2.fen" },
    { SCENE_SCROLLING_TICKER, "art/fe/ticker.fen" },
    { SCENE_MAIN_BACKGROUND, "art/fe/mainstart_background.fen" },
    { SCENE_SAVE, "art/fe/saving_loading.fen" },
    { SCENE_LOAD, "art/fe/saving_loading.fen" },
    { SCENE_ASK_SAVE, "art/fe/saving_loading.fen" },
    { SCENE_ASK_LOAD, "art/fe/saving_loading.fen" },
    { SCENE_SHOULD_LOAD_OR_SAVE, "art/fe/saving_loading.fen" },
    { SCENE_TOURN_SETPARAMS, "art/fe/custom_tournament_options_v2.fen" },
    { SCENE_TOURN_SETTEAMS, "art/fe/custom_tournament_capt_chooser.fen" },
    { SCENE_OPTIONS, "art/fe/main_options_v2.fen" },
    { SCENE_LEGAL, "art/fe/englegal.fen" },
    { SCENE_CUP_OPTIONS_INITIAL_CUP, "art/fe/main_options_v2.fen" },
    { SCENE_CUP_OPTIONS_INITIAL_SUPER, "art/fe/main_options_v2.fen" },
    { SCENE_CUP_OPTIONS_INITIAL_TOURN, "art/fe/main_options_v2.fen" },
    { SCENE_SUPER_LOADING, "art/fe/loadingtest.fen" },
    { SCENE_CUP_TROPHY, "art/fe/spoils_battles_history_v2.fen" },
    { SCENE_MILESTONE_TROPHY, "art/fe/spoils_milestone_history_v2.fen" },
    { SCENE_CUP_BRAG, "art/fe/bragging_rights.fen" },
    { SCENE_TOURNEY_BRAG, "art/fe/player_awards_v3.fen" },
    { SCENE_MOVIE_PLAYER, "art/fe/movieplayer.fen" },
    { SCENE_QUICK_GAMEPLAY_OPTIONS, "art/fe/main_options_v2.fen" },
    { SCENE_LOADING_TRANSITION, "art/fe/vs_transition.fen" },
    { SCENE_HEALTH_WARNING, "art/fe/health_and_safety.fen" },
    { SCENE_NLG_MOVIE, "art/fe/movieplayer.fen" },
    { SCENE_INTRO_MOVIE, "art/fe/movieplayer.fen" },
    { SCENE_CREDITS, "art/fe/credits.fen" },
    { SCENE_PROGRESSIVE_SCAN, "art/fe/progressive_scan.fen" },
    { SCENE_EURO_RGB60, "art/fe/progressive_scan.fen" },
    { IGSCENE_PAUSE, "art/fe/pausemenu_v3.fen" },
    { IGSCENE_CHOOSE_SIDES, "art/fe/choose_sides_v4.fen" },
    { IGSCENE_PAUSE_AUDIO, "art/fe/pause_options_v2.fen" },
    { IGSCENE_PAUSE_VISUAL, "art/fe/pause_options_v2.fen" },
    { IGSCENE_PAUSE_POST_GAME, "art/fe/post_game_menu_v2.fen" },
    { IGSCENE_STRIKERS_101_PAUSE, "art/fe/pausemenu101_v3.fen" },
    { IGSCENE_LESSON, "art/fe/lesson.fen" },
    { IGSCENE_LESSON_SELECT, "art/fe/strikers_101_lessons_v3.fen" },
    { IGSCENE_LESSON_MOVIE_PLAYER, "art/fe/lessonmovieplayer.fen" },
    { OVERLAY_START, NULL },
    { OVERLAY_HUD, "art/fe/hud.fen" },
    { OVERLAY_TEXT, "art/fe/ingame_text.fen" },
    { OVERLAY_POPUP, "art/fe/popup_menu.fen" },
    { OVERLAY_SUMMARY, "art/fe/summary.fen" },
    { OVERLAY_SUMMARY_PAUSE, "art/fe/summary.fen" },
    { OVERLAY_GOAL, "art/fe/goal_overlay.fen" },
    { OVERLAY_BRAG, "art/fe/player_awards_ig_v3.fen" },
    { OVERLAY_DEMO, "art/fe/demo_overlay.fen" },
    { OVERLAY_WINNER, "art/fe/winner.fen" },
    { OVERLAY_LESSON_TICKER, "art/fe/igticker.fen" },
    { NUM_SCENES, "art/fe/x2_sts.fen" },
    { (SceneList)0x4E, "art/fe/loading_screen.fen" },
    { (SceneList)0x4F, NULL }
};

/**
 * Offset/Address/Size: 0x142C | 0x800969E8 | size: 0xB4
 */
BaseGameSceneManager::BaseGameSceneManager()
{
    mCurrentStackDepth = 0;
    for (int i = 0; i < MAX_SCENE_DEPTH; ++i)
    {
        m_sceneStack[i] = SCENE_INVALID;
        mBaseSceneHandlerStack[i] = 0;
    }
}

/**
 * Offset/Address/Size: 0x13B0 | 0x8009696C | size: 0x7C
 */
BaseGameSceneManager::~BaseGameSceneManager()
{
    while (mCurrentStackDepth != 0)
    {
        this->Pop();
    }
}

/**
 * Offset/Address/Size: 0x324 | 0x800958E0 | size: 0x108C
 */
BaseSceneHandler* BaseGameSceneManager::Push(SceneList newscene, ScreenMovement movement, bool popfirst)
{
    if (popfirst)
    {
        Pop();
    }

    if (newscene == SCENE_MAIN_BACKGROUND)
    {
        newscene = SCENE_MARIO_BACKGROUND;
    }

    // PORT: progress marker, see the note on.
    if (getenv("STRIKERS_LOG_SCENES") != NULL)
        OSReport("[port] enter scene %d\n", (int)newscene);

    // PORT: and tell the synthetic-input driver, so a scripted press can be anchored to a screen rather than to a frame number.
    PortNoteSceneEntered((int)newscene);

    // PORT: and the debug menu, which had no way to name the screen it was drawn over.
    {
        char sceneLabel[64];
        BasicString<char, Detail::TempStringAllocator> sceneFile = GetFileName(newscene);
        nlSNPrintf(sceneLabel, sizeof sceneLabel, "%d  %s", (int)newscene,
                   sceneFile.c_str() != NULL ? sceneFile.c_str() : "?");
        PortOverlaySetScene(sceneLabel);
    }

    GetConfigBool(Config::Global(), "fev2", false);

    if (newscene == SCENE_MAIN_MENU)
    {
        Push(SCENE_MARIO_BACKGROUND, SCREEN_NOTHING, false);
    }

    BaseSceneHandler* newHandler = NULL;
    const SceneEntry& entry = SceneEntryTable[newscene];
    const char* filename = entry.mFenFileName;

    GetConfigBool(Config::Global(), "fev2", false);

    if (newscene == SCENE_TITLE)
    {
        if (g_e3_Build || g_Europe)
        {
            filename = "art/fe/start_screen_v2_german.fen";
        }
        else if (g_Language == 5)
        {
            filename = "art/fe/start_screen_jpn.fen";
        }
    }

    if (newscene == SCENE_CHOOSE_CAPTAINS)
    {
        if (g_e3_Build)
        {
            filename = "art/fe/choose_captains_vLeeptsig.fen";
        }
    }

    if (g_Language == 5)
    {
        switch (newscene)
        {
        case IGSCENE_LESSON:
            filename = "art/fe/lesson_jp.fen";
            break;
        case SCENE_CUP_BRAG:
            filename = "art/fe/bragging_rights_jp.fen";
            break;
        case SCENE_CHOOSE_CAPTAINS:
            filename = "art/fe/choose_captains_v3_jp.fen";
            break;
        case SCENE_CHOOSE_SIDES_FRIENDLY:
        case SCENE_CHOOSE_SIDES_CUP:
        case SCENE_CHOOSE_SIDES_SUPER_CUP:
        case SCENE_CHOOSE_SIDES_TOURNAMENT:
            filename = "art/fe/choose_sides_v3_jp.fen";
            break;
        case IGSCENE_CHOOSE_SIDES:
            filename = "art/fe/choose_sides_v4_jp.fen";
            break;
        case SCENE_CUP_TROPHY:
            filename = "art/fe/spoils_battles_history_v2_jp.fen";
            break;
        case SCENE_CUP_CHOOSE_CAPTAIN:
        case SCENE_SUPER_CUP_CHOOSE_CAPTAIN:
            filename = "art/fe/cup_choose_captains_v3_jp.fen";
            break;
        case SCENE_CUP_STANDINGS:
        case SCENE_CUP_STANDINGS_ANIM:
        case SCENE_CUP_STANDINGS_FINAL_ANIM:
        case SCENE_SUPER_CUP_STANDINGS:
        case SCENE_SUPER_CUP_STANDINGS_ANIM:
        case SCENE_SUPER_CUP_STANDINGS_FINAL_ANIM:
        case SCENE_TOURNAMENT_STANDINGS:
        case SCENE_TOURNAMENT_STANDINGS_ANIM:
        case SCENE_TOURNAMENT_STANDINGS_FINAL_ANIM:
            filename = "art/fe/cup_standings_v2_jp.fen";
            break;
        case SCENE_MILESTONE_TROPHY:
            filename = "art/fe/spoils_milestone_history_v2_jp.fen";
            break;
        case SCENE_POPUP_MENU:
        case OVERLAY_POPUP:
            filename = "art/fe/popup_menu_jp.fen";
            break;
        case SCENE_SAVE:
        case SCENE_LOAD:
        case SCENE_ASK_SAVE:
        case SCENE_ASK_LOAD:
        case SCENE_SHOULD_LOAD_OR_SAVE:
            filename = "art/fe/saving_loading_jp.fen";
            break;
        case SCENE_OPTIONS:
            filename = "art/fe/main_options_v2_jp.fen";
            break;
        case SCENE_TOURN_SETTEAMS:
            filename = "art/fe/custom_tournament_capt_chooser_jp.fen";
            break;
        }
    }

    switch (newscene)
    {
    case SCENE_FRIENDLY_BACKGROUND:
        newHandler = new (nlMalloc(sizeof(BackgroundScene), 8, false)) BackgroundScene();
        break;
    case SCENE_MARIO_BACKGROUND:
        newHandler = new (nlMalloc(sizeof(BackgroundScene), 8, false)) BackgroundScene();
        break;
    case SCENE_TITLE:
        newHandler = new (nlMalloc(sizeof(TitleScene), 8, false)) TitleScene();
        break;
    case SCENE_MAIN_MENU:
        newHandler = new (nlMalloc(sizeof(SHMainMenu), 8, false)) SHMainMenu();
        break;
    case SCENE_CHOOSE_SIDES_FRIENDLY:
        newHandler = new (nlMalloc(sizeof(SHChooseSides2), 8, false)) SHChooseSides2((SHChooseSides2::eCSContext)0);
        break;
    case SCENE_CHOOSE_SIDES_CUP:
        newHandler = new (nlMalloc(sizeof(SHChooseSides2), 8, false)) SHChooseSides2((SHChooseSides2::eCSContext)1);
        break;
    case SCENE_CHOOSE_SIDES_SUPER_CUP:
        newHandler = new (nlMalloc(sizeof(SHChooseSides2), 8, false)) SHChooseSides2((SHChooseSides2::eCSContext)2);
        break;
    case SCENE_CHOOSE_SIDES_TOURNAMENT:
        newHandler = new (nlMalloc(sizeof(SHChooseSides2), 8, false)) SHChooseSides2((SHChooseSides2::eCSContext)3);
        break;
    case SCENE_CHOOSE_CAPTAINS:
        newHandler = new (nlMalloc(sizeof(ChooseCaptainsSceneV2), 8, false)) ChooseCaptainsSceneV2((ChooseCaptainsSceneV2::SceneType)0);
        break;
    case SCENE_STADIUM_SELECT:
        newHandler = new (nlMalloc(sizeof(StadiumSelectSceneV2), 8, false)) StadiumSelectSceneV2();
        break;
    case SCENE_CUP_CHEATER:
        newHandler = new (nlMalloc(sizeof(CupCheaterScene), 8, false)) CupCheaterScene();
        break;
    case SCENE_CUP_BACKGROUND:
        newHandler = new (nlMalloc(sizeof(BackgroundScene), 8, false)) BackgroundScene();
        break;
    case SCENE_SUPER_CUP_BACKGROUND:
        newHandler = new (nlMalloc(sizeof(BackgroundScene), 8, false)) BackgroundScene();
        break;
    case SCENE_CUP_CHOOSE_CUP:
        newHandler = new (nlMalloc(sizeof(ChooseCupSceneV2), 8, false)) ChooseCupSceneV2(false);
        break;
    case SCENE_SUPER_CUP_CHOOSE_CUP:
        newHandler = new (nlMalloc(sizeof(ChooseCupSceneV2), 8, false)) ChooseCupSceneV2(true);
        break;
    case SCENE_CUP_CHOOSE_CAPTAIN:
        newHandler = new (nlMalloc(sizeof(CupChooseCaptainSceneV2), 8, false)) CupChooseCaptainSceneV2(false);
        break;
    case SCENE_SUPER_CUP_CHOOSE_CAPTAIN:
        newHandler = new (nlMalloc(sizeof(CupChooseCaptainSceneV2), 8, false)) CupChooseCaptainSceneV2(true);
        break;
    case SCENE_CUP_STANDINGS:
        newHandler = new (nlMalloc(sizeof(CupHubScene), 8, false)) CupHubScene(false, false);
        break;
    case SCENE_CUP_STANDINGS_ANIM:
        newHandler = new (nlMalloc(sizeof(CupHubScene), 8, false)) CupHubScene(true, false);
        break;
    case SCENE_CUP_STANDINGS_FINAL_ANIM:
        newHandler = new (nlMalloc(sizeof(CupHubScene), 8, false)) CupHubScene(false, true);
        break;
    case SCENE_SUPER_CUP_STANDINGS:
        newHandler = new (nlMalloc(sizeof(CupHubScene), 8, false)) CupHubScene(false, false);
        break;
    case SCENE_SUPER_CUP_STANDINGS_ANIM:
        newHandler = new (nlMalloc(sizeof(CupHubScene), 8, false)) CupHubScene(true, false);
        break;
    case SCENE_SUPER_CUP_STANDINGS_FINAL_ANIM:
        newHandler = new (nlMalloc(sizeof(CupHubScene), 8, false)) CupHubScene(false, true);
        break;
    case SCENE_TOURNAMENT_STANDINGS:
        newHandler = new (nlMalloc(sizeof(CupHubScene), 8, false)) CupHubScene(false, false);
        break;
    case SCENE_TOURNAMENT_STANDINGS_ANIM:
        newHandler = new (nlMalloc(sizeof(CupHubScene), 8, false)) CupHubScene(true, false);
        break;
    case SCENE_TOURNAMENT_STANDINGS_FINAL_ANIM:
        newHandler = new (nlMalloc(sizeof(CupHubScene), 8, false)) CupHubScene(false, true);
        break;
    case SCENE_CUP_SUPER_TEAM:
        newHandler = new (nlMalloc(sizeof(SuperTeamScene), 8, false)) SuperTeamScene();
        break;
    case SCENE_POPUP_MENU:
        newHandler = new (nlMalloc(sizeof(FEPopupMenu), 8, false)) FEPopupMenu();
        break;
    case SCENE_TROPHY_ROOM:
        newHandler = new (nlMalloc(sizeof(SpoilsScene), 8, false)) SpoilsScene();
        break;
    case SCENE_SCROLLING_TICKER:
    {
        ScrollingTickerScene* ticker = new (nlMalloc(sizeof(ScrollingTickerScene), 8, false)) ScrollingTickerScene();
        newHandler = ticker;
        break;
    }
    case SCENE_MAIN_BACKGROUND:
        newHandler = new (nlMalloc(sizeof(BackgroundScene), 8, false)) BackgroundScene();
        break;
    case SCENE_SAVE:
        newHandler = new (nlMalloc(sizeof(SaveLoadScene), 8, false)) SaveLoadScene((SaveLoadScene::eSaveLoadMode)1);
        break;
    case SCENE_LOAD:
        newHandler = new (nlMalloc(sizeof(SaveLoadScene), 8, false)) SaveLoadScene((SaveLoadScene::eSaveLoadMode)3);
        break;
    case SCENE_ASK_SAVE:
        newHandler = new (nlMalloc(sizeof(SaveLoadScene), 8, false)) SaveLoadScene((SaveLoadScene::eSaveLoadMode)2);
        break;
    case SCENE_ASK_LOAD:
        newHandler = new (nlMalloc(sizeof(SaveLoadScene), 8, false)) SaveLoadScene((SaveLoadScene::eSaveLoadMode)4);
        break;
    case SCENE_SHOULD_LOAD_OR_SAVE:
        newHandler = new (nlMalloc(sizeof(SaveLoadScene), 8, false)) SaveLoadScene((SaveLoadScene::eSaveLoadMode)0);
        break;
    case SCENE_TOURN_SETPARAMS:
        newHandler = new (nlMalloc(sizeof(TournSetParamsScene), 8, false)) TournSetParamsScene();
        break;
    case SCENE_TOURN_SETTEAMS:
        newHandler = new (nlMalloc(sizeof(TournTeamSetupSceneV2), 8, false)) TournTeamSetupSceneV2();
        break;
    case SCENE_OPTIONS:
        newHandler = new (nlMalloc(sizeof(OptionsScene), 8, false)) OptionsScene();
        break;
    case SCENE_LEGAL:
        newHandler = new (nlMalloc(sizeof(CrossFaderScene), 8, false)) CrossFaderScene();
        break;
    case SCENE_CUP_OPTIONS_INITIAL_CUP:
        newHandler = new (nlMalloc(sizeof(CupOptionsScene), 8, false)) CupOptionsScene(SCENE_CUP_CHOOSE_CAPTAIN, SCENE_CUP_CHOOSE_CUP);
        break;
    case SCENE_CUP_OPTIONS_INITIAL_SUPER:
        newHandler = new (nlMalloc(sizeof(CupOptionsScene), 8, false)) CupOptionsScene(SCENE_SUPER_CUP_CHOOSE_CAPTAIN, SCENE_SUPER_CUP_CHOOSE_CUP);
        break;
    case SCENE_CUP_OPTIONS_INITIAL_TOURN:
        newHandler = new (nlMalloc(sizeof(CupOptionsScene), 8, false)) CupOptionsScene(SCENE_TOURN_SETTEAMS, SCENE_TOURN_SETPARAMS);
        break;
    case SCENE_SUPER_LOADING:
        newHandler = new (nlMalloc(sizeof(SuperLoadingScene), 8, false)) SuperLoadingScene();
        break;
    case SCENE_CUP_TROPHY:
        newHandler = new (nlMalloc(sizeof(CupTrophyScene), 8, false)) CupTrophyScene();
        break;
    case SCENE_MILESTONE_TROPHY:
        newHandler = new (nlMalloc(sizeof(MilestoneTrophyScene), 8, false)) MilestoneTrophyScene();
        break;
    case SCENE_CUP_BRAG:
        newHandler = new (nlMalloc(sizeof(BraggingRightsScene), 8, false)) BraggingRightsScene();
        break;
    case SCENE_TOURNEY_BRAG:
        newHandler = new (nlMalloc(sizeof(BraggingRightsOverlay), 8, false)) BraggingRightsOverlay();
        break;
    case SCENE_MOVIE_PLAYER:
        newHandler = new (nlMalloc(sizeof(MoviePlayerScene), 8, false)) MoviePlayerScene();
        break;
    case SCENE_QUICK_GAMEPLAY_OPTIONS:
        newHandler = new (nlMalloc(sizeof(QuickGameplayOptionsScene), 8, false)) QuickGameplayOptionsScene();
        break;
    case SCENE_LOADING_TRANSITION:
        newHandler = new (nlMalloc(sizeof(LoadingTransitionScene), 8, false)) LoadingTransitionScene();
        break;
    case SCENE_HEALTH_WARNING:
        newHandler = new (nlMalloc(sizeof(HealthWarningSceneV2), 8, false)) HealthWarningSceneV2();
        break;
    case SCENE_NLG_MOVIE:
        newHandler = new (nlMalloc(sizeof(NLGLogoMovieScene), 8, false)) NLGLogoMovieScene();
        break;
    case SCENE_INTRO_MOVIE:
        newHandler = new (nlMalloc(sizeof(IntroMovieScene), 8, false)) IntroMovieScene();
        break;
    case SCENE_CREDITS:
        newHandler = new (nlMalloc(sizeof(CreditScene), 8, false)) CreditScene();
        break;
    case SCENE_PROGRESSIVE_SCAN:
        newHandler = new (nlMalloc(sizeof(ProgressiveScanScene), 8, false)) ProgressiveScanScene(false);
        break;
    case SCENE_EURO_RGB60:
        newHandler = new (nlMalloc(sizeof(ProgressiveScanScene), 8, false)) ProgressiveScanScene(true);
        break;
    case IGSCENE_PAUSE:
        newHandler = new (nlMalloc(sizeof(PauseMenuScene), 8, false)) PauseMenuScene((PauseMenuScene::ScreenContext)0);
        break;
    case IGSCENE_CHOOSE_SIDES:
        newHandler = new (nlMalloc(sizeof(SHChooseSides2), 8, false)) SHChooseSides2((SHChooseSides2::eCSContext)4);
        break;
    case IGSCENE_PAUSE_AUDIO:
        newHandler = new (nlMalloc(sizeof(PauseOptionsScene), 8, false)) PauseOptionsScene((PauseOptionsScene::Mode)0);
        break;
    case IGSCENE_PAUSE_VISUAL:
        newHandler = new (nlMalloc(sizeof(PauseOptionsScene), 8, false)) PauseOptionsScene((PauseOptionsScene::Mode)1);
        break;
    case IGSCENE_PAUSE_POST_GAME:
        newHandler = new (nlMalloc(sizeof(PausePostGameScene), 8, false)) PausePostGameScene();
        break;
    case IGSCENE_STRIKERS_101_PAUSE:
        newHandler = new (nlMalloc(sizeof(PauseMenuScene), 8, false)) PauseMenuScene((PauseMenuScene::ScreenContext)1);
        break;
    case IGSCENE_LESSON:
        newHandler = new (nlMalloc(sizeof(LessonScene), 8, false)) LessonScene();
        break;
    case IGSCENE_LESSON_SELECT:
        newHandler = new (nlMalloc(sizeof(LessonSelectScene), 8, false)) LessonSelectScene();
        break;
    case IGSCENE_LESSON_MOVIE_PLAYER:
        newHandler = new (nlMalloc(sizeof(LessonMoviePlayerScene), 8, false)) LessonMoviePlayerScene();
        break;
    case OVERLAY_START:
        break;
    case OVERLAY_HUD:
        newHandler = new (nlMalloc(sizeof(HUDOverlay), 8, false)) HUDOverlay();
        break;
    case OVERLAY_TEXT:
        newHandler = new (nlMalloc(sizeof(InGameTextOverlay), 8, false)) InGameTextOverlay();
        break;
    case OVERLAY_POPUP:
        newHandler = new (nlMalloc(sizeof(FEPopupMenu), 8, false)) FEPopupMenu();
        break;
    case OVERLAY_SUMMARY:
        newHandler = new (nlMalloc(sizeof(SummaryOverlay), 8, false)) SummaryOverlay((SummaryOverlay::eSummaryContext)0);
        break;
    case OVERLAY_SUMMARY_PAUSE:
        newHandler = new (nlMalloc(sizeof(SummaryOverlay), 8, false)) SummaryOverlay((SummaryOverlay::eSummaryContext)1);
        break;
    case OVERLAY_GOAL:
        newHandler = new (nlMalloc(sizeof(GoalOverlay), 8, false)) GoalOverlay();
        break;
    case OVERLAY_BRAG:
        newHandler = new (nlMalloc(sizeof(BraggingRightsOverlay), 8, false)) BraggingRightsOverlay();
        break;
    case OVERLAY_DEMO:
        newHandler = new (nlMalloc(sizeof(DemoOverlay), 8, false)) DemoOverlay();
        break;
    case OVERLAY_WINNER:
        newHandler = new (nlMalloc(sizeof(WinnerOverlay), 8, false)) WinnerOverlay();
        break;
    case OVERLAY_LESSON_TICKER:
    {
        NSNMessengerScene* nsn = new (nlMalloc(sizeof(NSNMessengerScene), 8, false)) NSNMessengerScene();
        newHandler = nsn;
        break;
    }
    case NUM_SCENES:
        newHandler = new (nlMalloc(sizeof(STSX2Overlay), 8, false)) STSX2Overlay();
        break;
    case 78:
        newHandler = new (nlMalloc(sizeof(BaseSceneHandler), 8, false)) BaseSceneHandler();
        break;
    }

    FESceneManager::Instance()->QueueScenePush(newHandler, filename);

    m_sceneStack[mCurrentStackDepth] = newscene;
    mBaseSceneHandlerStack[mCurrentStackDepth] = newHandler;
    mCurrentStackDepth++;

    switch (movement)
    {
    case SCREEN_FORWARD:
        FEAudio::PlayAnimAudioEvent("sfx_screen_forward", false);
        break;
    case SCREEN_BACK:
        FEAudio::PlayAnimAudioEvent("sfx_screen_back", false);
        break;
    }

    return newHandler;
}

/**
 * Offset/Address/Size: 0x2D8 | 0x80095894 | size: 0x4C
 */
BaseSceneHandler* BaseGameSceneManager::GetScene(SceneList scene)
{
    BaseSceneHandler* returnValue = NULL;

    for (int i = 0; i < mCurrentStackDepth; ++i)
    {
        if (m_sceneStack[i] == scene)
        {
            returnValue = mBaseSceneHandlerStack[i];
            break;
        }
    }

    return returnValue;
}

/**
 * Offset/Address/Size: 0x288 | 0x80095844 | size: 0x50
 */
void BaseGameSceneManager::Pop()
{
    FESceneManager::Instance()->QueueScenePop();
    mBaseSceneHandlerStack[mCurrentStackDepth] = 0;
    mCurrentStackDepth = (mCurrentStackDepth - 1);
}

/**
 * Offset/Address/Size: 0x23C | 0x800957F8 | size: 0x4C
 */
void BaseGameSceneManager::PopEntireStack()
{
    while (mCurrentStackDepth != 0)
    {
        this->Pop();
    }
}

void BaseGameSceneManager::SetVisible(SceneList sceneid, bool visibility)
{
    u32 uHashID = nlStringLowerHash(SceneEntryTable[sceneid].mFenFileName);
    BaseSceneHandler* sceneHandler = FESceneManager::Instance()->GetSceneHandler(uHashID);
    if (sceneHandler != NULL)
    {
        sceneHandler->SetVisible(visibility);
    }
}

bool BaseGameSceneManager::GetVisible(SceneList sceneid)
{
    u32 uHashID = nlStringLowerHash(SceneEntryTable[sceneid].mFenFileName);
    BaseSceneHandler* sceneHandler = FESceneManager::Instance()->GetSceneHandler(uHashID);
    if (sceneHandler != NULL)
    {
        return sceneHandler->m_bVisible;
    }
    return false;
}

/**
 * Offset/Address/Size: 0x1F4 | 0x800957B0 | size: 0x48
 */
SceneList BaseGameSceneManager::GetSceneType(BaseSceneHandler* scene)
{
    for (int i = 0; i < mCurrentStackDepth; ++i)
    {
        if (mBaseSceneHandlerStack[i] == scene)
        {
            return m_sceneStack[i];
        }
    }
    return SCENE_INVALID;
}

/**
 * Offset/Address/Size: 0x1C0 | 0x8009577C | size: 0x34
 */
bool BaseGameSceneManager::IsOnStack(SceneList scene)
{
    for (int i = 0; i < mCurrentStackDepth; ++i)
    {
        if (m_sceneStack[i] == scene)
            return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0x68 | 0x80095624 | size: 0x158
 */
BasicString<char, Detail::TempStringAllocator> BaseGameSceneManager::GetFileName(SceneList scene)
{
    BasicString<char, Detail::TempStringAllocator> retVal(
        SceneEntryTable[scene].mFenFileName);
    return retVal;
}

/**
 * Offset/Address/Size: 0x0 | 0x800955BC | size: 0x68
 */
void BaseGameSceneManager::PushLoadingScene(bool popfirst)
{
    if (popfirst)
    {
        this->Pop();
    }

    SuperLoadingScene* scene = (SuperLoadingScene*)Push(SCENE_SUPER_LOADING, SCREEN_FORWARD, false);
    scene->mType = SuperLoadingScene::TT_IN;
}
