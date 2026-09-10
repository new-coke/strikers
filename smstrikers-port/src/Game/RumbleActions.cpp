#include "Game/RumbleActions.h"

static struct RumbleActionState rumbleActionState[4];

RumbleOp opArrayShootToScoreContact[] = {
    { 0, 0x1F4 }, // { 0, 500 }
    { 2, 0 }      // { 2, 0 }
};

RumbleOp opArrayShootToScoreHyper[] = {
    { 0, 0xFA },  // { 0, 250 }
    { 1, 0xC8 },  // { 1, 200 }
    { 0, 0x1F4 }, // { 0, 500 }
    { 1, 0xC8 },  // { 1, 200 }
    { 0, 0x3E7 }, // { 0, 999 }
    { 2, 0 }      // { 2, 0 }
};

RumbleOp opArrayShotContact[] = {
    { 0, 0x14D }, // { 0, 333 }
    { 2, 0 }      // { 2, 0 }
};

RumbleOp opArraySolidContact[] = {
    { 0, 0x1F4 }, // { 0, 500 }
    { 2, 0 }      // { 2, 0 }
};

RumbleOp opArrayMediumContact[] = {
    { 0, 0xDE }, // { 0, 222 }
    { 2, 0 }     // { 2, 0 }
};

RumbleOp opArraySmallContact[] = {
    { 0, 0x46 }, // { 0, 70 }
    { 2, 0 }     // { 2, 0 }
};

/**
 * Offset/Address/Size: 0x134 | 0x801945C8 | size: 0x1D0
 */
void UpdateRumbleActions(float dt)
{
    for (int padIndex = 0; padIndex < 4; padIndex++)
    {
        RumbleActionState* state = &rumbleActionState[padIndex];
        if (state->active != 0)
        {
            cGlobalPad* pad = cPadManager::GetPad(padIndex);

            if (state->pending != 0)
            {
                state->timer -= dt;
                if (!(state->timer <= 0.0f))
                {
                    continue;
                }
                state->current += 1;
                state->pending = 0;
            }

            // Process the current operation
            int currentOp = state->current;
            RumbleOp* op = &state->ops[currentOp];

            switch (op->type)
            {
            case 0: // Set rumble intensity
                if (op->value != 0)
                {
                    state->timer = (float)op->value / 1000.0f;
                    state->pending = 1;
                    pad->StartRumble(state->timer, 0.0f, 0.0f);
                }
                else
                {
                    state->current++;
                }
                break;

            case 1: // Delay
            {
                pad->StopRumble();
                int nextOp = state->current;
                u32 delayValue = state->ops[nextOp].value;
                if (delayValue != 0)
                {
                    state->timer = (float)delayValue / 1000.0f;
                    state->pending = true;
                }
                else
                {
                    state->current++;
                }
                break;
            }

            case 2: // Stop rumble
                pad->StopRumble();
                state->active = 0;
                state->ops = nullptr;
                break;
            }
        }

        // padIndex += 1;
    }
}

/**
 * Offset/Address/Size: 0x64 | 0x801944F8 | size: 0xD0
 */
void BeginRumbleAction(eRumbleActionPreset preset, cGlobalPad* pad)
{
    RumbleOp* ops;
    s32 idx;

    if (pad != NULL)
    {
        idx = pad->m_padIndex;

        switch (preset)
        {
        case RUMBLE_SMALL_CONTACT:
            ops = opArraySmallContact;
            break;
        case RUMBLE_MEDIUM_CONTACT:
            ops = opArrayMediumContact;
            break;
        case RUMBLE_SOLID_CONTACT:
            ops = opArraySolidContact;
            break;
        case RUMBLE_SHOT_CONTACT:
            ops = opArrayShotContact;
            break;
        case RUMBLE_SHOOT_TO_SCORE:
            ops = opArrayShootToScoreContact;
            break;
        case RUMBLE_SHOOT_TO_SCORE_HYPER:
            ops = opArrayShootToScoreHyper;
            break;
        default:
            return;
        }

        rumbleActionState[idx].active = 1;
        rumbleActionState[idx].pending = 0;
        rumbleActionState[idx].current = 0;
        rumbleActionState[idx].ops = ops;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80194494 | size: 0x64
 */
void StopRumbleAction(cGlobalPad* pad)
{
    if (pad != NULL)
    {
        s32 idx = pad->m_padIndex;
        pad->StopRumble();

        rumbleActionState[idx].active = 0;
        rumbleActionState[idx].pending = 0;
        rumbleActionState[idx].ops = NULL;
        rumbleActionState[idx].current = 0;
    }
}
