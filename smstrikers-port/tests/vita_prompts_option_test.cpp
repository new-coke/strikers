// Exercise the native load hooks with real font/texture layouts and a recording GPU.
#include "port/prompts.h"
#include "NL/nlFont.h"
#include "NL/glx/glxTexture.h"
#include "port/native_match_runtime.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>

namespace fixture
{
PlatTexture fontTexture, legend;
unsigned creates = 0, lookups = 0, admissions = 0;
bool failAdmission = false;
port::nativegxm::TextureUpload lastImage;
}
nlFont::nlFont() { std::memset(static_cast<void*>(this), 0, sizeof *this); }
nlFont::~nlFont() {}
PlatTexture::~PlatTexture() {}
PlatTexture* glx_CreatePlatTexture() { ++fixture::creates; return &fixture::fontTexture; }
PlatTexture* glx_GetTex(uintptr_t handle, bool, bool)
{
    ++fixture::lookups;
    return handle == 0xecce7182u ? &fixture::legend : nullptr;
}
namespace port::directmatch
{
bool admit_texture(const PlatTexture*, const nativegxm::TextureUpload& image) noexcept
{
    ++fixture::admissions;
    if (fixture::failAdmission) return false;
    fixture::lastImage = image;
    return true;
}
}
#include "../src/platform/prompts_vita.inc"

static void font_fixture(nlFont& font, bool japanese)
{
    font.m_PageCount = japanese ? 9 : 3;
    font.m_PageSize = 256;
    font.m_InvTexSize = 1.f / 256;
    font.m_Metrics.RenderHeight = 45;
    const unsigned widths[]{33,22,27,33,46,44};
    for (unsigned i = 0; i < 6; ++i)
    {
        auto& glyph = font.m_GlyphLookup[kVitaGlyphs[i] - 32];
        glyph.Page = 1;
        glyph.Advance = glyph.RenderWidth = japanese ? widths[i] : i < 4 ? 43 : 73;
        glyph.uv = { .25f, .5f };
    }
}

int main()
{
    for (bool japanese : {false, true})
    {
        nlFont font;
        font_fixture(font, japanese);
        const auto original = font;
        setenv("STRIKERS_VITA_PLAYSTATION_GLYPHS", "0", 1);
        const auto creates = fixture::creates, admissions = fixture::admissions;
        PortPromptsFontLoaded(&font);
        fixture::lookups = 0;
        PortPromptsLegendsLoaded();
        assert(!fixture::lookups && fixture::creates == creates && fixture::admissions == admissions);
        assert(!std::memcmp(&font, &original, sizeof font));
        PortPromptsFontUnloading(&font);

        unsetenv("STRIKERS_VITA_PLAYSTATION_GLYPHS"); // Default is PlayStation.
        PortPromptsFontLoaded(&font);
        assert(fixture::creates == creates + 1 && fixture::admissions == admissions + 1);
        assert(font.m_PageCount == original.m_PageCount);
        for (unsigned i = 0; i < 6; ++i)
        {
            const auto& glyph = font.m_GlyphLookup[kVitaGlyphs[i] - 32];
            const auto& prior = original.m_GlyphLookup[kVitaGlyphs[i] - 32];
            assert(glyph.Advance == prior.Advance && glyph.RenderWidth == prior.RenderWidth);
            if (i < 4) assert(glyph.Page == font.m_PageCount);
            else assert(!std::memcmp(&glyph, &prior, sizeof glyph)); // L/R stay original.
        }
        PortPromptsFontLoaded(&font); // Repeated load must not replace again.
        assert(fixture::creates == creates + 1);
        setenv("STRIKERS_VITA_PLAYSTATION_GLYPHS", "0", 1);
        PortPromptsFontUnloading(&font); // Teardown must run even after an option change.
        assert(!std::memcmp(font.m_GlyphLookup, original.m_GlyphLookup, sizeof font.m_GlyphLookup));
    }
    setenv("STRIKERS_VITA_PLAYSTATION_GLYPHS", "1", 1);
    nlFont failed;
    font_fixture(failed, false);
    const auto original = failed;
    fixture::failAdmission = true;
    PortPromptsFontLoaded(&failed);
    assert(!std::memcmp(&failed, &original, sizeof failed));
    PortPromptsFontUnloading(&failed);
    fixture::failAdmission = false;

    auto& legend = fixture::legend;
    legend.m_Magic = 0x50544558;
    legend.m_Width = legend.m_Height = 32;
    legend.m_Levels = legend.m_MaxLevel = 1;
    legend.m_Format = GXTex_CI8;
    legend.m_NativeDataSize = 64;
    const auto admitted = fixture::admissions;
    PortPromptsLegendsLoaded();
    assert(fixture::admissions == admitted + 1 && legend.m_Format == GXTex_RGBA8);
    assert(fixture::lastImage.width == 32 && fixture::lastImage.pixels.size() == 32 * 32 * 4);
    // The A legend contains the same baked Cross art, including alpha.
    for (unsigned row = 0; row < vita_prompt_art::kSide; ++row)
        assert(!std::memcmp(fixture::lastImage.pixels.data() + ((row + 2) * 32 + 2) * 4,
                            vita_prompt_art::kPixels[0] + row * vita_prompt_art::kSide * 4,
                            vita_prompt_art::kSide * 4));
    PortPromptsLegendsUnloading();
    std::puts("Vita glyphs: default/on/off, Western/Japanese metrics, original L/R, failure, teardown and Cross legend passed");
}
