#ifndef _TERRAINTYPES_H_
#define _TERRAINTYPES_H_

enum eTerrain
{
    T_Grass = 0,
    T_BlueGrass = 1,
    T_Concrete = 2,
    T_BlueConcrete = 3,
    T_Wood = 4,
    T_Metal = 5,
    T_Rubber = 6,
    T_Num = 7,
};

const char* GetTerrainName(eTerrain terrain);

void TerrainInitialize();

extern int g_TerrainTypeHashes[7];

#endif // _TERRAINTYPES_H_
