#include "Game/TweakerTask.h"

#include "NL/globalpad.h"
#include "Game/main.h"

extern bool g_bEnableGamecubePadMonkey;

/**
 * Offset/Address/Size: 0x0 | 0x801709D0 | size: 0xFC
 */
void TweakerTask::Run(float dt)
{
    if (!g_bEnableGamecubePadMonkey)
    {
        cGlobalPad* pad = cPadManager::GetPad(0);
        if (pad != NULL)
        {
            if (!pad->IsConnected())
            {
                pad = cPadManager::GetPad(1);
            }
        }

        if (g_bTweaking)
        {
            pad->IsPressed(9, true);
            if (!pad->JustPressed(0xB, true)
                && !pad->JustPressed(0xC, true)
                && !pad->JustPressed(0xD, true)
                && !pad->JustPressed(0xE, true))
            {
                pad->JustPressed(0xA, true);
            }
        }
    }
}
