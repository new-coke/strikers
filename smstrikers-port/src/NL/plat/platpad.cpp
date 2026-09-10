#include "NL/platpad.h"
#include "port/overlay.h"
#include <stdlib.h>
#include "dolphin/os.h"

#include "math.h"
#include "NL/nlMemory.h"
#include "NL/gl/glPlat.h"
#include "NL/nlTicker.h"

PADStatus PadStatus::s_A[4];
PADStatus PadStatus::s_B[4];
PADStatus* PadStatus::s_Current = PadStatus::s_A;
PADStatus* PadStatus::s_Next = PadStatus::s_B;

u32 g_nPadMasks[PAD_MAX_CONTROLLERS] = { 0x80000000, 0x40000000, 0x20000000, 0x10000000 };

bool cPlatPad::m_bDisableRumble = false;

namespace
{
PadStatus padCategories[2];
PadStatus* padStatus = &padCategories[0];

// PORT: delta is real elapsed time; the console added a sixtieth per retrace.
static inline void UpdateButtonStateTime(PadStatus* pPadStatus, int padIdx, float delta)
{
    int button = 1;

    for (int buttonIndex = 0; buttonIndex < 12; buttonIndex++)
    {
        if (PadStatus::s_Current[padIdx].button & button)
            pPadStatus->m_GameCubePads[padIdx].fButtonStateTime[buttonIndex] += delta;
        else
        {
            pPadStatus->m_GameCubePads[padIdx].fButtonStateTime[buttonIndex] = 0.0f;
            pPadStatus->m_GameCubePads[padIdx].fButtonTimeSinceLastRepeat[buttonIndex] = 0.0f;
        }
        button <<= 1;
    }
}
} // namespace

/**
 * Offset/Address/Size: 0x0 | 0x801C2FB0 | size: 0x5C
 */
cPlatPad::~cPlatPad()
{
}

/**
 * Offset/Address/Size: 0x5C | 0x801C300C | size: 0x524
 */
void VBlankPadUpdate()
{
    PADRead(PadStatus::s_Next);
    PADClampCircle(PadStatus::s_Next);

    if (PortOverlayMenuOpen())
    {
        PortDebugSetPad(0, (int)PadStatus::s_Next[0].err, (unsigned int)PadStatus::s_Next[0].button,
                        (int)PadStatus::s_Next[0].stickX, (int)PadStatus::s_Next[0].stickY,
                        (int)PadStatus::s_Next[0].substickX, (int)PadStatus::s_Next[0].substickY,
                        (int)PadStatus::s_Next[0].triggerLeft, (int)PadStatus::s_Next[0].triggerRight);
    }

    if (getenv("STRIKERS_PROBE_PAD") != NULL)
    {
        // Only on change, or this prints sixty times a second.
        static s8 lastErr = 127;
        static u16 lastButtons = 0xFFFF;
        if (PadStatus::s_Next[0].err != lastErr
            || PadStatus::s_Next[0].button != lastButtons)
        {
            // PORT: the longest hold among the buttons just released, read before UpdateButtonStateTime zeroes it.
            float held = 0.0f;
            {
                const u16 released = lastButtons & ~PadStatus::s_Next[0].button;
                for (int b = 0; b < 12; b++)
                    if ((released & (1 << b)) && lastButtons != 0xFFFF)
                    {
                        const float h = padCategories[0].m_GameCubePads[0].fButtonStateTime[b];
                        if (h > held)
                            held = h;
                    }
            }
            lastErr = PadStatus::s_Next[0].err;
            lastButtons = PadStatus::s_Next[0].button;
            OSReport("pad0: err %d buttons 0x%04x stick %d,%d held %.2fs\n",
                     (int)PadStatus::s_Next[0].err,
                     (unsigned)PadStatus::s_Next[0].button,
                     (int)PadStatus::s_Next[0].stickX,
                     (int)PadStatus::s_Next[0].stickY, (double)held);
        }
    }

    // PORT: how long since the last sample, for the hold timers below.
    float buttonDelta;
    {
        static u32 s_prevTicker;
        static bool s_havePrev;
        const u32 now = nlGetTicker();
        buttonDelta = s_havePrev ? nlGetTickerDifference(s_prevTicker, now) / 1000.0f : 0.0f;
        s_prevTicker = now;
        s_havePrev = true;
        if (buttonDelta > 0.1f)
            buttonDelta = 0.1f;
    }

    int i;
    for (i = 0; i < 4; i++)
    {
        if (PadStatus::s_Next[i].err == -3)
            break;
    }

    if (i != 4)
        return;

    PADStatus* temp = PadStatus::s_Next;
    PadStatus::s_Next = PadStatus::s_Current;
    PadStatus::s_Current = temp;

    for (i = 0; i < 4; i++)
    {
        cGlobalPad* pad = cPadManager::GetPad(i);

        if (pad->m_isLeftAnalogToDPadMapEnabled)
        {
            float normalizedX = (float)PadStatus::s_Current[i].stickX / 56.0f;
            float normalizedY = (float)PadStatus::s_Current[i].stickY / 56.0f;

            if (fabsf(normalizedX) >= 0.6f || fabsf(normalizedY) >= 0.6f)
            {
                if (fabsf(normalizedX) >= 0.6f)
                {
                    goto doneNormalizedX;
                }
                else
                {
                    normalizedX = 0.0f;
                }
            doneNormalizedX:
                normalizedY = (fabsf(normalizedY) >= 0.6f) ? normalizedY : 0.0f;

                float angle = nlATan2f(normalizedY, normalizedX);
                u16 angleU16 = (u16)(int)(angle * 10430.378f);
                float degrees = (float)angleU16 * 0.005493164f;
                int roundedDeg = (int)degrees;
                roundedDeg = (roundedDeg / 45) * 45;

                switch (roundedDeg)
                {
                case 0:
                    PadStatus::s_Current[i].button |= PAD_BUTTON_RIGHT;
                    break;
                case 45:
                    PadStatus::s_Current[i].button |= (PAD_BUTTON_UP | PAD_BUTTON_RIGHT);
                    break;
                case 90:
                    PadStatus::s_Current[i].button |= PAD_BUTTON_UP;
                    break;
                case 135:
                    PadStatus::s_Current[i].button |= (PAD_BUTTON_UP | PAD_BUTTON_LEFT);
                    break;
                case 180:
                    PadStatus::s_Current[i].button |= PAD_BUTTON_LEFT;
                    break;
                case 225:
                    PadStatus::s_Current[i].button |= (PAD_BUTTON_DOWN | PAD_BUTTON_LEFT);
                    break;
                case 270:
                    PadStatus::s_Current[i].button |= PAD_BUTTON_DOWN;
                    break;
                case 315:
                    PadStatus::s_Current[i].button |= (PAD_BUTTON_DOWN | PAD_BUTTON_RIGHT);
                    break;
                }
            }
        }

        UpdateButtonStateTime(&padCategories[0], i, buttonDelta);
        UpdateButtonStateTime(&padCategories[1], i, buttonDelta);
    }
}

/**
 * Offset/Address/Size: 0x580 | 0x801C3530 | size: 0x40
 */
void cPlatPad::StopRumble()
{
    padStatus[0].m_GameCubePads[m_padIndex].bRumbleActive = 0;
    PADControlMotor(m_padIndex, 2);
}

/**
 * Offset/Address/Size: 0x5C0 | 0x801C3570 | size: 0x78
 */
void cPlatPad::StartRumble(float fDuration, float fIntensity, float fFrequency)
{
    if (m_bDisableRumble == 0)
    {
        PADControlMotor(m_padIndex, 1);
        padStatus[0].m_GameCubePads[m_padIndex].fRumbleTimer = fDuration;
        padStatus[0].m_GameCubePads[m_padIndex].bRumbleActive = 1;
    }
}

/**
 * Offset/Address/Size: 0x638 | 0x801C35E8 | size: 0x18
 */
bool cPlatPad::RumbleActive()
{
    return padStatus[0].m_GameCubePads[m_padIndex].bRumbleActive;
}

/**
 * Offset/Address/Size: 0x650 | 0x801C3600 | size: 0x18
 */
f32 cPlatPad::AnalogRightY()
{
    return padStatus[0].m_GameCubePads[m_padIndex].fAnalogRightY;
}

/**
 * Offset/Address/Size: 0x668 | 0x801C3618 | size: 0x18
 */
f32 cPlatPad::AnalogRightX()
{
    return padStatus[0].m_GameCubePads[m_padIndex].fAnalogRightX;
}

/**
 * Offset/Address/Size: 0x680 | 0x801C3630 | size: 0x18
 */
f32 cPlatPad::AnalogLeftY()
{
    return padStatus[0].m_GameCubePads[m_padIndex].fAnalogLeftY;
}

/**
 * Offset/Address/Size: 0x698 | 0x801C3648 | size: 0x14
 */
f32 cPlatPad::AnalogLeftX()
{
    return padStatus[0].m_GameCubePads[m_padIndex].fAnalogLeftX;
}

/**
 * Offset/Address/Size: 0x6AC | 0x801C365C | size: 0x8
 */
f32 cPlatPad::GetPressureDerivative(int button, bool remap)
{
    return 0.f;
}

/**
 * Offset/Address/Size: 0x6B4 | 0x801C3664 | size: 0x84
 */
f32 cPlatPad::GetPressure(int button, bool remap)
{
    if (remap != 0)
    {
        button = cPadManager::m_pRemapArray[button];
    }

    switch (button)
    {
    case 0x40:
        return padStatus[0].m_GameCubePads[m_padIndex].fTriggerLeft;
    case 0x20:
        return padStatus[0].m_GameCubePads[m_padIndex].fTriggerRight;
    default:
        if ((PadStatus::s_Current[m_padIndex].button & button) != 0)
        {
            return 1.f;
        }
        return 0.f;
    }
}

/**
 * Offset/Address/Size: 0x738 | 0x801C36E8 | size: 0x3C
 */
f32 cPlatPad::GetButtonStateTime(int button, bool remap)
{
    if (remap)
    {
        button = cPadManager::m_pRemapArray[button];
    }

    return padStatus[0].m_GameCubePads[m_padIndex].fButtonStateTime[0x1F - __cntlzw(button)];
}

/**
 * Offset/Address/Size: 0x774 | 0x801C3724 | size: 0x3C
 */
bool cPlatPad::PlatJustReleased(int button, bool remap)
{
    if (remap)
    {
        button = cPadManager::m_pRemapArray[button];
    }

    s32 buttonReleased = padStatus[0].m_justReleased[m_padIndex] & button;
    return (buttonReleased != 0) ? true : false;
}

/**
 * Offset/Address/Size: 0x7B0 | 0x801C3760 | size: 0x3C
 */
bool cPlatPad::PlatJustPressed(int button, bool remap)
{
    if (remap)
    {
        button = cPadManager::m_pRemapArray[button];
    }

    s32 buttonPressed = padStatus[0].m_justPressed[m_padIndex] & button;
    return (buttonPressed != 0) ? true : false;
}

/**
 * Offset/Address/Size: 0x7EC | 0x801C379C | size: 0x38
 */
bool cPlatPad::IsPressed(int button, bool remap)
{
    if (remap != 0)
    {
        button = cPadManager::m_pRemapArray[button];
    }

    s32 buttonPressed = PadStatus::s_Current[m_padIndex].button & button;
    return (buttonPressed != 0) ? true : false;
}

/**
 * Offset/Address/Size: 0x824 | 0x801C37D4 | size: 0x34
 */
bool cPlatPad::IsConnected()
{
    bool isConnected = false;

    s8 err = PadStatus::s_Current[m_padIndex].err;
    if ((err == 0) || (err == -3))
    {
        isConnected = true;
    }

    return isConnected;
}

/**
 * Offset/Address/Size: 0x858 | 0x801C3808 | size: 0x270
 */
void PadStatus::Update(float dt)
{
    u32 resetMask = 0;

    for (s32 i = 0; i < 4; ++i)
    {
        s8 err = PadStatus::s_Current[i].err;

        if (err == 0)
        {
            tGameCubePad& pad = m_GameCubePads[i];
            pad.fAnalogLeftX = (f32)PadStatus::s_Current[i].stickX / 56.0f;
            pad.fAnalogLeftY = (f32)PadStatus::s_Current[i].stickY / 56.0f;
            pad.fAnalogRightX = (f32)PadStatus::s_Current[i].substickX / 44.0f;
            pad.fAnalogRightY = (f32)PadStatus::s_Current[i].substickY / 44.0f;
            pad.fTriggerLeft = (f32)(s32)PadStatus::s_Current[i].triggerLeft / 150.0f;
            pad.fTriggerRight = (f32)(s32)PadStatus::s_Current[i].triggerRight / 150.0f;

            u16 button = PadStatus::s_Current[i].button;
            m_justPressed[i] = button & ~m_previousButtons[i];
            m_justReleased[i] = m_previousButtons[i] & ~PadStatus::s_Current[i].button;
            m_previousButtons[i] = PadStatus::s_Current[i].button;
            m_previousErr[i] = PadStatus::s_Current[i].err;
        }
        else if (err == -1)
        {
            if (m_previousErr[i] == 0)
            {
                memset(&m_GameCubePads[i], 0, sizeof(tGameCubePad));
            }

            u32 mask = g_nPadMasks[i];
            int zero = 0;
            m_justPressed[i] = zero;
            resetMask |= mask;
            m_justReleased[i] = zero;
            m_previousButtons[i] = zero;
        }

        if (m_GameCubePads[i].bRumbleActive)
        {
            m_GameCubePads[i].fRumbleTimer -= dt;
            if (m_GameCubePads[i].fRumbleTimer < 0.0f)
            {
                m_GameCubePads[i].bRumbleActive = 0;
                PADControlMotor(i, 0);
            }
        }
    }

    if (resetMask)
    {
        PADReset(resetMask);
    }
}

/**
 * Offset/Address/Size: 0xAC8 | 0x801C3A78 | size: 0x24
 */
void UpdatePlatPad(float dt)
{
    ((PadStatus*)padStatus)->Update(dt);
}

static inline void InitPadStatus(PadStatus* pad)
{
    for (int i = 0; i < 4; ++i)
    {
        pad->m_justPressed[i] = 0;
        pad->m_justReleased[i] = 0;
        pad->m_previousButtons[i] = 0;
        pad->m_previousErr[i] = 0;
        memset(&pad->m_GameCubePads[i], 0, sizeof(tGameCubePad));
    }
}

/**
 * Offset/Address/Size: 0xAEC | 0x801C3A9C | size: 0xB0
 */
void InitPlatPad()
{
    PADRead(PadStatus::s_Current);
    memcpy(PadStatus::s_Next, PadStatus::s_Next, 4);

    for (int i = 0; i < 2; ++i)
    {
        InitPadStatus(&padStatus[i]);
    }
}

/**
 * Offset/Address/Size: 0xB9C | 0x801C3B4C | size: 0xC
 */
u32 GetButtonIndex(int button)
{
    return 0x1F - __cntlzw(button);
}

/**
 * Offset/Address/Size: 0xBA8 | 0x801C3B58 | size: 0x10
 */
void UseDefaultPad()
{
    padStatus = &padCategories[0];
}

/**
 * Offset/Address/Size: 0xBB8 | 0x801C3B68 | size: 0x14
 */
void UseFixedUpdatePad()
{
    padStatus = &padCategories[1];
}
