#ifndef _DISPATCHEVENTSTASK_H_
#define _DISPATCHEVENTSTASK_H_

#include "NL/nlTask.h"

class DispatchEventsTask : public nlTask
{
public:
    void Run(float dt);
    virtual const char* GetName()
    {
        return "Dispatch Events";
    };
};

#endif // _DISPATCHEVENTSTASK_H_
