#ifndef _PADMONKEY_H_
#define _PADMONKEY_H_

#include "types.h"
#include "NL/globalpad.h"

class PadMonkey : public cGlobalPad
{
public:
    PadMonkey(int padIndex);
    /* 0x08 */ virtual ~PadMonkey() { };
    /* 0x0C */ virtual bool IsConnected() { return m_isConnected; };
    /* 0x10 */ virtual bool IsPressed(int button, bool remap);
    /* 0x14 */ virtual f32 GetPressure(int button, bool remap);
    /* 0x18 */ virtual f32 GetPressureDerivative(int, bool) { return 0.0f; };
    /* 0x1C */ virtual bool PlatJustPressed(int button, bool remap);
    /* 0x20 */ virtual bool PlatJustReleased(int button, bool remap);
    /* 0x24 */ virtual f32 GetButtonStateTime(int button, bool remap);
    /* 0x28 */ virtual f32 AnalogLeftX() { return m_unk_0x90; };
    /* 0x2C */ virtual f32 AnalogLeftY() { return m_unk_0x94; };
    /* 0x30 */ virtual f32 AnalogRightX() { return m_unk_0x98; };
    /* 0x34 */ virtual f32 AnalogRightY() { return m_unk_0x9C; };
    /* 0x38 */ virtual bool RumbleActive() { return false; };
    /* 0x3C */ virtual void StartRumble(float, float, float) { };
    /* 0x40 */ virtual void StopRumble() { };
    /* 0x44 */ virtual void Update(float dt);

    void SetButtonChance(int button, float pct);

    /* 0xA0 */ f32 m_unk_0xA0;
    /* 0xA4 */ f32 m_buttonChance[13];
    // /* 0xA0 */ f32 m_unk_0xA0;
    // /* 0xA4 */ f32 m_unk_0xA4;
    // /* 0xA8 */ f32 m_unk_0xA8;
    // /* 0xAC */ f32 m_unk_0xAC;
    // /* 0xB0 */ f32 m_unk_0xB0;
    // /* 0xB4 */ f32 m_unk_0xB4;
    // /* 0xB8 */ f32 m_unk_0xB8;
    // /* 0xBC */ f32 m_unk_0xBC;
    // /* 0xC0 */ f32 m_unk_0xC0;
    // /* 0xC4 */ f32 m_unk_0xC4;
    // /* 0xC8 */ f32 m_unk_0xC8;
    // /* 0xCC */ f32 m_unk_0xCC;
    // /* 0xD0 */ f32 m_unk_0xD0;
};

#endif // _PADMONKEY_H_
