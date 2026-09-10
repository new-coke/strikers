#ifndef _OVERLAYHANDLERGOAL_H_
#define _OVERLAYHANDLERGOAL_H_

#include "Game/FE/BaseOverlayHandler.h"
#include "Game/Sys/eventman.h"

class GoalOverlay : public BaseOverlayHandler
{
public:
    GoalOverlay();
    virtual ~GoalOverlay();
    virtual void SceneCreated();
    virtual void Update(float fDeltaT);
    void CreateEventHandler();
    void DestroyEventHandler();
    static void eventHandler(Event* event, void* param);
    void Reset();
    void UpdateGoalInfo(int homeAway, int playerIndex, bool isCaptainS2S, int numGoals);
    void SetHighlightNumber(int highlight);
    void DoMatchEndOverlay();
    void SetWinnerTitle();
    void DoCupWinOverlay();
    void Restart();
    int mCaptainGoals[2];        // offset 0x28, size 0x8
    int mSidekickGoals[2];       // offset 0x30, size 0x8
    EventHandler* mEventHandler; // offset 0x38, size 0x4
    u16 mClockBuffer[32];        // offset 0x3C, size 0x40
    u16 mDescriptionBuffer[128]; // offset 0x7C, size 0x100
    u16 mScoresBuffer[128];      // offset 0x17C, size 0x100
    bool mHasSniperCup;          // offset 0x27C, size 0x1
    bool mIsCreated;             // offset 0x27D, size 0x1
    bool mIsInOvertime;          // offset 0x27E, size 0x1
};

#endif // _OVERLAYHANDLERGOAL_H_
