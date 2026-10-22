#pragma once
#include "port/native_texture_record.hpp"
#include <array>

namespace port::movie
{
struct Info { unsigned width = 0, height = 0; };
// Missing MP4 returns false and leaves the existing THP path available.
bool open(const char* thpName, bool sound, bool loop, Info&) noexcept;
// 1: playing/buffering, 0: EOF, -1: failure. Changed planes own their bytes.
int poll(std::array<nativegxm::TextureUpload, 3>& planes, bool& changed) noexcept;
void close() noexcept;
// Preserve THPSimple's authored 0..127 curve and millisecond fade requests.
bool set_volume(long volume, long milliseconds, const unsigned short* curve) noexcept;
}
