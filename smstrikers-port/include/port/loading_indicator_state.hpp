#pragma once
#include <cstdint>

namespace port::render
{
struct LoadingAdvance
{
    bool clearBackground = false, present = false, icons = false;
    uint32_t selected = 0;
};

// Fold the original VI post-retrace callback into one current display update.
// This counter follows physical retraces, independently of engine-frame work.
class LoadingIndicatorClock
{
  public:
    constexpr void reset(uint32_t retrace, uint32_t waitRetraces,
                         uint32_t phaseRetraces) noexcept
    {
        previousRetrace_ = retrace;
        // The callback increments before comparing >= waitRetraces. Even a
        // zero delay therefore starts at the next physical retrace.
        delayRemaining_ = waitRetraces ? waitRetraces - 1 : 0;
        phaseRetraces_ = phaseRetraces ? phaseRetraces : 1;
        phasePosition_ = 0;
    }
    constexpr LoadingAdvance advance(uint32_t retrace, unsigned& firstFrames) noexcept
    {
        uint32_t elapsed = retrace - previousRetrace_;
        previousRetrace_ = retrace;
        const auto waiting = elapsed < delayRemaining_ ? elapsed : delayRemaining_;
        elapsed -= waiting;
        delayRemaining_ -= waiting;
        LoadingAdvance result;
        if (!elapsed)
            return result;

        const auto blackRetraces = elapsed < firstFrames ? elapsed : firstFrames;
        firstFrames -= blackRetraces;
        elapsed -= blackRetraces;
        if (blackRetraces && !firstFrames)
        {
            result.clearBackground = true;
            result.present = true;
        }
        if (elapsed)
        {
            // DrawLoadingIndicator uses selected before incrementing its
            // counter. Do not replay missed swaps or loop over lost retraces.
            const uint64_t cycle = uint64_t(phaseRetraces_) * 3;
            result.selected = uint32_t(((phasePosition_ + elapsed - 1) % cycle) /
                                       phaseRetraces_);
            phasePosition_ = (phasePosition_ + elapsed) % cycle;
            result.present = result.icons = true;
        }
        return result;
    }

  private:
    uint32_t previousRetrace_ = 0, delayRemaining_ = 0, phaseRetraces_ = 19;
    uint64_t phasePosition_ = 0;
};
} // namespace port::render
