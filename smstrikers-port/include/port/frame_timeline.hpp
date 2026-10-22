#pragma once
#include "port/frame_timeline.h"
#include <array>
#include <cstdint>

namespace port::render
{
// One finite-capture record. Producer and owner write distinct subobjects.
// The producer may inspect/copy the owner subobject only after end_frame's
// existing drain. No rendering payload or resource borrows this storage.
struct FrameTimeline
{
    static constexpr unsigned WaitCapacity = 32;
    struct Interval
    {
        uint64_t beginNs = 0, endNs = 0;
    };
    struct Producer
    {
        uint64_t beginRequestNs = 0, beginReturnNs = 0;
        uint64_t firstDrawReadyNs = 0, firstDrawPublishNs = 0;
        uint64_t endRequestNs = 0, endReturnNs = 0;
        uint32_t batches = 0, drawBatches = 0;
        uint32_t drawCommands = 0, borrowedDrawCommands = 0;
        bool observed = false, beginSucceeded = false, endSucceeded = false, cancelled = false;
    } producer;
    struct Owner
    {
        uint64_t beginStartNs = 0, beginEndNs = 0;
        uint64_t firstDrawCommandNs = 0, lastDrawCommandEndNs = 0;
        uint64_t endStartNs = 0, endEndNs = 0;
        uint64_t readyWaitNs = 0, maxReadyWaitNs = 0;
        uint32_t batches = 0, drawCommands = 0, failedDrawCommands = 0;
        uint32_t waitCalls = 0, storedWaits = 0, waitOverflow = 0;
        std::array<Interval, WaitCapacity> waits{};
        // These partition the existing completed draw timer, including internal
        // draw_scene calls. Early validation/cull exits have no timed scope;
        // queued command successes/failures are counted separately above.
        uint64_t drawPrepareNs = 0, drawApiNs = 0, drawSuffixNs = 0;
        uint32_t timedDrawScopes = 0;
        uint64_t presentPrefixNs = 0, presentSwapNs = 0, presentSuffixNs = 0;
        uint32_t timedPresentScopes = 0;
        bool drawTimingEnabled = false, presentRequested = false, cancelled = false;
        PortVglPresentProfile provider{};

        void add_wait(uint64_t begin, uint64_t end) noexcept
        {
            const auto elapsed = end - begin;
            readyWaitNs += elapsed;
            if (elapsed > maxReadyWaitNs) maxReadyWaitNs = elapsed;
            ++waitCalls;
            if (storedWaits < waits.size())
                waits[storedWaits++] = {begin, end};
            else
                ++waitOverflow;
        }
    } owner;
};
} // namespace port::render
