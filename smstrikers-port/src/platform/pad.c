// Native pad for Vita; neutral placeholder on other non-Aurora hosts.

#include <string.h>
#include <math.h>

#include "dolphin/pad.h"
#include "port/overlay.h"
#include "port/native_pad.h"

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

#if defined(STRIKERS_VITA)
enum { ENDPOINT_NONE, ENDPOINT_LEGACY, ENDPOINT_BUILTIN, ENDPOINT_EXTERNAL };
typedef struct PadEndpoint
{
    u8 kind, port, type;
} PadEndpoint;

typedef struct PadSlot
{
    PadEndpoint published;
    u32 generation, motorGeneration;
    u8 disconnectPending, motorAllowed, motorUnsupported, motorPort;
} PadSlot;

static PadSlot s_slots[PAD_MAX_CONTROLLERS];
static u8 s_motorActive[5], s_motorStopPending[5];
static int s_samplingReady, s_samplingExtReady, s_multiSupported = -1;

static int endpoint_equal(PadEndpoint a, PadEndpoint b)
{
    return a.kind == b.kind && a.port == b.port && a.type == b.type;
}

static int ensure_sampling(void)
{
    if (!s_samplingReady)
    {
        if (sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG) < 0)
            return 0;
        s_samplingReady = 1;
    }
    if (s_multiSupported < 0)
    {
        const int supported = sceCtrlIsMultiControllerSupported();
        if (supported != 0 && supported != 1)
            return 0;
        s_multiSupported = supported;
    }
    if (s_multiSupported && !s_samplingExtReady)
    {
        if (sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG) < 0)
            return 0;
        s_samplingExtReady = 1;
    }
    return 1;
}

static void queue_motor_stop(int slot)
{
    const unsigned port = s_slots[slot].motorPort;
    if (port > 0 && port < 5)
        s_motorStopPending[port] = 1;
}

static int retry_motor_stops(u32 ports)
{
    int firstError = 0;
    for (int port = 1; port < 5; ++port)
    {
        if (!(ports & ((u32)1u << port)) || !s_motorStopPending[port])
            continue;
        SceCtrlActuator stop;
        memset(&stop, 0, sizeof stop);
        const int result = sceCtrlSetActuator(port, &stop);
        if (result < 0 && (u32)result != (u32)SCE_CTRL_ERROR_NO_DEVICE)
        {
            if (!firstError)
                firstError = result;
            continue;
        }
        s_motorActive[port] = s_motorStopPending[port] = 0;
        for (int i = 0; i < PAD_MAX_CONTROLLERS; ++i)
        {
            PadSlot* slot = &s_slots[i];
            if (slot->motorPort != port)
                continue;
            // A late stop result belongs to its original attach generation.
            // It must not disconnect a new source which reused this port.
            if ((u32)result == (u32)SCE_CTRL_ERROR_NO_DEVICE &&
                slot->motorGeneration == slot->generation &&
                slot->published.kind == ENDPOINT_EXTERNAL &&
                slot->published.port == port)
            {
                slot->disconnectPending = 1;
                slot->motorAllowed = 0;
            }
            slot->motorPort = 0;
        }
    }
    return firstError;
}

static void transfer_failure(PADStatus* status)
{
    status[0].err = PAD_ERR_TRANSFER;
    for (int i = 1; i < PAD_MAX_CONTROLLERS; ++i)
        if (s_slots[i].published.kind != ENDPOINT_NONE)
            status[i].err = PAD_ERR_TRANSFER;
    (void)retry_motor_stops(0x1eu);
}

static unsigned legacy_buttons(unsigned buttons, int extended)
{
    if (extended)
    {
        // Positive2 puts the original shoulders on L1/R1. L2/R2 reuse the
        // legacy trigger bits and must not masquerade as those shoulders.
        buttons &= ~(SCE_CTRL_L2 | SCE_CTRL_R2);
        if (buttons & SCE_CTRL_L1) buttons |= SCE_CTRL_LTRIGGER;
        if (buttons & SCE_CTRL_R1) buttons |= SCE_CTRL_RTRIGGER;
    }
    return buttons;
}

static void decode_pad(PADStatus* status, const SceCtrlData* pad, unsigned buttons)
{
    u16 b = 0;
    if (buttons & SCE_CTRL_LEFT) b |= PAD_BUTTON_LEFT;
    if (buttons & SCE_CTRL_RIGHT) b |= PAD_BUTTON_RIGHT;
    if (buttons & SCE_CTRL_DOWN) b |= PAD_BUTTON_DOWN;
    if (buttons & SCE_CTRL_UP) b |= PAD_BUTTON_UP;
    if (buttons & SCE_CTRL_CROSS) b |= PAD_BUTTON_A;
    if (buttons & SCE_CTRL_CIRCLE) b |= PAD_BUTTON_B;
    if (buttons & SCE_CTRL_SQUARE) b |= PAD_BUTTON_X;
    if (buttons & SCE_CTRL_TRIANGLE) b |= PAD_BUTTON_Y;
    if (buttons & SCE_CTRL_LTRIGGER) b |= PAD_TRIGGER_L;
    if (buttons & SCE_CTRL_RTRIGGER) b |= PAD_TRIGGER_R;
    if (buttons & SCE_CTRL_SELECT) b |= PAD_TRIGGER_Z;
    if (buttons & SCE_CTRL_START) b |= PAD_BUTTON_START;
    status->err = PAD_ERR_NONE;
    status->button = b;
    status->stickX = vita_axis_x(pad->lx);
    status->stickY = vita_axis_y(pad->ly);
    status->substickX = vita_axis_x(pad->rx);
    status->substickY = vita_axis_y(pad->ry);
    status->triggerLeft = (buttons & SCE_CTRL_LTRIGGER) ? 255 : 0;
    status->triggerRight = (buttons & SCE_CTRL_RTRIGGER) ? 255 : 0;
    status->analogA = (buttons & SCE_CTRL_CROSS) ? 255 : 0;
    status->analogB = (buttons & SCE_CTRL_CIRCLE) ? 255 : 0;
}
#endif

BOOL PADInit(void)
{
#if defined(STRIKERS_VITA)
    return ensure_sampling() ? TRUE : FALSE;
#endif
    return TRUE;
}

int PADReset(u32 mask)
{
#if defined(STRIKERS_VITA)
    u32 ports = 0;
    for (int i = 0; i < PAD_MAX_CONTROLLERS; ++i)
        if (mask & ((u32)0x80000000u >> i))
        {
            queue_motor_stop(i);
            if (s_slots[i].motorPort)
                ports |= (u32)1u << s_slots[i].motorPort;
        }
    return retry_motor_stops(ports) == 0;
#else
    (void)mask;
    return 1;
#endif
}

u32 PADRead(PADStatus* status)
{
    if (status == NULL)
        return 0;
    memset(status, 0, sizeof(PADStatus) * PAD_MAX_CONTROLLERS);

    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++)
        status[i].err = PAD_ERR_NO_CONTROLLER;

#if defined(STRIKERS_VITA)
    PadEndpoint candidate[PAD_MAX_CONTROLLERS];
    memset(candidate, 0, sizeof candidate);
    for (int i = 0; i < PAD_MAX_CONTROLLERS; ++i)
        s_slots[i].motorAllowed = 0;
    if (!ensure_sampling())
    {
        transfer_failure(status);
        return 0;
    }
    if (!s_multiSupported)
    {
        candidate[0].kind = ENDPOINT_LEGACY;
    }
    else
    {
        SceCtrlPortInfo info;
        memset(&info, 0, sizeof info);
        if (sceCtrlGetControllerPortInfo(&info) < 0)
        {
            transfer_failure(status);
            return 0;
        }
        if (info.port[0] != SCE_CTRL_TYPE_PHY && info.port[0] != SCE_CTRL_TYPE_VIRT &&
            info.port[0] != SCE_CTRL_TYPE_UNPAIRED)
        {
            transfer_failure(status);
            return 0;
        }
        const int builtin = info.port[0] == SCE_CTRL_TYPE_PHY;
        if (builtin)
        {
            candidate[0].kind = ENDPOINT_BUILTIN;
            candidate[0].type = SCE_CTRL_TYPE_PHY;
        }
        // Port 0 may alias a physical controller on PSTV. Never add VIRT as
        // another player. The public port-info array has only indices 0..4.
        for (int i = builtin ? 1 : 0; i < PAD_MAX_CONTROLLERS; ++i)
        {
            const int port = builtin ? i : i + 1;
            const u8 type = info.port[port];
            if (type == SCE_CTRL_TYPE_DS3 || type == SCE_CTRL_TYPE_DS4)
            {
                candidate[i].kind = ENDPOINT_EXTERNAL;
                candidate[i].port = (u8)port;
                candidate[i].type = type;
            }
        }
    }

    unsigned primaryButtons = 0;
    for (int i = 0; i < PAD_MAX_CONTROLLERS; ++i)
    {
        PadSlot* slot = &s_slots[i];
        if (slot->published.kind != ENDPOINT_NONE &&
            !endpoint_equal(slot->published, candidate[i]))
            slot->disconnectPending = 1;
        if (slot->disconnectPending)
        {
            queue_motor_stop(i);
            continue;
        }
        if (candidate[i].kind == ENDPOINT_NONE)
            continue;
        SceCtrlData pad;
        memset(&pad, 0, sizeof pad);
        const int result = s_multiSupported
            ? sceCtrlPeekBufferPositive2(candidate[i].port, &pad, 1)
            : sceCtrlPeekBufferPositive(0, &pad, 1);
        if (result > 0)
        {
            const unsigned buttons = legacy_buttons(pad.buttons, s_multiSupported);
            decode_pad(&status[i], &pad, buttons);
            if (i == 0)
                primaryButtons = buttons;
        }
        else if ((u32)result == (u32)SCE_CTRL_ERROR_NO_DEVICE)
        {
            if (slot->published.kind != ENDPOINT_NONE)
                slot->disconnectPending = 1;
            queue_motor_stop(i);
        }
        else
            status[i].err = PAD_ERR_TRANSFER;
    }
    (void)retry_motor_stops(0x1eu);
    int publish = 1;
    for (int i = 0; i < PAD_MAX_CONTROLLERS; ++i)
    {
        if (s_slots[i].disconnectPending)
        {
            memset(&status[i], 0, sizeof status[i]);
            status[i].err = PAD_ERR_NO_CONTROLLER;
        }
        if (status[i].err == PAD_ERR_TRANSFER)
            publish = 0;
    }
    // VBlankPadUpdate rejects all four pads on any transfer error. Do not
    // consume a disconnect, attach generation or overlay edge for that batch.
    if (!publish)
        return 0;

    u32 motors = 0;
    for (int i = 0; i < PAD_MAX_CONTROLLERS; ++i)
    {
        PadSlot* slot = &s_slots[i];
        if (slot->disconnectPending)
        {
            memset(&slot->published, 0, sizeof slot->published);
            ++slot->generation;
            slot->disconnectPending = slot->motorUnsupported = 0;
        }
        else if (status[i].err == PAD_ERR_NONE)
        {
            if (!endpoint_equal(slot->published, candidate[i]))
            {
                slot->published = candidate[i];
                ++slot->generation;
                slot->motorUnsupported = 0;
            }
            if (slot->published.kind == ENDPOINT_EXTERNAL && !slot->motorUnsupported)
            {
                motors |= (u32)0x80000000u >> i;
                slot->motorAllowed = !s_motorStopPending[slot->published.port];
            }
        }
    }
    if (status[0].err == PAD_ERR_NONE)
        PortOverlayVitaInput(primaryButtons);
    if (PortOverlayMenuOpen())
    {
        for (int i = 0; i < PAD_MAX_CONTROLLERS; ++i)
            if (status[i].err == PAD_ERR_NONE)
                memset(&status[i], 0, sizeof status[i]);
    }
    return motors;
#else
    status[0].err = PAD_ERR_NONE;
    return 0;
#endif
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

void PADControlMotor(s32 chan, u32 command)
{
#if defined(STRIKERS_VITA)
    if (chan < 0 || chan >= PAD_MAX_CONTROLLERS || command > PAD_MOTOR_STOP_HARD)
        return;
    PadSlot* slot = &s_slots[chan];
    if (command != PAD_MOTOR_RUMBLE)
    {
        const u32 ports = slot->motorPort ? (u32)1u << slot->motorPort : 0;
        queue_motor_stop(chan);
        (void)retry_motor_stops(ports);
        return;
    }
    if (!slot->motorAllowed || slot->disconnectPending || slot->motorUnsupported ||
        slot->published.kind != ENDPOINT_EXTERNAL)
        return;
    const unsigned port = slot->published.port;
    if (port < 1 || port >= 5 || s_motorStopPending[port])
        return;
    if (slot->motorPort == port && s_motorActive[port] &&
        slot->motorGeneration == slot->generation)
        return;
    if (slot->motorPort)
    {
        const u32 ports = (u32)1u << slot->motorPort;
        queue_motor_stop(chan);
        if (retry_motor_stops(ports) < 0 || slot->motorPort)
            return;
    }
    // A prior slot may still own this physical port after a topology change.
    if (s_motorActive[port])
        return;
    SceCtrlActuator actuator;
    memset(&actuator, 0, sizeof actuator);
    actuator.small = actuator.large = 255;
    const int result = sceCtrlSetActuator(port, &actuator);
    if (result == 0)
    {
        slot->motorPort = (u8)port;
        slot->motorGeneration = slot->generation;
        s_motorActive[port] = 1;
    }
    else if ((u32)result == (u32)SCE_CTRL_ERROR_NOT_SUPPORTED)
        slot->motorUnsupported = 1;
    else if ((u32)result == (u32)SCE_CTRL_ERROR_NO_DEVICE)
    {
        slot->disconnectPending = 1;
        slot->motorAllowed = 0;
    }
#else
    (void)chan;
    (void)command;
#endif
}

int PortPadShutdown(void)
{
#if defined(STRIKERS_VITA)
    for (int i = 0; i < PAD_MAX_CONTROLLERS; ++i)
    {
        queue_motor_stop(i);
        s_slots[i].motorAllowed = 0;
        s_slots[i].disconnectPending = s_slots[i].motorUnsupported = 0;
        memset(&s_slots[i].published, 0, sizeof s_slots[i].published);
        ++s_slots[i].generation;
    }
    s_samplingReady = s_samplingExtReady = 0;
    s_multiSupported = -1;
    return retry_motor_stops(0x1eu);
#else
    return 0;
#endif
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
