#include "Game/SH/SHSaveLoad.h"

#include "dolphin/card.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/BaseGameSceneManager.h"
#include "Game/GameInfo.h"
#include "Game/GameSceneManager.h"
#include "Game/DB/SaveLoad.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/tlSlide.h"
#include "Game/FE/feInput.h"
#include "Game/FE/fePopupMenu.h"
#include "Game/FE/feSceneManager.h"
#include "Game/FE/tlTextInstance.h"
#include "Game/ResetTask.h"
#include "Game/SH/SHMoviePlayer.h"
#include "Game/Sys/gcmemcard.h"
#include "Game/TrophyTextures.h"
#include "NL/nlConfig.h"
#include "types.h"

extern bool g_e3_Build;

bool SaveLoadScene::mLastSaveLoadSuccess;
bool SaveLoadScene::mUltimateGoalIsToSave;
static u8 WasCardRemoved;
static bool PreviousNoCardInSlotState;
SaveLoadScene* SaveLoadScene::mInstance;
static int gSceneTypeStackDepth;
static float gSceneTime;
static bool gSaveLoadStarted;
static bool gSaveLoadFinished;
static bool gCallbackMade;
static bool gIgnoreMinWait;
static bool gContinueWithoutOperation;
static float gRetryTimerDelay;
static bool gSaveLoadEnabled = true;
bool SaveLoadScene::mIsFirstTimeAboutIPL = true;
static long gResult = -1;
static SaveLoadScene::eSaveLoad gSceneTypeStack[4];

static void CheckResults();

/**
 * Offset/Address/Size: 0x28D0 | 0x800B2E58 | size: 0x14
 */
bool DidContinueWithoutOperation()
{
    return (gContinueWithoutOperation == true);
}

/**
 * Offset/Address/Size: 0x28C4 | 0x800B2E4C | size: 0xC
 */
void ResetEnableSaveLoadFlag()
{
    gSaveLoadEnabled = true;
}

static SaveLoadScene::eSaveLoad GetSceneType()
{
    return gSceneTypeStack[gSceneTypeStackDepth - 1];
}

static void PushSceneType(SaveLoadScene::eSaveLoad type)
{
    gSceneTypeStack[gSceneTypeStackDepth++] = type;
    gSaveLoadStarted = false;
    gSaveLoadFinished = false;
    gCallbackMade = false;
    gSceneTime = 0.0f;
    ResetTask::s_resetPaused = (type == SaveLoadScene::ST_SAVE);
}

static void PopSceneType()
{
    int stackIndex = --gSceneTypeStackDepth;
    gSaveLoadStarted = false;
    SaveLoadScene::eSaveLoad prevScene = gSceneTypeStack[stackIndex];
    gSaveLoadFinished = false;
    if (prevScene == 0)
    {
        ResetTask::s_resetPaused = true;
    }
    else
    {
        ResetTask::s_resetPaused = false;
    }
}

static bool IsFormatSceneOnStack()
{
    for (int i = 0; i <= gSceneTypeStackDepth - 1; ++i)
    {
        if (gSceneTypeStack[i] == SaveLoadScene::ST_FORMAT)
        {
            return true;
        }
    }

    return false;
}

static const char* port_CardOpName(SaveLoadScene::eSaveLoad type)
{
    switch (type)
    {
    case SaveLoadScene::ST_SAVE: return "save";
    case SaveLoadScene::ST_LOAD: return "load";
    case SaveLoadScene::ST_GAMESAVEIDTEST: return "idcheck";
    case SaveLoadScene::ST_DELETE: return "delete";
    case SaveLoadScene::ST_FORMAT: return "format";
    case SaveLoadScene::ST_ASK_SAVE: return "exists";
    case SaveLoadScene::ST_ASK_LOAD: return "askload";
    case SaveLoadScene::ST_CHECKING: return "checking";
    case SaveLoadScene::ST_ABOUT_AUTOSAVE: return "aboutautosave";
    case SaveLoadScene::ST_CONFIRM_FORMAT: return "confirmformat";
    case SaveLoadScene::ST_SHOULD_LOAD_OR_SAVE: return "shouldloadorsave";
    default: return "none";
    }
}

/**
 * Offset/Address/Size: 0x28B4 | 0x800B2E3C | size: 0x10
 */
void SaveLoadCallback(long result)
{
    OSReport("[card] callback op=%s result=%d\n",
             port_CardOpName(gSceneTypeStackDepth > 0 ? GetSceneType()
                                                      : SaveLoadScene::ST_INVALID),
             (int)result);
    gResult = result;
    gCallbackMade = true;
}

/**
 * Offset/Address/Size: 0x2878 | 0x800B2E00 | size: 0x3C
 */
void ContinueWithoutSavingCB()
{
    gSceneTypeStackDepth = 1;
    SaveLoadScene* instance = SaveLoadScene::mInstance;
    gIgnoreMinWait = true;
    gSaveLoadFinished = true;
    gSaveLoadStarted = true;
    gSaveLoadEnabled = false;
    gContinueWithoutOperation = true;
    SaveLoadScene::mLastSaveLoadSuccess = false;
    instance->ShowText(false);
}

/**
 * Offset/Address/Size: 0x2840 | 0x800B2DC8 | size: 0x38
 */
void ContinueWithoutLoadingCB()
{
    gSceneTypeStackDepth = 1;
    SaveLoadScene* instance = SaveLoadScene::mInstance;
    gIgnoreMinWait = true;
    gSaveLoadFinished = true;
    gSaveLoadStarted = true;
    gContinueWithoutOperation = true;
    SaveLoadScene::mLastSaveLoadSuccess = false;
    instance->ShowText(false);
}

/**
 * Offset/Address/Size: 0x27DC | 0x800B2D64 | size: 0x64
 */
void ContinueLoadingCB()
{
    gCallbackMade = false;
    PopSceneType();
    PushSceneType(SaveLoadScene::ST_LOAD);
}

/**
 * Offset/Address/Size: 0x25E0 | 0x800B2B68 | size: 0x1FC
 */
void RetryCB()
{
    gSceneTypeStackDepth = 0;

    switch (SaveLoadScene::mInstance->mSaveLoadMode)
    {
    case SaveLoadScene::SLM_AT_BOOT:
        PushSceneType(SaveLoadScene::ST_SHOULD_LOAD_OR_SAVE);
        break;

    case SaveLoadScene::SLM_SAVING:
        PushSceneType(SaveLoadScene::ST_SAVE);
        PushSceneType(SaveLoadScene::ST_GAMESAVEIDTEST);
        break;

    case SaveLoadScene::SLM_LOADING:
        PushSceneType(SaveLoadScene::ST_LOAD);
        break;

    case SaveLoadScene::SLM_ASK_BEFORE_SAVING:
        PushSceneType(SaveLoadScene::ST_ASK_SAVE);
        break;

    case SaveLoadScene::SLM_ASK_BEFORE_LOADING:
        PushSceneType(SaveLoadScene::ST_ASK_LOAD);
        break;
    }

    gSceneTypeStack[gSceneTypeStackDepth++] = SaveLoadScene::ST_CHECKING;
    gCallbackMade = false;
    ResetTask::s_resetPaused = false;
    gSaveLoadStarted = true;
    gSaveLoadFinished = true;
    gIgnoreMinWait = false;
    gSceneTime = 0.0f;
    gContinueWithoutOperation = false;

    SaveLoadScene::mInstance->ShowText(true);

    gRetryTimerDelay = 1.0f;
    SaveLoadScene::mInstance->SceneCreated();
}

/**
 * Offset/Address/Size: 0x25A0 | 0x800B2B28 | size: 0x40
 */
void DeleteFileCB()
{
    PushSceneType(SaveLoadScene::ST_DELETE);
}

/**
 * Offset/Address/Size: 0x2540 | 0x800B2AC8 | size: 0x60
 */
void FormatConfirmCB()
{
    SaveLoad::RememberCurrentMemCardSerialID(0);
    PushSceneType(SaveLoadScene::ST_CONFIRM_FORMAT);
    gSceneTime = 999.9f;
}

/**
 * Offset/Address/Size: 0x2500 | 0x800B2A88 | size: 0x40
 */
void FormatCB()
{
    PushSceneType(SaveLoadScene::ST_FORMAT);
}

static void DiffCardProceedAnywayCB()
{
    SaveLoad::RememberCurrentMemCardSerialID(0);
    gSceneTypeStackDepth = 1;
    gSaveLoadStarted = false;
    gSaveLoadFinished = true;
}

/**
 * Offset/Address/Size: 0x24E0 | 0x800B2A68 | size: 0x20
 */
void ManageMemCardCB()
{
    ResetTask::s_ResetMode = 1;
    ResetTask::s_ResetState = (ResetTask::s_ResetState == RS_RUNNING) ? RS_STARTRESET : ResetTask::s_ResetState;
}

/**
 * Offset/Address/Size: 0x247C | 0x800B2A04 | size: 0x64
 */
void OverwriteFileAndContinueCB()
{
    gCallbackMade = false;
    int stackIndex = --gSceneTypeStackDepth;
    gSaveLoadStarted = false;
    gSaveLoadFinished = false;
    ResetTask::s_resetPaused = (gSceneTypeStack[stackIndex] == 0);
    gSceneTypeStackDepth = stackIndex + 1;
    gSceneTypeStack[stackIndex] = SaveLoadScene::ST_SAVE;
    gSaveLoadStarted = false;
    gSaveLoadFinished = false;
    gCallbackMade = false;
    gSceneTime = 0.0f;
    ResetTask::s_resetPaused = true;
}

/**
 * Offset/Address/Size: 0x2420 | 0x800B29A8 | size: 0x5C
 */
void CreateFileAndSaveCB()
{
    gCallbackMade = false;
    int stackIndex = --gSceneTypeStackDepth;
    SaveLoadScene* instance = SaveLoadScene::mInstance;
    gSaveLoadStarted = false;
    gSaveLoadFinished = false;
    ResetTask::s_resetPaused = (gSceneTypeStack[stackIndex] == 0);
    instance->SetupForAboutAutoSave();
}

/**
 * Offset/Address/Size: 0x15E4 | 0x800B1B6C | size: 0xE3C
 */
static void CheckResults()
{
    SaveLoadScene::eSaveLoad sceneType = GetSceneType();

    if (sceneType == SaveLoadScene::ST_FORMAT || sceneType == SaveLoadScene::ST_DELETE)
    {
        long result = gResult;
        if (result != 0 && result != -1 && (sceneType != SaveLoadScene::ST_FORMAT || result != -3) && result != -1001 && result != -2)
        {
            gResult = -5;
        }
    }
    else if (sceneType == SaveLoadScene::ST_CONFIRM_FORMAT)
    {
        SaveLoad::RememberCurrentMemCardSerialID(0);
        FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

        pPopup->Create(POPUP_MEMCARD_CONFIRM_FORMAT, ContinueWithoutSavingCB, FormatCB);
        return;
    }

    SaveLoadScene::mInstance->ShowText(true);

    switch (gResult)
    {
    case 0:
    {
        sceneType = GetSceneType();

        if (sceneType == SaveLoadScene::ST_ASK_SAVE)
        {
            FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

            pPopup->Create(POPUP_MEMCARD_ASK_SAVE_OVERWRITE, ContinueWithoutSavingCB, OverwriteFileAndContinueCB);
            return;
        }

        if (sceneType == SaveLoadScene::ST_ASK_LOAD)
        {
            FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

            pPopup->Create(POPUP_MEMCARD_ASK_LOAD_OVERWRITE, ContinueWithoutLoadingCB, ContinueLoadingCB);
            return;
        }

        gSaveLoadFinished = true;
        return;
    }

    case -3:
    {
        FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

        pPopup->Create(POPUP_NO_MEMCARD, RetryCB, ContinueWithoutSavingCB);
        return;
    }

    case -9:
    case -8:
    {
        FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

        sceneType = GetSceneType();
        if (sceneType != SaveLoadScene::ST_ASK_SAVE)
        {
            if (sceneType < SaveLoadScene::ST_ASK_SAVE)
            {
                if (sceneType >= SaveLoadScene::ST_DELETE || sceneType < SaveLoadScene::ST_SAVE)
                {
                    return;
                }
            }
            else
            {
                switch (sceneType)
                {
                case SaveLoadScene::ST_SHOULD_LOAD_OR_SAVE:
                    break;
                default:
                    return;
                }
            }
        }

        if (SaveLoadScene::mInstance->mSaveLoadMode == SaveLoadScene::SLM_AT_BOOT)
        {
            pPopup->Create(POPUP_NOT_ENOUGH_SPACE_CANMANAGE, RetryCB, ContinueWithoutSavingCB, ManageMemCardCB);
            return;
        }
        else
        {
            pPopup->Create(POPUP_NOT_ENOUGH_SPACE, RetryCB, ContinueWithoutSavingCB);
            return;
        }
    }

    case -2:
    {
        FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

        pPopup->Create(POPUP_WRONG_DEVICE, RetryCB, ContinueWithoutSavingCB);
        return;
    }

    case -6:
    {
        FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

        pPopup->Create(POPUP_MEMCARD_CORRUPTED, RetryCB, ContinueWithoutSavingCB, FormatConfirmCB);
        return;
    }

    case -13:
    {
        FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

        pPopup->Create(POPUP_MEMCARD_WRONGFORMAT, RetryCB, ContinueWithoutSavingCB, FormatConfirmCB);
        return;
    }

    case -5:
    {
        FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

        pPopup->Create(POPUP_MEMCARD_DAMAGED, RetryCB, ContinueWithoutSavingCB);
        return;
    }

    case -1000:
    case -11:
    {
        SaveLoad::RememberCurrentMemCardSerialID(0);
        FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

        pPopup->Create(POPUP_FILE_CORRUPTED, RetryCB, ContinueWithoutSavingCB, DeleteFileCB);
        return;
    }

    case -4:
    {
        sceneType = GetSceneType();

        if (sceneType == SaveLoadScene::ST_LOAD)
        {
            SaveLoad::RememberCurrentMemCardSerialID(0);
            FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

            pPopup->Create(POPUP_MEMCARD_ASK_SAVE_NO_FILE, CreateFileAndSaveCB, ContinueWithoutSavingCB);
            return;
        }

        if (sceneType == SaveLoadScene::ST_ASK_SAVE || sceneType == SaveLoadScene::ST_ASK_LOAD)
        {
            SaveLoad::RememberCurrentMemCardSerialID(0);
            FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

            pPopup->Create(POPUP_MEMCARD_ASK_SAVE_NO_FILE, CreateFileAndSaveCB, ContinueWithoutSavingCB);
            return;
        }

        if (sceneType == SaveLoadScene::ST_SHOULD_LOAD_OR_SAVE)
        {
            if (!SaveLoad::HasEnoughFreeSpace(0))
            {
                gResult = -9;
                CheckResults();
                return;
            }

            SaveLoad::RememberCurrentMemCardSerialID(0);
            FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

            pPopup->Create(POPUP_MEMCARD_ASK_SAVE_NO_FILE, CreateFileAndSaveCB, ContinueWithoutSavingCB);
            return;
        }

        if (sceneType == SaveLoadScene::ST_GAMESAVEIDTEST)
        {
            gSaveLoadFinished = true;
        }

        return;
    }

    case -1001:
    {
        FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

        pPopup->Create(POPUP_NOTSAMECARD, RetryCB, ContinueWithoutSavingCB);
        return;
    }

    case -10:
    case -7:
        return;

    default:
        return;
    }
}

static void port_IssueResult(long issued)
{
    if (!gCallbackMade)
        gResult = issued;
}

static void TrySaveLoad()
{
    gSaveLoadFinished = false;
    gCallbackMade = false;

    switch (GetSceneType())
    {
    case SaveLoadScene::ST_SAVE:
        port_IssueResult(SaveLoad::StartSave(0, SaveLoadCallback));
        break;
    case SaveLoadScene::ST_LOAD:
        port_IssueResult(SaveLoad::StartLoad(0, SaveLoadCallback, true, false));
        break;
    case SaveLoadScene::ST_GAMESAVEIDTEST:
        port_IssueResult(SaveLoad::StartMemoryCardIDCheck(0, SaveLoadCallback));
        break;
    case SaveLoadScene::ST_DELETE:
        port_IssueResult(SaveLoad::StartDelete(0, SaveLoadCallback));
        break;
    case SaveLoadScene::ST_FORMAT:
        port_IssueResult(SaveLoad::StartFormat(0, SaveLoadCallback));
        break;
    case SaveLoadScene::ST_ASK_SAVE:
        port_IssueResult(SaveLoad::StartFileExistsCheck(0, SaveLoadCallback));
        break;
    case SaveLoadScene::ST_ASK_LOAD:
        port_IssueResult(SaveLoad::StartLoad(0, SaveLoadCallback, false, false));
        break;
    case SaveLoadScene::ST_CHECKING:
    case SaveLoadScene::ST_ABOUT_AUTOSAVE:
        break;
    case SaveLoadScene::ST_CONFIRM_FORMAT:
        gSaveLoadFinished = true;
        gCallbackMade = false;
        break;
    case SaveLoadScene::ST_SHOULD_LOAD_OR_SAVE:
        port_IssueResult(SaveLoad::StartFileExistsCheck(0, SaveLoadCallback));
        break;
    }
}

static bool NoCardInSlot()
{
    MemCard* memCard = g_MemCards[0];
    s32 result = CARDProbeEx(memCard->m_Slot, &memCard->m_CardInfo.CardSize, &memCard->m_CardInfo.SectorSize);

    if (result != CARD_RESULT_READY)
    {
        memCard->m_State = IS_IDLE;
        memCard->m_CardState = CS_IDLE;
    }

    return result == CARD_RESULT_NOCARD;
}

/**
 * Offset/Address/Size: 0x1408 | 0x800B1990 | size: 0x1DC
 */
static bool PushNoCardMessage()
{
    if (NoCardInSlot() || WasCardRemoved)
    {
        BaseSceneHandler* handler = nlSingleton<GameSceneManager>::Instance()->GetCurrentScene();

        if (nlSingleton<GameSceneManager>::Instance()->GetSceneType(handler) == SCENE_POPUP_MENU)
        {
            if (((FEPopupMenu*)handler)->mType == POPUP_NO_MEMCARD)
            {
                WasCardRemoved = 0;
                return false;
            }

            if (!nlSingleton<FESceneManager>::Instance()->AreAllScenesValid())
            {
                return false;
            }

            nlSingleton<GameSceneManager>::Instance()->Pop();
            nlSingleton<FESceneManager>::Instance()->ForceImmediateStackProcessing();
        }

        FEPopupMenu* popup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);

        popup->Create(
            POPUP_NO_MEMCARD,
            RetryCB,
            (GetSceneType() == SaveLoadScene::ST_LOAD) ? ContinueWithoutLoadingCB : ContinueWithoutSavingCB);

        return true;
    }

    return false;
}

/**
 * Offset/Address/Size: 0x11C8 | 0x800B1750 | size: 0x240
 */
SaveLoadScene::SaveLoadScene(SaveLoadScene::eSaveLoadMode saveLoadMode)
{
    m_displayText = NULL;
    mNextScene = SCENE_INVALID;
    mIsAutoSaving = false;
    mIsFirstTimeCreateFile = true;
    mButtonComponent = NULL;
    mSaveLoadMode = saveLoadMode;

    gSceneTypeStackDepth = 0;

    int savingorloading = SCENE_SAVE;

    switch (mSaveLoadMode)
    {
    case SLM_SAVING:
        PushSceneType(SaveLoadScene::ST_SAVE);
        PushSceneType(SaveLoadScene::ST_GAMESAVEIDTEST);
        break;

    case SLM_LOADING:
        savingorloading = SCENE_LOAD;
        PushSceneType(SaveLoadScene::ST_LOAD);
        break;

    case SLM_ASK_BEFORE_SAVING:
        PushSceneType(SaveLoadScene::ST_ASK_SAVE);
        break;

    case SLM_ASK_BEFORE_LOADING:
        savingorloading = SCENE_LOAD;
        PushSceneType(SaveLoadScene::ST_ASK_LOAD);
        break;

    case SLM_AT_BOOT:
        savingorloading = SCENE_LOAD;
        PushSceneType(SaveLoadScene::ST_SHOULD_LOAD_OR_SAVE);
        break;
    }

    gSceneTypeStack[gSceneTypeStackDepth++] = SaveLoadScene::ST_CHECKING;
    gCallbackMade = false;
    ResetTask::s_resetPaused = false;
    gSaveLoadStarted = true;
    gSaveLoadFinished = true;
    gIgnoreMinWait = false;
    gSceneTime = 0.0f;
    gRetryTimerDelay = 1.0f;
    gContinueWithoutOperation = false;

    g_pFEInput->PushExclusiveInputLock(this, savingorloading);

    mInstance = this;
}

/**
 * Offset/Address/Size: 0x1124 | 0x800B16AC | size: 0xA4
 */
SaveLoadScene::~SaveLoadScene()
{
    g_pFEInput->PopExclusiveInputLock(this);
    mInstance = NULL;

    if (mButtonComponent != NULL)
    {
        delete mButtonComponent;
        mButtonComponent = NULL;
    }

    mIsFirstTimeAboutIPL = false;
}

/**
 * Offset/Address/Size: 0xF0C | 0x800B1494 | size: 0x218
 */
void SaveLoadScene::SceneCreated()
{
    FEPresentation* pres = m_pFEScene->m_pFEPackage->GetPresentation();
    pres->SetActiveSlide("Slide1");

    m_displayText = FEFinder<TLTextInstance, 3>::Find<FEPresentation>(
        pres,
        InlineHasher(nlStringLowerHash("Slide1")),
        InlineHasher(nlStringLowerHash("Layer")),
        InlineHasher(nlStringLowerHash("Text")));

    if (mIsAutoSaving)
    {
        pres->SetActiveSlide("Slide2");
    }

    mAboutAutoSaveSlide = FEFinder<TLSlide, 0>::Find<FEPresentation>(
        pres,
        InlineHasher(nlStringLowerHash("Slide3")));

    UpdateText();
}

void SaveLoadScene::UpdateText()
{
    TLTextInstance* text = m_displayText;
    if (text != NULL)
    {
        SaveLoadScene::eSaveLoad prevOp = GetSceneType();
        switch ((unsigned)prevOp)
        {
        case SaveLoadScene::ST_SAVE:
        case SaveLoadScene::ST_GAMESAVEIDTEST:
            text->m_LocStrId = 0xCF941DC9;
            text->m_OverloadFlags |= 0x8;
            break;
        case SaveLoadScene::ST_LOAD:
            text->m_LocStrId = 0xFAA420FA;
            text->m_OverloadFlags |= 0x8;
            break;
        case SaveLoadScene::ST_FORMAT:
        case SaveLoadScene::ST_CONFIRM_FORMAT:
            text->m_LocStrId = 0x81D26163;
            text->m_OverloadFlags |= 0x8;
            break;
        case SaveLoadScene::ST_DELETE:
            text->m_LocStrId = 0x1A7FDB2D;
            text->m_OverloadFlags |= 0x8;
            break;
        case SaveLoadScene::ST_ASK_SAVE:
        case SaveLoadScene::ST_ASK_LOAD:
        case SaveLoadScene::ST_CHECKING:
        case SaveLoadScene::ST_SHOULD_LOAD_OR_SAVE:
            text->m_LocStrId = 0xE8E70E54;
            text->m_OverloadFlags |= 0x8;
            break;
        case SaveLoadScene::ST_ABOUT_AUTOSAVE:
            break;
        case (SaveLoadScene::eSaveLoad)11:
            text->m_LocStrId = 0xF501447B;
            text->m_OverloadFlags |= 0x8;
            break;
        }
    }
}

/**
 * Offset/Address/Size: 0x8C8 | 0x800B0E50 | size: 0x644
 */
void SaveLoadScene::Update(float fDeltaT)
{
    PreviousNoCardInSlotState = NoCardInSlot();

    if (!g_pFEInput->HasInputLock(this))
    {
        FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::Instance()->GetCurrentScene();
        if (nlSingleton<GameSceneManager>::Instance()->GetSceneType(pPopup) == SCENE_POPUP_MENU)
        {
            PushNoCardMessage();
        }
        return;
    }

    BaseSceneHandler::Update(fDeltaT);
    gSceneTime += fDeltaT;

    if (mIsAutoSaving)
    {
        SaveLoadScene::eSaveLoad sceneType = GetSceneType();
        if (sceneType == (SaveLoadScene::eSaveLoad)11
            || IsFormatSceneOnStack()
        )
        {
            TLSlide* slide = FEFinder<TLSlide, 0>::Find<FEPresentation>(
                m_pFEPresentation, InlineHasher(nlStringLowerHash("Slide1")));
            if (m_pFEPresentation->m_currentSlide != slide)
            {
                m_pFEPresentation->SetActiveSlide(slide);
            }
        }
        else
        {
            TLSlide* slide = FEFinder<TLSlide, 0>::Find<FEPresentation>(
                m_pFEPresentation, InlineHasher(nlStringLowerHash("Slide2")));
            if (m_pFEPresentation->m_currentSlide != slide)
            {
                m_pFEPresentation->SetActiveSlide(slide);
            }
        }
    }

    UpdateText();

    if (ResetTask::s_ResetState == 1)
    {
        if (GetSceneType() == SaveLoadScene::ST_SAVE)
        {
            gIgnoreMinWait = true;
        }
    }

    if (!gIgnoreMinWait)
    {
        float minTime;
        if (GetSceneType() == (SaveLoadScene::eSaveLoad)11)
        {
            minTime = 3.5f;
        }
        else
        {
            minTime = 1.75f;
        }
        if (gSceneTime <= minTime)
        {
            gRetryTimerDelay -= fDeltaT;
            if (gRetryTimerDelay <= 0.0f)
            {
                PushNoCardMessage();
            }
            return;
        }
    }

    SaveLoadScene::eSaveLoad sceneType = GetSceneType();
    if (sceneType == SaveLoadScene::ST_CHECKING || sceneType == (SaveLoadScene::eSaveLoad)11)
    {
        gSaveLoadStarted = true;
        gCallbackMade = false;
        gSaveLoadFinished = true;
    }

    if (IsOnAboutAutoSaveSlide())
    {
        UpdateForAboutToSaveSlide();
        return;
    }

    if (!gSaveLoadStarted)
    {
        gSaveLoadStarted = true;
        TrySaveLoad();

        if (gResult == -1)
        {
            gSaveLoadStarted = false;
            return;
        }

        // PORT: only when the issue itself failed; a delivered completion is the branch below's, on the same pass.
        if (!gCallbackMade && gResult != 0)
        {
            CheckResults();
        }
    }

    if (gCallbackMade)
    {
        gCallbackMade = false;
        CheckResults();
        if (gResult == 0)
        {
            if (GetSceneType() == SaveLoadScene::ST_GAMESAVEIDTEST)
            {
                if (SaveLoad::DidGameIDChange())
                {
                    FEPopupMenu* pPopup = (FEPopupMenu*)nlSingleton<GameSceneManager>::s_pInstance
                                              ->Push(SCENE_POPUP_MENU, SCREEN_NOTHING, false);
                    pPopup->Create(POPUP_NOTSAMECARD, RetryCB, ContinueWithoutSavingCB);
                    gSaveLoadFinished = false;
                }
            }
        }
    }

    if (gSaveLoadFinished)
    {
        mLastSaveLoadSuccess = false;
        HandleSaveLoadFinishedResult();
    }
}

/**
 * Offset/Address/Size: 0x7D4 | 0x800B0D5C | size: 0xF4
 */
bool SaveLoadScene::IsIOEnabled()
{
    if (!gSaveLoadEnabled)
    {
        return false;
    }

    if (g_e3_Build)
    {
        return false;
    }

    return GetConfigBool(Config::Global(), "DisableMemCard", false) != true;
}

void SaveLoadScene::ShowText(bool newState)
{
    if (m_displayText != NULL)
    {
        m_displayText->m_bVisible = newState;
    }
}

/**
 * Offset/Address/Size: 0x6D8 | 0x800B0C60 | size: 0xFC
 */
void SaveLoadScene::SetupForAboutAutoSave()
{

    m_pFEPresentation->SetActiveSlide(mAboutAutoSaveSlide);

    if (mButtonComponent == NULL)
    {
        ButtonComponent* ptr = new ((u8*)nlMalloc(sizeof(ButtonComponent), 8, false)) ButtonComponent();
        mButtonComponent = ptr;

        mButtonComponent->mButtonInstance = FEFinder<TLComponentInstance, 4>::Find<TLSlide>(
            mAboutAutoSaveSlide,
            InlineHasher(nlStringLowerHash("Layer")),
            InlineHasher(nlStringLowerHash("buttons")));
    }

    mButtonComponent->SetState(ButtonComponent::BS_A_ONLY);
    mButtonComponent->CentreButtons();

    TLComponentInstance* inst = mButtonComponent->mButtonInstance;
    if (inst != NULL)
    {
        inst->m_bVisible = false;
    }
}

/**
 * Offset/Address/Size: 0x600 | 0x800B0B88 | size: 0xD8
 */
void SaveLoadScene::UpdateForAboutToSaveSlide()
{
    if (PushNoCardMessage())
    {
        SceneCreated();
    }
    else if (g_pFEInput->JustPressed(FE_ALL_PADS, 0x100, false, NULL))
    {
        SceneCreated();

        gSceneTypeStackDepth = 0;
        PushSceneType(SaveLoadScene::ST_SAVE);
    }

    if (mButtonComponent != NULL)
    {
        TLComponentInstance* inst = mButtonComponent->mButtonInstance;
        if (inst != NULL)
        {
            inst->m_bVisible = true;
        }
    }
}

/**
 * Offset/Address/Size: 0x224 | 0x800B07AC | size: 0x3DC
 */
void SaveLoadScene::HandleSaveLoadFinishedResult()
{
    OSReport("[card] finished type=%d result=%d success=%d next=%d\n",
             (int)GetSceneType(), (int)gResult, (gResult == 0) ? 1 : 0,
             (int)mNextScene);
    SaveLoadScene::eSaveLoad sceneType = GetSceneType();

    switch (sceneType)
    {
    case SaveLoadScene::ST_DELETE:
    case SaveLoadScene::ST_FORMAT:
    {
        if (sceneType == SaveLoadScene::ST_FORMAT)
        {
            PopSceneType();
            PushSceneType((SaveLoadScene::eSaveLoad)SCENE_CUP_BACKGROUND);
        }
        else
        {
            PopSceneType();
        }
        break;
    }

    case (SaveLoadScene::eSaveLoad)11:
        PopSceneType();
        break;

    case SaveLoadScene::ST_GAMESAVEIDTEST:
        PopSceneType();
        break;

    case SaveLoadScene::ST_SAVE:
    case SaveLoadScene::ST_LOAD:
    case SaveLoadScene::ST_ASK_SAVE:
    case SaveLoadScene::ST_ASK_LOAD:
    {
        if (mIsAutoSaving)
        {
            nlSingleton<GameSceneManager>::Instance()->Pop();
        }
        else
        {
            if (gContinueWithoutOperation)
            {
                SaveLoadScene::mLastSaveLoadSuccess = false;
                if (mNextScene == SCENE_OPTIONS)
                {
                    if (sceneType != SaveLoadScene::ST_FORMAT)
                    {
                        if (gResult != -8 && gResult != -9)
                        {
                            gSaveLoadEnabled = true;
                        }
                    }
                }
            }
            else
            {
                bool success = (gResult == 0);
                SaveLoadScene::mLastSaveLoadSuccess = success;
                if (success)
                {
                    gSaveLoadEnabled = true;
                }
            }

            BaseSceneHandler* scene = nlSingleton<GameSceneManager>::Instance()->Push(mNextScene, SCREEN_NOTHING, true);

            ShowText(false);

            if (GetSceneType() == SCENE_MARIO_BACKGROUND)
            {
                if (mNextScene != SCENE_LEGAL)
                {
                    eAudioMode currentMode = (eAudioMode)nlSingleton<GameInfoManager>::Instance()->mCurGameAudioSettings.Mode;
                    AudioSettings& opts = nlSingleton<GameInfoManager>::Instance()->GetAudioOptions();
                    eAudioMode memCardMode = (eAudioMode)opts.Mode;
                    bool playMusic = false;
                    if (currentMode == DOLBY || memCardMode == DOLBY)
                    {
                        playMusic = true;
                    }
                    nlSingleton<GameInfoManager>::Instance()->mUserInfo.mAudioOptions.ForceApplySettings(false);
                    if (playMusic)
                    {
                        AudioLoader::PlayFEMenuMusic();
                    }
                }
            }

            if (mNextScene == SCENE_MOVIE_PLAYER)
            {
                ((MoviePlayerScene*)scene)->SetMovieDetails("intromovie.thp", true, false);
                ((MoviePlayerScene*)scene)->mNextScene = SCENE_TITLE;
            }
        }
        break;
    }

    case SaveLoadScene::ST_CHECKING:
    {
        PopSceneType();
        gSceneTime = 0.0f;
        gIgnoreMinWait = false;
        break;
    }

    case SaveLoadScene::ST_CONFIRM_FORMAT:
    {
        int stackIndex = --gSceneTypeStackDepth;
        gSaveLoadFinished = false;
        SaveLoadScene::eSaveLoad prevScene = gSceneTypeStack[stackIndex];
        gSaveLoadStarted = false;
        ResetTask::s_resetPaused = (prevScene == 0);
        gSceneTime = 999.9f;
        gSaveLoadFinished = false;
        break;
    }

    case SaveLoadScene::ST_SHOULD_LOAD_OR_SAVE:
    {
        if (gResult == 0)
        {
            PopSceneType();
            gSaveLoadFinished = false;
            PushSceneType(SaveLoadScene::ST_LOAD);
        }
        else
        {
            nlSingleton<GameSceneManager>::Instance()->Push(mNextScene, SCREEN_NOTHING, true);
        }
        gSaveLoadFinished = false;
        break;
    }

    case SaveLoadScene::ST_ABOUT_AUTOSAVE:
    default:
        break;
    }

    SaveLoad::FreeAllCallbackMemory();
}

/**
 * Offset/Address/Size: 0xB0 | 0x800B0638 | size: 0x174
 */
void SaveLoadScene::StartSaveNow()
{
    if (mInstance == NULL)
    {
        return;
    }

    SaveLoadScene::eSaveLoad sceneType = GetSceneType();
    if (sceneType != SaveLoadScene::ST_SAVE)
    {
        return;
    }

    if (gSaveLoadStarted)
    {
        return;
    }

    if (mInstance->IsOnAboutAutoSaveSlide())
    {
        return;
    }

    gSaveLoadStarted = true;
    TrySaveLoad();
}

bool SaveLoadScene::IsOnAboutAutoSaveSlide()
{
    return m_pFEPresentation->m_currentSlide == mAboutAutoSaveSlide;
}

/**
 * Offset/Address/Size: 0x0 | 0x800B0588 | size: 0xB0
 */
void SaveLoadScene::UpdateCardRemovedFlag()
{
    if (!MemCard::s_InitDone)
    {
        return;
    }

    if (NoCardInSlot())
    {
        u8 currentNoCardState = NoCardInSlot();
        if (PreviousNoCardInSlotState != currentNoCardState)
        {
            WasCardRemoved = 1;
        }
    }
}
