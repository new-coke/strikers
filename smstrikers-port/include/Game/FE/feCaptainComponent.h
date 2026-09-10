#ifndef _FECAPTAINCOMPONENT_H_
#define _FECAPTAINCOMPONENT_H_

#include "NL/nlString.h"
#include "Game/FE/feInput.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feCaptainGridComponent.h"
#include "Game/FE/feSidekickGridComponent.h"
#include "Game/GameInfo.h"
#include "Game/FE/feAsyncImage.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feMapMenu.h"
#include "Game/FE/tlTextInstance.h"

class TLComponentInstance;
class TLInstance;
class TLSlide;
struct InlineHasher;
class FEPresentation;
class AsyncImage;
class ISidekickGridComponent;

enum UpdateResult
{
    UPDATE_OK = 0,
    UPDATE_GO_BACK = 1,
    UPDATE_GO_FORWARD = 2,
};

extern bool g_e3_Build;

class IChooseCaptain
{
public:
    class NameComponent
    {
    public:
        void SetSidekickName(unsigned long);
        void SetTextName(const char*, unsigned long);
        void SetCaptainName(unsigned long);
        void SetCaptainLogo(const char*);

        /* 0x0 */ TLComponentInstance* mComponent;
        /* 0x4 */ const char* mCaptainObjName;
        /* 0x8 */ const char* mSidekickObjName;
    }; // total size: 0xC

    class ComponentState
    {
    public:
        enum Phase
        {
            PHASE_IDLE = 0,
            PHASE_CHOOSING_CAPTAIN = 1,
            PHASE_CHOOSING_SIDEKICK = 2,
            PHASE_READY = 3,
        };

        void GotoNextPhase();
        void GotoPreviousPhase();
        void SetCurrentPhase(Phase);

        /* 0x0 */ Phase mCurrentPhase;
        /* 0x4 */ IChooseCaptain* mParent;
        /* 0x8 */ int mHomeAway;
    }; // total size: 0xC

    IChooseCaptain();
    ~IChooseCaptain();
    void Initialize(const char* captainfilename, const char* sidekickfilename);
    void UpdateSound(float dt);
    UpdateResult Update(float dt);
    void UpdateAsyncImages();
    void SceneCreated(FEPresentation* presentation);
    void SetupCaptainComponent(TLComponentInstance* compinstance, int homeaway);
    void StartSidekickMiniHead(int homeaway, eSidekickID sidekick);
    void CheckForDisconnectedHumanPlayers();
    void FindAliveHumanPlayers();
    void UpdateSinglePlayerState();
    int GetSide(int padid);
    void SetupForLastPhase(eFEINPUT_PAD pad);
    void PushPlayer(eFEINPUT_PAD pad, int side);
    void PopPlayer(eFEINPUT_PAD pad);
    void ResetPushPlayerData();
    void PushPlayerWithGameInfoDB();
    void MoveHighlightToCurrentCaptain(int which);
    void SetupNameComponentToCurrentCaptain(int slot);

    void SetPhaseReady(int homeaway)
    {
        mComponentState[homeaway].SetCurrentPhase(ComponentState::PHASE_READY);
    }

    inline bool IsPlayerPushed(const int pad)
    {
        for (int i = 0; i < mNumTotalPushedPlayers; i++)
        {
            if (mAllPushedPlayers[i] == pad)
            {
                return true;
            }
        }
        return false;
    }

    /* 0x00 */ ComponentState mComponentState[2];                   // size 0x18
    /* 0x18 */ unsigned char mIsSinglePlayerInput;                  // size 0x1
    /* 0x1C */ eFEINPUT_PAD mAllPushedPlayers[4];                   // size 0x10
    /* 0x2C */ int mAllPushedPlayerSides[4];                        // size 0x10
    /* 0x3C */ int mNumTotalPushedPlayers;                          // size 0x4
    /* 0x40 */ AsyncImage* mAsyncImage[2][3];                       // size 0x18
    /* 0x58 */ int mHomeAwayTeam[2];                                // size 0x8
    /* 0x60 */ int mHomeAwaySidekicks[2];                           // size 0x8
    /* 0x68 */ bool mDidSwapCaptains[2];                            // size 0x2
    /* 0x6A */ bool mDidSwapSidekicks[2];                           // size 0x2
    /* 0x6C */ TLComponentInstance* mCaptainComponents[2];          // size 0x8
    /* 0x74 */ TLComponentInstance* mSidekickComponents[2];         // size 0x8
    /* 0x7C */ TLComponentInstance* mSidekickMiniHeadComponents[2]; // size 0x8
    /* 0x84 */ float mCaptainSlideDurations[2];                     // size 0x8
    /* 0x8C */ float mSidekickSlideDurations[2];                    // size 0x8
    /* 0x94 */ float mCaptainSoundDelay[2];                         // size 0x8
    /* 0x9C */ ICaptainGridComponent* mCaptainGridComponents[2];    // size 0x8
    /* 0xA4 */ ISidekickGridComponent* mSidekickGridComponents[2];  // size 0x8
    /* 0xAC */ char* mLastCaptainSelectSoundStrPlayed[2];           // size 0x8
    /* 0xB4 */ NameComponent mNameComponents[2];                    // size 0x18
}; // total size: 0xCC

#define PHASE_IDLE              IChooseCaptain::ComponentState::PHASE_IDLE
#define PHASE_CHOOSING_CAPTAIN  IChooseCaptain::ComponentState::PHASE_CHOOSING_CAPTAIN
#define PHASE_CHOOSING_SIDEKICK IChooseCaptain::ComponentState::PHASE_CHOOSING_SIDEKICK
#define PHASE_READY             IChooseCaptain::ComponentState::PHASE_READY

/**
 * Offset/Address/Size: 0x1104 | 0x800C08A8 | size: 0x6CC
 */
inline void IChooseCaptain::ComponentState::GotoNextPhase()
{
    ICaptainGridComponent* captaingrid;
    ISidekickGridComponent* sidekickgrid;
    eTeamID chosenteam;
    char filenameC2[0x80];
    char filenameC1[0x80];
    char filenameC0[0x80];
    char filenameS2[0x80];
    char filenameS1[0x80];
    char filenameS0[0x80];

    switch (mCurrentPhase)
    {
    case PHASE_CHOOSING_CAPTAIN:
        captaingrid = mParent->mCaptainGridComponents[mHomeAway];

        if (!captaingrid->mMapMenu->IsSelectedItemActive())
        {
            FEAudio::PlayAnimAudioEvent("sfx_deny", false);
            break;
        }

        captaingrid->mParentComponent->SetActiveSlide("OUT");
        captaingrid->mParentComponent->Update(0.0f);
        captaingrid->RebuildInstanceTable();
        captaingrid->mMapMenu->UpdateAllItems();
        captaingrid->RebindHighliteComponent("HIGHLIGHT");
        captaingrid->mHighliteComponent->m_bVisible = false;

        FEAudio::PlayAnimAudioEvent((mHomeAway == 0) ? "sfx_character_group_left_exit" : "sfx_character_group_right_exit", false);

        mParent->mHomeAwayTeam[mHomeAway] = captaingrid->GetSelectedItem();

        {
            FEMapMenu* mapmenu = mParent->mCaptainGridComponents[mHomeAway ^ 1]->mMapMenu;
            mapmenu->SetItemActive(captaingrid->mMapMenu->GetSelectedItem(), false);
        }

        chosenteam = captaingrid->GetSelectedItem();
        if (chosenteam != TEAM_MYSTERY)
        {
            sidekickgrid = mParent->mSidekickGridComponents[mHomeAway];
            sidekickgrid->mParentComponent->SetActiveSlide("IN");
            sidekickgrid->mParentComponent->Update(0.0f);
            sidekickgrid->RebuildInstanceTable();
            sidekickgrid->mMapMenu->UpdateAllItems();
            sidekickgrid->RebindHighliteComponent("HIGHLIGHT");
            sidekickgrid->mHighliteComponent->m_bVisible = false;
            sidekickgrid->mHighliteVisibilityAtAnimEnd = true;
            sidekickgrid->SetVisibleInstanceTable(true);
            sidekickgrid->mParentComponent->m_bVisible = true;
            mCurrentPhase = PHASE_CHOOSING_SIDEKICK;

            FEAudio::PlayAnimAudioEvent((mHomeAway == 0) ? "sfx_character_group_left_enter" : "sfx_character_group_right_enter", false);

            NameComponent* namecomponent = &mParent->mNameComponents[mHomeAway];
            namecomponent->mComponent->SetActiveSlide("Slide2");
            namecomponent->mComponent->Update(0.0f);
            mParent->mNameComponents[mHomeAway].SetCaptainName(GetLOCCharacterName((eTeamID)mParent->mHomeAwayTeam[mHomeAway], false, false));
            mParent->mNameComponents[mHomeAway].SetCaptainLogo(GetTeamName((eTeamID)mParent->mHomeAwayTeam[mHomeAway]));
            mParent->mNameComponents[mHomeAway].SetSidekickName(GetLOCSidekickName(sidekickgrid->GetSelectedItem()));

            if (g_e3_Build)
            {
                mParent->mSidekickGridComponents[mHomeAway]->MoveHighlightToTarget((eSidekickID)(mHomeAway ? 1 : 0));
                mParent->mNameComponents[mHomeAway].SetSidekickName(GetLOCSidekickName(sidekickgrid->GetSelectedItem()));
                GotoNextPhase();
            }
        }
        else
        {
            int teamID = mParent->mHomeAwayTeam[mHomeAway];
            IChooseCaptain* parent = mParent;
            int homeaway = mHomeAway;

            CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN, filenameC0, 0x80, teamID, homeaway);
            CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN_OUTLINE, filenameC1, 0x80, teamID, homeaway);
            CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN_FLASH, filenameC2, 0x80, teamID, homeaway);

            parent->mAsyncImage[homeaway][0]->QueueLoad(filenameC0, false);
            parent->mAsyncImage[homeaway][1]->QueueLoad(filenameC1, false);
            parent->mAsyncImage[homeaway][2]->QueueLoad(filenameC2, false);

            parent->mDidSwapCaptains[homeaway] = false;
            mCurrentPhase = PHASE_READY;
        }

        FEAudio::PlayAnimAudioEvent("sfx_accept_no_screen_change", false);
        mParent->mLastCaptainSelectSoundStrPlayed[mHomeAway] = (char*)FECharacterSound::PlayCaptainName((eTeamID)mParent->mHomeAwayTeam[mHomeAway]);
        break;

    case PHASE_CHOOSING_SIDEKICK:
    {
        ISidekickGridComponent* sidekickgrid2 = mParent->mSidekickGridComponents[mHomeAway];

        sidekickgrid2->mParentComponent->SetActiveSlide("OUT");
        sidekickgrid2->mParentComponent->Update(0.0f);
        sidekickgrid2->RebuildInstanceTable();
        sidekickgrid2->mMapMenu->UpdateAllItems();
        sidekickgrid2->RebindHighliteComponent("HIGHLIGHT");
        sidekickgrid2->mHighliteComponent->m_bVisible = false;

        FEAudio::PlayAnimAudioEvent((mHomeAway == 0) ? "sfx_character_group_left_exit" : "sfx_character_group_right_exit", false);

        if (g_e3_Build)
        {
            sidekickgrid2->SetVisibleInstanceTable(false);
        }

        mParent->mHomeAwaySidekicks[mHomeAway] = sidekickgrid2->GetSelectedItem();
        {
            IChooseCaptain* parent = mParent;
            int homeaway = mHomeAway;
            int teamID = parent->mHomeAwayTeam[homeaway];

            CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN, filenameS0, 0x80, teamID, homeaway);
            CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN_OUTLINE, filenameS1, 0x80, teamID, homeaway);
            CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN_FLASH, filenameS2, 0x80, teamID, homeaway);

            parent->mAsyncImage[homeaway][0]->QueueLoad(filenameS0, false);
            parent->mAsyncImage[homeaway][1]->QueueLoad(filenameS1, false);
            parent->mAsyncImage[homeaway][2]->QueueLoad(filenameS2, false);

            parent->mDidSwapCaptains[homeaway] = false;
            mParent->StartSidekickMiniHead(mHomeAway, (eSidekickID)mParent->mHomeAwaySidekicks[mHomeAway]);
        }
        mCurrentPhase = PHASE_READY;

        FEAudio::PlayAnimAudioEvent("sfx_accept_no_screen_change", false);

        if (!g_e3_Build)
        {
            if (mParent->mLastCaptainSelectSoundStrPlayed[mHomeAway] != NULL)
            {
                FEAudio::StopAnimAudioEvent(mParent->mLastCaptainSelectSoundStrPlayed[mHomeAway]);
                mParent->mLastCaptainSelectSoundStrPlayed[mHomeAway] = NULL;
            }

            FECharacterSound::PlaySidekickName((eSidekickID)mParent->mHomeAwaySidekicks[mHomeAway]);
        }
        break;
    }

    case PHASE_READY:
        if (mParent->mCaptainGridComponents[mHomeAway]->GetSelectedItem() != TEAM_MYSTERY)
        {
            mParent->mSidekickComponents[mHomeAway]->m_bVisible = false;
        }

        FEAudio::PlayAnimAudioEvent("sfx_accept_no_screen_change", false);
        break;

    default:
        break;
    }
}

/**
 * Offset/Address/Size: 0xA48 | 0x800C01EC | size: 0x6BC
 */
inline void IChooseCaptain::ComponentState::GotoPreviousPhase()
{
    ICaptainGridComponent* gridcomponent;
    ISidekickGridComponent* sidekickgrid;
    ICaptainGridComponent* captaingrid;
    ICaptainGridComponent* othercaptaingrid;
    ICaptainGridComponent* captaingrid2;
    ICaptainGridComponent* othercaptaingrid2;
    NameComponent* namecomponent;

    switch (mCurrentPhase)
    {
    case PHASE_CHOOSING_CAPTAIN:
        mCurrentPhase = PHASE_IDLE;

        gridcomponent = mParent->mCaptainGridComponents[mHomeAway];
        gridcomponent->mParentComponent->SetActiveSlide("OUT");
        gridcomponent->mParentComponent->Update(0.0f);
        gridcomponent->RebuildInstanceTable();
        gridcomponent->mMapMenu->UpdateAllItems();
        gridcomponent->RebindHighliteComponent("HIGHLIGHT");
        gridcomponent->mHighliteComponent->m_bVisible = false;

        mParent->mNameComponents[mHomeAway].SetCaptainName(0);
        mParent->mNameComponents[mHomeAway].SetCaptainLogo(NULL);
        mParent->mSidekickMiniHeadComponents[mHomeAway]->m_bVisible = false;

        FEAudio::PlayAnimAudioEvent("sfx_back_no_screen_change", true);
        break;

    case PHASE_CHOOSING_SIDEKICK:
        mCurrentPhase = PHASE_CHOOSING_CAPTAIN;

        mParent->mSidekickGridComponents[mHomeAway]->mParentComponent->m_bVisible = false;

        captaingrid = mParent->mCaptainGridComponents[mHomeAway];
        captaingrid->mParentComponent->SetActiveSlide("SELECT");
        captaingrid->mParentComponent->Update(0.0f);
        captaingrid->RebuildInstanceTable();
        captaingrid->mMapMenu->UpdateAllItems();
        captaingrid->RebindHighliteComponent("HIGHLIGHT");
        captaingrid->mHighliteComponent->m_bVisible = true;
        captaingrid->mParentComponent->m_bVisible = true;
        captaingrid->MoveHighlightToTarget((eTeamID)mParent->mHomeAwayTeam[mHomeAway]);

        othercaptaingrid = mParent->mCaptainGridComponents[mHomeAway ^ 1];
        othercaptaingrid->RebuildInstanceTable();
        othercaptaingrid->SetAllItemsActive();
        captaingrid->RebuildInstanceTable();
        captaingrid->SetAllItemsActive();

        namecomponent = &mParent->mNameComponents[mHomeAway];
        namecomponent->mComponent->SetActiveSlide("Slide1");
        namecomponent->mComponent->Update(0.0f);
        mParent->mNameComponents[mHomeAway].SetCaptainName(GetLOCCharacterName(captaingrid->GetSelectedItem(), false, false));
        mParent->mNameComponents[mHomeAway].SetCaptainLogo(GetTeamName(captaingrid->GetSelectedItem()));

        if (mParent->mComponentState[mHomeAway ^ 1].mCurrentPhase > PHASE_CHOOSING_CAPTAIN)
        {
            FEMapMenu* menu = captaingrid->mMapMenu;
            menu->SetItemActive(othercaptaingrid->mMapMenu->GetSelectedItem(), false);
        }

        mParent->mSidekickMiniHeadComponents[mHomeAway]->m_bVisible = false;

        FEAudio::PlayAnimAudioEvent("sfx_back_no_screen_change", false);
        break;

    case PHASE_READY:
        if (mParent->mCaptainGridComponents[mHomeAway]->GetSelectedItem() != (eTeamID)8 && mParent->mHomeAwayTeam[mHomeAway] != 8)
        {
            mCurrentPhase = PHASE_CHOOSING_SIDEKICK;

            sidekickgrid = mParent->mSidekickGridComponents[mHomeAway];
            sidekickgrid->mParentComponent->SetActiveSlide("SELECT");
            sidekickgrid->mParentComponent->Update(0.0f);
            sidekickgrid->RebuildInstanceTable();
            sidekickgrid->mMapMenu->UpdateAllItems();

            mParent->mSidekickGridComponents[mHomeAway]->RebindHighliteComponent("HIGHLIGHT");
            mParent->mSidekickGridComponents[mHomeAway]->mHighliteComponent->m_bVisible = true;
            mParent->mSidekickGridComponents[mHomeAway]->SetVisibleInstanceTable(true);
            mParent->mSidekickGridComponents[mHomeAway]->MoveHighlightToTarget((eSidekickID)mParent->mHomeAwaySidekicks[mHomeAway]);
            mParent->mSidekickGridComponents[mHomeAway]->mParentComponent->m_bVisible = true;

            mParent->mSidekickComponents[mHomeAway]->m_bVisible = false;
            mParent->mCaptainComponents[mHomeAway]->m_bVisible = false;
            mParent->mSidekickMiniHeadComponents[mHomeAway]->m_bVisible = false;

            if (g_e3_Build)
            {
                GotoPreviousPhase();
            }
        }
        else
        {
            mCurrentPhase = PHASE_CHOOSING_CAPTAIN;

            captaingrid2 = mParent->mCaptainGridComponents[mHomeAway];
            captaingrid2->mParentComponent->SetActiveSlide("SELECT");
            captaingrid2->mParentComponent->Update(0.0f);
            captaingrid2->RebuildInstanceTable();
            captaingrid2->mMapMenu->UpdateAllItems();
            captaingrid2->RebindHighliteComponent("HIGHLIGHT");
            captaingrid2->mHighliteComponent->m_bVisible = true;
            captaingrid2->mParentComponent->m_bVisible = true;
            captaingrid2->MoveHighlightToTarget((eTeamID)mParent->mHomeAwayTeam[mHomeAway]);
            mParent->mCaptainComponents[mHomeAway]->m_bVisible = false;

            othercaptaingrid2 = mParent->mCaptainGridComponents[mHomeAway ^ 1];
            othercaptaingrid2->RebuildInstanceTable();
            othercaptaingrid2->SetAllItemsActive();
            captaingrid2->RebuildInstanceTable();
            captaingrid2->SetAllItemsActive();

            namecomponent = &mParent->mNameComponents[mHomeAway];
            namecomponent->mComponent->SetActiveSlide("Slide1");
            namecomponent->mComponent->Update(0.0f);
            mParent->mNameComponents[mHomeAway].SetCaptainName(GetLOCCharacterName(captaingrid2->GetSelectedItem(), false, false));
            mParent->mNameComponents[mHomeAway].SetCaptainLogo(GetTeamName(captaingrid2->GetSelectedItem()));

            if (mParent->mComponentState[mHomeAway ^ 1].mCurrentPhase > PHASE_CHOOSING_CAPTAIN)
            {
                FEMapMenu* menu = captaingrid2->mMapMenu;
                menu->SetItemActive(othercaptaingrid2->mMapMenu->GetSelectedItem(), false);
            }
        }

        FEAudio::PlayAnimAudioEvent("sfx_back_no_screen_change", false);
        break;

    case PHASE_IDLE:
    default:
        break;
    }
}

/**
 * Offset/Address/Size: 0xA20 | 0x800C01C4 | size: 0x28
 */
inline void IChooseCaptain::NameComponent::SetSidekickName(unsigned long id)
{
    SetTextName(mSidekickObjName, id);
}

/**
 * Offset/Address/Size: 0x1AC | 0x800BF950 | size: 0x874
 */
inline void IChooseCaptain::ComponentState::SetCurrentPhase(Phase phase)
{
    ICaptainGridComponent* captaingrid;
    NameComponent* namecomponent;
    ISidekickGridComponent* gridcomponent;
    FEMapMenu* menu;
    IChooseCaptain* parent;
    int teamID;
    int homeaway;
    char filename2[0x80];
    char filename1[0x80];
    char filename0[0x80];

    switch (phase)
    {
    case PHASE_IDLE:
        mParent->mCaptainGridComponents[mHomeAway]->mParentComponent->m_bVisible = false;
        mParent->mSidekickGridComponents[mHomeAway]->mParentComponent->m_bVisible = false;
        mParent->mCaptainComponents[mHomeAway]->m_bVisible = false;
        mParent->mSidekickComponents[mHomeAway]->m_bVisible = false;
        mParent->mSidekickMiniHeadComponents[mHomeAway]->m_bVisible = false;
        mParent->mNameComponents[mHomeAway].SetCaptainName(0);
        mParent->mNameComponents[mHomeAway].SetCaptainLogo(NULL);
        break;

    case PHASE_CHOOSING_CAPTAIN:
    {
        captaingrid = mParent->mCaptainGridComponents[mHomeAway];
        captaingrid->mParentComponent->SetActiveSlide("IN");
        captaingrid->mParentComponent->Update(0.0f);
        captaingrid->RebuildInstanceTable();
        captaingrid->mMapMenu->UpdateAllItems();
        captaingrid->mParentComponent->m_bVisible = true;
        captaingrid->RebindHighliteComponent("HIGHLIGHT");
        captaingrid->mHighliteComponent->m_bVisible = false;
        captaingrid->mHighliteVisibilityAtAnimEnd = true;
        captaingrid->RebuildInstanceTable();
        captaingrid->SetAllItemsActive();

        FEAudio::PlayAnimAudioEvent(mHomeAway == 0 ? "sfx_character_group_left_enter" : "sfx_character_group_right_enter", false);

        if (mParent->mComponentState[mHomeAway ^ 1].mCurrentPhase > PHASE_CHOOSING_CAPTAIN)
        {
            FEMapMenu* captaingridmenu = captaingrid->mMapMenu;
            captaingridmenu->SetItemActive(mParent->mCaptainGridComponents[mHomeAway ^ 1]->mMapMenu->GetSelectedItem(), false);
        }

        FEMapMenu* captaingridmenu = captaingrid->mMapMenu;
        int firstcaptain = captaingridmenu->GetSelectedItem();
        int rowfirstcaptain = firstcaptain;

        while (!captaingridmenu->IsSelectedItemActive())
        {
            captaingridmenu->MoveDown(true);
            if (rowfirstcaptain == captaingridmenu->GetSelectedItem())
            {
                captaingridmenu->MoveRight(true);
                rowfirstcaptain = captaingridmenu->GetSelectedItem();
                if (rowfirstcaptain == firstcaptain)
                {
                    break;
                }
            }
        }

        mParent->mSidekickGridComponents[mHomeAway]->mParentComponent->m_bVisible = false;
        mParent->mCaptainComponents[mHomeAway]->m_bVisible = false;
        mParent->mSidekickComponents[mHomeAway]->m_bVisible = false;

        namecomponent = &mParent->mNameComponents[mHomeAway];
        namecomponent->mComponent->SetActiveSlide("Slide1");
        namecomponent->mComponent->Update(0.0f);
        mParent->mNameComponents[mHomeAway].SetCaptainName(GetLOCCharacterName(captaingrid->GetSelectedItem(), false, true));
        mParent->mNameComponents[mHomeAway].SetCaptainLogo(GetTeamName(captaingrid->GetSelectedItem()));

        mParent->mSidekickMiniHeadComponents[mHomeAway]->m_bVisible = false;
        break;
    }

    case PHASE_CHOOSING_SIDEKICK:
        mParent->mCaptainGridComponents[mHomeAway]->mParentComponent->m_bVisible = false;

        gridcomponent = mParent->mSidekickGridComponents[mHomeAway];
        gridcomponent->mParentComponent->SetActiveSlide("IN");
        gridcomponent->mParentComponent->Update(0.0f);
        gridcomponent->RebuildInstanceTable();
        gridcomponent->mMapMenu->UpdateAllItems();
        gridcomponent->RebindHighliteComponent("HIGHLIGHT");
        gridcomponent->mHighliteComponent->m_bVisible = false;
        gridcomponent->mHighliteVisibilityAtAnimEnd = true;
        gridcomponent->SetVisibleInstanceTable(true);
        gridcomponent->mParentComponent->m_bVisible = true;

        FEAudio::PlayAnimAudioEvent(mHomeAway == 0 ? "sfx_character_group_left_enter" : "sfx_character_group_right_enter", false);

        namecomponent = &mParent->mNameComponents[mHomeAway];
        namecomponent->mComponent->SetActiveSlide("Slide2");
        namecomponent->mComponent->Update(0.0f);
        mParent->mNameComponents[mHomeAway].SetCaptainName(GetLOCCharacterName((eTeamID)mParent->mHomeAwayTeam[mHomeAway], false, false));
        mParent->mNameComponents[mHomeAway].SetSidekickName(GetLOCSidekickName(gridcomponent->GetSelectedItem()));
        mParent->mNameComponents[mHomeAway].SetCaptainLogo(GetTeamName((eTeamID)mParent->mHomeAwayTeam[mHomeAway]));

        mParent->mCaptainComponents[mHomeAway]->m_bVisible = false;
        mParent->mSidekickComponents[mHomeAway]->m_bVisible = false;
        mParent->mSidekickMiniHeadComponents[mHomeAway]->m_bVisible = false;
        break;

    case PHASE_READY:
    {
        mParent->mCaptainGridComponents[mHomeAway]->mParentComponent->m_bVisible = false;
        mParent->mSidekickGridComponents[mHomeAway]->mParentComponent->m_bVisible = true;
        mParent->mSidekickGridComponents[mHomeAway]->mParentComponent->SetActiveSlide("in");
        mParent->mSidekickGridComponents[mHomeAway]->SetVisibleInstanceTable(false);
        mParent->mSidekickGridComponents[mHomeAway]->mHighliteComponent->m_bVisible = false;

        teamID = mParent->mHomeAwayTeam[mHomeAway];
        parent = mParent;
        homeaway = mHomeAway;
        CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN, filename0, 0x80, teamID, homeaway);
        CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN_OUTLINE, filename1, 0x80, teamID, homeaway);
        CaptainSidekickFilename::Build(CaptainSidekickFilename::TYPE_CAPTAIN_FLASH, filename2, 0x80, teamID, homeaway);
        parent->mAsyncImage[homeaway][0]->QueueLoad(filename0, true);
        parent->mAsyncImage[homeaway][1]->QueueLoad(filename1, true);
        parent->mAsyncImage[homeaway][2]->QueueLoad(filename2, true);
        parent->mDidSwapCaptains[homeaway] = false;

        mParent->mCaptainComponents[mHomeAway]->m_bVisible = true;
        if (mParent->mHomeAwayTeam[mHomeAway] != TEAM_MYSTERY)
        {
            mParent->mSidekickComponents[mHomeAway]->m_bVisible = false;
            namecomponent = &mParent->mNameComponents[mHomeAway];
            namecomponent->mComponent->SetActiveSlide("Slide2");
            namecomponent->mComponent->Update(0.0f);
            mParent->mNameComponents[mHomeAway].SetSidekickName(GetLOCSidekickName((eSidekickID)mParent->mHomeAwaySidekicks[mHomeAway]));
        }

        mParent->mNameComponents[mHomeAway].SetCaptainName(GetLOCCharacterName((eTeamID)mParent->mHomeAwayTeam[mHomeAway], false, false));
        mParent->mNameComponents[mHomeAway].SetCaptainLogo(GetTeamName((eTeamID)mParent->mHomeAwayTeam[mHomeAway]));

        mParent->mCaptainGridComponents[mHomeAway]->mMapMenu->SetSelectedItem(mParent->mHomeAwayTeam[mHomeAway]);
        mParent->mCaptainGridComponents[mHomeAway ^ 1]->mMapMenu->SetSelectedItem(mParent->mHomeAwayTeam[mHomeAway ^ 1]);

        menu = mParent->mCaptainGridComponents[mHomeAway]->mMapMenu;
        menu->SetItemActive(menu->GetSelectedItem(), false);

        menu = mParent->mCaptainGridComponents[mHomeAway ^ 1]->mMapMenu;
        menu->SetItemActive(menu->GetSelectedItem(), false);

        mParent->StartSidekickMiniHead(mHomeAway, (eSidekickID)mParent->mHomeAwaySidekicks[mHomeAway]);
        break;
    }

    default:
        break;
    }

    mCurrentPhase = phase;
}

/**
 * Offset/Address/Size: 0xEC | 0x800BF890 | size: 0xC0
 */
inline void IChooseCaptain::NameComponent::SetTextName(const char* objname, unsigned long locstring)
{
    TLTextInstance* textinstance = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        mComponent->GetActiveSlide(),
        InlineHasher(nlStringLowerHash(objname)));

    if (locstring != 0)
    {
        textinstance->m_LocStrId = locstring;
        textinstance->m_OverloadFlags |= 0x8u;
        textinstance->m_bVisible = true;
    }
    else
    {
        textinstance->m_bVisible = false;
    }
}

/**
 * Offset/Address/Size: 0xC4 | 0x800BF868 | size: 0x28
 */
inline void IChooseCaptain::NameComponent::SetCaptainName(unsigned long captainID)
{
    SetTextName(mCaptainObjName, captainID);
}

/**
 * Offset/Address/Size: 0x0 | 0x800BF7A4 | size: 0xC4
 */
inline void IChooseCaptain::NameComponent::SetCaptainLogo(const char* name)
{
    TLComponentInstance* comp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        mComponent->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("component")));

    if (name != NULL)
    {
        comp->SetActiveSlide(name);
        comp->m_bVisible = true;
    }
    else
    {
        comp->m_bVisible = false;
    }
}

#endif // _FECAPTAINCOMPONENT_H_
