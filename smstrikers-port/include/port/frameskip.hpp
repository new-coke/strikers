#pragma once

#include <cstdint>

namespace port::frameskip
{
enum class Mode : unsigned char { Off = 0, One = 1, Auto = 2 };

inline bool equals_ignore_case(const char* value, const char* expected) noexcept
{
    if (!value || !expected) return false;
    while (*value && *expected)
    {
        const char a = (*value >= 'a' && *value <= 'z') ? char(*value - 'a' + 'A') : *value;
        const char b = (*expected >= 'a' && *expected <= 'z') ? char(*expected - 'a' + 'A') : *expected;
        if (a != b) return false;
        ++value;
        ++expected;
    }
    return *value == '\0' && *expected == '\0';
}

inline Mode parse_mode(const char* value) noexcept
{
    if (equals_ignore_case(value, "1")) return Mode::One;
    if (equals_ignore_case(value, "2") || equals_ignore_case(value, "AUTO")) return Mode::Auto;
    return Mode::Off;
}

inline const char* mode_name(Mode mode) noexcept
{
    switch (mode)
    {
    case Mode::One: return "1";
    case Mode::Auto: return "AUTO";
    default: return "OFF";
    }
}

// Render submission only: game tasks and their wall-clock simulation delta are
// never skipped. The presentation backend retains the previous scanout.
class Scheduler
{
  public:
    explicit Scheduler(Mode mode = Mode::Off) noexcept : mode_(mode) {}

    Mode mode() const noexcept { return mode_; }
    uint64_t debt_ns() const noexcept { return debtNs_; }

    void set_mode(Mode mode) noexcept
    {
        mode_ = mode;
        reset();
    }

    void reset() noexcept
    {
        previousStartNs_ = 0;
        debtNs_ = 0;
        skipNext_ = false;
        skippedLast_ = false;
    }

    bool should_render(uint64_t nowNs, unsigned targetHz, bool eligible) noexcept
    {
        if (!eligible || mode_ == Mode::Off)
        {
            reset();
            return true;
        }

        if (mode_ == Mode::One)
        {
            const bool render = !skipNext_;
            skipNext_ = !skipNext_;
            return render;
        }

        if (targetHz != 50 && targetHz != 60) targetHz = 60;
        const uint64_t budgetNs = 1000000000ull / targetHz;
        if (!previousStartNs_ || nowNs <= previousStartNs_ ||
            nowNs - previousStartNs_ > budgetNs * 16)
        {
            reset();
            previousStartNs_ = nowNs;
            return true;
        }

        const uint64_t elapsedNs = nowNs - previousStartNs_;
        previousStartNs_ = nowNs;
        if (skippedLast_)
        {
            // A render opportunity was freed; do not allow another skip yet.
            debtNs_ = debtNs_ > budgetNs ? debtNs_ - budgetNs : 0;
        }
        else if (elapsedNs > budgetNs)
        {
            const uint64_t excessNs = elapsedNs - budgetNs;
            const uint64_t maxDebtNs = 2 * budgetNs;
            debtNs_ += excessNs < maxDebtNs - debtNs_ ? excessNs : maxDebtNs - debtNs_;
        }
        else
        {
            const uint64_t spareNs = budgetNs - elapsedNs;
            debtNs_ = spareNs >= debtNs_ ? 0 : debtNs_ - spareNs;
        }

        const bool skip = !skippedLast_ && debtNs_ >= budgetNs / 2;
        skippedLast_ = skip;
        return !skip;
    }

  private:
    Mode mode_;
    uint64_t previousStartNs_ = 0;
    uint64_t debtNs_ = 0;
    bool skipNext_ = false;
    bool skippedLast_ = false;
};

// Single frontend thread owns both the scheduling decision and glSendFrame.
inline bool& frame_render_enabled() noexcept
{
    static bool enabled = true;
    return enabled;
}

inline void set_render_this_frame(bool render) noexcept { frame_render_enabled() = render; }
inline bool render_this_frame() noexcept { return frame_render_enabled(); }
} // namespace port::frameskip
