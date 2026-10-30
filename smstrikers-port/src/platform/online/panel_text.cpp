// The online panel's words, from the same Qt Linguist catalogues as the settings app.

#include "panel_text.h"
#include <cstring>

#ifndef QT_TRANSLATE_NOOP
#define QT_TRANSLATE_NOOP(context, source) source
#endif

namespace online_panel
{

namespace
{

struct Entry
{
    Text id;
    const char* args;   // the printf conversions in order: s, d, u or f
    const char* english;
};

// clang-format off
const Entry kEnglish[] = {
    { T_CHANGE_TEAM, "", QT_TRANSLATE_NOOP("Online", "Change team") },
    { T_CPU, "", QT_TRANSLATE_NOOP("Online", "CPU") },
    { T_CPU_HOST, "", QT_TRANSLATE_NOOP("Online", "CPU (host chooses)") },
    { T_ENTER_KEY, "", QT_TRANSLATE_NOOP("Online", "Enter") },
    { T_ESCAPE_KEY, "", QT_TRANSLATE_NOOP("Online", "Esc") },
    { T_CHOOSE_TEAMS_WAIT, "s", QT_TRANSLATE_NOOP("Online", "Waiting for %s to choose teams") },
    { T_CONNECTION_DETAILS, "", QT_TRANSLATE_NOOP("Online", "Connection details") },
    { T_INVITE, "", QT_TRANSLATE_NOOP("Online", "Invite player") },
    { T_CONNECTED, "", QT_TRANSLATE_NOOP("Online", "Connected") },
    { T_CHOOSING_CAPTAIN, "", QT_TRANSLATE_NOOP("Online", "Choosing captain") },
    { T_CHOOSING_SIDEKICK, "", QT_TRANSLATE_NOOP("Online", "Choosing sidekick") },
    { T_STADIUM_LOCKED, "", QT_TRANSLATE_NOOP("Online", "The host hasn't unlocked this stadium. Choose another.") },
    { T_STAGE_STADIUM, "", QT_TRANSLATE_NOOP("Online", "The host chooses the stadium") },
    { T_SHARED_BACK, "", QT_TRANSLATE_NOOP("Online", "Back returns everyone to team selection") },
    { T_ADVANCED, "", QT_TRANSLATE_NOOP("Online", "Advanced connection settings") },
    { T_OTHER_NETWORKS, "", QT_TRANSLATE_NOOP("Online", "Other network addresses") },
    { T_COPY_ADDRESS, "", QT_TRANSLATE_NOOP("Online", "Copy address") },
    { T_COPIED, "", QT_TRANSLATE_NOOP("Online", "Address copied") },
    { T_JOIN_HELP, "", QT_TRANSLATE_NOOP("Online", "Enter the host's address. Include the port if supplied.") },
    { T_RULES, "", QT_TRANSLATE_NOOP("Online", "Editing match rules") },
    { T_LOCAL_RUMBLE, "", QT_TRANSLATE_NOOP("Online", "Rumble only affects this device") },

    { T_ONLINE, "", QT_TRANSLATE_NOOP("Online", "Online") },
    { T_CREATE_ROOM, "", QT_TRANSLATE_NOOP("Online", "Create room") },
    { T_CREATE_ROOM_SUB, "", QT_TRANSLATE_NOOP("Online", "Invite players by address or on this network") },
    { T_NEARBY, "", QT_TRANSLATE_NOOP("Online", "Nearby rooms") },
    { T_NEARBY_SUB, "", QT_TRANSLATE_NOOP("Online", "Rooms on this network") },
    { T_JOIN_ADDRESS, "", QT_TRANSLATE_NOOP("Online", "Join by address") },
    { T_JOIN_ADDRESS_SUB, "", QT_TRANSLATE_NOOP("Online", "The host's IP address or Tailscale device name") },
    { T_RECENT, "", QT_TRANSLATE_NOOP("Online", "Recent") },
    { T_SETTINGS, "", QT_TRANSLATE_NOOP("Online", "Online settings") },
    { T_SETTINGS_SUB, "", QT_TRANSLATE_NOOP("Online", "Nickname, input delay, connection stats") },
    { T_CLOSE, "", QT_TRANSLATE_NOOP("Online", "Close") },
    { T_DATA_USA_EUROPE, "", QT_TRANSLATE_NOOP("Online", "USA/Europe data") },
    { T_DATA_JAPAN, "", QT_TRANSLATE_NOOP("Online", "Japanese data") },
    { T_BUILD_LINE, "ss", QT_TRANSLATE_NOOP("Online", "%s  |  version %s") },

    { T_HINT_SELECT, "", QT_TRANSLATE_NOOP("Online", "Select") },
    { T_HINT_BACK, "", QT_TRANSLATE_NOOP("Online", "Back") },
    { T_HINT_CHANGE, "", QT_TRANSLATE_NOOP("Online", "Change") },
    { T_HINT_MOVE, "", QT_TRANSLATE_NOOP("Online", "Move") },
    { T_HINT_JOIN, "", QT_TRANSLATE_NOOP("Online", "Join") },
    { T_HINT_DELETE, "", QT_TRANSLATE_NOOP("Online", "Delete") },
    { T_HINT_KEYBOARD, "", QT_TRANSLATE_NOOP("Online", "Keyboard") },
    { T_HINT_REFRESH, "", QT_TRANSLATE_NOOP("Online", "Refresh") },
    { T_HINT_LEAVE, "", QT_TRANSLATE_NOOP("Online", "Leave") },

    { T_WHEEL_HELP, "", QT_TRANSLATE_NOOP("Online", "Up and down change a character, left and right move.") },
    { T_JOIN_TYPE_HINT, "", QT_TRANSLATE_NOOP("Online", "Type or paste the address") },
    { T_JOIN, "", QT_TRANSLATE_NOOP("Online", "Join") },
    { T_TYPE, "", QT_TRANSLATE_NOOP("Online", "Type") },
    { T_PASTE, "", QT_TRANSLATE_NOOP("Online", "Paste") },

    { T_SEARCHING, "", QT_TRANSLATE_NOOP("Online", "Looking for rooms on this network...") },
    { T_NONE_FOUND, "", QT_TRANSLATE_NOOP("Online", "No rooms found on this network.") },
    { T_ROOM_OF, "s", QT_TRANSLATE_NOOP("Online", "%s's room") },
    { T_ONE_PLAYER, "", QT_TRANSLATE_NOOP("Online", "1 player") },
    { T_PLAYERS, "d", QT_TRANSLATE_NOOP("Online", "%d players") },
    { T_PLAYING, "", QT_TRANSLATE_NOOP("Online", "Playing") },
    { T_INCOMPATIBLE, "", QT_TRANSLATE_NOOP("Online", "Different version") },

    { T_JOINING, "", QT_TRANSLATE_NOOP("Online", "Joining") },
    { T_STAGE_RESOLVING, "s", QT_TRANSLATE_NOOP("Online", "Finding %s...") },
    { T_STAGE_CONTACTING, "", QT_TRANSLATE_NOOP("Online", "Contacting the room...") },
    { T_STAGE_HANDSHAKE, "", QT_TRANSLATE_NOOP("Online", "Checking versions...") },
    { T_STAGE_ADMITTED, "", QT_TRANSLATE_NOOP("Online", "Joining the room...") },
    { T_SECONDS, "d", QT_TRANSLATE_NOOP("Online", "%d s") },
    { T_CANCEL, "", QT_TRANSLATE_NOOP("Online", "Cancel") },

    { T_ROOM, "", QT_TRANSLATE_NOOP("Online", "Room") },
    { T_WAITING_FOR_PLAYERS, "", QT_TRANSLATE_NOOP("Online", "Waiting for players to join") },
    { T_YOU, "", QT_TRANSLATE_NOOP("Online", "(you)") },
    { T_HOST, "", QT_TRANSLATE_NOOP("Online", "Host") },
    { T_DIRECT, "", QT_TRANSLATE_NOOP("Online", "direct") },
    { T_UNSTABLE, "", QT_TRANSLATE_NOOP("Online", "unstable") },
    { T_LOSS, "d", QT_TRANSLATE_NOOP("Online", "%d%% loss") },
    { T_READY, "", QT_TRANSLATE_NOOP("Online", "Ready") },
    { T_NOT_READY, "", QT_TRANSLATE_NOOP("Online", "Not ready") },
    { T_HOME, "", QT_TRANSLATE_NOOP("Online", "Home") },
    { T_AWAY, "", QT_TRANSLATE_NOOP("Online", "Away") },
    { T_NO_SIDE, "", QT_TRANSLATE_NOOP("Online", "No side") },
    { T_SLOT, "d", QT_TRANSLATE_NOOP("Online", "P%d") },
    { T_INPUT_DELAY, "d", QT_TRANSLATE_NOOP("Online", "Input delay %d ms") },
    { T_INPUT_DELAY_AUTO, "d", QT_TRANSLATE_NOOP("Online", "Input delay %d ms (auto)") },
    { T_START_MATCH, "", QT_TRANSLATE_NOOP("Online", "Start match") },
    { T_SELECT_TEAMS, "", QT_TRANSLATE_NOOP("Online", "Choose teams") },
    { T_SELECTING_STADIUM, "s", QT_TRANSLATE_NOOP("Online", "%s is choosing the stadium") },
    { T_ADD_PLAYER, "", QT_TRANSLATE_NOOP("Online", "Add player") },
    { T_ADD_PLAYER_WAIT, "", QT_TRANSLATE_NOOP("Online", "Confirm on the other controller to join.") },
    { T_ADD_PLAYER_NO_PAD, "", QT_TRANSLATE_NOOP("Online", "Connect another controller, then confirm to join.") },
    { T_LEAVE, "", QT_TRANSLATE_NOOP("Online", "Leave") },
    { T_LEAVE_ROOM_Q, "", QT_TRANSLATE_NOOP("Online", "Leave the room?") },
    { T_CLOSE_ROOM_Q, "", QT_TRANSLATE_NOOP("Online", "Close the room? Everyone will be disconnected.") },
    { T_LEAVE_MATCH_Q, "", QT_TRANSLATE_NOOP("Online", "Leave the match? You can't rejoin it.") },
    { T_STAY, "", QT_TRANSLATE_NOOP("Online", "Stay") },
    { T_ADDRESS_BIG_HINT, "", QT_TRANSLATE_NOOP("Online", "For players on this network. For remote play, share a reachable VPN address or forward the port and share your public address.") },

    { T_MINUTES, "d", QT_TRANSLATE_NOOP("Online", "%d min") },
    { T_ON, "", QT_TRANSLATE_NOOP("Online", "On") },
    { T_OFF, "", QT_TRANSLATE_NOOP("Online", "Off") },
    { T_DONE, "", QT_TRANSLATE_NOOP("Online", "Done") },
    { T_VERSUS, "ss", QT_TRANSLATE_NOOP("Online", "%s vs %s") },
    { T_TEAM_WITH, "ss", QT_TRANSLATE_NOOP("Online", "%s + %s") },
    { T_SUMMARY, "sss", QT_TRANSLATE_NOOP("Online", "%s  |  %s  |  %s") },

    { T_GETTING_READY, "", QT_TRANSLATE_NOOP("Online", "Getting ready") },
    { T_LOAD_WAITING, "", QT_TRANSLATE_NOOP("Online", "Waiting") },
    { T_LOAD_LOADING, "d", QT_TRANSLATE_NOOP("Online", "Loading %d%%") },
    { T_LOAD_PIPELINES, "d", QT_TRANSLATE_NOOP("Online", "Preparing graphics %d%%") },
    { T_LOAD_READY, "", QT_TRANSLATE_NOOP("Online", "Ready") },
    { T_CHECKING_SETUP, "", QT_TRANSLATE_NOOP("Online", "Checking the match setup...") },
    { T_STILL_LOADING, "sd", QT_TRANSLATE_NOOP("Online", "%s is still loading (%d s).") },
    { T_KEEP_WAITING, "", QT_TRANSLATE_NOOP("Online", "Keep waiting") },

    { T_IN_MATCH, "", QT_TRANSLATE_NOOP("Online", "In a match") },
    { T_BACK_TO_GAME, "", QT_TRANSLATE_NOOP("Online", "Back to the game") },
    { T_LEAVE_MATCH, "", QT_TRANSLATE_NOOP("Online", "Leave match") },

    { T_FULL_TIME, "", QT_TRANSLATE_NOOP("Online", "Full time") },
    { T_REMATCH, "", QT_TRANSLATE_NOOP("Online", "Rematch") },
    { T_BACK_TO_ROOM, "", QT_TRANSLATE_NOOP("Online", "Back to room") },
    { T_VOTE_SECONDS, "d", QT_TRANSLATE_NOOP("Online", "%d s to vote") },
    { T_VOTED, "", QT_TRANSLATE_NOOP("Online", "voted") },

    { T_OPPONENTS_LEFT, "", QT_TRANSLATE_NOOP("Online", "Your opponents left") },
    { T_CONTINUE_CPU, "", QT_TRANSLATE_NOOP("Online", "Continue against CPU") },
    { T_END_MATCH, "", QT_TRANSLATE_NOOP("Online", "End match") },

    { T_OK, "", QT_TRANSLATE_NOOP("Online", "OK") },
    { T_ERR_UNKNOWN, "", QT_TRANSLATE_NOOP("Online", "Online play failed.") },
    { T_ERR_UNAVAILABLE, "", QT_TRANSLATE_NOOP("Online", "Online play isn't in this build.") },
    { T_ERR_NETWORK, "", QT_TRANSLATE_NOOP("Online", "No network connection.") },
    { T_ERR_BAD_ADDRESS, "", QT_TRANSLATE_NOOP("Online", "Enter a valid address.") },
    { T_ERR_PORT_IN_USE, "u", QT_TRANSLATE_NOOP("Online", "Port %u is in use by another program.") },
    { T_ERR_CANT_HOST, "", QT_TRANSLATE_NOOP("Online", "This system can join rooms but not host them.") },
    { T_ERR_UNREACHABLE, "", QT_TRANSLATE_NOOP("Online", "Couldn't reach the room. Check the address and ask the host if the room is open.") },
    { T_ERR_ROOM_FULL, "", QT_TRANSLATE_NOOP("Online", "That room is full.") },
    { T_ERR_IN_MATCH, "", QT_TRANSLATE_NOOP("Online", "A match is already in progress.") },
    { T_ERR_VERSION, "", QT_TRANSLATE_NOOP("Online", "That room runs a different version.") },
    { T_ERR_DATA_FAMILY, "", QT_TRANSLATE_NOOP("Online", "That room uses different game data.") },
    { T_ERR_HOST_LEFT, "", QT_TRANSLATE_NOOP("Online", "The host closed the room.") },
    { T_ERR_CONNECTION_LOST, "", QT_TRANSLATE_NOOP("Online", "The connection was lost.") },
    { T_ERR_PROTOCOL, "", QT_TRANSLATE_NOOP("Online", "The room sent data this version can't read.") },
    { T_ERR_SETUP_MISMATCH, "", QT_TRANSLATE_NOOP("Online", "The players' match settings don't match.") },

    { T_PROBLEM_WAITING, "sd", QT_TRANSLATE_NOOP("Online", "Waiting for %s... %d") },
    { T_PROBLEM_SLOW_DEVICE, "s", QT_TRANSLATE_NOOP("Online", "%s's device is running slowly") },
    { T_PROBLEM_HIGH_LATENCY, "", QT_TRANSLATE_NOOP("Online", "High latency") },
    { T_PROBLEM_UNSTABLE, "", QT_TRANSLATE_NOOP("Online", "Unstable connection") },
    { T_PROBLEM_PACKET_LOSS, "", QT_TRANSLATE_NOOP("Online", "Packet loss") },
    { T_STATS, "dddf", QT_TRANSLATE_NOOP("Online", "Ping %d ms   Delay %d ms   Stalls %d/min   %+.1f ticks") },
    { T_STATS_INPUT, "d", QT_TRANSLATE_NOOP("Online", "   Input %d ms") },

    { T_NOTICE_JOINED, "s", QT_TRANSLATE_NOOP("Online", "%s joined") },
    { T_NOTICE_LEFT, "s", QT_TRANSLATE_NOOP("Online", "%s left") },
    { T_NOTICE_DROPPED, "s", QT_TRANSLATE_NOOP("Online", "%s left. CPU took over.") },
    { T_NOTICE_SETUP_MISMATCH, "", QT_TRANSLATE_NOOP("Online", "Match settings differ. Returning to the room.") },
    { T_NOTICE_REMATCH, "", QT_TRANSLATE_NOOP("Online", "Rematch!") },

    { T_DESYNC_ENDED, "", QT_TRANSLATE_NOOP("Online", "The match ended because the games went out of sync.") },
    { T_DESYNC_REPORT, "s", QT_TRANSLATE_NOOP("Online", "Report saved to %s") },

    { T_NICKNAME, "", QT_TRANSLATE_NOOP("Online", "Nickname") },
    { T_ROOM_PORT, "", QT_TRANSLATE_NOOP("Online", "Room port") },
    { T_SHOW_STATS, "", QT_TRANSLATE_NOOP("Online", "Connection stats in matches") },
    { T_DELAY_LABEL, "", QT_TRANSLATE_NOOP("Online", "Input delay") },
    { T_AUTO, "", QT_TRANSLATE_NOOP("Online", "Automatic") },
    { T_MILLISECONDS, "d", QT_TRANSLATE_NOOP("Online", "%d ms") },
    { T_PORT_RANGE, "", QT_TRANSLATE_NOOP("Online", "Use a port from 1025 to 49151.") },
    { T_PAUSED_BY, "s", QT_TRANSLATE_NOOP("Online", "%s paused the match") },
    { T_PAUSE_ALLOWED, "", QT_TRANSLATE_NOOP("Online", "Allow pausing (3 per side)") },
    { T_MATCH_SETTINGS, "", QT_TRANSLATE_NOOP("Online", "Match settings") },
    { T_MATCH_LENGTH, "", QT_TRANSLATE_NOOP("Online", "Match length") },
    { T_ITEMS, "", QT_TRANSLATE_NOOP("Online", "Items") },
    { T_ITEM_RED_SHELLS, "", QT_TRANSLATE_NOOP("Online", "Red shells") },
    { T_ITEM_GREEN_SHELLS, "", QT_TRANSLATE_NOOP("Online", "Green shells") },
    { T_ITEM_FREEZE_SHELLS, "", QT_TRANSLATE_NOOP("Online", "Freeze shells") },
    { T_ITEM_SPINY_SHELLS, "", QT_TRANSLATE_NOOP("Online", "Spiny shells") },
    { T_ITEM_STARS, "", QT_TRANSLATE_NOOP("Online", "Stars") },
    { T_ITEM_BOBOMBS, "", QT_TRANSLATE_NOOP("Online", "Bob-ombs") },
    { T_ITEM_BANANAS, "", QT_TRANSLATE_NOOP("Online", "Bananas") },
    { T_OTHER_ADDRESSES, "", QT_TRANSLATE_NOOP("Online", "Share an address the other players can reach:") },
    { T_HINT_SAVE, "", QT_TRANSLATE_NOOP("Online", "Save") },
    { T_CHANGE_TEAMS, "", QT_TRANSLATE_NOOP("Online", "Change teams") },
    { T_NOT_VOTED, "", QT_TRANSLATE_NOOP("Online", "Choosing...") },
    { T_FOR_INTERNET, "", QT_TRANSLATE_NOOP("Online", "Players on the internet") },
    { T_FOR_THIS_NETWORK, "", QT_TRANSLATE_NOOP("Online", "Players on this network") },
    { T_PUBLIC_LOOKING, "", QT_TRANSLATE_NOOP("Online", "Finding your internet address...") },
    { T_PUBLIC_OPEN, "", QT_TRANSLATE_NOOP("Online", "Share this address with players anywhere.") },
    { T_PUBLIC_FOUND, "", QT_TRANSLATE_NOOP("Online", "Players elsewhere may not be able to join. If they can't, use Tailscale or ZeroTier.") },
    { T_PUBLIC_SHARED, "", QT_TRANSLATE_NOOP("Online", "Your connection doesn't let players elsewhere join. Use Tailscale or ZeroTier instead.") },
    { T_PUBLIC_FAILED, "", QT_TRANSLATE_NOOP("Online", "Couldn't find your internet address. Players elsewhere can join over Tailscale or ZeroTier.") },
    { T_SHARE_PRIVATELY, "", QT_TRANSLATE_NOOP("Online", "Share it only with people you're playing with.") },
    { T_ERR_HOME_ADDRESS, "", QT_TRANSLATE_NOOP("Online", "That address only works on the host's own network. Ask them for their internet address.") },

    // The US disc's own names, in the order of the game's enums.
    { (Text)(T_CAPTAIN_FIRST + 0),  "", "Daisy" },
    { (Text)(T_CAPTAIN_FIRST + 1),  "", "Donkey Kong" },
    { (Text)(T_CAPTAIN_FIRST + 2),  "", "Luigi" },
    { (Text)(T_CAPTAIN_FIRST + 3),  "", "Mario" },
    { (Text)(T_CAPTAIN_FIRST + 4),  "", "Peach" },
    { (Text)(T_CAPTAIN_FIRST + 5),  "", "Waluigi" },
    { (Text)(T_CAPTAIN_FIRST + 6),  "", "Wario" },
    { (Text)(T_CAPTAIN_FIRST + 7),  "", "Yoshi" },
    { (Text)(T_CAPTAIN_FIRST + 8),  "", "Super Team" },
    { (Text)(T_SIDEKICK_FIRST + 0), "", "Toad" },
    { (Text)(T_SIDEKICK_FIRST + 1), "", "Koopa" },
    { (Text)(T_SIDEKICK_FIRST + 2), "", "Hammer Bros." },
    { (Text)(T_SIDEKICK_FIRST + 3), "", "Birdo" },
    { (Text)(T_STADIUM_FIRST + 0),  "", "Pipeline Central" },
    { (Text)(T_STADIUM_FIRST + 1),  "", "The Palace" },
    { (Text)(T_STADIUM_FIRST + 2),  "", "Konga Coliseum" },
    { (Text)(T_STADIUM_FIRST + 3),  "", "The Underground" },
    { (Text)(T_STADIUM_FIRST + 4),  "", "Crater Field" },
    { (Text)(T_STADIUM_FIRST + 5),  "", "Bowser Stadium" },
    { (Text)(T_STADIUM_FIRST + 6),  "", "The Battle Dome" },
    { (Text)(T_SKILL_FIRST + 0),    "", "Training" },
    { (Text)(T_SKILL_FIRST + 1),    "", "Rookie" },
    { (Text)(T_SKILL_FIRST + 2),    "", "Professional" },
    { (Text)(T_SKILL_FIRST + 3),    "", "Superstar" },
    { (Text)(T_SKILL_FIRST + 4),    "", "Legend" },
};
// clang-format on

struct Translation
{
    Text id;
    const char* values[3];
};
#include "panel_translations.inc"

int s_language = -1;
char s_native[T_COUNT - T_CAPTAIN_FIRST][256];

unsigned decode(const char*& p)
{
    const unsigned lead = (unsigned char)*p++;
    if (lead < 128)
        return lead;
    int bytes = lead >= 0xF0 && lead <= 0xF4   ? 3
                : lead >= 0xE0 && lead <= 0xEF ? 2
                : lead >= 0xC2 && lead <= 0xDF ? 1
                                               : 0;
    if (!bytes)
        return 0xFFFD;
    unsigned cp = lead & ((1u << (6 - bytes)) - 1);
    for (int i = 0; i < bytes; ++i)
    {
        if (((unsigned char)*p & 0xC0) != 0x80)
            return 0xFFFD;
        cp = (cp << 6) | ((unsigned char)*p++ & 63);
    }
    const unsigned minimum[] = { 0, 128, 2048, 65536 };
    return cp < minimum[bytes] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) ? 0xFFFD : cp;
}

bool encode(unsigned cp, char*& out, char* end)
{
    unsigned n = cp < 128 ? 1 : cp < 2048 ? 2 : cp < 65536 ? 3 : 4;
    if ((unsigned)(end - out) <= n)
        return false;
    if (n == 1)
        *out++ = (char)cp;
    else
    {
        *out++ = (char)((n == 2 ? 0xC0 : n == 3 ? 0xE0 : 0xF0) | (cp >> (6 * (n - 1))));
        for (unsigned i = n - 1; i > 0; --i)
            *out++ = (char)(0x80 | ((cp >> (6 * (i - 1))) & 63));
    }
    return true;
}

struct Table
{
    const Entry* rows[T_COUNT];
    const Translation* translations[T_COUNT] = {};
    Table()
    {
        for (const Entry*& r : rows)
            r = nullptr;
        for (const Entry& e : kEnglish)
            if (e.id >= 0 && e.id < T_COUNT)
                rows[e.id] = &e;
        for (const Translation& t : kTranslations)
            translations[t.id] = &t;
    }
};

const Table& table()
{
    static const Table value;
    return value;
}

const Entry* row(Text id) { return id >= 0 && id < T_COUNT ? table().rows[id] : nullptr; }

}   // namespace

const char* text(Text id)
{
    const Entry* e = row(id);
    if (!e)
        return "";
    if (id >= T_CAPTAIN_FIRST && s_native[id - T_CAPTAIN_FIRST][0])
        return s_native[id - T_CAPTAIN_FIRST];
    const Translation* translated = table().translations[id];
    return s_language >= 0 && translated ? translated->values[s_language] : e->english;
}

void text_language(const char* tag)
{
    const int language = tag && std::strncmp(tag, "de", 2) == 0   ? 0
                         : tag && std::strncmp(tag, "fr", 2) == 0 ? 1
                         : tag && std::strncmp(tag, "es", 2) == 0 ? 2
                                                                  : -1;
    if (language != s_language)
        std::memset(s_native, 0, sizeof s_native);
    s_language = language;
}

void text_native(Text id, const unsigned short* value)
{
    if (id < T_CAPTAIN_FIRST || id >= T_COUNT)
        return;
    char* out = s_native[id - T_CAPTAIN_FIRST];
    if (!value)
    {
        *out = 0;
        return;
    }
    char* end = out + sizeof s_native[0];
    while (*value)
    {
        unsigned cp = *value++;
        if (cp >= 0xD800 && cp <= 0xDBFF && *value >= 0xDC00 && *value <= 0xDFFF)
            cp = 0x10000 + ((cp - 0xD800) << 10) + (*value++ - 0xDC00);
        else if (cp >= 0xD800 && cp <= 0xDFFF)
            cp = 0xFFFD;
        if (!encode(cp, out, end))
            break;
    }
    *out = 0;
}

void text_utf16(Text id, unsigned short* out, unsigned capacity, bool uppercase)
{
    if (!capacity)
        return;
    char title[1024];
    if (uppercase)
        text_upper(text(id), title, sizeof title);
    const char* p = uppercase ? title : text(id);
    unsigned used = 0;
    while (*p)
    {
        unsigned cp = decode(p);
        if (used + (cp > 65535 ? 2 : 1) >= capacity)
            break;
        if (cp > 65535)
        {
            cp -= 65536;
            out[used++] = 0xD800 + (cp >> 10);
            out[used++] = 0xDC00 + (cp & 1023);
        }
        else
            out[used++] = (unsigned short)cp;
    }
    out[used] = 0;
}

void text_upper(const char* value, char* out, unsigned capacity)
{
    if (!capacity)
        return;
    char* end = out + capacity;
    while (*value)
    {
        unsigned cp = decode(value);
        if ((cp >= 'a' && cp <= 'z') || (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7))
            cp -= 32;
        else if (cp == 0x153)
            cp = 0x152;
        else if (cp == 0xFF)
            cp = 0x178;
        else if (cp == 0xDF)
        {
            if (!encode('S', out, end))
                break;
            cp = 'S';
        }
        if (!encode(cp, out, end))
            break;
    }
    *out = 0;
}

const char* text_args(Text id)
{
    const Entry* e = row(id);
    return e != nullptr ? e->args : "";
}

}   // namespace online_panel
