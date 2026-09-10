#include "Game/ProfileTask.h"
#include "types.h"

static s32 frequencyIndex = 0;
static s8 init = false;

/**
 * Offset/Address/Size: 0x0 | 0x80170B60 | size: 0x20
 */
void UpdateProfile()
{
    if (!init)
    {
        frequencyIndex = 0;
        init = true;
    }
}
