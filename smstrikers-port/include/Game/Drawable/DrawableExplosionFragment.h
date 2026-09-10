#ifndef _DRAWABLEEXPLOSIONFRAGMENT_H_
#define _DRAWABLEEXPLOSIONFRAGMENT_H_

#include "Game/Replay.h"
#include "Game/Physics/PhysicsObject.h"

class DrawableExplosionFragment
{
public:
    template <typename T>
    void Replay(T&);

    void Blend(const float* w, const DrawableExplosionFragment& a, const DrawableExplosionFragment& b);
    void Render() const;
    void Grab();

    /* 0x0, */ u16 mID;
    /* 0x2, */ bool mVisible;
    /* 0x4, */ float mOpacity;
    /* 0x8, */ nlVector3 mPosition;
    /* 0x14 */ nlQuaternion mOrientation;
    /* 0x24 */ u32 mFragmentModelHash;
    /* 0x28 */ PhysicsObject* mpPhysicsObject;
}; // total size: 0x2C

template <typename T>
void DrawableExplosionFragment::Replay(T& frame)
{
    Replayable<3, T, bool>(frame, mVisible);
    if (mVisible)
    {
        // PORT: the member is a u32; `unsigned long` is wider here and would bind a temporary to the wrong overload.
        Replayable<3, T, u32>(frame, mFragmentModelHash);
        Replayable<3, T, nlVector3>(frame, mPosition);
        Replayable<3, T, nlQuaternion>(frame, mOrientation);
        Replayable<3, T, float>(frame, mOpacity);
    }
}

// PORT: `template <>` here made this a specialisation, which stops DrawableExplosionFragment.cpp's explicit instantiation emitting anything.

// PORT: `template <>` here made this a specialisation, which stops DrawableExplosionFragment.cpp's explicit instantiation emitting anything.

#endif // _DRAWABLEEXPLOSIONFRAGMENT_H_
