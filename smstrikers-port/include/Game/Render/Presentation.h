#ifndef _PRESENTATION_H_
#define _PRESENTATION_H_

#include "types.h"
#include "Game/BaseGameSceneManager.h"
#include "Game/InterpreterCore.h"
#include "Game/NisPlayer.h"
#include "Game/Replay.h"
#include "Game/Sys/eventman.h"
#include "Game/OverlayManager.h"

enum HighlightQuality
{
    HIGHLIGHT_QUALITY_EMPTY = 0,
    HIGHLIGHT_QUALITY_SAVE = 1,
    HIGHLIGHT_QUALITY_GOAL_DECREASE_DIFF = 2,
    HIGHLIGHT_QUALITY_GOAL_EQUALIZER = 3,
    HIGHLIGHT_QUALITY_GOAL_INCREASE_DIFF = 4,
    NUM_QUALITY_LEVELS = 5,
};

class Presentation : public InterpreterCore
{
public:
    Presentation();
    virtual void DoFunctionCall(unsigned int);

    static Presentation& Instance();

    void LoadTrophyModel();
    void Finish();
    void Update(float deltaT);
    bool DuringEndOfGamePresentation() const;
    bool DetectSkipPress() const;
    void HandleOverlay(float deltaT);
    void Call(const char* functionName, const char* nisFilter);
    void EventHandler(Event* event);
    static void EventHandler(Event* event, void* userData) { ((Presentation*)userData)->EventHandler(event); }
    void PlayOverlay(const char* name, float delay, float length);
    void StopOverlay();
    void Reset();
    static void RaiseEvent(const char* type, const char* param);
    void BeginByPass();
    void EndByPass();
    void PlayCharacterDirection();
    void PlayCupOverlay();
    void PlayHighlights();
    void UpdateAndRenderLetterBox();
    void PlayAutoReplay(ReplayType type);
    void LoadNis(const char* name, NisTarget target, NisUseStadiumOffset stadiumOffset, NisUseFilter filter, NisWinnerType winnerType);
    void PlayNis();
    void PlaySfx(const char* sfx);
    void PlaySfxWithVol(const char* sfx, float volume);
    static void ResetNisPlayer();
    void SaveGoalAsHighlight();
    static void SetTrophyVisible(bool visible);
    static void StopAllStreams();
    static void StopJumbotron();
    static void UnloadJumbotron();
    static void PlayJumbotron();
    void Wipe(const char* wipe);
    void WaitForAutoReplayCompletion(const char* wipe);
    void WaitForCharacterDirection();
    void WaitForNisCompletion(const char* wipe);

    /* 0x24 */ char mCurrentFunction[64];
    /* 0x64 */ bool mByPassWasSkipped;
    /* 0x65 */ bool mSkipPressed;
    /* 0x66 */ bool mInsideByPass;
    /* 0x67 */ bool mByPassing;
    /* 0x68 */ float mWaitingForCharacterDirectionSince;
    /* 0x6C */ float mTimeInFunction;
    /* 0x70 */ bool mTrophyTextureLoaded;
    /* 0x74 */ float mDisplayLetterBox;
    /* 0x78 */ float mLetterBoxDuration;
    /* 0x7C */ bool mLetterBoxEnabled;
    /* 0x80 */ float mOverlayDelay;
    /* 0x84 */ float mOverlayDisplayLength;
    /* 0x88 */ bool mOverlayDisplayed;
    /* 0x8C */ SceneList mOverlayToDisplay;
    /* 0x90 */ const char* mInterruptWipe;
    /* 0x94 */ const char* mUseInterruptWipe;
    /* 0x98 */ const char* mQueuedFunction;
    /* 0x9C */ bool mIsAllowedToSkip[4];
    /* 0xA0 */ HighlightQuality mGoalQuality;
}; // total size: 0xA4

#endif // _PRESENTATION_H_
