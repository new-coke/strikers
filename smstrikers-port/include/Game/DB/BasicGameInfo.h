#ifndef _BASICGAMEINFO_H_
#define _BASICGAMEINFO_H_

#include "types.h"
#include "Game/Team.h"

enum eSidekickID
{
    SK_MYSTERY = -2,
    SK_INVALID = -1,
    SK_TOAD = 0,
    SK_KOOPA = 1,
    SK_HAMMERBROS = 2,
    SK_BIRDO = 3,
    NUM_SIDEKICKS = 4,
};

enum eStadiumID
{
    STAD_INVALID = -1,
    STAD_MARIO_STADIUM = 0,
    STAD_PEACH_TOAD_STADIUM = 1,
    STAD_DK_DAISY = 2,
    STAD_WARIO_STADIUM = 3,
    STAD_YOSHI_STADIUM = 4,
    STAD_SUPER_STADIUM = 5,
    STAD_FORBIDDEN_DOME = 6,
    MAX_STADIUMS = 7,
};

struct BasicGameInfo
{
    /* 0x00 */ eTeamID mTeamIndex[2];
    /* 0x08 */ eSidekickID mSidekickIndex[2];
    /* 0x10 */ eStadiumID mStadiumIndex;
    /* 0x14 */ s16 mPadSides[4];
    /* 0x1C */ s16 mFinalScore[2];

    eTeamID GetTeam(s16 index) const { return mTeamIndex[index]; }

    BasicGameInfo()
    {
        mTeamIndex[0] = TEAM_MARIO;
        mTeamIndex[1] = TEAM_LUIGI;
        mSidekickIndex[0] = SK_TOAD;
        mSidekickIndex[1] = SK_KOOPA;
        mFinalScore[1] = 0;
        mFinalScore[0] = 0;
        mPadSides[0] = -1;
        mPadSides[1] = -1;
        mPadSides[2] = -1;
        mPadSides[3] = -1;
        mStadiumIndex = STAD_MARIO_STADIUM;
    }
}; // total size: 0x20

#endif // _BASICGAMEINFO_H_
