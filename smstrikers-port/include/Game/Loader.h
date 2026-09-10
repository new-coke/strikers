#ifndef _LOADER_H_
#define _LOADER_H_

#include "types.h"

class LoadingManager;

class Loader
{
public:
    virtual bool StartLoad(LoadingManager*) = 0;
    virtual bool Update() { return false; };
    virtual const char* GetName() { return "Loader"; };
};

#endif // _LOADER_H_
