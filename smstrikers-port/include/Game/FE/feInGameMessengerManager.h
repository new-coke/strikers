#ifndef _FEINGAMEMESSENGERMANAGER_H_
#define _FEINGAMEMESSENGERMANAGER_H_

#include "types.h"
#include "NL/nlList.h"
#include "NL/nlBasicString.h"

class FEIMessenger;

class FEInGameMessengerManager
{
public:
    enum eTimeStates
    {
        TS_INVALID = -1,
        TS_GAME_BEGINNING = 0,
        TS_GAME_EARLYMID = 1,
        TS_GAME_MIDLATE = 2,
        TS_GAME_LATE = 3,
        TS_NUMTIMESTATES = 4,
    };

    enum eInGameMessages
    {
        MSG_INVALID = -1,
        MSG_LIVESCOREUPDATE1 = 0,
        MSG_LIVESCOREUPDATE2 = 1,
        MSG_FINALSCOREUPDATE1 = 2,
        MSG_FINALSCOREUPDATE2 = 3,
        MSG_RANKCHANGE1 = 4,
        MSG_RANKCHANGE2 = 5,
        MSG_RANKCHANGE3 = 6,
        MSG_RANKCHANGE4 = 7,
        MSG_CUSTOMTOURNNEXTMATCHUP = 8,
        MSG_NUMMESSAGES = 9,
    };

    int MessageLength(FEInGameMessengerManager::eInGameMessages msg) const { return m_messageList[msg].size(); }
    void ShowMessage(FEInGameMessengerManager::eInGameMessages msg);
    void Update(float fDeltaT);
    void EnterNewTimeState(FEInGameMessengerManager::eTimeStates timeState);
    ~FEInGameMessengerManager();

    static const float TIMESTATE_TIMES[4];

    /* 0x00 */ FEIMessenger* m_messenger;
    /* 0x04 */ eTimeStates m_curTimeState;
    /* 0x08 */ int m_numWatchGames;
    /* 0x0C */ BasicString<unsigned short, Detail::TempStringAllocator> m_messageList[9];
    /* 0x30 */ nlListContainer<eInGameMessages> m_messageQueue;
    /* 0x3C */ float m_openFor;
    /* 0x40 */ u8 m_isOpen;
    /* 0x41 */ u8 m_waitingToDisplay;
    /* 0x42 */ u8 _pad42[2];
    /* 0x44 */ float m_waitedToDisplay;
};

// class ListContainerBase<FEInGameMessengerManager
// {
// public:
//     void eInGameMessages,
// };

// class nlWalkList<ListEntry<FEInGameMessengerManager
// {
// public:
//     void eInGameMessages>, ListContainerBase<FEInGameMessengerManager::eInGameMessages,
//     NewAdapter<ListEntry<FEInGameMessengerManager::eInGameMessages>>>>(ListEntry<FEInGameMessengerManager::eInGameMessages>*,
//     ListContainerBase<FEInGameMessengerManager::eInGameMessages, NewAdapter<ListEntry<FEInGameMessengerManager::eInGameMessages>>>*, void
//     (ListContainerBase<FEInGameMessengerManager::eInGameMessages,
// };

// class nlListRemoveStart<ListEntry<FEInGameMessengerManager
// {
// public:
//     void eInGameMessages>>(ListEntry<FEInGameMessengerManager::eInGameMessages>**,
// };

// class nlListAddEnd<ListEntry<FEInGameMessengerManager
// {
// public:
//     void eInGameMessages>>(ListEntry<FEInGameMessengerManager::eInGameMessages>**,
// };

#endif // _FEINGAMEMESSENGERMANAGER_H_
