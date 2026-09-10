#ifndef _GAMESCENEMANAGER_H_
#define _GAMESCENEMANAGER_H_

#include "types.h"

#include "NL/nlSingleton.h"
#include "Game/BaseGameSceneManager.h"

class GameSceneManager : public BaseGameSceneManager, public nlSingleton<GameSceneManager>
{
public:
    GameSceneManager();
    ~GameSceneManager();

    void Pop();
};

#endif // _GAMESCENEMANAGER_H_
