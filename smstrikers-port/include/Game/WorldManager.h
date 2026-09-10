#ifndef _WORLDMANAGER_H_
#define _WORLDMANAGER_H_

#include "Game/World.h"

class WorldManager
{
public:
    static World* s_World;

    static void DestroyWorld();
    static bool LoadWorld(const char* szBaseName, bool forfe);
    static void UpdateWorld(float timeDelta = 0.f);
    static void RenderWorld();
};

#endif // _WORLDMANAGER_H_
