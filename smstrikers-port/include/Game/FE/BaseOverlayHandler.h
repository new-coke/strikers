#ifndef _BASEOVERLAYHANDLER_H_
#define _BASEOVERLAYHANDLER_H_

#include "Game/BaseSceneHandler.h"

enum ScreenPosition
{
    POSITION_INVALID = -1,
    POSITION_TOP_LEFT = 0,
    POSITION_TOP_MIDDLE = 1,
    POSITION_TOP_RIGHT = 2,
    POSITION_MIDDLE_LEFT = 3,
    POSITION_MIDDLE_MIDDLE = 4,
    POSITION_MIDDLE_RIGHT = 5,
    POSITION_BOTTOM_LEFT = 6,
    POSITION_BOTTOM_MIDDLE = 7,
    POSITION_BOTTOM_RIGHT = 8,
    POSITION_TOP = 9,
    POSITION_MIDDLE = 10,
    POSITION_BOTTOM = 11,
    POSITION_ALL = 12,
    NUM_POSITIONS = 13,
};

class BaseOverlayHandler : public BaseSceneHandler
{
public:
    BaseOverlayHandler(u32 VisibilityMask, ScreenPosition position = POSITION_TOP)
        : BaseSceneHandler()
    {
        mPosition = position;
        mVisibilityMask = VisibilityMask;
        mWasLastVisible = false;
    }
    virtual ~BaseOverlayHandler() { };

    /* 0x1C */ ScreenPosition mPosition;
    /* 0x20 */ u32 mVisibilityMask;
    /* 0x24 */ bool mWasLastVisible;
}; // total size: 0x28

#endif // _BASEOVERLAYHANDLER_H_
