#ifndef _WIPER_H_
#define _WIPER_H_

#include "Game/Effects/EmissionController.h"
#include "Game/Transitions/ScreenTransitionManager.h"

struct WiperCallback : public ScreenTransitionCallback
{
    WiperCallback()
    {
        mTransitionActive = false;
    }
    virtual inline void TransitionFinished();
    virtual inline void TransitionProgressed(float fDeltaT);

    /* 0x04 */ bool mTransitionActive;
}; // total size: 0x8

class Wiper
{
public:
    Wiper()
    {
        unsigned long fileSize;
        char* loadedData;

        if (ScreenTransitionManager::s_pInstance == NULL)
        {
            ScreenTransitionManager::s_pInstance = new (nlMalloc(sizeof(ScreenTransitionManager), 8, 0)) ScreenTransitionManager();
        }
        glLoadTextureBundle("transitions/transitions.glt");
        fileSize = 0;
        loadedData = (char*)fxLoadEntireFileHigh("art/transitions/transitions.fx", &fileSize);
        ScreenTransitionManager::s_pInstance->AddTransitions(loadedData, fileSize);
        nlFree(loadedData);
    }

    void Reset();
    void Initialize();
    bool WipeInProgress() const;
    bool CutHasOccured() const;
    static Wiper& Instance();
    void DoWipe(const char* wipe);
    void Render(float dt);
};

#endif // _WIPER_H_
