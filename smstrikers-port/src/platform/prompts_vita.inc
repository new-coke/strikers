// Native, fixed-layout Vita prompts. Included only by the Vita branch of prompts.cpp.
// Front-end load/unload hooks own this state; there is no frame/event polling.
#include "NL/nlFont.h"
#include "NL/glx/glxTexture.h"
#include "port/native_match_runtime.hpp"
#include "prompts_vita_art.hpp"

#include <array>
#include <cstdlib>
#include <cstring>

namespace
{
constexpr unsigned kVitaFontSlots = 16;
constexpr unsigned kVitaFontSide = 256;
constexpr unsigned kVitaFaceCount = 4;
static_assert(sizeof(unsigned long) >= sizeof(std::uintptr_t),
              "Font texture handles must preserve native arena pointers");
constexpr char kVitaGlyphs[6] = { '@', '~', '|', '+', '*', '^' };
struct VitaLegendImage
{
    std::uint32_t handle;
    unsigned side;
    unsigned icon; // kVitaFaceCount selects the four-face controller badge.
};
constexpr std::array<VitaLegendImage, 7> kVitaLegendImages = {{
    { 0xecce7182u, 32, 0 }, { 0x288d277bu, 32, 1 },
    { 0x12fa669au, 32, 3 }, { 0xd9104663u, 32, 1 },
    { 0x3c4b529au, 32, 0 },
    { 0x885a2673u, 64, kVitaFaceCount }, { 0x1eefbc5bu, 64, kVitaFaceCount }
}};

struct VitaFontPrompt
{
    nlFont* font = nullptr;
    PlatTexture* texture = nullptr;
    unsigned page = 0;
    bool installed = false;
    std::array<nlFont::GlyphInfo, kVitaFaceCount> originals{};
};
struct VitaLegendPrompt
{
    const PlatTexture* texture = nullptr;
    std::uint32_t textureLevel = 0;
    std::uint32_t resourceLevel = 0;
};
std::array<VitaFontPrompt, kVitaFontSlots> s_vitaFonts{};
std::array<VitaLegendPrompt, kVitaLegendImages.size()> s_vitaLegends{};

bool vitaPlaystationGlyphsEnabled()
{
    const char* value = std::getenv("STRIKERS_VITA_PLAYSTATION_GLYPHS");
    return !value || std::strcmp(value, "0") != 0;
}

struct VitaFontLayout
{
    unsigned width[6];
    unsigned iconSide[4];
    unsigned iconX[4];
    unsigned iconY[4];
};
constexpr VitaFontLayout kVitaWesternLayout = {
    {43, 43, 43, 43, 73, 73}, {28, 28, 28, 28}, {12, 12, 12, 12}, {2, 2, 2, 2}
};
constexpr VitaFontLayout kVitaJapaneseLayout = {
    {33, 22, 27, 33, 46, 44}, {28, 20, 24, 28}, {2, 1, 1, 2}, {2, 7, 4, 2}
};

const VitaFontLayout* vitaPromptFontLayout(const nlFont& font)
{
    if (!font.m_PageCount || font.m_PageCount >= 15 ||
        font.m_PageSize != kVitaFontSide ||
        font.m_InvTexSize != 1.0f / kVitaFontSide ||
        font.m_Metrics.RenderHeight != 45)
        return nullptr;
    const VitaFontLayout* layouts[] = { &kVitaWesternLayout, &kVitaJapaneseLayout };
    for (const auto* layout : layouts)
    {
        // The three retail Japanese text resources share identical controller
        // cells and have 7 (101), 9 (in-game), or 13 (front-end) pages. Heading
        // cells are ordinary punctuation and deliberately match neither layout.
        if (layout == &kVitaJapaneseLayout && font.m_PageCount != 7 &&
            font.m_PageCount != 9 && font.m_PageCount != 13)
            continue;
        bool matches = true;
        for (unsigned i = 0; i < 6; ++i)
        {
            const auto& glyph = font.m_GlyphLookup[kVitaGlyphs[i] - 32];
            if (glyph.Page >= font.m_PageCount || glyph.Advance != layout->width[i] ||
                glyph.RenderWidth != layout->width[i] || glyph.Offset != 0)
                matches = false;
        }
        if (matches)
            return layout;
    }
    return nullptr;
}

void vitaPromptCopyIcon(port::nativegxm::TextureUpload& image, unsigned icon,
                        unsigned x, unsigned y)
{
    constexpr auto side = vita_prompt_art::kSide;
    for (unsigned row = 0; row < side; ++row)
        std::memcpy(image.pixels.data() + ((y + row) * image.stride + x) * 4,
                    vita_prompt_art::kPixels[icon] + row * side * 4, side * 4);
}

// Small prompt cells need smaller pictograms. Area filtering is done only at
// resource load, in premultiplied space, then converted back to straight
// RGBA8 for the existing font blend mode. The 28px path stays byte-identical.
void vitaPromptScaledIcon(port::nativegxm::TextureUpload& image, unsigned icon,
                          unsigned x, unsigned y, unsigned side)
{
    constexpr unsigned sourceSide = vita_prompt_art::kSide;
    if (side == sourceSide)
    {
        vitaPromptCopyIcon(image, icon, x, y);
        return;
    }
    constexpr unsigned area = sourceSide * sourceSide;
    for (unsigned row = 0; row < side; ++row)
    {
        const unsigned top = row * sourceSide, bottom = top + sourceSide;
        for (unsigned column = 0; column < side; ++column)
        {
            const unsigned left = column * sourceSide, right = left + sourceSide;
            unsigned alpha = 0, colour[3] = {};
            for (unsigned sy = top / side; sy < (bottom + side - 1) / side; ++sy)
            {
                const unsigned highY = bottom < (sy + 1) * side ? bottom : (sy + 1) * side;
                const unsigned lowY = top > sy * side ? top : sy * side;
                for (unsigned sx = left / side; sx < (right + side - 1) / side; ++sx)
                {
                    const unsigned highX = right < (sx + 1) * side ? right : (sx + 1) * side;
                    const unsigned lowX = left > sx * side ? left : sx * side;
                    const auto* pixel = vita_prompt_art::kPixels[icon] + (sy * sourceSide + sx) * 4;
                    const unsigned weightedAlpha = (highX - lowX) * (highY - lowY) * pixel[3];
                    alpha += weightedAlpha;
                    for (unsigned c = 0; c < 3; ++c)
                        colour[c] += weightedAlpha * pixel[c];
                }
            }
            auto* out = image.pixels.data() + ((y + row) * image.stride + x + column) * 4;
            out[3] = static_cast<std::uint8_t>((alpha + area / 2) / area);
            for (unsigned c = 0; c < 3; ++c)
                out[c] = alpha ? static_cast<std::uint8_t>((colour[c] + alpha / 2) / alpha) : 0;
        }
    }
}

port::nativegxm::TextureUpload vitaPromptImage(unsigned side)
{
    port::nativegxm::TextureUpload image;
    image.width = image.height = image.stride = side;
    image.mipCount = 1;
    image.storage = port::nativegxm::TextureStorage::Rgba8;
    image.swizzled = false;
    image.pixels.assign(std::size_t(side) * side * 4, 0);
    return image;
}

void vitaPromptCommitMetadata(PlatTexture& texture, unsigned side)
{
    texture.m_Width = texture.m_Height = static_cast<u16>(side);
    texture.m_Levels = texture.m_MaxLevel = 1;
    texture.m_Format = GXTex_RGBA8;
    texture.m_nPaletteEntries = 0;
    texture.m_bMissingTexture = false;
    texture.m_PaletteData = nullptr;
    // All admitted pixels are straight RGBA8, including their alpha precision.
    std::memset(texture.m_Bits, 8, sizeof texture.m_Bits);
    // The pixel vector is owned/copied by admission. No CPU pixel pointer is retained.
    // Preserve native-record size and both original ownership levels on replacement.
}

void vitaPromptLoadFont(nlFont& font)
{
    for (const auto& state : s_vitaFonts)
        if (state.font == &font)
            return; // Includes failed attempts; a load hook is idempotent.
    const auto* layout = vitaPromptFontLayout(font);
    if (!layout)
        return;

    VitaFontPrompt* state = nullptr;
    for (unsigned i = 0; i < s_vitaFonts.size(); ++i)
    {
        if (!s_vitaFonts[i].font)
        {
            state = &s_vitaFonts[i];
            break;
        }
    }
    if (!state)
        return;

    auto image = vitaPromptImage(kVitaFontSide);
    unsigned cell = 0;
    for (unsigned i = 0; i < kVitaFaceCount; ++i)
    {
        vitaPromptScaledIcon(image, i, cell + layout->iconX[i], layout->iconY[i], layout->iconSide[i]);
        cell += layout->width[i];
    }

    state->font = &font;
    state->page = font.m_PageCount;
    for (unsigned i = 0; i < kVitaFaceCount; ++i)
        state->originals[i] = font.m_GlyphLookup[kVitaGlyphs[i] - 32];

    state->texture = glx_CreatePlatTexture();
    if (!state->texture)
        return;

    // glx_GetTex already resolves direct arena PTEX handles. Marker ownership
    // releases the native image independently of the legacy texture-name tree.
    // An unused header after failed admission remains owned by its resource
    // marker; 16 slots bound concurrent fonts, not headers across manual reloads.
    if (!port::directmatch::admit_texture(state->texture, image))
        return;

    const auto handle = reinterpret_cast<std::uintptr_t>(state->texture);
    vitaPromptCommitMetadata(*state->texture, kVitaFontSide);
    font.m_TextureHandles[state->page] = handle;
    font.m_EffectTextureHandles[state->page] = handle;
    cell = 0;
    for (unsigned i = 0; i < kVitaFaceCount; ++i)
    {
        auto& glyph = font.m_GlyphLookup[kVitaGlyphs[i] - 32];
        glyph.Page = state->page;
        glyph.uv.x = static_cast<float>(cell) / kVitaFontSide;
        cell += layout->width[i];
        glyph.uv.y = 0.0f;
    }
    // m_PageCount and all advances/widths/offsets remain original. DrawString
    // visits glyph pages until all characters have been submitted.
    state->installed = true;
}
} // namespace

extern "C" void PortPromptsFontLoaded(nlFont* font)
{
    if (!font || !vitaPlaystationGlyphsEnabled())
        return;
    try
    {
        vitaPromptLoadFont(*font);
    }
    catch (...)
    {
        // Exceptions preserve the original glyphs. CPU allocation exhaustion is
        // fatal in the game's operator new/arena; this is not an OOM fallback.
    }
}

extern "C" void PortPromptsFontUnloading(nlFont* font)
{
    for (auto& state : s_vitaFonts)
    {
        if (state.font != font || !font)
            continue;
        if (state.installed)
            for (unsigned i = 0; i < kVitaFaceCount; ++i)
                font->m_GlyphLookup[kVitaGlyphs[i] - 32] = state.originals[i];
        // FontManager calls this before deleting the font; the marker owns
        // the header and GPU image and releases both after font destruction.
        state = {};
    }
}

extern "C" void PortPromptsLegendsLoaded()
{
    if (!vitaPlaystationGlyphsEnabled())
        return;
    for (unsigned i = 0; i < s_vitaLegends.size(); ++i)
    {
        const auto& definition = kVitaLegendImages[i];
        auto* texture = glx_GetTex(definition.handle, false, false);
        if (!texture)
            continue;
        auto& state = s_vitaLegends[i];
        if (state.texture == texture &&
            state.textureLevel == texture->m_OwnerTextureLevel &&
            state.resourceLevel == texture->m_OwnerResourceLevel)
            continue;
        // Prepared retail legends/badges are native CI8-semantic images.
        // A mismatched asset is left intact rather than guessed from its name.
        if (texture->m_Magic != 0x50544558 || texture->m_Width != definition.side ||
            texture->m_Height != definition.side || texture->m_Levels != 1 ||
            texture->m_MaxLevel != 1 || texture->m_Format != GXTex_CI8 ||
            texture->m_NativeDataSize < 64 || texture->m_bMissingTexture ||
            texture->m_SwizzledData || texture->m_LinearData ||
            texture->m_PaletteData || texture->m_nPaletteEntries)
            continue;
        try
        {
            auto image = vitaPromptImage(definition.side);
            if (definition.icon == kVitaFaceCount)
            {
                // Existing icon order is Cross, Circle, Square, Triangle. Keep
                // the surrounding P1-P4 labels and human/CPU slides untouched.
                vitaPromptScaledIcon(image, 0, 22, 42, 20);
                vitaPromptScaledIcon(image, 1, 42, 22, 20);
                vitaPromptScaledIcon(image, 2, 2, 22, 20);
                vitaPromptScaledIcon(image, 3, 22, 2, 20);
            }
            else
                vitaPromptCopyIcon(image, definition.icon, 2, 2);
            if (!port::directmatch::admit_texture(texture, image))
                continue;
            vitaPromptCommitMetadata(*texture, definition.side);
            state = { texture, texture->m_OwnerTextureLevel, texture->m_OwnerResourceLevel };
        }
        catch (...)
        {
            // Returned admission errors preserve the old image. CPU allocation
            // exhaustion follows the game's fatal allocator contract.
        }
    }
}

extern "C" void PortPromptsLegendsUnloading() { s_vitaLegends = {}; }
extern "C" void PortPromptsEvent(const SDL_Event*) {}
extern "C" void PortPromptsFrame() {}
// Vita bindings are fixed by PADRead; runtime desktop-family switching is unsupported.
extern "C" int PortPromptsSetFamily(const char*) { return 0; }
