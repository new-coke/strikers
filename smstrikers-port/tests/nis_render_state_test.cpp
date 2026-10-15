#include "port/nis_render_state.hpp"
#include <array>
#include <cassert>
#include <cstdio>

int main()
{
    constexpr std::array<unsigned, 6> states{0, 1, 4, 0x100, 0x200, 0x20000};
    constexpr bool expected[6][6] = {
        {false, false, false, true, false, false},
        {false, false, false, true, false, false},
        {false, false, false, true, false, false},
        {false, true,  false, true, false, false},
        {false, false, false, true, false, false},
        {false, false, false, true, false, false}
    };
    for (unsigned previous = 0; previous < states.size(); ++previous)
        for (unsigned current = 0; current < states.size(); ++current)
            assert(port::nis_controls_render_snapshot(states[current], states[previous]) ==
                   expected[previous][current]);
    // The predicate covers NIS and the transition to play. In the engine,
    // previous records the last state transition, not the preceding frame.
    assert(port::nis_controls_render_snapshot(0x100, 0x100));
    assert(port::nis_controls_render_snapshot(1, 0x100));
    assert(!port::nis_controls_render_snapshot(1, 1));
    std::puts("NIS rendering: entry, every active frame, transition to play and other states passed");
}
