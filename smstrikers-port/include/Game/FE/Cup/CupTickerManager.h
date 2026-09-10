#ifndef _CUPTICKERMANAGER_H_
#define _CUPTICKERMANAGER_H_

#include "Game/FE/tlTextInstance.h"
#include "NL/nlBasicString.h"

class FEScrollText;

enum eCupTickerState
{
    CUP_TICKER_STATE_0 = 0
};

class CupTickerManager
{
public:
    CupTickerManager();
    ~CupTickerManager();
    void SetTickerTextInstance(TLTextInstance* tickerText);
    void CreateNewMessage();
    void Update(float dt);
    void BuildGoalTotalTickerMessage(BasicString<unsigned short, Detail::TempStringAllocator>& result, bool bIsHuman);

    /* 0x000 */ FEScrollText* mTicker;
    /* 0x004 */ eCupTickerState mState;
    /* 0x008 */ unsigned short mMessageBuffer[512];
}; // total size: 0x408

#endif // _CUPTICKERMANAGER_H_
