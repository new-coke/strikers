#ifndef _AILOADER_H_
#define _AILOADER_H_

#include "Game/Loader.h"

class AILoader : public Loader
{
public:
    virtual bool StartLoad(LoadingManager* manager);
};

#endif // _AILOADER_H_
