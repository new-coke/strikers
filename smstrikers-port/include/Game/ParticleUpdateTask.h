#ifndef _PARTICLEUPDATETASK_H_
#define _PARTICLEUPDATETASK_H_

#include "NL/nlTask.h"

class ParticleUpdateTask : public nlTask
{
public:
    virtual void Run(float dt);
    virtual const char* GetName()
    {
        return "Particle Update";
    }
    static void SetTimeScale(float timeScale);

    static float sfTimeScale;
}; // total size: 0x18

#endif // _PARTICLEUPDATETASK_H_
