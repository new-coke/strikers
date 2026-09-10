#ifndef _SHCUPHUB_H_
#define _SHCUPHUB_H_

#include "Game/BaseSceneHandler.h"
#include "Game/DB/StatsTracker.h"
#include "Game/FE/Cup/CupTickerManager.h"
#include "Game/FE/feAsyncImage.h"
#include "Game/FE/feButtonComponent.h"
#include "Game/FE/tlComponentInstance.h"
#include "NL/nlColour.h"

enum eHubColour
{
    HUB_COLOUR_INVALID = -1,
    HUB_COLOUR_RED = 0,
    HUB_COLOUR_GREEN = 1,
    HUB_COLOUR_BLUE = 2,
    HUB_COLOUR_YELLOW = 3,
    NUM_HUB_COLOURS = 4
};

enum eHubState
{
    HUB_INVALID = -1,
    HUB_LEAGUE = 0,
    HUB_BOWSER_TRANSITION = 1,
    HUB_KNOCKOUT2 = 2,
    HUB_KNOCKOUT4 = 3,
    HUB_KNOCKOUT8 = 4,
    NUM_HUB_STATES = 5,
};

class CupHubScene : public BaseSceneHandler
{
public:
    CupHubScene(bool doAnimations, bool playAllKnockoutAnimations);
    ~CupHubScene();
    void SceneCreated();
    void Update(float fDeltaT);
    void Animate();
    void MakeTextBoxReallyWide(TLTextInstance& textInstance);
    void EndCup();
    void ReturnToMainMenu();
    unsigned char UpdateDisplayedStat();
    void CreateLeague();
    void CreateBowserLeague();
    void CreateKnockout();
    unsigned char UpdateLeague(float fDeltaT);
    unsigned char UpdateKnockout8(float fDeltaT);
    unsigned char UpdateKnockout4(float fDeltaT);
    unsigned char UpdateKnockout2(float fDeltaT);
    void UpdateProgressIndicator();
    void ColourUserRow();
    static unsigned char IsUserRow(eTeamID teamInRow);
    void HandleButtonComponent();
    void SetRoundColours(eHubColour* coloursArray, int sizeOfArray);
    void UpdateRoundMessage(bool hideMessage);
    void LoadCaptainImage();

    /* 0x001C */ TeamStats mAllTeamStats[8];    // size 0x200
    /* 0x021C */ nlColour mTextColour;          // size 0x4
    /* 0x0220 */ bool mDoAnimations;            // size 0x1
    /* 0x0221 */ bool mUpdatingStats;           // size 0x1
    /* 0x0222 */ bool mKnockoutLoserAnimations; // size 0x1
    /* 0x0223 */ bool mAllKnockoutAnimations;   // size 0x1
    /* 0x0224 */ bool mSuperTeamAnimation;      // size 0x1
    /* 0x0225 */ bool mHasHumanTeamPlayed;      // size 0x1
    /* 0x0226 */ bool mDoAutoSave;              // size 0x1
    /* 0x0227 */ bool mPlayPopSound;            // size 0x1
    /* 0x0228 */ float mRowMovement[8];         // size 0x20
    /* 0x0248 */ unsigned short mColumnsByRowsBuffers[5][8][64]; // size 0x1400 (G4QJ01)
    /* 0x0C48 (G4QE01), 0x1648 (G4QJ01) */ unsigned short mProgressBuffer[128];     // size 0x100
    /* 0x0D48 (G4QE01), 0x1748 (G4QJ01) */ TLComponentInstance* mAnimComponents[8]; // size 0x20
    /* 0x0D68 (G4QE01), 0x1768 (G4QJ01) */ int mOldRanks[9];                        // size 0x24
    /* 0x0D8C (G4QE01), 0x178C (G4QJ01) */ int mNewRanks[9];                        // size 0x24
    /* 0x0DB0 (G4QE01), 0x17B0 (G4QJ01) */ int mStandingsIndices[8];                // size 0x20
    /* 0x0DD0 (G4QE01), 0x17D0 (G4QJ01) */ int mAnimatingKnockoutTeams[4];          // size 0x10
    /* 0x0DE0 (G4QE01), 0x17E0 (G4QJ01) */ int mCurrentKnockoutAnimationRound;      // size 0x4
    /* 0x0DE4 (G4QE01), 0x17E4 (G4QJ01) */ unsigned short mOldStats[8][4];          // size 0x40
    /* 0x0E24 (G4QE01), 0x1824 (G4QJ01) */ float mStatUpdateDelay;                  // size 0x4
    /* 0x0E28 (G4QE01), 0x1828 (G4QJ01) */ float mSlideSwitchDelay;                 // size 0x4
    /* 0x0E2C (G4QE01), 0x182C (G4QJ01) */ eHubState mHubState;                     // size 0x4
    /* 0x0E30 (G4QE01), 0x1830 (G4QJ01) */ CupTickerManager mTickerManager;         // size 0x408
    /* 0x1238 (G4QE01), 0x1C38 (G4QJ01) */ AsyncImage* mCaptainImage;               // size 0x4
    /* 0x123C (G4QE01), 0x1C3C (G4QJ01) */ ButtonComponent mButtons;                // size 0x24
}; // total size: 0x1260 (G4QE01), 0x1C60 (G4QJ01)

#endif // _SHCUPHUB_H_
