#include "Game/TerrainTypes.h"

#include "NL/nlString.h"

static const char* g_TerrainNames[7] = {
    "grass",
    "bluegrass",
    "concrete",
    "blueconcrete",
    "wood",
    "metal",
    "rubber",
};

int g_TerrainTypeHashes[7];

/**
 * Offset/Address/Size: 0x0 | 0x8019EC68 | size: 0x14
 */
const char* GetTerrainName(eTerrain terrain)
{
    return g_TerrainNames[(int)terrain];
}

/**
 * Offset/Address/Size: 0x14 | 0x8019EC7C | size: 0x68
 */
void TerrainInitialize()
{
    int i = 0;
    do
    {
        const char* name = g_TerrainNames[i];
        g_TerrainTypeHashes[i] = nlStringHash(name);
        ++i;
    } while (i < 7);
}
