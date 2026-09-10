#include "Game/GoalieFatigue.h"

/**
 * Offset/Address/Size: 0xA0 | 0x80052CEC | size: 0x1C
 */
GoalieFatigue::GoalieFatigue()
{
    mfEnergyLevel = 0.0f;
    mfRecoverRate = 1.0f;
    mfTimeSinceLastSave = 0.0f;
    mfHotStreakTimer = 0.0f;
}

/**
 * Offset/Address/Size: 0x88 | 0x80052CD4 | size: 0x18
 */
void GoalieFatigue::Reset()
{
    mfEnergyLevel = 100.0f;
    mfTimeSinceLastSave = 0.0f;
    mfHotStreakTimer = 0.0f;
}

/**
 * Offset/Address/Size: 0x38 | 0x80052C84 | size: 0x50
 */
void GoalieFatigue::Update(float dt)
{
    if (mfEnergyLevel < 100.0f)
    {
        mfEnergyLevel = (mfRecoverRate * dt) + mfEnergyLevel;
    }

    if (mfHotStreakTimer > 0.0f)
    {
        mfHotStreakTimer = mfHotStreakTimer - dt;
        if (mfHotStreakTimer <= 0.0f)
        {
            mfEnergyLevel = 100.0f;
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80052C4C | size: 0x38
 */
void GoalieFatigue::RegisterShot(float fLevel)
{
    mfEnergyLevel -= fLevel;
    if (mfEnergyLevel < 100.0f)
    {
        mfHotStreakTimer = 0.0f;
        if (mfEnergyLevel < 0.0f)
        {
            mfEnergyLevel = 0.0f;
        }
    }
}
