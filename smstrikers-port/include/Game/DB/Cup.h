#ifndef _CUP_H_
#define _CUP_H_

#include "types.h"
#include "Game/Team.h"
#include "Game/DB/BasicGameInfo.h"
#include "Game/DB/UserOptions.h"
#include "Game/DB/StatsTracker.h"

struct CupRecord
{
    CupRecord()
    {
        mPlace = 0;
        mTeam = TEAM_INVALID;
        mDifficulty = (GameplaySettings::eSkillLevel)1;
        memset(&mDate, 0, sizeof(mDate));
    }

    /* 0x00 */ OSCalendarTime mDate;
    /* 0x28 */ signed char mPlace;
    /* 0x2C */ eTeamID mTeam;
    /* 0x30 */ GameplaySettings::eSkillLevel mDifficulty;
}; // total size: 0x34

struct Spoil
{
    Spoil()
        : mNumRecords(0)
        , mNumWins(0)
        , mNumLosses(0)
        , mNumCupWins(0)
        , mCurrentChamp(TEAM_INVALID)
        , mIsCPUChamp(false)
    {
    }

    /* 0x000 */ CupRecord mCupHistory[10];
    /* 0x208 */ unsigned char mNumRecords;
    /* 0x20A */ unsigned short mNumWins;
    /* 0x20C */ unsigned short mNumLosses;
    /* 0x20E */ unsigned short mNumCupWins;
    /* 0x210 */ eTeamID mCurrentChamp;
    /* 0x214 */ bool mIsCPUChamp;
}; // total size: 0x218

struct BaseCup
{
    /* 0x0, */ eTeamID mUserSelectedTeam;
    /* 0x4, */ eSidekickID mUserSelectedSidekick;
    /* 0x8, */ s16 mRoundNumber;
    /* 0xA, */ s16 mGameNumber;
    /* 0xC, */ u16 mHumanTeams;
    /* 0xE, */ bool mCupStarted;
    /* 0x10 */ GameplaySettings mCupSettings;

    BaseCup()
        : mUserSelectedTeam(TEAM_INVALID)
        , mUserSelectedSidekick(SK_INVALID)
        , mRoundNumber(0)
        , mGameNumber(0)
        , mHumanTeams(0)
        , mCupStarted(false)
        , mCupSettings()
    {
    }

    virtual BasicGameInfo* GetGameInfo(int team, int round) = 0;
    virtual TeamStats* GetTeamStats(int team) = 0;
    virtual u16 GetNumTeams() = 0;
    virtual u16 GetNumRounds() = 0;
    virtual signed char* GetRoundResults(int round) = 0;
    virtual void* SerializeData(void* dst) const
    {
        memcpy(dst, &mUserSelectedTeam, 4);
        dst = (u8*)dst + 4;
        memcpy(dst, &mUserSelectedSidekick, 4);
        dst = (u8*)dst + 4;
        memcpy(dst, &mRoundNumber, 2);
        dst = (u8*)dst + 2;
        memcpy(dst, &mGameNumber, 2);
        dst = (u8*)dst + 2;
        memcpy(dst, &mHumanTeams, 2);
        dst = (u8*)dst + 2;
        memcpy(dst, &mCupStarted, 1);
        dst = (u8*)dst + 1;
        memcpy(dst, &mCupSettings, sizeof(mCupSettings));
        return (u8*)dst + sizeof(mCupSettings);
    };
    virtual void* DeserializeData(void* src)
    {
        memcpy(&mUserSelectedTeam, src, 4);
        src = (u8*)src + 4;
        memcpy(&mUserSelectedSidekick, src, 4);
        src = (u8*)src + 4;
        memcpy(&mRoundNumber, src, 2);
        src = (u8*)src + 2;
        memcpy(&mGameNumber, src, 2);
        src = (u8*)src + 2;
        memcpy(&mHumanTeams, src, 2);
        src = (u8*)src + 2;
        memcpy(&mCupStarted, src, 1);
        src = (u8*)src + 1;
        memcpy(&mCupSettings, src, sizeof(mCupSettings));
        src = (u8*)src + sizeof(mCupSettings);
        return src;
    };
    virtual int GetSaveDataSize() const { return 0x1B; };
}; // total size: 0x20

template <u16 Teams, u16 Rounds>
struct Cup : public BaseCup
{
    virtual BasicGameInfo* GetGameInfo(int team, int round) { return &mGameInfo[team][round]; };
    virtual TeamStats* GetTeamStats(int team) { return &mTeamStats[team]; };
    virtual u16 GetNumTeams() { return Teams; };
    virtual u16 GetNumRounds() { return Rounds; };
    virtual signed char* GetRoundResults(int round) { return &mRoundResults[round]; };
    virtual int GetSaveDataSize() const { return 0x1B + sizeof(mGameInfo) + sizeof(mTeamStats) + sizeof(mRoundResults); };
    virtual void* SerializeData(void* dst) const
    {
        memcpy(dst, &mUserSelectedTeam, 4);
        dst = (u8*)dst + 4;
        memcpy(dst, &mUserSelectedSidekick, 4);
        dst = (u8*)dst + 4;
        memcpy(dst, &mRoundNumber, 2);
        dst = (u8*)dst + 2;
        memcpy(dst, &mGameNumber, 2);
        dst = (u8*)dst + 2;
        memcpy(dst, &mHumanTeams, 2);
        dst = (u8*)dst + 2;
        memcpy(dst, &mCupStarted, 1);
        dst = (u8*)dst + 1;
        memcpy(dst, &mCupSettings, sizeof(mCupSettings));
        dst = (u8*)dst + sizeof(mCupSettings);
        memcpy(dst, mGameInfo, sizeof(mGameInfo));
        dst = (u8*)dst + sizeof(mGameInfo);
        memcpy(dst, mTeamStats, sizeof(mTeamStats));
        dst = (u8*)dst + sizeof(mTeamStats);
        memcpy(dst, mRoundResults, sizeof(mRoundResults));
        dst = (u8*)dst + sizeof(mRoundResults);
        return dst;
    }
    virtual void* DeserializeData(void* src)
    {
        memcpy(&mUserSelectedTeam, src, 4);
        src = (u8*)src + 4;
        memcpy(&mUserSelectedSidekick, src, 4);
        src = (u8*)src + 4;
        memcpy(&mRoundNumber, src, 2);
        src = (u8*)src + 2;
        memcpy(&mGameNumber, src, 2);
        src = (u8*)src + 2;
        memcpy(&mHumanTeams, src, 2);
        src = (u8*)src + 2;
        memcpy(&mCupStarted, src, 1);
        src = (u8*)src + 1;
        memcpy(&mCupSettings, src, sizeof(mCupSettings));
        src = (u8*)src + sizeof(mCupSettings);
        memcpy(mGameInfo, src, sizeof(mGameInfo));
        src = (u8*)src + sizeof(mGameInfo);
        memcpy(mTeamStats, src, sizeof(mTeamStats));
        src = (u8*)src + sizeof(mTeamStats);
        memcpy(mRoundResults, src, sizeof(mRoundResults));
        src = (u8*)src + sizeof(mRoundResults);
        return src;
    }

    BasicGameInfo mGameInfo[Rounds][Teams / 2];
    TeamStats mTeamStats[Teams];
    signed char mRoundResults[Rounds];
};

template <u16 Teams>
struct Knockout : public BaseCup
{
    virtual BasicGameInfo* GetGameInfo(int team, int round);
    virtual TeamStats* GetTeamStats(int team) { return &mTeamStats[team]; };
    virtual u16 GetNumTeams() { return Teams; };
    virtual u16 GetNumRounds() { return Teams == 8 ? 3 : 2; };
    virtual signed char* GetRoundResults(int round) { return &mRoundResults[round]; };
    virtual int GetSaveDataSize() const { return 0x1B + sizeof(mGameInfo) + sizeof(mTeamStats) + sizeof(mRoundResults); };
    virtual void* SerializeData(void* dst) const
    {
        memcpy(dst, &mUserSelectedTeam, 4);
        dst = (u8*)dst + 4;
        memcpy(dst, &mUserSelectedSidekick, 4);
        dst = (u8*)dst + 4;
        memcpy(dst, &mRoundNumber, 2);
        dst = (u8*)dst + 2;
        memcpy(dst, &mGameNumber, 2);
        dst = (u8*)dst + 2;
        memcpy(dst, &mHumanTeams, 2);
        dst = (u8*)dst + 2;
        memcpy(dst, &mCupStarted, 1);
        dst = (u8*)dst + 1;
        memcpy(dst, &mCupSettings, sizeof(mCupSettings));
        dst = (u8*)dst + sizeof(mCupSettings);
        memcpy(dst, mGameInfo, sizeof(mGameInfo));
        dst = (u8*)dst + sizeof(mGameInfo);
        memcpy(dst, mTeamStats, sizeof(mTeamStats));
        dst = (u8*)dst + sizeof(mTeamStats);
        memcpy(dst, mRoundResults, sizeof(mRoundResults));
        dst = (u8*)dst + sizeof(mRoundResults);
        return dst;
    }
    virtual void* DeserializeData(void* src)
    {
        memcpy(&mUserSelectedTeam, src, 4);
        src = (u8*)src + 4;
        memcpy(&mUserSelectedSidekick, src, 4);
        src = (u8*)src + 4;
        memcpy(&mRoundNumber, src, 2);
        src = (u8*)src + 2;
        memcpy(&mGameNumber, src, 2);
        src = (u8*)src + 2;
        memcpy(&mHumanTeams, src, 2);
        src = (u8*)src + 2;
        memcpy(&mCupStarted, src, 1);
        src = (u8*)src + 1;
        memcpy(&mCupSettings, src, sizeof(mCupSettings));
        src = (u8*)src + sizeof(mCupSettings);
        memcpy(mGameInfo, src, sizeof(mGameInfo));
        src = (u8*)src + sizeof(mGameInfo);
        memcpy(mTeamStats, src, sizeof(mTeamStats));
        src = (u8*)src + sizeof(mTeamStats);
        memcpy(mRoundResults, src, sizeof(mRoundResults));
        src = (u8*)src + sizeof(mRoundResults);
        return src;
    }

    BasicGameInfo mGameInfo[Teams - 1];
    TeamStats mTeamStats[Teams];
    signed char mRoundResults[3];
}; // total size: 0x184

template <u16 Teams>
BasicGameInfo* Knockout<Teams>::GetGameInfo(int i, int j)
{
    int offset = 0;
    int k;

    for (k = 1; k <= i; k++)
    {
        offset += Teams / (k * 2);
    }

    offset = j + offset;
    return &mGameInfo[offset];
}

#endif // _CUP_H_
