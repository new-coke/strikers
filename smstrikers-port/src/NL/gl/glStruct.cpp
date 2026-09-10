#include "NL/gl/glStruct.h"

gl_ScreenInfo _ScreenInfo;

/**
 * Offset/Address/Size: 0x0 | 0x801DE3CC | size: 0x10
 */
u32 glGetScreenHeight()
{
    return _ScreenInfo.ScreenHeight;
}

/**
 * Offset/Address/Size: 0x10 | 0x801DE3DC | size: 0x10
 */
u32 glGetScreenWidth()
{
    return _ScreenInfo.ScreenWidth;
}

/**
 * Offset/Address/Size: 0x20 | 0x801DE3EC | size: 0xC
 */
gl_ScreenInfo* glGetScreenInfo()
{
    return &_ScreenInfo;
}
