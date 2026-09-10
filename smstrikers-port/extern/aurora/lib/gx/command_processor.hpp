#pragma once

#include "../internal.hpp"

#include <cstdint>

namespace aurora::gx::fifo {

struct ProcessResult {
  uint32_t bytesProcessed;
  bool drawDone;
};

// Process GX FIFO commands until the next draw done event or end of buffer.
// smstrikers-port: streamPos is the absolute FIFO position of data[0], used only
// to name the display list a fatal byte came from.
ProcessResult process(const uint8_t* data, uint32_t size, uint64_t streamPos = 0) noexcept;
void clear_draw_cache() noexcept;

} // namespace aurora::gx::fifo
