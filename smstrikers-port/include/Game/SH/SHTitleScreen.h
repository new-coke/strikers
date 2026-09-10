#ifndef _SHTITLESCREEN_H_
#define _SHTITLESCREEN_H_

#include "Game/BaseSceneHandler.h"
#include "Game/FE/tlTextInstance.h"
#include "NL/nlConfig.h"

void DoNothingCallback();
void StartMovieCB();

class TitleScene : public BaseSceneHandler
{
public:
    TitleScene();
    virtual ~TitleScene();
    virtual void Update(float dt);
    virtual void SceneCreated();

    static void StartIntroMovie();

    /* 0x1C */ float m_fTimeElapsed;
    /* 0x20 */ TLTextInstance* mTextPressStart;
    /* 0x24 */ bool mStartedDemo;
    /* 0x25 */ bool mStartedMovie;
}; // total size: 0x28

#endif // _SHTITLESCREEN_H_
