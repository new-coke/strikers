#ifndef _SHCREDITS_H_
#define _SHCREDITS_H_

#include "Game/BaseSceneHandler.h"
#include "Game/Sys/simpleparser.h"
#include "Game/FE/tlTextInstance.h"
#include "Game/FE/feSceneManager.h"
#include "Game/FE/feFinder.h"
#include "Game/GameSceneManager.h"
#include "Game/FE/feMusic.h"

class TLComponentInstance;
class TLImageInstance;

class CreditScene : public BaseSceneHandler
{
public:
    struct CreditParser
    {
        CreditParser()
            : mFileSize(0)
            , mFileData(NULL)
        {
        }
        ~CreditParser()
        {
            if (mFileData != NULL)
            {
                nlFree(mFileData);
                mFileData = NULL;
            }
        }
        /* 0x00 */ unsigned long mFileSize; // size 0x4
        /* 0x04 */ char* mFileData;         // size 0x4
        /* 0x08 */ SimpleParser mParser;    // size 0x514
    }; // total size: 0x51C

    CreditScene();
    ~CreditScene();
    void SceneCreated();
    void Update(float fDeltaT);
    void DisplayFinalMessage();
    void SetupForPhase();
    void GotoNextPhase();
    void SetupForCopyrightMessage();
    void SetupForCredits();
    void SetupForNintendoLogo();
    void SetupForNLGMovie();
    void UpdateForCopyrightMessage(float fDeltaT);
    void UpdateForCredits(float fDeltaT);
    void UpdateForNintendoLogo(float fDeltaT);
    void UpdateForNLGMovie(float fDeltaT);
    TLComponentInstance* GetWhiteFadeComponent();

    static SceneList mNextScene;

    /*  0x01C */ TLTextInstance* m_pTextLines[10]; // offset 0x1C, size 0x28
    /*  0x044 */ bool mLineOnScreen[10];           // offset 0x44, size 0xA
    /*  0x04E */ bool mAreCreditsOver;             // offset 0x4E, size 0x1
    /*  0x04F */ bool mFinalMessageDisplayed;      // offset 0x4F, size 0x1
    /*  0x050 */ bool mFadeStarted;                // offset 0x50, size 0x1
    /*  0x054 */ float mTimeElapsed;               // offset 0x54, size 0x4
    /*  0x058 */ int mPhase;                       // offset 0x58, size 0x4
    /*  0x05C */ CreditParser mCreditParser;       // offset 0x5C, size 0x51C
    /*  0x578 */ unsigned short mStrings[10][64];  // offset 0x578, size 0x500
}; // total size: 0xA78

#endif // _SHCREDITS_H_
