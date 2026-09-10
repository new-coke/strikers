#include "PowerPC_EABI_Support/MetroTRK/trk.h"

/**
 * Offset/Address/Size: 0x0 | 0x80229EEC | size: 0x34
 */
DSError TRKTargetContinue(void)
{
    TRKTargetSetStopped(0);
    UnreserveEXI2Port();
    TRKSwapAndGo();
    ReserveEXI2Port();

    return DS_NoError;
}
