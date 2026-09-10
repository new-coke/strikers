#ifndef _REPLAYCHOREO_H_
#define _REPLAYCHOREO_H_

#include "types.h"
#include "Game/InterpreterCore.h"
#include "Game/Replay.h"
#include "Game/ReplayManager.h"
#include "Game/Camera/ReplayCamera.h"
#include "Game/Goalie.h"
#include "Game/Sys/eventman.h"
#include "NL/nlBasicString.h"

enum ReplayCameraFocus
{
    REPLAY_CAMERA_FOCUS_BALL = 1,
    REPLAY_CAMERA_FOCUS_CURRENT_OR_LAST_OWNER = 2,
    REPLAY_CAMERA_FOCUS_NET = 4,
    REPLAY_CAMERA_FOCUS_GOALIE = 8,
    REPLAY_CAMERA_FOCUS_NUM = 4,
};

enum ReplayEvent
{
    REPLAY_EVENT_BALL_IN_NET = 1,
    REPLAY_EVENT_SHOT_AT_GOAL = 2,
    REPLAY_EVENT_RECEIVE_BALL = 4,
    REPLAY_EVENT_PASS_BALL = 8,
    REPLAY_EVENT_GOALIE_SAVE = 22,
};

class ReplayChoreo : public InterpreterCore
{
public:
    enum HighlightQuality
    {
        HIGHLIGHT_QUALITY_EMPTY = 0,
        HIGHLIGHT_QUALITY_SAVE = 1,
        HIGHLIGHT_QUALITY_GOAL_DECREASE_DIFF = 2,
        HIGHLIGHT_QUALITY_GOAL_EQUALIZER = 3,
        HIGHLIGHT_QUALITY_GOAL_INCREASE_DIFF = 4,
        NUM_QUALITY_LEVELS = 5,
    };

    struct Highlight
    {
        /**
         * Offset/Address/Size: 0x0 | 0x80128730 | size: 0x34
         */
        Highlight()
            : mSideOfInterest(0)
            , mTime(0.0f)
            , mReplayPad(-1)
        {
        }

        int mSideOfInterest;            // offset 0x0, size 0x4
        float mTime;                    // offset 0x4, size 0x4
        int mReplayPad;                 // offset 0x8, size 0x4
        GoalScoredData mGoalScoredData; // offset 0xC, size 0x24
        int mSavedReplayPad;            // offset 0x30, size 0x4
    }; // total size: 0x34

    ReplayChoreo();

    void DoFunctionCall(unsigned int);
    static ReplayChoreo& Instance();
    void LoadScript();
    void EventHandler(Event* event);
    static void EventHandler(Event* event, void* userData) { ((ReplayChoreo*)userData)->EventHandler(event); }
    void Reset();
    BasicString<char, Detail::TempStringAllocator> CalcAutoReplayScriptName(ReplayType rt) const;
    void StartAutoReplay(ReplayType rt);
    void FlushHighlights();
    void Update(float deltaT);
    void SetCamera(ReplayCameraPosition position);
    void FreezeCamera();
    void SetCameraFov(float fov);
    void SetCameraFocus(ReplayCameraFocus focus);
    void AddCameraFocus(ReplayCameraFocus focus);
    void Speed(float speed);
    void StartSpeedUp(float deltaSpeed);
    void Rewind(ReplayEvent event, float timeOffset);
    void RunTill(ReplayEvent event, float timeOffset);
    bool Done() const;
    void StartCameraZoom(float deltaFov);
    void RunFor(float time);
    void SaveHighlight(ReplayChoreo::HighlightQuality quality);
    void PlayWorldSfx(const char* name, const char* target);
    void PlayWorldSfxWithVol(const char* name, float volume, const char* target);
    void StopWorldSfx(const char* name);
    void FilterOn(float fadeTime);
    void FilterOff(float fadeTime);
    int NumHighlights() const;
    void Finish();

    int mNumScripts[3][3][9];               // offset 0x24, size 0x144
    mutable ReplayManager* mReplayManager;  // offset 0x168, size 0x4
    mutable Replay* mReplay;                // offset 0x16C, size 0x4
    ReplayCamera mCamera;                   // offset 0x170, size 0x8C
    float mRunForTimeLeft;                  // offset 0x1FC, size 0x4
    bool mRunningFor;                       // offset 0x200, size 0x1
    void* mByteCode;                        // offset 0x204, size 0x4
    mutable GoalScoredData mGoalScoredData; // offset 0x208, size 0x24
    int mReplayPad;                         // offset 0x22C, size 0x4
    Highlight mHighlights[3];               // offset 0x230, size 0x9C
    int mHighlightIndex;                    // offset 0x2CC, size 0x4
    int mNumHighlights;                     // offset 0x2D0, size 0x4
}; // total size: 0x2D4

#endif // _REPLAYCHOREO_H_
