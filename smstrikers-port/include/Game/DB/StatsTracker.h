#ifndef _STATSTRACKER_H_
#define _STATSTRACKER_H_

#include "Game/Team.h"
#include "Game/DB/Simmer.h"
#include "NL/nlSingleton.h"

enum eType
{
    TYPE_INVALID = -1,
    TYPE_CHARACTER = 0,
    TYPE_TEAM = 1,
    TYPE_USER = 2,
};

enum ePlayerStats
{
    STATS_INVALID = -1,
    STATS_SHOTS_ON_GOAL = 0,
    STATS_GOALS_FOR = 1,
    STATS_GOALS_AGAINST = 2,
    STATS_GOALS_FOR_STS = 3,
    STATS_ASSISTS = 4,
    STATS_FOULS = 5,
    STATS_WIN = 6,
    STATS_OT_WIN = 7,
    STATS_LOSS = 8,
    STATS_OT_LOSS = 9,
    STATS_POWERUPS_USED = 10,
    STATS_POWERUPS_HIT = 11,
    STATS_PASSES_MADE = 12,
    STATS_PASSES_RECEIVED = 13,
    STATS_PASSES_INTERCEPTED = 14,
    STATS_POSSESION_TIME = 15,
    STATS_GAMES_PLAYED = 16,
    STATS_STEALS = 17,
    STATS_BUTTON_PRESSES = 18,
    STATS_HITS_MADE = 19,
    STATS_GOALS_FOR_ONE_TIMERS = 20,
    STATS_STS_ATTEMPTS = 21,
    STATS_PERFECT_PASSES = 22,
    NUM_STATS = 23,
};

enum eUserGameResult
{
    RESULT_INVALID = -1,
    RESULT_USER_WINS = 0,
    RESULT_USER_LOSES = 1,
    RESULT_USER_OT_WINS = 2,
    RESULT_USER_OT_LOSES = 3,
    RESULT_CUP_WIN = 4,
    RESULT_USER_QUALIFIES_FLOWER = 5,
    RESULT_USER_QUALIFIES_STAR = 6,
    RESULT_USER_QUALIFIES_BOWSER = 7,
    RESULT_USER_DOES_NOT_QUALIFY_FLOWER = 8,
    RESULT_USER_DOES_NOT_QUALIFY_STAR = 9,
    RESULT_USER_PLAYOFF_QUALIFIES = 10,
    RESULT_USER_DOES_NOT_PLAYOFF_QUALIFY = 11,
    RESULT_USER_QUALIFIES_SUPER = 12,
    RESULT_HOME_FORFEITS = 13,
    RESULT_AWAY_FORFEITS = 14,
    RESULT_CUP_START = 15,
    RESULT_USER_ELIMINATED_QUARTER = 16,
    RESULT_USER_ELIMINATED_SEMI = 17,
    NUM_RESULTS = 18,
};

enum eSortOrder
{
    SORT_ASCENDING = 0,
    SORT_DESCENDING = 1,
};

union RECORDTYPE
{
    /* 0x0 */ eCharacterClass mCharacterClass;
    /* 0x0 */ eTeamID mTeamID;
    /* 0x0 */ int mControllerID;
}; // total size: 0x4

struct PlayerStats
{
    /* 0x00 */ unsigned short mNumShotsOnGoal;
    /* 0x02 */ unsigned short mNumGoalsFor;
    /* 0x04 */ unsigned short mNumGoalsAgainst;
    /* 0x06 */ unsigned short mNumAssists;
    /* 0x08 */ unsigned short mNumFouls;
    /* 0x0A */ unsigned short mNumGamesPlayed;
    /* 0x0C */ unsigned short mNumPowerupsUsed;
    /* 0x0E */ unsigned short mNumPowerupsHit;
    /* 0x10 */ unsigned short mNumShootToScoreGoals;
    /* 0x12 */ unsigned short mNumPassesMade;
    /* 0x14 */ unsigned short mNumPassesReceived;
    /* 0x16 */ unsigned short mNumPassesIntercepted;
    /* 0x18 */ unsigned short mNumHitsMade;
    /* 0x1A */ unsigned short mNumSteals;
    /* 0x1C */ unsigned short mNumGoalsOneTimers;
    /* 0x1E */ unsigned short mNumSTSAttempts;
    /* 0x20 */ unsigned short mNumPerfectPasses;
    /* 0x24 */ unsigned long mBallPossessionTime;
    /* 0x28 */ unsigned long mNumButtonPresses;
    /* 0x2C */ RECORDTYPE mRecordType;
    /* 0x30 */ eType mType;
}; // total size: 0x34

struct TeamStats
{
    TeamStats();
    /* 0x0 */ eTeamID mTeamIndex;
    /* 0x4 */ unsigned short mNumWins;
    /* 0x6 */ unsigned short mNumLosses;
    /* 0x8 */ unsigned short mNumOTLosses;
    /* 0xA */ unsigned short mNumPoints;
    /* 0xC */ PlayerStats mPlayerTotalStats;
}; // total size: 0x40

struct BasicGameInfo;

class StatsTracker : public nlSingleton<StatsTracker>
{
public:
    StatsTracker();
    void SetBasicGameInfoPointer(BasicGameInfo* pGameInfo, bool initializeStats);
    void ResetCurrentStats();
    void CreateEventHandler();
    void DestroyEventHandler();
    static void eventHandler(Event* event, void* userData);
    void TrackStat(ePlayerStats stat, int homeaway, int playerindex, int param0, int param1, int param2, int param3);
    static void Track(ePlayerStats, int, int, int, int, int, int);
    void GetSortedStats(PlayerStats* source, int numsource, int* dest, int numelements, ePlayerStats statType, eSortOrder sortOrder);
    void GetSortedTeamStats(TeamStats* source, int numsource, int* dest, int numelements);
    void CompileEndOfGameStats();
    void SimulateRemainingGames();
    void SimulateGame();
    void AddStat(ePlayerStats stat, int team, int player, int value);
    void AddUserStatByPad(ePlayerStats stat, int pad, int amount);
    void AddMilestoneUserStat(ePlayerStats stat, int amount);
    void TrackWinner(int forfeitSide);
    void WriteStats(float gameTime, float gameDuration, const char* filename);
    void AwardCup(eUserGameResult gameResult);
    void WriteCurrentlyPlaying() const;
    bool MoveTeamBUp(TeamStats b, TeamStats a);
    bool IsOvertime() { return mIsOvertime; }

    /* 0x000 */ BasicGameInfo* mBasicGameInfo;
    /* 0x004 */ EventHandler* mEventHandler;
    /* 0x008 */ TeamStats mCurrentTeamStats[2];
    /* 0x088 */ TeamStats mCumulativeTeamStats[2];
    /* 0x108 */ PlayerStats mCurrentPlayerStats[2][5];
    /* 0x310 */ PlayerStats mCurrentUserStats[4];
    /* 0x3E0 */ PlayerStats mCumulativeUserStats[4];
    /* 0x4B0 */ unsigned short mNumConsecutiveGamesPlayed;
    /* 0x4B4 */ int mNumGamesWon[2];
    /* 0x4BC */ Simulator* m_pSimulator;
    /* 0x4C0 */ bool mIsUserCupWinner;
    /* 0x4C1 */ bool mIsOvertime;
    /* 0x4C2 */ bool mHasGameEnded;
}; // total size: 0x4C4

inline TeamStats::TeamStats()
{
    memset(&mPlayerTotalStats, 0, sizeof(mPlayerTotalStats));
    mPlayerTotalStats.mRecordType.mTeamID = TEAM_MARIO;
    mPlayerTotalStats.mType = TYPE_TEAM;
    mTeamIndex = TEAM_MARIO;
    mNumWins = 0;
    mNumLosses = 0;
    mNumOTLosses = 0;
    mNumPoints = 0;
}

inline void StatsTracker::Track(ePlayerStats stat, int homeaway, int playerindex, int param0, int param1, int param2, int param3)
{
    s_pInstance->TrackStat(stat, homeaway, playerindex, param0, param1, param2, param3);
}
// class BasicString < char, Detail
// {
// public:
//     void TempStringAllocator > ::AppendInPlace<Detail::TempStringAllocator>(const BasicString<char, Detail::TempStringAllocator>&);
//     void TempStringAllocator > ::Append<Detail::TempStringAllocator>(const BasicString<char, Detail::TempStringAllocator>&) const;
// };

// class Format < BasicString < char, Detail
// {
// public:
//     void TempStringAllocator >, const char *, const char *, const char *, const char *, const char * > (const BasicString<char, Detail::TempStringAllocator>&, const char* const&, const char* const&, const char* const&, const char* const&, const char* const&);
//     void TempStringAllocator >, int, int, int, int, int, int, int > (const BasicString<char, Detail::TempStringAllocator>&, const int&, const int&, const int&, const int&, const int&, const int&, const int&);
//     void TempStringAllocator >, const char *, const char *, const char *, const char *, const char *, float, float > (const BasicString<char, Detail::TempStringAllocator>&, const char* const&, const char* const&, const char* const&, const char* const&, const char* const&, const float&, const float&);
//     void TempStringAllocator >, int, int, int, int, int, int, int, int > (const BasicString<char, Detail::TempStringAllocator>&, const int&, const int&, const int&, const int&, const int&, const int&, const int&, const int&);
// };

// class FormatImpl < BasicString < char, Detail
// {
// public:
//     void TempStringAllocator >> ::operator% <const char*>(const char* const&);
//     void TempStringAllocator >> ::operator% <float>(const float&);
//     void TempStringAllocator >> ::operator% <int>(const int&);
// };

#endif // _STATSTRACKER_H_
