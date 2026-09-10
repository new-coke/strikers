#ifndef _OVERLAYHANDLERWINNER_H_
#define _OVERLAYHANDLERWINNER_H_

#include "Game/FE/BaseOverlayHandler.h"
#include "Game/FE/feAsyncImage.h"
#include "Game/Team.h"

class WinnerOverlay : public BaseOverlayHandler
{
public:
    WinnerOverlay();
    ~WinnerOverlay();
    void SceneCreated();
    void Update(float fDeltaT);

    /* 0x28 */ unsigned short mScoresBuffer[32];
    /* 0x68 */ unsigned short mWinnerBuffer[32];
    /* 0xA8 */ float mInputDelay;
    /* 0xAC */ bool mDoingOutTransition;
    /* 0xB0 */ AsyncImage* mWinnerActionWhite;
    /* 0xB4 */ AsyncImage* mWinnerAction;
    /* 0xB8 */ AsyncImage* mWinnerActionOutline;
    /* 0xBC */ eTeamID mWinningTeam;
}; // total size: 0xC0

// class FEFinder<TLImageInstance, 2>
// {
// public:
// };

// class FEFinder<TLComponentInstance, 4>
// {
// public:
// };

// class FEFinder<TLTextInstance, 3>
// {
// public:
// };

#endif // _OVERLAYHANDLERWINNER_H_
