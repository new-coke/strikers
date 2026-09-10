#include "PowerPC_EABI_Support/MetroTRK/trk.h"

/**
 * Offset/Address/Size: 0x10 | 0x80227248 | size: 0x8
 */
DSError TRKInitializeMutex(void* p1)
{
    return DS_NoError;
}

/**
 * Offset/Address/Size: 0x8 | 0x80227240 | size: 0x8
 */
DSError TRKAcquireMutex(void* p1)
{
    return DS_NoError;
}

/**
 * Offset/Address/Size: 0x0 | 0x80227238 | size: 0x8
 */
DSError TRKReleaseMutex(void* p1)
{
    return DS_NoError;
}
