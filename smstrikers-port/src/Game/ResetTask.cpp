#include "Game/ResetTask.h"

#include "dolphin/os.h"
#include "dolphin/vi.h"
#include "dolphin/dvd.h"
#include "NL/gl/glPlat.h"
#include "NL/globalpad.h"

#include "Game/SH/SHSaveLoad.h"
#include "Game/DB/SaveLoad.h"

s32 ResetTask::s_ResetMode = 0;
RESET_STATE ResetTask::s_ResetState = RS_RUNNING;
bool ResetTask::s_AudioInInit = false;
bool ResetTask::s_ResetPressed = false;
bool ResetTask::s_resetPaused = false;
bool ResetTask::s_checkCardRemoved = false;

u32 softResetTime[4] = { 0, 0, 0, 0 };

extern bool g_bEnableGamecubePadMonkey;

/**
 * Offset/Address/Size: 0x1EC | 0x801733F8 | size: 0x110
 */
void ResetTask::Run(float dt)
{
    if (DVDGetDriveStatus() == -1)
    {
        return;
    }

    if (OSGetResetButtonState())
    {
        s_ResetPressed = 1;
        return;
    }

    if (!OSGetResetButtonState())
    {
        if (s_ResetPressed)
        {
            s_ResetMode = 0;
            s_ResetState = (s_ResetState == RS_RUNNING) ? RS_STARTRESET : s_ResetState;
        }
    }

    if (s_checkCardRemoved)
    {
        SaveLoadScene::UpdateCardRemovedFlag();
    }

    if (s_ResetState != RS_STARTRESET)
    {
        if (s_ResetState < RS_STARTRESET)
        {
            return;
        }
    }
    else if (s_resetPaused == 0)
    {

        SaveLoadScene::StartSaveNow();
        while (s_AudioInInit)
        {
            OSYieldThread();
        }

        while (SaveLoad::CardBusy())
        {
            OSYieldThread();
        }

        VISetBlack(1);
        VIFlush();
        VIWaitForRetrace();

        OSResetSystem(s_ResetMode, glx_GetResetCode(), (s_ResetMode == 1) ? true : false);

        for (;;)
        {
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8017320C | size: 0x1EC
 */
void HandleSoftReset()
{
    static OSTime LastTime = 0;
    static u32 MinButtonHoldTime = 0x1F4; // 500 ms

    if (!LastTime)
    {
        LastTime = OSGetTime();
    }

    const OSTime now = OSGetTime();
    const u32 dtMs = OSTicksToMilliseconds(now - LastTime);

    LastTime = OSGetTime();

    if (g_bEnableGamecubePadMonkey)
    {
        return;
    }

    for (s32 i = 0; i < 4; ++i)
    {
        cGlobalPad* pad = cPadManager::GetPad(i);
        if (!pad)
            break;

        if (pad->IsConnected())
        {
            if (pad->IsPressed(0x1000, 0) && pad->IsPressed(0x0400, 0) && pad->IsPressed(0x0200, 0))
            {
                softResetTime[i] += dtMs;

                if (softResetTime[i] > MinButtonHoldTime)
                {
                    ResetTask::s_ResetMode = 0;
                    ResetTask::s_ResetState = (ResetTask::s_ResetState == 0) ? RS_STARTRESET : ResetTask::s_ResetState;
                    return;
                }
            }
            else
            {
                softResetTime[i] = 0;
            }
        }
    }
}
