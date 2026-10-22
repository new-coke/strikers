#include "port/frameskip.hpp"
#include <cassert>
#include <cstdint>

int main()
{
    using namespace port::frameskip;
    assert(parse_mode(nullptr) == Mode::Off);
    assert(parse_mode("") == Mode::Off);
    assert(parse_mode("0") == Mode::Off);
    assert(parse_mode("OFF") == Mode::Off);
    assert(parse_mode("unknown") == Mode::Off);
    assert(parse_mode("1") == Mode::One);
    assert(parse_mode("AUTO") == Mode::Auto);
    assert(parse_mode("auto") == Mode::Auto);
    assert(parse_mode("2") == Mode::Auto);
    assert(mode_name(Mode::Off)[0] == 'O');
    assert(mode_name(Mode::Auto)[0] == 'A');

    Scheduler off;
    for (unsigned i = 0; i < 100; ++i)
        assert(off.should_render(uint64_t(i + 1) * 33333333ull, 60, true));

    Scheduler fixed(Mode::One);
    for (unsigned i = 0; i < 100; ++i)
        assert(fixed.should_render(uint64_t(i + 1) * 20000000ull, 60, true) == (i % 2 == 0));
    // State transitions or diagnostic captures restart the phase at render.
    assert(fixed.should_render(3000000000ull, 60, false));
    assert(fixed.should_render(3010000000ull, 60, true));
    assert(!fixed.should_render(3020000000ull, 60, true));
    fixed.set_mode(Mode::Off);
    assert(fixed.should_render(3030000000ull, 60, true));

    Scheduler adaptive(Mode::Auto);
    constexpr uint64_t start = 1000000000ull;
    constexpr uint64_t budget = 1000000000ull / 60ull;
    assert(adaptive.should_render(start, 60, true));
    assert(!adaptive.should_render(start + budget * 2, 60, true));
    assert(adaptive.should_render(start + budget * 4, 60, true));
    assert(!adaptive.should_render(start + budget * 6, 60, true));
    assert(adaptive.should_render(start + budget * 8, 60, true));
    // No time regression, suspended task or menu exit can inherit debt.
    assert(adaptive.should_render(start + budget * 100, 60, true));
    assert(adaptive.debt_ns() == 0);
    assert(adaptive.should_render(start + budget * 101, 60, false));
    assert(adaptive.debt_ns() == 0);
    assert(adaptive.should_render(start + budget * 102, 60, true));
    adaptive.set_mode(Mode::One);
    assert(adaptive.should_render(start + budget * 103, 60, true));

    Scheduler pal(Mode::Auto);
    constexpr uint64_t palBudget = 1000000000ull / 50ull;
    assert(pal.should_render(start, 50, true));
    assert(pal.should_render(start + palBudget, 50, true));
    assert(!pal.should_render(start + 3 * palBudget, 50, true));
    assert(pal.should_render(start + 5 * palBudget, 50, true));

    set_render_this_frame(false);
    assert(!render_this_frame());
    set_render_this_frame(true);
    assert(render_this_frame());
}
