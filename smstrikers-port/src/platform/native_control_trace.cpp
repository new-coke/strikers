#include "port/native_control_trace.hpp"

#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#if defined(STRIKERS_CONTROL_TRACE_TEST)
#include "control_trace_fixture_world.hpp"
#else
#include "Game/AI/AIPad.h"
#include "Game/AI/Fielder.h"
#include "Game/AI/FielderActions.h"
#include "Game/AI/ShotMeter.h"
#include "Game/Ball.h"
#include "Game/CharacterTemplate.h"
#include "Game/EventDataTypes.h"
#include "Game/Game.h"
#include "Game/Team.h"
#include "NL/platpad.h"
#include "dolphin/os.h"
#include "port/config.h"
#endif

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern PowerupBase* g_pPowerups[25];

namespace port::control_trace
{
bool enabled = false;

namespace
{
constexpr unsigned kRecordLimit = 16384;
constexpr unsigned kPeriodicTicks = 10;
bool initialized = false;
bool requested = false;
bool exhausted = false;
unsigned records = 0;
unsigned match = 0;
unsigned tick = 0;
float simulationTime = 0.0f;

struct PadMemo
{
    bool seen = false;
    int err = 0;
    unsigned buttons = 0, pressed = 0, released = 0;
};
struct PlayerMemo
{
    bool seen = false;
    int present = 0, pad = -1, team = -1, character = -1, kind = -1;
    int action = -1, meter = -1, power = -1, powerCount = 0, effectActive = 0;
    float sts1 = -1.0f, sts2 = -1.0f;
};
struct PowerMemo
{
    bool seen = false;
    int present = 0, type = -1, thrower = -1, destroying = 0;
};
struct GameMemo
{
    bool seen = false;
    int state = -1, owner = -1, target = -1, shooter = -1, score[2] = {};
    int item[4] = {}, count[4] = {};
};
PadMemo pads[4];
PlayerMemo players[10];
PowerMemo powers[25];
GameMemo game;
bool missingWorldReported = false;

void emit(const char* kind, const char* format, ...) noexcept
{
    if (!enabled)
        return;
    if (records >= kRecordLimit - 1)
    {
        ++records;
        OSReport("[control] seq=%u match=%u tick=%u kind=truncated limit=%u\n",
            records, match, tick, kRecordLimit);
        exhausted = true;
        enabled = false;
        return;
    }
    char payload[640];
    va_list args;
    va_start(args, format);
    const int length = std::vsnprintf(payload, sizeof(payload), format, args);
    va_end(args);
    ++records;
    if (length < 0 || static_cast<unsigned>(length) >= sizeof(payload))
    {
        OSReport("[control] seq=%u match=%u tick=%u kind=format_error\n", records, match, tick);
        exhausted = true;
        enabled = false;
        return;
    }
    OSReport("[control] seq=%u match=%u tick=%u sim=%.6g kind=%s %s\n",
        records, match, tick, static_cast<double>(simulationTime), kind, payload);
}

// Event pointers are compared against current live players before any dereference.
int actor_index(const cPlayer* actor) noexcept
{
    if (!actor)
        return -1;
    for (int i = 0; i < 10; ++i)
    {
        const cCharacter* live = g_pCharacters[i];
        if (live && (live->m_eClassType == FIELDER || live->m_eClassType == GOALIE)
            && static_cast<const cPlayer*>(live) == actor)
            return i;
    }
    return -1;
}

int player_pad(const cPlayer* player) noexcept
{
    if (!player || !player->m_pController)
        return -1;
    for (int i = 0; i < 4; ++i)
        if (player->m_pController == &AIPadManager::mAIPads[i])
            return i;
    return -1;
}

int actor_pad(int index) noexcept
{
    return index < 0 ? -1 : player_pad(static_cast<const cPlayer*>(g_pCharacters[index]));
}

int team_index(const cTeam* team) noexcept
{
    if (!team)
        return -1;
    for (int i = 0; i < 2; ++i)
        if (g_pTeams[i] == team)
            return i;
    return -1;
}

bool player_changed(const PlayerMemo& a, const PlayerMemo& b) noexcept
{
    return !a.seen || a.present != b.present || a.pad != b.pad || a.team != b.team
        || a.character != b.character || a.kind != b.kind || a.action != b.action
        || a.meter != b.meter || a.power != b.power || a.powerCount != b.powerCount
        || a.effectActive != b.effectActive
        || a.sts1 != b.sts1 || a.sts2 != b.sts2;
}
}

void begin_match(bool rematch, bool straight_to_kickoff) noexcept
{
    if (!initialized)
    {
        initialized = true;
        const char* option = std::getenv("STRIKERS_VITA_CONTROL_TRACE");
        requested = option && std::strcmp(option, "1") == 0 && PortDiagnosticsEnabled();
    }
    if (!requested || exhausted)
        return;
    enabled = true;
    ++match;
    tick = 0;
    simulationTime = 0.0f;
    for (auto& value : pads) value = {};
    for (auto& value : players) value = {};
    for (auto& value : powers) value = {};
    game = {};
    missingWorldReported = false;
    emit("begin", "v=1 rematch=%d kickoff=%d limit=%u periodic_ticks=%u event_time=dispatch",
        rematch ? 1 : 0, straight_to_kickoff ? 1 : 0, kRecordLimit, kPeriodicTicks);
}

void end_match() noexcept
{
    if (enabled)
        emit("end", "reason=destroy_characters");
    enabled = false;
}

void after_fixed_update(float simulation_time) noexcept
{
    if (!enabled)
        return;
    const PadStatus* const currentPad = GetCurrentPadStatus();
    ++tick;
    simulationTime = simulation_time;
    const bool periodic = tick == 1 || tick % kPeriodicTicks == 0;
    if (!g_pGame || !g_pBall || !g_pTeams[0] || !g_pTeams[1])
    {
        if (!missingWorldReported)
            emit("world_unavailable", "game=%d ball=%d home=%d away=%d",
                g_pGame != nullptr, g_pBall != nullptr, g_pTeams[0] != nullptr, g_pTeams[1] != nullptr);
        missingWorldReported = true;
        return;
    }
    missingWorldReported = false;
    GameMemo next;
    next.seen = true;
    next.state = static_cast<int>(g_pGame->m_eGameState);
    next.owner = actor_index(g_pBall->m_pOwner);
    next.target = actor_index(g_pBall->m_pPassTarget);
    next.shooter = actor_index(g_pBall->m_pShooter);
    bool changed = !game.seen || next.state != game.state || next.owner != game.owner
        || next.target != game.target || next.shooter != game.shooter;
    for (int t = 0; t < 2; ++t)
    {
        next.score[t] = g_pTeams[t]->m_nScore;
        changed |= next.score[t] != game.score[t];
        for (int s = 0; s < 2; ++s)
        {
            const int index = 2 * t + s;
            next.item[index] = static_cast<int>(g_pTeams[t]->m_ePowerupList[s].eType);
            next.count[index] = g_pTeams[t]->m_ePowerupList[s].nnumOfPowerups;
            changed |= next.item[index] != game.item[index] || next.count[index] != game.count[index];
        }
    }
    if (periodic || changed)
    {
        const auto& pos = g_pBall->m_v3Position;
        const auto& vel = g_pBall->m_v3Velocity;
        emit("world", "state=%d score=%d,%d owner=%d target=%d shooter=%d ball=%.5g,%.5g,%.5g velocity=%.5g,%.5g,%.5g items=%d:%d,%d:%d;%d:%d,%d:%d",
            next.state, next.score[0], next.score[1], next.owner, next.target, next.shooter,
            double(pos.x), double(pos.y), double(pos.z), double(vel.x), double(vel.y), double(vel.z),
            next.item[0], next.count[0], next.item[1], next.count[1], next.item[2], next.count[2], next.item[3], next.count[3]);
    }
    game = next;
    if (!enabled)
        return;

    if (PadStatus::s_Current && currentPad)
    {
        for (int i = 0; i < 4 && enabled; ++i)
        {
            const auto& raw = PadStatus::s_Current[i];
            PadMemo now;
            now.seen = true;
            now.err = raw.err;
            now.buttons = raw.button;
            now.pressed = currentPad->m_justPressed[i];
            now.released = currentPad->m_justReleased[i];
            const PadMemo& old = pads[i];
            if (periodic || !old.seen || now.err != old.err || now.buttons != old.buttons
                || now.pressed != old.pressed || now.released != old.released)
            {
                const auto& input = currentPad->m_GameCubePads[i];
                emit("input", "pad=%d err=%d buttons=%04x pressed=%04x released=%04x raw_l=%d,%d raw_r=%d,%d axes=%.5g,%.5g,%.5g,%.5g triggers=%.5g,%.5g",
                    i, now.err, now.buttons, now.pressed, now.released,
                    int(raw.stickX), int(raw.stickY), int(raw.substickX), int(raw.substickY),
                    double(input.fAnalogLeftX), double(input.fAnalogLeftY), double(input.fAnalogRightX),
                    double(input.fAnalogRightY), double(input.fTriggerLeft), double(input.fTriggerRight));
            }
            pads[i] = now;
        }
    }

    for (int i = 0; i < 10 && enabled; ++i)
    {
        const cCharacter* character = g_pCharacters[i];
        PlayerMemo now;
        now.seen = true;
        const cPlayer* player = nullptr;
        const cFielder* fielder = nullptr;
        float meterTime = 0.0f;
        if (character && (character->m_eClassType == FIELDER || character->m_eClassType == GOALIE))
        {
            player = static_cast<const cPlayer*>(character);
            now.present = 1;
            now.pad = player_pad(player);
            now.team = team_index(player->m_pTeam);
            now.character = static_cast<int>(character->m_eCharacterClass);
            now.kind = static_cast<int>(character->m_eClassType);
            if (character->m_eClassType == FIELDER)
            {
                fielder = static_cast<const cFielder*>(player);
                now.action = static_cast<int>(fielder->m_eActionState);
                now.power = static_cast<int>(fielder->m_ePowerup);
                now.powerCount = fielder->mnNumPowerups;
                now.effectActive = fielder->m_tPowerupEffectTime.m_uPackedTime != 0;
                if (fielder->m_pShotMeter)
                {
                    now.meter = static_cast<int>(fielder->m_pShotMeter->m_eShotMeterState);
                    // Reset initializes time before the active states become visible.
                    if (now.meter == SHOT_METER_ACTIVE || now.meter == SHOT_METER_STS_ACTIVE
                        || now.meter == SHOT_METER_STS_TRANSISTION)
                        meterTime = fielder->m_pShotMeter->m_fTime;
                }
                if (now.action == ACTION_SHOOT_TO_SCORE)
                {
                    now.sts1 = fielder->mActionShootToScoreVars.fFrameButtonDownTime1;
                    now.sts2 = fielder->mActionShootToScoreVars.fFrameButtonDownTime2;
                }
            }
        }
        const bool human = now.pad >= 0 || players[i].pad >= 0;
        if (!players[i].seen || now.present != players[i].present
            || (human && (periodic || player_changed(players[i], now))))
        {
            if (!player)
                emit("player", "index=%d present=0", i);
            else
                emit("player", "index=%d present=1 pad=%d team=%d class=%d kind=%d action=%d anim=%d pos=%.5g,%.5g,%.5g speed=%.5g desired=%.5g ball=%d power=%d count=%d effect_active=%d meter=%d charge_time=%.5g sts=%.5g,%.5g",
                    i, now.pad, now.team, now.character, now.kind, now.action, int(player->m_eAnimID),
                    double(player->m_v3Position.x), double(player->m_v3Position.y), double(player->m_v3Position.z),
                    double(player->m_fActualSpeed), double(player->m_fDesiredSpeed),
                    g_pBall->m_pOwner == player, now.power, now.powerCount, now.effectActive, now.meter, double(meterTime),
                    double(now.sts1), double(now.sts2));
        }
        players[i] = now;
    }
    for (int i = 0; i < 25 && enabled; ++i)
    {
        const PowerupBase* object = g_pPowerups[i];
        PowerMemo now;
        now.seen = true;
        if (object)
        {
            now.present = 1;
            now.type = static_cast<int>(object->m_eType);
            now.thrower = actor_index(object->m_pThrower);
            now.destroying = object->m_bShouldDestroy ? 1 : 0;
        }
        const auto& old = powers[i];
        if ((!old.seen && now.present) || old.present != now.present || old.type != now.type
            || old.thrower != now.thrower || old.destroying != now.destroying)
            emit("powerup_slot", "slot=%d present=%d type=%d thrower=%d destroying=%d",
                i, now.present, now.type, now.thrower, now.destroying);
        powers[i] = now;
    }
}

void observe_event(Event* event) noexcept
{
    if (!enabled || !event)
        return;
    const unsigned id = static_cast<unsigned>(event->m_uEventID);
    unsigned expected = 0;
    switch (id)
    {
    case 0x0d: expected = 0x121; break;
    case 0x0e: expected = 0x131; break;
    case 0x14: case 0x15: case 0x3f: expected = 0x14a; break;
    case 0x16: case 0x17: case 0x19: case 0x1a: expected = 0x19a; break;
    default: return;
    }
    const unsigned actual = static_cast<unsigned>(port_event_data_id(&event->m_data));
    if (actual != expected)
    {
        emit("payload_mismatch", "event=%x expected=%x actual=%x", id, expected, actual);
        return;
    }
    int actor = -1, target = -1, capturedPad = -1, result = -1;
    const char* name = nullptr;
    if (id == 0x0d)
    {
        const auto* data = static_cast<const ReceiveBallData*>(&event->m_data);
        actor = actor_index(data->pReceiver);
        result = static_cast<int>(data->eResult);
        name = "receive";
    }
    else if (id == 0x0e)
    {
        const auto* data = static_cast<const PassBallData*>(&event->m_data);
        actor = actor_index(data->pPasser);
        target = actor_index(data->pTarget);
        capturedPad = data->mPasserControllerID;
        name = "pass";
    }
    else if (expected == 0x14a)
    {
        const auto* data = static_cast<const ShotAtGoalData*>(&event->m_data);
        actor = actor_index(data->pShooter);
        name = id == 0x14 ? "shot_release" : (id == 0x15 ? "shot_windup" : "special_start");
    }
    else
    {
        const auto* data = static_cast<const PlayerAttackData*>(&event->m_data);
        actor = actor_index(data->pAttacker);
        target = actor_index(data->pTarget);
        capturedPad = data->nAttackerPadID;
        name = id == 0x16 ? "hit_start" : (id == 0x17 ? "hit_contact" : (id == 0x19 ? "slide_stat" : "slide_contact"));
        // fAttackIntensity is not initialized by every producer; never read it.
    }
    emit("event", "event=%x name=%s actor=%d pad_now=%d target=%d target_pad_now=%d captured_pad=%d result=%d time=dispatch",
        id, name, actor, actor_pad(actor), target, actor_pad(target), capturedPad, result);
}
}
#endif
