#include <array>
#include <cstdint>
#include <cstdio>

// Include the production unit so the getters, setters, startup helper and local
// state class all run against the same independently specified packed layout.
#include "../src/NL/gl/glState.cpp"

u32 glHash(const char*) { return 0; }
uintptr_t glGetIdentityMatrix() { return 0; }

namespace
{
using Word = unsigned long long;
constexpr std::array<unsigned, 18> widths{ 2,2,2,2,2,2, 1,1,1,1,1,1, 6,6,6,6,6,6 };
constexpr std::array<unsigned, 10> rasterWidths{ 1,1,2,1,8,3,2,2,1,1 };
unsigned checks = 0, failures = 0;

void expect(Word actual, Word expected, const char* operation, unsigned setter, unsigned field)
{
    ++checks;
    if (actual == expected)
        return;
    if (failures++ < 8)
        std::fprintf(stderr, "%s setter=%u field=%u expected=%016llx actual=%016llx\n",
                     operation, setter, field, expected, actual);
}

unsigned long set(unsigned setter, Word& word, eGLTextureState field, unsigned long value)
{
    if (setter == 0)
        return glSetTextureState(word, field, value);
    if (setter == 1)
    {
        glUnHandleizeTextureState(word);
        const auto previous = glSetTextureState(field, value);
        word = glHandleizeTextureState();
        return previous;
    }
    glTextureState state;
    state.m_State = word;
    const auto previous = setter == 2 ? state.SetTextureState(field, value)
                                     : SetTextureStateImpl(state, field, value);
    word = state.m_State;
    return previous;
}

Word extract_bitwise(Word word, unsigned offset, unsigned width)
{
    Word result = 0;
    for (unsigned bit = 0; bit < width; ++bit)
    {
        if (word & (1ULL << (offset + bit)))
            result |= 1ULL << bit;
    }
    return result;
}

void check_getters(Word word)
{
    glTextureState local;
    local.m_State = word;
    glUnHandleizeTextureState(word);
    unsigned offset = 0;
    for (unsigned field = 0; field < widths.size(); ++field)
    {
        const auto expected = extract_bitwise(word, offset, widths[field]);
        expect(glGetTextureState(word, eGLTextureState(field)), expected, "texture-getter", 0, field);
        expect(glGetTextureState(eGLTextureState(field)), expected, "texture-getter", 1, field);
        expect(local.GetTextureState(eGLTextureState(field)), expected, "texture-getter", 2, field);
        offset += widths[field];
    }
    expect(local.m_State, word, "local-unmodified", 0, 0);
    expect(glHandleizeTextureState(), word, "texture-unmodified", 0, 0);

    const unsigned long raster = static_cast<unsigned long>(word);
    glUnHandleizeRasterState(raster);
    offset = 0;
    for (unsigned field = 0; field < rasterWidths.size(); ++field)
    {
        const auto expected = extract_bitwise(word, offset, rasterWidths[field]);
        expect(glGetRasterState(raster, eGLState(field)), expected, "raster-getter", 0, field);
        expect(glGetRasterState(eGLState(field)), expected, "raster-getter", 1, field);
        // This read helper also provides the old value returned by the global
        // raster setter; keep its extraction under the independent oracle.
        expect(extractStateBits(raster, &packed_raster[field], packed_raster[field].numBits),
               expected, "raster-getter", 2, field);
        offset += rasterWidths[field];
    }
    expect(glHandleizeRasterState(), raster, "raster-unmodified", 0, 0);
}

void check_getter_layout()
{
    unsigned offset = 0;
    for (unsigned field = 0; field < widths.size(); ++field)
    {
        expect(packed_texture[field].startBit, offset, "texture-start", 0, field);
        expect(packed_texture[field].numBits, widths[field], "texture-width", 0, field);
        offset += widths[field];
    }
    offset = 0;
    for (unsigned field = 0; field < rasterWidths.size(); ++field)
    {
        expect(packed_raster[field].startBit, offset, "raster-start", 0, field);
        expect(packed_raster[field].numBits, rasterWidths[field], "raster-width", 0, field);
        offset += rasterWidths[field];
    }
}

template <std::size_t N>
void check_all_field_values(const std::array<unsigned, N>& fieldWidths)
{
    unsigned offset = 0;
    for (const unsigned width : fieldWidths)
    {
        const Word maximum = (1ULL << width) - 1;
        const Word mask = maximum << offset;
        for (Word value = 0; value <= maximum; ++value)
        {
            const Word word = value << offset;
            check_getters(word);
            check_getters(word | ~mask);
        }
        offset += width;
    }
}
}

int main()
{
    static_assert(sizeof(Word) == 8 && sizeof(int) == 4);
    gl_StateStartup();
    constexpr Word defaults = (63ULL << 18) | (63ULL << 30) | (63ULL << 36) |
                              (63ULL << 42) | (63ULL << 48);
    expect(glHandleizeTextureState(), defaults, "startup", 0, 0);
    for (unsigned setter = 0; setter < 4; ++setter)
    {
        unsigned offset = 0;
        for (unsigned field = 0; field < widths.size(); ++field)
        {
            const unsigned long maximum = (1UL << widths[field]) - 1;
            const Word mask = Word(maximum) << offset;
            for (Word seed : { 0ULL, ~0ULL, 0xa55aa55aa55aa55aULL, defaults })
            {
                Word actual = seed, expected = seed;
                for (unsigned long value : { maximum, 1UL, 0UL, maximum - 1, maximum, 0UL })
                {
                    const auto old = set(setter, actual, eGLTextureState(field), value);
                    expect(old, (expected >> offset) & maximum, "previous", setter, field);
                    expected = (expected & ~mask) | (Word(value) << offset);
                    expect(actual, expected, "whole-state", setter, field);
                    expect(glGetTextureState(actual, eGLTextureState(field)), value,
                           "field", setter, field);
                    // The complete word comparison preserves all neighbouring
                    // fields and the unused high ten bits, including bit 63.
                }
            }
            offset += widths[field];
        }
    }
    glUnHandleizeTextureState(~0ULL);
    glSetTextureStateDefaults();
    expect(glHandleizeTextureState(), defaults, "restore-defaults", 0, 0);
    const unsigned setterChecks = checks;

    check_getter_layout();
    for (Word word : { 0ULL, ~0ULL, 0xaaaaaaaaaaaaaaaaULL, 0x5555555555555555ULL, defaults })
        check_getters(word);
    for (unsigned bit = 0; bit < 64; ++bit)
    {
        check_getters(1ULL << bit);
        check_getters(~(1ULL << bit));
    }
    check_all_field_values(widths);
    check_all_field_values(rasterWidths);
    Word random = 0x535452494b455253ULL;
    for (unsigned sample = 0; sample < 4096; ++sample)
    {
        random ^= random << 13;
        random ^= random >> 7;
        random ^= random << 17;
        check_getters(random);
    }
    std::printf("Packed state: %u setter checks, %u getter checks, %u failures; "
                "four setters, 18 texture fields, 10 raster fields, 4096 random 64-bit words\n",
                setterChecks, checks - setterChecks, failures);
    return failures ? 1 : 0;
}
