#ifndef _COMUPDATETASK_H_
#define _COMUPDATETASK_H_

#include "NL/nlTask.h"

class ComUpdateTask : public nlTask
{
public:
    virtual const char* GetName()
    {
        return "Com Update";
    };
    virtual void Run(float dt);
    void Initialize();
};

#endif // _COMUPDATETASK_H_
