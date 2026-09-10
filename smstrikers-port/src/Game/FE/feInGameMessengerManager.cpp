#include "Game/FE/feInGameMessengerManager.h"
#include "Game/FE/feIMessenger.h"
#include "Game/Game.h"
#include "Game/Ball.h"
#include "Game/GameInfo.h"

const float FEInGameMessengerManager::TIMESTATE_TIMES[4] = { 0.0f, 0.3f, 0.6f, 0.85f };

/**
 * Offset/Address/Size: 0x554 | 0x800FFE70 | size: 0x104
 */
FEInGameMessengerManager::~FEInGameMessengerManager()
{
    Function<FnVoidVoid> cb;
    m_messenger->SetMessageFinishedCB(cb);
}
/**
 * Offset/Address/Size: 0x140 | 0x800FFA5C | size: 0x414
 */
void FEInGameMessengerManager::EnterNewTimeState(FEInGameMessengerManager::eTimeStates timeState)
{
    switch (timeState)
    {
    case TS_GAME_BEGINNING:
        break;

    case TS_GAME_EARLYMID:
        for (int i = 0; i < m_numWatchGames; i++)
        {
            ShowMessage((eInGameMessages)i);
        }
        break;

    case TS_GAME_MIDLATE:
        for (int i = 0; i < m_numWatchGames; i++)
        {
            ShowMessage((eInGameMessages)(i + 2));
        }
        break;

    case TS_GAME_LATE:
    {
        int sequence[4] = { 0, 1, 2, 3 };

        for (int i = 0; i < 4; i++)
        {
            int swapInd = i + nlRandom(4 - i, &nlDefaultSeed);
            int temp = sequence[i];
            sequence[i] = sequence[swapInd];
            sequence[swapInd] = temp;
        }

        int numDisplayed = 0;
        for (int i = 0; i < 4; i++)
        {
            if (numDisplayed >= 2)
            {
                break;
            }

            eInGameMessages msg = (eInGameMessages)(i + 4);

            if (MessageLength(msg) != 0)
            {
                ShowMessage(msg);
                ShowMessage(msg);
                numDisplayed++;
            }
        }

        if (nlSingleton<GameInfoManager>::Instance()->mCurrentMode == GameInfoManager::GM_TOURNAMENT)
        {
            ShowMessage(MSG_CUSTOMTOURNNEXTMATCHUP);
        }
        break;
    }

    default:
        break;
    }

    m_curTimeState = timeState;
}
/**
 * Offset/Address/Size: 0x0 | 0x800FF91C | size: 0x140
 */
void FEInGameMessengerManager::Update(float fDeltaT)
{
    if (m_waitingToDisplay)
    {
        m_waitedToDisplay += fDeltaT;
    }

    float normTime = g_pGame->GetNormalizedGameTime();
    eTimeStates newState = m_curTimeState;

    while (newState + 1 != TS_NUMTIMESTATES)
    {
        if (normTime >= TIMESTATE_TIMES[newState + 1])
        {
            newState = (eTimeStates)(newState + 1);
        }
        else
        {
            break;
        }
    }

    if (newState != m_curTimeState)
    {
        EnterNewTimeState(newState);
    }

    if (m_messageQueue.m_Head == NULL)
    {
        return;
    }

    if (m_messenger->IsMessengerOpen())
    {
        return;
    }

    if (!(m_waitedToDisplay > 25.f))
    {
        if (g_pBall->GetOwnerGoalie() == NULL)
        {
            return;
        }
    }

    ListEntry<eInGameMessages>* entry = nlListRemoveStart<ListEntry<eInGameMessages> >(&m_messageQueue.m_Head, &m_messageQueue.m_Tail);
    eInGameMessages msg;
    eInGameMessages* pMsg = &msg;
    if (pMsg != NULL)
    {
        msg = entry->entry;
    }
    delete entry;

    m_messenger->SetDisplayMessage(m_messageList[(int)msg]);
    m_messenger->OpenMessenger();
    m_waitingToDisplay = false;
}
/**
 * Every call is inlined into EnterNewTimeState, so the out-of-line copy is
 * unreferenced and dead-stripped by mwld; it survives in the DOL only as the
 * .sdata2 pool block it interns ahead of Update. DWARF puts its codegen slot
 * here, between the destructor and Update.
 */
void FEInGameMessengerManager::ShowMessage(FEInGameMessengerManager::eInGameMessages msg)
{
    if (MessageLength(msg) != 0)
    {
        if (m_messageQueue.m_Head == NULL)
        {
            m_waitingToDisplay = true;
            m_waitedToDisplay = 0.0f;
        }

        m_messageQueue.AddEnd(msg);
    }
}
