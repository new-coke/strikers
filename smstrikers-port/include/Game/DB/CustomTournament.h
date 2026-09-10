#ifndef _CUSTOMTOURNAMENT_H_
#define _CUSTOMTOURNAMENT_H_

#include "types.h"
#include "Game/Team.h"
#include "Game/DB/UserOptions.h"
#include "Game/DB/Cup.h"

enum eTournamentMode
{
    TM_INVALID = -1,
    TM_LEAGUE = 0,
    TM_KNOCKOUT = 1,
    TM_NUMTOURNMODES = 2,
};

class CustomTournament
{
public:
    CustomTournament();
    ~CustomTournament();
    BaseCup* ConstructCup();
    int GetSaveDataSize() const;
    void SerializeData(void* buffer) const;
    void DeserializeData(void* buffer);

    union DataSpace
    {
        DataSpace() {}
        Cup<3, 3> cup31;                         // size 0x144
        Cup<4, 3> cup41;                         // size 0x1E4
        Cup<5, 5> cup51;                         // size 0x2A8
        Cup<6, 5> cup61;                         // size 0x388
        Cup<7, 7> cup71;                         // size 0x488
        Cup<8, 7> cup81;                         // size 0x5A8
        Cup<3, 6> cup32;                         // size 0x1A8
        Cup<4, 6> cup42;                         // size 0x2A8
        Cup<5, 10> cup52;                        // size 0x3EC
        Cup<6, 10> cup62;                        // size 0x56C
        Cup<7, 14> cup72;                        // size 0x730
        Cup<8, 14> cup82;                        // size 0x930
        Knockout<4> knockout4;                   // size 0x184
        Knockout<8> knockout8;                   // size 0x304
    } m_dataSpace;                               // offset 0x0, size 0x930
    /* 0x930 */ unsigned long m_fenceBoundary;   // offset 0x930, size 0x4
    /* 0x934 */ BaseCup* m_cup;                  // offset 0x934, size 0x4
    /* 0x938 */ eTournamentMode m_tournMode;     // offset 0x938, size 0x4
    /* 0x93C */ unsigned char m_numTeams;        // offset 0x93C, size 0x1
    /* 0x93D */ unsigned char m_numGamesPerTeam; // offset 0x93D, size 0x1
    /* 0x93E */ bool m_cupConstructed;           // offset 0x93E, size 0x1
    /* 0x940 */ int m_largestCupSize;            // offset 0x940, size 0x4
}; // total size: 0x944

// class Knockout<8>
// {
// public:
// };

// class Cup<7, 14>
// {
// public:
// };

// class Cup<5, 10>
// {
// public:
// };

// class Cup<3, 6>
// {
// public:
// };

// class Cup<7, 7>
// {
// public:
// };

// class Cup<5, 5>
// {
// public:
// };

// class Cup<3, 3>
// {
// public:
// };

#endif // _CUSTOMTOURNAMENT_H_
