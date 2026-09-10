#include "Game/DispatchEventsTask.h"

#include "Game/Sys/eventman.h"

extern EventManager* g_pEventManager;

/**
 * Offset/Address/Size: 0x0 | 0x801715A0 | size: 0x24
 */
void DispatchEventsTask::Run(float dt)
{
    g_pEventManager->DispatchEvents();
}
