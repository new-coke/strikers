#ifndef _FLICKDETECTION_H_
#define _FLICKDETECTION_H_

#include "types.h"

template <typename T>
class StaticRing
{
public:
    StaticRing()
    {
        {
            mStorage = NULL;
            mCapacity = 0;
            mSize = 0;
            mEnd = 0;
        }
    }

    /* 0x0 */ T* mStorage;
    /* 0x4 */ int mCapacity;
    /* 0x8 */ int mSize;
    /* 0xC */ int mEnd;
}; // total size: 0x10

class FlickDetection
{
public:
    static void Initialize();
    // PORT: static, as Game/FlickDetection.h and every caller already have it.
    static void Update();
};

#endif // _FLICKDETECTION_H_
