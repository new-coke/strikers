// Controller input placeholder: port 0 reports a pad at neutral, ports 1-3 report none.

#include <string.h>

#include "dolphin/types.h"

#define PAD_CHANMAX 4
#define PAD_ERR_NONE 0
#define PAD_ERR_NO_CONTROLLER (-1)

typedef struct PADStatus
{
    u16 button;
    s8 stickX;
    s8 stickY;
    s8 substickX;
    s8 substickY;
    u8 triggerLeft;
    u8 triggerRight;
    u8 analogA;
    u8 analogB;
    s8 err;
} PADStatus;

typedef void (*PADSamplingCallback)(void);

static PADSamplingCallback s_sampling_cb;

BOOL PADInit(void)
{
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
    memset(status, 0, sizeof(PADStatus) * PAD_CHANMAX);

    status[0].err = PAD_ERR_NONE;
    for (int i = 1; i < PAD_CHANMAX; i++)
        status[i].err = PAD_ERR_NO_CONTROLLER;

    // Bitmask of ports whose read failed.
    return 0;
}

void PADClampCircle(PADStatus* status)
{
    // Nothing to clamp while every stick reads neutral.
    (void)status;
}

void PADControlMotor(s32 chan, u32 command)
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
