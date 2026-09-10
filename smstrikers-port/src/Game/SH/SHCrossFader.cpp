#include "Game/SH/SHCrossFader.h"
#include "port/benchmark.h"

#include "NL/nlColour.h"
#include "NL/nlConfig.h"
#include "NL/nlFileGC.h"
#include "NL/nlMath.h"
#include "NL/nlMemory.h"
#include "NL/nlPrint.h"
#include "NL/nlString.h"
#include "NL/plat/plataudio.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/BaseGameSceneManager.h"
#include "Game/FE/FEAudio.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/fePresentation.h"
#include "Game/FE/feResourceManager.h"
#include "Game/FE/feSceneManager.h"
#include "Game/GameInfo.h"
#include "Game/GameSceneManager.h"

/**
 * Offset/Address/Size: 0x93C | 0x800BC840 | size: 0x5C
 */
CrossFaderScene::CrossFaderScene()
{
    mNumImages = 0;
    mCurrentImage = 0;
    mImageInstances = NULL;
    mTimer = 0.0f;
    mAlpha = 0.0f;
    mFadeState = FS_INVALID;
    mFadeToBlackTimer = 0.0f;
}

/**
 * Offset/Address/Size: 0x8B8 | 0x800BC7BC | size: 0x84
 */
CrossFaderScene::~CrossFaderScene()
{
    if (mImageInstances != NULL)
    {
        delete[] mImageInstances;
        mImageInstances = NULL;
    }
}

/**
 * Offset/Address/Size: 0x510 | 0x800BC414 | size: 0x3A8
 */
void CrossFaderScene::SceneCreated()
{
    typedef TLImageInstance* (*FindImageByValue)(FEPresentation*, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher, InlineHasher);
    typedef TLImageInstance* (*FindImageByRef)(FEPresentation*, InlineHasher&, InlineHasher&, InlineHasher&, InlineHasher&, InlineHasher&, InlineHasher&);

    FEPresentation* pPresentation;
    bool doCrossFade = true;
    if (Config::Global().Exists("showlegal"))
    {
        doCrossFade = GetConfigBool(Config::Global(), "showlegal", false);
    }

    if (!doCrossFade)
    {
        AudioLoader::LoadFEAudioData(true);
        while (!nlSingleton<FESceneManager>::Instance()->AreAllScenesValid())
        {
            nlServiceFileSystem();
            nlSingleton<FESceneManager>::Instance()->Update(0.0f);
            nlSingleton<FEResourceManager>::Instance()->Run(0.0f);
        }
        nlSingleton<GameSceneManager>::Instance()->PopEntireStack();
        nlSingleton<GameSceneManager>::Instance()->Push(SCENE_INTRO_MOVIE, SCREEN_NOTHING, false);
    }
    else
    {
        char buf[64];
        nlColour colour;
        volatile unsigned long hB, hA, h9, h8, h7, h6, h5, h4, h3, h2, h1, h0;
        volatile unsigned long s8, s7, s6, s5, s4, s3, s2, s1, s0;
        volatile unsigned long t8, t7, t6, t5, t4, t3, t2, t1, t0;

        pPresentation = m_pFEScene->m_pFEPackage->GetPresentation();
        mNumImages = 0;

        do
        {
            nlSNPrintf(buf, 64, "screen%d", mNumImages);

            h0 = 0;
            h1 = 0;
            h2 = 0;
            h3 = 0;
            h4 = 0;
            h5 = 0;

            unsigned long hash = nlStringLowerHash(buf);
            h6 = hash;
            h7 = hash;
            hash = nlStringLowerHash("Layer");
            h8 = hash;
            h9 = hash;
            hash = nlStringLowerHash("Slide1");
            hA = hash;
            hB = hash;

            union
            {
                FindImageByValue byValue;
                FindImageByRef byRef;
            } f1;
            f1.byValue = FEFinder<TLImageInstance, 2>::Find<FEPresentation>;
            TLImageInstance* found = f1.byValue(
                pPresentation,
                (InlineHasher)hB,
                (InlineHasher)h9,
                (InlineHasher)h7,
                (InlineHasher)h5,
                (InlineHasher)h3,
                (InlineHasher)h1);

            if (found == NULL)
            {
                break;
            }
            mNumImages++;
        } while (true);

        mImageInstances = (TLImageInstance**)nlMalloc(mNumImages * sizeof(void*), 8, false);

        for (int i = 0; i < mNumImages; i++)
        {
            nlSNPrintf(buf, 64, "screen%d", i);

            s0 = 0;
            h1 = 0;
            s1 = 0;
            h3 = 0;
            s2 = 0;
            h5 = 0;

            unsigned long hash = nlStringLowerHash(buf);
            s3 = hash;
            s4 = hash;
            hash = nlStringLowerHash("Layer");
            s5 = hash;
            s6 = hash;
            hash = nlStringLowerHash("Slide1");
            s7 = hash;
            s8 = hash;

            union
            {
                FindImageByValue byValue;
                FindImageByRef byRef;
            } f2;
            f2.byValue = FEFinder<TLImageInstance, 2>::Find<FEPresentation>;
            mImageInstances[i] = f2.byValue(
                pPresentation,
                (InlineHasher)s8,
                (InlineHasher)s6,
                (InlineHasher)s4,
                (InlineHasher)h5,
                (InlineHasher)h3,
                (InlineHasher)h1);

            if (i == 0)
            {
                colour.c[0] = 0;
                colour.c[1] = 0;
                colour.c[2] = 0;
                colour.c[3] = 0xFF;
            }
            else
            {
                colour.c[0] = 0xFF;
                colour.c[1] = 0xFF;
                colour.c[2] = 0xFF;
                colour.c[3] = 0;
            }
            mImageInstances[i]->SetAssetColour(colour);
        }

        t0 = 0;
        h1 = 0;
        t1 = 0;
        h3 = 0;
        t2 = 0;
        h5 = 0;

        {
            unsigned long hash = nlStringLowerHash("whitebackground");
            t3 = hash;
            t4 = hash;
            hash = nlStringLowerHash("Layer");
            t5 = hash;
            t6 = hash;
            hash = nlStringLowerHash("Slide1");
            t7 = hash;
            t8 = hash;
        }

        union
        {
            FindImageByValue byValue;
            FindImageByRef byRef;
        } f3;
        f3.byValue = FEFinder<TLImageInstance, 2>::Find<FEPresentation>;
        mCurrentImageInstance = f3.byValue(
            pPresentation,
            (InlineHasher)t8,
            (InlineHasher)t6,
            (InlineHasher)t4,
            (InlineHasher)h5,
            (InlineHasher)h3,
            (InlineHasher)h1);

        mFadeState = FS_FADE_IN_INIT;
        mCurrentImage = 0;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x800BBF04 | size: 0x510
 */
void CrossFaderScene::Update(float fDeltaT)
{
    nlColour colWhite = { 0xFF, 0xFF, 0xFF, 0xFF };
    nlColour colTransparent = { 0xFF, 0xFF, 0xFF, 0x00 };

    BaseSceneHandler::Update(fDeltaT);

    switch (mFadeState)
    {
    case FS_FADE_IN_INIT:
    {
        for (int i = 0; i < mNumImages; i++)
        {
            if (i == 0)
                mImageInstances[i]->SetAssetColour(colWhite);
            else
                mImageInstances[i]->SetAssetColour(colTransparent);
        }
        mCurrentImageInstance->SetAssetColour(colWhite);
        mAlpha = 255.0f;
        mFadeState = FS_FADE_IN;
        break;
    }
    case FS_FADE_IN:
    {
        mAlpha = mAlpha - 510.0f * fDeltaT;
        if (mAlpha <= 0.0f)
        {
            mAlpha = 0.0f;
            mFadeState = FS_WAIT;
        }
        nlColour col = { 0xFF, 0xFF, 0xFF, 0x00 };
        col.c[3] = (u8)(int)mAlpha;
        mCurrentImageInstance->SetAssetColour(col);
        break;
    }
    case FS_WAIT:
    {
        static bool triggeraudioload;
        static s8 init;
        if (!init)
        {
            triggeraudioload = true;
            init = 1;
        }
        if (triggeraudioload)
        {
            switch (mCurrentImage)
            {
            case 2:
                nlSingleton<GameInfoManager>::Instance()->mUserInfo.mAudioOptions.ForceApplySettings(true);
                break;
            case 0:
            {
                AudioLoader::LoadNintendoDialogueGroup(true);
                const char* nintendoSounds[] = {
                    "sfx_nintendo_mario", "sfx_nintendo_wario", "sfx_nintendo_luigi", "sfx_nintendo_waluigi", "sfx_nintendo_peach", "sfx_nintendo_daisy", NULL, NULL, "sfx_nintendo_toad", NULL, NULL, NULL, NULL
                };
                const char* sound;
                do
                {
                    sound = nintendoSounds[nlRandom(13, &nlDefaultSeed)];
                } while (sound == NULL);
                FEAudio::PlayAnimAudioEvent(sound, false);
                AudioLoader::LoadNLGDialogueGroup(true);
                AudioLoader::ReadEntireSampleFileIntoMem(false);
                break;
            }
            case 1:
            {
                const char* nlgSounds[] = {
                    "sfx_nlg_mario", "sfx_nlg_wario", "sfx_nlg_luigi", "sfx_nlg_waluigi", "sfx_nlg_peach", "sfx_nlg_daisy", NULL, NULL, "sfx_nlg_toad", NULL, NULL, NULL, NULL
                };
                const char* sound;
                do
                {
                    sound = nlgSounds[nlRandom(13, &nlDefaultSeed)];
                } while (sound == NULL);
                FEAudio::PlayAnimAudioEvent(sound, false);
                break;
            }
            }
            triggeraudioload = false;
        }
        mTimer += fDeltaT;
        if (mCurrentImage == 2)
        {
            static bool LoadIsDone;
            static s8 init;
            if (!init)
            {
                LoadIsDone = false;
                init = 1;
            }
            if (!PlatAudio::IsEntireSampleFileInMem())
            {
                if (!LoadIsDone)
                    break;
            }
            else
            {
                AudioLoader::LoadFEAudioData(false);
                AudioLoader::LoadPermanentSoundGroups(false);
                LoadIsDone = true;
            }
        }
        // PORT: three logos at two seconds each, in front of every measurement.
        if (mTimer >= (PortBenchWantsDemo() ? 0.0f : 2.0f))
        {
            if (mCurrentImage < mNumImages - 1)
                mFadeState = FS_CROSS_FADE;
            else
                mFadeState = FS_FADE_TO_BLACK;
            mTimer = 0.0f;
            mAlpha = 0.0f;
            triggeraudioload = true;
        }
        break;
    }
    case FS_CROSS_FADE:
    {
        mAlpha = mAlpha + 849.0f * fDeltaT;
        if (mAlpha >= 255.0f)
            mAlpha = 255.0f;
        nlColour col = { 0xFF, 0xFF, 0xFF, 0x00 };
        col.c[3] = (u8)(int)mAlpha;
        mCurrentImageInstance->SetAssetColour(col);
        if (mAlpha >= 255.0f)
        {
            mImageInstances[mCurrentImage]->SetAssetColour(colTransparent);
            mCurrentImage++;
            mImageInstances[mCurrentImage]->SetAssetColour(colWhite);
            mFadeState = FS_FADE_IN;
        }
        break;
    }
    case FS_FADE_TO_BLACK:
    {
        mAlpha = mAlpha + 510.0f * fDeltaT;
        if (mAlpha >= 255.0f)
            mAlpha = 255.0f;
        nlColour col = { 0xFF, 0xFF, 0xFF, 0x00 };
        col.c[3] = (u8)(int)mAlpha;
        mCurrentImageInstance->SetAssetColour(col);
        if (mAlpha >= 255.0f)
        {
            f32 timer = mFadeToBlackTimer + fDeltaT;
            mFadeToBlackTimer = timer;
            if (timer >= 0.2f)
            {
                nlSingleton<GameSceneManager>::Instance()->PopEntireStack();
                nlSingleton<GameSceneManager>::Instance()->Push(SCENE_INTRO_MOVIE, SCREEN_NOTHING, false);
                mFadeToBlackTimer = 0.0f;
            }
        }
        break;
    }
    }
}
