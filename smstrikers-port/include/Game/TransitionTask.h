#ifndef _TRANSITIONTASK_H_
#define _TRANSITIONTASK_H_

#include "NL/nlTask.h"
#include "NL/nlMemory.h"
#include "Game/Loader.h"
#include "Game/FE/feManager.h"

enum TRANSITION_STATE
{
    eTS_Unknown = 0,
    eTS_InState = 1,
    eTS_Initializing = 2,
    eTS_Destroying = 3,
};

class LoadingManager;
class EventHandler;

class TransitionTask : public nlTask
{
public:
    TransitionTask();

    virtual const char* GetName() { return "Transition Manager"; };
    virtual void Run(float) { };

    void DestroyFEFast();

    void InitializeFEFast();

    static void DisplayFirstScreen();

    static void CompactSlotPools();

    void DestroyFEState();

    void Initialize(LoadingManager& loadingManager);
    void StateTransition(unsigned int from, unsigned int to);
    void InitializeGameState();
    void DestroyGameState();
    void InitializeFEState();

    /* 0x18 */ unsigned long long m_GameResourceMark;
    /* 0x20 */ EventHandler* m_pAIHandler;
    /* 0x24 */ EventHandler* m_pGoalieHandler;
    /* 0x28 */ LoadingManager* m_pLoadingManager;
    /* 0x2C */ TRANSITION_STATE m_TransitionState;

    static TransitionTask* sm_pGlobalTask;
}; // total size: 0x30

extern bool g_bFrameStatsOnDisk;

#endif // _TRANSITIONTASK_H_
