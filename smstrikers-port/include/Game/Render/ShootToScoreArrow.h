#ifndef _SHOOTTOSCOREARROW_H_
#define _SHOOTTOSCOREARROW_H_

#include "NL/gl/glDraw2.h"

class GameRenderTask;

class WorldDarkening
{
public:
    WorldDarkening()
    {
        mRate = 0.f;
        mPos = 0.f;
        mTo = 0.f;
        mActive = 0;
    };

    static WorldDarkening& Instance();
    void Fade(float rate, float to);
    void UpdateAndRender(float deltaTime);

    /* 0x0 */ float mRate;
    /* 0x4 */ float mPos;
    /* 0x8 */ float mTo;
    /* 0xC */ bool mActive;

private:
    friend class GameRenderTask;

    static inline void SetPolyColour(glPoly2& poly, u8 r, u8 g, u8 b, u8 a)
    {
        nlColour color = { 0, 0, 0, 0 };
        nlColourSet(color, r, g, b, a);
        poly.SetColour(color);
    }
}; // total size: 0x10

#endif // _SHOOTTOSCOREARROW_H_
