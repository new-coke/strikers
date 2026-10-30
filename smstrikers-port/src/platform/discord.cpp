#include "port/discord.h"
#include "port/online.h"
#include "port/online_panel.h"
#include "port/region.h"

#include <borealis/discord.hpp>

#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace {

using Clock = std::chrono::steady_clock;
using borealis::discord::Presence;

// Discord's documented limit on presence updates; borealis does not enforce it.
constexpr auto kMinInterval = std::chrono::seconds(15);

// cGame::Update runs each fixed step; this long without it, outside the menu, is the match over.
constexpr auto kMatchStale = std::chrono::seconds(2);

// Seconds of countdown drift worth a send; goals stop the match clock but not Discord's.
constexpr int64_t kClockSlack = 2;

bool s_initialized = false;
int64_t s_startTime = 0;

Presence s_wanted;
Presence s_sent;
bool s_sentAny = false;
Clock::time_point s_lastSent;

bool s_inMatch = false;
bool s_paused = false;
Presence s_match;
int64_t s_suddenDeathSince = 0;
Clock::time_point s_lastMatch;

// The US disc's own names, in the order of the game's enums.
constexpr const char* kCaptains[] = {"Daisy", "Donkey Kong", "Luigi", "Mario", "Peach",
                                     "Waluigi", "Wario", "Yoshi", "Super Team"};
constexpr const char* kSidekicks[] = {"Toad", "Koopa", "Hammer Bros.", "Birdo"};
constexpr const char* kModes[] = {"Grudge Match", "Mushroom Cup", "Flower Cup", "Star Cup",
                                  "Bowser Cup", "Super Mushroom Cup", "Super Flower Cup",
                                  "Super Star Cup", "Super Bowser Cup", "Custom Battle"};
constexpr const char* kStadiums[] = {"Pipeline Central", "The Palace", "Konga Coliseum",
                                     "The Underground", "Crater Field", "Bowser Stadium",
                                     "The Battle Dome"};

// Art asset keys uploaded to kApplicationId, in eTeamID order.
constexpr const char* kCaptainImages[] = {"daisy", "donkeykong", "luigi", "mario", "peach",
                                          "waluigi", "wario", "yoshi", "superteam"};

template <size_t N>
const char* name_of(const char* const (&table)[N], int v) {
    return (v >= 0 && v < (int)N) ? table[v] : nullptr;
}

const char* or_unknown(const char* name) { return name != nullptr ? name : "?"; }

int64_t unix_now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// The Super Team has no sidekick of its own.
std::string team_name(int team, int sidekick) {
    const char* kick = name_of(kSidekicks, sidekick);
    return kick != nullptr ? std::string(or_unknown(name_of(kCaptains, team))) + " (" + kick + ")"
                           : std::string(or_unknown(name_of(kCaptains, team)));
}

// BaseCup::mRoundNumber: league rounds count from 0, and the knockout stages are negative.
std::string round_name(int round) {
    if (round >= 0)
        return " round " + std::to_string(round + 1);
    if (round == -4)
        return " quarter-final";
    if (round == -3)
        return " semi-final";
    if (round == -2 || round == -1)
        return " final";
    return "";
}

bool enabled() {
    const char* v = std::getenv("STRIKERS_DISCORD");
    if (v == nullptr)
        return true;
    std::string s;
    for (const char* p = v; *p != '\0'; ++p)
        if (*p != ' ' && *p != '\t')
            s += (char)std::tolower((unsigned char)*p);
    return !(s == "0" || s == "false" || s == "no" || s == "off");
}

constexpr const char* kApplicationId = "1550132237826261123";

// By PortDiscordMenu.
constexpr const char* kMenus[] = { "In the menus",
                                   "Title screen",
                                   "Main menu",
                                   "Choosing teams",
                                   "Choosing a stadium",
                                   "Setting up a cup",
                                   "Setting up a Super Cup",
                                   "Checking the standings",
                                   "Setting up a Custom Battle",
                                   "In the Trophy Room",
                                   "Options",
                                   "Watching the credits" };
static_assert(std::size(kMenus) == PORT_DISCORD_MENU_COUNT);

int s_menu = PORT_DISCORD_MENU_OTHER;

Presence menus() {
    Presence p{};
    p.details = or_unknown(name_of(kMenus, s_menu));
    p.startTimestamp = s_startTime;
    p.largeImageKey = "app-icon";
    p.largeImageText = std::string("Super Mario Strikers (") + port_disc_game_code() +
                       port_disc_maker_code() + ")";
    return p;
}

bool near(int64_t a, int64_t b) { return a - b <= kClockSlack && b - a <= kClockSlack; }

// Equal but for timestamps within the slack.
bool same(const Presence& a, const Presence& b) {
    Presence x = a;
    Presence y = b;
    x.startTimestamp = y.startTimestamp = 0;
    x.endTimestamp = y.endTimestamp = 0;
    return x == y && near(a.startTimestamp, b.startTimestamp) &&
           near(a.endTimestamp, b.endTimestamp) &&
           (a.startTimestamp == 0) == (b.startTimestamp == 0) &&
           (a.endTimestamp == 0) == (b.endTimestamp == 0);
}

// A join secret's format version; the host's public address follows it.
constexpr std::string_view kSecretPrefix = "s1 ";

// Decimal digits at `i` without a leading zero, up to `max`; -1 when there are none or more.
long take_number(std::string_view s, size_t& i, long max)
{
    const size_t start = i;
    long v = 0;
    while (i < s.size() && i - start < 5 && s[i] >= '0' && s[i] <= '9')
        v = v * 10 + (s[i++] - '0');
    if (i == start || (s[start] == '0' && i - start > 1) || v > max)
        return -1;
    return v;
}

// Exactly "a.b.c.d:port", with an address the internet routes to.
bool public_room_address(std::string_view s)
{
    uint32_t ip = 0;
    size_t i = 0;
    for (int octet = 0; octet < 4; ++octet)
    {
        if (octet > 0 && (i >= s.size() || s[i++] != '.'))
            return false;
        const long v = take_number(s, i, 255);
        if (v < 0)
            return false;
        ip = ip << 8 | (uint32_t)v;
    }
    if (i >= s.size() || s[i++] != ':')
        return false;
    const long port = take_number(s, i, 65535);
    if (port < 1 || i != s.size())
        return false;
    // This network, private, carrier NAT, loopback, link-local, multicast and reserved.
    const uint32_t a = ip >> 24;
    const uint32_t b = ip >> 16 & 0xff;
    return !(a == 0 || a == 10 || (a == 100 && b >= 64 && b < 128) || a == 127 ||
             (a == 169 && b == 254) || (a == 172 && b >= 16 && b < 32) || (a == 192 && b == 168) ||
             a >= 224);
}

// Online, every human in the room holds a pad slot; -2 offline, -1 no local human, 2 both sides.
int online_side()
{
    const OnlineState& st = *PortOnlineGetState();
    if (!PortOnlineInMatch() || st.self < 0 || st.self >= ONLINE_MAX_MACHINES)
        return -2;
    const OnlineMachine& m = st.machines[st.self];
    bool home = false;
    bool away = false;
    for (int h = 0; h < m.humanCount && h < ONLINE_MAX_LOCAL_HUMANS; ++h)
    {
        home |= m.humans[h].side == 0;
        away |= m.humans[h].side == 1;
    }
    return home && away ? 2 : home ? 0 : away ? 1 : -1;
}

// The online room as a Discord party; only a host the internet reaches offers Join.
Presence with_room(Presence p)
{
    const OnlineState& st = *PortOnlineGetState();
    const bool inRoom = st.phase == ONLINE_PHASE_HOSTING || st.phase == ONLINE_PHASE_ROOM ||
                        st.phase == ONLINE_PHASE_LOADING || st.phase == ONLINE_PHASE_MATCH ||
                        st.phase == ONLINE_PHASE_POST_MATCH;
    if (!inRoom || st.roomId == 0 || st.self < 0)
        return p;
    const bool host = st.self == st.host;

    // A room holds four humans across all its machines.
    int humans = 0;
    for (const OnlineMachine& m : st.machines)
        if (m.present)
            humans += m.humanCount;
    char id[32];
    std::snprintf(id, sizeof id, "strikers-room-%08x", (unsigned)st.roomId);
    p.partyId = id;
    p.partySize = humans;
    p.partyMax = ONLINE_MAX_SLOTS;

    // A host takes joins only between matches.
    if (host && (st.phase == ONLINE_PHASE_HOSTING || st.phase == ONLINE_PHASE_ROOM) &&
        st.publicState == ONLINE_PUBLIC_OPEN && public_room_address(st.publicAddress))
        p.joinSecret = std::string(kSecretPrefix) + st.publicAddress;

    if (!s_inMatch)
    {
        p.details = "In an online room";
        p.state = host ? "Hosting" : "Guest";
    }
    return p;
}

void flush() {
    const Presence wanted = with_room(s_wanted);
    const Clock::time_point now = Clock::now();
    if (s_sentAny && (same(wanted, s_sent) || now - s_lastSent < kMinInterval))
        return;
    s_sentAny = true;
    s_lastSent = now;
    s_sent = wanted;
    borealis::discord::update_presence(wanted);
}

void join_from_discord(std::string_view secret)
{
    const std::string address(secret.substr(0, kSecretPrefix.size()) == kSecretPrefix
                                  ? secret.substr(kSecretPrefix.size())
                                  : std::string_view());
    const int phase = PortOnlineGetState()->phase;
    if (!public_room_address(address) ||
        (phase != ONLINE_PHASE_IDLE && phase != ONLINE_PHASE_ERROR))
    {
        std::fprintf(stderr, "[port] discord: ignored a join from Discord\n");
        return;
    }
    if (phase == ONLINE_PHASE_ERROR)
        PortOnlineLeave();
    if (PortOnlineJoin(address.c_str(), nullptr) == 0)
        return;
    std::fprintf(stderr, "[port] discord: joining a room from Discord\n");
    PortOnlinePanelSetOpen(1);
}

} // namespace

void PortDiscordInit(void) {
    if (s_initialized)
        return;
    if (!enabled())
        return;

    s_startTime = unix_now();

    borealis::AppInfo info{};
    info.discordApplicationId = kApplicationId;

    borealis::discord::EventHandlers handlers{};
    handlers.ready = [](const borealis::discord::User&) {
        std::fprintf(stderr, "[port] discord: connected\n");
    };
    handlers.error = [](int code, std::string_view message) {
        std::fprintf(stderr, "[port] discord: error %d: %.*s\n", code, (int)message.size(),
                     message.data());
    };
    handlers.activityJoin = join_from_discord;

    s_initialized = borealis::discord::initialize(info, std::move(handlers));
    s_wanted = menus();
}

void PortDiscordUpdate(int inMenuState, int menu)
{
    if (!s_initialized)
        return;
    borealis::discord::run_callbacks();
    if (menu != s_menu)
    {
        s_menu = menu;
        if (!s_inMatch)
            s_wanted = menus();
    }

    if (s_inMatch) {
        const Clock::time_point now = Clock::now();
        if (inMenuState) {
            s_lastMatch = now;
            if (!s_paused) {
                s_paused = true;
                s_wanted = s_match;
                s_wanted.state += ", paused";
                s_wanted.startTimestamp = s_wanted.endTimestamp = 0;
            }
        } else if (now - s_lastMatch > kMatchStale) {
            s_inMatch = false;
            s_paused = false;
            s_suddenDeathSince = 0;
            s_wanted = menus();
        }
    }
    flush();
}

void PortDiscordSetMatch(const PortDiscordMatch* match) {
    if (!s_initialized || match == nullptr)
        return;
    s_inMatch = true;
    s_paused = false;
    s_lastMatch = Clock::now();

    const int online = online_side();
    const int me = online == 0 || online == 1 ? online : match->userSide == 1 ? 1 : 0;
    const int them = 1 - me;
    const char* stadium = name_of(kStadiums, match->stadium);

    Presence p{};
    if (online == -1)
        p.details = std::string("Watching ") + or_unknown(name_of(kCaptains, match->team[0])) +
                    " vs " + or_unknown(name_of(kCaptains, match->team[1]));
    else
        p.details = std::string("Playing as ") + or_unknown(name_of(kCaptains, match->team[me]));
    if (stadium != nullptr)
        p.details += std::string(" at ") + stadium;

    if (match->mode == -1) {
        p.state = "Strikers 101";
        p.startTimestamp = s_startTime;
    } else {
        p.state = online != -2 ? "Online match" : or_unknown(name_of(kModes, match->mode));
        if (match->hasRound)
            p.state += round_name(match->round);
        if (online != -1)
            p.state += std::string(" vs ") + or_unknown(name_of(kCaptains, match->team[them]));
        p.state += std::string(match->finished ? ", final score " : ", ") +
                   std::to_string(match->score[me]) + " - " + std::to_string(match->score[them]);
        if (match->finished)
        {
            s_suddenDeathSince = 0;
        }
        else if (match->suddenDeath)
        {
            if (s_suddenDeathSince == 0)
                s_suddenDeathSince = unix_now();
            p.state += ", sudden death";
            p.startTimestamp = s_suddenDeathSince;
        }
        else
        {
            s_suddenDeathSince = 0;
            const float remaining = match->duration - match->clock;
            p.endTimestamp = unix_now() + (remaining > 0.0f ? (int64_t)remaining : 0);
        }
    }

    const char* myImage = name_of(kCaptainImages, match->team[me]);
    const char* theirImage = name_of(kCaptainImages, match->team[them]);
    p.largeImageKey = myImage != nullptr ? myImage : "";
    p.largeImageText = team_name(match->team[me], match->sidekick[me]);
    p.smallImageKey = theirImage != nullptr ? theirImage : "";
    p.smallImageText = team_name(match->team[them], match->sidekick[them]);

    s_match = p;
    s_wanted = std::move(p);
}

void PortDiscordShutdown(void) {
    if (!s_initialized)
        return;
    borealis::discord::clear_presence();
    borealis::discord::shutdown();
    s_initialized = false;
}
