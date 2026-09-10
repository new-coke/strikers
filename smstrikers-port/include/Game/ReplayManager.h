#ifndef _REPLAYMANAGER_H_
#define _REPLAYMANAGER_H_

#include "Game/Sys/eventman.h"
#include "Game/Camera/FollowCam.h"
#include "Game/RenderSnapshot.h"
#include "Game/Replay.h"

class ReplayManager
{
    ReplayManager();
    void SwapPreviousAndCurrent();
    void DoPotentialDebugReplay(float& deltaTime);
    void DoPotentialAutoReplay(float deltaTime);

public:
    ~ReplayManager();
    static ReplayManager* Instance();
    void Initialize();
    void InitializeSnapshots();
    void Uninitialize();
    void GrabSnapshot();
    RenderSnapshot& GetMutableRenderSnapshot();
    void Flush();
    void ResetSnapshots();
    void PrepareForRecording();
    void SetCurrentTime(float time);
    void EventHandler(Event* event);
    static void EventHandler(Event* event, void* userData) { ((ReplayManager*)userData)->EventHandler(event); }
    void RenderSnapshotAt(float deltaTime);

    /* 0x0000 */ RenderSnapshot mSnapshots[3]; // size: 0x5040
    /* 0x5040 */ RenderSnapshot* mCurrent;
    /* 0x5044 */ RenderSnapshot* mPrevious;
    /* 0x5048 */ RenderSnapshot* mRender;
    /* 0x504C */ cFollowCamera mDebugCamera;
    /* 0x50EC */ u32 mEvents;
    /* 0x50F0 */ f32 mSpeed;
    /* 0x50F4 */ f32 mSpeedUp;
    /* 0x50F8 */ f32 mDeltaTime;
    /* 0x50FC */ f32 mTime;
    /* 0x5100 */ f32 mBlend[3];
    /* 0x510C */ Replay* mReplay;
    /* 0x5110 */ u8* mMemory;
}; // total size: 0x5114

#endif // _REPLAYMANAGER_H_
