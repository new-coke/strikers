#include "Game/MarioTriggers.h"

#include "Game/CharacterTriggers.h"

/**
 * Offset/Address/Size: 0x0 | 0x801A3FD4 | size: 0x20
 */
void AnimTriggerCallback_MARIO(unsigned int uParam)
{
    CharacterTriggerHandler(uParam);
}
