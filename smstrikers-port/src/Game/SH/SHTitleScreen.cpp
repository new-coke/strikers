#include "Game/SH/SHTitleScreen.h"
#include "Game/GameSceneManager.h"
#include "Game/GameInfo.h"
#include "Game/FE/feSceneManager.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feMusic.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/Audio/StreamTrack.h"
#include "Game/FE/tlComponentInstance.h"
#include "Game/SH/SHLoading.h"
#include "Game/SH/SHMainMenu.h"
#include "Game/main.h"

/**
 * Offset/Address/Size: 0x9F8 | 0x800ACFB4 | size: 0xA8
 */
void StartMovieCB()
{
    BaseSceneHandler* handler;

    if (GameSceneManager::Instance() != NULL)
    {
        handler = GameSceneManager::Instance()->GetCurrentScene();
        if ((handler != NULL) && (GameSceneManager::Instance()->GetSceneType(handler) == 2)
            && (handler->m_pFEScene->m_bValid != false))
        {
            GameSceneManager::Instance()->PopEntireStack();
            FESceneManager::Instance()->ForceImmediateStackProcessing();
            GameSceneManager::Instance()->Push((SceneList)0x35, SCREEN_NOTHING, false);
        }
    }
}

/**
 * Offset/Address/Size: 0x9F4 | 0x800ACFB0 | size: 0x4
 */
void DoNothingCallback()
{
}

static inline void SetIdleCallback(AudioStreamTrack::StreamTrack* track, const Function0<void>& f0)
{
    track->m_IdleCallback = Function<FnVoidVoid>(f0);
}

/**
 * Offset/Address/Size: 0x7F0 | 0x800ACDAC | size: 0x204
 */
TitleScene::TitleScene()
{
    m_fTimeElapsed = 0.0f;
    mStartedDemo = false;
    mStartedMovie = false;

    AudioStreamTrack::TrackManagerBase* trackMgr = g_pTrackManager;
    // PORT: null when audio is disabled, there is no track manager to ask.
    AudioStreamTrack::StreamTrack* track =
        trackMgr != NULL ? trackMgr->GetTrack(nlStringLowerHash("FE")) : NULL;
    if (track != NULL)
    {
        Function0<void> f0(DoNothingCallback);
        SetIdleCallback(track, f0);
    }
}

/**
 * Offset/Address/Size: 0x794 | 0x800ACD50 | size: 0x5C
 */
TitleScene::~TitleScene()
{
}

/**
 * Offset/Address/Size: 0x688 | 0x800ACC44 | size: 0x10C
 */
void TitleScene::SceneCreated()
{

    FEMusic::StopStream();
    AudioLoader::PlayFETitleMusicWithFade();

    TLComponentInstance* comp = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer2")),
        InlineHasher(nlStringLowerHash("Component2")));

    mTextPressStart = FEFinder<TLTextInstance, 3>::Find<TLSlide>(
        comp->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("Text")));
}

/**
 * Offset/Address/Size: 0x254 | 0x800AC810 | size: 0x434
 */
void TitleScene::Update(float dt)
{
    BaseSceneHandler::Update(dt);

    m_fTimeElapsed += dt;
    if (m_fTimeElapsed < 1.0f)
    {
        mTextPressStart->m_bVisible = false;
        return;
    }

    mTextPressStart->m_bVisible = true;
    if (mStartedDemo)
    {
        mTextPressStart->m_bVisible = false;
        return;
    }

    float demoTimeout = GetConfigFloat(Config::Global(), "fe_demo_mode_time_out", 60.0f);

    if (!mStartedDemo && m_fTimeElapsed >= demoTimeout)
    {
        if (nlSingleton<GameInfoManager>::Instance()->mDemoEnabled)
        {
            bool doSoak = GetConfigBool(Config::Global(), "dosoak", false);

            if (doSoak)
            {
                eSidekickID awaySidekick;
                eSidekickID homeSidekick;
                eTeamID awayTeam;
                eTeamID homeTeam;
                GameInfoManager* gim;

                (gim = nlSingleton<GameInfoManager>::s_pInstance)->SetMode(GameInfoManager::GM_DEMO);

                homeTeam = (eTeamID)nlRandom(8, &nlDefaultSeed);
                awayTeam = homeTeam;
                while (homeTeam == awayTeam)
                {
                    awayTeam = (eTeamID)nlRandom(8, &nlDefaultSeed);
                }

                if (g_e3_Build)
                {
                    do
                    {
                        homeTeam = (eTeamID)nlRandom(8, &nlDefaultSeed);
                    } while (homeTeam != TEAM_MARIO && homeTeam != TEAM_DONKEYKONG && homeTeam != TEAM_PEACH && homeTeam != TEAM_WARIO);

                    while (true)
                    {
                        eTeamID randomTeam = (eTeamID)nlRandom(8, &nlDefaultSeed);

                        if (randomTeam != TEAM_MARIO && randomTeam != TEAM_DONKEYKONG && randomTeam != TEAM_PEACH && randomTeam != TEAM_WARIO)
                        {
                            continue;
                        }

                        if ((homeTeam != TEAM_INVALID) && (homeTeam == randomTeam))
                        {
                            continue;
                        }

                        awayTeam = randomTeam;
                        break;
                    }
                }

                homeSidekick = (eSidekickID)nlRandom(4, &nlDefaultSeed);
                awaySidekick = homeSidekick;
                while (awaySidekick == homeSidekick)
                {
                    awaySidekick = (eSidekickID)nlRandom(4, &nlDefaultSeed);
                }

                if (g_e3_Build)
                {
                    homeSidekick = SK_TOAD;
                    awaySidekick = SK_KOOPA;
                }

                gim->SetStadium(gim->PickStadium(false, STAD_INVALID));
                gim->SetTeam(0, homeTeam);
                gim->SetTeam(1, awayTeam);
                gim->SetSidekick(0, homeSidekick);
                gim->SetSidekick(1, awaySidekick);
                gim->ResetPlayingSides();

                SuperLoadingScene* scene = (SuperLoadingScene*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_SUPER_LOADING, SCREEN_NOTHING, true);
                scene->mType = SuperLoadingScene::TT_IN;
            }
            else
            {
                ((void (*)(TitleScene*))StartIntroMovie)(this);
            }
        }

        m_fTimeElapsed = 0.0f;
        mStartedDemo = true;
        return;
    }

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x24, true, NULL)
        || g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
    {
        FEAudio::PlayAnimAudioEvent("sfx_accept", false);
        nlSingleton<GameSceneManager>::Instance()->Push(SCENE_MAIN_MENU, SCREEN_FORWARD, true);

        SHMainMenu::mSnapMenuIntoPosition = false;
        SHMainMenu::mLastMenuItem = 0;

        nlSingleton<GameInfoManager>::Instance()->mUserInfo.mGameplayOptions.OnSettingsUpdated();
        nlSingleton<GameInfoManager>::Instance()->mUserInfo.mPowerupOptions.OnSettingsUpdated();
        nlSingleton<GameInfoManager>::Instance()->mUserInfo.mCheatOptions.OnSettingsUpdated();
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x800AC5BC | size: 0x254
 */
void TitleScene::StartIntroMovie()
{
    AudioStreamTrack::TrackManagerBase* trackMgr = g_pTrackManager;
    // PORT: null when audio is disabled, nothing to ask.
    AudioStreamTrack::StreamTrack* track = trackMgr != NULL ? trackMgr->GetTrack(nlStringLowerHash("FE")) : NULL;

    if (track != NULL)
    {
        track->Stop(1000);
    }

    // PORT: a null track takes the same path as silent music.
    if ((track == NULL) || (Audio::MasterVolume::GetVolume(Audio::MasterVolume::VG_Music) == 0.0f))
    {
        BaseSceneHandler* handler;

        if (GameSceneManager::Instance() == NULL)
        {
            return;
        }

        handler = GameSceneManager::Instance()->GetCurrentScene();

        if ((handler != NULL) && (GameSceneManager::Instance()->GetSceneType(handler) == 2)
            && (handler->m_pFEScene->m_bValid != false))
        {
            GameSceneManager::Instance()->PopEntireStack();
            FESceneManager::Instance()->ForceImmediateStackProcessing();
            GameSceneManager::Instance()->Push((SceneList)0x35, SCREEN_NOTHING, false);
        }
    }
    else
    {
        Function0<void> f0(StartMovieCB);
        SetIdleCallback(track, f0);
    }
}
