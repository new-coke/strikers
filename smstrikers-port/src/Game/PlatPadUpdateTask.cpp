#include "Game/PlatPadUpdateTask.h"

#include "Game/RumbleActions.h"

/**
 * Offset/Address/Size: 0x0 | 0x80170B80 | size: 0x20
 */
void PlatPadUpdateTask::Run(float dt)
{
    UpdateRumbleActions(dt);
}
