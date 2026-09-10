// The two THP entry points src/Game/Sys/THPSimple.cpp needs that the SDK's header does not declare,
// because they are the port's and not the SDK's.

#ifndef PORT_THP_H
#define PORT_THP_H

#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

s32 port_thp_video_decode(const void* frame, u32 size, void* tileY, void* tileU,
                          void* tileV);
int port_thp_frame_due(float frameRate);
void port_thp_frame_shown(void);

#ifdef __cplusplus
}
#endif

#endif // PORT_THP_H
