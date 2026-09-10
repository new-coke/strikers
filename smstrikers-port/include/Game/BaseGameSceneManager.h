#ifndef _BASEGAMESCENEMANAGER_H_
#define _BASEGAMESCENEMANAGER_H_

#include "types.h"

#include "Game/BaseSceneHandler.h"
#include "NL/nlBasicString.h"

enum SceneList
{
    SCENE_INVALID = -2,
    SCENE_TESTSCENE = -1,
    SCENE_FRIENDLY_BACKGROUND = 0,
    SCENE_MARIO_BACKGROUND = 1,
    SCENE_TITLE = 2,
    SCENE_MAIN_MENU = 3,
    SCENE_CHOOSE_SIDES_FRIENDLY = 4,
    SCENE_CHOOSE_SIDES_CUP = 5,
    SCENE_CHOOSE_SIDES_SUPER_CUP = 6,
    SCENE_CHOOSE_SIDES_TOURNAMENT = 7,
    SCENE_CHOOSE_CAPTAINS = 8,
    SCENE_STADIUM_SELECT = 9,
    SCENE_CUP_CHEATER = 10,
    SCENE_CUP_BACKGROUND = 11,
    SCENE_SUPER_CUP_BACKGROUND = 12,
    SCENE_CUP_CHOOSE_CUP = 13,
    SCENE_SUPER_CUP_CHOOSE_CUP = 14,
    SCENE_CUP_CHOOSE_CAPTAIN = 15,
    SCENE_SUPER_CUP_CHOOSE_CAPTAIN = 16,
    SCENE_CUP_STANDINGS = 17,
    SCENE_CUP_STANDINGS_ANIM = 18,
    SCENE_CUP_STANDINGS_FINAL_ANIM = 19,
    SCENE_SUPER_CUP_STANDINGS = 20,
    SCENE_SUPER_CUP_STANDINGS_ANIM = 21,
    SCENE_SUPER_CUP_STANDINGS_FINAL_ANIM = 22,
    SCENE_TOURNAMENT_STANDINGS = 23,
    SCENE_TOURNAMENT_STANDINGS_ANIM = 24,
    SCENE_TOURNAMENT_STANDINGS_FINAL_ANIM = 25,
    SCENE_CUP_SUPER_TEAM = 26,
    SCENE_POPUP_MENU = 27,
    SCENE_TROPHY_ROOM = 28,
    SCENE_SCROLLING_TICKER = 29,
    SCENE_MAIN_BACKGROUND = 30,
    SCENE_SAVE = 31,
    SCENE_LOAD = 32,
    SCENE_ASK_SAVE = 33,
    SCENE_ASK_LOAD = 34,
    SCENE_SHOULD_LOAD_OR_SAVE = 35,
    SCENE_TOURN_SETPARAMS = 36,
    SCENE_TOURN_SETTEAMS = 37,
    SCENE_OPTIONS = 38,
    SCENE_LEGAL = 39,
    SCENE_CUP_OPTIONS_INITIAL_CUP = 40,
    SCENE_CUP_OPTIONS_INITIAL_SUPER = 41,
    SCENE_CUP_OPTIONS_INITIAL_TOURN = 42,
    SCENE_SUPER_LOADING = 43,
    SCENE_CUP_TROPHY = 44,
    SCENE_MILESTONE_TROPHY = 45,
    SCENE_CUP_BRAG = 46,
    SCENE_TOURNEY_BRAG = 47,
    SCENE_MOVIE_PLAYER = 48,
    SCENE_QUICK_GAMEPLAY_OPTIONS = 49,
    SCENE_LOADING_TRANSITION = 50,
    SCENE_HEALTH_WARNING = 51,
    SCENE_NLG_MOVIE = 52,
    SCENE_INTRO_MOVIE = 53,
    SCENE_CREDITS = 54,
    SCENE_PROGRESSIVE_SCAN = 55,
    SCENE_EURO_RGB60 = 56,
    IGSCENE_PAUSE = 57,
    IGSCENE_CHOOSE_SIDES = 58,
    IGSCENE_PAUSE_AUDIO = 59,
    IGSCENE_PAUSE_VISUAL = 60,
    IGSCENE_PAUSE_POST_GAME = 61,
    IGSCENE_STRIKERS_101_PAUSE = 62,
    IGSCENE_LESSON = 63,
    IGSCENE_LESSON_SELECT = 64,
    IGSCENE_LESSON_MOVIE_PLAYER = 65,
    SCENE_LAST = 66,
    OVERLAY_START = 66,
    OVERLAY_HUD = 67,
    OVERLAY_TEXT = 68,
    OVERLAY_POPUP = 69,
    OVERLAY_SUMMARY = 70,
    OVERLAY_SUMMARY_PAUSE = 71,
    OVERLAY_GOAL = 72,
    OVERLAY_BRAG = 73,
    OVERLAY_DEMO = 74,
    OVERLAY_WINNER = 75,
    OVERLAY_LESSON_TICKER = 76,
    NUM_SCENES = 77,
};

enum ScreenMovement
{
    SCREEN_NOTHING = 0,
    SCREEN_FORWARD = 1,
    SCREEN_BACK = 2,
};

class BaseGameSceneManager
{
public:
    BaseGameSceneManager();
    virtual ~BaseGameSceneManager();
    virtual BaseSceneHandler* Push(SceneList newscene, ScreenMovement movement, bool popfirst);
    BaseSceneHandler* GetScene(SceneList scene);
    BaseSceneHandler* GetCurrentScene()
    {
        return mCurrentStackDepth != 0 ? mBaseSceneHandlerStack[mCurrentStackDepth - 1] : NULL;
    }
    virtual void Pop();
    void PopEntireStack();
    SceneList GetSceneType(BaseSceneHandler* scene);
    bool IsOnStack(SceneList scene);
    BasicString<char, Detail::TempStringAllocator> GetFileName(SceneList scene);
    void PushLoadingScene(bool popfirst);
    static bool GetVisible(SceneList sceneid);
    static void SetVisible(SceneList sceneid, bool visibility);

public:
    static const u32 MAX_SCENE_DEPTH = 32;
    /* 0x04 */ u32 mCurrentStackDepth;
    /* 0x08 */ SceneList m_sceneStack[MAX_SCENE_DEPTH];
    /* 0x88 */ BaseSceneHandler* mBaseSceneHandlerStack[MAX_SCENE_DEPTH];
};

#endif // _BASEGAMESCENEMANAGER_H_
