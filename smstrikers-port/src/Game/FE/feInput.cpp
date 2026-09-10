#include "Game/FE/feInput.h"

#include "NL/nlMemory.h"
#include "NL/platpad.h"
#include "types.h"
#include <string.h>
#include <math.h>

FEInput* g_pFEInput = nullptr;

struct FEPadData
{
    /* 0x00 */ float fButtonInitialDelay[12];
    /* 0x30 */ float fButtonRepeatRate[12];
    /* 0x60 */ float fButtonTimeSinceLastRepeat[12];
    /* 0x90 */ bool bIsPressed[12];
}; // total size: 0x9C

FEPadData g_aFEPadData[4];

/**
 * Offset/Address/Size: 0x10B4 | 0x8020FAC0 | size: 0x40
 */
void FEInput::Initialize()
{
    g_pFEInput = new ((u8*)nlMalloc(sizeof(FEInput), 8, false)) FEInput();
}

/**
 * Offset/Address/Size: 0x1050 | 0x8020FA5C | size: 0x64
 */
FEInput::FEInput()
{
    for (int i = 0; i < 4; i++)
    {
        m_nExclusiveInputSceneHashIDStack[i].m_pBaseSceneHandler = nullptr;
        m_nExclusiveInputSceneHashIDStack[i].m_customID = -1;
    }
    Reset();
}

/**
 * Offset/Address/Size: 0xFF4 | 0x8020FA00 | size: 0x5C
 */
void FEInput::Reset()
{
    m_bEnableInput[0] = true;
    m_bEnableInput[1] = true;
    m_bEnableInput[2] = true;
    m_bEnableInput[3] = true;
    m_InputLockDepth = 0;
    m_bInputAllowed = true;
    EnableAnalogToDPadMapping(FE_ALL_PADS, false);
    memset(g_aFEPadData, 0, sizeof(g_aFEPadData));
}

/**
 * Offset/Address/Size: 0xF94 | 0x8020F9A0 | size: 0x60
 */
cGlobalPad* FEInput::GetGlobalPad(eFEINPUT_PAD pad) const
{
    if (pad >= 4)
    {
        return nullptr;
    }

    if (!m_bInputAllowed)
    {
        return nullptr;
    }

    if (!m_bEnableInput[pad])
    {
        return nullptr;
    }

    return cPadManager::GetPad(pad);
}

/**
 * Offset/Address/Size: 0xEF8 | 0x8020F904 | size: 0x9C
 */
bool FEInput::IsConnected(eFEINPUT_PAD pad)
{
    if (pad != FE_ALL_PADS)
    {
        if (m_bEnableInput[pad])
        {
            cGlobalPad* pPad = cPadManager::GetPad(pad);
            return pPad->IsConnected();
        }
        else
        {
            return false;
        }
    }

    for (int i = 0; i < 4; i++)
    {
        cGlobalPad* pPad = cPadManager::GetPad(i);
        if (pPad->IsConnected())
        {
            return true;
        }
    }

    return false;
}

/**
 * Offset/Address/Size: 0xBE8 | 0x8020F5F4 | size: 0x310
 */
bool FEInput::IsPressed(eFEINPUT_PAD pad, int button, bool remap, eFEINPUT_PAD* pOutPad)
{
    if (m_bInputAllowed)
    {
        if (pad != FE_ALL_PADS)
        {
            if (!m_bEnableInput[pad])
            {
                return false;
            }
            cGlobalPad* pPad = cPadManager::GetPad(pad);
            bool ispressed = pPad->IsPressed(button, remap);
            return ispressed;
        }

        for (int i = 0; i < 4; i++)
        {
            if (IsPressed((eFEINPUT_PAD)i, button, remap, (eFEINPUT_PAD*)0))
            {
                if (pOutPad)
                {
                    *pOutPad = (eFEINPUT_PAD)i;
                }
                return true;
            }
        }
    }
    return false;
}

/**
 * Offset/Address/Size: 0x87C | 0x8020F288 | size: 0x36C
 * NOTE: this function (and SetAutoRepeatParams) requires "-msext on" in this
 *       TU's extra_cflags (see configure.py). Without it, MWCC 2.5 keeps a
 *       convert node for the (eFEINPUT_PAD)i cast at the recursive call, which
 *       blocks address strength-reduction: the g_aFEPadData base is never fused
 *       into the depth-1 induction variable and every parameter register shifts
 *       by one. With -msext on ("ignore casts to the same type", enum==int under
 *       -enum int) the cast is dropped in the IL and the whole TU matches 100%.
 */
bool FEInput::IsAutoPressed(eFEINPUT_PAD pad, int button, bool remap, eFEINPUT_PAD* pOutPad)
{
    if (m_bInputAllowed)
    {
        if (pad != FE_ALL_PADS)
        {
            if (!m_bEnableInput[pad])
            {
                return false;
            }

            cGlobalPad* pPad = cPadManager::GetPad(pad);
            bool ispressed = pPad->IsPressed(button, remap);

            int mappedButton;
            if (remap)
            {
                mappedButton = cPadManager::GetRemapArray()[button];
            }
            else
            {
                mappedButton = button;
            }
            u32 buttonIndex = GetButtonIndex(mappedButton);

            if (ispressed && !g_aFEPadData[pad].bIsPressed[buttonIndex])
            {
                ispressed = false;
            }

            return ispressed;
        }

        for (int i = 0; i < 4; i++)
        {
            if (IsAutoPressed((eFEINPUT_PAD)i, button, remap, (eFEINPUT_PAD*)0))
            {
                if (pOutPad)
                {
                    *pOutPad = (eFEINPUT_PAD)i;
                }
                return true;
            }
        }
    }
    return false;
}

/**
 * Offset/Address/Size: 0x7AC | 0x8020F1B8 | size: 0xD0
 */
bool FEInput::JustPressed(eFEINPUT_PAD pad, int button, bool remap, eFEINPUT_PAD* pOutPad)
{
    if (m_bInputAllowed)
    {
        if (pad != FE_ALL_PADS)
        {
            if (m_bEnableInput[pad])
            {
                cGlobalPad* pPad = cPadManager::GetPad(pad);
                return pPad->JustPressed(button, remap);
            }
            else
            {
                return false;
            }
        }

        for (int i = 0; i < 4; i++)
        {
            cGlobalPad* pPad = cPadManager::GetPad(i);
            if (pPad->JustPressed(button, remap))
            {
                if (pOutPad != nullptr)
                {
                    *pOutPad = (eFEINPUT_PAD)i;
                }
                return true;
            }
        }
    }
    return false;
}

/**
 * Offset/Address/Size: 0x6DC | 0x8020F0E8 | size: 0xD0
 */
bool FEInput::JustReleased(eFEINPUT_PAD pad, int button, bool remap, eFEINPUT_PAD* pOutPad)
{
    if (m_bInputAllowed)
    {
        if (pad != FE_ALL_PADS)
        {
            if (m_bEnableInput[pad])
            {
                cGlobalPad* pPad = cPadManager::GetPad(pad);
                return pPad->JustReleased(button, remap);
            }
            else
            {
                return false;
            }
        }

        for (int i = 0; i < 4; i++)
        {
            cGlobalPad* pPad = cPadManager::GetPad(i);
            if (pPad->JustReleased(button, remap))
            {
                if (pOutPad != nullptr)
                {
                    *pOutPad = (eFEINPUT_PAD)i;
                }
                return true;
            }
        }
    }
    return false;
}

/**
 * Offset/Address/Size: 0x6A4 | 0x8020F0B0 | size: 0x38
 */
void FEInput::EnableInputIfSceneHasFocus(BaseSceneHandler* pSceneHandler)
{
    int depth = m_InputLockDepth;
    if (depth == 0 || m_nExclusiveInputSceneHashIDStack[depth - 1].m_pBaseSceneHandler == pSceneHandler)
    {
        m_bInputAllowed = true;
        return;
    }

    m_bInputAllowed = false;
}

/**
 * Offset/Address/Size: 0x678 | 0x8020F084 | size: 0x2C
 */
void FEInput::PushExclusiveInputLock(BaseSceneHandler* pRequestingSceneHandler, int customID)
{
    m_nExclusiveInputSceneHashIDStack[m_InputLockDepth].m_pBaseSceneHandler = pRequestingSceneHandler;
    m_nExclusiveInputSceneHashIDStack[m_InputLockDepth].m_customID = customID;
    m_InputLockDepth++;
}

/**
 * Offset/Address/Size: 0x64C | 0x8020F058 | size: 0x2C
 */
void FEInput::PopExclusiveInputLock(BaseSceneHandler* pRequestingSceneHandler)
{
    m_InputLockDepth -= 1;
    InputLockEntry& entry = m_nExclusiveInputSceneHashIDStack[m_InputLockDepth];
    entry.m_pBaseSceneHandler = nullptr;
    entry.m_customID = -1;
}

/**
 * Offset/Address/Size: 0x61C | 0x8020F028 | size: 0x30
 */
bool FEInput::HasInputLock(BaseSceneHandler* pRequestingSceneHandler) const
{
    int depth = m_InputLockDepth;
    if (depth == 0)
    {
        return false;
    }

    return m_nExclusiveInputSceneHashIDStack[depth - 1].m_pBaseSceneHandler == pRequestingSceneHandler;
}

/**
 * Offset/Address/Size: 0x494 | 0x8020EEA0 | size: 0x188
 */
void FEInput::Update(float dt)
{
    for (int padIndex = 0; padIndex < 4; padIndex++)
    {
        for (int buttonMask = 1, buttonIndex = 0; buttonIndex < 12; buttonIndex++, buttonMask <<= 1)
        {
            g_aFEPadData[padIndex].bIsPressed[buttonIndex] = false;

            if (cPadManager::GetPad(padIndex)->IsPressed(buttonMask, false))
            {
                if (g_aFEPadData[padIndex].fButtonTimeSinceLastRepeat[buttonIndex] != 0.0f)
                {
                    float buttonStateTime = cPadManager::GetPad(padIndex)->GetButtonStateTime(buttonMask, false);
                    float diff = buttonStateTime - g_aFEPadData[padIndex].fButtonInitialDelay[buttonIndex];
                    bool bShouldRepeat = true;
                    float timeSinceRepeat = g_aFEPadData[padIndex].fButtonTimeSinceLastRepeat[buttonIndex];
                    float repeatRate = g_aFEPadData[padIndex].fButtonRepeatRate[buttonIndex];

                    if (!(diff > 0.0001f) && !((float)fabs(diff) <= 0.0001f))
                    {
                        bShouldRepeat = false;
                    }

                    if (bShouldRepeat)
                    {
                        float diff2 = buttonStateTime - timeSinceRepeat - repeatRate;
                        bool bShouldRepeat2 = true;

                        if (!(diff2 > 0.0001f) && !((float)fabs(diff2) <= 0.0001f))
                        {
                            bShouldRepeat2 = false;
                        }

                        if (bShouldRepeat2)
                        {
                            g_aFEPadData[padIndex].fButtonTimeSinceLastRepeat[buttonIndex] = buttonStateTime;
                            g_aFEPadData[padIndex].bIsPressed[buttonIndex] = true;
                        }
                    }
                }
                else
                {
                    g_aFEPadData[padIndex].fButtonTimeSinceLastRepeat[buttonIndex] = cPadManager::GetPad(padIndex)->GetButtonStateTime(buttonMask, false);
                    g_aFEPadData[padIndex].bIsPressed[buttonIndex] = true;
                }
            }
            else
            {
                g_aFEPadData[padIndex].fButtonTimeSinceLastRepeat[buttonIndex] = 0.0f;
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x274 | 0x8020EC80 | size: 0x220
 */
void FEInput::SetAutoRepeatParams(eFEINPUT_PAD pad, int button, float initialdelay, float repeatrate)
{
    int remappedButton = cPadManager::GetRemapArray()[button];
    int buttonIndex = GetButtonIndex(remappedButton);

    if (pad == FE_ALL_PADS)
    {
        for (int i = 0; i < 4; i++)
        {
            SetAutoRepeatParams((eFEINPUT_PAD)i, button, initialdelay, repeatrate);
        }
    }
    else
    {
        g_aFEPadData[pad].fButtonInitialDelay[buttonIndex] = initialdelay;
        g_aFEPadData[pad].fButtonRepeatRate[buttonIndex] = repeatrate;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8020EA0C | size: 0x274
 */
void FEInput::EnableAnalogToDPadMapping(eFEINPUT_PAD pad, bool enable)
{
    // FORCE_DONT_INLINE;
    if (pad == FE_ALL_PADS)
    {
        for (int i = 0; i < 4; i++)
        {
            EnableAnalogToDPadMapping((eFEINPUT_PAD)i, enable);
        }
    }
    else
    {
        if (enable)
        {
            cPadManager::GetPad(pad)->EnableLeftAnalogToDPadMap();
        }
        else
        {
            cPadManager::GetPad(pad)->DisableLeftAnalogToDPadMap();
        }
    }
}
