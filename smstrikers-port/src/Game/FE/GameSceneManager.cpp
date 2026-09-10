#include "Game/GameSceneManager.h"

template <>
GameSceneManager* nlSingleton<GameSceneManager>::s_pInstance = 0;

/**
 * Offset/Address/Size: 0x80 | 0x80096C18 | size: 0x3C
 */
GameSceneManager::GameSceneManager()
{
}

/**
 * Offset/Address/Size: 0x20 | 0x80096BB8 | size: 0x60
 */
GameSceneManager::~GameSceneManager()
{
}

/**
 * Offset/Address/Size: 0x0 | 0x80096B98 | size: 0x20
 */
void GameSceneManager::Pop()
{
    BaseGameSceneManager::Pop();
}
