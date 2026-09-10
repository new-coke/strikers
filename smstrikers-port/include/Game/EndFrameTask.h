#ifndef _ENDFRAMETASK_H_
#define _ENDFRAMETASK_H_

#include "NL/nlTask.h"

class EndFrameTask : public nlTask
{
public:
    virtual const char* GetName() { return "End Frame"; };
    virtual void Run(float dt);
};

#endif // _ENDFRAMETASK_H_
