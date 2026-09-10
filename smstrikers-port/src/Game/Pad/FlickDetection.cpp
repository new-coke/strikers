#include "Game/Pad/FlickDetection.h"

#include "Game/AI/AIPad.h"

namespace
{

static char FLICK_NONE = 0x20;

StaticRing<char> history[4];
char historyStorage[4][16] = { };

} // namespace

/**
 * Offset/Address/Size: 0x174 | 0x80192264 | size: 0x68
 */
void FlickDetection::Initialize()
{
    for (int i = 0; i < 4; i++)
    {
        history[i].mStorage = &historyStorage[i][0];
        history[i].mCapacity = 16;
        history[i].mSize = 0;
        history[i].mEnd = 0;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x801920F0 | size: 0x174
 */
void FlickDetection::Update()
{
    for (int i = 0; i < 4; i++)
    {
        cAIPad& pad = AIPadManager::mAIPads[i];
        StaticRing<char>& ring = history[i];

        float magnitude = pad.GetCStickMovementStickMagnitude();

        if (magnitude > 0.6f)
        {
            u16 direction = pad.GetCStickMovementStickDirection();

            char flickValue = 0x72;
            if (direction >= 0x2000 && direction < 0x6000)
            {
                flickValue = 0x75;
            }
            else if (direction >= 0x6000 && direction < 0xA000)
            {
                flickValue = 0x6c;
            }
            else if (direction >= 0xA000 && direction < 0xE000)
            {
                flickValue = 0x64;
            }

            ring.mStorage[ring.mEnd] = flickValue;
            ring.mEnd = (ring.mEnd + 1) % ring.mCapacity;

            ring.mSize += 1;
            if (ring.mSize > ring.mCapacity)
            {
                ring.mSize = ring.mCapacity;
            }
        }
        else
        {
            ring.mStorage[ring.mEnd] = FLICK_NONE;
            ring.mEnd = (ring.mEnd + 1) % ring.mCapacity;

            ring.mSize += 1;
            if (ring.mSize > ring.mCapacity)
            {
                ring.mSize = ring.mCapacity;
            }
        }
    }
}
