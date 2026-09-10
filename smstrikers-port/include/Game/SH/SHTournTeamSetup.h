#ifndef _SHTOURNTEAMSETUP_H_
#define _SHTOURNTEAMSETUP_H_

#include "types.h"
#include "NL/nlBasicString.h"
#include "Game/BaseSceneHandler.h"
#include "Game/DB/Cup.h"
#include "Game/DB/CustomTournament.h"
#include "Game/FE/feAsyncImage.h"
#include "Game/FE/feButtonComponent.h"
#include "Game/FE/feCaptainGridComponent.h"
#include "Game/FE/feMenu.h"
#include "Game/FE/feScrollText.h"
#include "Game/FE/feSidekickGridComponent.h"
#include "Game/FE/tlComponentInstance.h"

struct TeamData
{
    /* 0x0 */ bool isEmpty;
    /* 0x4 */ eTeamID captain;
    /* 0x8 */ eSidekickID sidekick;
    /* 0xC */ bool isHumanPlayer;
}; // total size: 0x10

class TournTeamSetupSceneV2 : public BaseSceneHandler
{
public:
    enum eTeamChooserState
    {
        STATE_INVALID = -1,
        STATE_IN = 0,
        STATE_SCROLLING = 1,
        STATE_CAPTAIN = 2,
        STATE_SIDEKICK = 3,
        NUM_STATES = 4,
    };

    TournTeamSetupSceneV2();
    virtual ~TournTeamSetupSceneV2();
    virtual void SceneCreated();
    virtual void Update(float fDeltaT);

    void UpdateControllerIcon(int onScreenRow);
    void UpdateRow(int onScreenRow);
    void ChangeState(TournTeamSetupSceneV2::eTeamChooserState from, TournTeamSetupSceneV2::eTeamChooserState to);
    void StartChooseCaptain(int onScreenRow);
    void UpdateCaptainName();
    void UpdateSKName();
    void SetTeam();
    void UpdateAllRows();
    int CanProceed() const;
    void Proceed();
    void CreateLeagueLineup();
    void CreateKnockout();
    BasicString<char, Detail::TempStringAllocator> FindCaptainSlideName(eTeamID captain);
    BasicString<char, Detail::TempStringAllocator> FindSidekickSlideName(eSidekickID sidekick);
    bool CanAutoFill() const;
    void AutoFill();
    unsigned char IsAlreadySelected(eTeamID captain) const;
    void UpdateForCurrentRow();
    void UpdateArrowVisibility();
    void ScrollUp(bool bPlaySound);
    void ScrollDown(bool bPlaySound);
    void AutoTagCurrentRowAsHumanPlayer();
    void SetupButtonsBasedOnState(eTeamChooserState state);

    /* 0x01C */ MenuList<TLComponentInstance> mMenuItems;  // size 0x214
    /* 0x230 */ TeamData mTeamData[8];                     // size 0x80
    /* 0x2B0 */ int mRowOffset;                            // size 0x4
    /* 0x2B4 */ int mCurrentRow;                           // size 0x4
    /* 0x2B8 */ CustomTournament& mTournInfo;              // size 0x4
    /* 0x2BC */ eTeamChooserState mCurrentState;           // size 0x4
    /* 0x2C0 */ ICaptainGridComponent* mCaptainGrid;       // size 0x4
    /* 0x2C4 */ ISidekickGridComponent* mSKGrid;           // size 0x4
    /* 0x2C8 */ TLComponentInstance* mComponents[3];       // size 0xC
    /* 0x2D4 */ TLComponentInstance* mPressStartComponent; // size 0x4
    /* 0x2D8 */ eTeamID mCurrentCaptain;                   // size 0x4
    /* 0x2DC */ eSidekickID mCurrentSK;                    // size 0x4
    /* 0x2E0 */ FEScrollText* mTicker;                     // size 0x4
    /* 0x2E4 */ ButtonComponent mButtons1;                 // size 0x24
    /* 0x308 */ ButtonComponent mButtons2;                 // size 0x24
    /* 0x32C */ TLImageInstance* mUpArrow;                 // size 0x4
    /* 0x330 */ TLImageInstance* mDownArrow;               // size 0x4
    /* 0x334 */ bool mHasTaggedHumanPlayer;                // size 0x1
}; // total size: 0x338

#endif // _SHTOURNTEAMSETUP_H_
