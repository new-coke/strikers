#ifndef _FLICKDETECTION_H_
#define _FLICKDETECTION_H_

// void 0x80192308..0x80192320 | size: 0x18;

class FlickDetection
{
public:
    static void Initialize();
    static void Update();
};

template <typename T>
class StaticRing
{
public:
    StaticRing();

    /* 0x00 */ T* mStorage;
    /* 0x04 */ int mCapacity;
    /* 0x08 */ int mSize;
    /* 0x0C */ int mEnd;
};

#endif // _FLICKDETECTION_H_
