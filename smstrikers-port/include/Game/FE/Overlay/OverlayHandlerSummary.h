#ifndef _OVERLAYHANDLERSUMMARY_H_
#define _OVERLAYHANDLERSUMMARY_H_

#include "Game/FE/BaseOverlayHandler.h"
#include "Game/FE/feInput.h"
#include "Game/FE/feButtonComponent.h"

#include "Game/DB/StatsTracker.h"

class FESlideMenu;

enum eSummaryType
{
    SUMMARY_INVALID = -1,
    SUMMARY_MATCH = 0,
    SUMMARY_CUMULATIVE_MATCH = 1,
    SUMMARY_USER = 2,
    SUMMARY_CUMULATIVE_USER = 3,
    NUM_SUMMARIES = 4,
};

class SummaryOverlay : public BaseOverlayHandler
{
public:
    enum eSummaryContext
    {
        ENDGAME = 0,
        PAUSE = 1,
    };

    SummaryOverlay(SummaryOverlay::eSummaryContext context);
    ~SummaryOverlay();
    void SceneCreated();
    void Update(float fDeltaT);
    void DisplayMatchSummary(eSummaryType matchSummaryType);
    void DisplayUserSummary(eSummaryType matchSummaryType);

    /* 0x028 */ eSummaryContext mContext;
    /* 0x02C */ eFEINPUT_PAD m_controllingInput;
    /* 0x030 */ PlayerStats mCurrentPlayerStats[2];
    /* 0x098 */ PlayerStats mCumulativePlayerStats[2];
    /* 0x100 */ PlayerStats mCurrentUserStats[4];
    /* 0x1D0 */ PlayerStats mCumulativeUserStats[4];
    /* 0x2A0 */ unsigned short mBuffersColBySide[2][6][32];
    /* 0x5A0 */ unsigned short mBuffersColByUser[4][6][32];
    /* 0xBA0 */ unsigned short mTitleBuffer[64];
    /* 0xC20 */ bool mSummaryDisplayed;
    /* 0xC21 */ bool mShowCumulativeStats;
    /* 0xC22 */ bool mShowUserStats;
    /* 0xC24 */ FESlideMenu* mSlideMenu;
    /* 0xC28 */ eSummaryType summaryOrder[4];
    /* 0xC38 */ int mCurrentSummaryIndex;
    /* 0xC3C */ ButtonComponent::ButtonState mButtonState;
    /* 0xC40 */ ButtonComponent mButtons;
}; // total size: 0xC64

#endif // _OVERLAYHANDLERSUMMARY_H_
