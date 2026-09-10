#ifndef _PLATPADUPDATETASK_H_
#define _PLATPADUPDATETASK_H_

#include "NL/nlTask.h"

class PlatPadUpdateTask : public nlTask
{
public:
    virtual void Run(float dt);
    virtual const char* GetName()
    {
        return "Plat Pad";
    };

}; // size 0x18

#endif // _PLATPADUPDATETASK_H_
