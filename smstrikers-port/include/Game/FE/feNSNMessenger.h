#ifndef _FENSNMESSENGER_H_
#define _FENSNMESSENGER_H_

#include "NL/nlFunction.h"

#include "Game/FE/BaseOverlayHandler.h"
#include "Game/FE/feScrollText.h"
#include "Game/FE/feIMessenger.h"

class NSNMessengerScene : public FEIMessenger, public BaseOverlayHandler
{
public:
    enum eMessengerState
    {
        MS_INVALID = -1,
        MS_OPENING = 0,
        MS_OPEN = 1,
        MS_CLOSING = 2,
        MS_CLOSED = 3,
        MS_NUMSTATES = 4,
    };

    NSNMessengerScene();
    ~NSNMessengerScene();

    virtual void OpenMessenger();
    virtual void OpenMessengerNow();
    virtual void SetDisplayMessage(const BasicString<unsigned short, Detail::TempStringAllocator>& theMessage);
    virtual void SetMessageFinishedCB(const Function<FnVoidVoid>& cb)
    {
        m_messageFinishedCB = cb;
    }
    virtual void CloseMessenger();
    virtual void CloseMessengerNow();
    virtual bool IsMessengerOpen() const;

    virtual void SceneCreated();
    virtual void Update(float fDeltaT);

    void EnableScrolling(bool state);
    void SetDisplayMessage(const char* locMessage);
    void ForceMessengerVisibleNow();

    /* 0x02C */ Function<FnVoidVoid> m_messageFinishedCB;
    /* 0x034 */ eMessengerState m_curState;
    /* 0x038 */ float m_messageDisplayTime;
    /* 0x03C */ bool m_messageDisplaying;
    /* 0x03E */ unsigned short m_displayMessage[255];
    /* 0x23C */ FEScrollText* m_scrollText;
}; // total size: 0x240

// class BaseSceneHandler
// {
// public:
// };

// class FEFinder<TLTextInstance, 3>
// {
// public:
//     void _Find<FEPresentation>(FEPresentation*, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned
// };

#endif // _FENSNMESSENGER_H_
