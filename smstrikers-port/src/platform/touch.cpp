// See include/port/touch.h.

#include "port/touch.h"

#if defined(__ANDROID__) && defined(PORT_USE_AURORA)

#include <atomic>
#include <cstdio>
#include <cstdlib>

#include <jni.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_stdinc.h>

namespace {

// The state the overlay last sent, and the only thing that crosses between the
// two threads. Written from the UI thread inside the JNI call, read from the
// game thread in PortTouchDraw: relaxed atomics rather than a lock, because a
// frame that reads the previous button state is a frame late and a frame late
// is invisible, while a UI thread that blocks on the game's loop is a dropped
// touch the player feels.
std::atomic<int> s_buttons{0};
std::atomic<int> s_triggerL{0};      // 0..255
std::atomic<int> s_triggerR{0};
std::atomic<int> s_stickX{0};        // -32767..32767
std::atomic<int> s_stickY{0};
std::atomic<int> s_cstickX{0};
std::atomic<int> s_cstickY{0};

// The bits Toque.java sends. They are ours, not anyone's hardware: the overlay
// speaks GameCube and the mapping to SDL happens here, once.
enum
{
    TOUCH_A     = 1 << 0,
    TOUCH_B     = 1 << 1,
    TOUCH_X     = 1 << 2,
    TOUCH_Y     = 1 << 3,
    TOUCH_Z     = 1 << 4,
    TOUCH_START = 1 << 5,
    TOUCH_UP    = 1 << 6,
    TOUCH_DOWN  = 1 << 7,
    TOUCH_LEFT  = 1 << 8,
    TOUCH_RIGHT = 1 << 9,
    TOUCH_LB    = 1 << 10,
    TOUCH_L3    = 1 << 11,
    TOUCH_R3    = 1 << 12,
    TOUCH_BACK  = 1 << 13,
};

SDL_JoystickID s_virtualId;
SDL_Joystick*  s_virtual;
bool           s_tried;
bool           s_disabled;

// Read by Java through PortTouchActive, so the overlay can take itself off the
// screen the moment a real controller arrives.
std::atomic<bool> s_showing{false};

bool real_gamepad_connected()
{
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids == nullptr)
        return false;
    bool found = false;
    for (int i = 0; i < count; i++)
    {
        if (ids[i] != s_virtualId)
        {
            found = true;
            break;
        }
    }
    SDL_free(ids);
    return found;
}

void attach()
{
    if (s_virtual != nullptr || s_disabled)
        return;

    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = (Uint16)SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.name = "Strikers touch controls";
    // The mask says which of those SDL should believe in. Without it SDL
    // advertises a full controller and the button prompts offer the player
    // inputs the overlay does not draw.
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_SOUTH) | (1u << SDL_GAMEPAD_BUTTON_EAST) |
                       (1u << SDL_GAMEPAD_BUTTON_WEST) | (1u << SDL_GAMEPAD_BUTTON_NORTH) |
                       (1u << SDL_GAMEPAD_BUTTON_START) |
                       (1u << SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) |
                       (1u << SDL_GAMEPAD_BUTTON_BACK) |
                       (1u << SDL_GAMEPAD_BUTTON_LEFT_STICK) |
                       (1u << SDL_GAMEPAD_BUTTON_RIGHT_STICK) |
                       (1u << SDL_GAMEPAD_BUTTON_DPAD_UP) | (1u << SDL_GAMEPAD_BUTTON_DPAD_DOWN) |
                       (1u << SDL_GAMEPAD_BUTTON_DPAD_LEFT) | (1u << SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_LEFTX) | (1u << SDL_GAMEPAD_AXIS_LEFTY) |
                     (1u << SDL_GAMEPAD_AXIS_RIGHTX) | (1u << SDL_GAMEPAD_AXIS_RIGHTY) |
                     (1u << SDL_GAMEPAD_AXIS_LEFT_TRIGGER) |
                     (1u << SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

    s_virtualId = SDL_AttachVirtualJoystick(&desc);
    if (s_virtualId == 0)
    {
        // Once, and then never again: a failure here is a build without virtual
        // joystick support, not a transient.
        std::fprintf(stderr, "[touch] no virtual gamepad (%s); on-screen controls are off\n",
                     SDL_GetError());
        s_disabled = true;
        return;
    }
    s_virtual = SDL_OpenJoystick(s_virtualId);
    if (s_virtual == nullptr)
    {
        std::fprintf(stderr, "[touch] virtual gamepad attached but would not open (%s)\n",
                     SDL_GetError());
        SDL_DetachVirtualJoystick(s_virtualId);
        s_virtualId = 0;
        s_disabled = true;
        return;
    }
    s_showing.store(true, std::memory_order_release);
    std::fprintf(stderr, "[touch] on-screen controls attached as joystick %u\n",
                 (unsigned)s_virtualId);
}

void detach()
{
    if (s_virtual == nullptr)
        return;
    // Everything released first: a detach with a button held leaves the game
    // holding it.
    s_buttons.store(0, std::memory_order_relaxed);
    s_triggerL.store(0, std::memory_order_relaxed);
    s_triggerR.store(0, std::memory_order_relaxed);
    s_stickX.store(0, std::memory_order_relaxed);
    s_stickY.store(0, std::memory_order_relaxed);
    s_cstickX.store(0, std::memory_order_relaxed);
    s_cstickY.store(0, std::memory_order_relaxed);

    SDL_CloseJoystick(s_virtual);
    SDL_DetachVirtualJoystick(s_virtualId);
    s_virtual = nullptr;
    s_virtualId = 0;
    s_showing.store(false, std::memory_order_release);
    std::fprintf(stderr, "[touch] a real controller arrived; on-screen controls off\n");
}

void push_state()
{
    if (s_virtual == nullptr)
        return;

    const int b = s_buttons.load(std::memory_order_relaxed);

    SDL_SetJoystickVirtualButton(s_virtual, SDL_GAMEPAD_BUTTON_SOUTH, (b & TOUCH_A) != 0);
    SDL_SetJoystickVirtualButton(s_virtual, SDL_GAMEPAD_BUTTON_EAST, (b & TOUCH_B) != 0);
    SDL_SetJoystickVirtualButton(s_virtual, SDL_GAMEPAD_BUTTON_WEST, (b & TOUCH_X) != 0);
    SDL_SetJoystickVirtualButton(s_virtual, SDL_GAMEPAD_BUTTON_NORTH, (b & TOUCH_Y) != 0);
    SDL_SetJoystickVirtualButton(s_virtual, SDL_GAMEPAD_BUTTON_START, (b & TOUCH_START) != 0);
    // Z sai de qualquer um dos dois ombros. O conjunto desenhado e o do Xbox
    // 360, que tem LB e RB; o GameCube tem um Z so, e nao ha razao para o
    // jogador ter de descobrir qual dos dois e o de verdade.
    SDL_SetJoystickVirtualButton(s_virtual, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
                                 (b & (TOUCH_Z | TOUCH_LB)) != 0);
    SDL_SetJoystickVirtualButton(s_virtual, SDL_GAMEPAD_BUTTON_BACK, (b & TOUCH_BACK) != 0);
    SDL_SetJoystickVirtualButton(s_virtual, SDL_GAMEPAD_BUTTON_LEFT_STICK, (b & TOUCH_L3) != 0);
    SDL_SetJoystickVirtualButton(s_virtual, SDL_GAMEPAD_BUTTON_RIGHT_STICK, (b & TOUCH_R3) != 0);
    SDL_SetJoystickVirtualButton(s_virtual, SDL_GAMEPAD_BUTTON_DPAD_UP, (b & TOUCH_UP) != 0);
    SDL_SetJoystickVirtualButton(s_virtual, SDL_GAMEPAD_BUTTON_DPAD_DOWN, (b & TOUCH_DOWN) != 0);
    SDL_SetJoystickVirtualButton(s_virtual, SDL_GAMEPAD_BUTTON_DPAD_LEFT, (b & TOUCH_LEFT) != 0);
    SDL_SetJoystickVirtualButton(s_virtual, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, (b & TOUCH_RIGHT) != 0);

    // L and R are axes, not buttons: Aurora's standard mapping leaves their
    // button entries invalid and reads the triggers, because on a GameCube pad
    // they are analog and the game asks how far. Released is 0 and not
    // SDL_JOYSTICK_AXIS_MIN -- a gamepad trigger axis runs 0..32767, unlike
    // every other axis, and -32768 there reads as a trigger held down.
    const int l = s_triggerL.load(std::memory_order_relaxed);
    const int r = s_triggerR.load(std::memory_order_relaxed);
    SDL_SetJoystickVirtualAxis(s_virtual, SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
                               (Sint16)(l * SDL_JOYSTICK_AXIS_MAX / 255));
    SDL_SetJoystickVirtualAxis(s_virtual, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
                               (Sint16)(r * SDL_JOYSTICK_AXIS_MAX / 255));

    SDL_SetJoystickVirtualAxis(s_virtual, SDL_GAMEPAD_AXIS_LEFTX,
                               (Sint16)s_stickX.load(std::memory_order_relaxed));
    SDL_SetJoystickVirtualAxis(s_virtual, SDL_GAMEPAD_AXIS_LEFTY,
                               (Sint16)s_stickY.load(std::memory_order_relaxed));
    SDL_SetJoystickVirtualAxis(s_virtual, SDL_GAMEPAD_AXIS_RIGHTX,
                               (Sint16)s_cstickX.load(std::memory_order_relaxed));
    SDL_SetJoystickVirtualAxis(s_virtual, SDL_GAMEPAD_AXIS_RIGHTY,
                               (Sint16)s_cstickY.load(std::memory_order_relaxed));
}

bool suppressed()
{
    static int s_off = -1;
    if (s_off < 0)
    {
        const char* e = std::getenv("STRIKERS_TOUCH");
        // Present and zero turns them off; anything else, including absent,
        // leaves them on. One key, because there is no third state to want.
        s_off = (e != nullptr && e[0] == '0') ? 1 : 0;
    }
    return s_off != 0;
}

} // namespace

// The overlay is drawn by OverlayView on the Java side and the fingers never
// reach here, so there is nothing to do with an SDL event. Kept because the
// call site in main.cpp is the same on every platform.
extern "C" void PortTouchEvent(const SDL_Event* event) { (void)event; }

extern "C" void PortTouchDraw(void)
{
    if (s_disabled || suppressed())
        return;

    if (!s_tried)
    {
        s_tried = true;
        attach();
    }

    if (real_gamepad_connected())
    {
        detach();
        return;
    }
    if (s_virtual == nullptr)
    {
        // Detached because a controller arrived, and now it is gone.
        attach();
        if (s_virtual == nullptr)
            return;
    }

    push_state();
}

extern "C" int PortTouchActive(void)
{
    return s_showing.load(std::memory_order_acquire) ? 1 : 0;
}

// ---------------------------------------------------------------------------
// The bridge from Toque.java.
//
// The names are the contract: the virtual machine finds these by their full
// spelling, so renaming the class or moving it to another package compiles on
// both sides and breaks at the first touch with UnsatisfiedLinkError.
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL
Java_io_github_newcoke_strikers_Toque_enviar(JNIEnv*, jclass, jint botoes, jint gatilhoEsq,
                                             jint gatilhoDir, jint lx, jint ly, jint cx, jint cy)
{
    s_buttons.store((int)botoes, std::memory_order_relaxed);
    s_triggerL.store((int)gatilhoEsq, std::memory_order_relaxed);
    s_triggerR.store((int)gatilhoDir, std::memory_order_relaxed);
    s_stickX.store((int)lx, std::memory_order_relaxed);
    s_stickY.store((int)ly, std::memory_order_relaxed);
    s_cstickX.store((int)cx, std::memory_order_relaxed);
    s_cstickY.store((int)cy, std::memory_order_relaxed);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_newcoke_strikers_Toque_pronto(JNIEnv*, jclass)
{
    // True once there is somewhere for the state to go. Asked rather than
    // assumed: SDLActivity loads the library inside onCreate, and anything of
    // ours that arrives before that finds a native method with no
    // implementation.
    return s_showing.load(std::memory_order_acquire) ? JNI_TRUE : JNI_FALSE;
}

#else // !__ANDROID__ || !PORT_USE_AURORA

extern "C" void PortTouchEvent(const SDL_Event* event) { (void)event; }
extern "C" void PortTouchDraw(void) {}
extern "C" int PortTouchActive(void) { return 0; }

#endif
