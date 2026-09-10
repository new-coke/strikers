#ifndef _SHCUPCHEATER_H_
#define _SHCUPCHEATER_H_

#include "Game/BaseSceneHandler.h"
#include "Game/FE/feSlideMenu.h"
#include "Game/GameSceneManager.h"

class CupCheaterScene : public BaseSceneHandler
{
public:
    CupCheaterScene();
    ~CupCheaterScene();
    void SceneCreated();
    void Update(float dt);
    void OnSelectGameplay();
    void OnSelectHomeWin();
    void OnSelectAwayWin();
    void OnSelectHomeOTWin();
    void OnSelectAwayOTWin();
    void AddMilestoneStats();
    void UpdateSlides();

    /* 0x1C */ FESlideMenu* m_SlideMenu;            // size 0x4
    /* 0x20 */ int mSniper;                         // size 0x4
    /* 0x24 */ int mStriker;                        // size 0x4
    /* 0x28 */ int mTactician;                      // size 0x4
    /* 0x2C */ int mParamedic;                      // size 0x4
    /* 0x30 */ int mVeteran;                        // size 0x4
    /* 0x34 */ unsigned short mSniperBuffer[10];    // size 0x14
    /* 0x48 */ unsigned short mStrikerBuffer[10];   // size 0x14
    /* 0x5C */ unsigned short mTacticianBuffer[10]; // size 0x14
    /* 0x70 */ unsigned short mParamedicBuffer[10]; // size 0x14
    /* 0x84 */ unsigned short mVeteranBuffer[10];   // size 0x14
}; // total size: 0x98

// class FEFinder<TLTextInstance, 3>
// {
// public:
// };

// class FEFinder<TLComponentInstance, 4>
// {
// public:
//     void _Find<FEPresentation>(FEPresentation*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned
//     long); void Find<FEPresentation>(FEPresentation*, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher,
// };

// class Function0<void>
// {
// public:
// };

// class MemFun<CupCheaterScene, void>(void (CupCheaterScene
// {
// public:
// };

// class Bind<void, Detail
// {
// public:
//     void MemFunImpl<void, void (CupCheaterScene::*)()>, CupCheaterScene*>(Detail::MemFunImpl<void, void (CupCheaterScene::*)()>,
// };

#endif // _SHCUPCHEATER_H_
