#ifndef _WORLDLOADER_H_
#define _WORLDLOADER_H_

#include "Game/Loader.h"
#include "Game/GameInfo.h"
#include "Game/TerrainTypes.h"

class WorldLoader : public Loader
{
public:
    WorldLoader() { mTEMP_FOR_FE = false; }

    bool StartLoad(LoadingManager* manager);
    const char* GetStadiumFilename(eStadiumID stadiumid) const;
    eTerrain GetStadiumTerrain(eStadiumID stadiumID) const;
    void SetStadiumTerrain(eStadiumID stadiumID, const char* szTerrainName);

    /* 0x4 */ bool mTEMP_FOR_FE;
}; // total size: 0x8

extern WorldLoader TheWorldLoader;

#endif // _WORLDLOADER_H_
