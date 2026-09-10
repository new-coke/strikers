#ifndef _DRAWABLEPOWERUP_H_
#define _DRAWABLEPOWERUP_H_

#include "Game/Replay.h"
#include "Game/Drawable/DrawableObj.h"

class PowerupBase;

class DrawablePowerup
{
public:
    template <typename T>
    void Replay(T&);

    void Blend(const float* blendFactors, const DrawablePowerup& lhs, const DrawablePowerup& rhs);
    void Render(int idx) const;
    void Grab(int idx);
    PowerupBase* GetPowerup(int idx) const;

    /* 0x00 */ s8 mType;
    /* 0x04 */ float mScale;
    /* 0x08 */ float mRadius;
    /* 0x0C */ bool mVisible;
    /* 0x10 */ nlVector3 mPosition;
    /* 0x1C */ u16 mOrientation;
}; // total size: 0x20

template <typename T>
void DrawablePowerup::Replay(T& frame)
{
    Replayable<3, T, bool>(frame, mVisible);
    if (mVisible)
    {
        Replayable<3, T, char>(frame, (char&)mType);
        Replayable<3, T, unsigned short>(frame, mOrientation);
        Replayable<3, T, nlVector3>(frame, mPosition);
        Replayable<3, T, float>(frame, mScale);
        Replayable<3, T, float>(frame, mRadius);
    }
}

// PORT: `template <>` here made this a specialisation, which stops DrawablePowerup.cpp's explicit instantiation emitting anything.

// PORT: `template <>` here made this a specialisation, which stops DrawablePowerup.cpp's explicit instantiation emitting anything.

#endif // _DRAWABLEPOWERUP_H_
