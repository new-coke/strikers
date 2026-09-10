#ifndef _RUMBLEACTIONS_H_
#define _RUMBLEACTIONS_H_

#include "NL/globalpad.h"

enum eRumbleActionPreset
{
    RUMBLE_SMALL_CONTACT = 0,
    RUMBLE_MEDIUM_CONTACT = 1,
    RUMBLE_SOLID_CONTACT = 2,
    RUMBLE_SHOT_CONTACT = 3,
    RUMBLE_SHOOT_TO_SCORE_HYPER = 4,
    RUMBLE_SHOOT_TO_SCORE = 5,
};

struct RumbleOp
{
    s32 type;  // 0 = set intensity, 1 = delay, 2 = stop?, etc.
    u32 value; // duration or magnitude depending on type
};

struct RumbleActionState
{
    /* 0x00 */ bool active;
    /* 0x01 */ char pad_0x01[3];
    /* 0x04 */ RumbleOp* ops;
    /* 0x08 */ u32 current;
    /* 0x0C */ bool pending;
    /* 0x0D */ char pad_0x0D[3];
    /* 0x10 */ f32 timer;
}; /* size = 0x14 */

void UpdateRumbleActions(float dt);
void BeginRumbleAction(eRumbleActionPreset preset, cGlobalPad* pad);
void StopRumbleAction(cGlobalPad* pad);

#endif // _RUMBLEACTIONS_H_
