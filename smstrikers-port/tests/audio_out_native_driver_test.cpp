#include "port/audio.h"
#include "port/config.h"
#include "port/host.h"
#include "port/native_watchdog.hpp"
#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/cpu.h>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <thread>

namespace port::watchdog
{
std::atomic<uint32_t> enabledFlag{0};
Channel channels[static_cast<unsigned>(Role::Count)]{};
}

namespace
{
pthread_t caller;
std::atomic<unsigned> calls{ 0 }, released{ 0 }, dumps{ 0 };
std::atomic<unsigned> irqEnters{ 0 }, irqLeaves{ 0 }, statsReads{ 0 }, snapshotLogs{ 0 };
thread_local unsigned irqDepth = 0;
bool rejectVolume = false, missingDMA = false;
bool diagnostics = false, renderedStats = false;
const int16_t* retained = nullptr;
std::array<int16_t, 1024> previous{};
std::array<int16_t, 320> input{};
unsigned tick = 0;
void reset()
{
    calls = dumps = 0;
    irqEnters = irqLeaves = statsReads = snapshotLogs = 0;
    retained = nullptr;
    tick = 0;
}
void waitForFailure()
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (calls < 20 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(calls == 20);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(calls == 20); // The negative device result must stop the producer.
    PortAudioStop();
}
}
extern "C" {
int fprintf(FILE* output, const char* format, ...)
{
    assert(irqDepth == 0);
    if (std::strstr(format, "[audio] outputs="))
    {
        assert(statsReads == 1 && irqEnters == 1 && irqLeaves == 1);
        snapshotLogs.fetch_add(1);
    }
    va_list arguments;
    va_start(arguments, format);
    const int result = vfprintf(output, format, arguments);
    va_end(arguments);
    return result;
}
SceUID sceKernelGetThreadId()
{
    assert(!pthread_equal(pthread_self(), caller));
    return 21;
}
int sceKernelChangeThreadCpuAffinityMask(SceUID id, int affinity)
{
    assert(id == 21 && affinity == SCE_KERNEL_CPU_MASK_USER_0);
    return 0;
}
int sceKernelChangeThreadPriority(SceUID id, int priority)
{
    assert(id == 21 && priority == 0x10000100);
    return 0;
}
int sceAudioOutOpenPort(SceAudioOutPortType type, int len, int freq, SceAudioOutMode mode)
{
    assert(type == SCE_AUDIO_OUT_PORT_TYPE_MAIN && len == 512 && freq == 48000 &&
           mode == SCE_AUDIO_OUT_MODE_STEREO);
    return 3;
}
int sceAudioOutSetVolume(int port, SceAudioOutChannelFlag channels, int* volume)
{
    assert(port == 3 && channels == (SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH));
    assert(volume[0] == SCE_AUDIO_VOLUME_0DB && volume[1] == SCE_AUDIO_VOLUME_0DB);
    return rejectVolume ? -7 : 0;
}
int sceAudioOutReleasePort(int port)
{
    assert(port == 3 && irqDepth == 0 && irqEnters == irqLeaves);
    released.fetch_add(1);
    return 0;
}
int sceAudioOutOutput(int port, const void* pcm)
{
    assert(port == 3 && pcm && uintptr_t(pcm) % 64 == 0 && irqDepth == 0);
    // Model a driver that retains the previous submitted block until this call.
    if (retained)
    {
        assert(retained != pcm);
        assert(std::memcmp(retained, previous.data(), sizeof(previous)) == 0);
    }
    std::memcpy(previous.data(), pcm, sizeof(previous));
    retained = static_cast<const int16_t*>(pcm);
    if (!missingDMA)
        for (unsigned i = 0; i < 512; ++i)
            { if (!previous[2 * i] || previous[2 * i] != -previous[2 * i + 1]) std::fprintf(stderr, "invalid stereo call=%u sample=%u left=%d right=%d tick=%u\n", calls.load(), i, previous[2*i], previous[2*i+1], tick); assert(previous[2 * i] != 0 && previous[2 * i] == -previous[2 * i + 1]); }
    else
        for (auto s : previous)
            assert(s == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return calls.fetch_add(1) + 1 == 20 ? -9 : 0;
}
void* salPortNextBuffer()
{
    assert(irqDepth == 0);
    if (missingDMA)
        return nullptr;
    ++tick;
    for (unsigned i = 0; i < 160; ++i)
    {
        input[2 * i] = int16_t(100 + tick + i);
        input[2 * i + 1] = -input[2 * i];
    }
    return input.data();
}
unsigned salPortBufferBytes() { return 640; }
unsigned long long port_monotonic_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}
int PortDiagnosticsEnabled() { return diagnostics; }
void hwIRQEnterCritical()
{
    assert(!pthread_equal(pthread_self(), caller) && irqDepth == 0);
    ++irqDepth;
    irqEnters.fetch_add(1);
}
void hwIRQLeaveCritical()
{
    assert(!pthread_equal(pthread_self(), caller) && irqDepth == 1);
    --irqDepth;
    irqLeaves.fetch_add(1);
}
int PortAudioMixStats(PortAudioMixInfo* out)
{
    assert(irqDepth == 1 && out);
    statsReads.fetch_add(1);
    *out = {};
    if (renderedStats)
    {
        out->studios = 1;
        out->voices = 3;
    }
    return renderedStats;
}
void PortAudioDumpWrite(const void*, unsigned int frames)
{
    assert(frames == 512 && irqDepth == 0);
    dumps.fetch_add(1);
}
}
int main()
{
    caller = pthread_self();
    assert(PortAudioStart() && PortAudioDeviceOpen());
    waitForFailure();
    unsigned long buffers = 0, underruns = 0;
    int nonSilent = 0;
    PortAudioStats(&buffers, &underruns, &nonSilent);
    assert(buffers > 40 && underruns == 1 && nonSilent && dumps == 20 && released == 1);
    assert(irqEnters == 0 && irqLeaves == 0 && statsReads == 0 && snapshotLogs == 0);
    assert(!PortAudioDeviceOpen());
    reset();
    missingDMA = true;
    diagnostics = true;
    assert(PortAudioStart());
    waitForFailure();
    PortAudioStats(&buffers, &underruns, &nonSilent);
    assert(!buffers && underruns == 21 && !nonSilent && released == 2);
    assert(irqEnters == 1 && irqLeaves == 1 && statsReads == 1 && snapshotLogs == 1);
    reset();
    missingDMA = false;
    renderedStats = true;
    assert(PortAudioStart());
    waitForFailure();
    assert(irqEnters == 1 && irqLeaves == 1 && statsReads == 1 && snapshotLogs == 1);
    assert(released == 3);
    rejectVolume = true;
    assert(!PortAudioStart() && !PortAudioDeviceOpen() && released == 4);
    PortAudioStop();
    std::puts("Native audio: retained DMA lifetime, aligned alternating buffers, stereo resampling, "
              "missing DMA, output failure without spin, volume failure, restart and locked stats "
              "released before logging passed");
}
