#ifndef _GLOBALPAD_H_
#define _GLOBALPAD_H_

#include "types.h"
#include "dolphin/pad.h"
#include "NL/nlMath.h"

class cGlobalPad;

class cPadManager
{
public:
    static void Update(float deltaTime);

    static cGlobalPad* GetPad(int idx);
    // static s32* GetRemapArray(); // { return m_pRemapArray; };
    static s32* GetRemapArray()
    {
        return m_pRemapArray;
    }

    static s32* m_pRemapArray;
    static cGlobalPad* m_aPads[PAD_MAX_CONTROLLERS];
    static float m_DeltaT;
};

class cGlobalPad
{
public:
    cGlobalPad(int padIndex)
    {
        m_padIndex = padIndex;
        m_isLeftAnalogToDPadMapEnabled = false;
    }

    /* 0x08 */ virtual ~cGlobalPad() { };
    /* 0x0C */ virtual bool IsConnected() = 0;
    /* 0x10 */ virtual bool IsPressed(int, bool) = 0;
    /* 0x14 */ virtual f32 GetPressure(int, bool) = 0;
    /* 0x18 */ virtual f32 GetPressureDerivative(int, bool) = 0;
    /* 0x1C */ virtual bool PlatJustPressed(int, bool) = 0;
    /* 0x20 */ virtual bool PlatJustReleased(int, bool) = 0;
    /* 0x24 */ virtual f32 GetButtonStateTime(int, bool) = 0;
    /* 0x28 */ virtual f32 AnalogLeftX() = 0;
    /* 0x2C */ virtual f32 AnalogLeftY() = 0;
    /* 0x30 */ virtual f32 AnalogRightX() = 0;
    /* 0x34 */ virtual f32 AnalogRightY() = 0;
    /* 0x38 */ virtual bool RumbleActive() = 0;
    /* 0x3C */ virtual void StartRumble(float, float, float) = 0;
    /* 0x40 */ virtual void StopRumble() = 0;
    /* 0x44 */ virtual void Update(float deltaTime);

    bool JustReleased(int button, bool remap);
    bool JustPressed(int button, bool remap);

    /**
     * Offset/Address/Size: 0x0 | 0x8020FB00 | size: 0xC
     */
    void DisableLeftAnalogToDPadMap()
    {
        m_isLeftAnalogToDPadMapEnabled = false;
    }

    /**
     * Offset/Address/Size: 0xC | 0x8020FB0C | size: 0xC
     */
    void EnableLeftAnalogToDPadMap()
    {
        m_isLeftAnalogToDPadMapEnabled = true;
    }

    /* 0x04 */ s32 m_padIndex;
    /* 0x08 */ nlPolar m_polarAnalogLeft;
    /* 0x10 */ nlPolar m_polarAnalogRight;
    /* 0x18 */ bool m_isLeftAnalogToDPadMapEnabled;
    /* 0x19 */ bool m_isRightAnalogToDPadMapEnabled;
    /* 0x1A */ bool m_unk_0x1A;
    /* 0x1B */ bool m_unk_0x1B;
    /* 0x1C */ bool m_isConnected;
    /* 0x20 */ float m_prevPressure[13];
    /* 0x54 */ float m_currPressure[13];
    /* 0x88 */ float* m_prevPressurePtr;
    /* 0x8C */ float* m_currPressurePtr;

    /* 0x90 */ float m_unk_0x90; // ?? analog left x
    /* 0x94 */ float m_unk_0x94; // ?? analog left y
    /* 0x98 */ float m_unk_0x98; // ?? analog right x
    /* 0x9C */ float m_unk_0x9C; // ?? analog right y
};

#endif // _GLOBALPAD_H_
