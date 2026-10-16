#pragma once
#include <cstdint>
#if defined(PORT_VITA)
struct NativeAudioStats
{
    uint64_t hits = 0, misses = 0, loads = 0, rejected = 0, cacheBytes = 0, maxLoadUs = 0;
};
NativeAudioStats PortNativeAudioGetStats() noexcept;
void PortNativeAudioInitialize() noexcept;
void PortNativeAudioShutdown() noexcept;
bool PortNativeAudioBlock(const uint8_t* source, const int16_t* coefficients, int16_t y1,
                          int16_t y2, int16_t* pcm) noexcept;
#else
inline void PortNativeAudioInitialize() noexcept {}
inline void PortNativeAudioShutdown() noexcept {}
inline bool PortNativeAudioBlock(const uint8_t*, const int16_t*, int16_t, int16_t,
                                 int16_t*) noexcept
{
    return false;
}
#endif
