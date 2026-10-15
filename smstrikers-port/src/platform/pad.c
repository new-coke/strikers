// Native pad for Vita; neutral placeholder on other non-Aurora hosts.

#include <string.h>
#include <math.h>

#include "dolphin/pad.h"
#include "port/overlay.h"

#if defined(STRIKERS_VITA)
#include <psp2/ctrl.h>
#endif

static PADSamplingCallback s_sampling_cb;

// Vita reports stick axes in [0,255], with 0 at left/up and 255 at
// right/down. GameCube PAD uses signed axes with positive Y meaning up.
// Use 127 for the inverted axis: 128 - 0 would overflow s8 to -128 exactly
// at full-up, turning maximum up into maximum down in gameplay.
static s8 vita_axis_x(u8 value)
{
    return (s8)((int)value - 128);
}

static s8 vita_axis_y(u8 value)
{
    return (s8)(127 - (int)value);
}

BOOL PADInit(void)
{
#if defined(STRIKERS_VITA)
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
#endif
    return TRUE;
}

int PADReset(u32 mask)
{
    (void)mask;
    return 1;
}

u32 PADRead(PADStatus* status)
{
    if (status == NULL)
        return 0;
    memset(status, 0, sizeof(PADStatus) * PAD_MAX_CONTROLLERS);

    status[0].err = PAD_ERR_NONE;
    for (int i = 1; i < PAD_MAX_CONTROLLERS; i++)
        status[i].err = PAD_ERR_NO_CONTROLLER;

#if defined(STRIKERS_VITA)
    SceCtrlData pad;
    memset(&pad, 0, sizeof pad);
    if (sceCtrlPeekBufferPositive(0, &pad, 1) > 0)
    {
        PortOverlayVitaInput(pad.buttons);
        u16 b = 0;
        if (pad.buttons & SCE_CTRL_LEFT) b |= PAD_BUTTON_LEFT;
        if (pad.buttons & SCE_CTRL_RIGHT) b |= PAD_BUTTON_RIGHT;
        if (pad.buttons & SCE_CTRL_DOWN) b |= PAD_BUTTON_DOWN;
        if (pad.buttons & SCE_CTRL_UP) b |= PAD_BUTTON_UP;
        if (pad.buttons & SCE_CTRL_CROSS) b |= PAD_BUTTON_A;
        if (pad.buttons & SCE_CTRL_CIRCLE) b |= PAD_BUTTON_B;
        if (pad.buttons & SCE_CTRL_SQUARE) b |= PAD_BUTTON_X;
        if (pad.buttons & SCE_CTRL_TRIANGLE) b |= PAD_BUTTON_Y;
        if (pad.buttons & SCE_CTRL_LTRIGGER) b |= PAD_TRIGGER_L;
        if (pad.buttons & SCE_CTRL_RTRIGGER) b |= PAD_TRIGGER_R;
        if (pad.buttons & SCE_CTRL_SELECT) b |= PAD_TRIGGER_Z;
        if (pad.buttons & SCE_CTRL_START) b |= PAD_BUTTON_START;

        status[0].button = b;
        status[0].stickX = vita_axis_x(pad.lx);
        status[0].stickY = vita_axis_y(pad.ly);
        status[0].substickX = vita_axis_x(pad.rx);
        status[0].substickY = vita_axis_y(pad.ry);
        status[0].triggerLeft = (pad.buttons & SCE_CTRL_LTRIGGER) ? 255 : 0;
        status[0].triggerRight = (pad.buttons & SCE_CTRL_RTRIGGER) ? 255 : 0;
        status[0].analogA = (pad.buttons & SCE_CTRL_CROSS) ? 255 : 0;
        status[0].analogB = (pad.buttons & SCE_CTRL_CIRCLE) ? 255 : 0;

        // The Vita quick menu polls the raw controller itself. Keep its D-pad
        // and face-button navigation from leaking into the game while open.
        if (PortOverlayMenuOpen())
        {
            status[0].button = 0;
            status[0].stickX = 0;
            status[0].stickY = 0;
            status[0].substickX = 0;
            status[0].substickY = 0;
            status[0].triggerLeft = 0;
            status[0].triggerRight = 0;
            status[0].analogA = 0;
            status[0].analogB = 0;
        }
    }
#endif

    // Bitmask of ports whose read failed.
    return 0;
}

void PADClampCircle(PADStatus* status)
{
    if (status == NULL)
        return;

    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++)
    {
        if (status[i].err != PAD_ERR_NONE)
            continue;

        s8* axes[4] = {
            &status[i].stickX, &status[i].stickY,
            &status[i].substickX, &status[i].substickY
        };
        const int radii[2] = { 56, 44 };
        const int deadZones[2] = { 15, 15 };

        for (int stick = 0; stick < 2; stick++)
        {
            int x = *axes[stick * 2];
            int y = *axes[stick * 2 + 1];
            const int dead = deadZones[stick];
            const int radius = radii[stick];

            if (-dead < x && x < dead)
                x = 0;
            else if (x > 0)
                x -= dead;
            else
                x += dead;

            if (-dead < y && y < dead)
                y = 0;
            else if (y > 0)
                y -= dead;
            else
                y += dead;

            const int squared = x * x + y * y;
            if (squared > radius * radius)
            {
                const int length = (int)sqrtf((float)squared);
                if (length > 0)
                {
                    x = x * radius / length;
                    y = y * radius / length;
                }
            }

            *axes[stick * 2] = (s8)x;
            *axes[stick * 2 + 1] = (s8)y;
        }

        // Match Dolphin PADClampCircle semantics: a digital Vita trigger maps
        // to exactly 1.0 in the game's later /150.0f normalization.
        status[i].triggerLeft =
            status[i].triggerLeft <= 30 ? 0 :
            (u8)(((status[i].triggerLeft > 180 ? 180 : status[i].triggerLeft) - 30));
        status[i].triggerRight =
            status[i].triggerRight <= 30 ? 0 :
            (u8)(((status[i].triggerRight > 180 ? 180 : status[i].triggerRight) - 30));
    }
}

#if defined(PORT_VITA) && !defined(STRIKERS_VITA_NATIVE_RUNTIME)
void PADControlMotor(u32 chan, u32 command)
#else
void PADControlMotor(s32 chan, u32 command)
#endif
{
    (void)chan;
    (void)command;
}

PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback)
{
    PADSamplingCallback prev = s_sampling_cb;
    s_sampling_cb = callback;
    return prev;
}

void PortInvokePadSamplingCallback(void)
{
    if (s_sampling_cb)
        s_sampling_cb();
}
