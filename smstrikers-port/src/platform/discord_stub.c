// These builds have no desktop Discord IPC.

#if defined(__SWITCH__) || defined(__ANDROID__)

#include "port/discord.h"

void PortDiscordInit(void) {}

void PortDiscordUpdate(int inMenuState, int menu)
{
    (void)inMenuState;
    (void)menu;
}

void PortDiscordSetMatch(const PortDiscordMatch* match)
{
    (void)match;
}

void PortDiscordShutdown(void) {}

#endif
