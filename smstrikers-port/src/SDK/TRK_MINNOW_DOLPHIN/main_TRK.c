#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/main_TRK.h"

static DSError TRK_mainError;

/**
 * Offset/Address/Size: 0x0 | 0x8022993C | size: 0x58
 */
DSError TRK_main(void)
{
    MWTRACE(1, "TRK_Main \n");
    TRK_mainError = TRKInitializeNub();

    if (TRK_mainError == DS_NoError)
    {
        TRKNubWelcome();
        TRKNubMainLoop();
    }

    TRK_mainError = TRKTerminateNub();
    return TRK_mainError;
}
