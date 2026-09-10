#ifndef _PLATPAD_H_
#define _PLATPAD_H_

#include "types.h"
#include "NL/globalpad.h"

void VBlankPadUpdate();
void UpdatePlatPad(float dt);
void InitPlatPad();
u32 GetButtonIndex(int button);
void UseDefaultPad();
void UseFixedUpdatePad();

class cPlatPad : public cGlobalPad
{
public:
    static bool m_bDisableRumble;

    cPlatPad(int padIndex)
        : cGlobalPad(padIndex) { };
    virtual ~cPlatPad();

    /* 0x0C */ virtual bool IsConnected();
    /* 0x10 */ virtual bool IsPressed(int button, bool remap);
    /* 0x14 */ virtual f32 GetPressure(int button, bool remap);
    /* 0x18 */ virtual f32 GetPressureDerivative(int button, bool remap);
    /* 0x1C */ virtual bool PlatJustPressed(int button, bool remap);
    /* 0x20 */ virtual bool PlatJustReleased(int button, bool remap);
    /* 0x24 */ virtual f32 GetButtonStateTime(int button, bool remap);
    /* 0x28 */ virtual f32 AnalogLeftX();
    /* 0x2C */ virtual f32 AnalogLeftY();
    /* 0x30 */ virtual f32 AnalogRightX();
    /* 0x34 */ virtual f32 AnalogRightY();
    /* 0x38 */ virtual bool RumbleActive();
    /* 0x3C */ virtual void StartRumble(float fDuration, float fIntensity, float fFrequency);
    /* 0x40 */ virtual void StopRumble();
};

struct tGameCubePad
{
    /* 0x0, */ f32 fAnalogLeftX;
    /* 0x4, */ f32 fAnalogLeftY;
    /* 0x8, */ f32 fAnalogRightX;
    /* 0xC, */ f32 fAnalogRightY;
    /* 0x10 */ f32 fTriggerLeft;
    /* 0x14 */ f32 fTriggerRight;
    /* 0x18 */ f32 fButtonStateTime[12];
    /* 0x48 */ f32 fButtonInitialDelay[12];
    /* 0x78 */ f32 fButtonRepeatRate[12];
    /* 0xA8 */ f32 fButtonTimeSinceLastRepeat[12];
    /* 0xD8 */ f32 fRumbleTimer;
    /* 0xDC */ bool bRumbleActive;
    /* 0xDD */ bool bMapAnalogToDPad;
}; // size 0xE0

class PadStatus
{
public:
    static PADStatus s_A[4];
    static PADStatus s_B[4];
    static PADStatus* s_Current;
    static PADStatus* s_Next;

    void Update(float dt);

    /* 0x000 */ struct tGameCubePad m_GameCubePads[4]; // size 0x380
    /* 0x380 */ u16 m_justPressed[4];                  // size 0x8
    /* 0x388 */ u16 m_justReleased[4];                 // size 0x8
    /* 0x390 */ u16 m_previousButtons[4];              // size 0x8
    /* 0x398 */ s8 m_previousErr[4];                   // size 0x4
};

#endif // _PLATPAD_H_
