#include "Game/PadMonkey.h"

#include "NL/nlMath.h"
#include "NL/nlMemory.h"
#include "NL/platpad.h"

extern unsigned int nlDefaultSeed;

/**
 * Offset/Address/Size: 0x39C | 0x80194B34 | size: 0xF4
 */
PadMonkey::PadMonkey(int padIndex)
    : cGlobalPad(padIndex)
{
    m_isConnected = false;

    m_prevPressurePtr = &m_prevPressure[0];
    m_currPressurePtr = &m_currPressure[0];

    m_unk_0x90 = 0.0f;
    m_unk_0x94 = 0.0f;
    m_unk_0x98 = 0.0f;
    m_unk_0x9C = 0.0f;

    memset(m_prevPressurePtr, 0, 0x34);
    memset(m_currPressurePtr, 0, 0x34);

    for (int i = 0; i < 13; ++i)
    {
        m_buttonChance[i] = 10.0f;
    }
}

/**
 * Offset/Address/Size: 0x338 | 0x80194AD0 | size: 0x64
 */
bool PadMonkey::IsPressed(int button, bool remap)
{
    if (remap)
    {
        button = cPadManager::m_pRemapArray[button];
    }

    return m_currPressurePtr[GetButtonIndex(button)] >= 0.5f;
}

/**
 * Offset/Address/Size: 0x2E8 | 0x80194A80 | size: 0x50
 */
f32 PadMonkey::GetPressure(int button, bool remap)
{
    if (remap)
    {
        button = cPadManager::m_pRemapArray[button];
    }
    return m_currPressurePtr[GetButtonIndex(button)];
}

/**
 * Offset/Address/Size: 0x270 | 0x80194A08 | size: 0x78
 */
bool PadMonkey::PlatJustPressed(int button, bool remap)
{
    if (remap)
    {
        button = cPadManager::m_pRemapArray[button];
    }
    const int idx = GetButtonIndex(button);

    bool pressed = false;
    if (m_currPressurePtr[idx] >= 0.5f && m_prevPressurePtr[idx] < 0.5f)
    {
        pressed = true;
    }
    return pressed;
}

/**
 * Offset/Address/Size: 0x1F8 | 0x80194990 | size: 0x78
 */
bool PadMonkey::PlatJustReleased(int button, bool remap)
{
    if (remap)
    {
        button = cPadManager::m_pRemapArray[button];
    }
    const int idx = GetButtonIndex(button);

    bool released = false;
    if (m_currPressurePtr[idx] < 0.5f && m_prevPressurePtr[idx] >= 0.5f)
    {
        released = true;
    }
    return released;
}

/**
 * Offset/Address/Size: 0x1F0 | 0x80194988 | size: 0x8
 */
f32 PadMonkey::GetButtonStateTime(int button, bool remap)
{
    return 0.0f;
}

/**
 * Offset/Address/Size: 0x48 | 0x801947E0 | size: 0x1A8
 */
void PadMonkey::Update(float dt)
{
    // swap prev <-> curr buffers
    float* p = m_prevPressurePtr;
    m_prevPressurePtr = m_currPressurePtr;
    m_currPressurePtr = p;

    float r = nlRandomf(100.0f, &nlDefaultSeed);
    m_isConnected = m_unk_0xA0 > r;

    if (m_isConnected)
    {
        for (int i = 0; i < 13; ++i)
        {
            const float pick = nlRandomf(100.0f, &nlDefaultSeed);
            if (m_buttonChance[i] > pick)
            {
                m_currPressurePtr[i] = 0.5f + 0.5f * nlRandomf(1.0f, &nlDefaultSeed);
            }
            else
            {
                m_currPressurePtr[i] = 0.0f;
            }
        }

        // analogs: randf(2.0) - 1.0 into this+0x90..0x9C
        m_unk_0x90 = nlRandomf(2.0f, &nlDefaultSeed) - 1.0f;
        m_unk_0x94 = nlRandomf(2.0f, &nlDefaultSeed) - 1.0f;
        m_unk_0x98 = nlRandomf(2.0f, &nlDefaultSeed) - 1.0f;
        m_unk_0x9C = nlRandomf(2.0f, &nlDefaultSeed) - 1.0f;
    }
    else
    {
        // not active: clear current pressures/analogs
        memset(m_currPressurePtr, 0, 0x34);
        m_unk_0x90 = 0.0f;
        m_unk_0x94 = 0.0f;
        m_unk_0x98 = 0.0f;
        m_unk_0x9C = 0.0f;
    }

    cGlobalPad::Update(dt);
}

/**
 * Offset/Address/Size: 0x0 | 0x80194798 | size: 0x48
 */
void PadMonkey::SetButtonChance(int button, float pct)
{
    m_buttonChance[GetButtonIndex(button)] = pct;
}
