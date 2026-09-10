#ifndef _SHPAUSEPOSTGAME_H_
#define _SHPAUSEPOSTGAME_H_

#include "Game/BaseSceneHandler.h"
#include "Game/FE/feButtonComponent.h"
#include "Game/FE/feMenu.h"
#include "Game/FE/tlComponentInstance.h"
#include "Game/FE/tlTextInstance.h"
#include "NL/nlBasicString.h"

class PausePostGameScene : public BaseSceneHandler
{
public:
    PausePostGameScene();
    virtual ~PausePostGameScene();
    virtual void SceneCreated();
    virtual void Update(float dt);

    void OnSelectRematch();
    void OnSelectQuit();
    void OnSelectChangeTeams();
    void SetText(TLTextInstance& textinstance, const BasicString<unsigned short, Detail::TempStringAllocator>& string);

    /* 0x01C */ MenuList<TLComponentInstance> mMenuItems; // size 0x214
    /* 0x230 */ unsigned short mScoreBuffer[2][8];        // size 0x20
    /* 0x250 */ unsigned short mRematchTextBuffer[128];   // size 0x100
    /* 0x350 */ ButtonComponent mButtons;                 // size 0x24
}; // total size: 0x374

// class FEFinder<TLTextInstance, 3>
// {
// public:
// };

// class FEFinder<TLComponentInstance, 4>
// {
// public:
// };

// class Function1<void, TLComponentInstance*>
// {
// public:
//     void FunctorImpl<BindExp1<void, Detail::MemFunImpl<void, void (PausePostGameScene::*)()>,
//     PausePostGameScene*>>::operator()(TLComponentInstance*); void FunctorImpl<BindExp1<void, Detail::MemFunImpl<void, void
//     (PausePostGameScene::*)()>, PausePostGameScene*>>::Clone() const;
// };

// class MemFun<PausePostGameScene, void>(void (PausePostGameScene
// {
// public:
// };

// class Bind<void, Detail
// {
// public:
//     void MemFunImpl<void, void (PausePostGameScene::*)()>, PausePostGameScene*>(Detail::MemFunImpl<void, void (PausePostGameScene::*)()>,
// };

// class Format<BasicString<unsigned short, Detail
// {
// public:
//     void TempStringAllocator>, unsigned short[8]>(const BasicString<unsigned short, Detail::TempStringAllocator>&, const unsigned
// };

// class FormatImpl<BasicString<unsigned short, Detail
// {
// public:
// };

#endif // _SHPAUSEPOSTGAME_H_
