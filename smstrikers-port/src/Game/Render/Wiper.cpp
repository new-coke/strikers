#include "Game/Render/Wiper.h"

#include "string.h"

#include "NL/nlConfig.h"
#include "Game/Audio/WorldAudio.h"

extern bool g_ForceDoubleBallTransition;

namespace
{
static WiperCallback wiperCallback;
}

/**
 * Offset/Address/Size: 0x314 | 0x80127624 | size: 0xC
 */
void WiperCallback::TransitionFinished()
{
    mTransitionActive = false;
};
/**
 * Offset/Address/Size: 0x320 | 0x80127630 | size: 0x4
 */
void WiperCallback::TransitionProgressed(float fDeltaT) { };

/**
 * Offset/Address/Size: 0x2FC | 0x8012760C | size: 0x18
 */
void Wiper::Reset()
{
    wiperCallback.mTransitionActive = false;
    ScreenTransitionManager::Instance()->m_SelectedTransition = NULL;
}

/**
 * Offset/Address/Size: 0x2F8 | 0x80127608 | size: 0x4
 */
void Wiper::Initialize()
{
}

/**
 * Offset/Address/Size: 0x2EC | 0x801275FC | size: 0xC
 */
bool Wiper::WipeInProgress() const
{
    return wiperCallback.mTransitionActive;
}

/**
 * Offset/Address/Size: 0x2E0 | 0x801275F0 | size: 0xC
 */
bool Wiper::CutHasOccured() const
{
    return ScreenTransitionManager::Instance()->m_Cut;
}

/**
 * Offset/Address/Size: 0x234 | 0x80127544 | size: 0xAC
 */
Wiper& Wiper::Instance()
{
    static Wiper instance;
    return instance;
}

/**
 * Offset/Address/Size: 0xDC | 0x801273EC | size: 0x158
 */
void Wiper::DoWipe(const char* wipe)
{
    if (!wiperCallback.mTransitionActive)
    {
        if (g_ForceDoubleBallTransition != 0)
        {
            wipe = "double_ball";
        }

        wiperCallback.mTransitionActive = true;

        if ((nlStrICmp<char>(wipe, "break_glass") != 0)
            && (nlStrICmp<char>(wipe, "cut") != 0)
            && (nlStrICmp<char>(wipe, "fade_to_white_and_back") != 0)
            && (nlStrICmp<char>(wipe, "crossfade") != 0)
            && (nlStrICmp<char>(wipe, "cross_fade") != 0))
        {
            Audio::gWorldSFX.Play(Audio::WORLDSFX_HUD_TOGGLE_UP, 100.0f, -1.0f, 1, 100.0f);
        }

        if (strcmp(wipe, "cut") == 0)
        {
            wiperCallback.mTransitionActive = false;
            return;
        }

        ScreenTransitionManager::Instance()->m_pCallback = &wiperCallback;
        if ((g_ForceDoubleBallTransition == 0) && (ScreenTransitionManager::Instance()->m_SelectedTransition != 0))
        {
            ScreenTransitionManager::Instance()->EnableSelectedTransition();
            return;
        }

        ScreenTransitionManager::Instance()->EnableRandomTransition(wipe);
        g_ForceDoubleBallTransition = 0;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80127310 | size: 0xDC
 */
void Wiper::Render(float dt)
{
    dt = dt * GetConfigFloat(Config::Global(), "transitions/speed", 1.0f);
    ScreenTransitionManager::Instance()->Render(dt);
}

/**
 * Offset/Address/Size: 0x32C | 0x8012763C | size: 0x4
 */
void ScreenTransitionCallback::TransitionFinished() { };
/**
 * Offset/Address/Size: 0x330 | 0x80127640 | size: 0x4
 */
void ScreenTransitionCallback::TransitionProgressed(float fDeltaT) { };
/**
 * Offset/Address/Size: 0x324 | 0x80127634 | size: 0x4
 */
void ScreenTransitionCallback::Cut() { };
/**
 * Offset/Address/Size: 0x328 | 0x80127638 | size: 0x4
 */
void ScreenTransitionCallback::SequenceSwitch() { };
