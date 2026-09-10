#ifndef _WORLDUPDATETASK_H_
#define _WORLDUPDATETASK_H_

#include "NL/nlTask.h"

class WorldUpdateTask : public nlTask
{
public:
    virtual void Run(float dt);
    virtual const char* GetName()
    {
        return "World";
    };
};

#endif // _WORLDUPDATETASK_H_
