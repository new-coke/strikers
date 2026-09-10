#include "Game/Render/Presentation.h"
#include "port/aspect.h"
#include "Game/BeginFrameTask.h"
#include "Game/FixedUpdateTask.h"\n#include <stdlib.h>
#include "Game/ParticleUpdateTask.h"
#include "Game/WorldManager.h"
#include "Game/Drawable/DrawableObj.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/Audio/AudioStream.h"
#include "Game/Audio/CrowdMood.h"
#include "Game/Audio/StreamTrack.h"
#include "Game/Effects/EffectsGroup.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/Effects/EmissionController.h"
#include "Game/AI/AIPad.h"
#include "Game/CharacterTemplate.h"
#include "Game/Debug/ShapeRender.h"
#include "Game/EventDataTypes.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feManager.h"
#include "Game/GameInfo.h"
#include "Game/NisPlayer.h"
#include "Game/ReplayChoreo.h"
#include "Game/Render/CrowdManager.h"
#include "Game/Render/ElectricFence.h"
#include "Game/Render/Jumbotron.h"
#include "Game/Render/Wiper.h"
#include "NL/gl/gl.h"
#include "NL/nlConfig.h"
#include "NL/nlFile.h"
#include "NL/nlDebug.h"
#include "NL/nlString.h"
#include "NL/globalpad.h"
#include "Game/TrophyInfo.h"

extern AudioStreamTrack::TrackManagerBase* g_pTrackManager;

// PORT: this file declared its own cGame, padded to console offsets.
#include "Game/Game.h"

class GoalOverlay
{
public:
    void SetHighlightNumber(int);
    void DoMatchEndOverlay();
    void DoCupWinOverlay();
};

char trophyFileName[0xFF] = "";
static const char* idleFun = "Idle";
static bool loopPresentation;

extern unsigned long cupTrophyHash;

static void CupWinStingerDone();
static void ReadTrophyModel(void*, unsigned long, void*);
static void ReadTrophyTexture(void*, unsigned long, void*);

static inline bool IsDuringGamePauseState()
{
    return !FrontEnd::m_bGameOver && nlTaskManager::m_pInstance->m_CurrState == 1;
}

int nlSNPrintf(char*, unsigned long, const char*, ...);

void Presentation::RaiseEvent(const char* type, const char* param)
{
    NISData* data = new (/* PORT: m_data is at 0x18 here. */ (u8*)&g_pEventManager->CreateValidEvent(0x56, 0x20)->m_data) NISData();
    data->Type = type;
    data->Param = param;
}

#include "Presentation_interp.cpp"

Presentation::Presentation()
    : InterpreterCore(10)
{
    mByPassWasSkipped = false;
    mSkipPressed = false;
    mInsideByPass = false;
    mByPassing = false;
    mWaitingForCharacterDirectionSince = 0.0f;
    mTimeInFunction = 0.0f;
    mDisplayLetterBox = 0.0f;
    mLetterBoxDuration = 0.0f;
    mLetterBoxEnabled = false;
    mOverlayDelay = 0.0f;
    mOverlayDisplayLength = 0.0f;
    mOverlayDisplayed = false;
    mOverlayToDisplay = SCENE_INVALID;
    mInterruptWipe = NULL;
    mUseInterruptWipe = NULL;
    mQueuedFunction = NULL;
    mGoalQuality = HIGHLIGHT_QUALITY_EMPTY;
    unsigned long fileSize = 0;
    void* byteCode = nlLoadEntireFile("art/presentation/presentation.byte_code", &fileSize, 0x20, AllocateStart);
    LoadByteCode(byteCode);
    nlStrNCpy<char>(mCurrentFunction, idleFun, 64);
    mIsAllowedToSkip[0] = true;
    mIsAllowedToSkip[1] = true;
    mIsAllowedToSkip[2] = true;
    mIsAllowedToSkip[3] = true;
}

/**
 * Offset/Address/Size: 0x1E58 | 0x8012663C | size: 0x114
 */
Presentation& Presentation::Instance()
{
    static Presentation instance;
    return instance;
}

unsigned long cupTrophyHash;

/**
 * Offset/Address/Size: 0x1D18 | 0x801264FC | size: 0x140
 */
static void ReadTrophyTexture(void* data, unsigned long size, void* userData)
{
    Presentation& presentation = Presentation::Instance();
    presentation.mTrophyTextureLoaded = true;
    glEndLoadTextureBundle(data, size);
}

/**
 * Offset/Address/Size: 0x1CA8 | 0x8012648C | size: 0x70
 */
static void ReadTrophyModel(void* data, unsigned long size, void* userData)
{
    unsigned long modelSize = 0;
    glModel* model = glEndLoadModel(data, size, &modelSize);

    int numModelsLoaded = 0;
    WorldManager::s_World->LoadGeometry(model, modelSize, true, true, &cupTrophyHash, &numModelsLoaded, true);

    DrawableObject* trophy = WorldManager::s_World->FindDrawableObject(cupTrophyHash);
    trophy->m_uObjectFlags &= ~1;
}

static inline const char* GetGimmeCupTrophyName()
{
    Config& cfg = Config::Global();
    TagValuePair& tvp = cfg.FindTvp("gimme_cup_trophy");
    if (tvp.tag == NULL)
    {
        cfg.Set("gimme_cup_trophy", "FlowerCup");
        return "FlowerCup";
    }
    return tvp.Get<const char*>();
}

/**
 * Offset/Address/Size: 0x1848 | 0x8012602C | size: 0x460
 */
void Presentation::LoadTrophyModel()
{
    cupTrophyHash = 0;

    bool hasCupOverride = Config::Global().Exists("gimme_cup_trophy");
    if (!hasCupOverride)
    {
        if (!nlSingleton<GameInfoManager>::Instance()->IsPossibleCupMatch())
        {
            return;
        }
    }

    cupTrophyHash = 1;

    BasicString<char, Detail::TempStringAllocator> trophyName(
        hasCupOverride
            ? BasicString<char, Detail::TempStringAllocator>("Gameplay/").Append(GetGimmeCupTrophyName())
            : BasicString<char, Detail::TempStringAllocator>(
                  GetThrophyModelName(nlSingleton<GameInfoManager>::Instance()->GetTrophyTypeByCurrentMode())));

    nlSNPrintf(trophyFileName, 0xFF, "%s.glg", trophyName.c_str());
    glBeginLoadModel(trophyFileName, ReadTrophyModel, NULL);
}

inline bool Presentation::DetectSkipPress() const
{
    if (nlSingleton<GameInfoManager>::Instance()->IsInDemoMode())
    {
        return false;
    }

    Config& cfg = Config::Global();
    TagValuePair& tvp = cfg.FindTvp("no_presentation_skip");
    bool noPresentationSkip;

    if (tvp.tag == 0)
    {
        cfg.Set("no_presentation_skip", false);
        noPresentationSkip = false;
    }
    else if (tvp.type == _BOOL)
    {
        noPresentationSkip = LexicalCast<bool, bool>(tvp.value.b);
    }
    else if (tvp.type == _INT)
    {
        noPresentationSkip = LexicalCast<bool, int>(tvp.value.i);
    }
    else if (tvp.type == _FLOAT)
    {
        noPresentationSkip = LexicalCast<bool, float>(tvp.value.f);
    }
    else if (tvp.type == _STRING)
    {
        noPresentationSkip = LexicalCast<bool, const char*>(tvp.value.s);
    }
    else
    {
        noPresentationSkip = false;
    }

    if (noPresentationSkip)
    {
        bool trophyShown;
        if (cupTrophyHash != 0)
        {
            trophyShown = (WorldManager::s_World->FindDrawableObject(cupTrophyHash)->m_uObjectFlags & 1) != 0;
        }
        else
        {
            trophyShown = false;
        }

        if (nlStrCmp<char>(mCurrentFunction, "PlayHighlight") != 0 && !trophyShown)
        {
            return false;
        }
    }

    if (DuringEndOfGamePresentation() & (mTimeInFunction <= 1.2f))
    {
        return false;
    }

    if (nlTaskManager::m_pInstance->m_CurrState == 0x100 || nlTaskManager::m_pInstance->m_CurrState == 0x10)
    {
        for (int i = 0; i < 4; i++)
        {
            if (!mIsAllowedToSkip[i])
            {
                if (nlStrCmp<char>(mCurrentFunction, "PlayHighlight") != 0)
                {
                    continue;
                }
            }

            cGlobalPad* pad = cPadManager::GetPad(i);
            if (pad != 0)
            {
                if (cPadManager::GetPad(i)->JustPressed(7, true))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

/**
 * Offset/Address/Size: 0x14EC | 0x80125CD0 | size: 0x35C
 */
void Presentation::Finish()
{
    if ((strcmp("PlayHighlight", mCurrentFunction) == 0 || loopPresentation) && mByPassWasSkipped == false)
    {
        FixedUpdateTask::mTimeScale = 1.0f;
        ParticleUpdateTask::SetTimeScale(1.0f);

        if (nlStrCmp<char>(idleFun, mCurrentFunction) != 0 && nlStrCmp<char>(idleFun, "PlayHighlight") != 0)
        {
            mQueuedFunction = "PlayHighlight";
        }
        else
        {
            nlStrNCpy<char>(mCurrentFunction, "PlayHighlight", 64);
            mSkipPressed = false;
            mInsideByPass = false;
            mByPassing = false;
            mInterruptWipe = 0;
            mUseInterruptWipe = 0;
            mTimeInFunction = 0.0f;

            NisPlayer::Instance()->SetExtraNameFilter("");
            CallFunction(nlStringHash("PlayHighlight"));
        }
    }
    else
    {
        if (mCurrentFunction == strstr(mCurrentFunction, "Goal"))
        {
            if (g_pGame->m_eGameState != GS_END_GAME)
            {
                g_pGame->ChangeGameState(GS_KICKOFF);
            }
        }

        if (mQueuedFunction == 0)
        {
            bool duringEndOfGamePresentation = false;
            if (nlStrCmp<char>("ImplGameEnd", mCurrentFunction) == 0 || nlStrCmp<char>("GameEndNoSuddenDeath", mCurrentFunction) == 0 || nlStrCmp<char>("GoalSuddenDeath", mCurrentFunction) == 0 || nlStrCmp<char>("PlayHighlight", mCurrentFunction) == 0 || nlStrCmp<char>("PlayCupThrophy", mCurrentFunction) == 0)
            {
                duringEndOfGamePresentation = true;
            }

            if (duringEndOfGamePresentation)
            {
                g_pEventManager->CreateValidEvent(3, 0x14);
                nlTaskManager::SetNextState(1);
            }
            else
            {
                if (nlStrCmp<char>(mCurrentFunction, "GameBegin") == 0)
                {
                    g_pGame->ChangeGameState(GS_KICKOFF);
                }
                nlTaskManager::SetNextState(2);
            }
        }
    }

    FixedUpdateTask::mTimeScale = 1.0f;
    const char* functionName = idleFun;
    ParticleUpdateTask::SetTimeScale(1.0f);

    if (nlStrCmp<char>(idleFun, mCurrentFunction) != 0 && nlStrCmp<char>(idleFun, functionName) != 0)
    {
        mQueuedFunction = functionName;
    }
    else
    {
        nlStrNCpy<char>(mCurrentFunction, functionName, 64);
        mSkipPressed = false;
        mInsideByPass = false;
        mByPassing = false;
        mInterruptWipe = 0;
        mUseInterruptWipe = 0;
        mTimeInFunction = 0.0f;

        NisPlayer::Instance()->SetExtraNameFilter("");
        CallFunction(nlStringHash(functionName));
    }

    functionName = mQueuedFunction;
    if (functionName)
    {
        mQueuedFunction = 0;
        FixedUpdateTask::mTimeScale = 1.0f;
        ParticleUpdateTask::SetTimeScale(1.0f);

        if (nlStrCmp<char>(idleFun, mCurrentFunction) != 0 && nlStrCmp<char>(idleFun, functionName) != 0)
        {
            mQueuedFunction = functionName;
        }
        else
        {
            nlStrNCpy<char>(mCurrentFunction, functionName, 64);
            mSkipPressed = false;
            mInsideByPass = false;
            mByPassing = false;
            mInterruptWipe = 0;
            mUseInterruptWipe = 0;
            mTimeInFunction = 0.0f;

            NisPlayer::Instance()->SetExtraNameFilter("");
            CallFunction(nlStringHash(functionName));
        }
    }

    if (strcmp(mCurrentFunction, idleFun) == 0)
    {
        ReplayChoreo::Instance().Reset();
    }
}

void Presentation::HandleOverlay(float deltaT)
{
    if (!mOverlayDisplayed)
    {
        mOverlayDelay -= deltaT;
        if (mOverlayDelay <= 0.0)
        {
            nlSingleton<OverlayManager>::Instance()->SetVisible(mOverlayToDisplay, true, false);
            if (mOverlayToDisplay == OVERLAY_GOAL)
            {
                nlSingleton<OverlayManager>::Instance()->RestartGoalOverlay();
            }
            mOverlayDelay = 0.0f;
            mOverlayDisplayed = true;
        }
    }
    else if (mOverlayDisplayLength != -15.0f)
    {
        mOverlayDisplayLength -= deltaT;
        if (mOverlayDisplayLength <= 0.0)
        {
            if (mOverlayDisplayed)
            {
                nlSingleton<OverlayManager>::Instance()->SetVisible(mOverlayToDisplay, false, false);
            }
            mOverlayDisplayed = false;
            mOverlayToDisplay = SCENE_INVALID;
            mOverlayDisplayLength = 0.0f;
            mOverlayDelay = 0.0f;
        }
    }
}

static inline void SetIdleCallback(AudioStreamTrack::StreamTrack* track, const Function0<void>& callback)
{
    track->m_IdleCallback = Function<FnVoidVoid>(callback);
}

void Presentation::BeginByPass()
{
    mByPassWasSkipped = false;
    mInsideByPass = true;
}

void Presentation::EndByPass()
{
    if (mByPassing)
    {
        mByPassWasSkipped = true;
    }
    mInsideByPass = false;
    mByPassing = false;
}

void Presentation::PlayCharacterDirection()
{
    mWaitingForCharacterDirectionSince = FixedUpdateTask::mSimulationTime;
    NisPlayer::Instance()->PlayCharacterDirection();
}

void Presentation::PlayNis()
{
    if (NisPlayer::Instance()->Play())
    {
        if (nlTaskManager::m_pInstance->m_CurrState == 0x100)
        {
            return;
        }
        if (!IsDuringGamePauseState())
        {
            nlTaskManager::SetNextState(0x100);
        }
    }
    else
    {
        StopWithUndo();
    }
}

void Presentation::ResetNisPlayer()
{
    NisPlayer::Instance()->Reset();
}

void Presentation::SaveGoalAsHighlight()
{
    ReplayChoreo::Instance().SaveHighlight((ReplayChoreo::HighlightQuality)mGoalQuality);
}

void Presentation::SetTrophyVisible(bool visible)
{
    if (cupTrophyHash == 0)
    {
        return;
    }
    DrawableObject* obj = WorldManager::s_World->FindDrawableObject(cupTrophyHash);
    if (visible)
    {
        obj->m_uObjectFlags |= 1;
    }
    else
    {
        obj->m_uObjectFlags &= ~1;
    }
    if (visible)
    {
        CrowdMood::AdjustMood(CrowdMood::CM_Positive, 0xa);
        CrowdMood::EnableCrowdDecay(false);
    }
    else
    {
        cupTrophyHash = 0;
        CrowdMood::EnableCrowdDecay(true);
    }
}

void Presentation::StopAllStreams()
{
    Audio::StopStreaming();
}

void Presentation::StopJumbotron()
{
    if (Jumbotron::instance.m_State == 4)
    {
        Jumbotron::instance.StopPlaying();
    }
}

void Presentation::UnloadJumbotron()
{
    if (Jumbotron::instance.m_State == 4)
    {
        Jumbotron::instance.StopPlaying();
    }
    Jumbotron::instance.Reset();
}

void Presentation::PlayJumbotron()
{
    Jumbotron::instance.WaitForLoad();
    Jumbotron::instance.BeginPlaying();
}

void Presentation::WaitForAutoReplayCompletion(const char* wipe)
{
    if (nlSingleton<ScreenTransitionManager>::Instance()->m_SelectedTransition == NULL)
    {
        nlSingleton<ScreenTransitionManager>::Instance()->SelectRandomTransition(wipe);
    }
    if (!ReplayChoreo::Instance().Done())
    {
        StopWithUndo();
    }
}

void Presentation::WaitForCharacterDirection()
{
    if (mWaitingForCharacterDirectionSince > 0.0f)
    {
        if (FixedUpdateTask::mSimulationTime - mWaitingForCharacterDirectionSince < 1.0f)
        {
            StopWithUndo();
        }
    }
}

void Presentation::WaitForNisCompletion(const char* wipe)
{
    float cutTime = 0.0f;
    if (nlSingleton<ScreenTransitionManager>::Instance()->m_SelectedTransition == NULL)
    {
        nlSingleton<ScreenTransitionManager>::Instance()->SelectRandomTransition(wipe);
    }
    if (nlSingleton<ScreenTransitionManager>::Instance()->m_SelectedTransition != NULL)
    {
        cutTime = 0.2f + nlSingleton<ScreenTransitionManager>::Instance()->GetSelectedTransitionCutTime();
    }
    if (NisPlayer::Instance()->TimeLeft() > cutTime)
    {
        StopWithUndo();
    }
}

void Presentation::Wipe(const char* wipe)
{
    if (mByPassing)
    {
        return;
    }
    if (mUseInterruptWipe != NULL)
    {
        wipe = mUseInterruptWipe;
    }
    Wiper::Instance().DoWipe(wipe);
    if (!Wiper::Instance().CutHasOccured() && Wiper::Instance().WipeInProgress())
    {
        StopWithUndo();
    }
    else
    {
        mUseInterruptWipe = 0;
    }
}

/**
 * Offset/Address/Size: 0xE70 | 0x80125654 | size: 0x67C
 */
void Presentation::Update(float deltaT)
{
    mTimeInFunction += deltaT;

    if (getenv("STRIKERS_LOG_NIS") != NULL)
    {
        static int nPresLog = 0;
        if ((nPresLog++ % 60) == 0)
        {
            OSReport("[pres] fn='%s' t=%.2f simT=%.2f wipe=%d cut=%d "
                     "chardir=%.2f paused=%d state=0x%x lb=%d/%.2f\n",
                     mCurrentFunction, (double)mTimeInFunction,
                     (double)FixedUpdateTask::mSimulationTime,
                     (int)Wiper::Instance().WipeInProgress(),
                     (int)Wiper::Instance().CutHasOccured(),
                     (double)mWaitingForCharacterDirectionSince,
                     (int)IsDuringGamePauseState(),
                     (unsigned int)nlTaskManager::m_pInstance->m_CurrState,
                     (int)mLetterBoxEnabled, (double)mLetterBoxDuration);
        }
    }

    if (mDisplayLetterBox > 0.0f)
    {
        mDisplayLetterBox -= deltaT;
        if (mDisplayLetterBox <= 0.0f)
        {
            ReplayChoreo::Instance().SaveHighlight(ReplayChoreo::HIGHLIGHT_QUALITY_SAVE);
            mDisplayLetterBox = 0.0f;
        }
    }

    if (!IsDuringGamePauseState())
    {
        if (nlStrCmp<char>(mCurrentFunction, "GameBegin") == 0)
        {
            if (nlTaskManager::m_pInstance->m_CurrState != 0x100)
            {
                glDiscardFrame(1);
            }
        }

        Run();

        ReplayChoreo::Instance().Update(deltaT);
        ReplayCamera::UpdateTweakMode();

        if (!mSkipPressed)
        {
            mSkipPressed = DetectSkipPress();
        }

        if (mSkipPressed && mInsideByPass)
        {
            mByPassing = true;
            mSkipPressed = false;
            g_pEventManager->CreateValidEvent(0x1C, 0x14);
            mUseInterruptWipe = mInterruptWipe;
            mInterruptWipe = 0;

            if (DuringEndOfGamePresentation())
            {
                mTimeInFunction = 0.0f;
            }
        }

        if (IsFinished())
        {
            Finish();
        }
    }

    Wiper::Instance().Render(IsDuringGamePauseState() ? 0.0f : deltaT);

    UpdateAndRenderLetterBox();

    if (IsDuringGamePauseState())
    {
        return;
    }

    if (mOverlayToDisplay == SCENE_INVALID)
    {
        return;
    }

    HandleOverlay(deltaT);
}

/**
 * Offset/Address/Size: 0xDBC | 0x801255A0 | size: 0xB4
 */
bool Presentation::DuringEndOfGamePresentation() const
{
    return nlStrCmp<char>("ImplGameEnd", mCurrentFunction) == 0 || nlStrCmp<char>("GameEndNoSuddenDeath", mCurrentFunction) == 0 || nlStrCmp<char>("GoalSuddenDeath", mCurrentFunction) == 0 || nlStrCmp<char>("PlayHighlight", mCurrentFunction) == 0 || nlStrCmp<char>("PlayCupThrophy", mCurrentFunction) == 0;
}

/**
 * Offset/Address/Size: 0xCF0 | 0x801254D4 | size: 0xCC
 */
void Presentation::Call(const char* functionName, const char* nisFilter)
{
    FixedUpdateTask::mTimeScale = 1.0f;
    ParticleUpdateTask::SetTimeScale(1.0f);

    if (nlStrCmp<char>(idleFun, mCurrentFunction) != 0 && nlStrCmp<char>(idleFun, functionName) != 0)
    {
        mQueuedFunction = functionName;
        return;
    }

    nlStrNCpy<char>(mCurrentFunction, functionName, 64);
    mSkipPressed = false;
    mInsideByPass = false;
    mByPassing = false;
    mInterruptWipe = 0;
    mUseInterruptWipe = 0;
    mTimeInFunction = 0.0f;

    NisPlayer::Instance()->SetExtraNameFilter(nisFilter);
    CallFunction(nlStringHash(functionName));
}
struct TreeNodeLocal;

struct HelperObjectLocal
{
    u32 m_uHashID;
    float _pad0[12];
    nlVector3 position;
    float _pad1;
    char name[64];
};

struct TreeNodeLocal
{
    TreeNodeLocal* left;
    TreeNodeLocal* right;
    s8 heavy;
    u8 _pad0[3];
    u32 key;
    HelperObjectLocal* value;
};

struct WorldLocal
{
    u8 _pad[0x74];
    TreeNodeLocal* helperRoot;
    u8 _pad2[0x4];
    u32 helperCount;
};

struct TreeStackLocal
{
    TreeNodeLocal** nodes;
    u32 count;
};

struct EmissionControllerUserLocal
{
    u8 _pad[0x78];
    u32 m_uUserData;
};

static inline void TriggerParticleEffects()
{
    // PORT: this walked the helper tree through shadow structs padded with console byte offsets.
    typedef AVLTreeEntry<unsigned long, HelperObject*> HelperEntry;

    World* world = WorldManager::s_World;
    if (world == NULL)
    {
        return;
    }

    HelperEntry** nodes = (HelperEntry**)nlMalloc(
        (world->m_helperMap.m_NumElements + 1) * sizeof(HelperEntry*), 8, false);
    if (nodes == NULL)
    {
        return;
    }

    u32 count = 0;
    HelperEntry* node = world->m_helperMap.m_Root;
    if (node != 0)
    {
        while (node->node.left != 0)
        {
            nodes[count++] = node;
            node = (HelperEntry*)node->node.left;
        }

        nodes[count++] = node;
    }

    static int len;
    static signed char init;
    const char* persistentEffectsTag = "fx_goal_";

    while (count != 0)
    {
        HelperObject* helper = nodes[count - 1]->value;

        if (!init)
        {
            len = strlen(persistentEffectsTag);
            init = 1;
        }

        char fxName[256];
        char* fxStart = strstr(helper->m_szName, persistentEffectsTag);
        if (fxStart)
        {
            nlStrNCpy<char>(fxName, fxStart + len, 256);
            char* underscore = strstr(fxName, "_");
            if (underscore != 0)
            {
                *underscore = 0;
            }

            EffectsGroup* fx = fxGetGroup(fxName);
            if (fx != 0)
            {
                EmissionController* ec = EmissionManager::Create(fx, 0);
                ec->SetPosition(helper->m_worldMatrix.GetTranslation());
                ec->m_uUserData = 0xDEADBEEF;
            }
        }

        count--;
        HelperEntry* right = (HelperEntry*)nodes[count]->node.right;
        if (right != 0)
        {
            while (right->node.left != 0)
            {
                nodes[count++] = right;
                right = (HelperEntry*)right->node.left;
            }

            nodes[count++] = right;
        }
    }

    nlFree(nodes);
}
/**
 * Offset/Address/Size: 0x57C | 0x80124D60 | size: 0x774
 */
void Presentation::EventHandler(Event* event)
{
    struct GameLocal
    {
        char _pad[0x1C];
        float m_fGameDuration;
    };

    if (g_pGame == 0)
    {
        return;
    }

    if (event->m_uEventID == 8)
    {
        mWaitingForCharacterDirectionSince = 0.0f;
    }

    if (event->m_uEventID == 5)
    {
        TriggerParticleEffects();

        Config& cfg = Config::Global();
        TagValuePair& tvp = cfg.FindTvp("no_presentation");
        bool noPresentation;

        if (tvp.tag == 0)
        {
            cfg.Set("no_presentation", false);
            noPresentation = false;
        }
        else if (tvp.type == _BOOL)
        {
            noPresentation = LexicalCast<bool, bool>(tvp.value.b);
        }
        else if (tvp.type == _INT)
        {
            noPresentation = LexicalCast<bool, int>(tvp.value.i);
        }
        else if (tvp.type == _FLOAT)
        {
            noPresentation = LexicalCast<bool, float>(tvp.value.f);
        }
        else if (tvp.type == _STRING)
        {
            noPresentation = LexicalCast<bool, const char*>(tvp.value.s);
        }
        else
        {
            noPresentation = false;
        }

        if (noPresentation)
        {
            g_pGame->ChangeGameState(GS_KICKOFF);
            return;
        }

        bool takeTheLead;
        bool inSuddenDeath;
        GoalScoredData* gsd;
        bool tiesTheGame;
        s32 id = port_event_data_id(&event->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            gsd = 0;
        }
        else
        {
            id = port_event_data_id(&event->m_data);
            if (id != 0x18A)
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                gsd = 0;
            }
            else
            {
                gsd = (GoalScoredData*)&event->m_data;
            }
        }

        if (gsd != 0)
        {
            NisPlayer::Instance()->mWinnerSide[1] = gsd->uTeamIndex;

            cTeam* team = g_pTeams[gsd->uTeamIndex];
            s32 scoreDiff = team->m_nScore - team->GetOtherTeam()->m_nScore;
            tiesTheGame = (scoreDiff == 0);

            takeTheLead = true;
            if (scoreDiff != 1 && scoreDiff != (s32)gsd->uNumGoalsScored)
            {
                takeTheLead = false;
            }

            inSuddenDeath = (g_pGame->m_eGameState == GS_OVERTIME);

            bool byCaptain;
            if (gsd->uGoalType == 5)
            {
                byCaptain = gsd->pLastTouch[gsd->uTeamIndex]->IsCaptain();
            }
            else
            {
                byCaptain = gsd->pScorer->IsCaptain();
            }

            const char* filter = "high";
            const char* script = "GoalCaptainCelebration";

            if (tiesTheGame)
            {
                mGoalQuality = HIGHLIGHT_QUALITY_GOAL_EQUALIZER;
            }
            else if (takeTheLead)
            {
                mGoalQuality = HIGHLIGHT_QUALITY_GOAL_INCREASE_DIFF;
            }
            else
            {
                mGoalQuality = HIGHLIGHT_QUALITY_GOAL_DECREASE_DIFF;
            }

            if (!inSuddenDeath && !takeTheLead && !tiesTheGame)
            {
                if (nlRandom(100, &nlDefaultSeed) < 80)
                {
                    filter = "low";
                }
            }

            if (!byCaptain)
            {
                if (nlRandom(100, &nlDefaultSeed) < 80)
                {
                    script = "GoalSidekickCelebration";
                }
            }

            if (nlStrCmp<char>(filter, "low") == 0)
            {
                if (gsd->uGoalType != 6 && !gsd->uIsHyper)
                {
                    if (nlRandom(100, &nlDefaultSeed) < 20)
                    {
                        script = "GoalJumbotron";
                        if (nlRandom(100, &nlDefaultSeed) < 50)
                        {
                            filter = "high";
                        }
                    }
                }
            }

            if (!tiesTheGame)
            {
                if (g_pGame != 0)
                {
                    // PORT: GameLocal is padded to m_fGameDuration's console offset.
                    float duration = g_pGame->m_fGameDuration;
                    if (g_pGame->GetGameTime() >= duration)
                    {
                        s32 awayScore = g_pTeams[1]->m_nScore;
                        s32 homeScore = g_pTeams[0]->m_nScore;
                        NisPlayer* nisPlayer = NisPlayer::Instance();
                        nisPlayer->mWinnerSide[0] = (awayScore > homeScore);
                        script = "GoalSuddenDeath";
                    }
                }
            }

            FixedUpdateTask::mTimeScale = 1.0f;
            ParticleUpdateTask::SetTimeScale(1.0f);

            if (nlStrCmp<char>(idleFun, mCurrentFunction) != 0 && nlStrCmp<char>(idleFun, script) != 0)
            {
                mQueuedFunction = script;
            }
            else
            {
                nlStrNCpy<char>(mCurrentFunction, script, 64);
                mSkipPressed = false;
                mInsideByPass = false;
                mByPassing = false;
                mInterruptWipe = 0;
                mUseInterruptWipe = 0;
                mTimeInFunction = 0.0f;

                NisPlayer::Instance()->SetExtraNameFilter(filter);
                CallFunction(nlStringHash(script));
            }

            bool foundTeamPad = false;
            for (s32 i = 0; i < 4; i++)
            {
                cAIPad* aiPad = &AIPadManager::mAIPads[i];
                for (s32 c = 0; c < 10; c++)
                {
                    cPlayer* ch = ((cPlayer**)g_pCharacters)[c];
                    if (ch->m_pController == aiPad)
                    {
                        if ((s32)gsd->uTeamIndex == ch->m_pTeam->m_nSide)
                        {
                            mIsAllowedToSkip[i] = true;
                            foundTeamPad = true;
                        }
                        else
                        {
                            mIsAllowedToSkip[i] = false;
                        }
                    }
                }
            }

            if (!foundTeamPad)
            {
                mIsAllowedToSkip[0] = true;
                mIsAllowedToSkip[1] = true;
                mIsAllowedToSkip[2] = true;
                mIsAllowedToSkip[3] = true;
            }
        }
    }

    if (event->m_uEventID == 0xF)
    {
        GoalieSaveData* gsd;

        s32 id = port_event_data_id(&event->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            gsd = 0;
        }
        else
        {
            id = port_event_data_id(&event->m_data);
            if (id != 0x13C)
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                gsd = 0;
            }
            else
            {
                gsd = (GoalieSaveData*)&event->m_data;
            }
        }

        if (gsd != 0 && !gsd->isSTS)
        {
            mDisplayLetterBox = 1.0f;
        }
    }
}

void Presentation::LoadNis(const char* name, NisTarget target, NisUseStadiumOffset stadiumOffset, NisUseFilter filter, NisWinnerType winnerType)
{
    if (mByPassing)
    {
        return;
    }
    if (nlStrCmp<char>(name, "trophy") == 0 && !Config::Global().Exists("gimme_cup_trophy") && !nlSingleton<StatsTracker>::Instance()->mIsUserCupWinner)
    {
        return;
    }
    NisPlayer::Instance()->Load(name, target, stadiumOffset, filter, winnerType);
}

void Presentation::PlayAutoReplay(ReplayType type)
{
    if (mByPassing)
    {
        return;
    }
    if (nlTaskManager::m_pInstance->m_CurrState != 0x10 && !IsDuringGamePauseState())
    {
        nlTaskManager::SetNextState(0x10);
    }
    ReplayChoreo::Instance().StartAutoReplay(type);
    if (type == REPLAY_TYPE_HIGHLIGHT)
    {
        nlSingleton<OverlayManager>::Instance()->SetCurrentTextOverlaySlide((OverlaySlideName)7);
        nlSingleton<OverlayManager>::Instance()->SetVisible((SceneList)0x44, false, true);
        nlSingleton<OverlayManager>::Instance()->mIsInHighlights = true;
        if (mOverlayDisplayed)
        {
            nlSingleton<OverlayManager>::Instance()->SetVisible(mOverlayToDisplay, false, false);
        }
        mOverlayDisplayed = false;
        mOverlayToDisplay = SCENE_INVALID;
        mOverlayDisplayLength = 0.0f;
        mOverlayDelay = 0.0f;
        PlayOverlay("highlight", 0.5f, 30.0f);
    }
    else
    {
        nlSingleton<OverlayManager>::Instance()->SetCurrentTextOverlaySlide((OverlaySlideName)7);
        nlSingleton<OverlayManager>::Instance()->SetVisible((SceneList)0x44, true, true);
        nlSingleton<OverlayManager>::Instance()->mIsInHighlights = false;
    }
}

void Presentation::PlaySfx(const char* sfx)
{
    Audio::PlayWorldSFXbyStr(sfx, 100.0f, -1.0f, false, true, NULL, NULL, NULL);
}

void Presentation::PlaySfxWithVol(const char* sfx, float volume)
{
    Audio::PlayWorldSFXbyStr(sfx, volume, -1.0f, false, true, NULL, NULL, NULL);
}

/**
 * Offset/Address/Size: 0x3D0 | 0x80124BB4 | size: 0x1AC
 */
void Presentation::PlayOverlay(const char* name, float delay, float length)
{
    if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
    {
        return;
    }

    if (mOverlayDisplayed)
    {
        nlSingleton<OverlayManager>::Instance()->SetVisible(mOverlayToDisplay, false, false);
    }

    mOverlayDisplayed = false;
    mOverlayToDisplay = SCENE_INVALID;
    mOverlayDisplayLength = 0.0f;
    mOverlayDelay = 0.0f;

    if (nlStrCmp<char>("goal", name) == 0)
    {
        mOverlayToDisplay = OVERLAY_GOAL;
        mOverlayDelay = delay;
        mOverlayDisplayLength = length;
        mOverlayDisplayed = false;
        return;
    }

    if (nlStrCmp<char>("highlight", name) == 0)
    {
        mOverlayToDisplay = OVERLAY_GOAL;
        mOverlayDelay = delay;
        mOverlayDisplayLength = length;
        mOverlayDisplayed = false;
        GoalOverlay* scene = (GoalOverlay*)nlSingleton<OverlayManager>::Instance()->GetScene(mOverlayToDisplay);
        scene->SetHighlightNumber(ReplayChoreo::Instance().mNumHighlights);
        return;
    }

    if (nlStrCmp<char>("end", name) == 0)
    {
        mOverlayToDisplay = OVERLAY_GOAL;
        mOverlayDelay = delay;
        mOverlayDisplayLength = length;
        mOverlayDisplayed = false;
        GoalOverlay* scene = (GoalOverlay*)nlSingleton<OverlayManager>::Instance()->GetScene(mOverlayToDisplay);
        scene->DoMatchEndOverlay();
        return;
    }

    if (nlStrCmp<char>("cup", name) == 0)
    {
        mOverlayToDisplay = OVERLAY_GOAL;
        mOverlayDelay = delay;
        mOverlayDisplayLength = length;
        mOverlayDisplayed = false;
        GoalOverlay* scene = (GoalOverlay*)nlSingleton<OverlayManager>::Instance()->GetScene(mOverlayToDisplay);
        scene->DoCupWinOverlay();
    }
}

/**
 * Offset/Address/Size: 0x36C | 0x80124B50 | size: 0x64
 */
void Presentation::StopOverlay()
{
    if (mOverlayDisplayed)
    {
        nlSingleton<OverlayManager>::Instance()->SetVisible(mOverlayToDisplay, false, false);
    }
    mOverlayDisplayed = false;
    mOverlayToDisplay = SCENE_INVALID;
    mOverlayDisplayLength = 0.0f;
    mOverlayDelay = 0.0f;
}

void Presentation::PlayHighlights()
{
    nlSingleton<ScreenTransitionManager>::Instance()->m_SelectedTransition = NULL;
    if (ReplayChoreo::Instance().NumHighlights() <= 0)
    {
        return;
    }
    FixedUpdateTask::mTimeScale = 1.0f;
    ParticleUpdateTask::SetTimeScale(1.0f);
    if (nlStrCmp<char>(idleFun, mCurrentFunction) != 0 && nlStrCmp<char>(idleFun, "PlayHighlight") != 0)
    {
        mQueuedFunction = "PlayHighlight";
        return;
    }
    nlStrNCpy<char>(mCurrentFunction, "PlayHighlight", 64);
    mSkipPressed = false;
    mInsideByPass = false;
    mByPassing = false;
    mInterruptWipe = 0;
    mUseInterruptWipe = 0;
    mTimeInFunction = 0.0f;
    NisPlayer::Instance()->SetExtraNameFilter("");
    CallFunction(nlStringHash("PlayHighlight"));
}

/**
 * Offset/Address/Size: 0x19C | 0x80124980 | size: 0x1D0
 */
static void CupWinStingerDone()
{
    AudioStreamTrack::TrackManagerBase* trackManager = g_pTrackManager;
    // PORT: null when audio is disabled, nothing to ask.
    AudioStreamTrack::StreamTrack* track = trackManager != NULL ? trackManager->GetTrack(nlStringLowerHash("Music")) : NULL;
    {
        Function<FnVoidVoid> emptyCallback;
        SetIdleCallback(track, emptyCallback);
    }
    track->PlayStream(nlStringLowerHash("STAD_Intro"), 0.5f, true, 500, 500, "Stadium", Audio::MasterVolume::VG_Special);
}

void Presentation::PlayCupOverlay()
{
    bool gimmeCup = Config::Global().Exists("gimme_cup_trophy");
    if (!gimmeCup && !nlSingleton<StatsTracker>::Instance()->mIsUserCupWinner)
    {
        return;
    }
    if (gimmeCup)
    {
        return;
    }

    PlayOverlay("cup", 0.2f, -15.0f);
    const char* streamName = GetCupStreamName(nlSingleton<GameInfoManager>::Instance()->GetTrophyTypeByCurrentMode());
    AudioLoader::StartFEStream(streamName, false, "Music");
    AudioStreamTrack::TrackManagerBase* trackManager = AudioStreamTrack::TrackManagerBase::Get();
    AudioStreamTrack::StreamTrack* track = trackManager->GetTrack(nlStringLowerHash("Music"));
    Function0<void> callback(CupWinStingerDone);
    SetIdleCallback(track, callback);
}

void Presentation::UpdateAndRenderLetterBox()
{
    if (mLetterBoxEnabled)
    {
        mLetterBoxDuration += 0.05f;
    }
    else
    {
        mLetterBoxDuration -= 0.05f;
    }

    if (mLetterBoxDuration < 0.0f)
    {
        mLetterBoxDuration = 0.0f;
    }

    if (mLetterBoxDuration > 1.0f)
    {
        mLetterBoxDuration = 1.0f;
    }

    // PORT: was the game's own widescreen option, which this port leaves false because it requests the anamorphic squeeze.
    if (PortDrawCinematicBars() && mLetterBoxDuration > 0.0f)
    {
        nlColour black = { { 0x00, 0x00, 0x00, 0xFF } };
        g_ShapeRenderer.DrawRectangle2D(0.0f, 0.0f, glGetOrthographicWidth(), 38.0f * mLetterBoxDuration, -0.5f, black, GLV_Transitions);
        g_ShapeRenderer.DrawRectangle2D(0.0f, glGetOrthographicHeight() - 38.0f * mLetterBoxDuration, glGetOrthographicWidth(), 38.0f * mLetterBoxDuration, -0.5f, black, GLV_Transitions);
    }
}

/**
 * Offset/Address/Size: 0x60 | 0x80124844 | size: 0x13C
 */
void Presentation::Reset()
{
    mIsAllowedToSkip[0] = true;
    mIsAllowedToSkip[1] = true;
    mIsAllowedToSkip[2] = true;
    mIsAllowedToSkip[3] = true;

    FixedUpdateTask::mTimeScale = 1.0f;
    const char* functionName = idleFun;
    ParticleUpdateTask::SetTimeScale(1.0f);

    if (nlStrCmp<char>(idleFun, mCurrentFunction) != 0 && nlStrCmp<char>(idleFun, functionName) != 0)
    {
        mQueuedFunction = functionName;
    }
    else
    {
        nlStrNCpy<char>(mCurrentFunction, functionName, 64);
        mSkipPressed = false;
        mInsideByPass = false;
        mByPassing = false;
        mInterruptWipe = 0;
        mUseInterruptWipe = 0;
        mTimeInFunction = 0.0f;

        NisPlayer::Instance()->SetExtraNameFilter("");
        CallFunction(nlStringHash(functionName));
    }

    mQueuedFunction = 0;
    mOverlayDisplayed = false;
    if (mOverlayDisplayed)
    {
        nlSingleton<OverlayManager>::Instance()->SetVisible(mOverlayToDisplay, false, false);
    }
    mOverlayDisplayed = false;
    mOverlayToDisplay = SCENE_INVALID;
    mOverlayDisplayLength = 0.0f;
    mOverlayDelay = 0.0f;

    Wiper::Instance().Reset();
    NisPlayer::Instance()->Reset();
    ReplayChoreo::Instance().Reset();
    ReplayManager::Instance()->Flush();
}
