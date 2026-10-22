// The reference follows the original sequential CPU XFB writes. Production
// instead emits texture operations consumed by a separate loading-only shader.
#include "port/loading_indicator.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>

namespace
{
using Frame = std::array<uint8_t, 32 * 32 * 2>;
unsigned comparisons = 0;

void original_blit(Frame& destination, const uint16_t (&source)[32][32])
{
    for (unsigned y = 0; y < 32; ++y)
    {
        int previousU = 0, previousV = 0, previousPreviousU = 0, previousPreviousV = 0;
        for (unsigned x = 0; x < 32; ++x)
        {
            const unsigned raw = source[y][x];
            const int Y = int((raw >> 12) & 15) * 17;
            const int U = int((raw >> 8) & 15) * 17;
            const int V = int((raw >> 4) & 15) * 17;
            if (raw & 15)
            {
                const unsigned at = y * 64 + x * 2;
                destination[at] = uint8_t(Y);
                if (x & 1)
                {
                    destination[at - 1] = uint8_t(.25f * previousPreviousU + .5f * previousU + .25f * U);
                    destination[at + 1] = uint8_t(.25f * previousPreviousV + .5f * previousV + .25f * V);
                }
            }
            if (x == 0) { previousU = U; previousV = V; }
            previousPreviousU = previousU;
            previousPreviousV = previousV;
            previousU = U;
            previousV = V;
        }
    }
}

void apply_operations(Frame& destination, const port::loading::Operations& operations)
{
    for (unsigned y = 0; y < 32; ++y)
    {
        for (unsigned x = 0; x < 32; ++x)
        {
            const unsigned at = (y * 32 + x) * 4;
            const unsigned flags = operations[at + 3] / 85;
            assert(operations[at + 3] == flags * 85 && flags <= 3);
            if (flags & 1)
                destination[y * 64 + x * 2] = operations[at];
            if (flags & 2)
            {
                const unsigned pair = y * 64 + (x & ~1u) * 2;
                destination[pair + 1] = operations[at + 1];
                destination[pair + 3] = operations[at + 2];
            }
        }
    }
}

void check(const uint16_t (&source)[32][32], const port::loading::Operations& operations)
{
    uint32_t random = 0x53545249;
    for (unsigned trial = 0; trial < 256; ++trial)
    {
        Frame original{}, converted{};
        for (auto& byte : original)
        {
            random = random * 1664525u + 1013904223u;
            byte = uint8_t(random >> 24);
        }
        if (trial == 0)
            for (unsigned i = 0; i < original.size(); ++i)
                original[i] = (i & 1) ? 128 : 16;
        converted = original;
        original_blit(original, source);
        apply_operations(converted, operations);
        for (unsigned i = 0; i < original.size(); ++i)
        {
            assert(original[i] == converted[i]);
            ++comparisons;
        }
    }
}

void check_assets()
{
    using namespace port::loading;
    check(normalPixels, normalOperations);
    check(selectedPixels, selectedOperations);
    for (const auto* source : { normalPixels, selectedPixels })
    {
        unsigned mixed = 0, visible = 0;
        for (unsigned y = 0; y < 32; ++y)
            for (unsigned x = 0; x < 32; x += 2)
            {
                mixed += bool(source[y][x] & 15) != bool(source[y][x + 1] & 15);
                visible += bool(source[y][x] & 15) + bool(source[y][x + 1] & 15);
            }
        assert(mixed == 14 && visible == 194);
    }
    for (const auto* operations : { &normalOperations, &selectedOperations })
        for (unsigned y = 0; y < 32; ++y)
            for (unsigned x = 24; x < 32; ++x)
                assert((*operations)[(y * 32 + x) * 4 + 3] == 0);

    // Exercise the row-start replicated sample and all mask combinations;
    // the actual assets leave column zero transparent.
    uint16_t synthetic[32][32]{};
    for (unsigned y = 0; y < 32; ++y)
        for (unsigned x = 0; x < 32; ++x)
            synthetic[y][x] = uint16_t(((x * 113u + y * 311u) & 0xfff0u) | ((x + y) % 4));
    check(synthetic, operations(synthetic));

    Frame sequence{}, reference{};
    for (unsigned i = 0; i < sequence.size(); ++i)
        sequence[i] = reference[i] = uint8_t(i * 17 + 3);
    for (unsigned phase = 0; phase < 6; ++phase)
    {
        const bool selected = (phase & 1) != 0;
        original_blit(reference, selected ? selectedPixels : normalPixels);
        apply_operations(sequence, selected ? selectedOperations : normalOperations);
        assert(sequence == reference);
    }
}

void check_state()
{
    using namespace port::loading;
    State state;
    auto request = state.change(true, false, 60);
    assert(request.submit && request.begin && request.waitRetraces == 120 && state.enabled());
    request = state.change(true, true, 60);
    assert(request.submit && request.begin && request.otherPosition && request.waitRetraces == 0);
    request = state.save(60);
    assert(request.submit && !request.begin && !state.enabled());
    request = state.restore(60);
    assert(request.submit && request.begin && !request.otherPosition && request.blackBackground);
    assert(request.waitRetraces == 0 && state.enabled());
    state.reject();
    assert(!state.enabled());
    request = state.restore(60);
    assert(request.submit && request.begin && request.blackBackground);

    State offFirst;
    request = offFirst.change(false, true, 60);
    assert(request.submit && !request.begin && request.waitRetraces == 120);
    assert(offFirst.change(true, false, 60).waitRetraces == 0);

    State savedOff;
    assert(!savedOff.save(50).submit);
    assert(!savedOff.restore(50).submit);
    request = savedOff.change(true, true, 50);
    assert(request.waitRetraces == 100);
    request = savedOff.restore(50);
    assert(request.submit && !request.begin && !savedOff.enabled());

    State nested;
    assert(!nested.save(60).submit);
    assert(nested.change(true, false, 60).waitRetraces == 120);
    assert(nested.save(60).submit);
    assert(!nested.save(60).submit); // The retail save slot is not a stack.
    assert(!nested.restore(60).submit);
    assert(!nested.enabled());

    assert(phase_retraces(60) == 19 && phase_retraces(50) == 16);
    const auto normal = rectangles({ true, true, false, false, 60, 0 });
    const auto other = rectangles({ true, true, true, false, 60, 0 });
    for (unsigned i = 0; i < 3; ++i)
    {
        assert(normal[i][0] == 432.f + 36.f * i && normal[i][2] == 48.f);
        assert(std::fabs(normal[i][1] - 463.857142857f) < .001f);
        assert(std::fabs(other[i][1] - 444.428571429f) < .001f);
        assert(std::fabs(normal[i][3] - 38.857142857f) < .001f);
    }
    const auto pal = rectangles({ true, true, false, false, 50, 0 });
    const auto palOther = rectangles({ true, true, true, false, 50, 0 });
    assert(std::fabs(pal[0][1] - 436.f * 544.f / 542.f) < .001f);
    assert(std::fabs(palOther[0][1] - 418.f * 544.f / 542.f) < .001f);
}
}   // namespace

int main()
{
    check_assets();
    check_state();
    std::printf("loading indicator: %u XFB-byte oracle comparisons; state/layout PASS\n", comparisons);
}
