#include "Game/PadActions.h"

#include "NL/platpad.h"
#include "NL/nlConfig.h"
#include "NL/nlFormat.h"

#include "NL/nlMemory.h"

#include "Game/PadMonkey.h"
#include "NL/globalpad.h"

bool g_bEnableGamecubePadMonkey;

s32 g_pPadRemapArray[38] = {
    0x00000020, 0x00000040, 0x00000008, 0x00000004, 0x00000004, 0x00000040, 0x00000020, 0x00000100, 0x00000010, 0x00000100, 0x00000200, 0x00000001, 0x00000002, 0x00000008, 0x00000004, 0x00000100, 0x00000200, 0x00000400, 0x00000800, 0x00001000, 0x00000020, 0x00000040, 0x00000400, 0x00000010, 0x00000800, 0x00000200, 0x00000100, 0x00000100, 0x00000200, 0x00000800, 0x00000800, 0x00000800, 0x00000020, 0x00001000, 0x00000010, 0x00000010, 0x00001000, 0x00001000
};

/**
 * Offset/Address/Size: 0x10F8 | 0x80193418 | size: 0x198
 */
void InitPads()
{
    g_bEnableGamecubePadMonkey = GetConfigBool(Config::Global(), "enable_pad_monkey", false);

    InitPlatPad();

    if (!g_bEnableGamecubePadMonkey)
    {
        for (int i = 0; i < 4; i++)
        {
            cPadManager::m_aPads[i] = new (nlMalloc(sizeof(cPlatPad), 8, false)) cPlatPad(i);
        }
    }
    else
    {
        for (int i = 0; i < 4; i++)
        {
            cPadManager::m_aPads[i] = new (nlMalloc(sizeof(PadMonkey), 8, false)) PadMonkey(i);
        }
    }

    UpdateMonkeyState(0);
    cPadManager::m_pRemapArray = g_pPadRemapArray;
    if (g_bEnableGamecubePadMonkey)
    {
        cPlatPad::m_bDisableRumble = true;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80192320 | size: 0x10F8
 */
void UpdateMonkeyState(int monkeySet)
{
    if (!g_bEnableGamecubePadMonkey)
    {
        return;
    }

    for (int j = 0; j < 4; j++)
    {
        PadMonkey* monkey = (PadMonkey*)cPadManager::GetPad(j);
        BasicString<char, Detail::TempStringAllocator> monkeyPad = Format<BasicString<char, Detail::TempStringAllocator>, int, int>(
            BasicString<char, Detail::TempStringAllocator>("{0}_pad_monkey_{1}/"), monkeySet, j);

        monkey->m_unk_0xA0 = GetConfigFloat(Config::Global(), monkeyPad.Append("connected").c_str(), 100.0f);
        monkey->SetButtonChance(0x0001, GetConfigFloat(Config::Global(), monkeyPad.Append("button_left").c_str(), 10.0f));
        monkey->SetButtonChance(0x0002, GetConfigFloat(Config::Global(), monkeyPad.Append("button_right").c_str(), 10.0f));
        monkey->SetButtonChance(0x0004, GetConfigFloat(Config::Global(), monkeyPad.Append("button_down").c_str(), 10.0f));
        monkey->SetButtonChance(0x0008, GetConfigFloat(Config::Global(), monkeyPad.Append("button_up").c_str(), 10.0f));
        monkey->SetButtonChance(0x0010, GetConfigFloat(Config::Global(), monkeyPad.Append("trigger_z").c_str(), 10.0f));
        monkey->SetButtonChance(0x0020, GetConfigFloat(Config::Global(), monkeyPad.Append("trigger_r").c_str(), 10.0f));
        monkey->SetButtonChance(0x0040, GetConfigFloat(Config::Global(), monkeyPad.Append("trigger_l").c_str(), 10.0f));
        monkey->SetButtonChance(0x0100, GetConfigFloat(Config::Global(), monkeyPad.Append("button_a").c_str(), 10.0f));
        monkey->SetButtonChance(0x0200, GetConfigFloat(Config::Global(), monkeyPad.Append("button_b").c_str(), 10.0f));
        monkey->SetButtonChance(0x0400, GetConfigFloat(Config::Global(), monkeyPad.Append("button_x").c_str(), 10.0f));
        monkey->SetButtonChance(0x0800, GetConfigFloat(Config::Global(), monkeyPad.Append("button_y").c_str(), 10.0f));
        monkey->SetButtonChance(0x1000, GetConfigFloat(Config::Global(), monkeyPad.Append("button_start").c_str(), 10.0f));

        monkey->Update(0.0f);
    }
}
