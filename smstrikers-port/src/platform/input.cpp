// What the keyboard and a gamepad are bound to, and where a person changes it.

// Everything below needs Aurora.
#if !defined(PORT_USE_AURORA)

extern "C" void PortInstallKeyboardBindings(void) {}
extern "C" void PortNoteSceneEntered(int scene) { (void)scene; }
extern "C" void PortUpdateSyntheticInput(unsigned long frame) { (void)frame; }

#else

#include <cstdlib>
#include <cstring>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>

#include "dolphin/os.h"
#include "dolphin/pad.h"
#include "port/input.h"

namespace
{

// Reading the [input] section.

const char* input_cfg(const char* envName)
{
    const char* v = std::getenv(envName);
    return (v != nullptr && *v != '\0') ? v : nullptr;
}

bool str_ieq(const char* a, const char* b)
{
    for (; *a != '\0' && *b != '\0'; a++, b++)
    {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb)
            return false;
    }
    return *a == *b;
}

bool cfg_off(const char* v)
{
    return str_ieq(v, "0") || str_ieq(v, "false") || str_ieq(v, "off") || str_ieq(v, "no");
}

bool probe_pad() { return std::getenv("STRIKERS_PROBE_PAD") != nullptr; }

// SDL's scancode table is spelled exactly ("Space", "Left Shift", "Keypad 0"), and a settings file
// is not.
SDL_Scancode scancode_from_name(const char* name)
{
    if (name == nullptr || *name == '\0')
        return SDL_SCANCODE_UNKNOWN;

    SDL_Scancode code = SDL_GetScancodeFromName(name);
    if (code != SDL_SCANCODE_UNKNOWN)
        return code;

    char buf[64];
    const size_t n = std::strlen(name);
    if (n >= sizeof buf)
        return SDL_SCANCODE_UNKNOWN;

    bool wordStart = true;
    for (size_t i = 0; i < n; i++)
    {
        char c = name[i];
        if (wordStart)
        {
            if (c >= 'a' && c <= 'z')
                c = (char)(c - 'a' + 'A');
        }
        else if (c >= 'A' && c <= 'Z')
        {
            c = (char)(c - 'A' + 'a');
        }
        buf[i] = c;
        wordStart = (name[i] == ' ');
    }
    buf[n] = '\0';
    return SDL_GetScancodeFromName(buf);
}

const char* key_name(SDL_Scancode code)
{
    const char* n = SDL_GetScancodeName(code);
    return (n != nullptr && *n != '\0') ? n : "(none)";
}

enum KeyId
{
    KEY_A, KEY_B, KEY_X, KEY_Y, KEY_Z, KEY_START, KEY_L, KEY_R,
    KEY_DPAD_UP, KEY_DPAD_DOWN, KEY_DPAD_LEFT, KEY_DPAD_RIGHT,
    KEY_STICK_UP, KEY_STICK_DOWN, KEY_STICK_LEFT, KEY_STICK_RIGHT,
    KEY_CSTICK_UP, KEY_CSTICK_DOWN, KEY_CSTICK_LEFT, KEY_CSTICK_RIGHT,
    KEY_COUNT
};

struct KeyDefault
{
    const char* key;   // as strikers.ini spells it
    const char* env;   // as config.c turns it into a variable
    const char* def;   // the layout that shipped
};

// clang-format off
const KeyDefault kKeyDefaults[KEY_COUNT] = {
    { "key_a",           "STRIKERS_KEY_A",           "X" },
    { "key_b",           "STRIKERS_KEY_B",           "Z" },
    { "key_x",           "STRIKERS_KEY_X",           "C" },
    { "key_y",           "STRIKERS_KEY_Y",           "V" },
    { "key_z",           "STRIKERS_KEY_Z",           "Space" },
    { "key_start",       "STRIKERS_KEY_START",       "Return" },
    { "key_l",           "STRIKERS_KEY_L",           "Q" },
    { "key_r",           "STRIKERS_KEY_R",           "E" },
    { "key_dpad_up",     "STRIKERS_KEY_DPAD_UP",     "Up" },
    { "key_dpad_down",   "STRIKERS_KEY_DPAD_DOWN",   "Down" },
    { "key_dpad_left",   "STRIKERS_KEY_DPAD_LEFT",   "Left" },
    { "key_dpad_right",  "STRIKERS_KEY_DPAD_RIGHT",  "Right" },
    { "key_stick_up",    "STRIKERS_KEY_STICK_UP",    "W" },
    { "key_stick_down",  "STRIKERS_KEY_STICK_DOWN",  "S" },
    { "key_stick_left",  "STRIKERS_KEY_STICK_LEFT",  "A" },
    { "key_stick_right", "STRIKERS_KEY_STICK_RIGHT", "D" },
    { "key_cstick_up",   "STRIKERS_KEY_CSTICK_UP",   "I" },
    { "key_cstick_down", "STRIKERS_KEY_CSTICK_DOWN", "K" },
    { "key_cstick_left", "STRIKERS_KEY_CSTICK_LEFT", "J" },
    { "key_cstick_right","STRIKERS_KEY_CSTICK_RIGHT","L" },
};

struct KeyButtonRow { KeyId key; PADButton button; };

const KeyButtonRow kKeyButtons[] = {
    { KEY_A,           PAD_BUTTON_A },
    { KEY_B,           PAD_BUTTON_B },
    { KEY_X,           PAD_BUTTON_X },
    { KEY_Y,           PAD_BUTTON_Y },
    { KEY_Z,           PAD_TRIGGER_Z },
    { KEY_START,       PAD_BUTTON_START },
    { KEY_L,           PAD_TRIGGER_L },
    { KEY_R,           PAD_TRIGGER_R },
    { KEY_DPAD_UP,     PAD_BUTTON_UP },
    { KEY_DPAD_DOWN,   PAD_BUTTON_DOWN },
    { KEY_DPAD_LEFT,   PAD_BUTTON_LEFT },
    { KEY_DPAD_RIGHT,  PAD_BUTTON_RIGHT },
};

struct KeyAxisRow { KeyId key; PADAxis axis; };

// key_l and key_r appear here as well as above.
const KeyAxisRow kKeyAxes[] = {
    { KEY_STICK_RIGHT,  PAD_AXIS_LEFT_X_POS },
    { KEY_STICK_LEFT,   PAD_AXIS_LEFT_X_NEG },
    { KEY_STICK_UP,     PAD_AXIS_LEFT_Y_POS },
    { KEY_STICK_DOWN,   PAD_AXIS_LEFT_Y_NEG },
    { KEY_CSTICK_RIGHT, PAD_AXIS_RIGHT_X_POS },
    { KEY_CSTICK_LEFT,  PAD_AXIS_RIGHT_X_NEG },
    { KEY_CSTICK_UP,    PAD_AXIS_RIGHT_Y_POS },
    { KEY_CSTICK_DOWN,  PAD_AXIS_RIGHT_Y_NEG },
    { KEY_L,            PAD_AXIS_TRIGGER_L },
    { KEY_R,            PAD_AXIS_TRIGGER_R },
};
// clang-format on

SDL_Scancode s_keyCode[KEY_COUNT];

void resolve_keys()
{
    for (int i = 0; i < KEY_COUNT; i++)
    {
        SDL_Scancode code = SDL_SCANCODE_UNKNOWN;
        if (const char* v = input_cfg(kKeyDefaults[i].env))
        {
            code = scancode_from_name(v);
            // Refusing to start over a typo in a settings file is worse than ignoring it, but
            // ignoring it silently is worse than either: the key simply stops working and nothing
            // says why.
            if (code == SDL_SCANCODE_UNKNOWN)
                OSReport("[port] input: %s = \"%s\" is not a key name; keeping %s\n",
                         kKeyDefaults[i].key, v, kKeyDefaults[i].def);
        }
        if (code == SDL_SCANCODE_UNKNOWN)
            code = scancode_from_name(kKeyDefaults[i].def);
        s_keyCode[i] = code;
    }
}

// The layout is only correct if no key does two jobs; a duplicate is invisible until something
// moves on its own.
void port_check_bindings_distinct()
{
    for (int i = 0; i < KEY_COUNT; i++)
    {
        if (s_keyCode[i] == SDL_SCANCODE_UNKNOWN)
            continue;
        for (int j = i + 1; j < KEY_COUNT; j++)
        {
            if (s_keyCode[i] == s_keyCode[j])
            {
                OSReport("[port] input: %s and %s are both \"%s\"; one key cannot "
                         "do two jobs; %s keeps it, %s is left unbound\n",
                         kKeyDefaults[i].key, kKeyDefaults[j].key, key_name(s_keyCode[i]),
                         kKeyDefaults[i].key, kKeyDefaults[j].key);
                s_keyCode[j] = SDL_SCANCODE_UNKNOWN;
            }
        }
    }
}

// Port 0 only. The game supports four, but a second keyboard player is not a thing anyone wants;
// ports 1-3 stay available for real controllers.
const u32 kKeyboardPort = 0;

void install_keyboard()
{
    PADClearKeyBindings(kKeyboardPort);

    for (unsigned i = 0; i < sizeof kKeyButtons / sizeof kKeyButtons[0]; i++)
    {
        if (s_keyCode[kKeyButtons[i].key] == SDL_SCANCODE_UNKNOWN)
            continue;   // refused in port_check_bindings_distinct, or never named
        PADKeyButtonBinding binding;
        binding.scancode = (s32)s_keyCode[kKeyButtons[i].key];
        binding.padButton = kKeyButtons[i].button;
        PADSetKeyButtonBinding(kKeyboardPort, binding);
    }

    for (unsigned i = 0; i < sizeof kKeyAxes / sizeof kKeyAxes[0]; i++)
    {
        if (s_keyCode[kKeyAxes[i].key] == SDL_SCANCODE_UNKNOWN)
            continue;
        PADKeyAxisBinding binding;
        binding.scancode = (s32)s_keyCode[kKeyAxes[i].key];
        binding.padAxis = kKeyAxes[i].axis;
        binding.influence = 100;   // a key is all the way on or all the way off
        PADSetKeyAxisBinding(kKeyboardPort, binding);
    }

    PADSetKeyboardActive(kKeyboardPort, TRUE);
}

// What Aurora holds, not what was just sent to it.
void report_keyboard()
{
    u32 nb = 0, na = 0;
    const PADKeyButtonBinding* buttons = PADGetKeyButtonBindings(kKeyboardPort, &nb);
    const PADKeyAxisBinding* axes = PADGetKeyAxisBindings(kKeyboardPort, &na);
    if (buttons == nullptr)
    {
        OSReport("[port] input: keyboard pad is off\n");
        return;
    }

    OSReport("[port] input: keyboard, port %u (strikers.ini [input])\n", kKeyboardPort);
    for (unsigned i = 0; i < sizeof kKeyButtons / sizeof kKeyButtons[0]; i++)
    {
        for (u32 j = 0; j < nb; j++)
        {
            if (buttons[j].padButton != kKeyButtons[i].button)
                continue;
            OSReport("[port] input:   %-18s = %-12s -> %s\n", kKeyDefaults[kKeyButtons[i].key].key,
                     key_name((SDL_Scancode)buttons[j].scancode),
                     PADGetButtonName(kKeyButtons[i].button));
        }
    }
    for (unsigned i = 0; i < sizeof kKeyAxes / sizeof kKeyAxes[0]; i++)
    {
        if (kKeyAxes[i].axis == PAD_AXIS_TRIGGER_L || kKeyAxes[i].axis == PAD_AXIS_TRIGGER_R)
            continue;   // printed with its button, above
        for (u32 j = 0; j < na; j++)
        {
            if (axes[j].padAxis != kKeyAxes[i].axis)
                continue;
            OSReport("[port] input:   %-18s = %-12s -> %s %s\n", kKeyDefaults[kKeyAxes[i].key].key,
                     key_name((SDL_Scancode)axes[j].scancode), PADGetAxisName(kKeyAxes[i].axis),
                     PADGetAxisDirectionLabel(kKeyAxes[i].axis));
        }
    }
}

// Sentinels for a value that is a trigger rather than a button.
const int kPadNativeInvalid = -1;
const int kPadNativeLeftTrigger = -2;
const int kPadNativeRightTrigger = -3;

// -1000 means "the name was not understood", which is different from unset.
const int kPadNativeBad = -1000;

int parse_pad_button(const char* v)
{
    if (str_ieq(v, "lefttrigger"))
        return kPadNativeLeftTrigger;
    if (str_ieq(v, "righttrigger"))
        return kPadNativeRightTrigger;

    const SDL_GamepadButton b = SDL_GetGamepadButtonFromString(v);
    if (b == SDL_GAMEPAD_BUTTON_INVALID)
        return kPadNativeBad;
    return (int)b;
}

struct PadButtonRow
{
    const char* key;
    const char* env;
    const char* def;   // for the warning text only; an unset key is not applied
    PADButton pad;
};

// clang-format off
const PadButtonRow kPadButtons[] = {
    { "pad_a",          "STRIKERS_PAD_A",          "a",             PAD_BUTTON_A },
    { "pad_b",          "STRIKERS_PAD_B",          "b",             PAD_BUTTON_B },
    { "pad_x",          "STRIKERS_PAD_X",          "x",             PAD_BUTTON_X },
    { "pad_y",          "STRIKERS_PAD_Y",          "y",             PAD_BUTTON_Y },
    { "pad_z",          "STRIKERS_PAD_Z",          "rightshoulder", PAD_TRIGGER_Z },
    { "pad_start",      "STRIKERS_PAD_START",      "start",         PAD_BUTTON_START },
    { "pad_l",          "STRIKERS_PAD_L",          "lefttrigger",   PAD_TRIGGER_L },
    { "pad_r",          "STRIKERS_PAD_R",          "righttrigger",  PAD_TRIGGER_R },
    { "pad_dpad_up",    "STRIKERS_PAD_DPAD_UP",    "dpup",          PAD_BUTTON_UP },
    { "pad_dpad_down",  "STRIKERS_PAD_DPAD_DOWN",  "dpdown",        PAD_BUTTON_DOWN },
    { "pad_dpad_left",  "STRIKERS_PAD_DPAD_LEFT",  "dpleft",        PAD_BUTTON_LEFT },
    { "pad_dpad_right", "STRIKERS_PAD_DPAD_RIGHT", "dpright",       PAD_BUTTON_RIGHT },
};
// clang-format on

const unsigned kPadButtonCount = sizeof kPadButtons / sizeof kPadButtons[0];

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// A fraction of the stick's radius, in the raw SDL axis units Aurora compares against
// (SDL_JOYSTICK_AXIS_MAX).
u16 axis_fraction(float f) { return (u16)(clampf(f, 0.0f, 1.0f) * 32767.0f); }

void set_axis(u32 port, PADAxis axis, SDL_GamepadAxis native, PADAxisSign sign)
{
    PADAxisMapping m;
    m.nativeAxis.nativeAxis = (int32_t)native;
    m.nativeAxis.sign = sign;
    m.nativeButton = SDL_GAMEPAD_BUTTON_INVALID;
    m.padAxis = axis;
    PADSetAxisMapping(port, m);
}

void report_gamepad(u32 port)
{
    u32 nb = 0, na = 0;
    const PADButtonMapping* buttons = PADGetButtonMappings(port, &nb);
    const PADAxisMapping* axes = PADGetAxisMappings(port, &na);
    if (buttons == nullptr)
    {
        OSReport("[port] input: pad port %u: no controller\n", port);
        return;
    }

    const char* name = PADGetName(port);
    OSReport("[port] input: pad port %u: %s\n", port, name != nullptr ? name : "(unnamed)");
    for (u32 i = 0; i < nb; i++)
    {
        const char* native = buttons[i].nativeButton == PAD_NATIVE_BUTTON_INVALID
                                 ? "(trigger axis)"
                                 : PADGetNativeButtonName(buttons[i].nativeButton);
        OSReport("[port] input:   %-6s <- %s\n", PADGetButtonName(buttons[i].padButton),
                 native != nullptr ? native : "(none)");
    }
    for (u32 i = 0; i < na; i++)
    {
        const char* native = PADGetNativeAxisName(axes[i].nativeAxis);
        OSReport("[port] input:   %-10s <- %s%s\n", PADGetAxisName(axes[i].padAxis),
                 axes[i].nativeAxis.sign == AXIS_SIGN_NEGATIVE ? "-" : "+",
                 native != nullptr ? native : "(none)");
    }

    if (const PADDeadZones* dz = PADGetDeadZones(port))
    {
        OSReport("[port] input:   deadzone %.2f stick / %.2f c-stick%s, trigger %.2f%s\n",
                 (double)dz->stickDeadZone / 32767.0, (double)dz->substickDeadZone / 32767.0,
                 dz->useDeadzones ? "" : " (off)",
                 (double)dz->leftTriggerActivationZone / 32767.0,
                 dz->emulateTriggers ? "" : " (digital)");
    }

    u16 low = 0, high = 0;
    if (PADGetRumbleIntensity(port, &low, &high))
        OSReport("[port] input:   rumble %u%%\n", (unsigned)((low * 100u + 32767u) / 65535u));
    else
        OSReport("[port] input:   rumble not adjustable on this device\n");
}

// A typo in a pad_* value is reported whether or not a controller is plugged in: the person who
// mistyped it is the one without a working pad.
void check_pad_config()
{
    for (unsigned i = 0; i < kPadButtonCount; i++)
    {
        const char* v = input_cfg(kPadButtons[i].env);
        if (v == nullptr)
            continue;

        const int native = parse_pad_button(v);
        if (native == kPadNativeBad)
        {
            OSReport("[port] input: %s = \"%s\" is not a pad button; keeping %s\n",
                     kPadButtons[i].key, v, kPadButtons[i].def);
        }
        else if ((native == kPadNativeLeftTrigger || native == kPadNativeRightTrigger) &&
                 kPadButtons[i].pad != PAD_TRIGGER_L && kPadButtons[i].pad != PAD_TRIGGER_R)
        {
            OSReport("[port] input: %s = \"%s\": only pad_l and pad_r can be a trigger; "
                     "keeping %s\n",
                     kPadButtons[i].key, v, kPadButtons[i].def);
        }
    }
}

void apply_gamepad(u32 port, bool report)
{
    // Ask for the mappings before changing any, because that is what makes Aurora load them:
    // __PADLoadMapping is deferred to the first read and overwrites the whole table.
    u32 count = 0;
    if (PADGetButtonMappings(port, &count) == nullptr)
        return;   // nothing on this port

    bool triggerMapped = false;

    for (unsigned i = 0; i < kPadButtonCount; i++)
    {
        const char* v = input_cfg(kPadButtons[i].env);
        if (v == nullptr)
            continue;   // the file said nothing; the device default stands

        const int native = parse_pad_button(v);
        if (native == kPadNativeBad)
            continue;   // named at init by check_pad_config, once, not per port

        PADButtonMapping m;
        m.padButton = kPadButtons[i].pad;

        if (native == kPadNativeLeftTrigger || native == kPadNativeRightTrigger)
        {
            // Only L and R can come from a trigger: emulateTriggers is the only path from an analog
            // pull to a digital bit, and it knows about exactly those two.
            if (m.padButton != PAD_TRIGGER_L && m.padButton != PAD_TRIGGER_R)
                continue;
            m.nativeButton = PAD_NATIVE_BUTTON_INVALID;
            set_axis(port, m.padButton == PAD_TRIGGER_L ? PAD_AXIS_TRIGGER_L : PAD_AXIS_TRIGGER_R,
                     native == kPadNativeLeftTrigger ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER
                                                     : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
                     AXIS_SIGN_POSITIVE);
            triggerMapped = true;
        }
        else
        {
            m.nativeButton = (u32)native;
        }

        PADSetButtonMapping(port, m);
    }

    // Sticks are not remappable beyond the swap; there is nothing else on a pad to move them to.
    if (const char* v = input_cfg("STRIKERS_PAD_SWAP_STICKS"))
    {
        const bool swap = !cfg_off(v);
        const SDL_GamepadAxis mainX = swap ? SDL_GAMEPAD_AXIS_RIGHTX : SDL_GAMEPAD_AXIS_LEFTX;
        const SDL_GamepadAxis mainY = swap ? SDL_GAMEPAD_AXIS_RIGHTY : SDL_GAMEPAD_AXIS_LEFTY;
        const SDL_GamepadAxis subX = swap ? SDL_GAMEPAD_AXIS_LEFTX : SDL_GAMEPAD_AXIS_RIGHTX;
        const SDL_GamepadAxis subY = swap ? SDL_GAMEPAD_AXIS_LEFTY : SDL_GAMEPAD_AXIS_RIGHTY;
        // SDL's gamepad y-axis is inverted from the GameCube's, which is why the positive direction
        // takes the negative sign.
        set_axis(port, PAD_AXIS_LEFT_X_POS, mainX, AXIS_SIGN_POSITIVE);
        set_axis(port, PAD_AXIS_LEFT_X_NEG, mainX, AXIS_SIGN_NEGATIVE);
        set_axis(port, PAD_AXIS_LEFT_Y_POS, mainY, AXIS_SIGN_NEGATIVE);
        set_axis(port, PAD_AXIS_LEFT_Y_NEG, mainY, AXIS_SIGN_POSITIVE);
        set_axis(port, PAD_AXIS_RIGHT_X_POS, subX, AXIS_SIGN_POSITIVE);
        set_axis(port, PAD_AXIS_RIGHT_X_NEG, subX, AXIS_SIGN_NEGATIVE);
        set_axis(port, PAD_AXIS_RIGHT_Y_POS, subY, AXIS_SIGN_NEGATIVE);
        set_axis(port, PAD_AXIS_RIGHT_Y_NEG, subY, AXIS_SIGN_POSITIVE);
    }

    if (PADDeadZones* dz = PADGetDeadZones(port))
    {
        if (const char* v = input_cfg("STRIKERS_PAD_DEADZONE"))
        {
            const float f = clampf((float)std::atof(v), 0.0f, 0.9f);
            dz->useDeadzones = f > 0.0f;
            dz->stickDeadZone = axis_fraction(f);
            dz->substickDeadZone = axis_fraction(f);
        }
        if (const char* v = input_cfg("STRIKERS_PAD_TRIGGER_THRESHOLD"))
        {
            const u16 z = axis_fraction((float)std::atof(v));
            dz->leftTriggerActivationZone = z;
            dz->rightTriggerActivationZone = z;
        }
        // A GameCube adapter and an NSO GameCube pad arrive with emulateTriggers off, because they
        // have real digital L and R.
        if (triggerMapped)
            dz->emulateTriggers = true;
    }

    {
        const char* on = input_cfg("STRIKERS_PAD_RUMBLE");
        const char* strength = input_cfg("STRIKERS_PAD_RUMBLE_STRENGTH");
        if (on != nullptr && cfg_off(on))
        {
            PADSetRumbleIntensity(port, 0, 0);
        }
        else if (strength != nullptr)
        {
            // `pad_rumble = 1` on its own says "on", not "as hard as it goes", so it leaves the
            // device's own intensity alone.
            int pct = std::atoi(strength);
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            const u16 v = (u16)((65535 * pct) / 100);
            PADSetRumbleIntensity(port, v, v);
        }
    }

    if (report)
        report_gamepad(port);
}

// A gamepad that is not there.
SDL_JoystickID s_fakePad = 0;
SDL_Joystick* s_fakePadJoystick = nullptr;

void attach_fake_pad()
{
    if (std::getenv("STRIKERS_FAKE_PAD") == nullptr || s_fakePad != 0)
        return;

    // A headless run has no window focus, and SDL stops updating joysticks when the app is not
    // focused, so without this the press lands or does not depending on where the mouse was, which
    // is worse than not testing.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = (Uint16)SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.vendor_id = 0x045E;
    desc.product_id = 0x02FD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = 15;   // south .. dpright, the set every pad has
    desc.button_mask = (1u << 15) - 1u;
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1u;
    desc.name = "Strikers Test Pad";

    s_fakePad = SDL_AttachVirtualJoystick(&desc);
    if (s_fakePad == 0)
    {
        OSReport("[port] input: STRIKERS_FAKE_PAD: %s\n", SDL_GetError());
        return;
    }
    // Held open so its buttons can be driven; Aurora opens the *gamepad* for the same device when
    // SDL announces it, which is a separate handle.
    s_fakePadJoystick = SDL_OpenJoystick(s_fakePad);
    OSReport("[port] input: attached a virtual pad, instance %u\n", (unsigned)s_fakePad);
}

void update_fake_pad(unsigned long frame, unsigned long holdFrames)
{
    if (s_fakePadJoystick == nullptr)
        return;
    const char* spec = std::getenv("STRIKERS_FAKE_PAD_PRESS");
    if (spec == nullptr || *spec == '\0')
        return;

    for (const char* p = spec; *p != '\0';)
    {
        const char* at = std::strchr(p, '@');
        if (at == nullptr)
            break;

        char name[32];
        size_t len = (size_t)(at - p);
        if (len >= sizeof name)
            len = sizeof name - 1;
        std::memcpy(name, p, len);
        name[len] = '\0';

        const SDL_GamepadButton b = SDL_GetGamepadButtonFromString(name);
        if (b != SDL_GAMEPAD_BUTTON_INVALID)
        {
            const unsigned long when = std::strtoul(at + 1, nullptr, 10);
            const bool down = frame >= when && frame < when + holdFrames;
            const bool ok = SDL_SetJoystickVirtualButton(s_fakePadJoystick, (int)b, down);
            if (down && frame == when)
                OSReport("[port] input: fake pad %s down: %s\n", name,
                         ok ? "ok" : SDL_GetError());
        }

        const char* comma = std::strchr(at, ',');
        if (comma == nullptr)
            break;
        p = comma + 1;
    }
}

// Which device is on each port, so a connect can be noticed without a callback Aurora does not
// offer to C. -2 is "not looked at yet", so the first pass reports what is already there.
s32 s_padIndex[PAD_CHANMAX] = { -2, -2, -2, -2 };

void poll_controllers(bool report)
{
    for (u32 p = 0; p < PAD_CHANMAX; p++)
    {
        const s32 idx = PADGetIndexForPort(p);
        if (idx == s_padIndex[p])
            continue;
        const bool firstLook = s_padIndex[p] == -2;
        s_padIndex[p] = idx;
        if (idx >= 0)
            apply_gamepad(p, report);
        else if (firstLook && report)
            OSReport("[port] input: pad port %u: no controller\n", p);
    }
}

}   // namespace

extern "C" void PortInstallKeyboardBindings(void)
{
    const char* keyboard = input_cfg("STRIKERS_KEYBOARD");
    const bool keyboardOff = keyboard != nullptr && cfg_off(keyboard);

    if (!keyboardOff)
    {
        resolve_keys();
        port_check_bindings_distinct();
        install_keyboard();
    }

    check_pad_config();
    attach_fake_pad();

    // A controller plugged in before the game started is added by Aurora's init, so there can
    // already be one here; the rest arrive as AURORA_CONTROLLER_ADDED and are picked up per frame.
    poll_controllers(probe_pad());

    if (probe_pad())
        report_keyboard();
}

// Synthetic input: a headless run has nobody at the keyboard, so the pad is driven from here.

#include <cstring>

namespace
{

struct NamedButton
{
    const char* name;
    PADButton button;
};

const NamedButton kPressNames[] = {
    { "A", PAD_BUTTON_A },       { "B", PAD_BUTTON_B },         { "X", PAD_BUTTON_X },
    { "Y", PAD_BUTTON_Y },       { "Z", PAD_TRIGGER_Z },        { "L", PAD_TRIGGER_L },
    { "R", PAD_TRIGGER_R },      { "START", PAD_BUTTON_START }, { "UP", PAD_BUTTON_UP },
    { "DOWN", PAD_BUTTON_DOWN }, { "LEFT", PAD_BUTTON_LEFT },   { "RIGHT", PAD_BUTTON_RIGHT },
};

PADButton button_from_name(const char* name, size_t len)
{
    for (size_t i = 0; i < sizeof kPressNames / sizeof kPressNames[0]; i++)
    {
        const size_t n = std::strlen(kPressNames[i].name);
        if (n == len && std::strncmp(name, kPressNames[i].name, n) == 0)
            return kPressNames[i].button;
    }
    return 0;
}

// The frame each scene was first entered on, so a press can be anchored to a screen rather than to
// a stopwatch.
const int kMaxScene = 128;
unsigned long s_sceneEntryFrame[kMaxScene];
unsigned long s_currentFrame = 0;

// Resolve one spec's trigger frame. Returns false if it can never fire yet, a scene that has not
// been entered.
bool trigger_frame(const char* at, unsigned long* out)
{
    if (at[1] != 's')
    {
        *out = std::strtoul(at + 1, nullptr, 10);
        return true;
    }

    char* end = nullptr;
    const unsigned long scene = std::strtoul(at + 2, &end, 10);
    if (scene >= (unsigned long)kMaxScene || s_sceneEntryFrame[scene] == 0)
        return false;

    unsigned long delay = 0;
    if (end != nullptr && *end == '+')
        delay = std::strtoul(end + 1, nullptr, 10);

    *out = s_sceneEntryFrame[scene] + delay;
    return true;
}

// How many frames a synthetic press is held.
const unsigned long kHoldFrames = 6;

// Pressing a *key*, rather than a button.
PADButton fake_key_state(unsigned long frame, int* lx, int* ly, int* rx, int* ry, int* tl, int* tr)
{
    const char* spec = std::getenv("STRIKERS_FAKE_KEY");
    if (spec == nullptr || *spec == '\0')
        return 0;

    u32 nb = 0, na = 0;
    const PADKeyButtonBinding* buttons = PADGetKeyButtonBindings(0, &nb);
    const PADKeyAxisBinding* axes = PADGetKeyAxisBindings(0, &na);
    if (buttons == nullptr)
        return 0;

    PADButton held = 0;
    for (const char* p = spec; *p != '\0';)
    {
        const char* at = std::strchr(p, '@');
        if (at == nullptr)
            break;

        char name[64];
        size_t len = (size_t)(at - p);
        if (len >= sizeof name)
            len = sizeof name - 1;
        std::memcpy(name, p, len);
        name[len] = '\0';

        unsigned long when = 0;
        const SDL_Scancode code = scancode_from_name(name);
        if (code != SDL_SCANCODE_UNKNOWN && trigger_frame(at, &when) && frame >= when &&
            frame < when + kHoldFrames)
        {
            for (u32 i = 0; i < nb; i++)
            {
                if (buttons[i].scancode == (s32)code)
                    held |= buttons[i].padButton;
            }
            for (u32 i = 0; axes != nullptr && i < na; i++)
            {
                if (axes[i].scancode != (s32)code)
                    continue;
                switch (axes[i].padAxis)
                {
                case PAD_AXIS_LEFT_X_POS:  *lx += 127; break;
                case PAD_AXIS_LEFT_X_NEG:  *lx -= 127; break;
                case PAD_AXIS_LEFT_Y_POS:  *ly += 127; break;
                case PAD_AXIS_LEFT_Y_NEG:  *ly -= 127; break;
                case PAD_AXIS_RIGHT_X_POS: *rx += 127; break;
                case PAD_AXIS_RIGHT_X_NEG: *rx -= 127; break;
                case PAD_AXIS_RIGHT_Y_POS: *ry += 127; break;
                case PAD_AXIS_RIGHT_Y_NEG: *ry -= 127; break;
                case PAD_AXIS_TRIGGER_L:   *tl = 255; break;
                case PAD_AXIS_TRIGGER_R:   *tr = 255; break;
                default: break;
                }
            }
        }

        const char* comma = std::strchr(at, ',');
        if (comma == nullptr)
            break;
        p = comma + 1;
    }
    return held;
}

}   // namespace

// Called from BaseGameSceneManager::Push. Records the first entry into each scene so `@sN` specs
// have something to anchor to; frame 0 doubles as "not entered", which costs nothing because no
// scene is pushed on frame 0.
extern "C" void PortNoteSceneEntered(int scene)
{
    if (scene >= 0 && scene < kMaxScene && s_sceneEntryFrame[scene] == 0)
        s_sceneEntryFrame[scene] = s_currentFrame != 0 ? s_currentFrame : 1;
}

#include <cstdio>

namespace
{

struct PadFrame
{
    unsigned long frame;
    PADStatus status;
};

bool pad_state_equal(const PADStatus& a, const PADStatus& b)
{
    return a.button == b.button && a.stickX == b.stickX && a.stickY == b.stickY &&
           a.substickX == b.substickX && a.substickY == b.substickY &&
           a.triggerLeft == b.triggerLeft && a.triggerRight == b.triggerRight;
}

// Replay is a flat array walked with a cursor, because it is only ever read forwards.
const size_t kMaxPadFrames = 65536;
PadFrame* s_replay = nullptr;
size_t s_replayCount = 0;
size_t s_replayCursor = 0;
signed char s_replayState = 0;   // 0 unknown, 1 loaded, -1 unavailable

FILE* s_recordFile = nullptr;
signed char s_recordState = 0;
PADStatus s_lastRecorded;
bool s_haveLastRecorded = false;
char s_recordPath[512];
char s_replayPath[512];

// Frame numbers in a recording are absolute, counted from boot.
unsigned long s_replayOffset = 0;

// The menu's press: a button mask held for a number of frames.
PADButton s_menuHeld = 0;
int s_menuHeldFrames = 0;

// And the same for the stick, in the pad's own -127..127 rather than the caller's -100..100, so the
// merge below adds and clamps nothing.
int s_menuStickX = 0, s_menuStickY = 0;
int s_menuStickFrames = 0;

void load_replay_from(const char* path)
{
    if (path == nullptr)
    {
        s_replayState = -1;
        return;
    }

    FILE* f = std::fopen(path, "r");
    if (f == nullptr)
    {
        OSReport("[port] cannot open replay file '%s'\n", path);
        s_replayState = -1;
        return;
    }

    s_replay = (PadFrame*)std::calloc(kMaxPadFrames, sizeof(PadFrame));
    if (s_replay == nullptr)
    {
        std::fclose(f);
        s_replayState = -1;
        return;
    }

    char line[256];
    while (std::fgets(line, sizeof line, f) != nullptr && s_replayCount < kMaxPadFrames)
    {
        if (line[0] == '#' || line[0] == '\n')
            continue;

        unsigned long frame;
        unsigned int button;
        int sx, sy, cx, cy, tl, tr;
        if (std::sscanf(line, "%lu %u %d %d %d %d %d %d", &frame, &button, &sx, &sy, &cx, &cy, &tl,
                        &tr) != 8)
            continue;

        PadFrame& e = s_replay[s_replayCount++];
        e.frame = frame;
        std::memset(&e.status, 0, sizeof e.status);
        e.status.button = (u16)button;
        e.status.stickX = (s8)sx;
        e.status.stickY = (s8)sy;
        e.status.substickX = (s8)cx;
        e.status.substickY = (s8)cy;
        e.status.triggerLeft = (u8)tl;
        e.status.triggerRight = (u8)tr;
        e.status.err = PAD_ERR_NONE;
    }
    std::fclose(f);

    OSReport("[port] replaying %zu pad states from '%s'\n", s_replayCount, path);
    std::strncpy(s_replayPath, path, sizeof s_replayPath - 1);
    s_replayState = 1;
}

void load_replay() { load_replay_from(std::getenv("STRIKERS_REPLAY_INPUT")); }

// Returns true if replay drove the pad this frame.
bool replay_pad(unsigned long frame)
{
    if (s_replayState == 0)
        load_replay();
    if (s_replayState != 1 || s_replayCount == 0)
        return false;

    frame += s_replayOffset;
    while (s_replayCursor + 1 < s_replayCount && s_replay[s_replayCursor + 1].frame <= frame)
    {
        s_replayCursor++;
    }

    // Before the first recorded state there is nothing to say, so leave the pad alone rather than
    // pinning it to neutral; a recording that starts at frame 900 should not stop the boot doing
    // whatever it does before then.
    if (frame < s_replay[0].frame)
        return false;

    PADSetVirtualStatus(0, &s_replay[s_replayCursor].status);
    return true;
}

bool open_recording(const char* path)
{
    if (path == nullptr)
    {
        s_recordState = -1;
        return false;
    }
    s_recordFile = std::fopen(path, "w");
    if (s_recordFile == nullptr)
    {
        OSReport("[port] cannot write input recording to '%s'\n", path);
        s_recordState = -1;
        return false;
    }
    std::fprintf(s_recordFile,
                 "# strikers pad recording\n"
                 "# frame button stickX stickY substickX substickY triggerL triggerR\n");
    OSReport("[port] recording input to '%s'\n", path);
    std::strncpy(s_recordPath, path, sizeof s_recordPath - 1);
    s_haveLastRecorded = false;
    s_recordState = 1;
    return true;
}

void record_pad(unsigned long frame)
{
    if (s_recordState == 0)
        open_recording(std::getenv("STRIKERS_RECORD_INPUT"));
    if (s_recordState != 1)
        return;

    // Read the pad the same way the game is about to.
    PADStatus status[PAD_CHANMAX];
    PADRead(status);

    if (status[0].err != PAD_ERR_NONE)
        return;

    if (s_haveLastRecorded && pad_state_equal(status[0], s_lastRecorded))
        return;

    std::fprintf(s_recordFile, "%lu %u %d %d %d %d %d %d\n", frame, (unsigned)status[0].button,
                 (int)status[0].stickX, (int)status[0].stickY, (int)status[0].substickX,
                 (int)status[0].substickY, (int)status[0].triggerLeft, (int)status[0].triggerRight);
    std::fflush(s_recordFile);   // a run that ends in a crash still has its input

    s_lastRecorded = status[0];
    s_haveLastRecorded = true;
}

}   // namespace

extern "C" void PortUpdateSyntheticInput(unsigned long frame)
{
    s_currentFrame = frame;

    // A controller can arrive at any time, and its mapping is Aurora's until the file's is put over
    // it.
    poll_controllers(probe_pad());
    update_fake_pad(frame, kHoldFrames);

    record_pad(frame);

    // A recording is a complete description of the input, so it wins outright.
    if (replay_pad(frame))
        return;

    PADButton held = 0;
    if (s_menuHeldFrames > 0)
    {
        held |= s_menuHeld;
        s_menuHeldFrames--;
    }

    const char* spec = std::getenv("STRIKERS_AUTOPRESS");
    if (spec == nullptr)
        spec = "";

    for (const char* p = spec; *p != '\0';)
    {
        const char* at = std::strchr(p, '@');
        if (at == nullptr)
            break;
        const PADButton button = button_from_name(p, (size_t)(at - p));
        unsigned long when = 0;
        if (button != 0 && trigger_frame(at, &when) && frame >= when && frame < when + kHoldFrames)
            held |= button;

        const char* comma = std::strchr(at, ',');
        if (comma == nullptr)
            break;
        p = comma + 1;
    }

    int lx = 0, ly = 0, rx = 0, ry = 0, tl = 0, tr = 0;
    if (s_menuStickFrames > 0)
    {
        lx += s_menuStickX;
        ly += s_menuStickY;
        s_menuStickFrames--;
    }
    held |= fake_key_state(frame, &lx, &ly, &rx, &ry, &tl, &tr);

    if (held == 0 && lx == 0 && ly == 0 && rx == 0 && ry == 0 && tl == 0 && tr == 0)
    {
        PADClearVirtualStatus(0);
        return;
    }

    PADStatus status;
    std::memset(&status, 0, sizeof status);
    status.button = held;
    status.stickX = (s8)(lx < -127 ? -127 : (lx > 127 ? 127 : lx));
    status.stickY = (s8)(ly < -127 ? -127 : (ly > 127 ? 127 : ly));
    status.substickX = (s8)(rx < -127 ? -127 : (rx > 127 ? 127 : rx));
    status.substickY = (s8)(ry < -127 ? -127 : (ry > 127 ? 127 : ry));
    status.triggerLeft = (u8)tl;
    status.triggerRight = (u8)tr;
    status.err = PAD_ERR_NONE;
    PADSetVirtualStatus(0, &status);
}

// The debug menu's handles on all of the above.

extern "C" int PortInputRecordStart(const char* path)
{
    if (s_recordState == 1)
        PortInputRecordStop();
    s_recordState = 0;
    return open_recording(path) ? 1 : 0;
}

extern "C" void PortInputRecordStop(void)
{
    if (s_recordFile != nullptr)
    {
        std::fclose(s_recordFile);
        s_recordFile = nullptr;
        OSReport("[port] recording stopped: '%s'\n", s_recordPath);
    }
    s_recordState = -1;
}

extern "C" const char* PortInputRecordPath(void)
{
    return s_recordState == 1 ? s_recordPath : nullptr;
}

extern "C" int PortInputReplayStart(const char* path)
{
    if (s_replay != nullptr)
    {
        std::free(s_replay);
        s_replay = nullptr;
    }
    s_replayCount = 0;
    s_replayCursor = 0;
    s_replayOffset = 0;
    load_replay_from(path);
    if (s_replayState != 1 || s_replayCount == 0)
        return 0;
    // Shift so the first recorded state fires on the next frame.
    if (s_replay[0].frame > s_currentFrame + 1)
        s_replayOffset = s_replay[0].frame - (s_currentFrame + 1);
    return 1;
}

extern "C" void PortInputReplayStop(void)
{
    if (s_replayState == 1)
        PADClearVirtualStatus(0);
    s_replayState = -1;
    s_replayCount = 0;
    s_replayCursor = 0;
}

extern "C" int PortInputReplayStatus(unsigned long* count, unsigned long* cursor,
                                     unsigned long* firstFrame, unsigned long* lastFrame,
                                     const char** path)
{
    const bool live = (s_replayState == 1 && s_replayCount != 0);
    if (count) *count = live ? (unsigned long)s_replayCount : 0;
    if (cursor) *cursor = live ? (unsigned long)s_replayCursor : 0;
    if (firstFrame) *firstFrame = live ? s_replay[0].frame - s_replayOffset : 0;
    if (lastFrame) *lastFrame = live ? s_replay[s_replayCount - 1].frame - s_replayOffset : 0;
    if (path) *path = live ? s_replayPath : nullptr;
    return live ? 1 : 0;
}

extern "C" void PortInputPress(unsigned int buttons, int frames)
{
    s_menuHeld = (PADButton)buttons;
    s_menuHeldFrames = frames > 0 ? frames : (int)kHoldFrames;
}

extern "C" void PortInputStick(int x, int y, int frames)
{
    const int cx = x < -100 ? -100 : (x > 100 ? 100 : x);
    const int cy = y < -100 ? -100 : (y > 100 ? 100 : y);
    s_menuStickX = cx * 127 / 100;
    s_menuStickY = cy * 127 / 100;
    s_menuStickFrames = frames > 0 ? frames : (int)kHoldFrames;
}

extern "C" unsigned int PortInputButtonFromName(const char* name, int len)
{
    if (name == nullptr)
        return 0;
    return (unsigned int)button_from_name(name, len < 0 ? std::strlen(name) : (size_t)len);
}

extern "C" unsigned long PortInputFrame(void) { return s_currentFrame; }

#endif   // PORT_USE_AURORA
