// The online session in the game: local pads in, slot pads out, match loading and transitions.

#include "port/online_game.h"
#include "panel_text.h"
#include "NL/nlLocalization.h"

#include "port/host.h"
#include "port/lockstep.h"
#include "port/online.h"
#include "port/online_panel.h"
#include "port/region.h"
#include "Game/FE/tlTextInstance.h"

#include "Game/BaseGameSceneManager.h"
#include "Game/FE/feHelpFuncs.h"
#include "Game/FE/feManager.h"
#include "Game/FixedUpdateTask.h"
#include "Game/Game.h"
#include "Game/GameInfo.h"
#include "Game/GameSceneManager.h"
#include "Game/Team.h"
#include "Game/SH/SHChooseCaptains.h"
#include "NL/nlTask.h"
#include "NL/platpad.h"
#include "dolphin/os.h"
#include "dolphin/pad.h"

#if defined(PORT_USE_AURORA)
#include <aurora/gfx.h>
#endif

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace
{

enum Stage
{
    STAGE_NONE,        // no online match on this machine
    STAGE_START,       // a match is agreed; the front end has to reach a menu to load it from
    STAGE_LOAD,        // loading, BeginGame included
    STAGE_PIPELINES,   // loaded; the pipelines the load queued compile
    STAGE_READY,       // reported loaded; the first tick waits for the host's start time
    STAGE_PLAY,        // the match and its post-match
    STAGE_LEAVE        // back to the front end, then the room
};

Stage s_stage;
uint32_t s_serial;
OnlineMatchDesc s_desc;
unsigned int s_matchAtLoad;
unsigned long long s_loadedAtNs;
int s_lockstepDefault;
PADStatus s_device[4];
// The pad array the slot pads went into last frame, or NULL.
PADStatus* s_slotPads;
bool s_waitLogged;
bool s_selectOpen;
bool s_selectUsedSettings;
uint32_t s_selectEpoch;
IChooseCaptain* s_captains;
int s_captainButtonMode = -1;
bool s_stadiumUnlocked;
int s_sentTeam[2], s_sentSidekick[2], s_sentPhase[2];
// A remote side's ready artwork waits for its images' previous load to finish.
bool s_artworkOwed[2];

const unsigned short* localized_name(unsigned long hash)
{
    const nlLocalization* loc = g_pLocalization;
    unsigned first = 0, last = loc->m_pFile->StringCount;
    while (first < last)
    {
        const unsigned mid = first + (last - first) / 2;
        const auto& entry = loc->m_LookupTable[mid];
        if (entry.hash == hash)
            return loc->m_FirstString + entry.StringOffset;
        if (entry.hash < hash)
            first = mid + 1;
        else
            last = mid;
    }
    return nullptr;
}

void refresh_language()
{
    using namespace online_panel;
    const nlLocalization* loc = g_pLocalization;
    if (!loc || !loc->m_pFile || !loc->m_LookupTable || !loc->m_FirstString)
        return;
    static const LOCHeader* loaded = nullptr;
    static int language = -1;
    if (loaded == loc->m_pFile && language == loc->m_CurrentLanguage)
        return;
    loaded = loc->m_pFile;
    language = loc->m_CurrentLanguage;
    text_language(language == nlLocalization::LangGerman    ? "de"
                  : language == nlLocalization::LangFrench  ? "fr"
                  : language == nlLocalization::LangSpanish ? "es"
                                                            : "en");
    s_captainButtonMode = -1;
    for (int i = T_CAPTAIN_FIRST; i < T_COUNT; ++i)
        text_native((Text)i, nullptr);
    // The panel font has no Japanese glyphs, so the game's own names stay English there.
    if (language == nlLocalization::LangJapanese)
        return;
    for (int i = 0; i < 9; ++i)
        text_native((Text)(T_CAPTAIN_FIRST + i),
                    localized_name(GetLOCCharacterName((eTeamID)i, false, false)));
    for (int i = 0; i < 4; ++i)
        text_native((Text)(T_SIDEKICK_FIRST + i),
                    localized_name(GetLOCSidekickName((eSidekickID)i)));
    for (int i = 0; i < 7; ++i)
        text_native((Text)(T_STADIUM_FIRST + i), localized_name(GetStadiumStringID((eStadiumID)i)));
    for (int i = 0; i < 5; ++i)
        text_native((Text)(T_SKILL_FIRST + i),
                    localized_name(GetLOCDifficultyName((GameplaySettings::eSkillLevel)i)));
}

int selection_port(int side)
{
    const OnlineState& st = *PortOnlineGetState();
    for (int s = 0; s < ONLINE_MAX_SLOTS; s++)
        if (st.desc.padSide[s] == side && st.desc.slotMachine[s] >= 0)
            return PortOnlineSlotLocalPort(s);
    return st.self == st.host ? PortOnlineSlotLocalPort(0) : -1;
}

void selection_frame(const OnlineState& st)
{
    if (st.selection.stage == 1 && (!s_selectOpen || s_selectEpoch != st.selection.epoch))
    {
        const nlTaskManager* t = nlTaskManager::m_pInstance;
        if (t == NULL || t->m_CurrState != 4 || t->m_PendingState != 4)
            return;
        if (!s_selectOpen)
            s_selectUsedSettings = GameInfoManager::Instance()->mUseCurGameSettings;
        s_selectOpen = true;
        s_selectEpoch = st.selection.epoch;
        s_captains = NULL;
        GameInfoManager* info = GameInfoManager::Instance();
        info->SetMode(GameInfoManager::GM_FRIENDLY);
        info->mIsInStrikers101Mode = false;
        for (int side = 0; side < 2; side++)
        {
            info->SetTeam((short)side, (eTeamID)st.desc.team[side]);
            info->SetSidekick((short)side, (eSidekickID)st.desc.sidekick[side]);
        }
        for (int slot = 0; slot < ONLINE_MAX_SLOTS; slot++)
            info->SetPlayingSide((unsigned short)slot, st.desc.padSide[slot]);
        GameSceneManager::Instance()->PopEntireStack();
        GameSceneManager::Instance()->Push(SCENE_CHOOSE_CAPTAINS, SCREEN_FORWARD, false);
        PortOnlinePanelSetOpen(0);
    }
    else if (s_selectOpen && (st.selection.stage == 0 || st.phase != ONLINE_PHASE_ROOM))
    {
        GameInfoManager::Instance()->mUseCurGameSettings = s_selectUsedSettings;
        s_selectOpen = false;
        s_captains = NULL;
        GameSceneManager::Instance()->PopEntireStack();
        GameSceneManager::Instance()->Push(SCENE_MAIN_MENU, SCREEN_BACK, false);
        if (st.phase != ONLINE_PHASE_LOADING)
            PortOnlinePanelSetOpen(1);
    }
}

// A cold pipeline cache can take this long; past it the match starts and the rest compile in play.
const unsigned long long kPipelineWaitNs = 60000000000ull;

bool resting_on(SceneList scene)
{
    const nlTaskManager* t = nlTaskManager::m_pInstance;
    if (t == NULL || t->m_CurrState != 4 || t->m_PendingState != t->m_CurrState)
        return false;
    const GameSceneManager* fe = nlSingleton<GameSceneManager>::Instance();
    return fe != NULL && fe->mCurrentStackDepth != 0 &&
           fe->m_sceneStack[fe->mCurrentStackDepth - 1] == scene;
}

// A match loads from the title or the main menu.
bool at_menu(void) { return resting_on(SCENE_TITLE) || resting_on(SCENE_MAIN_MENU); }

// Out of the match running here, through the pause menu's forfeit path.
void leave_match(void)
{
    const nlTaskManager* t = nlTaskManager::m_pInstance;
    if (t != NULL && t->m_CurrState != 4 && t->m_PendingState != 4 && g_pGame != NULL)
        FrontEnd::ReturnToFE();
}

void start_load(void)
{
    GameInfoManager* info = GameInfoManager::Instance();
    info->mIsInStrikers101Mode = false;
    info->SetMode(GameInfoManager::GM_FRIENDLY);
    for (int side = 0; side < 2; side++)
    {
        // Team 8 is the Super Team, built whether or not this machine's save has unlocked it.
        info->SetTeam((short)side, (eTeamID)s_desc.team[side]);
        info->SetSidekick((short)side, (eSidekickID)s_desc.sidekick[side]);
    }
    info->SetStadium((eStadiumID)s_desc.stadium);
    for (int slot = 0; slot < ONLINE_MAX_SLOTS; slot++)
        info->SetPlayingSide((unsigned short)slot, (short)s_desc.padSide[slot]);

    PortSimMatchSettings m;
    m.seed = s_desc.seed;
    m.bowserRepeat = s_desc.bowserRepeat;
    m.pureGame = s_desc.pureGame;
    m.pauseAllowed = s_desc.pauseAllowed;
    PortSimSetMatchSettings(&m);
    g_simLockstep = 1;
    PortSimSetOnline(1);
    s_matchAtLoad = PortSimMatch();
    nlTaskManager::SetNextState(2);
    s_stage = STAGE_LOAD;
    OSReport("[online] match %u: loading\n", (unsigned)s_serial);
}

void finish_leave(void)
{
    g_simLockstep = s_lockstepDefault;
    PortSimSetOnline(0);
    PortSimSetMatchSettings(NULL);
    s_slotPads = NULL;
    s_stage = STAGE_NONE;
    OSReport("[online] match %u: left\n", (unsigned)s_serial);
}

void pipeline_counts(uint32_t* queued, uint32_t* done)
{
    *queued = 0;
    *done = 0;
#if defined(PORT_USE_AURORA)
    aurora_get_pipeline_counts(queued, done);
#endif
}

bool match_phase(int phase)
{
    return phase == ONLINE_PHASE_LOADING || phase == ONLINE_PHASE_MATCH ||
           phase == ONLINE_PHASE_POST_MATCH;
}

void follow(const OnlineState& st)
{
    // Each room numbers its matches from 1 again.
    if (st.phase == ONLINE_PHASE_IDLE || st.phase == ONLINE_PHASE_HOSTING ||
        st.phase == ONLINE_PHASE_JOINING || st.phase == ONLINE_PHASE_ERROR)
        s_serial = 0;
    const bool agreed = st.phase == ONLINE_PHASE_LOADING && st.matchSerial != s_serial;
    // Called off, over, or followed by a rematch, which loads afresh.
    if (s_stage != STAGE_NONE && s_stage != STAGE_LEAVE && (!match_phase(st.phase) || agreed))
    {
        if (s_stage == STAGE_START)
            s_stage = STAGE_NONE;
        else
        {
            leave_match();
            s_stage = STAGE_LEAVE;
            if (st.desync != ONLINE_DESYNC_NONE)
                PortOnlinePanelSetOpen(1);
        }
    }

    switch (s_stage)
    {
    case STAGE_NONE:
        if (agreed)
        {
            s_serial = st.matchSerial;
            s_desc = st.desc;
            s_stage = STAGE_START;
            s_waitLogged = false;
            PortOnlineLoadProgress(0);
            OSReport("[online] match %u: waiting to load, seed %08x\n", (unsigned)s_serial,
                     (unsigned)s_desc.seed);
        }
        break;
    case STAGE_START:
        if (at_menu())
            start_load();
        else
        {
            leave_match();
            if (!s_waitLogged)
                OSReport("[online] match %u: waiting for the title or main menu\n",
                         (unsigned)s_serial);
            s_waitLogged = true;
        }
        break;
    case STAGE_LOAD:
        if (PortSimMatch() != s_matchAtLoad)
        {
            s_stage = STAGE_PIPELINES;
            s_loadedAtNs = port_monotonic_ns();
        }
        break;
    case STAGE_PIPELINES:
    {
        uint32_t queued, done;
        pipeline_counts(&queued, &done);
        if (queued == 0 || port_monotonic_ns() - s_loadedAtNs > kPipelineWaitNs)
        {
            PortOnlineMatchLoaded();
            s_stage = STAGE_READY;
            OSReport("[online] match %u: loaded, %u pipelines still compiling\n",
                     (unsigned)s_serial, (unsigned)queued);
        }
        else
            PortOnlineLoadProgress((int)(99ull * done / (done + queued)));
        break;
    }
    case STAGE_READY:
        if (st.phase != ONLINE_PHASE_LOADING)
        {
            s_stage = STAGE_PLAY;
            OSReport("[online] match %u: first tick, input delay %d ticks\n", (unsigned)s_serial,
                     st.delayTicks);
        }
        break;
    case STAGE_PLAY:
    {
        // Out of the match by the game's own way, the pause menu's Quit: leave the room too, so
        // the others drop this machine instead of waiting on it.
        const nlTaskManager* t = nlTaskManager::m_pInstance;
        if (t != NULL && (t->m_CurrState == 4 || t->m_PendingState == 4))
        {
            OSReport("[online] match %u: left from the game\n", (unsigned)s_serial);
            s_stage = STAGE_LEAVE;
            PortOnlineLeave();
        }
        break;
    }
    case STAGE_LEAVE:
    {
        const nlTaskManager* t = nlTaskManager::m_pInstance;
        if (t != NULL && t->m_CurrState == 4 && t->m_PendingState == 4)
            finish_leave();
        break;
    }
    }
}

// The stick a machine sends is turned by the camera on its screen before it is quantised.
void sample_pads(void)
{
    // A pad read that was not ready leaves last frame's slot pads, and the devices unchanged.
    if (PadStatus::s_Current != s_slotPads)
        std::memcpy(s_device, PadStatus::s_Current, sizeof s_device);
    PADStatus turned[4];
    std::memcpy(turned, s_device, sizeof turned);
    PortSimTurnSticks(turned);
    PortOnlineSampleLocalPads(turned);
}

// The match reads slots, remote humans among them as connected pads; menus read the devices.
void slot_pads(void)
{
    PADStatus pads[4];
    if (s_stage < STAGE_LOAD || !PortOnlineRenderPads(pads))
    {
        s_slotPads = NULL;
        return;
    }
    for (int s = 0; s < ONLINE_MAX_SLOTS; s++)
    {
        if (PortOnlineSlotKind(s) != ONLINE_SLOT_LOCAL)
            continue;
        const int port = PortOnlineSlotLocalPort(s);
        if (port >= 0 && port < 4)
            pads[s] = s_device[port];
    }
    std::memcpy(PadStatus::s_Current, pads, sizeof pads);
    s_slotPads = PadStatus::s_Current;
}

static_assert(ONLINE_MAX_STADIUMS == MAX_STADIUMS, "the descriptor's stadium follows eStadiumID");

// The name, logo and artwork for a side's picks, as entering its phase shows them.
void refresh_selection(IChooseCaptain& c, int side, int phase)
{
    const eTeamID team = (eTeamID)c.mHomeAwayTeam[side];
    const eSidekickID sidekick = (eSidekickID)c.mHomeAwaySidekicks[side];
    IChooseCaptain::NameComponent& name = c.mNameComponents[side];
    name.SetCaptainName(GetLOCCharacterName(team, false, phase == PHASE_CHOOSING_CAPTAIN));
    name.SetCaptainLogo(GetTeamName(team));
    if (phase == PHASE_CHOOSING_CAPTAIN)
        return;
    name.SetSidekickName(GetLOCSidekickName(sidekick));
    s_artworkOwed[side] = phase == PHASE_READY;
}

// The ready captain's three images; an image still loading drops a new request.
void load_ready_artwork(IChooseCaptain& c, int side)
{
    for (int i = 0; i < 3; i++)
        if (c.mAsyncImage[side][i]->mLoadState == LS_ISSUED_LOAD)
            return;
    s_artworkOwed[side] = false;
    const eTeamID team = (eTeamID)c.mHomeAwayTeam[side];
    const CaptainSidekickFilename::Type types[3] = { CaptainSidekickFilename::TYPE_CAPTAIN,
                                                     CaptainSidekickFilename::TYPE_CAPTAIN_OUTLINE,
                                                     CaptainSidekickFilename::TYPE_CAPTAIN_FLASH };
    for (int i = 0; i < 3; i++)
    {
        char file[0x80];
        CaptainSidekickFilename::Build(types[i], file, sizeof file, team, side);
        c.mAsyncImage[side][i]->QueueLoad(file, false);
    }
    c.mDidSwapCaptains[side] = false;
    c.StartSidekickMiniHead(side, (eSidekickID)c.mHomeAwaySidekicks[side]);
}

// A grid shows its highlight again from its Update, which only a side with a local pad runs.
template <typename T> void reveal_highlight(IGridComponent<T>* grid)
{
    if (!grid->mHighliteVisibilityAtAnimEnd)
        return;
    const TLSlide* slide = grid->mParentComponent->GetActiveSlide();
    if (slide != NULL && slide->m_time < slide->m_start + slide->m_duration)
        return;
    grid->mHighliteVisibilityAtAnimEnd = false;
    grid->mHighliteComponent->m_bVisible = true;
}

}   // namespace

extern "C" void PortOnlineGameInit(void)
{
    s_lockstepDefault = g_simLockstep;
    PortOnlineInit();
}

extern "C" int PortOnlineGameAtMainMenu(void) { return resting_on(SCENE_MAIN_MENU); }

extern "C" void PortOnlineAutotestFrame(int atMainMenu);

extern "C" void PortOnlineGameFrame(void)
{
    PortOnlineAutotestFrame(resting_on(SCENE_MAIN_MENU));
    if (s_stage == STAGE_NONE && !s_selectOpen && PortOnlinePanelIdle())
    {
        // The menu's online prompt; the session stays closed.
        if (resting_on(SCENE_MAIN_MENU))
        {
            refresh_language();
            PortOnlinePanelPrepare();
        }
        return;
    }
    refresh_language();
    PortOnlinePanelPrepare();
    sample_pads();
    PortOnlineUpdate(FixedUpdateTask::mTimeScale);
    const OnlineState st = *PortOnlineGetState();
    selection_frame(st);
    follow(st);
    slot_pads();
}

extern "C" int PortOnlineGameSelecting(void) { return s_selectOpen; }

extern "C" int PortOnlineGameSelectionSide(int port)
{
    if (!s_selectOpen || !s_captains)
        return -1;
    for (int side = 0; side < 2; side++)
        if (selection_port(side) == port &&
            s_captains->mComponentState[side].mCurrentPhase != PHASE_READY)
            return side;
    for (int side = 1; side >= 0; side--)
        if (selection_port(side) == port)
            return side;
    return -1;
}

void PortOnlineGameCaptainsCreated(ChooseCaptainsSceneV2* scene)
{
    if (!s_selectOpen)
        return;
    s_captains = &scene->mChooseCaptain;
    s_captainButtonMode = -1;
    s_artworkOwed[0] = s_artworkOwed[1] = false;
    const OnlineState& st = *PortOnlineGetState();
    for (int side = 0; side < 2; side++)
    {
        s_captains->mHomeAwayTeam[side] = st.desc.team[side];
        s_captains->mHomeAwaySidekicks[side] = st.desc.sidekick[side];
        s_captains->mComponentState[side].SetCurrentPhase(PHASE_CHOOSING_CAPTAIN);
        s_captains->MoveHighlightToCurrentCaptain(side);
        s_captains->SetupNameComponentToCurrentCaptain(side);
        s_sentTeam[side] = st.desc.team[side];
        s_sentSidekick[side] = st.desc.sidekick[side];
        s_sentPhase[side] = 1;
    }
    GameInfoManager* info = GameInfoManager::Instance();
    info->mUseCurGameSettings = true;
    GameplaySettings& g = info->mCurGameGameplayOptions;
    g.SkillLevel = (GameplaySettings::eSkillLevel)st.desc.skill;
    g.GameTime = st.desc.gameTime;
    g.PowerUps = st.desc.powerUps;
    g.Shoot2Score = st.desc.shoot2Score;
    g.BowserAttackEnabled = st.desc.bowser;
    scene->mButtons.SetState(st.self == st.host ? ButtonComponent::BS_A_AND_B_AND_Y
                                                : ButtonComponent::BS_A_AND_B);
}

int PortOnlineGameCaptainsUpdate(ChooseCaptainsSceneV2* scene, float dt)
{
    if (!s_selectOpen)
        return 0;
    const OnlineState st = *PortOnlineGetState();
    IChooseCaptain& c = scene->mChooseCaptain;
    s_captains = &c;
    const GameSceneManager* fe = GameSceneManager::Instance();
    if (!fe->mCurrentStackDepth ||
        fe->m_sceneStack[fe->mCurrentStackDepth - 1] != SCENE_CHOOSE_CAPTAINS)
    {
        c.UpdateAsyncImages();
        return 1;
    }
    if (st.selection.stage == 2)
    {
        GameSceneManager::Instance()->Push(SCENE_STADIUM_SELECT, SCREEN_FORWARD, true);
        return 1;
    }
    if (st.selection.stage != 1)
    {
        c.UpdateAsyncImages();
        return 1;
    }
    for (int side = 0; side < 2; side++)
    {
        if (selection_port(side) >= 0)
            continue;
        const int phase = st.selection.phase[side];
        if (phase < 1 || phase > 3)
            continue;
        const bool changed = c.mHomeAwayTeam[side] != st.desc.team[side] ||
                             c.mHomeAwaySidekicks[side] != st.desc.sidekick[side];
        c.mHomeAwayTeam[side] = st.desc.team[side];
        c.mHomeAwaySidekicks[side] = st.desc.sidekick[side];
        if (phase != c.mComponentState[side].mCurrentPhase)
        {
            c.mCaptainGridComponents[side]->MoveHighlightToTarget((eTeamID)st.desc.team[side]);
            c.mSidekickGridComponents[side]->MoveHighlightToTarget(
                (eSidekickID)st.desc.sidekick[side]);
            const eTeamID otherCursor = c.mCaptainGridComponents[side ^ 1]->GetSelectedItem();
            c.mComponentState[side].SetCurrentPhase((IChooseCaptain::ComponentState::Phase)phase);
            s_artworkOwed[side] = false;
            // The retail ready transition writes team IDs into both grids' icon-ID cursors.
            c.mCaptainGridComponents[side]->MoveHighlightToTarget((eTeamID)st.desc.team[side]);
            c.mCaptainGridComponents[side ^ 1]->MoveHighlightToTarget(otherCursor);
            for (int grid = 0; grid < 2; ++grid)
            {
                c.mCaptainGridComponents[grid]->SetAllItemsActive();
                if (c.mComponentState[grid ^ 1].mCurrentPhase >= PHASE_CHOOSING_SIDEKICK)
                    c.mCaptainGridComponents[grid]->SetValid((eTeamID)c.mHomeAwayTeam[grid ^ 1],
                                                             false);
                c.mCaptainGridComponents[grid]->UpdateSuperTeamIconState();
            }
        }
        else if (changed)
        {
            // Entering the phase again would replay the grid's IN slide.
            const eTeamID team = (eTeamID)st.desc.team[side];
            const eSidekickID sidekick = (eSidekickID)st.desc.sidekick[side];
            c.mCaptainGridComponents[side]->MoveHighlightToTarget(team);
            c.mSidekickGridComponents[side]->MoveHighlightToTarget(sidekick);
            refresh_selection(c, side, phase);
        }
        if (s_artworkOwed[side])
            load_ready_artwork(c, side);
        if (phase == PHASE_CHOOSING_CAPTAIN)
            reveal_highlight(c.mCaptainGridComponents[side]);
        else if (phase == PHASE_CHOOSING_SIDEKICK)
            reveal_highlight(c.mSidekickGridComponents[side]);
    }
    bool ready = true;
    for (int side = 0; side < 2; ++side)
        if (selection_port(side) >= 0 && c.mComponentState[side].mCurrentPhase != PHASE_READY)
            ready = false;
    const int mode = ready                ? ButtonComponent::BS_B_ONLY
                     : st.self == st.host ? ButtonComponent::BS_A_AND_B_AND_Y
                                          : ButtonComponent::BS_A_AND_B;
    if (s_captainButtonMode != mode)
    {
        scene->mButtons.SetState((ButtonComponent::ButtonState)mode);
        static unsigned short label[128];
        online_panel::text_utf16(ready ? online_panel::T_CHANGE_TEAM : online_panel::T_HINT_BACK,
                                 label, 128, true);
        scene->mButtons.mButtonLabels[0]->SetString(label);
        scene->mButtons.mAlreadyCentred = false;
        s_captainButtonMode = mode;
    }
    if (!ready && st.self == st.host &&
        g_pFEInput->JustPressed(FE_ALL_PADS, PAD_BUTTON_Y, false, NULL))
    {
        PortOnlineSelect(ONLINE_SELECT_OPTIONS, 0, 0, 0, 1);
        GameSceneManager::Instance()->Push(SCENE_QUICK_GAMEPLAY_OPTIONS, SCREEN_FORWARD, false);
        return 1;
    }
    if (c.Update(dt) == UPDATE_GO_BACK)
    {
        PortOnlineSelect(ONLINE_SELECT_CANCEL, 0, 0, 0, 0);
        return 1;
    }
    for (int side = 0; side < 2; side++)
    {
        if (selection_port(side) < 0)
            continue;
        const int phase = c.mComponentState[side].mCurrentPhase;
        const int team = phase == PHASE_CHOOSING_CAPTAIN
                             ? c.mCaptainGridComponents[side]->GetSelectedItem()
                             : c.mHomeAwayTeam[side];
        const int sk = phase == PHASE_CHOOSING_SIDEKICK
                           ? c.mSidekickGridComponents[side]->GetSelectedItem()
                           : c.mHomeAwaySidekicks[side];
        if (phase != s_sentPhase[side] || team != s_sentTeam[side] || sk != s_sentSidekick[side])
            if (PortOnlineSelect(ONLINE_SELECT_TEAM, side, team, sk, phase))
            {
                s_sentPhase[side] = phase;
                s_sentTeam[side] = team;
                s_sentSidekick[side] = sk;
            }
    }
    return 1;
}

extern "C" void PortOnlineGameSaveOptions(void)
{
    const OnlineState& st = *PortOnlineGetState();
    if (!s_selectOpen || st.self != st.host)
        return;
    OnlineMatchDesc d = st.desc;
    const GameplaySettings& g = GameInfoManager::Instance()->mCurGameGameplayOptions;
    d.skill = g.SkillLevel;
    // The retail menu has no entry past 900 and shows a longer length as 120.
    if (st.desc.gameTime <= 900 || g.GameTime != 120)
        d.gameTime = g.GameTime;
    d.powerUps = g.PowerUps;
    d.shoot2Score = g.Shoot2Score;
    d.bowser = g.BowserAttackEnabled;
    PortOnlineSetDesc(&d);
    PortOnlineSelect(ONLINE_SELECT_OPTIONS, 0, 0, 0, 0);
}

extern "C" int PortOnlineGameStadiumUnlocked(void) { return s_stadiumUnlocked; }
extern "C" int PortOnlineGameStadiumOpen(int own) { return s_selectOpen ? 1 : own; }
extern "C" int PortOnlineGameOptionsOpen(void)
{
    const GameSceneManager* fe = GameSceneManager::Instance();
    return s_selectOpen && fe && fe->mCurrentStackDepth &&
           fe->m_sceneStack[fe->mCurrentStackDepth - 1] == SCENE_QUICK_GAMEPLAY_OPTIONS;
}

void PortOnlineGameStadiumButtons(ButtonComponent* buttons, bool unlocked)
{
    if (!s_selectOpen)
        return;
    s_stadiumUnlocked = unlocked;
    const OnlineState& st = *PortOnlineGetState();
    const bool canStart = unlocked && st.self == st.host;
    buttons->SetState(canStart ? ButtonComponent::BS_A_AND_B : ButtonComponent::BS_B_ONLY);
    if (canStart)
    {
        static unsigned short label[128];
        online_panel::text_utf16(online_panel::T_START_MATCH, label, 128, true);
        buttons->mButtonLabels[1]->SetString(label);
    }
}

extern "C" int PortOnlineGameSelectStadium(int stadium)
{
    if (!s_selectOpen)
        return 0;
    const OnlineState& st = *PortOnlineGetState();
    if (st.self == st.host && st.selection.stage == 2)
    {
        OnlineMatchDesc d = st.desc;
        d.stadium = (int8_t)stadium;
        if (PortOnlineSetDesc(&d))
            PortOnlineSelect(ONLINE_SELECT_FINISH, 0, 0, 0, 0);
    }
    return 1;
}

extern "C" int PortOnlineGameSelectionBack(void)
{
    if (!s_selectOpen)
        return 0;
    PortOnlineSelect(ONLINE_SELECT_BACK, 0, 0, 0, 0);
    return 1;
}

extern "C" void PortOnlineGameShutdown(void) { PortOnlineShutdown(); }

extern "C" int PortOnlineGameHoldLoading(void)
{
    return s_stage == STAGE_LOAD || s_stage == STAGE_PIPELINES || s_stage == STAGE_READY;
}

void PortOnlineGameApplySettings(GameInfoManager* info)
{
    if (s_stage != STAGE_LOAD)
        return;
    const OnlineMatchDesc& d = s_desc;
    GameplaySettings& g = info->mCurGameGameplayOptions;
    g.SkillLevel = (GameplaySettings::eSkillLevel)d.skill;
    g.GameTime = d.gameTime;
    g.PowerUps = d.powerUps != 0;
    g.Shoot2Score = d.shoot2Score != 0;
    g.BowserAttackEnabled = d.bowser != 0;
    bool* items[8] = {
        &info->mCurGamePowerupOptions.RedShells,  &info->mCurGamePowerupOptions.GreenShells,
        &info->mCurGamePowerupOptions.BlueShells, &info->mCurGamePowerupOptions.SpinyShells,
        &info->mCurGamePowerupOptions.Starman,    &info->mCurGamePowerupOptions.Twister,
        &info->mCurGamePowerupOptions.Bobombs,    &info->mCurGamePowerupOptions.Bananas
    };
    for (int i = 0; i < 8; i++)
        *items[i] = (d.powerupMask & (1u << i)) != 0;
    CheatSettings& c = info->mCurGameCheatOptions;
    c.mCustomPowerups = (CheatSettings::CustomPowerups)d.customPowerups;
    c.mStunnedGoalies = (d.cheats & ONLINE_CHEAT_STUNNED_GOALIES) != 0;
    c.mInfinitePowerups = (d.cheats & ONLINE_CHEAT_INFINITE_POWERUPS) != 0;
    c.mCheatTBD1Enabled = (d.cheats & ONLINE_CHEAT_TILTING_FIELD) != 0;
    c.mCheatTBD2Enabled = (d.cheats & ONLINE_CHEAT_PERFECT_STRIKES) != 0;
}
