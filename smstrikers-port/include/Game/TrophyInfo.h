#ifndef _TROPHYINFO_H_
#define _TROPHYINFO_H_

#include "types.h"

enum eMilestoneColour
{
    INVALID_MILESTONE_COLOUR = -1,
    MILESTONE_BLACK = 0,
    MILESTONE_BRONZE = 1,
    MILESTONE_SILVER = 2,
    MILESTONE_GOLD = 3,
    NUM_MILESTONE_COLOURS = 4,
};

enum eTrophyType
{
    INVALID_TROPHY = -1,
    TROPHY_MUSHROOM_CUP = 0,
    TROPHY_FLOWER_CUP = 1,
    TROPHY_STAR_CUP = 2,
    TROPHY_BOWSER_CUP = 3,
    TROPHY_SUPER_MUSHROOM_CUP = 4,
    TROPHY_SUPER_FLOWER_CUP = 5,
    TROPHY_SUPER_STAR_CUP = 6,
    TROPHY_SUPER_BOWSER_CUP = 7,
    NUM_BATTLE_TROPHIES = 8,
    TROPHY_VETERAN_CUP = 8,
    TROPHY_SNIPER_CUP = 9,
    TROPHY_STRIKER_CUP = 10,
    TROPHY_TACTICIAN_CUP = 11,
    TROPHY_PARAMEDIC_CUP = 12,
    NUM_TROPHIES = 13,
};

const char* GetThrophyModelName(eTrophyType type);

static const char* TROPHY_TEXTURE_FILENAMES[NUM_TROPHIES] = {
    "fe/trophies/cups_mushroom",
    "fe/trophies/cups_flower",
    "fe/trophies/cups_star",
    "fe/trophies/cups_bowser",
    "fe/trophies/cups_super_mushroom",
    "fe/trophies/cups_super_flower",
    "fe/trophies/cups_super_star",
    "fe/trophies/cups_super_bowser",
    "fe/trophies/cups_veteran",
    "fe/trophies/cups_sniper",
    "fe/trophies/cups_super_striker",
    "fe/trophies/cups_super_team",
    "fe/trophies/cups_lakitu",
};

#endif // _TROPHYINFO_H_
