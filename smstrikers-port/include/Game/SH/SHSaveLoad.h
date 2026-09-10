#ifndef _SHSAVELOAD_H_
#define _SHSAVELOAD_H_

#include "Game/BaseGameSceneManager.h"
#include "Game/FE/feButtonComponent.h"

void CreateFileAndSaveCB();
void OverwriteFileAndContinueCB();
void ManageMemCardCB();
void FormatCB();
void FormatConfirmCB();
void DeleteFileCB();
void RetryCB();
void ContinueLoadingCB();
void ContinueWithoutLoadingCB();
void ContinueWithoutSavingCB();
void SaveLoadCallback(long result);
void ResetEnableSaveLoadFlag();
bool DidContinueWithoutOperation();

class SaveLoadScene : public BaseSceneHandler
{
public:
    enum eSaveLoad
    {
        ST_INVALID = -1,
        ST_SAVE = 0,
        ST_LOAD = 1,
        ST_GAMESAVEIDTEST = 2,
        ST_DELETE = 3,
        ST_FORMAT = 4,
        ST_ASK_SAVE = 5,
        ST_ASK_LOAD = 6,
        ST_CHECKING = 7,
        ST_ABOUT_AUTOSAVE = 8,
        ST_CONFIRM_FORMAT = 9,
        ST_SHOULD_LOAD_OR_SAVE = 10,
    };

    enum eSaveLoadMode
    {
        SLM_INVALID = -1,
        SLM_AT_BOOT = 0,
        SLM_SAVING = 1,
        SLM_ASK_BEFORE_SAVING = 2,
        SLM_LOADING = 3,
        SLM_ASK_BEFORE_LOADING = 4,
    };

    SaveLoadScene(SaveLoadScene::eSaveLoadMode saveLoadMode);
    ~SaveLoadScene();
    void SceneCreated();
    void UpdateText();
    void Update(float fDeltaT);
    static bool IsIOEnabled();
    void ShowText(bool newState);
    void SetupForAboutAutoSave();
    void UpdateForAboutToSaveSlide();
    void HandleSaveLoadFinishedResult();
    bool IsOnAboutAutoSaveSlide();

    static void StartSaveNow();
    static void UpdateCardRemovedFlag();

    /* 0x1C */ TLTextInstance* m_displayText;
    /* 0x20 */ SceneList mNextScene;
    /* 0x24 */ bool mIsAutoSaving;
    /* 0x25 */ bool mIsFirstTimeCreateFile;
    /* 0x28 */ TLSlide* mAboutAutoSaveSlide;
    /* 0x2C */ ButtonComponent* mButtonComponent;
    /* 0x30 */ eSaveLoadMode mSaveLoadMode;

    static SaveLoadScene* mInstance;
    static bool mLastSaveLoadSuccess;
    static bool mUltimateGoalIsToSave;
    static bool mIsFirstTimeAboutIPL;
}; // total size: 0x34

#endif // _SHSAVELOAD_H_
