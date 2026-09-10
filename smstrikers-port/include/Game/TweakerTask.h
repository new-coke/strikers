#ifndef _TWEAKERTASK_H_
#define _TWEAKERTASK_H_

#include "NL/nlTask.h"

class TweakerTask : public nlTask
{
public:
    virtual void Run(float dt);
    virtual const char* GetName() { return "Tweaker"; };
}; // total size: 0x18

#endif // _TWEAKERTASK_H_
