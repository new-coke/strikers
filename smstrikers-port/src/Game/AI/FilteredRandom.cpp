#include "Game/AI/FilteredRandom.h"

#include "NL/nlMath.h"

extern unsigned int nlDefaultSeed;

const static u8 InitialHistoryChance[20] = {
    0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0
};

const static u8 InitialHistoryRange[10] = {
    3, 8, 0, 6, 7, 9, 7, 0, 3, 5
};

const static f32 InitialHistoryReal[5] = {
    0.9, 0.3, 0.1, 0.4, 0.6
};

/**
 * Offset/Address/Size: 0x190 | 0x800381A4 | size: 0xAC
 */
FilteredRandomChance::FilteredRandomChance()
{
    for (int i = 0; i < 10; i++)
    {
        m_hist[i] = InitialHistoryChance[i];
    }
    for (int i = 0; i < 10; i++)
    {
        m_hist[i + 10] = InitialHistoryChance[i + 10];
    }
}

/**
 * Offset/Address/Size: 0x144 | 0x80038158 | size: 0x4C
 */
bool FilteredRandomChance::genrand(float chance)
{
    float random = nlRandomf(1.0f, &nlDefaultSeed);
    if (random <= chance)
    {
        return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0xE0 | 0x800380F4 | size: 0x64
 */
FilteredRandomRange::FilteredRandomRange()
{
    m_repeatingRunLength = 1;
    for (int i = 0; i < 10; i++)
    {
        m_hist[i] = InitialHistoryRange[i];
    }
}

/**
 * Offset/Address/Size: 0xB8 | 0x800380CC | size: 0x28
 */
int FilteredRandomRange::genrand(int range)
{
    return nlRandom(range, &nlDefaultSeed);
}

/**
 * Offset/Address/Size: 0x28 | 0x8003803C | size: 0x90
 */
FilteredRandomReal::FilteredRandomReal()
    : m_filteredRandomRange()
{
    const float* p = InitialHistoryReal;
    for (int i = 0; i < 5; i++)
    {
        m_hist[i] = p[i];
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80038014 | size: 0x28
 */
float FilteredRandomReal::genrand()
{
    return nlRandomf(1.0f, &nlDefaultSeed);
}
