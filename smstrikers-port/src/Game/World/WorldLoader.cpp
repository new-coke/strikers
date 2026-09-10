#include "Game/World/WorldLoader.h"

#include "Game/WorldManager.h"

#include "NL/nlDebug.h"
#include "Game/Sys/debug.h"

WorldLoader TheWorldLoader;

static char* g_szStadiumName[7] = {
    "Mario_Stadium",
    "The_Palace",
    "DK_Daisy",
    "Wario_Stadium",
    "Yoshi_Stadium",
    "Super_Stadium",
    "Forbidden_Dome"
};

eTerrain g_StadiumTerrain[7] = {
    T_Grass,
    T_Grass,
    T_Wood,
    T_Concrete,
    T_BlueGrass,
    T_Metal,
    T_Metal
};

/**
 * Offset/Address/Size: 0xB4 | 0x8019ABCC | size: 0x6C
 */
bool WorldLoader::StartLoad(LoadingManager* manager)
{
    eStadiumID stadiumtoload = GameInfoManager::Instance()->GetStadium();
    if (WorldManager::LoadWorld(g_szStadiumName[stadiumtoload], mTEMP_FOR_FE) == 0)
    {
        tDebugPrintManager::Print(DC_LOADER, "Failed to load the game world.\n");
        nlBreak();
    }
    return true;
}

/**
 * Offset/Address/Size: 0xA0 | 0x8019ABB8 | size: 0x14
 */
const char* WorldLoader::GetStadiumFilename(eStadiumID stadiumid) const
{
    return g_szStadiumName[stadiumid];
}

/**
 * Offset/Address/Size: 0x8C | 0x8019ABA4 | size: 0x14
 */
eTerrain WorldLoader::GetStadiumTerrain(eStadiumID stadiumID) const
{
    return g_StadiumTerrain[stadiumID];
}

/**
 * Offset/Address/Size: 0x0 | 0x8019AB18 | size: 0x8C
 */
void WorldLoader::SetStadiumTerrain(eStadiumID stadiumID, const char* szTerrainName)
{
    eTerrain foundTerrain = T_Num;

    for (eTerrain terrain = T_Grass; terrain < T_Num; terrain = (eTerrain)((int)terrain + 1))
    {
        if (nlStrCmp(szTerrainName, GetTerrainName(terrain)) == 0)
        {
            foundTerrain = terrain;
            break;
        }
    }

    g_StadiumTerrain[stadiumID] = foundTerrain;
}
