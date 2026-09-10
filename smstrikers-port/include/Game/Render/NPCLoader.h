#ifndef _NPCLOADER_H_
#define _NPCLOADER_H_

#include "Game/Loader.h"

class NPCLoader : public Loader
{
public:
    bool StartLoad(LoadingManager* manager);
};

#endif // _NPCLOADER_H_
