#ifndef _SHLESSONSELECT_H_
#define _SHLESSONSELECT_H_

#include "Game/BaseSceneHandler.h"
#include "Game/FE/feMenu.h"
#include "Game/FE/tlComponentInstance.h"
#include "Game/FE/feButtonComponent.h"
#include "Game/FE/feAsyncImage.h"

void SetTickerLesson(int lesson);
void LessonTickerDoneCB();

class LessonSelectScene : public BaseSceneHandler
{
public:
    LessonSelectScene();
    virtual ~LessonSelectScene();
    virtual void SceneCreated();
    virtual void Update(float fDeltaT);

    void UpdateRow(int onScreenRow, bool playsound);
    void StartLesson();

    /* 0x01C */ MenuList<TLComponentInstance> mMenuItems; // size 0x214
    /* 0x230 */ bool mDoSlideIn;                          // size 0x1
    /* 0x232 */ unsigned short mNumberBuffers[4][16];     // size 0x80
    /* 0x2B2 */ bool mStartAnimAtEnd;                     // size 0x1
    /* 0x2B4 */ ButtonComponent mButtons;                 // size 0x24
    /* 0x2D8 */ TLImageInstance* mUpArrow;                // size 0x4
    /* 0x2DC */ TLImageInstance* mDownArrow;              // size 0x4
}; // total size: 0x2E0

// class Function1<void, TLComponentInstance*>
// {
// public:
//     void FunctorImpl<BindExp1<void, Detail::MemFunImpl<void, void (LessonSelectScene::*)()>,
//     LessonSelectScene*>>::operator()(TLComponentInstance*); void FunctorImpl<BindExp1<void, Detail::MemFunImpl<void, void
//     (LessonSelectScene::*)()>, LessonSelectScene*>>::Clone() const;
// };

// class FEFinder<TLTextInstance, 3>
// {
// public:
// };

// class FEFinder<TLComponentInstance, 4>
// {
// public:
// };

// class FEFinder<TLImageInstance, 2>
// {
// public:
// };

// class MemFun<LessonSelectScene, void>(void (LessonSelectScene
// {
// public:
// };

// class Bind<void, Detail
// {
// public:
//     void MemFunImpl<void, void (LessonSelectScene::*)()>, LessonSelectScene*>(Detail::MemFunImpl<void, void (LessonSelectScene::*)()>,
// };

#endif // _SHLESSONSELECT_H_
