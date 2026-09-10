
#include "Game/TrophyInfo.h"

/**
 * Offset/Address/Size: 0x0 | 0x801918A0 | size: 0xA8
 */
const char* GetThrophyModelName(eTrophyType type)
{
    if (type == TROPHY_MUSHROOM_CUP)
        return "gameplay/MushroomCup";
    if (type == TROPHY_FLOWER_CUP)
        return "gameplay/FlowerCup";
    if (type == TROPHY_STAR_CUP)
        return "gameplay/StarCup";
    if (type == TROPHY_BOWSER_CUP)
        return "gameplay/BowserCup";
    if (type == TROPHY_SUPER_MUSHROOM_CUP)
        return "gameplay/SuperMushroomCup";
    if (type == TROPHY_SUPER_FLOWER_CUP)
        return "gameplay/SuperFlowerCup";
    if (type == TROPHY_SUPER_STAR_CUP)
        return "gameplay/SuperStarCup";
    if (type == TROPHY_SUPER_BOWSER_CUP)
        return "gameplay/SuperBowserCup";
    return "invalid eTrophyType";
}
