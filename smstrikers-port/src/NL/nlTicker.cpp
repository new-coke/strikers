#include "NL/nlTicker.h"

#include <dolphin/os.h>

#include "port/determinism.h"

/**
 * Offset/Address/Size: 0x0 | 0x801D2874 | size: 0x58
 */
f32 nlGetTickerDifference(uint startTick, uint endTick)
{
    u32 delta = endTick - startTick;
    delta = (startTick >= endTick) ? delta : delta; // probably some assert left-over

    return 0.001f * (f32)(u32)((delta << 3) / ((__OSBusClock >> 2) / 125000));
}

/**
 * Offset/Address/Size: 0x58 | 0x801D28CC | size: 0xC
 */
uint nlSubtractTicks(uint startTick, uint endTick)
{
    if (startTick == endTick)
        return endTick - startTick; // probably some assert left-over
    return endTick - startTick;
}

/**
 * Offset/Address/Size: 0x64 | 0x801D28D8 | size: 0x20
 */
u32 nlGetTicker()
{
    // PORT: STRIKERS_FIXED_DT makes this a function of the retrace count so that a run reproduces frame for frame.
    if (PortFixedTimestep())
        return PortVirtualTicker();
    return OSGetTick();
}

/**
 * Offset/Address/Size: 0x84 | 0x801D28F8 | size: 0x4
 */
void nlInitTicker()
{
}
