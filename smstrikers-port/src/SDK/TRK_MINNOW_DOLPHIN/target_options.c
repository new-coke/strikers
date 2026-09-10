#include "PowerPC_EABI_Support/MetroTRK/trk.h"

static u8 bUseSerialIO;

/**
 * Offset/Address/Size: 0x10 | 0x80229F30 | size: 0xC
 */
void SetUseSerialIO(u8 sio)
{
    bUseSerialIO = sio;
    return;
}

/**
 * Offset/Address/Size: 0x0 | 0x80229F20 | size: 0x10
 */
u8 GetUseSerialIO(void)
{
    return bUseSerialIO;
}
