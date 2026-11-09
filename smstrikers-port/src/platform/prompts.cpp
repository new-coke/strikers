#include "port/prompts.h"

#if !defined(PORT_USE_AURORA)

extern "C" void PortPromptsFontLoaded(nlFont*) {}
extern "C" void PortPromptsFontUnloading(nlFont*) {}
extern "C" void PortPromptsEvent(const SDL_Event*) {}
extern "C" void PortPromptsFrame() {}
extern "C" const unsigned char* PortPromptsMenuImage(unsigned, unsigned*, unsigned*)
{
    return nullptr;
}
extern "C" const unsigned char* PortPromptsBackImage(unsigned*, unsigned*) { return nullptr; }
extern "C" int PortPromptsKeyboardActive(void) { return 0; }
extern "C" void PortPromptsLegendsLoaded() {}
extern "C" void PortPromptsLegendReloaded(unsigned long) {}
extern "C" void PortPromptsLegendsUnloading() {}
extern "C" int PortPromptsSetFamily(const char*) { return 0; }
extern "C" void PortPromptsPadInstances(const void* const*) {}
extern "C" uintptr_t PortPromptsPadTexture(const void*) { return 0; }

#else

#include "port/prompts_resolve.h"
#include "port/host.h"
#include "port/input.h"
#include "port/overlay.h"
#include "port/steamdeck.h"
#include "port/texture_packs.h"
#include "prompt_art.h"

#include "NL/nlFont.h"
#include "NL/nlMemory.h"
#include "NL/nlString.h"
#include "NL/glx/glxTexture.h"
#include "NL/gl/gl.h"

#include <SDL3/SDL.h>
#include <aurora/aurora.h>
#include <dolphin/pad.h>

#if defined(__SWITCH__)
#include <switch.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <map>
#include <new>
#include <set>
#include <string>
#include <vector>

namespace
{

using namespace prompts;

const unsigned kControls[Count] = { PAD_BUTTON_A, PAD_BUTTON_B,  PAD_BUTTON_X,
                                    PAD_BUTTON_Y, PAD_TRIGGER_L, PAD_TRIGGER_R };
const char kGlyphs[Count] = { '@', '~', '|', '+', '*', '^' };

const int kLegendCount = 6;
const unsigned long kLegends[kLegendCount] = { 0xecce7182, 0x288d277b, 0x12fa669a,
                                               0xd9104663, 0x3c4b529a, 0x627747b2 };
const Control kLegendControls[kLegendCount] = { A, B, Y, B, A, Y };

struct FontState
{
    nlFont* font;
    PlatTexture* page;
    unsigned long hash;
    nlFont::GlyphInfo originals[Count];
};

std::vector<FontState> s_fonts;
bool s_loaded[kLegendCount] = {};
std::array<unsigned char, 1568> s_originals[kLegendCount];
Icon s_icons[Count] = {};
bool s_initialized = false;
Family s_override = Auto;
int s_activePort = -2;
SDL_JoystickID s_activeId;
int s_axis[4][6] = {};
SDL_JoystickID s_axisId[4] = {};
std::string s_signature;
std::set<std::string> s_unboundLogs;
std::set<std::string> s_missingIcons;
bool s_disabledWarning;

std::string s_artDir;

// The choose-sides controller images are 64x64 texels over 64 of the 480 frame lines.
const unsigned kPadLines = 64;

const void* s_padInstances[4];
std::map<std::string, Image> s_padArt;
// Never rewritten or freed: a queued draw may still read one after the window is resized.
std::map<std::string, PlatTexture*> s_padTextures;

std::map<std::string, Image> s_art;
std::map<std::string, Image> s_cells;

static_assert(South == int(SDL_GAMEPAD_BUTTON_SOUTH) && Misc6 == int(SDL_GAMEPAD_BUTTON_MISC6),
              "SDL button enum changed");
static_assert(int(SDL_GAMEPAD_AXIS_LEFT_TRIGGER) == 4 && int(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) == 5,
              "SDL axis enum changed");

bool keyboardEnabled() { return PortInputKeyboardEnabled() != 0; }

SDL_Gamepad* padForPort(int port)
{
    if (port < 0 || port >= 4)
        return nullptr;
    int index = PADGetIndexForPort(port);
    return index < 0 ? nullptr : PADGetSDLGamepadForIndex(index);
}

// The Deck's built-in controller, and the virtual pad Steam Input puts in front of it. SDL has no
// gamepad type for either, so both arrive as PAD_TYPE_STANDARD
const Uint16 kValveVendor = 0x28de;
const Uint16 kDeckProduct = 0x1205;
const Uint16 kSteamVirtualProduct = 0x11ff;

bool deckPad(int port)
{
    SDL_Gamepad* pad = padForPort(port);
    if (pad == nullptr || SDL_GetGamepadVendor(pad) != kValveVendor)
        return false;
    if (SDL_GetGamepadProduct(pad) == kDeckProduct)
        return true;
    // Steam Input hides the real device, so only the machine says whether it is a Deck.
    return SDL_GetGamepadProduct(pad) == kSteamVirtualProduct && PortIsSteamDeck() != 0;
}

// Held sideways, a single Joy-Con's face buttons do not carry the letters SDL gives them.
bool loneJoyCon(int port)
{
    const PADControllerType type = PADGetControllerType(port);
    return type == PAD_TYPE_JOYCON_LEFT || type == PAD_TYPE_JOYCON_RIGHT;
}

Family padFamily(int port)
{
    if (deckPad(port))
        return Steamdeck;

    switch (PADGetControllerType(port))
    {
    case PAD_TYPE_XBOX360:
    case PAD_TYPE_XBOXONE:       return Xbox;
    case PAD_TYPE_PS3:
    case PAD_TYPE_PS4:
    case PAD_TYPE_PS5:           return Playstation;
    case PAD_TYPE_SWITCH_PROCON:
    case PAD_TYPE_JOYCON_LEFT:
    case PAD_TYPE_JOYCON_RIGHT:
    case PAD_TYPE_JOYCON_PAIR:   return Nintendo;
    case PAD_TYPE_GAMECUBE:
    case PAD_TYPE_NSO_GAMECUBE:  return Gamecube;
    default:                     return Generic;
    }
}

SDL_GamepadType sdlType(Family family)
{
    switch (family)
    {
    case Xbox:        return SDL_GAMEPAD_TYPE_XBOXONE;
    case Playstation: return SDL_GAMEPAD_TYPE_PS5;
    case Nintendo:    return SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO;
    case Steamdeck:   return SDL_GAMEPAD_TYPE_XBOXONE;
    default:          return SDL_GAMEPAD_TYPE_STANDARD;
    }
}

Label labelFor(SDL_GamepadButtonLabel label)
{
    switch (label)
    {
    case SDL_GAMEPAD_BUTTON_LABEL_A:        return LabelA;
    case SDL_GAMEPAD_BUTTON_LABEL_B:        return LabelB;
    case SDL_GAMEPAD_BUTTON_LABEL_X:        return LabelX;
    case SDL_GAMEPAD_BUTTON_LABEL_Y:        return LabelY;
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:    return Cross;
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:   return Circle;
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:   return Square;
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return Triangle;
    default:                                return Unknown;
    }
}

void selectDefault()
{
    s_activePort = -2;
    s_activeId = 0;
    if (padForPort(0))
        s_activePort = 0;
    else if (keyboardEnabled())
        s_activePort = -1;
    else
        for (int p = 1; p < 4; ++p)
            if (padForPort(p))
            {
                s_activePort = p;
                break;
            }
    if (s_activePort >= 0)
        s_activeId = SDL_GetGamepadID(padForPort(s_activePort));
}

void initialize()
{
    if (s_initialized)
        return;
    s_initialized = true;
    const char* env = getenv("STRIKERS_BUTTON_PROMPTS");
    if (env && *env)
    {
        Family f = parseFamily(env);
        if (f < Auto)
            fprintf(stderr, "[prompts] unknown family '%s'; using auto\n", env);
        else
            s_override = f;
    }
    selectDefault();

    std::vector<std::string> candidates;
#if defined(__SWITCH__)
    // Packed into the .nro's romfs.
    candidates.push_back("romfs:/input-prompts");
#endif
    char dir[1024];
    if (port_executable_dir(dir, sizeof dir) == 0)
    {
        candidates.push_back(std::string(dir) + "/input-prompts");
        candidates.push_back(std::string(dir) + "/../assets/input-prompts");
    }
    candidates.push_back("assets/input-prompts");
    for (const auto& candidate : candidates)
        if (SDL_GetPathInfo((candidate + "/LICENSE-Kenney.txt").c_str(), nullptr))
        {
            s_artDir = candidate;
            return;
        }
    fprintf(stderr, "[prompts] no input-prompts folder beside the game; keeping the GameCube "
                    "prompts\n");
}

const Image* loadArt(const char* key)
{
    const char* file = nullptr;
    for (const auto& art : prompt_art::kArt)
        if (!strcmp(key, art.key))
        {
            file = art.file;
            break;
        }
    if (!file)
        return nullptr;

    auto found = s_art.find(file);
    if (found == s_art.end())
    {
        Image image = {};
        const std::string path = s_artDir + "/" + file;
        SDL_Surface* png = SDL_LoadPNG(path.c_str());
        SDL_Surface* rgba = png ? SDL_ConvertSurface(png, SDL_PIXELFORMAT_RGBA32) : nullptr;
        if (rgba && SDL_LockSurface(rgba))
        {
            image.width = unsigned(rgba->w);
            image.height = unsigned(rgba->h);
            image.rgba.resize(size_t(rgba->w) * rgba->h * 4);
            for (int y = 0; y < rgba->h; ++y)
                memcpy(&image.rgba[size_t(y) * rgba->w * 4],
                       static_cast<const unsigned char*>(rgba->pixels) + y * rgba->pitch,
                       size_t(rgba->w) * 4);
            SDL_UnlockSurface(rgba);
            image = trim(image);
        }
        else
            fprintf(stderr, "[prompts] %s: %s\n", path.c_str(), SDL_GetError());
        SDL_DestroySurface(rgba);
        SDL_DestroySurface(png);
        found = s_art.emplace(file, std::move(image)).first;
    }
    return found->second.width ? &found->second : nullptr;
}

const Image* compose(const Icon& icon, Form form)
{
    if (s_artDir.empty())
        return nullptr;
    char key[64];
    if (icon.family == Keyboard)
    {
        const char* name = form == Wide ? icon.wideKey : icon.faceKey;
        if (strlen(name) == 1)
            snprintf(key, sizeof key, "keyboard/%u", static_cast<unsigned char>(*name));
        else
            snprintf(key, sizeof key, "keyboard/%s", name);
    }
    else
        snprintf(key, sizeof key, "%s/%s", familyName(icon.family), icon.token);

    const Image* art = loadArt(key);
    if (!art)
    {
        if (s_missingIcons.insert(key).second)
            fprintf(stderr, "[prompts] no supplied icon for %s; using the question key\n", key);
        snprintf(key, sizeof key, "keyboard/63");
        art = loadArt(key);
        if (!art)
            return nullptr;
    }

    const std::string cellKey = std::string(key) + "/" + char('0' + form);
    auto found = s_cells.find(cellKey);
    if (found == s_cells.end())
    {
        const bool keyboard = strncmp(key, "keyboard/", 9) == 0;
        found = s_cells
                    .emplace(cellKey, place(*art, keyboard ? Keyboard : icon.family,
                                            strchr(key, '/') + 1, form))
                    .first;
    }
    return &found->second;
}

void paintFont(FontState& state)
{
    nlFont* font = state.font;
    int width = font->m_PageSize * 4;
    unsigned char* rgba = static_cast<unsigned char*>(state.page->m_LinearData);
    memset(rgba, 0, size_t(width) * width * 4);
    for (int c = 0; c < Count; ++c)
    {
        auto& glyph = font->m_GlyphLookup[kGlyphs[c] - 32];
        glyph = state.originals[c];
        if (originalFits(s_icons[c], static_cast<Control>(c)))
        {
            glyph.Page = state.originals[s_icons[c].original].Page;
            glyph.uv = state.originals[s_icons[c].original].uv;
            continue;
        }
        const Image* cell = compose(s_icons[c], c < L ? Face : Wide);
        if (!cell)
            continue;
        const int x = c < L ? c * 43 : (c - L) * 73, y = c < L ? 0 : 45;
        for (unsigned py = 0; py < cell->height; ++py)
            memcpy(rgba + ((size_t(y) * 4 + py) * width + size_t(x) * 4) * 4,
                   &cell->rgba[size_t(py) * cell->width * 4], size_t(cell->width) * 4);
        glyph.Page = font->m_PageCount;
        glyph.uv.x = float(x) / font->m_PageSize;
        glyph.uv.y = float(y) / font->m_PageSize;
    }
    state.page->Swizzle(false);
    state.page->Prepare();
}

void be16(unsigned char* p, unsigned n)
{
    p[0] = n >> 8;
    p[1] = n;
}

void be32(unsigned char* p, unsigned n)
{
    p[0] = n >> 24;
    p[1] = n >> 16;
    p[2] = n >> 8;
    p[3] = n;
}

void textureHeader(unsigned char* blob)
{
    memset(blob, 0, 32);
    be32(blob, 1);
    be32(blob + 4, 8);
    blob[8] = blob[9] = blob[10] = 5;
    blob[11] = 3;
    be16(blob + 14, 32);
    be16(blob + 16, 32);
    be32(blob + 20, 256);
}

// Queued draws read a legend's bytes later on Aurora's thread, so a kept blob is never rewritten.
const unsigned char* keepBlob(const unsigned char* blob)
{
    static std::map<std::string, unsigned char*> kept;
    unsigned char*& slot = kept[std::string(reinterpret_cast<const char*>(blob), 1568)];
    if (!slot)
    {
        slot = static_cast<unsigned char*>(nlMalloc(1568, 32, false));
        memcpy(slot, blob, 1568);
    }
    return slot;
}

void pointLegend(PlatTexture* tex, const unsigned char* blob)
{
    blob = keepBlob(blob);
    tex->m_SwizzledData = const_cast<unsigned char*>(blob + 32);
    tex->m_PaletteData = reinterpret_cast<u16*>(const_cast<unsigned char*>(blob + 1056));
    tex->Prepare();
    PortTextureDumpSkip(1);
    PortTextureCreated(&tex->m_TexObj, &tex->m_TlutObj);
    PortTextureDumpSkip(0);
}

void paintLegend(int c)
{
    PlatTexture* tex = glx_GetTex(kLegends[c], false, false);
    if (!s_loaded[c] || !tex)
        return;
    int original = originalLegend(s_icons[kLegendControls[c]], s_loaded, c);
    if (original >= 0)
    {
        pointLegend(tex, s_originals[original].data());
        return;
    }
    const Image* icon = compose(s_icons[kLegendControls[c]], Menu);
    if (!icon)
    {
        pointLegend(tex, s_originals[c].data());
        return;
    }
    unsigned char blob[1568], rows[1024];
    uint16_t palette[256];
    textureHeader(blob);
    quantizeCI8(icon->rgba.data(), 1024, rows, palette);
    c8Tile(rows, 32, 32, blob + 32);
    for (int i = 0; i < 256; ++i)
        be16(blob + 1056 + i * 2, palette[i]);
    pointLegend(tex, blob);
}

void paintLegends()
{
    for (int c = 0; c < kLegendCount; ++c)
        paintLegend(c);
}

// Keeps the disc's art for a freshly loaded legend, so a family with no art of its own restores it.
bool captureLegend(int c)
{
    auto* tex = glx_GetTex(kLegends[c], false, false);
    if (!tex || tex->m_Format != GXTex_CI8 || tex->m_Width != 32 || tex->m_Height != 32 ||
        tex->m_Levels != 1 || tex->m_nPaletteEntries != 256)
        return false;
    textureHeader(s_originals[c].data());
    memcpy(s_originals[c].data() + 32, tex->m_SwizzledData, 1024);
    memcpy(s_originals[c].data() + 1056, tex->m_PaletteData, 512);
    s_loaded[c] = true;
    return true;
}

void update()
{
    initialize();
    if (s_activePort >= 0)
    {
        auto* pad = padForPort(s_activePort);
        if (!pad || SDL_GetGamepadID(pad) != s_activeId)
            selectDefault();
    }
    else if (s_activePort == -2 || (s_activePort == -1 && !keyboardEnabled()))
        selectDefault();
    Family forced = s_override;
    if (forced == Keyboard && !keyboardEnabled())
    {
        if (!s_disabledWarning)
            fprintf(stderr, "[prompts] keyboard disabled; using auto\n");
        s_disabledWarning = true;
        forced = Auto;
    }
    Family family = forced != Auto       ? forced
                    : s_activePort == -1 ? Keyboard
                    : s_activePort >= 0  ? padFamily(s_activePort)
                                         : Gamecube;
    Input in = defaults(family);
    SDL_Gamepad* pad = padForPort(s_activePort);
    int port = s_activePort;
    if (family != Keyboard && !pad)
        for (int p = 0; p < 4; ++p)
            if ((pad = padForPort(p)))
            {
                port = p;
                break;
            }
    in.connected = pad && family != Keyboard;
    char keyNames[Count][128] = {};
    if (family == Keyboard)
    {
        u32 n = 0;
        const auto* keys = PADGetKeyButtonBindings(0, &n);
        for (int c = 0; c < Count; ++c)
        {
            in.keys[c] = nullptr;
            for (u32 i = 0; keys && i < n; ++i)
                if (keys[i].padButton == kControls[c] && keys[i].scancode > 0)
                {
                    SDL_Scancode sc = static_cast<SDL_Scancode>(keys[i].scancode);
                    const char* name =
                        SDL_GetKeyName(SDL_GetKeyFromScancode(sc, SDL_KMOD_NONE, false));
                    if (!name || !*name)
                        name = SDL_GetScancodeName(sc);
                    snprintf(keyNames[c], sizeof keyNames[c], "%s", name);
                    in.keys[c] = keyNames[c];
                    break;
                }
        }
    }
    else if (in.connected)
    {
        u32 nb = 0, na = 0;
        const auto* buttons = PADGetButtonMappings(port, &nb);
        const auto* axes = PADGetAxisMappings(port, &na);
        const auto* dz = PADGetDeadZones(port);
        in.emulateTriggers = dz && dz->emulateTriggers;
        for (int c = 0; c < Count; ++c)
        {
            in.bindings[c] = { -1, -1, -1, 1 };
            for (u32 i = 0; buttons && i < nb; ++i)
                if (buttons[i].padButton == kControls[c])
                {
                    in.bindings[c].button = static_cast<int>(buttons[i].nativeButton);
                    break;
                }
            if (c >= L)
                for (u32 i = 0; axes && i < na; ++i)
                    if (axes[i].padAxis == (c == L ? PAD_AXIS_TRIGGER_L : PAD_AXIS_TRIGGER_R))
                    {
                        in.bindings[c].axis = axes[i].nativeAxis.nativeAxis;
                        in.bindings[c].axisButton = axes[i].nativeButton;
                        in.bindings[c].sign = axes[i].nativeAxis.sign;
                        break;
                    }
        }
    }
    if (family != Keyboard)
    {
        for (int c = 0; c < Count; ++c)
            in.overrides[c] = PortInputPadSetting(kControls[c]);
        for (int i = 0; i < 4; ++i)
        {
            if (family == Generic)
                in.labels[i] = Unknown;
            else if (in.connected && (forced == Auto || padFamily(port) == family))
                in.labels[i] = loneJoyCon(port) ? Unknown
                                                : labelFor(SDL_GetGamepadButtonLabel(
                                                      pad, static_cast<SDL_GamepadButton>(i)));
            else if (family != Gamecube && family != Generic)
                in.labels[i] = labelFor(SDL_GetGamepadButtonLabelForType(
                    sdlType(family), static_cast<SDL_GamepadButton>(i)));
        }
    }
    std::string signature = familyName(family);
    signature +=
        ":" + std::to_string(in.connected ? SDL_GetGamepadID(pad) : 0) + ":" + std::to_string(port);
    for (int c = 0; c < Count; ++c)
    {
        s_icons[c] = resolve(in, static_cast<Control>(c));
        signature += ":" + std::string(s_icons[c].token) + ":" + s_icons[c].faceKey + ":" +
                     s_icons[c].wideKey;
        if (s_icons[c].unbound)
        {
            std::string key = std::to_string(in.connected ? SDL_GetGamepadID(pad) : 0) + ":" +
                              familyName(family) + ":" + std::to_string(c);
            if (s_unboundLogs.insert(key).second)
                fprintf(stderr, "[prompts] %s %c unbound\n", familyName(family), "ABXYLR"[c]);
        }
    }
    if (signature == s_signature)
        return;
    s_signature = signature;
    glFinish();
    for (auto& font : s_fonts)
        paintFont(font);
    paintLegends();
    const char* probe = getenv("STRIKERS_PROBE_PAD");
    if (probe && *probe && strcmp(probe, "0"))
    {
        fprintf(stderr, "[prompts] device %s port %d \"%s\"%s\n", familyName(family), port,
                in.connected         ? SDL_GetGamepadName(pad)
                : family == Keyboard ? "keyboard"
                                     : "no pad",
                !in.connected && family != Keyboard ? " (standard table + ini)" : "");
        for (int c = 0; c < Count; ++c)
        {
            const Binding b = bindingFor(in, static_cast<Control>(c));
            const char* source =
                b.button >= 0
                    ? SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(b.button))
                : b.axis >= 0 ? SDL_GetGamepadStringForAxis(static_cast<SDL_GamepadAxis>(b.axis))
                              : "unbound";
            fprintf(stderr, "[prompts] %c <- %s \"%s\"\n", "ABXYLR"[c],
                    family == Keyboard ? (in.keys[c] ? in.keys[c] : "unbound")
                    : source           ? source
                                       : "unknown",
                    family == Keyboard ? s_icons[c].wideKey : s_icons[c].token);
        }
    }
}

}   // namespace

extern "C" const unsigned char* PortPromptsMenuImage(unsigned button, unsigned* width,
                                                     unsigned* height)
{
    Icon icon = {};
    bool found = false;
    for (int c = 0; c < Count; ++c)
        if (kControls[c] == button)
        {
            icon = s_icons[c];
            found = true;
            break;
        }
    if (!found && (button == PAD_BUTTON_START || button == PAD_TRIGGER_Z))
    {
        Input in = defaults(s_icons[A].family);
        in.connected = padForPort(s_activePort) != nullptr;
        in.overrides[A] = PortInputPadSetting(button);
        if (in.overrides[A] == Unset)
            in.overrides[A] = button == PAD_BUTTON_START ? Start : RightShoulder;
        if (in.connected)
        {
            in.bindings[A].button = -1;
            u32 count = 0;
            const auto* mappings = PADGetButtonMappings(s_activePort, &count);
            for (u32 i = 0; mappings && i < count; ++i)
                if (mappings[i].padButton == button)
                {
                    in.bindings[A].button = mappings[i].nativeButton;
                    break;
                }
            if (s_override == Auto || padFamily(s_activePort) == in.family)
                for (int i = 0; i < 4; ++i)
                    in.labels[i] =
                        loneJoyCon(s_activePort)
                            ? Unknown
                            : labelFor(SDL_GetGamepadButtonLabel(
                                  padForPort(s_activePort), static_cast<SDL_GamepadButton>(i)));
        }
        char keyName[128] = {};
        if (in.family == Keyboard)
        {
            in.keys[A] = nullptr;
            u32 count = 0;
            const auto* keys = PADGetKeyButtonBindings(0, &count);
            for (u32 i = 0; keys && i < count; ++i)
                if (keys[i].padButton == button && keys[i].scancode > 0)
                {
                    const auto sc = static_cast<SDL_Scancode>(keys[i].scancode);
                    const char* name =
                        SDL_GetKeyName(SDL_GetKeyFromScancode(sc, SDL_KMOD_NONE, false));
                    snprintf(keyName, sizeof keyName, "%s",
                             name && *name ? name : SDL_GetScancodeName(sc));
                    in.keys[A] = keyName;
                    break;
                }
        }
        icon = resolve(in, A);
        found = true;
    }
    if (!found)
        return nullptr;
    const Image* art = compose(icon, Menu);
    if (!art)
        return nullptr;
    *width = art->width;
    *height = art->height;
    return art->rgba.data();
}

extern "C" const unsigned char* PortPromptsBackImage(unsigned* width, unsigned* height)
{
    const Family family = s_icons[A].family;
    if (family == Keyboard || family == Gamecube || s_activePort < 0)
        return nullptr;
    // A Back bound to a game button does not open the online panel.
    u32 count = 0;
    const auto* mappings = PADGetButtonMappings(s_activePort, &count);
    for (u32 i = 0; mappings && i < count; ++i)
        if (mappings[i].nativeButton == (u32)SDL_GAMEPAD_BUTTON_BACK)
            return nullptr;
    Icon icon = {};
    icon.family = family;
    snprintf(icon.token, sizeof icon.token, "back");
    const Image* art = compose(icon, Menu);
    if (!art)
        return nullptr;
    *width = art->width;
    *height = art->height;
    return art->rgba.data();
}

extern "C" int PortPromptsKeyboardActive(void) { return s_icons[A].family == Keyboard; }

extern "C" void PortPromptsFrame()
{
    for (const auto& font : s_fonts)
    {
        if (glx_GetTex(font.hash, false, false) != font.page)
        {
            fprintf(stderr,
                    "[prompts] registered font page was released without its teardown hook\n");
            abort();
        }
    }
    update();
}

extern "C" int PortPromptsSetFamily(const char* name)
{
    initialize();
    Family f = parseFamily(name);
    if (f < Auto)
        return 0;
    s_override = f;
    return 1;
}

extern "C" void PortPromptsFontLoaded(nlFont* font)
{
    if (!font || font->m_PageCount >= 15 || font->m_PageSize != 256 ||
        font->m_Metrics.RenderHeight != 45)
        return;
    for (int c = 0; c < Count; ++c)
    {
        const auto& g = font->m_GlyphLookup[kGlyphs[c] - 32];
        if (g.Page == 15 || g.Advance != g.RenderWidth || g.Offset ||
            g.Advance != (c < L ? 43 : 73))
            return;
    }
    update();
    FontState state = {};
    state.font = font;
    for (int c = 0; c < Count; ++c)
        state.originals[c] = font->m_GlyphLookup[kGlyphs[c] - 32];
    char name[80];
    snprintf(name, sizeof name, "port/prompts/%s", font->m_FontName);
    state.hash = nlStringHash(name);
    state.page = glx_CreatePlatTexture();
    state.page->Create(font->m_PageSize * 4, font->m_PageSize * 4, GXTex_RGBA8, 1, true, false);
    glx_AddTex(state.hash, state.page);
    font->m_TextureHandles[font->m_PageCount] = state.hash;
    font->m_EffectTextureHandles[font->m_PageCount] = state.hash;
    paintFont(state);
    s_fonts.push_back(state);
}

extern "C" void PortPromptsFontUnloading(nlFont* font)
{
    for (auto it = s_fonts.begin(); it != s_fonts.end(); ++it)
        if (it->font == font)
        {
            s_fonts.erase(it);
            return;
        }
}

// Called as the disc's legend is added, so it is replaced before anything draws it.
extern "C" void PortPromptsLegendReloaded(unsigned long hash)
{
    for (int i = 0; i < kLegendCount; ++i)
        if (kLegends[i] == hash)
        {
            s_loaded[i] = false;
            update();
            if (captureLegend(i))
                paintLegend(i);
        }
}

extern "C" void PortPromptsLegendsUnloading()
{
    for (bool& loaded : s_loaded)
        loaded = false;
}

extern "C" void PortPromptsLegendsLoaded()
{
    update();
    for (int i = 0; i < kLegendCount; ++i)
        if (!s_loaded[i])
            captureLegend(i);
    paintLegends();
}

namespace
{

// SDL reports attached and gripped Joy-Cons alike; libnx tells them apart.
bool handheldPad(int port)
{
#if defined(__SWITCH__)
    // sdl3on2 reads the console's Joy-Cons as player one when no controller is on No1.
    SDL_Gamepad* pad = padForPort(port);
    return pad != nullptr && SDL_GetGamepadPlayerIndex(pad) == 0 &&
           hidGetNpadStyleSet(HidNpadIdType_No1) == 0 &&
           (hidGetNpadStyleSet(HidNpadIdType_Handheld) & HidNpadStyleTag_NpadHandheld) != 0;
#else
    (void)port;
    return false;
#endif
}

// The art for the pad on one port, or nullptr for the disc's GameCube pad.
const char* padFile(int port)
{
    Family family = s_override;
    if (family == Auto)
    {
        if (!padForPort(port))
            return nullptr;
        family = padFamily(port);
    }
    switch (family)
    {
    case Xbox:        return "xbox.png";
    case Playstation: return "playstation.png";
    case Steamdeck:   return "steamdeck.png";
    case Nintendo:
        switch (s_override == Auto ? PADGetControllerType(port) : PAD_TYPE_SWITCH_PROCON)
        {
        case PAD_TYPE_JOYCON_PAIR:
            return handheldPad(port) ? "joycon_handheld.png" : "joycon_grip.png";
        case PAD_TYPE_JOYCON_LEFT:  return "joycon_left.png";
        case PAD_TYPE_JOYCON_RIGHT: return "joycon_right.png";
        default:                    return "switchpro.png";
        }
    default: return nullptr;
    }
}

const Image* padArt(const std::string& file)
{
    auto found = s_padArt.find(file);
    if (found == s_padArt.end())
    {
        Image image = {};
        const std::string path = s_artDir + "/controllers/" + file;
        SDL_Surface* png = SDL_LoadPNG(path.c_str());
        SDL_Surface* rgba = png ? SDL_ConvertSurface(png, SDL_PIXELFORMAT_RGBA32) : nullptr;
        if (rgba && SDL_LockSurface(rgba))
        {
            image.width = unsigned(rgba->w);
            image.height = unsigned(rgba->h);
            image.rgba.resize(size_t(rgba->w) * rgba->h * 4);
            for (int y = 0; y < rgba->h; ++y)
                memcpy(&image.rgba[size_t(y) * rgba->w * 4],
                       static_cast<const unsigned char*>(rgba->pixels) + y * rgba->pitch,
                       size_t(rgba->w) * 4);
            SDL_UnlockSurface(rgba);
        }
        else
            fprintf(stderr, "[prompts] %s: %s\n", path.c_str(), SDL_GetError());
        SDL_DestroySurface(rgba);
        SDL_DestroySurface(png);
        found = s_padArt.emplace(file, std::move(image)).first;
    }
    return found->second.width ? &found->second : nullptr;
}

// About one texel per framebuffer pixel, so no mip chain; in steps of 32 so a resize makes few.
unsigned padSize()
{
    const unsigned lines = aurora_window_size().fb_height * kPadLines / 480;
    return std::max(64u, std::min(1024u, (lines + 31) & ~31u));
}

PlatTexture* padTexture(const std::string& file, unsigned size)
{
    PlatTexture*& texture = s_padTextures[file + "@" + std::to_string(size)];
    if (!texture)
    {
        const Image* art = padArt(file);
        if (!art)
            return nullptr;
        texture = new (nlMalloc(sizeof(PlatTexture), 32, false)) PlatTexture();
        texture->Create(size, size, GXTex_RGBA8, 1, true, true);
        scale(*art, size, size, static_cast<uint8_t*>(texture->m_LinearData));
        texture->Swizzle(true);
        texture->Prepare();
    }
    return texture;
}

}   // namespace

extern "C" void PortPromptsPadInstances(const void* const* instances)
{
    for (int p = 0; p < 4; ++p)
        s_padInstances[p] = instances[p];
}

extern "C" uintptr_t PortPromptsPadTexture(const void* instance)
{
    if (!instance || s_artDir.empty())
        return 0;
    for (int p = 0; p < 4; ++p)
    {
        if (s_padInstances[p] != instance)
            continue;
        const char* file = padFile(p);
        // glx_GetTex reads a tagged PlatTexture pointer in the port's region as the texture.
        return file ? reinterpret_cast<uintptr_t>(padTexture(file, padSize())) : 0;
    }
    return 0;
}

extern "C" void PortPromptsEvent(const SDL_Event* ev)
{
    initialize();
    if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat && keyboardEnabled() &&
        !PortOverlayMenuOpen())
    {
        u32 n = 0;
        auto* keys = PADGetKeyButtonBindings(0, &n);
        bool bound = false;
        for (u32 i = 0; keys && i < n; ++i)
            if (keys[i].scancode == int(ev->key.scancode))
                bound = true;
        n = 0;
        auto* axes = PADGetKeyAxisBindings(0, &n);
        for (u32 i = 0; axes && i < n; ++i)
            if (axes[i].scancode == int(ev->key.scancode))
                bound = true;
        if (bound)
        {
            s_activePort = -1;
            s_activeId = 0;
        }
    }
    bool button = ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN,
         axis = ev->type == SDL_EVENT_GAMEPAD_AXIS_MOTION;
    if (!button && !axis)
        return;
    SDL_JoystickID id = button ? ev->gbutton.which : ev->gaxis.which;
    int port = SDL_GetGamepadPlayerIndexForID(id);
    if (port < 0 || port >= 4 || !padForPort(port) || SDL_GetGamepadID(padForPort(port)) != id)
        return;
    if (axis)
    {
        int a = ev->gaxis.axis;
        if (a >= 6)
            return;
        if (s_axisId[port] != id)
        {
            memset(s_axis[port], 0, sizeof s_axis[port]);
            s_axisId[port] = id;
        }
        int old = s_axis[port][a], value = ev->gaxis.value;
        s_axis[port][a] = value;
        const auto* dz = PADGetDeadZones(port);
        if (!dz)
            return;
        int threshold = a == 4              ? dz->leftTriggerActivationZone
                        : a == 5            ? dz->rightTriggerActivationZone
                        : !dz->useDeadzones ? 0
                        : a < 2             ? dz->stickDeadZone
                                            : dz->substickDeadZone;
        if (a < 4)
        {
            old = abs(old);
            value = abs(value);
        }
        if (value <= threshold || old > threshold)
            return;
    }
    s_activePort = port;
    s_activeId = id;
}

#endif
