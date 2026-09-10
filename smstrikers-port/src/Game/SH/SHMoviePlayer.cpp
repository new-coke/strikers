#include "Game/SH/SHMoviePlayer.h"
#include "dolphin/dolphin.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/BaseGameSceneManager.h"
#include "Game/BaseSceneHandler.h"
#include "Game/FE/FEAudio.h"
#include "Game/FE/feAsyncImage.h"
#include "Game/FE/feButtonComponent.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feInput.h"
#include "Game/FE/fePresentation.h"
#include "Game/GameInfo.h"
#include "Game/GameRenderTask.h"
#include "Game/GameSceneManager.h"
#include "Game/OverlayManager.h"
#include "Game/Sys/movie.h"
#include "NL/nlConfig.h"
#include "NL/gl/gl.h"
#include "NL/nlPrint.h"
#include "types.h"

extern bool g_e3_Build;
extern FEInput* g_pFEInput;

extern "C" void THPSimpleSetVolume(int, int);

/**
 * Offset/Address/Size: 0x81C | 0x800BD374 | size: 0x78
 */
MoviePlayerScene::MoviePlayerScene()
{
    mSwappedTexture = false;
    mMovieStarted = false;
    mMovieInstance = nullptr;
    mNextScene = SCENE_INVALID;
    mWithSound = false;
    mLoopMovie = false;
    mPushWithPop = true;
    mMovieFilename[0] = '\0';
    if (nlSingleton<GameSceneManager>::s_pInstance)
    {
        mGameSceneManager = nlSingleton<GameSceneManager>::s_pInstance;
        return;
    }
    mGameSceneManager = nlSingleton<OverlayManager>::s_pInstance;
}

/**
 * Offset/Address/Size: 0x778 | 0x800BD2D0 | size: 0xA4
 */
MoviePlayerScene::~MoviePlayerScene()
{
    if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
    {
        g_bRenderWorld = true;
    }

    if (g_pFEInput->HasInputLock(this))
    {
        g_pFEInput->PopExclusiveInputLock(this);
    }
}

/**
 * Offset/Address/Size: 0x74C | 0x800BD2A4 | size: 0x2C
 */
void MoviePlayerScene::SceneCreated()
{
    OverrideMovieDimensions();
}

/**
 * Offset/Address/Size: 0x6F0 | 0x800BD248 | size: 0x5C
 */
void MoviePlayerScene::SetMovieDetails(const char* filename, bool withsound, bool loopmovie)
{
    nlStrNCpy<char>((char*)mMovieFilename, filename, 0x80);
    mMovieFilename[127] = 0;
    mWithSound = withsound;
    mLoopMovie = loopmovie;
}

/**
 * Offset/Address/Size: 0x214 | 0x800BCD6C | size: 0x4DC
 */
void MoviePlayerScene::Update(float fDeltaT)
{

    BaseSceneHandler::Update(fDeltaT);

    if (g_e3_Build || (OSGetConsoleType() & 0x20000000))
    {
        if (mPushWithPop)
        {
            MoviePlayerScene* scene = (MoviePlayerScene*)mGameSceneManager->Push(mNextScene, SCREEN_NOTHING, true);
            if (mNextScene == SCENE_TITLE)
            {
                ((u8*)&scene->mNextScene)[1] = 1;
            }
            PlayScreenForwardSFX();
        }
        else
        {
            mGameSceneManager->Pop();
            PlayScreenBackSFX();
        }
        return;
    }

    if (mMovieStarted == false)
    {
        mMovieStarted = MovieStart(mMovieFilename, mWithSound, mLoopMovie);

        const char* streamName;
        if (strstr(mMovieFilename, "nlg"))
        {
            streamName = "FE_Eggman_Movie";
        }
        else
        {
            streamName = "FE_Intro_Movie";
        }

        char var_68[64];
        nlSNPrintf(var_68, 64, "%s/Volume", streamName);
        float volume = (float)GetConfigInt(g_FEStreamConfig, var_68, 100) / 100.0f;

        nlSNPrintf(var_68, 64, "%s/FadeIn", streamName);
        s32 fadeIn = GetConfigInt(g_FEStreamConfig, var_68, 500);

        volume *= Audio::MasterVolume::GetVolume(Audio::MasterVolume::VG_Music);
        THPSimpleSetVolume(0, 0);
        THPSimpleSetVolume((s32)(127.0f * volume), fadeIn);

        if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
        {
            g_bRenderWorld = false;
        }
    }

    if ((mMovieStarted == false) || CheckMoviePlayerAbort())
    {
        MovieStop();

        if (mPushWithPop)
        {
            MoviePlayerScene* scene = (MoviePlayerScene*)mGameSceneManager->Push(mNextScene, SCREEN_NOTHING, true);
            if (mNextScene == SCENE_TITLE)
            {
                ((u8*)&scene->mNextScene)[1] = 1;
            }
            PlayScreenForwardSFX();
        }
        else
        {
            mGameSceneManager->Pop();
            PlayScreenBackSFX();
        }

        FEAudio::PlayAnimAudioEvent("sfx_accept", false);
        mMovieStarted = false;
        return;
    }

    if (mSwappedTexture == false)
    {

        mMovieInstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
            m_pFEPresentation->m_currentSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("movie")));

        u32 movieHandle = glGetTexture("movie");
        mMovieInstance->m_pTextureResource->m_glTextureHandle = movieHandle;
        mSwappedTexture = true;
    }

    if (MoviePlay())
    {
        return;
    }

    MovieStop();

    if (mPushWithPop)
    {
        MoviePlayerScene* scene = (MoviePlayerScene*)mGameSceneManager->Push(mNextScene, SCREEN_NOTHING, true);
        if (mNextScene == SCENE_TITLE)
        {
            ((u8*)&scene->mNextScene)[1] = 1;
        }
        PlayScreenForwardSFX();
    }
    else
    {
        mGameSceneManager->Pop();
        PlayScreenBackSFX();
    }
}

/**
 * Offset/Address/Size: 0x1A4 | 0x800BCCFC | size: 0x70
 */
bool MoviePlayerScene::CheckMoviePlayerAbort()
{
    bool isAbort = false;

    if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x24, true, nullptr) || g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, nullptr))
    {
        isAbort = true;
    }
    return isAbort;
}

/**
 * Offset/Address/Size: 0x178 | 0x800BCCD0 | size: 0x2C
 */
void MoviePlayerScene::PlayScreenForwardSFX()
{
    FEAudio::PlayAnimAudioEvent("sfx_screen_forward", false);
}

/**
 * Offset/Address/Size: 0x14C | 0x800BCCA4 | size: 0x2C
 */
void MoviePlayerScene::PlayScreenBackSFX()
{
    FEAudio::PlayAnimAudioEvent("sfx_screen_back", false);
}

/**
 * Offset/Address/Size: 0x148 | 0x800BCCA0 | size: 0x4
 */
void MoviePlayerScene::OverrideMovieDimensions()
{
}

/**
 * Offset/Address/Size: 0x98 | 0x800BCBF0 | size: 0xB0
 */
void LessonMoviePlayerScene::SceneCreated()
{

    OverrideMovieDimensions();

    mButtonComponent.mButtonInstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
        m_pFEPresentation->m_currentSlide,
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("buttons")));

    mButtonComponent.SetState(ButtonComponent::BS_B_ONLY);
}

/**
 * Offset/Address/Size: 0x64 | 0x800BCBBC | size: 0x34
 */
bool LessonMoviePlayerScene::CheckMoviePlayerAbort()
{
    return g_pFEInput->JustPressed(FE_ALL_PADS, 0x200, false, nullptr);
}

/**
 * Offset/Address/Size: 0x4 | 0x800BCB5C | size: 0x60
 */
void LessonMoviePlayerScene::Update(float fDeltaT)
{
    MoviePlayerScene::Update(fDeltaT);
    if (mMovieStarted)
    {
        mButtonComponent.CentreButtons();
        return;
    }
    if (mButtonComponent.mButtonInstance != nullptr)
    {
        mButtonComponent.mButtonInstance->m_bVisible = false;
    }
    glDiscardFrame(2);
}

/**
 * Offset/Address/Size: 0x0 | 0x800BCB58 | size: 0x4
 */
void NLGLogoMovieScene::OverrideMovieDimensions()
{
    // Empty
}
