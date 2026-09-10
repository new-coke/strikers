#ifndef _PHOTOFLASHEFFECT_H_
#define _PHOTOFLASHEFFECT_H_

#include "types.h"

class PhotoFlash
{
public:
    // PORT: static, as both call sites and FielderActions.cpp's own declaration have them.
    static void Flash();
    static void Render(float dt);

    static s32 sNumFramesSinceFlash;
};

#endif // _PHOTOFLASHEFFECT_H_
