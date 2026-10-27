// The online panel's words; a translation keeps each string's printf arguments in the same order.

#ifndef PORT_ONLINE_PANEL_TEXT_H
#define PORT_ONLINE_PANEL_TEXT_H

namespace online_panel
{

enum Text
{
    T_CHANGE_TEAM,
    T_CPU,
    T_CPU_HOST,
    T_ENTER_KEY,
    T_ESCAPE_KEY,
    T_CHOOSE_TEAMS_WAIT,
    T_CONNECTION_DETAILS,
    T_INVITE,
    T_CONNECTED,
    T_CHOOSING_CAPTAIN,
    T_CHOOSING_SIDEKICK,
    T_STADIUM_LOCKED,
    T_STAGE_STADIUM,
    T_SHARED_BACK,
    T_ADVANCED,
    T_OTHER_NETWORKS,
    T_COPY_ADDRESS,
    T_COPIED,
    T_JOIN_HELP,
    T_RULES,
    T_LOCAL_RUMBLE,
    T_ONLINE,
    T_CREATE_ROOM,
    T_CREATE_ROOM_SUB,
    T_NEARBY,
    T_NEARBY_SUB,
    T_JOIN_ADDRESS,
    T_JOIN_ADDRESS_SUB,
    T_RECENT,
    T_SETTINGS,
    T_SETTINGS_SUB,
    T_CLOSE,
    T_DATA_USA_EUROPE,
    T_DATA_JAPAN,
    T_BUILD_LINE,

    T_HINT_SELECT,
    T_HINT_BACK,
    T_HINT_CHANGE,
    T_HINT_MOVE,
    T_HINT_JOIN,
    T_HINT_DELETE,
    T_HINT_KEYBOARD,
    T_HINT_REFRESH,
    T_HINT_LEAVE,

    T_WHEEL_HELP,
    T_JOIN_TYPE_HINT,
    T_JOIN,
    T_TYPE,
    T_PASTE,

    T_SEARCHING,
    T_NONE_FOUND,
    T_ROOM_OF,
    T_ONE_PLAYER,
    T_PLAYERS,
    T_PLAYING,
    T_INCOMPATIBLE,

    T_JOINING,
    T_STAGE_RESOLVING,
    T_STAGE_CONTACTING,
    T_STAGE_HANDSHAKE,
    T_STAGE_ADMITTED,
    T_SECONDS,
    T_CANCEL,

    T_ROOM,
    T_WAITING_FOR_PLAYERS,
    T_YOU,
    T_HOST,
    T_DIRECT,
    T_UNSTABLE,
    T_LOSS,
    T_READY,
    T_NOT_READY,
    T_HOME,
    T_AWAY,
    T_NO_SIDE,
    T_SLOT,
    T_INPUT_DELAY,
    T_INPUT_DELAY_AUTO,
    T_START_MATCH,
    T_SELECT_TEAMS,
    T_SELECTING_STADIUM,
    T_ADD_PLAYER,
    T_ADD_PLAYER_WAIT,
    T_ADD_PLAYER_NO_PAD,
    T_LEAVE,
    T_LEAVE_ROOM_Q,
    T_CLOSE_ROOM_Q,
    T_LEAVE_MATCH_Q,
    T_STAY,
    T_ADDRESS_BIG_HINT,

    T_MINUTES,
    T_ON,
    T_OFF,
    T_DONE,
    T_VERSUS,
    T_TEAM_WITH,
    T_SUMMARY,

    T_GETTING_READY,
    T_LOAD_WAITING,
    T_LOAD_LOADING,
    T_LOAD_PIPELINES,
    T_LOAD_READY,
    T_CHECKING_SETUP,
    T_STILL_LOADING,
    T_KEEP_WAITING,

    T_IN_MATCH,
    T_BACK_TO_GAME,
    T_LEAVE_MATCH,

    T_FULL_TIME,
    T_REMATCH,
    T_BACK_TO_ROOM,
    T_VOTE_SECONDS,
    T_VOTED,

    T_OPPONENTS_LEFT,
    T_CONTINUE_CPU,
    T_END_MATCH,

    T_OK,
    T_ERR_UNKNOWN,
    T_ERR_UNAVAILABLE,
    T_ERR_NETWORK,
    T_ERR_BAD_ADDRESS,
    T_ERR_PORT_IN_USE,
    T_ERR_CANT_HOST,
    T_ERR_UNREACHABLE,
    T_ERR_ROOM_FULL,
    T_ERR_IN_MATCH,
    T_ERR_VERSION,
    T_ERR_DATA_FAMILY,
    T_ERR_HOST_LEFT,
    T_ERR_CONNECTION_LOST,
    T_ERR_PROTOCOL,
    T_ERR_SETUP_MISMATCH,

    T_PROBLEM_WAITING,
    T_PROBLEM_SLOW_DEVICE,
    T_PROBLEM_HIGH_LATENCY,
    T_PROBLEM_UNSTABLE,
    T_PROBLEM_PACKET_LOSS,
    T_STATS,
    T_STATS_INPUT,

    T_NOTICE_JOINED,
    T_NOTICE_LEFT,
    T_NOTICE_DROPPED,
    T_NOTICE_SETUP_MISMATCH,
    T_NOTICE_REMATCH,

    T_DESYNC_ENDED,
    T_DESYNC_REPORT,

    T_NICKNAME,
    T_ROOM_PORT,
    T_SHOW_STATS,
    T_DELAY_LABEL,
    T_AUTO,
    T_MILLISECONDS,
    T_PORT_RANGE,
    T_HINT_SAVE,
    T_CHANGE_TEAMS,
    T_NOT_VOTED,
    T_PAUSED_BY,
    T_PAUSE_ALLOWED,
    T_MATCH_SETTINGS,
    T_MATCH_LENGTH,
    T_ITEMS,
    T_ITEM_RED_SHELLS,
    T_ITEM_GREEN_SHELLS,
    T_ITEM_FREEZE_SHELLS,
    T_ITEM_SPINY_SHELLS,
    T_ITEM_STARS,
    T_ITEM_BOBOMBS,
    T_ITEM_BANANAS,
    T_OTHER_ADDRESSES,

    T_CAPTAIN_FIRST,
    T_SIDEKICK_FIRST = T_CAPTAIN_FIRST + 9,
    T_STADIUM_FIRST = T_SIDEKICK_FIRST + 4,
    T_SKILL_FIRST = T_STADIUM_FIRST + 7,

    T_COUNT = T_SKILL_FIRST + 5
};

// A language with no catalogue falls back to English.
void text_language(const char* tag);
void text_native(Text id, const unsigned short* value);
void text_utf16(Text id, unsigned short* out, unsigned capacity, bool uppercase = false);
void text_upper(const char* value, char* out, unsigned capacity);

// Never NULL.
const char* text(Text id);

// The printf conversions `id` takes, in order, one letter each: s, d, u or f.
const char* text_args(Text id);

}   // namespace online_panel

#endif   // PORT_ONLINE_PANEL_TEXT_H
