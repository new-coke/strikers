// The online panel: ImGui screens over the session's polled state that only call its commands.

#include "port/lockstep.h"
#include "port/online_panel.h"
#include "port/prompts.h"
#include "port/online_game.h"
#include "port/online.h"
#include "port/region.h"

#include "panel_edit.h"
#include "panel_font.h"
#include "panel_text.h"
#include "pipe.h"
#include "strikers_version.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include <imgui.h>
#include <imgui_internal.h>

#include <aurora/imgui.h>
// Declared here: aurora's gfx.hpp needs WebGPU headers the game target does not have.
namespace aurora
{
namespace gfx
{
void synchronize();
}
}   // namespace aurora
#include <dolphin/pad.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_stdinc.h>
#if __has_include(<SDL3/SDL_clipboard.h>)
#include <SDL3/SDL_clipboard.h>
#define PANEL_HAS_CLIPBOARD 1
#endif
#if !defined(__SWITCH__)
#include <backends/imgui_impl_sdl3.h>
#endif

#if defined(__SWITCH__)
// No platform backend feeds ImGui typed text on the Switch, so the wheel is the only entry there.
#define PANEL_HAS_TYPING 0
#else
#define PANEL_HAS_TYPING 1
#endif

namespace online_panel
{
namespace
{

const ImVec4 kInk(0.047f, 0.071f, 0.110f, 1.0f);
const ImVec4 kRaised(0.083f, 0.137f, 0.192f, 1.0f);
const ImVec4 kButton(0.100f, 0.221f, 0.294f, 1.0f);
const ImVec4 kButtonHover(0.180f, 0.380f, 0.486f, 1.0f);
const ImVec4 kButtonActive(0.237f, 0.466f, 0.584f, 1.0f);
const ImVec4 kText(0.957f, 0.965f, 0.980f, 1.0f);
const ImVec4 kMuted(0.694f, 0.784f, 0.835f, 1.0f);
const ImVec4 kGold(1.000f, 0.690f, 0.063f, 1.0f);
const ImVec4 kGreen(0.380f, 0.827f, 0.259f, 1.0f);
const ImVec4 kRed(1.000f, 0.365f, 0.325f, 1.0f);
const ImVec4 kOrange(1.000f, 0.604f, 0.200f, 1.0f);
const ImVec4 kHome(0.259f, 0.557f, 1.000f, 1.0f);

ImU32 col(const ImVec4& c, float alpha = 1.0f)
{
    return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * alpha));
}

ImGuiStyle make_style(float px)
{
    ImGuiStyle st;
    ImGui::StyleColorsDark(&st);
    const float u = px / 20.0f;
    st.WindowPadding = ImVec2(24.0f * u, 18.0f * u);
    st.FramePadding = ImVec2(14.0f * u, 9.0f * u);
    st.ItemSpacing = ImVec2(14.0f * u, 14.0f * u);
    st.ItemInnerSpacing = ImVec2(8.0f * u, 6.0f * u);
    st.WindowRounding = 0.0f;
    st.ChildRounding = 0.0f;
    st.FrameRounding = 0.0f;
    st.PopupRounding = 10.0f * u;
    st.GrabRounding = 8.0f * u;
    st.ScrollbarSize = 10.0f * u;
    st.ScrollbarRounding = 5.0f * u;
    st.WindowBorderSize = 0.0f;
    st.ChildBorderSize = 0.0f;
    st.PopupBorderSize = 0.0f;
    st.FrameBorderSize = 0.0f;
    st.WindowMinSize = ImVec2(8.0f, 8.0f);

    ImVec4* c = st.Colors;
    c[ImGuiCol_Text] = kText;
    c[ImGuiCol_TextDisabled] = kMuted;
    c[ImGuiCol_WindowBg] = kInk;
    c[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_PopupBg] = kRaised;
    c[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_FrameBg] = kButton;
    c[ImGuiCol_FrameBgHovered] = kButtonHover;
    c[ImGuiCol_FrameBgActive] = kButtonActive;
    c[ImGuiCol_Button] = kButton;
    c[ImGuiCol_ButtonHovered] = kButtonHover;
    c[ImGuiCol_ButtonActive] = kButtonActive;
    c[ImGuiCol_Header] = kButton;
    c[ImGuiCol_HeaderHovered] = kButtonHover;
    c[ImGuiCol_HeaderActive] = kButtonActive;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_ScrollbarGrab] = kButtonHover;
    c[ImGuiCol_ScrollbarGrabHovered] = kButtonActive;
    c[ImGuiCol_ScrollbarGrabActive] = kGold;
    c[ImGuiCol_TextSelectedBg] = ImVec4(kGold.x, kGold.y, kGold.z, 0.35f);
    // Focus is drawn by each widget, thicker than ImGui's 2 px frame.
    c[ImGuiCol_NavCursor] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    return st;
}

// Body text covers Latin, Greek and Cyrillic for players' names; the large face covers addresses.
const ImWchar kTextRanges[] = { 0x0020, 0x017F, 0x0370, 0x03FF, 0x0400, 0x04FF, 0x2010,
                                0x205E, 0x20AC, 0x20AC, 0x2122, 0x2122, 0 };
const ImWchar kHeadRanges[] = { 0x0020, 0x017F, 0x2010, 0x2027, 0 };
const ImWchar kBigRanges[] = { 0x0020, 0x0020, 0x002D, 0x002E, 0x0030, 0x003A, 0x0041, 0x005B,
                               0x005D, 0x005D, 0x005F, 0x005F, 0x0061, 0x007A, 0 };

struct FontSet
{
    int px;
    float density;
    ImFontAtlas* atlas;
    ImFont* body;
    ImFont* head;
    ImFont* big;
};

// Aurora has no call to free an ImGui texture, so a few sizes are kept and reused.
const int kFontSets = 4;
FontSet s_sets[kFontSets];
FontSet* s_fonts;
ImGuiStyle s_style;
int s_pendingPx;
double s_pendingSince;

int body_px(float displayHeight)
{
    int px = (int)std::lround(displayHeight * 0.027f);
    return px < 14 ? 14 : (px > 48 ? 48 : px);
}

// A separate atlas with its own texture, so io.Fonts, locked for the frame, is never touched.
FontSet* load_fonts(int px, float density)
{
    for (FontSet& f : s_sets)
        if (f.atlas != nullptr && f.px == px && f.density == density)
            return &f;
    FontSet* slot = nullptr;
    for (FontSet& f : s_sets)
        if (f.atlas == nullptr)
        {
            slot = &f;
            break;
        }
    if (slot == nullptr)
        return s_fonts;

    ImFontAtlas* atlas = IM_NEW(ImFontAtlas)();
    atlas->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
    ImFontConfig cfg;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = true;
    const float bodySize = (float)px * density;
    const float headSize = std::round((float)px * 1.45f) * density;
    const float bigSize = std::round((float)px * 3.0f) * density;
    ImFont* body =
        atlas->AddFontFromMemoryCompressedBase85TTF(kFontRobotoMedium, bodySize, &cfg, kTextRanges);
    ImFont* head =
        atlas->AddFontFromMemoryCompressedBase85TTF(kFontRobotoMedium, headSize, &cfg, kHeadRanges);
    ImFont* big =
        atlas->AddFontFromMemoryCompressedBase85TTF(kFontRobotoMedium, bigSize, &cfg, kBigRanges);
    unsigned char* pixels = nullptr;
    int w = 0, h = 0;
    if (body != nullptr && head != nullptr && big != nullptr)
        atlas->GetTexDataAsRGBA32(&pixels, &w, &h);
    if (pixels == nullptr)
    {
        IM_DELETE(atlas);
        return s_fonts;
    }
    aurora::gfx::synchronize();
    atlas->SetTexID(aurora_imgui_add_texture((uint32_t)w, (uint32_t)h, pixels));
    atlas->ClearTexData();
    std::fprintf(stderr, "[online] panel fonts at %d px x%.2g: atlas %dx%d\n", px, (double)density,
                 w, h);

    // Rasterised at the framebuffer's density, laid out in ImGui's points.
    body->Scale = head->Scale = big->Scale = 1.0f / density;
    *slot = FontSet{ px, density, atlas, body, head, big };
    return slot;
}

// Waits for the window to settle before building another size.
void update_fonts()
{
    const ImGuiIO& io = ImGui::GetIO();
    const float density =
        io.DisplayFramebufferScale.y >= 1.0f ? io.DisplayFramebufferScale.y : 1.0f;
    const int px = body_px(io.DisplaySize.y);
    if (s_fonts != nullptr && s_fonts->px == px && s_fonts->density == density)
    {
        s_pendingPx = 0;
        return;
    }
    const double now = ImGui::GetTime();
    if (s_fonts != nullptr)
    {
        if (s_pendingPx != px)
        {
            s_pendingPx = px;
            s_pendingSince = now;
            return;
        }
        if (now - s_pendingSince < 0.4)
            return;
    }
    FontSet* next = load_fonts(px, density);
    if (next != nullptr && next != s_fonts)
    {
        s_fonts = next;
        s_style = make_style((float)px);
    }
    s_pendingPx = 0;
}

// The main stick's directions and the Back button, beside the PAD_* bits.
const unsigned kStickLeft = 1u << 16;
const unsigned kStickRight = 1u << 17;
const unsigned kStickDown = 1u << 18;
const unsigned kStickUp = 1u << 19;
const unsigned kBackButton = 1u << 20;
const int kPadBits = 21;

const unsigned kLeft = PAD_BUTTON_LEFT | kStickLeft;
const unsigned kRight = PAD_BUTTON_RIGHT | kStickRight;
const unsigned kUp = PAD_BUTTON_UP | kStickUp;
const unsigned kDown = PAD_BUTTON_DOWN | kStickDown;
const unsigned kRepeating = kLeft | kRight | kUp | kDown;
const unsigned kDpad = PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT | PAD_BUTTON_UP | PAD_BUTTON_DOWN;
const unsigned kChord = PAD_TRIGGER_L | PAD_TRIGGER_R | PAD_TRIGGER_Z;

struct Device
{
    bool present;
    unsigned held;
    float x, y;   // main stick, -1 to 1, up positive
};

Device s_dev[PAD_CHANMAX];
unsigned s_prevHeld[PAD_CHANMAX];
unsigned s_pressed[PAD_CHANMAX];
float s_heldFor[PAD_CHANMAX][kPadBits];
float s_chordFor;
bool s_chordFired;

// Device ports that drive the panel, and what they pressed this frame while it had the pads.
unsigned s_navPorts = 0xF;
unsigned s_navPressed;

// The screen reads the pads itself and ImGui is fed none (the character wheels).
bool s_padOwned;
bool s_fedDown[5];
float s_fedStick[4];
bool s_feeding;
// Held when feeding began, so the press that changed screens does not press the next screen too.
unsigned s_heldAtFeed;

// Through Aurora's mapping: PADBlockInput does not hide the device, and its buttons are the game's.
Device read_device(u32 port)
{
    Device d = {};
    const s32 index = PADGetIndexForPort(port);
    SDL_Gamepad* pad = index >= 0 ? PADGetSDLGamepadForIndex((u32)index) : nullptr;
    if (pad == nullptr)
        return d;
    d.present = true;

    u32 count = 0;
    bool backMapped = false;
    if (const PADButtonMapping* map = PADGetButtonMappings(port, &count))
        for (u32 i = 0; i < count; i++)
        {
            if (map[i].nativeButton == PAD_NATIVE_BUTTON_INVALID)
                continue;
            backMapped = backMapped || map[i].nativeButton == (u32)SDL_GAMEPAD_BUTTON_BACK;
            if (SDL_GetGamepadButton(pad, (SDL_GamepadButton)map[i].nativeButton))
                d.held |= map[i].padButton;
        }

    float axis[PAD_AXIS_COUNT] = {};
    count = 0;
    if (const PADAxisMapping* map = PADGetAxisMappings(port, &count))
        for (u32 i = 0; i < count; i++)
        {
            const PADAxisMapping& m = map[i];
            float v = 0.0f;
            if (m.nativeAxis.nativeAxis != -1)
                v = (float)SDL_GetGamepadAxis(pad, (SDL_GamepadAxis)m.nativeAxis.nativeAxis) *
                    (float)m.nativeAxis.sign / 32767.0f;
            else if (m.nativeButton != -1 &&
                     SDL_GetGamepadButton(pad, (SDL_GamepadButton)m.nativeButton))
                v = 1.0f;
            if (m.padAxis < PAD_AXIS_COUNT)
                axis[m.padAxis] = v;
        }
    d.x = (axis[PAD_AXIS_LEFT_X_POS] - axis[PAD_AXIS_LEFT_X_NEG]) * 0.5f;
    d.y = (axis[PAD_AXIS_LEFT_Y_POS] - axis[PAD_AXIS_LEFT_Y_NEG]) * 0.5f;
    if (axis[PAD_AXIS_TRIGGER_L] > 0.6f)
        d.held |= PAD_TRIGGER_L;
    if (axis[PAD_AXIS_TRIGGER_R] > 0.6f)
        d.held |= PAD_TRIGGER_R;
    if (d.x < -0.5f)
        d.held |= kStickLeft;
    if (d.x > 0.5f)
        d.held |= kStickRight;
    if (d.y < -0.5f)
        d.held |= kStickDown;
    if (d.y > 0.5f)
        d.held |= kStickUp;
#if !defined(__SWITCH__)
    // Unless a game button has it, as the NSO GameCube pad's Z does; Minus is the Switch's stats.
    if (!backMapped && SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK))
        d.held |= kBackButton;
#else
    (void)backMapped;
#endif
    return d;
}

bool s_toggleRequest;
bool s_startPressed;

void poll_pads(float dt)
{
    const float kDelay = 0.38f, kRate = 0.09f;
    bool chordHeld = false;
    for (u32 p = 0; p < PAD_CHANMAX; p++)
    {
        s_dev[p] = read_device(p);
        const unsigned held = s_dev[p].held;
        unsigned pressed = held & ~s_prevHeld[p];
        for (int b = 0; b < kPadBits; b++)
        {
            const unsigned bit = 1u << b;
            if ((held & bit) == 0)
            {
                s_heldFor[p][b] = 0.0f;
                continue;
            }
            const float before = s_heldFor[p][b];
            const float after = before + dt;
            s_heldFor[p][b] = after;
            if ((kRepeating & bit) != 0 && after > kDelay &&
                std::floor((after - kDelay) / kRate) != std::floor((before - kDelay) / kRate))
                pressed |= bit;
        }
        s_pressed[p] = pressed;
        s_prevHeld[p] = held;
        if ((held & kChord) == kChord)
            chordHeld = true;
        if ((pressed & kBackButton) != 0 && PortOnlineGetState()->phase != ONLINE_PHASE_MATCH)
            s_toggleRequest = true;
        if ((pressed & PAD_BUTTON_START) != 0)
            s_startPressed = true;
    }

    // L, R and Z held together for 0.4 s: no game screen acts on the three at once.
    s_chordFor = chordHeld ? s_chordFor + dt : 0.0f;
    if (s_chordFor < 0.4f)
        s_chordFired = false;
    else if (!s_chordFired)
    {
        s_chordFired = true;
        s_toggleRequest = true;
    }
}

void feed_imgui(ImGuiIO& io, bool feed)
{
    unsigned held = 0;
    float x = 0.0f, y = 0.0f;
    if (feed)
    {
        for (u32 p = 0; p < PAD_CHANMAX; p++)
        {
            if ((s_navPorts & (1u << p)) == 0)
                continue;
            held |= s_dev[p].held;
            if (std::fabs(s_dev[p].x) > std::fabs(x))
                x = s_dev[p].x;
            if (std::fabs(s_dev[p].y) > std::fabs(y))
                y = s_dev[p].y;
        }
        s_heldAtFeed = s_feeding ? s_heldAtFeed & held : held;
        held &= ~s_heldAtFeed;
        if ((s_heldAtFeed & (kStickLeft | kStickRight)) != 0)
            x = 0.0f;
        if ((s_heldAtFeed & (kStickUp | kStickDown)) != 0)
            y = 0.0f;
        io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    }
    s_feeding = feed;

    const struct
    {
        ImGuiKey key;
        unsigned bit;
    } kKeys[] = {
        { ImGuiKey_GamepadFaceDown, PAD_BUTTON_A },
        { ImGuiKey_GamepadDpadUp, PAD_BUTTON_UP },
        { ImGuiKey_GamepadDpadDown, PAD_BUTTON_DOWN },
        { ImGuiKey_GamepadDpadLeft, PAD_BUTTON_LEFT },
        { ImGuiKey_GamepadDpadRight, PAD_BUTTON_RIGHT },
    };
    for (int i = 0; i < 5; i++)
    {
        const bool down = (held & kKeys[i].bit) != 0;
        if (down != s_fedDown[i])
        {
            io.AddKeyEvent(kKeys[i].key, down);
            s_fedDown[i] = down;
        }
    }

    const ImGuiKey kStick[4] = { ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight,
                                 ImGuiKey_GamepadLStickDown, ImGuiKey_GamepadLStickUp };
    const float raw[4] = { -x, x, -y, y };
    for (int i = 0; i < 4; i++)
    {
        float v = (raw[i] - 0.3f) / 0.7f;
        v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        if (v != s_fedStick[i])
        {
            io.AddKeyAnalogEvent(kStick[i], v > 0.1f, v);
            s_fedStick[i] = v;
        }
    }
}

bool s_open;
bool s_focused;
bool s_navFlags;
bool s_lastInputPad;

// Runs at the top of ImGui::NewFrame, after the backends and before ImGui reads its input.
void on_new_frame(ImGuiContext*, ImGuiContextHook*)
{
    ImGuiIO& io = ImGui::GetIO();
    poll_pads(io.DeltaTime > 0.0f ? io.DeltaTime : 1.0f / 60.0f);

    const bool nav = s_open && s_focused;
    s_navPressed = 0;
    if (nav)
        for (u32 p = 0; p < PAD_CHANMAX; p++)
            if ((s_navPorts & (1u << p)) != 0)
                s_navPressed |= s_pressed[p];
    if (s_navPressed != 0)
        s_lastInputPad = true;

    // ImGui activates only under a visible nav cursor, which it otherwise shows after a move.
    if (nav)
    {
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigNavCursorVisibleAlways = true;
        s_navFlags = true;
    }
    else if (s_navFlags)
    {
        io.ConfigFlags &= ~(ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard);
        io.ConfigNavCursorVisibleAlways = false;
        s_navFlags = false;
    }
    feed_imgui(io, nav && !s_padOwned);
}

bool pad_pressed(unsigned bits) { return (s_navPressed & bits) != 0; }

const char* format(const char* f, ...)
{
    static char s_buf[16][256];
    static int s_next;
    char* out = s_buf[s_next++ & 15];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(out, sizeof s_buf[0], f, ap);
    va_end(ap);
    return out;
}

const char* captain_name(int team)
{
    return team >= 0 && team < 9 ? text((Text)(T_CAPTAIN_FIRST + team)) : "?";
}

const char* sidekick_name(int sidekick)
{
    return sidekick >= 0 && sidekick < 4 ? text((Text)(T_SIDEKICK_FIRST + sidekick)) : "?";
}

const char* stadium_name(int stadium)
{
    return stadium >= 0 && stadium < 7 ? text((Text)(T_STADIUM_FIRST + stadium)) : "?";
}

// The Super Team has no sidekicks of its own.
const char* team_label(const OnlineMatchDesc& d, int side)
{
    if (d.team[side] == 8)
        return captain_name(8);
    return format(text(T_TEAM_WITH), captain_name(d.team[side]), sidekick_name(d.sidekick[side]));
}

const char* summary(const OnlineMatchDesc& d)
{
    return format(text(T_SUMMARY), format(text(T_VERSUS), team_label(d, 0), team_label(d, 1)),
                  stadium_name(d.stadium), format(text(T_MINUTES), d.gameTime / 60));
}

const char* machine_name(const OnlineState& st, int index)
{
    if (index < 0 || index >= ONLINE_MAX_MACHINES)
        return "?";
    const OnlineMachine& m = st.machines[index];
    return m.nickname[0] != '\0' ? m.nickname : PortOnlinePlatformName(m.platform);
}

const char* error_sentence(const OnlineState& st)
{
    switch (st.error)
    {
    case ONLINE_ERR_UNAVAILABLE: return text(T_ERR_UNAVAILABLE);
    case ONLINE_ERR_NETWORK:     return text(T_ERR_NETWORK);
    case ONLINE_ERR_BAD_ADDRESS: return text(T_ERR_BAD_ADDRESS);
    case ONLINE_ERR_PORT_IN_USE:
        return format(text(T_ERR_PORT_IN_USE),
                      (unsigned)(st.roomPort != 0 ? st.roomPort : st.settings.hostPort));
    case ONLINE_ERR_CANT_HOST:       return text(T_ERR_CANT_HOST);
    case ONLINE_ERR_UNREACHABLE:     return text(T_ERR_UNREACHABLE);
    case ONLINE_ERR_ROOM_FULL:       return text(T_ERR_ROOM_FULL);
    case ONLINE_ERR_IN_MATCH:        return text(T_ERR_IN_MATCH);
    case ONLINE_ERR_VERSION:         return text(T_ERR_VERSION);
    case ONLINE_ERR_DATA_FAMILY:     return text(T_ERR_DATA_FAMILY);
    case ONLINE_ERR_HOST_LEFT:       return text(T_ERR_HOST_LEFT);
    case ONLINE_ERR_CONNECTION_LOST: return text(T_ERR_CONNECTION_LOST);
    case ONLINE_ERR_PROTOCOL:        return text(T_ERR_PROTOCOL);
    case ONLINE_ERR_SETUP_MISMATCH:  return text(T_ERR_SETUP_MISMATCH);
    default:                         break;
    }
    return st.lastError[0] != '\0' ? st.lastError : text(T_ERR_UNKNOWN);
}

const int kRecentMax = 4;
char s_recent[kRecentMax][ONLINE_ADDRESS_MAX];
int s_recentCount;

bool recent_path(char* out, size_t cap)
{
    const char* ini = ImGui::GetIO().IniFilename;
    if (ini == nullptr)
        return false;
    const char* slash = std::strrchr(ini, '/');
    const char* back = std::strrchr(ini, '\\');
    if (back != nullptr && (slash == nullptr || back > slash))
        slash = back;
    const int dir = slash != nullptr ? (int)(slash - ini) : 0;
    const int n =
        std::snprintf(out, cap, "%.*s%sonline-recent.txt", dir, ini, slash != nullptr ? "/" : "");
    return n > 0 && (size_t)n < cap;
}

void load_recent()
{
    char path[1024];
    if (!recent_path(path, sizeof path))
        return;
    FILE* f = std::fopen(path, "r");
    if (f == nullptr)
        return;
    char line[256];
    while (s_recentCount < kRecentMax && std::fgets(line, sizeof line, f) != nullptr)
    {
        line[std::strcspn(line, "\r\n")] = '\0';
        if (std::strlen(line) < ONLINE_ADDRESS_MAX && plain_address(line))
            std::snprintf(s_recent[s_recentCount++], ONLINE_ADDRESS_MAX, "%s", line);
    }
    std::fclose(f);
}

void remember(const char* address)
{
    if (!plain_address(address) || std::strlen(address) >= ONLINE_ADDRESS_MAX)
        return;
    char keep[kRecentMax][ONLINE_ADDRESS_MAX];
    int n = 0;
    std::snprintf(keep[n++], ONLINE_ADDRESS_MAX, "%s", address);
    for (int i = 0; i < s_recentCount && n < kRecentMax; i++)
        if (std::strcmp(s_recent[i], address) != 0)
            std::memcpy(keep[n++], s_recent[i], ONLINE_ADDRESS_MAX);
    std::memcpy(s_recent, keep, sizeof keep);
    s_recentCount = n;

    char path[1024];
    if (!recent_path(path, sizeof path))
        return;
    if (FILE* f = std::fopen(path, "w"))
    {
        for (int i = 0; i < s_recentCount; i++)
            std::fprintf(f, "%s\n", s_recent[i]);
        std::fclose(f);
    }
}

enum Screen
{
    SCR_HOME,
    SCR_JOIN,
    SCR_NEARBY,
    SCR_SETTINGS,
    SCR_EDIT_NAME,
    SCR_EDIT_PORT,
    SCR_CONNECTING,
    SCR_ROOM,
    SCR_SETUP,
    SCR_OPTIONS,
    SCR_ADDRESS,
    SCR_LEAVE,
    SCR_LOADING,
    SCR_MATCH,
    SCR_POST,
    SCR_OPPONENTS,
    SCR_ERROR
};

Screen s_idleScreen = SCR_HOME;
Screen s_roomScreen = SCR_ROOM;
bool s_leaveAsked;
bool s_focusRequest;
int s_homeChoice;
int s_lastPhase = ONLINE_PHASE_IDLE;
int s_lastOpponentsLeft;
uint32_t s_lastNotice;
double s_addWaitUntil;
double s_nearbyAskedAt = -100.0;
float s_slowLoadDismissedAt = -1.0f;
bool s_textActive;
bool s_focusText;
char s_portError;

Wheel s_address;
Wheel s_name;
Wheel s_port;

Screen screen_for(const OnlineState& st)
{
    switch (st.phase)
    {
    case ONLINE_PHASE_ERROR:   return SCR_ERROR;
    case ONLINE_PHASE_JOINING: return SCR_CONNECTING;
    case ONLINE_PHASE_HOSTING:
    case ONLINE_PHASE_ROOM:    return s_leaveAsked ? SCR_LEAVE : s_roomScreen;
    case ONLINE_PHASE_LOADING: return s_leaveAsked ? SCR_LEAVE : SCR_LOADING;
    case ONLINE_PHASE_MATCH:
        if (st.opponentsLeft)
            return SCR_OPPONENTS;
        return s_leaveAsked ? SCR_LEAVE : SCR_MATCH;
    case ONLINE_PHASE_POST_MATCH: return s_leaveAsked ? SCR_LEAVE : SCR_POST;
    default:                      return s_idleScreen;
    }
}

void set_open(bool open)
{
    if (open && !s_open)
        s_focusRequest = true;
    s_open = open;
    if (!open)
    {
        s_focused = false;
        s_leaveAsked = false;
    }
}

const OnlineMachine* self_machine(const OnlineState& st)
{
    return st.self >= 0 && st.self < ONLINE_MAX_MACHINES && st.machines[st.self].present
               ? &st.machines[st.self]
               : nullptr;
}

// The device ports this machine's humans play on.
unsigned local_ports(const OnlineState& st)
{
    unsigned ports = 0;
    if (const OnlineMachine* m = self_machine(st))
        for (int h = 0; h < m->humanCount && h < ONLINE_MAX_LOCAL_HUMANS; h++)
            if (m->humans[h].localPort >= 0 && m->humans[h].localPort < PAD_CHANMAX)
                ports |= 1u << m->humans[h].localPort;
    return ports;
}

bool in_room_phase(const OnlineState& st)
{
    return st.phase == ONLINE_PHASE_HOSTING || st.phase == ONLINE_PHASE_ROOM ||
           st.phase == ONLINE_PHASE_LOADING || st.phase == ONLINE_PHASE_MATCH ||
           st.phase == ONLINE_PHASE_POST_MATCH;
}

float px() { return ImGui::GetFontSize(); }

void slant_panel(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 color, float cut)
{
    const ImVec2 points[] = { ImVec2(a.x + cut, a.y), ImVec2(b.x, a.y), ImVec2(b.x - cut, b.y),
                              ImVec2(a.x, b.y) };
    dl->AddConvexPolyFilled(points, 4, color);
}

void focus_ring(ImVec2 a, ImVec2 b)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float p = px();
    const ImVec2 points[] = { ImVec2(a.x + p * .45f, a.y), ImVec2(b.x, a.y),
                              ImVec2(b.x - p * .45f, b.y), ImVec2(a.x, b.y) };
    dl->AddPolyline(points, 4, col(kGold), ImDrawFlags_Closed, p * .08f);
    const float y = (a.y + b.y) * .5f;
    dl->AddTriangleFilled(ImVec2(a.x + p * .22f, y - p * .22f), ImVec2(a.x + p * .50f, y),
                          ImVec2(a.x + p * .22f, y + p * .22f), col(kGold));
}

void text_at(ImDrawList* dl, ImFont* font, ImVec2 pos, const ImVec4& c, const char* s,
             float alpha = 1.0f)
{
    dl->AddText(font, font->FontSize * font->Scale, pos, col(c, alpha), s);
}

void title_at(ImDrawList* dl, ImFont* font, ImVec2 pos, const ImVec4& color, const char* value,
              float available = 0.0f)
{
    char title[256];
    text_upper(value, title, sizeof title);
    float size = font->FontSize * font->Scale;
    if (available > 0.0f)
    {
        const float width = font->CalcTextSizeA(size, 1e9f, 0.0f, title).x + size * .25f;
        if (width > available)
            size *= available / width;
    }
    auto draw = [&](float x, float y, ImU32 ink) {
        const int first = dl->VtxBuffer.Size;
        dl->AddText(font, size, ImVec2(pos.x + x, pos.y + y), ink, title);
        for (int i = first; i < dl->VtxBuffer.Size; ++i)
            dl->VtxBuffer[i].pos.x += (pos.y + size - dl->VtxBuffer[i].pos.y) * .20f;
    };
    const float edge = size * .055f;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            if (x || y)
                draw(x * edge, y * edge + edge, IM_COL32(0, 0, 0, 230));
    draw(0, 0, col(color));
    draw(size * .025f, 0, col(color));
}

ImVec2 text_size(ImFont* font, const char* s)
{
    return font->CalcTextSizeA(font->FontSize * font->Scale, 1e9f, 0.0f, s);
}

void check_mark(ImDrawList* dl, ImVec2 c, float s, ImU32 color)
{
    const ImVec2 pts[3] = { ImVec2(c.x - s * 0.45f, c.y), ImVec2(c.x - s * 0.1f, c.y + s * 0.35f),
                            ImVec2(c.x + s * 0.5f, c.y - s * 0.4f) };
    dl->AddPolyline(pts, 3, color, 0, s * 0.18f);
}

void spinner(ImDrawList* dl, ImVec2 c, float r, ImU32 color)
{
    const float t = (float)ImGui::GetTime() * 6.0f;
    dl->PathArcTo(c, r, t, t + 4.2f, 24);
    dl->PathStroke(color, 0, r * 0.28f);
}

void ping_bars(ImDrawList* dl, ImVec2 a, float h, int ms)
{
    const int bars = ms < 40 ? 4 : ms < 80 ? 3 : ms < 150 ? 2 : 1;
    const ImVec4& c = bars >= 3 ? kGreen : bars == 2 ? kGold : kRed;
    const float w = h * 0.22f, gap = h * 0.12f;
    for (int i = 0; i < 4; i++)
    {
        const float bh = h * (0.4f + 0.2f * (float)i);
        const ImVec2 p(a.x + (float)i * (w + gap), a.y + h - bh);
        dl->AddRectFilled(p, ImVec2(p.x + w, a.y + h), i < bars ? col(c) : col(kButtonHover),
                          w * 0.3f);
    }
}

void progress_bar(ImDrawList* dl, ImVec2 a, ImVec2 b, float frac, const ImVec4& c)
{
    frac = frac < 0.0f ? 0.0f : (frac > 1.0f ? 1.0f : frac);
    const float r = (b.y - a.y) * 0.5f;
    dl->AddRectFilled(a, b, col(kButton), r);
    if (frac > 0.0f)
        dl->AddRectFilled(a, ImVec2(a.x + (b.x - a.x) * frac, b.y), col(c), r);
}

std::map<const unsigned char*, ImTextureID> s_promptTextures;

float button_glyph(ImDrawList* dl, ImVec2 pos, float h, unsigned bit)
{
    unsigned width = 0, height = 0;
    const unsigned char* pixels = PortPromptsMenuImage(bit, &width, &height);
    if (pixels && width && height)
    {
        auto it = s_promptTextures.find(pixels);
        if (it == s_promptTextures.end())
            return h;
        const float w = h * (float)width / (float)height;
        dl->AddImage(it->second, pos, ImVec2(pos.x + w, pos.y + h));
        return w;
    }
    const ImVec2 c(pos.x + h * 0.5f, pos.y + h * 0.5f);
    const bool ud = (bit & (PAD_BUTTON_UP | PAD_BUTTON_DOWN)) != 0;
    const bool lr = (bit & (PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT)) != 0;
    if (ud || lr)
    {
        const float arm = h * 0.18f, len = h * 0.46f;
        dl->AddRectFilled(ImVec2(c.x - arm, c.y - len), ImVec2(c.x + arm, c.y + len),
                          col(ud ? kText : kMuted), arm * 0.4f);
        dl->AddRectFilled(ImVec2(c.x - len, c.y - arm), ImVec2(c.x + len, c.y + arm),
                          col(lr ? kText : kMuted), arm * 0.4f);
    }
    else
        dl->AddText(pos, col(kMuted), "?");
    return h;
}

// A keyboard key, for the hints when the keyboard is in use.
float key_glyph(ImDrawList* dl, ImVec2 pos, float h, const char* key)
{
    ImFont* font = s_fonts->body;
    const float size = font->FontSize * font->Scale * 0.72f;
    const ImVec2 ts = font->CalcTextSizeA(size, 1e9f, 0.0f, key);
    const float w = ts.x + h * 0.55f;
    dl->AddRect(ImVec2(pos.x, pos.y + h * 0.08f), ImVec2(pos.x + w, pos.y + h * 0.92f), col(kMuted),
                h * 0.2f, 0, h * 0.07f);
    dl->AddText(font, size, ImVec2(pos.x + (w - ts.x) * 0.5f, pos.y + (h - ts.y) * 0.5f),
                col(kText), key);
    return w;
}

struct Hint
{
    unsigned bit;
    Text label;
};

const char* keyboard_key(unsigned bit)
{
    switch (bit)
    {
    case PAD_BUTTON_A: return text(T_ENTER_KEY);
    case PAD_BUTTON_B: return text(T_ESCAPE_KEY);
    default:           return nullptr;
    }
}

const ImGuiWindowFlags kScreenFlags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollWithMouse;

void begin_screen(const char* id, const char* title, const char* context, bool busy = false,
                  float rows = 26.0f)
{
    const ImGuiIO& io = ImGui::GetIO();
    const float p = px();
    const float margin = p;
    const ImVec2 size(ImMin(io.DisplaySize.x - 2.0f * margin, p * 48.0f),
                      ImMin(io.DisplaySize.y - 2.0f * margin, p * rows * 1.12f));
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    if (s_focusRequest)
    {
        ImGui::SetNextWindowFocus();
        s_focusRequest = false;
    }
    ImGui::Begin(id, nullptr, kScreenFlags);
    s_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const float headH = p * 3.1f;
    dl->PushClipRect(wp, ImVec2(wp.x + size.x, wp.y + size.y), false);
    dl->AddRectFilledMultiColor(wp, ImVec2(wp.x + size.x, wp.y + headH), col(kButtonActive),
                                col(kButton), col(kButton), col(kRaised));
    slant_panel(dl, ImVec2(wp.x - p * .35f, wp.y + p * .35f),
                ImVec2(wp.x + size.x * .72f, wp.y + headH - p * .25f), col(kInk), p * .7f);
    dl->AddLine(ImVec2(wp.x, wp.y + headH), ImVec2(wp.x + size.x, wp.y + headH), col(kHome, .65f),
                p * .08f);
    ImFont* head = s_fonts->head;
    const ImVec2 ts = text_size(head, title);
    const float pad = ImGui::GetStyle().WindowPadding.x;

    float right = wp.x + size.x - pad;
    if (busy)
    {
        spinner(dl, ImVec2(right - p * 0.5f, wp.y + headH * 0.5f), p * 0.45f, col(kGold));
        right -= p * 1.6f;
    }
    if (context != nullptr && context[0] != '\0')
    {
        ImFont* body = s_fonts->body;
        const ImVec2 cs = text_size(body, context);
        const float width = ImMin(cs.x, size.x * .30f);
        const float scale = cs.x > 0 ? width / cs.x : 1.0f;
        dl->AddText(body, body->FontSize * body->Scale * scale,
                    ImVec2(right - width, wp.y + (headH - cs.y * scale) * .5f), col(kMuted),
                    context);
        right -= width + p;
    }
    title_at(dl, head, ImVec2(wp.x + pad, wp.y + (headH - ts.y) * .5f), kText, title,
             right - wp.x - pad);
    dl->PopClipRect();
    ImGui::SetCursorPosY(headH + p * 0.7f);
    ImGui::BeginChild("body", ImVec2(0.0f, -p * 2.6f), ImGuiChildFlags_NavFlattened);
}

void end_screen(const Hint* hints, int count)
{
    ImGui::EndChild();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 ws = ImGui::GetWindowSize();
    const float p = px();
    const float pad = ImGui::GetStyle().WindowPadding.x;
    const float y = wp.y + ws.y - p * 2.1f;
    dl->PushClipRect(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), false);
    dl->AddLine(ImVec2(wp.x + pad, y - p * 0.35f), ImVec2(wp.x + ws.x - pad, y - p * 0.35f),
                col(kButtonHover), 1.0f);
    float x = wp.x + pad;
    const float h = p * 1.35f;
    for (int i = 0; i < count; i++)
    {
        const char* key = keyboard_key(hints[i].bit);
        if (!s_lastInputPad && key == nullptr)
            continue;
        x += (!s_lastInputPad ? key_glyph(dl, ImVec2(x, y), h, key)
                              : button_glyph(dl, ImVec2(x, y), h, hints[i].bit)) +
             p * 0.4f;
        const char* label = text(hints[i].label);
        const ImVec2 ts = text_size(s_fonts->body, label);
        text_at(dl, s_fonts->body, ImVec2(x, y + (h - ts.y) * 0.5f), kMuted, label);
        x += ts.x + p * 1.3f;
    }
    dl->PopClipRect();
    ImGui::End();
}

void muted_text(const char* s)
{
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextWrapped("%s", s);
    ImGui::PopStyleColor();
}

void section(const char* s)
{
    ImGui::Dummy(ImVec2(0.0f, px() * 0.2f));
    ImGui::PushStyleColor(ImGuiCol_Text, kGold);
    ImGui::TextUnformatted(s);
    ImGui::PopStyleColor();
}

// A full-width menu row; `right` is drawn at the far end.
bool row(const char* id, const char* label, const char* sub = nullptr, bool enabled = true,
         const char* right = nullptr, bool accent = false)
{
    const float p = px();
    const float w = ImGui::GetContentRegionAvail().x;
    ImFont* font = s_fonts->body;
    const float lh = font->FontSize * font->Scale;
    const float labelWidth = w - p * 2.0f - (right ? text_size(font, right).x + p : 0.0f);
    const float wrapWidth = labelWidth > p ? labelWidth : p;
    const float subHeight = sub ? font->CalcTextSizeA(lh, 1e9f, wrapWidth, sub).y : 0.0f;
    const float h = sub ? p * .75f + lh + subHeight : p * 2.05f;
    ImGui::BeginDisabled(!enabled);
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(w, h), ImGuiButtonFlags_EnableNav);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const bool focused = ImGui::IsItemFocused();
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float alpha = enabled ? 1.0f : 0.7f;
    ImVec4 bg = active ? kButtonActive : (hovered || focused) ? kButtonHover : kButton;
    if (accent && enabled && !(hovered || focused))
        bg = kButtonHover;
    slant_panel(dl, a, b, col(bg, alpha), p * .45f);
    const float x = a.x + p * 1.0f;
    if (sub != nullptr)
    {
        title_at(dl, font, ImVec2(x, a.y + p * .3f), focused && enabled ? kGold : kText, label,
                 wrapWidth);
        dl->AddText(font, lh, ImVec2(x, a.y + p * .35f + lh),
                    col(accent ? kText : kMuted, alpha * (accent ? .8f : 1.0f)), sub, nullptr,
                    wrapWidth);
    }
    else
    {
        if (std::strcmp(id, "recent") == 0)
            text_at(dl, font, ImVec2(x, a.y + (h - lh) * .5f), kText, label, alpha);
        else
            title_at(dl, font, ImVec2(x, a.y + (h - lh) * .5f), focused && enabled ? kGold : kText,
                     label, wrapWidth);
    }
    if (right != nullptr)
    {
        const ImVec2 rs = text_size(font, right);
        text_at(dl, font, ImVec2(b.x - p * 1.0f - rs.x, a.y + (h - rs.y) * 0.5f), kMuted, right,
                alpha);
    }
    if (focused)
        focus_ring(a, b);
    ImGui::EndDisabled();
    return pressed && enabled;
}

bool key_left()
{
    return ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft) ||
           ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft) ||
           ImGui::IsKeyPressed(ImGuiKey_LeftArrow);
}

bool key_right()
{
    return ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight) ||
           ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight) ||
           ImGui::IsKeyPressed(ImGuiKey_RightArrow);
}

// Left and right step the value, -1 or +1; a read-only row still takes focus, for scrolling.
int spin_row(const char* id, const char* label, const char* value, bool editable)
{
    const float p = px();
    const float h = p * 2.0f;
    const float w = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = s_fonts->body;
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(w, h), ImGuiButtonFlags_EnableNav);
    const bool hovered = editable && ImGui::IsItemHovered();
    const bool focused = ImGui::IsItemFocused();
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    dl->AddRectFilled(a, b,
                      col((hovered || (focused && editable)) ? kButtonHover
                          : editable                         ? kButton
                                                             : kRaised),
                      ImGui::GetStyle().FrameRounding);
    const float lh = font->FontSize * font->Scale;
    text_at(dl, font, ImVec2(a.x + p, a.y + (h - lh) * 0.5f), kMuted, label);
    const ImVec2 vs = text_size(font, value);
    const float arrowW = p * 0.45f;
    const float right = b.x - p;
    float vx = right - vs.x;
    float lx = 0.0f;
    if (editable)
    {
        vx -= arrowW + p * 0.5f;
        const ImU32 ac = col(focused ? kGold : kMuted);
        const float cy = a.y + h * 0.5f;
        dl->AddTriangleFilled(ImVec2(right, cy), ImVec2(right - arrowW, cy - arrowW * 0.8f),
                              ImVec2(right - arrowW, cy + arrowW * 0.8f), ac);
        lx = vx - p * 0.5f;
        dl->AddTriangleFilled(ImVec2(lx - arrowW, cy), ImVec2(lx, cy - arrowW * 0.8f),
                              ImVec2(lx, cy + arrowW * 0.8f), ac);
    }
    text_at(dl, font, ImVec2(vx, a.y + (h - lh) * 0.5f), kText, value);
    if (focused)
        focus_ring(a, b);

    if (!editable)
        return 0;
    if (focused && key_left())
        return -1;
    if (focused && key_right())
        return 1;
    if (!pressed)
        return 0;
    // A click on the left arrow steps back; A or a click anywhere else steps on.
    const float mx = ImGui::GetIO().MousePos.x;
    const bool onLeft = ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
                        mx >= lx - arrowW - p * 0.4f && mx <= lx + p * 0.4f;
    return onLeft ? -1 : 1;
}

unsigned wheel_moves(unsigned pressed)
{
    unsigned moves = 0;
    if ((pressed & kLeft) != 0)
        moves |= WHEEL_LEFT;
    if ((pressed & kRight) != 0)
        moves |= WHEEL_RIGHT;
    if ((pressed & kUp) != 0)
        moves |= WHEEL_UP;
    if ((pressed & kDown) != 0)
        moves |= WHEEL_DOWN;
    if ((pressed & PAD_BUTTON_X) != 0)
        moves |= WHEEL_DELETE;
    return moves;
}

void wheel_draw(const Wheel& w, int maxLen)
{
    const float p = px();
    ImFont* font = s_fonts->head;
    const float cw = p * 1.35f, ch = p * 2.4f, gap = p * 0.18f;
    const float avail = ImGui::GetContentRegionAvail().x;
    const int len = (int)std::strlen(w.text);
    const int cells = ImMin(maxLen, ImMax(len + 1, w.cursor + 1));
    const int fit = ImMax(1, (int)((avail + gap) / (cw + gap)));
    int first = 0;
    if (w.cursor >= fit)
        first = w.cursor - fit + 1;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 base(origin.x, origin.y + p * 0.7f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float fs = font->FontSize * font->Scale;
    for (int i = first; i < cells && i < first + fit; i++)
    {
        const ImVec2 a(base.x + (float)(i - first) * (cw + gap), base.y);
        const ImVec2 b(a.x + cw, a.y + ch);
        const bool cur = i == w.cursor;
        dl->AddRectFilled(a, b, col(cur ? kButtonActive : kButton), p * 0.3f);
        if (i < len)
        {
            char c[2] = { (unsigned char)w.text[i] < 0x80 ? w.text[i] : '?', '\0' };
            const ImVec2 ts = font->CalcTextSizeA(fs, 1e9f, 0.0f, c);
            dl->AddText(font, fs, ImVec2(a.x + (cw - ts.x) * 0.5f, a.y + (ch - ts.y) * 0.5f),
                        col(kText), c);
        }
        if (cur)
        {
            dl->AddRect(a, b, col(kGold), p * 0.3f, 0, p * 0.14f);
            const float t = p * 0.32f, cx = (a.x + b.x) * 0.5f;
            dl->AddTriangleFilled(ImVec2(cx, a.y - p * 0.62f), ImVec2(cx - t, a.y - p * 0.18f),
                                  ImVec2(cx + t, a.y - p * 0.18f), col(kGold));
            dl->AddTriangleFilled(ImVec2(cx, b.y + p * 0.62f), ImVec2(cx - t, b.y + p * 0.18f),
                                  ImVec2(cx + t, b.y + p * 0.18f), col(kGold));
        }
    }
    ImGui::Dummy(ImVec2(avail, ch + p * 1.4f));
}

#if defined(PANEL_HAS_CLIPBOARD)
void paste_into(Wheel& w, int maxLen)
{
    char* clip = SDL_GetClipboardText();
    if (clip == nullptr)
        return;
    char cleaned[ONLINE_ADDRESS_MAX];
    clean_field(cleaned, sizeof cleaned, clip, false);
    SDL_free(clip);
    wheel_set(w, cleaned, maxLen);
}
#endif

#if PANEL_HAS_TYPING
// The typed alternative to the wheel; Enter returns true.
bool text_field(const char* id, Wheel& w, int maxLen, ImGuiInputTextFlags extra)
{
    const float p = px();
    if (s_focusText)
    {
        ImGui::SetKeyboardFocusHere();
        s_focusText = false;
    }
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(p * 0.8f, p * 0.55f));
    const bool enter = ImGui::InputTextWithHint(id, text(T_JOIN_TYPE_HINT), w.text,
                                                (size_t)ImMin(maxLen + 1, (int)sizeof w.text),
                                                ImGuiInputTextFlags_EnterReturnsTrue | extra);
    ImGui::PopStyleVar();
    if (ImGui::IsItemActive())
    {
        s_textActive = true;
        w.cursor = (int)std::strlen(w.text);
    }
    if (ImGui::IsItemFocused() && !ImGui::IsItemActive() && !s_lastInputPad)
        focus_ring(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    return enter;
}
#endif

// The keyboard's Escape and the pad's B, whichever came.
bool back_pressed()
{
    return pad_pressed(PAD_BUTTON_B) ||
           (s_focused && !s_textActive && ImGui::IsKeyPressed(ImGuiKey_Escape, false));
}

const int kAddressLen = ONLINE_ADDRESS_MAX - 1;
const int kPortLen = 5;

void start_join(const char* address)
{
    char cleaned[ONLINE_ADDRESS_MAX];
    clean_field(cleaned, sizeof cleaned, address, false);
    if (cleaned[0] == '\0')
        return;
    wheel_set(s_address, cleaned, kAddressLen);
    PortOnlineJoin(cleaned, nullptr);
}

// Pad moves go to the wheel; a text field being typed in lets go of its copy first.
void wheel_input(Wheel& w, const char* charset, int maxLen)
{
    const unsigned moves = s_focused ? wheel_moves(s_navPressed) : 0;
    if (moves == 0)
        return;
    if (ImGui::GetActiveID() != 0)
        ImGui::ClearActiveID();
    wheel_edit(w, charset, maxLen, moves);
}

void draw_home(const OnlineState& st)
{
    begin_screen("##online.home", text(T_ONLINE), st.settings.nickname);
    enum
    {
        HOME_CREATE,
        HOME_NEARBY,
        HOME_JOIN,
        HOME_SETTINGS
    };
    int choice = -1;
    int action = -1;
    int item = 0;
    auto remembered = [&](int index) {
        if (index == s_homeChoice)
            ImGui::SetItemDefaultFocus();
    };
    auto entry = [&](const char* id, Text label, Text sub, int what) {
        if (row(id, text(label), text(sub), true, nullptr, what == HOME_CREATE))
        {
            choice = item;
            action = what;
        }
        remembered(item++);
    };
    entry("create", T_CREATE_ROOM, T_CREATE_ROOM_SUB, HOME_CREATE);
    entry("nearby", T_NEARBY, T_NEARBY_SUB, HOME_NEARBY);
    entry("join", T_JOIN_ADDRESS, T_JOIN_ADDRESS_SUB, HOME_JOIN);
    entry("settings", T_SETTINGS, T_SETTINGS_SUB, HOME_SETTINGS);

    if (s_recentCount > 0)
    {
        section(text(T_RECENT));
        for (int i = 0; i < s_recentCount; i++)
        {
            ImGui::PushID(i);
            if (row("recent", s_recent[i]))
            {
                s_homeChoice = item;
                start_join(s_recent[i]);
            }
            remembered(item++);
            ImGui::PopID();
        }
    }

    ImGui::Dummy(ImVec2(0.0f, px() * 0.3f));

    const Hint hints[] = { { PAD_BUTTON_A, T_HINT_SELECT }, { PAD_BUTTON_B, T_CLOSE } };
    end_screen(hints, 2);

    if (choice >= 0)
        s_homeChoice = choice;
    switch (action)
    {
    case HOME_CREATE: PortOnlineHost(0, nullptr); break;
    case HOME_NEARBY:
        s_idleScreen = SCR_NEARBY;
        s_nearbyAskedAt = -100.0;
        break;
    case HOME_JOIN:
        s_idleScreen = SCR_JOIN;
        wheel_set(s_address,
                  s_address.text[0] == '\0' && s_recentCount > 0 ? s_recent[0] : s_address.text,
                  kAddressLen);
        s_focusText = !s_lastInputPad;
        break;
    case HOME_SETTINGS: s_idleScreen = SCR_SETTINGS; break;
    default:            break;
    }
    if (back_pressed())
        set_open(false);
}

// By address, typed or on a wheel the pad edits.
void draw_join(const OnlineState&)
{
    Wheel& w = s_address;
    const int maxLen = kAddressLen;
    const bool wheel = !PANEL_HAS_TYPING || s_lastInputPad;
    s_padOwned = wheel;
    if (wheel)
        wheel_input(w, kAddressChars, maxLen);

    begin_screen("##online.join", text(T_JOIN_ADDRESS), nullptr, false, 17.0f);
    if (wheel)
    {
        muted_text(text(T_WHEEL_HELP));
        wheel_draw(w, maxLen);
    }
    else
        muted_text(text(T_JOIN_HELP));
    bool join = false;
#if PANEL_HAS_TYPING
    if (!wheel)
        join = text_field("##address", w, maxLen, ImGuiInputTextFlags_CharsNoBlank);
#endif
    ImGui::Dummy(ImVec2(0.0f, px() * 0.3f));
    const float bw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px() * 0.8f, px() * 0.55f));
    if (ImGui::Button(text(T_JOIN), ImVec2(bw, 0.0f)))
        join = true;
#if PANEL_HAS_TYPING
    ImGui::SameLine();
    if (wheel && ImGui::Button(text(T_TYPE), ImVec2(bw, 0.0f)))
    {
        s_lastInputPad = false;
        s_focusText = true;
    }
#endif
#if defined(PANEL_HAS_CLIPBOARD)
    if (!wheel)
    {
        ImGui::SameLine();
        if (ImGui::Button(text(T_PASTE), ImVec2(bw, 0.0f)))
            paste_into(w, maxLen);
    }
#endif
    ImGui::PopStyleVar();

    const Hint hints[] = { { kDpad, T_HINT_MOVE },
                           { PAD_BUTTON_A, T_HINT_JOIN },
                           { PAD_BUTTON_X, T_HINT_DELETE },
                           { PAD_BUTTON_B, T_HINT_BACK },
                           { PAD_BUTTON_Y, T_HINT_KEYBOARD } };
    end_screen(hints, PANEL_HAS_TYPING ? 5 : 4);

    if (pad_pressed(PAD_BUTTON_A))
        join = true;
    if (PANEL_HAS_TYPING && pad_pressed(PAD_BUTTON_Y))
    {
        s_lastInputPad = false;
        s_focusText = true;
    }
    if (join)
        start_join(s_address.text);
    else if (back_pressed())
        s_idleScreen = SCR_HOME;
}

void draw_nearby(const OnlineState& st)
{
    const double now = ImGui::GetTime();
    if (!st.nearbySearching && now - s_nearbyAskedAt > 5.0)
    {
        s_nearbyAskedAt = now;
        PortOnlineRefreshNearby();
    }

    begin_screen("##online.nearby", text(T_NEARBY), nullptr, st.nearbySearching != 0);
    if (st.nearbyCount == 0)
        muted_text(text(st.nearbySearching ? T_SEARCHING : T_NONE_FOUND));
    // Rooms arrive after the screen opened with nothing on it to focus.
    bool focusFirst = ImGui::GetFocusID() == 0;
    for (int i = 0; i < st.nearbyCount && i < ONLINE_MAX_NEARBY; i++)
    {
        const OnlineNearbyRoom& r = st.nearby[i];
        const char* players =
            r.humans == 1 ? text(T_ONE_PLAYER) : format(text(T_PLAYERS), r.humans);
        const char* sub =
            format("%s  |  %s  |  %s", PortOnlinePlatformName(r.platform), players, r.address);
        const char* right = !r.compatible ? text(T_INCOMPATIBLE)
                            : r.inMatch   ? text(T_PLAYING)
                                          : nullptr;
        const bool joinable = r.compatible && !r.inMatch;
        if (focusFirst && joinable)
        {
            ImGui::SetKeyboardFocusHere();
            focusFirst = false;
        }
        ImGui::PushID(i);
        if (row("room", format(text(T_ROOM_OF), r.hostName), sub, joinable, right))
            start_join(r.address);
        ImGui::PopID();
    }
    const Hint hints[] = { { PAD_BUTTON_A, T_HINT_JOIN },
                           { PAD_BUTTON_Y, T_HINT_REFRESH },
                           { PAD_BUTTON_B, T_HINT_BACK } };
    end_screen(hints, 3);

    if (pad_pressed(PAD_BUTTON_Y))
        s_nearbyAskedAt = -100.0;
    if (back_pressed())
        s_idleScreen = SCR_HOME;
}

void draw_settings(const OnlineState& st)
{
    OnlineSettings next = st.settings;
    bool changed = false;
    static bool advanced = false;
    begin_screen("##online.settings", text(T_SETTINGS), nullptr, false, advanced ? 22.0f : 15.0f);

    if (row("nickname", text(T_NICKNAME), nullptr, true, st.settings.nickname))
    {
        wheel_set(s_name, st.settings.nickname, ONLINE_NAME_MAX);
        s_focusText = !s_lastInputPad;
        s_idleScreen = SCR_EDIT_NAME;
    }
    if (row("advanced", text(T_ADVANCED), nullptr, true, advanced ? "-" : "+"))
        advanced = !advanced;
    if (advanced)
    {
        {
            const int delay = st.settings.delayTicks;
            const char* value =
                delay == 0 ? text(T_AUTO) : format(text(T_MILLISECONDS), delay * ONLINE_TICK_MS);
            const int step = spin_row("delay", text(T_DELAY_LABEL), value, true);
            if (step != 0)
            {
                next.delayTicks = (uint8_t)((delay + step + ONLINE_MAX_DELAY_TICKS + 1) %
                                            (ONLINE_MAX_DELAY_TICKS + 1));
                changed = true;
            }
        }
        if (spin_row("stats", text(T_SHOW_STATS), text(st.settings.showStats ? T_ON : T_OFF),
                     true) != 0)
        {
            next.showStats = st.settings.showStats ? 0 : 1;
            changed = true;
        }
        if (row("port", text(T_ROOM_PORT), nullptr, true,
                format("%u", (unsigned)st.settings.hostPort)))
        {
            wheel_set(s_port, format("%u", (unsigned)st.settings.hostPort), kPortLen);
            s_portError = 0;
            s_focusText = !s_lastInputPad;
            s_idleScreen = SCR_EDIT_PORT;
        }
    }
    const Hint hints[] = { { PAD_BUTTON_A, T_HINT_SELECT },
                           { PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT, T_HINT_CHANGE },
                           { PAD_BUTTON_B, T_HINT_BACK } };
    end_screen(hints, 3);
    if (changed)
        PortOnlineSetSettings(&next);
    if (back_pressed())
        s_idleScreen = SCR_HOME;
}

// One setting's wheel over its own characters; A saves.
struct EditField
{
    Wheel* wheel;
    const char* charset;
    int maxLen;
    const char* id;
    Text title;
    Text help;   // T_COUNT for none
};

EditField edit_field(Screen field)
{
    switch (field)
    {
    case SCR_EDIT_PORT:
        return { &s_port, kDigitChars, kPortLen, "##online.port", T_ROOM_PORT, T_COUNT };
    default: return { &s_name, kNameChars, ONLINE_NAME_MAX, "##online.name", T_NICKNAME, T_COUNT };
    }
}

void draw_edit(const OnlineState& st, Screen field)
{
    const EditField f = edit_field(field);
    Wheel& w = *f.wheel;
    const int maxLen = f.maxLen;
    const bool port = field == SCR_EDIT_PORT;
    s_padOwned = true;
    wheel_input(w, f.charset, maxLen);

    begin_screen(f.id, text(f.title), nullptr);
    if (f.help != T_COUNT)
        muted_text(text(f.help));
    muted_text(text(T_WHEEL_HELP));
    wheel_draw(w, maxLen);
    bool save = false;
#if PANEL_HAS_TYPING
    save = text_field(f.id, w, maxLen, port ? ImGuiInputTextFlags_CharsDecimal : 0);
#endif
    if (port && s_portError)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kRed);
        ImGui::TextWrapped("%s", text(T_PORT_RANGE));
        ImGui::PopStyleColor();
    }
    ImGui::Dummy(ImVec2(0.0f, px() * 0.3f));
    const float bw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px() * 0.8f, px() * 0.55f));
    if (ImGui::Button(text(T_HINT_SAVE), ImVec2(bw, 0.0f)))
        save = true;
#if PANEL_HAS_TYPING
    ImGui::SameLine();
    if (ImGui::Button(text(T_TYPE), ImVec2(bw, 0.0f)))
    {
        s_lastInputPad = false;
        s_focusText = true;
    }
#endif
    ImGui::PopStyleVar();

    const Hint hints[] = { { kDpad, T_HINT_MOVE },
                           { PAD_BUTTON_A, T_HINT_SAVE },
                           { PAD_BUTTON_X, T_HINT_DELETE },
                           { PAD_BUTTON_B, T_HINT_BACK },
                           { PAD_BUTTON_Y, T_HINT_KEYBOARD } };
    end_screen(hints, PANEL_HAS_TYPING ? 5 : 4);

    if (pad_pressed(PAD_BUTTON_A))
        save = true;
    if (PANEL_HAS_TYPING && pad_pressed(PAD_BUTTON_Y))
    {
        s_lastInputPad = false;
        s_focusText = true;
    }
    if (save)
    {
        OnlineSettings next = st.settings;
        if (port)
        {
            const long v = std::strtol(w.text, nullptr, 10);
            s_portError = v < 1025 || v > 49151;
            if (s_portError)
                return;
            next.hostPort = (uint16_t)v;
        }
        else
        {
            char cleaned[ONLINE_NAME_MAX + 1];
            clean_field(cleaned, sizeof cleaned, w.text, true);
            if (cleaned[0] == '\0')
                return;
            std::memcpy(next.nickname, cleaned, sizeof next.nickname);
        }
        PortOnlineSetSettings(&next);
        s_idleScreen = SCR_SETTINGS;
    }
    else if (back_pressed())
        s_idleScreen = SCR_SETTINGS;
}

void draw_connecting(const OnlineState& st)
{
    const char* where = st.roomAddress[0] != '\0' ? st.roomAddress : s_address.text;
    begin_screen("##online.connecting", text(T_JOINING), where, true);
    const char* stage = nullptr;
    switch (st.joinStage)
    {
    case ONLINE_JOIN_RESOLVING:  stage = format(text(T_STAGE_RESOLVING), where); break;
    case ONLINE_JOIN_CONTACTING: stage = text(T_STAGE_CONTACTING); break;
    case ONLINE_JOIN_HANDSHAKE:  stage = text(T_STAGE_HANDSHAKE); break;
    default:                     stage = text(T_STAGE_ADMITTED); break;
    }
    ImGui::Dummy(ImVec2(0.0f, px() * 2.0f));
    ImGui::PushFont(s_fonts->head);
    ImGui::TextWrapped("%s", stage);
    ImGui::PopFont();
    muted_text(format(text(T_SECONDS), (int)st.joinSeconds));
    ImGui::Dummy(ImVec2(0.0f, px() * 1.5f));
    const bool cancel = row("cancel", text(T_CANCEL));
    const Hint hints[] = { { PAD_BUTTON_B, T_CANCEL } };
    end_screen(hints, 1);
    if (cancel || back_pressed())
        PortOnlineLeave();
}

void side_chip(ImDrawList* dl, ImVec2 a, float h, int slot, int side)
{
    const ImVec4& c = side == 0 ? kHome : side == 1 ? kRed : kButtonHover;
    ImFont* font = s_fonts->body;
    const float size = font->FontSize * font->Scale * 0.8f;
    const char* label = format(text(T_SLOT), slot + 1);
    const ImVec2 ts = font->CalcTextSizeA(size, 1e9f, 0.0f, label);
    const float w = h * 1.7f;
    dl->AddRectFilled(a, ImVec2(a.x + w, a.y + h), col(c), h * 0.3f);
    dl->AddText(font, size, ImVec2(a.x + (w - ts.x) * 0.5f, a.y + (h - ts.y) * 0.5f), col(kText),
                label);
}

const char* side_name(int side)
{
    return text(side == 0 ? T_HOME : side == 1 ? T_AWAY : T_NO_SIDE);
}

void machine_card(const OnlineState& st, int index)
{
    const OnlineMachine& m = st.machines[index];
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = s_fonts->body;
    const float p = px();
    const float lh = font->FontSize * font->Scale;
    const int lines = m.humanCount > 0 ? m.humanCount : 1;
    const float lineH = p * 1.65f;
    const float h = p * 0.9f + lineH * (float)(lines > 2 ? lines : 2);
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 a = ImGui::GetCursorScreenPos();
    const ImVec2 b(a.x + w, a.y + h);
    ImGui::Dummy(ImVec2(w, h));
    dl->AddRectFilled(a, b, col(kRaised), ImGui::GetStyle().ChildRounding);
    if (m.isSelf)
        dl->AddRectFilled(a, ImVec2(a.x + p * 0.25f, b.y), col(kGold),
                          ImGui::GetStyle().ChildRounding, ImDrawFlags_RoundCornersLeft);

    const float x0 = a.x + p * 0.9f, y0 = a.y + p * 0.45f;
    text_at(dl, font, ImVec2(x0, y0 + (lineH - lh) * 0.5f), kMuted,
            PortOnlinePlatformName(m.platform));
    const float y1 = y0 + lineH;
    if (m.isHost)
    {
        const char* host = text(T_HOST);
        const ImVec2 hs = text_size(font, host);
        dl->AddRectFilled(ImVec2(x0, y1 + p * 0.1f),
                          ImVec2(x0 + hs.x + p * 0.9f, y1 + lineH - p * 0.1f), col(kGold),
                          p * 0.4f);
        text_at(dl, font, ImVec2(x0 + p * 0.45f, y1 + (lineH - hs.y) * 0.5f),
                ImVec4(0.12f, 0.10f, 0.04f, 1.0f), host);
    }
    else if (m.connected)
    {
        check_mark(dl, ImVec2(x0 + p * 0.5f, y1 + lineH * 0.5f), p * 0.8f, col(kGreen));
        text_at(dl, font, ImVec2(x0 + p * 1.2f, y1 + (lineH - lh) * 0.5f), kGreen,
                text(T_CONNECTED));
    }
    else
    {
        text_at(dl, font, ImVec2(x0, y1 + (lineH - lh) * 0.5f), kMuted, text(T_NOT_READY));
    }

    const float x1 = a.x + w * 0.27f;
    if (m.humanCount == 0)
        text_at(dl, font, ImVec2(x1, y0 + (lineH - lh) * 0.5f), kMuted, "-");
    for (int i = 0; i < m.humanCount && i < ONLINE_MAX_LOCAL_HUMANS; i++)
    {
        const OnlineHuman& hu = m.humans[i];
        const float y = y0 + lineH * (float)i;
        side_chip(dl, ImVec2(x1, y + p * 0.2f), lineH - p * 0.4f, hu.slot, hu.side);
        const float nx = x1 + (lineH - p * 0.4f) * 1.7f + p * 0.6f;
        text_at(dl, font, ImVec2(nx, y + (lineH - lh) * 0.5f), kText, hu.name);
        float after = nx + text_size(font, hu.name).x + p * 0.4f;
        if (m.isSelf)
        {
            text_at(dl, font, ImVec2(after, y + (lineH - lh) * 0.5f), kMuted, text(T_YOU));
            after += text_size(font, text(T_YOU)).x + p * 0.4f;
        }
        text_at(dl, font, ImVec2(after, y + (lineH - lh) * 0.5f),
                hu.side == 0   ? kHome
                : hu.side == 1 ? kRed
                               : kMuted,
                side_name(hu.side));
    }

    if (!m.isSelf)
    {
        const float xr = b.x - p * 0.9f;
        const char* ms = m.connected ? format(text(T_MILLISECONDS), m.pingMs) : "...";
        const ImVec2 ms_size = text_size(font, ms);
        text_at(dl, font, ImVec2(xr - ms_size.x, y0 + (lineH - lh) * 0.5f),
                m.connected ? kText : kRed, ms);
        if (m.connected)
            ping_bars(dl, ImVec2(xr - ms_size.x - p * 1.9f, y0 + p * 0.25f), lineH - p * 0.5f,
                      m.pingMs);
        char how[96];
        std::snprintf(how, sizeof how, "%s", text(T_DIRECT));
        if (m.jitterMs > 20)
            std::snprintf(how + std::strlen(how), sizeof how - std::strlen(how), ", %s",
                          text(T_UNSTABLE));
        if (m.lossPercent >= 5)
            std::snprintf(how + std::strlen(how), sizeof how - std::strlen(how), ", %s",
                          format(text(T_LOSS), m.lossPercent));
        const ImVec2 hs = text_size(font, how);
        text_at(dl, font, ImVec2(xr - hs.x, y1 + (lineH - lh) * 0.5f),
                m.jitterMs > 20 || m.lossPercent >= 5 ? kOrange : kMuted, how);
    }
    ImGui::Dummy(ImVec2(0.0f, p * 0.1f));
}

bool other_pad_connected(const OnlineState& st)
{
    const unsigned mine = local_ports(st);
    for (u32 p = 0; p < PAD_CHANMAX; p++)
        if (s_dev[p].present && (mine & (1u << p)) == 0)
            return true;
    return false;
}

const char* room_where(const OnlineState& st) { return st.roomAddress; }

void draw_room(const OnlineState& st)
{
    const OnlineMachine* me = self_machine(st);
    const bool host = st.self >= 0 && st.self == st.host;
    const bool canSelect = host && st.machineCount >= 2 && PortOnlineDescCheck(&st.desc) == nullptr;
    OnlineMatchDesc d = st.desc;
    bool changed = false;
    begin_screen("##online.room", text(T_ROOM), nullptr, false, 30.0f);
    if (ImGui::BeginTable("teams", 2, ImGuiTableFlags_SizingStretchSame))
    {
        for (int side = 0; side < 2; ++side)
        {
            ImGui::TableNextColumn();
            ImGui::PushID(side);
            ImGui::PushStyleColor(ImGuiCol_Text, side == 0 ? kHome : kRed);
            ImGui::TextUnformatted(side_name(side));
            ImGui::PopStyleColor();
            int players = 0;
            for (int m = 0; m < ONLINE_MAX_MACHINES; ++m)
                if (st.machines[m].present)
                    for (int h = 0; h < st.machines[m].humanCount; ++h)
                    {
                        const OnlineHuman& human = st.machines[m].humans[h];
                        if (human.side != side)
                            continue;
                        ++players;
                        ImGui::PushID(human.slot);
                        const char* name =
                            format("%s%s", human.name,
                                   st.machines[m].isSelf ? format(" %s", text(T_YOU)) : "");
                        if (host)
                        {
                            if (spin_row("side", name, side_name(side), true))
                            {
                                d.padSide[human.slot] = 1 - side;
                                changed = true;
                            }
                        }
                        else
                            ImGui::TextWrapped("%s", name);
                        ImGui::PopID();
                    }
            if (!players)
                muted_text(text(T_CPU));
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::Dummy(ImVec2(0, px() * 0.4f));
    muted_text(summary(st.desc));
    if (changed)
        PortOnlineSetDesc(&d);
    if (st.desync == ONLINE_DESYNC_ENDED)
        ImGui::TextWrapped("%s", text(T_DESYNC_ENDED));
    if (host)
    {
        if (canSelect)
        {
            if (row("start", text(T_SELECT_TEAMS), nullptr, true, nullptr, true))
                PortOnlineSelect(ONLINE_SELECT_BEGIN, 0, 0, 0, 0);
            ImGui::SetItemDefaultFocus();
        }
        else
            ImGui::TextWrapped("%s", text(T_WAITING_FOR_PLAYERS));
    }
    else
        ImGui::TextWrapped("%s", format(text(T_CHOOSE_TEAMS_WAIT), machine_name(st, st.host)));
    if (row("invite", text(T_INVITE)))
        s_roomScreen = SCR_ADDRESS;
    if (!canSelect)
        ImGui::SetItemDefaultFocus();
    if (me && me->humanCount < ONLINE_MAX_LOCAL_HUMANS)
    {
        const char* sub =
            ImGui::GetTime() < s_addWaitUntil
                ? text(other_pad_connected(st) ? T_ADD_PLAYER_WAIT : T_ADD_PLAYER_NO_PAD)
                : nullptr;
        if (row("add", text(T_ADD_PLAYER), sub))
            s_addWaitUntil = ImGui::GetTime() + 15.0;
    }
    if (host && row("match", text(T_MATCH_SETTINGS)))
        s_roomScreen = SCR_OPTIONS;
    if (row("details", text(T_CONNECTION_DETAILS)))
        s_roomScreen = SCR_SETUP;
    if (row("leave", text(T_LEAVE)))
        s_leaveAsked = true;
    const Hint hints[] = { { PAD_BUTTON_A, T_HINT_SELECT }, { PAD_BUTTON_B, T_HINT_LEAVE } };
    end_screen(hints, 2);
    if (pad_pressed(PAD_BUTTON_Y))
        s_roomScreen = SCR_ADDRESS;
    if (pad_pressed(PAD_BUTTON_START))
        s_addWaitUntil = ImGui::GetTime() + 15.0;
    if (back_pressed())
        s_leaveAsked = true;
}

// A second person on this machine joins with A on their own pad and leaves with B.
void local_players(const OnlineState& st)
{
    const OnlineMachine* me = self_machine(st);
    if (me == nullptr || !s_open || !s_focused ||
        (st.phase != ONLINE_PHASE_HOSTING && st.phase != ONLINE_PHASE_ROOM))
        return;
    const unsigned mine = local_ports(st);
    for (u32 p = 0; p < PAD_CHANMAX; p++)
    {
        if (((mine | s_navPorts) & (1u << p)) != 0 || !s_dev[p].present ||
            (s_pressed[p] & PAD_BUTTON_A) == 0)
            continue;
        if (me->humanCount < ONLINE_MAX_LOCAL_HUMANS)
        {
            PortOnlineAddLocalPlayer((int)p, nullptr);
            s_addWaitUntil = 0.0;
        }
    }
    for (int h = 1; h < me->humanCount && h < ONLINE_MAX_LOCAL_HUMANS; h++)
    {
        const int port = me->humans[h].localPort;
        if (port >= 0 && port < PAD_CHANMAX && (s_pressed[port] & PAD_BUTTON_B) != 0)
            PortOnlineRemoveLocalPlayer(port);
    }
}

void draw_match_settings(const OnlineState& st)
{
    if (st.self < 0 || st.self != st.host)
    {
        s_roomScreen = SCR_ROOM;
        return;
    }
    OnlineMatchDesc d = st.desc;
    bool changed = false;
    begin_screen("##online.options", text(T_MATCH_SETTINGS), nullptr, false, 33.0f);
    const int step =
        spin_row("length", text(T_MATCH_LENGTH), format(text(T_MINUTES), d.gameTime / 60), true);
    if (step != 0)
    {
        d.gameTime = PortOnlineGameTimeStep(d.gameTime, step);
        changed = true;
    }
    if (spin_row("pause", text(T_PAUSE_ALLOWED), text(d.pauseAllowed ? T_ON : T_OFF), true))
    {
        d.pauseAllowed = !d.pauseAllowed;
        changed = true;
    }
    section(text(T_ITEMS));
    // The game's Twister setting turns off red shells.
    static const struct
    {
        uint8_t bits;
        Text label;
    } kItems[] = {
        { ONLINE_POWERUP_RED_SHELLS | ONLINE_POWERUP_TWISTER, T_ITEM_RED_SHELLS },
        { ONLINE_POWERUP_GREEN_SHELLS, T_ITEM_GREEN_SHELLS },
        { ONLINE_POWERUP_BLUE_SHELLS, T_ITEM_FREEZE_SHELLS },
        { ONLINE_POWERUP_SPINY_SHELLS, T_ITEM_SPINY_SHELLS },
        { ONLINE_POWERUP_STARMAN, T_ITEM_STARS },
        { ONLINE_POWERUP_BOBOMBS, T_ITEM_BOBOMBS },
        { ONLINE_POWERUP_BANANAS, T_ITEM_BANANAS },
    };
    for (int i = 0; i < 7; ++i)
    {
        const bool on = (d.powerupMask & kItems[i].bits) == kItems[i].bits;
        ImGui::PushID(i);
        if (spin_row("item", text(kItems[i].label), text(on ? T_ON : T_OFF), true))
        {
            d.powerupMask = on ? d.powerupMask & ~kItems[i].bits : d.powerupMask | kItems[i].bits;
            changed = true;
        }
        ImGui::PopID();
    }
    if (changed)
        PortOnlineSetDesc(&d);
    ImGui::Dummy(ImVec2(0.0f, px() * 0.2f));
    const bool done = row("done", text(T_DONE));
    const Hint hints[] = { { PAD_BUTTON_B, T_HINT_BACK } };
    end_screen(hints, 1);
    if (done || back_pressed())
        s_roomScreen = SCR_ROOM;
}

void draw_setup(const OnlineState& st)
{
    begin_screen("##online.details", text(T_CONNECTION_DETAILS), nullptr);
    for (int i = 0; i < ONLINE_MAX_MACHINES; ++i)
        if (st.machines[i].present)
            machine_card(st, i);
    muted_text(format(text(st.delayAuto ? T_INPUT_DELAY_AUTO : T_INPUT_DELAY), st.delayMs));
    muted_text(format(text(T_BUILD_LINE),
                      text(port_region() == PORT_REGION_JAPAN ? T_DATA_JAPAN : T_DATA_USA_EUROPE),
                      STRIKERS_VERSION_STR));
    const bool done = row("done", text(T_DONE));
    const Hint hints[] = { { PAD_BUTTON_B, T_HINT_BACK } };
    end_screen(hints, 1);
    if (done || back_pressed())
        s_roomScreen = SCR_ROOM;
}

void big_address(const char* address)
{
    ImFont* big = s_fonts->big;
    const ImVec2 ts = text_size(big, address);
    const float w = ImGui::GetContentRegionAvail().x;
    const float scale = ts.x > w ? w / ts.x : 1.0f;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddText(big, big->FontSize * big->Scale * scale,
                                        ImVec2(at.x + ImMax(0.0f, (w - ts.x) * 0.5f), at.y),
                                        col(kGold), address);
    ImGui::Dummy(ImVec2(w, ts.y * scale + px() * 0.7f));
}

void copy_row(const char* id, const char* address)
{
#if defined(PANEL_HAS_CLIPBOARD)
    static char copied[ONLINE_ADDRESS_MAX];
    static double copiedUntil = 0;
    const bool recent = ImGui::GetTime() < copiedUntil && std::strcmp(copied, address) == 0;
    if (row(id, text(recent ? T_COPIED : T_COPY_ADDRESS)))
    {
        SDL_SetClipboardText(address);
        std::snprintf(copied, sizeof copied, "%s", address);
        copiedUntil = ImGui::GetTime() + 2.0;
    }
#else
    (void)id;
    (void)address;
#endif
}

void public_status(int state)
{
    const char* s = state == ONLINE_PUBLIC_LOOKING  ? text(T_PUBLIC_LOOKING)
                    : state == ONLINE_PUBLIC_OPEN   ? text(T_PUBLIC_OPEN)
                    : state == ONLINE_PUBLIC_FOUND  ? text(T_PUBLIC_FOUND)
                    : state == ONLINE_PUBLIC_SHARED ? text(T_PUBLIC_SHARED)
                                                    : text(T_PUBLIC_FAILED);
    ImGui::PushStyleColor(ImGuiCol_Text, state == ONLINE_PUBLIC_LOOKING ? kMuted
                                         : state == ONLINE_PUBLIC_OPEN  ? kGreen
                                                                        : kOrange);
    ImGui::TextWrapped("%s", s);
    ImGui::PopStyleColor();
}

void draw_address(const OnlineState& st)
{
    static bool showNetworks = false;
    const bool host = st.self >= 0 && st.self == st.host;
    const int pub = host ? st.publicState : ONLINE_PUBLIC_NONE;
    const bool havePublic = pub != ONLINE_PUBLIC_NONE && st.publicAddress[0] != '\0';
    const float rows =
        pub == ONLINE_PUBLIC_NONE ? (showNetworks ? 30.0f : 23.0f) : (showNetworks ? 37.0f : 30.0f);
    begin_screen("##online.address", text(T_INVITE), nullptr, pub == ONLINE_PUBLIC_LOOKING, rows);
    const char* lan = room_where(st);
    if (pub == ONLINE_PUBLIC_NONE)
    {
        ImGui::Dummy(ImVec2(0.0f, px() * 0.8f));
        big_address(lan);
        copy_row("copy", lan);
        if (host)
            muted_text(text(T_ADDRESS_BIG_HINT));
    }
    else if (havePublic)
    {
        section(text(T_FOR_INTERNET));
        big_address(st.publicAddress);
        copy_row("copy", st.publicAddress);
        public_status(pub);
        muted_text(text(T_SHARE_PRIVATELY));
        section(text(T_FOR_THIS_NETWORK));
        ImGui::PushFont(s_fonts->head);
        ImGui::TextUnformatted(lan);
        ImGui::PopFont();
        copy_row("copy.lan", lan);
    }
    else
    {
        section(text(T_FOR_THIS_NETWORK));
        big_address(lan);
        copy_row("copy", lan);
        section(text(T_FOR_INTERNET));
        public_status(pub);
    }
    if (host && st.roomOtherAddressCount > 0)
    {
        ImGui::Dummy(ImVec2(0.0f, px() * 0.5f));
        if (row("networks", text(T_OTHER_NETWORKS), nullptr, true, showNetworks ? "-" : "+"))
            showNetworks = !showNetworks;
        if (showNetworks)
        {
            muted_text(text(T_OTHER_ADDRESSES));
            for (int i = 0; i < st.roomOtherAddressCount; i++)
                ImGui::TextUnformatted(st.roomOtherAddresses[i]);
        }
    }
    const Hint hints[] = { { PAD_BUTTON_B, T_HINT_BACK } };
    end_screen(hints, 1);
    if (back_pressed() || pad_pressed(PAD_BUTTON_Y))
        s_roomScreen = SCR_ROOM;
}

void draw_leave(const OnlineState& st)
{
    const bool host = st.self >= 0 && st.self == st.host;
    Text question = T_LEAVE_ROOM_Q;
    if (st.phase == ONLINE_PHASE_MATCH)
        question = T_LEAVE_MATCH_Q;
    else if (host && st.machineCount > 1)
        question = T_CLOSE_ROOM_Q;
    begin_screen("##online.leave", text(T_LEAVE), nullptr, false, 17.0f);
    ImGui::Dummy(ImVec2(0.0f, px() * 0.3f));
    ImGui::PushFont(s_fonts->head);
    ImGui::TextWrapped("%s", text(question));
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, px() * 0.3f));
    const bool stay = row("stay", text(T_STAY));
    ImGui::SetItemDefaultFocus();
    const bool leave = row("leave", text(st.phase == ONLINE_PHASE_MATCH ? T_LEAVE_MATCH : T_LEAVE));
    const Hint hints[] = { { PAD_BUTTON_A, T_HINT_SELECT }, { PAD_BUTTON_B, T_HINT_BACK } };
    end_screen(hints, 2);
    if (leave)
    {
        s_leaveAsked = false;
        PortOnlineLeave();
    }
    else if (stay || back_pressed())
        s_leaveAsked = false;
}

void draw_loading(const OnlineState& st)
{
    begin_screen("##online.loading", text(T_GETTING_READY), nullptr, true);
    muted_text(summary(st.desc));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = s_fonts->body;
    const float p = px();
    bool allAcked = true;
    for (int i = 0; i < ONLINE_MAX_MACHINES; i++)
    {
        const OnlineMachine& m = st.machines[i];
        if (!m.present)
            continue;
        allAcked = allAcked && m.descAcked;
        const float w = ImGui::GetContentRegionAvail().x;
        const float h = p * 2.9f;
        const ImVec2 a = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(w, h));
        dl->AddRectFilled(a, ImVec2(a.x + w, a.y + h), col(kRaised),
                          ImGui::GetStyle().ChildRounding);
        const char* name =
            m.isSelf ? format("%s %s", machine_name(st, i), text(T_YOU)) : machine_name(st, i);
        text_at(dl, font, ImVec2(a.x + p * 0.9f, a.y + p * 0.4f), kText, name);
        const char* state = text(T_LOAD_WAITING);
        float frac = 0.0f;
        ImVec4 c = kHome;
        switch (m.loadState)
        {
        case ONLINE_LOAD_LOADING:
            state = format(text(T_LOAD_LOADING), m.loadPercent);
            frac = (float)m.loadPercent / 100.0f;
            break;
        case ONLINE_LOAD_PIPELINES:
            state = format(text(T_LOAD_PIPELINES), m.loadPercent);
            frac = (float)m.loadPercent / 100.0f;
            c = kGold;
            break;
        case ONLINE_LOAD_READY:
            state = text(T_LOAD_READY);
            frac = 1.0f;
            c = kGreen;
            break;
        default: break;
        }
        const ImVec2 ss = text_size(font, state);
        text_at(dl, font, ImVec2(a.x + w - p * 0.9f - ss.x, a.y + p * 0.4f),
                m.loadState == ONLINE_LOAD_READY ? kGreen : kMuted, state);
        progress_bar(dl, ImVec2(a.x + p * 0.9f, a.y + h - p * 0.95f),
                     ImVec2(a.x + w - p * 0.9f, a.y + h - p * 0.55f), frac, c);
    }
    if (!allAcked)
        muted_text(text(T_CHECKING_SETUP));

    bool keepWaiting = false, leave = false;
    if (st.problem == ONLINE_PROBLEM_SLOW_LOADING &&
        (s_slowLoadDismissedAt < 0.0f || st.problemSeconds > s_slowLoadDismissedAt + 30.0f))
    {
        ImGui::Dummy(ImVec2(0.0f, p * 0.3f));
        ImGui::PushStyleColor(ImGuiCol_Text, kOrange);
        ImGui::TextWrapped(text(T_STILL_LOADING), machine_name(st, st.problemMachine),
                           (int)st.problemSeconds);
        ImGui::PopStyleColor();
        if (ImGui::GetFocusID() == 0)
            ImGui::SetKeyboardFocusHere();
        keepWaiting = row("wait", text(T_KEEP_WAITING));
        leave = row("leave", text(T_LEAVE));
    }
    const Hint hints[] = { { PAD_BUTTON_B, T_HINT_LEAVE } };
    end_screen(hints, 1);
    if (keepWaiting)
        s_slowLoadDismissedAt = st.problemSeconds;
    if (leave || back_pressed())
        s_leaveAsked = true;
}

int worst_ping(const OnlineState& st)
{
    int ping = 0;
    for (int i = 0; i < ONLINE_MAX_MACHINES; i++)
        if (st.machines[i].present && !st.machines[i].isSelf && st.machines[i].pingMs > ping)
            ping = st.machines[i].pingMs;
    return ping;
}

const char* stats_line(const OnlineState& st)
{
    const char* line = format(text(T_STATS), worst_ping(st), st.delayMs, st.stallsLastMinute,
                              (double)st.advanceTicks);
    if (st.inputToEffectMs > 0)
        line = format("%s%s", line, format(text(T_STATS_INPUT), st.inputToEffectMs));
    return line;
}

// The named in-match problem, or NULL while the connection is fine.
const char* problem_line(const OnlineState& st)
{
    const char* who = machine_name(st, st.problemMachine);
    switch (st.problem)
    {
    case ONLINE_PROBLEM_WAITING:
        return format(text(T_PROBLEM_WAITING), who, (int)std::ceil(st.problemSeconds));
    case ONLINE_PROBLEM_SLOW_DEVICE:  return format(text(T_PROBLEM_SLOW_DEVICE), who);
    case ONLINE_PROBLEM_HIGH_LATENCY: return text(T_PROBLEM_HIGH_LATENCY);
    case ONLINE_PROBLEM_UNSTABLE:     return text(T_PROBLEM_UNSTABLE);
    case ONLINE_PROBLEM_PACKET_LOSS:  return text(T_PROBLEM_PACKET_LOSS);
    default:                          return nullptr;
    }
}

void draw_match(const OnlineState& st)
{
    begin_screen("##online.match", text(T_IN_MATCH), nullptr);
    muted_text(summary(st.desc));
    muted_text(stats_line(st));
    if (const char* line = problem_line(st))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kOrange);
        ImGui::TextWrapped("%s", line);
        ImGui::PopStyleColor();
    }
    if (st.desync != ONLINE_DESYNC_NONE)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kOrange);
        ImGui::TextWrapped("%s", text(T_DESYNC_ENDED));
        ImGui::PopStyleColor();
    }
    ImGui::Dummy(ImVec2(0.0f, px() * 0.5f));
    const bool back = row("back", text(T_BACK_TO_GAME));
    const bool leave = row("leave", text(T_LEAVE_MATCH));
    const Hint hints[] = { { PAD_BUTTON_A, T_HINT_SELECT }, { PAD_BUTTON_B, T_HINT_BACK } };
    end_screen(hints, 2);
    if (leave)
        s_leaveAsked = true;
    else if (back || back_pressed())
        set_open(false);
}

const char* vote_name(int vote)
{
    switch (vote)
    {
    case ONLINE_VOTE_REMATCH:      return text(T_REMATCH);
    case ONLINE_VOTE_CHANGE_TEAMS: return text(T_CHANGE_TEAMS);
    case ONLINE_VOTE_BACK_TO_ROOM: return text(T_BACK_TO_ROOM);
    case ONLINE_VOTE_LEAVE:        return text(T_LEAVE);
    default:                       return text(T_NOT_VOTED);
    }
}

void draw_post(const OnlineState& st)
{
    const char* left = st.voteSeconds > 0.0f
                           ? format(text(T_VOTE_SECONDS), (int)std::ceil(st.voteSeconds))
                           : nullptr;
    begin_screen("##online.post", text(T_FULL_TIME), left);
    muted_text(summary(st.desc));
    const OnlineMachine* me = self_machine(st);
    const int mine = me != nullptr && me->humanCount > 0 ? me->humans[0].vote : ONLINE_VOTE_NONE;
    const bool rematch = row("rematch", text(T_REMATCH), nullptr, true,
                             mine == ONLINE_VOTE_REMATCH ? text(T_VOTED) : nullptr, true);
    const bool room = row("room", text(T_BACK_TO_ROOM), nullptr, true,
                          mine == ONLINE_VOTE_BACK_TO_ROOM ? text(T_VOTED) : nullptr);
    const bool leave = row("leave", text(T_LEAVE));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < ONLINE_MAX_MACHINES; i++)
    {
        const OnlineMachine& m = st.machines[i];
        if (!m.present)
            continue;
        for (int h = 0; h < m.humanCount && h < ONLINE_MAX_LOCAL_HUMANS; h++)
        {
            const OnlineHuman& hu = m.humans[h];
            const float lh = px() * 1.6f;
            const ImVec2 a = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, lh));
            side_chip(dl, ImVec2(a.x, a.y + px() * 0.2f), lh - px() * 0.4f, hu.slot, hu.side);
            text_at(dl, s_fonts->body, ImVec2(a.x + lh * 1.9f, a.y + px() * 0.25f), kText, hu.name);
            text_at(dl, s_fonts->body, ImVec2(a.x + px() * 14.0f, a.y + px() * 0.25f),
                    hu.vote == ONLINE_VOTE_NONE ? kMuted : kGold, vote_name(hu.vote));
        }
    }
    const Hint hints[] = { { PAD_BUTTON_A, T_HINT_SELECT } };
    end_screen(hints, 1);
    if (rematch)
        PortOnlineVote(-1, ONLINE_VOTE_REMATCH);
    else if (room)
        PortOnlineVote(-1, ONLINE_VOTE_BACK_TO_ROOM);
    else if (leave)
        s_leaveAsked = true;
}

void draw_opponents(const OnlineState&)
{
    begin_screen("##online.opponents", text(T_OPPONENTS_LEFT), nullptr);
    ImGui::Dummy(ImVec2(0.0f, px() * 1.5f));
    const bool cpu = row("cpu", text(T_CONTINUE_CPU), nullptr, true, nullptr, true);
    const bool end = row("end", text(T_END_MATCH));
    const Hint hints[] = { { PAD_BUTTON_A, T_HINT_SELECT } };
    end_screen(hints, 1);
    if (cpu)
        PortOnlineAfterOpponentsLeft(1);
    else if (end)
        PortOnlineAfterOpponentsLeft(0);
}

// A private IPv4 address typed by hand; a room Nearby found is on this network anyway.
bool typed_home_address(const OnlineState& st)
{
    for (int i = 0; i < st.nearbyCount && i < ONLINE_MAX_NEARBY; i++)
        if (std::strcmp(st.nearby[i].address, s_address.text) == 0)
            return false;
    char host[ONLINE_ADDRESS_MAX];
    uint16_t port = 0;
    online::NetAddr a;
    if (!online::AddrSplit(s_address.text, host, sizeof host, &port, ONLINE_DEFAULT_PORT) ||
        !online::AddrParseNumeric(host, port, &a) || a.family != 4)
        return false;
    return a.ip[0] == 10 || (a.ip[0] == 172 && (a.ip[1] & 0xF0) == 16) ||
           (a.ip[0] == 192 && a.ip[1] == 168);
}

void draw_error(const OnlineState& st)
{
    begin_screen("##online.error", text(T_ONLINE), nullptr);
    ImGui::Dummy(ImVec2(0.0f, px() * 1.5f));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float p = px();
    const ImVec2 a = ImGui::GetCursorScreenPos();
    const float s = p * 2.2f;
    dl->AddTriangleFilled(ImVec2(a.x + s * 0.5f, a.y), ImVec2(a.x, a.y + s * 0.9f),
                          ImVec2(a.x + s, a.y + s * 0.9f), col(kOrange));
    const ImVec2 bang = text_size(s_fonts->head, "!");
    text_at(dl, s_fonts->head, ImVec2(a.x + (s - bang.x) * 0.5f, a.y + s * 0.62f - bang.y * 0.5f),
            kInk, "!");
    ImGui::SetCursorScreenPos(ImVec2(a.x + s + p * 0.8f, a.y));
    ImGui::PushFont(s_fonts->head);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(error_sentence(st));
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(
        ImVec2(a.x, ImMax(ImGui::GetCursorScreenPos().y, a.y + s) + p * 1.5f));
    if (st.error == ONLINE_ERR_UNREACHABLE && typed_home_address(st))
        muted_text(text(T_ERR_HOME_ADDRESS));
    const bool ok = row("ok", text(T_OK));
    const Hint hints[] = { { PAD_BUTTON_A, T_OK } };
    end_screen(hints, 1);
    if (ok || back_pressed())
        PortOnlineLeave();
}

// Lines from the top of the screen to just under the match HUD's score band.
const float kBelowHud = 6.6f;

struct Toast
{
    char text[160];
    double until;
};
Toast s_toasts[3];

void push_toast(const char* s)
{
    for (int i = 2; i > 0; i--)
        s_toasts[i] = s_toasts[i - 1];
    std::snprintf(s_toasts[0].text, sizeof s_toasts[0].text, "%s", s);
    s_toasts[0].until = ImGui::GetTime() + 4.0;
}

const ImGuiWindowFlags kOverlayFlags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize;

bool begin_overlay(const char* id, ImVec2 pos, ImVec2 pivot)
{
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);
    ImGui::SetNextWindowBgAlpha(0.9f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px() * 0.8f, px() * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    const bool shown = ImGui::Begin(id, nullptr, kOverlayFlags);
    ImGui::PopStyleVar(2);
    if (shown)
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    return shown;
}

void draw_toasts()
{
    const ImGuiIO& io = ImGui::GetIO();
    const double now = ImGui::GetTime();
    // Above the open panel, else below the match HUD's top band.
    float y = px() * (s_open ? 0.25f : kBelowHud);
    for (int i = 0; i < 3; i++)
    {
        if (s_toasts[i].until <= now)
            continue;
        char id[24];
        std::snprintf(id, sizeof id, "##online.toast%d", i);
        if (begin_overlay(id, ImVec2(io.DisplaySize.x * 0.5f, y), ImVec2(0.5f, 0.0f)))
        {
            ImGui::TextUnformatted(s_toasts[i].text);
            y += ImGui::GetWindowHeight() + px() * 0.4f;
        }
        ImGui::End();
    }
}

// Bottom left of the main menu: the key or button that opens the panel.
void draw_menu_prompt()
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::PushFont(s_fonts->body);
    const float p = px();
    if (begin_overlay("##online.menu", ImVec2(p, io.DisplaySize.y - p), ImVec2(0.0f, 1.0f)))
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 a = ImGui::GetCursorScreenPos();
        const float h = p * 1.35f;
        float x = a.x;
        unsigned w = 0, ht = 0;
        const unsigned char* back = PortPromptsBackImage(&w, &ht);
        const auto glyph = s_promptTextures.find(back);
        if (PortPromptsKeyboardActive())
            x += key_glyph(dl, ImVec2(x, a.y), h, "F5");
        else if (back != nullptr && glyph != s_promptTextures.end())
        {
            const float gw = h * (float)w / (float)ht;
            dl->AddImage(glyph->second, ImVec2(x, a.y), ImVec2(x + gw, a.y + h));
            x += gw;
        }
        else
            for (unsigned bit :
                 { (unsigned)PAD_TRIGGER_L, (unsigned)PAD_TRIGGER_R, (unsigned)PAD_TRIGGER_Z })
                x += button_glyph(dl, ImVec2(x, a.y), h, bit) + p * 0.15f;
        x += p * 0.4f;
        const char* label = text(T_ONLINE);
        const ImVec2 ts = text_size(s_fonts->body, label);
        text_at(dl, s_fonts->body, ImVec2(x, a.y + (h - ts.y) * 0.5f), kText, label);
        ImGui::Dummy(ImVec2(x + ts.x - a.x, h));
    }
    ImGui::End();
    ImGui::PopFont();
}

void notice_toast(const OnlineState& st)
{
    if (st.noticeSerial == s_lastNotice)
        return;
    s_lastNotice = st.noticeSerial;
    const char* who = st.noticeName[0] ? st.noticeName : machine_name(st, st.noticeMachine);
    switch (st.notice)
    {
    case ONLINE_NOTICE_JOINED:         push_toast(format(text(T_NOTICE_JOINED), who)); break;
    case ONLINE_NOTICE_LEFT:           push_toast(format(text(T_NOTICE_LEFT), who)); break;
    case ONLINE_NOTICE_DROPPED_TO_CPU: push_toast(format(text(T_NOTICE_DROPPED), who)); break;
    case ONLINE_NOTICE_SETUP_MISMATCH: push_toast(text(T_NOTICE_SETUP_MISMATCH)); break;
    case ONLINE_NOTICE_REMATCH:        push_toast(text(T_NOTICE_REMATCH)); break;
    default:                           break;
    }
}

// A named problem shows in the corner only while it holds, and on the open panel instead.
void draw_match_status(const OnlineState& st)
{
    if (st.phase != ONLINE_PHASE_MATCH || s_open)
        return;
    const ImGuiIO& io = ImGui::GetIO();
    const float p = px();
    if (const char* line = problem_line(st))
    {
        if (begin_overlay("##online.problem", ImVec2(io.DisplaySize.x - p, p * kBelowHud),
                          ImVec2(1.0f, 0.0f)))
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 a = ImGui::GetCursorScreenPos();
            const float h = ImGui::GetTextLineHeight();
            if (st.problem == ONLINE_PROBLEM_WAITING)
                spinner(dl, ImVec2(a.x + h * 0.5f, a.y + h * 0.5f), h * 0.38f, col(kGold));
            else
                ping_bars(dl, ImVec2(a.x, a.y + h * 0.1f), h * 0.8f, 1000);
            ImGui::Dummy(ImVec2(h * 1.1f, h));
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  st.problem == ONLINE_PROBLEM_WAITING ? kGold : kOrange);
            ImGui::TextUnformatted(line);
            ImGui::PopStyleColor();
        }
        ImGui::End();
    }
    if (st.settings.showStats)
    {
        if (begin_overlay("##online.stats", ImVec2(p, io.DisplaySize.y - p), ImVec2(0.0f, 1.0f)))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
            ImGui::TextUnformatted(stats_line(st));
            ImGui::PopStyleColor();
        }
        ImGui::End();
    }
    if (st.desync != ONLINE_DESYNC_NONE)
    {
        if (begin_overlay("##online.desync",
                          ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.3f),
                          ImVec2(0.5f, 0.5f)))
        {
            ImGui::PushFont(s_fonts->head);
            ImGui::TextUnformatted(text(T_DESYNC_ENDED));
            ImGui::PopFont();
            if (st.desyncReport[0] != '\0')
            {
                ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
                ImGui::TextUnformatted(format(text(T_DESYNC_REPORT), st.desyncReport));
                ImGui::PopStyleColor();
            }
        }
        ImGui::End();
    }
    // A player on another machine has the pause menu.
    const int paused = PortSimPauseSlot();
    if (paused >= 0 && PortOnlineSlotLocalPort(paused) < 0)
    {
        if (begin_overlay("##online.paused",
                          ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.4f),
                          ImVec2(0.5f, 0.5f)))
        {
            ImGui::PushFont(s_fonts->head);
            ImGui::TextUnformatted(format(text(T_PAUSED_BY), PortOnlineSlotName(paused)));
            ImGui::PopFont();
        }
        ImGui::End();
    }
}

// These bands sit between the native heading, nameplates, portraits, ticker and button bar.
void selection_band(ImVec2 a, ImVec2 b, const char* value, const ImVec4& ink)
{
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* font = s_fonts->body;
    const float pad = px() * .35f;
    const float width = b.x - a.x - 2 * pad;
    const float height = b.y - a.y - 2 * pad;
    if (width <= 0 || height <= 0)
        return;
    float size = font->FontSize * font->Scale;
    ImVec2 measured = font->CalcTextSizeA(size, 1e9f, width, value);
    while (measured.y > height || measured.x > width)
    {
        size *= .95f;
        measured = font->CalcTextSizeA(size, 1e9f, width, value);
    }
    dl->PushTextureID(font->ContainerAtlas->TexID);
    dl->AddRectFilled(a, b, col(kInk, .94f));
    dl->PushClipRect(a, b, true);
    dl->AddText(font, size, ImVec2(a.x + pad, a.y + (b.y - a.y - measured.y) * .5f), col(ink),
                value, nullptr, width);
    dl->PopClipRect();
    dl->PopTextureID();
}

void draw_selection(const OnlineState& st)
{
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    const bool options = PortOnlineGameOptionsOpen() != 0;
    if (options)
    {
        selection_band(ImVec2(screen.x * .12f, screen.y * .06f),
                       ImVec2(screen.x * .88f, screen.y * .13f), text(T_LOCAL_RUMBLE), kText);
        return;
    }
    if (st.selection.stage == 1)
    {
        for (int side = 0; side < 2; ++side)
        {
            char names[128] = {};
            bool local = false;
            for (int m = 0; m < ONLINE_MAX_MACHINES; ++m)
                if (st.machines[m].present)
                    for (int h = 0; h < st.machines[m].humanCount; ++h)
                    {
                        const OnlineHuman& person = st.machines[m].humans[h];
                        if (person.side != side)
                            continue;
                        local = local || st.machines[m].isSelf;
                        const size_t used = std::strlen(names);
                        std::snprintf(names + used, sizeof names - used, "%s%s", used ? " + " : "",
                                      person.name);
                    }
            const int phase = st.selection.phase[side];
            const char* status = text(phase == 3   ? T_READY
                                      : phase == 2 ? T_CHOOSING_SIDEKICK
                                                   : T_CHOOSING_CAPTAIN);
            const char* label = format("%s%s: %s", names[0] ? names : text(T_CPU_HOST),
                                       local ? format(" %s", text(T_YOU)) : "", status);
            selection_band(ImVec2(screen.x * (side ? .515f : .10f), screen.y * .274f),
                           ImVec2(screen.x * (side ? .90f : .485f), screen.y * .325f), label,
                           side ? kRed : kHome);
        }
        if (st.selection.optionsOpen)
            selection_band(ImVec2(screen.x * .12f, screen.y * .13f),
                           ImVec2(screen.x * .88f, screen.y * .175f),
                           format("%s: %s", machine_name(st, st.host), text(T_RULES)), kText);
    }
    if (st.selection.stage == 2)
    {
        const char* stage = st.self == st.host
                                ? text(T_STAGE_STADIUM)
                                : format(text(T_SELECTING_STADIUM), machine_name(st, st.host));
        const char* help = text(T_SHARED_BACK);
        selection_band(ImVec2(screen.x * .12f, screen.y * .755f),
                       ImVec2(screen.x * .88f, screen.y * .828f), format("%s\n%s", stage, help),
                       kText);
    }
}

// Opens the panel where a player has to act, and closes it when play starts.
void follow_session(const OnlineState& st)
{
    const int phase = st.phase;
    if (phase != s_lastPhase)
    {
        s_leaveAsked = false;
        if (s_lastPhase == ONLINE_PHASE_JOINING && phase == ONLINE_PHASE_ROOM)
            remember(st.roomAddress[0] != '\0' ? st.roomAddress : s_address.text);
        const bool wasRoom =
            s_lastPhase == ONLINE_PHASE_HOSTING || s_lastPhase == ONLINE_PHASE_ROOM;
        switch (phase)
        {
        case ONLINE_PHASE_HOSTING:
        case ONLINE_PHASE_ROOM:
            if (!wasRoom)
            {
                s_roomScreen = SCR_ROOM;
                s_addWaitUntil = 0.0;
            }
            s_idleScreen = SCR_HOME;
            break;
        case ONLINE_PHASE_LOADING:
            s_slowLoadDismissedAt = -1.0f;
            set_open(true);
            break;
        case ONLINE_PHASE_MATCH:      set_open(false); break;
        case ONLINE_PHASE_POST_MATCH:
        case ONLINE_PHASE_ERROR:      set_open(true); break;
        default:                      break;
        }
        s_lastPhase = phase;
    }
    if (st.opponentsLeft && !s_lastOpponentsLeft)
        set_open(true);
    s_lastOpponentsLeft = st.opponentsLeft;
    notice_toast(st);
}

void draw_panel(const OnlineState& st)
{
    s_padOwned = false;
    s_textActive = false;

    // In a room the first player's pad drives the panel, or any other pad while that one is absent.
    s_navPorts = 0xF;
    if (in_room_phase(st))
        if (const OnlineMachine* me = self_machine(st))
            if (me->humanCount > 0)
            {
                const int first = me->humans[0].localPort;
                if (first >= 0 && first < PAD_CHANMAX && s_dev[first].present)
                    s_navPorts = 1u << first;
                else
                    s_navPorts = 0xFu & ~local_ports(st);
            }

    local_players(st);
    switch (screen_for(st))
    {
    case SCR_HOME:       draw_home(st); break;
    case SCR_JOIN:       draw_join(st); break;
    case SCR_NEARBY:     draw_nearby(st); break;
    case SCR_SETTINGS:   draw_settings(st); break;
    case SCR_EDIT_NAME:
    case SCR_EDIT_PORT:  draw_edit(st, screen_for(st)); break;
    case SCR_CONNECTING: draw_connecting(st); break;
    case SCR_ROOM:       draw_room(st); break;
    case SCR_SETUP:      draw_setup(st); break;
    case SCR_OPTIONS:    draw_match_settings(st); break;
    case SCR_ADDRESS:    draw_address(st); break;
    case SCR_LEAVE:      draw_leave(st); break;
    case SCR_LOADING:    draw_loading(st); break;
    case SCR_MATCH:      draw_match(st); break;
    case SCR_POST:       draw_post(st); break;
    case SCR_OPPONENTS:  draw_opponents(st); break;
    case SCR_ERROR:      draw_error(st); break;
    }
}

bool s_blocked;

void set_block(bool block)
{
    if (block == s_blocked)
        return;
    s_blocked = block;
    PADBlockInput(block);
}

void note_keyboard_use()
{
    const ImGuiIO& io = ImGui::GetIO();
    if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f || io.InputQueueCharacters.Size > 0 ||
        ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow) ||
        ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape) ||
        ImGui::IsKeyPressed(ImGuiKey_Tab))
        s_lastInputPad = false;
}

void init_once()
{
    static bool s_done;
    if (s_done)
        return;
    s_done = true;

    ImGuiContextHook hook;
    hook.Type = ImGuiContextHookType_NewFramePre;
    hook.Callback = on_new_frame;
    ImGui::AddContextHook(ImGui::GetCurrentContext(), &hook);
#if !defined(__SWITCH__)
    // The panel feeds ImGui the pads itself, through the game's mapping.
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual, nullptr, 0);
#endif

    load_recent();
}

}   // namespace
}   // namespace online_panel

using namespace online_panel;

extern "C" int PortOnlinePanelIdle(void)
{
    return !s_open && !s_toggleRequest &&
           (ImGui::GetCurrentContext() == nullptr || s_toasts[0].until <= ImGui::GetTime()) &&
           PortOnlineGetState()->phase == ONLINE_PHASE_IDLE;
}

extern "C" void PortOnlinePanelPrepare(void)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return;
    const auto upload = [](const unsigned char* pixels, unsigned w, unsigned h) {
        if (pixels && w && h && s_promptTextures.find(pixels) == s_promptTextures.end())
        {
            // Metal staging-buffer creation must wait for the render worker's encoding.
            aurora::gfx::synchronize();
            s_promptTextures.emplace(pixels, aurora_imgui_add_texture(w, h, pixels));
        }
    };
    const unsigned buttons[] = { PAD_BUTTON_A,  PAD_BUTTON_B,  PAD_BUTTON_X,  PAD_BUTTON_Y,
                                 PAD_TRIGGER_L, PAD_TRIGGER_R, PAD_TRIGGER_Z, PAD_BUTTON_START };
    for (unsigned button : buttons)
    {
        unsigned w = 0, h = 0;
        const unsigned char* pixels = PortPromptsMenuImage(button, &w, &h);
        upload(pixels, w, h);
    }
    unsigned w = 0, h = 0;
    const unsigned char* pixels = PortPromptsBackImage(&w, &h);
    upload(pixels, w, h);
}

extern "C" void PortOnlinePanelDraw(void)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return;
    init_once();
    if (PortOnlinePanelIdle())
    {
        s_startPressed = false;
        if (PortOnlineGameAtMainMenu())
        {
            update_fonts();
            if (s_fonts != nullptr)
            {
                ImGuiStyle saved = ImGui::GetStyle();
                ImGui::GetStyle() = s_style;
                draw_menu_prompt();
                ImGui::GetStyle() = saved;
            }
        }
        return;
    }
    update_fonts();
    if (s_fonts == nullptr)
        return;

    const OnlineState st = *PortOnlineGetState();
    follow_session(st);
    // In a match Start opens the panel, and play goes on behind it, unless the room allows pauses.
    if (s_startPressed && st.phase == ONLINE_PHASE_MATCH && !st.desc.pauseAllowed)
    {
        if (!s_open)
        {
            set_open(true);
            s_lastInputPad = true;
        }
        else if (s_focused && screen_for(st) == SCR_MATCH)
            set_open(false);
    }
    s_startPressed = false;
    // The full-time vote keeps the panel open, so no vote is cast from the game's menu behind it.
    const bool voting = st.phase == ONLINE_PHASE_POST_MATCH;
    if (s_toggleRequest)
    {
        s_toggleRequest = false;
        if (!s_open)
            set_open(true);
        else if (!s_focused)
            s_focusRequest = true;
        else if (!voting)
            set_open(false);
    }
    if (voting)
    {
        if (!s_open)
            set_open(true);
        else if (!s_focused)
            s_focusRequest = true;
    }

    ImGuiStyle saved = ImGui::GetStyle();
    ImGui::GetStyle() = s_style;
    ImGui::PushFont(s_fonts->body);
    draw_match_status(st);
    if (PortOnlineGameSelecting())
        draw_selection(st);
    draw_toasts();
    if (PortOnlineGameSelecting())
        set_open(false);
    if (s_open)
    {
        note_keyboard_use();
        ImDrawList* background = ImGui::GetBackgroundDrawList();
        const ImVec2 size = ImGui::GetIO().DisplaySize;
        const ImVec4 field(0.13f, .32f, .43f, 1);
        background->AddRectFilledMultiColor(ImVec2(0, 0), size, col(kRaised), col(field),
                                            col(kRaised), col(field));
        const ImVec2 origin(size.x * .52f, size.y * .48f);
        for (int ray = 0; ray < 28; ++ray)
        {
            const float angle = (float)ray * 6.2831853f / 28;
            const float reach = size.x + size.y;
            const ImVec2 a(origin.x + std::cos(angle) * reach, origin.y + std::sin(angle) * reach);
            const ImVec2 b(origin.x + std::cos(angle + .018f) * reach,
                           origin.y + std::sin(angle + .018f) * reach);
            background->AddTriangleFilled(origin, a, b, col(kHome, .07f));
        }
        background->AddRectFilled(ImVec2(0, 0), ImVec2(size.x, size.y * .075f), col(kRaised));
        background->AddRectFilled(ImVec2(0, size.y * .925f), size, col(kRaised));
        draw_panel(st);
    }
    else
        s_focused = false;
    ImGui::PopFont();
    ImGui::GetStyle() = saved;

    set_block(s_open && s_focused);
}

extern "C" int PortOnlinePanelHandleKey(int scancode, int down)
{
    if (!down || scancode != SDL_SCANCODE_F5)
        return 0;
    s_toggleRequest = true;
    s_lastInputPad = false;
    return 1;
}

extern "C" void PortOnlinePanelSetOpen(int open)
{
    set_open(open != 0);
    if (!open)
        set_block(false);
}

extern "C" int PortOnlinePanelOpen(void) { return s_open ? 1 : 0; }

extern "C" int PortOnlinePanelHasPads(void) { return s_blocked ? 1 : 0; }
