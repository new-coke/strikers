#include "Game/AI/AILoader.h"

#include "NL/nlMemory.h"

#include "Game/Ball.h"
#include "Game/Physics/PhysicsFakeBall.h"
#include "Game/CharacterLifecycle.h"

AILoader TheAILoader;

/**
 * Offset/Address/Size: 0x0 | 0x800056C0 | size: 0x50
 */
bool AILoader::StartLoad(LoadingManager* manager)
{
    g_pBall = new (nlMalloc(sizeof(cBall), 8, false)) cBall();

    FakeBallWorld::Init(g_pBall);

    CreateCharacters();

    return true;
}
