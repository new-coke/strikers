#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "port/audio.h"
#include "port/host.h"
#include "port/config.h"
#include "port/native_watchdog.hpp"
#include "musyx/port_audio_watchdog.h"
#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/cpu.h>
#include <pthread.h>
#include <cstdio>
#include <atomic>
#include <array>
#include <cstring>
#include <cstdint>
extern "C" void* salPortNextBuffer(void);
extern "C" unsigned salPortBufferBytes(void);
extern "C" void hwIRQEnterCritical(void);
extern "C" void hwIRQLeaveCritical(void);
namespace
{
std::atomic<unsigned int> audioProbeEnabled{ 0 };
thread_local bool audioProbeOwner = false;
// Keep the TLS accessor behind a non-inlined function with observable stores:
// an optimizing emutls compiler must not move first-touch work into the OFF path.
[[gnu::noinline]] void enable_audio_watchdog_owner() noexcept
{
    audioProbeOwner = true;
    audioProbeEnabled.store(1, std::memory_order_release);
}
[[gnu::noinline]] unsigned int mark_audio_watchdog_owner(unsigned int detail) noexcept
{
    if (!audioProbeOwner)
        return 0;
    auto& field = port::watchdog::channel(port::watchdog::Role::AudioOutput).detail;
    const unsigned int previous = field.load(std::memory_order_relaxed);
    field.store(detail, std::memory_order_relaxed);
    return previous;
}
void begin_audio_watchdog_probe() noexcept
{
    if (!PortDiagnosticsEnabled() || !port::watchdog::enabled())
        return;
    // Vita emutls can allocate on first access. Warm the owner's slot once,
    // before its mixer loop, and never access TLS while the probe is off.
    enable_audio_watchdog_owner();
}
void end_audio_watchdog_probe() noexcept
{
    if (audioProbeEnabled.load(std::memory_order_relaxed))
        audioProbeEnabled.store(0, std::memory_order_release);
}
}
extern "C" unsigned int PortAudioWatchdogSetDetail(unsigned int detail)
{
    if (!audioProbeEnabled.load(std::memory_order_acquire) || !port::watchdog::enabled())
        return 0;
    return mark_audio_watchdog_owner(detail);
}
namespace
{
int channel = -1;
pthread_t thread{};
bool threadStarted = false;
std::atomic<bool> running{ false };
std::atomic<unsigned long> buffers{ 0 }, underruns{ 0 };
std::atomic<int> nonSilent{ 0 };
std::atomic<unsigned long long> mixTotalUs{ 0 }, mixMaxUs{ 0 };
std::atomic<unsigned long> mixOver2ms{ 0 }, outputCalls{ 0 };
std::atomic<int> outputError{ 0 };
void* audio_thread(void*)
{
    const SceUID id = sceKernelGetThreadId();
    port::watchdog::enter_thread(port::watchdog::Role::AudioOutput, id);
    begin_audio_watchdog_probe();
    const int affinity = sceKernelChangeThreadCpuAffinityMask(id, SCE_KERNEL_CPU_MASK_USER_0);
    if (affinity < 0)
        std::fprintf(stderr, "[audio] affinity error=%08x\n", unsigned(affinity));
    sceKernelChangeThreadPriority(id, 0x10000100);
    std::array<int16_t, 2048> queue{};
    size_t used = 0;
    // AudioOut can retain a submitted block until the following call (alternating-block transport).
    alignas(64) std::array<std::array<int16_t, 512 * 2>, 2> blocks{};
    unsigned nextBlock = 0;
    while (running.load())
    {
        while (used < 512)
        {
            port::watchdog::stage(port::watchdog::Role::AudioOutput, port::watchdog::Stage::AudioMix);
            const auto mixBegin = port_monotonic_ns();
            const auto* in = static_cast<const int16_t*>(salPortNextBuffer());
            const auto mixUs = (port_monotonic_ns() - mixBegin) / 1000;
            mixTotalUs.fetch_add(mixUs, std::memory_order_relaxed);
            auto previous = mixMaxUs.load(std::memory_order_relaxed);
            while (previous < mixUs &&
                   !mixMaxUs.compare_exchange_weak(previous, mixUs, std::memory_order_relaxed))
            {
            }
            if (mixUs > 2000)
                mixOver2ms.fetch_add(1, std::memory_order_relaxed);
            const unsigned frames = salPortBufferBytes() / 4;
            if (!in || !frames || frames > 320)
            {
                queue.fill(0);
                used = 512;
                underruns.fetch_add(1);
                break;
            }
            // 32 kHz stereo MusyX to native 48 kHz, retaining the 5-ms mixer cadence.
            for (unsigned i = 0; i < frames * 3 / 2; ++i)
            {
                const unsigned whole = i * 2 / 3;
                const int phase = int(i * 2 % 3);
                const unsigned next = whole + 1 < frames ? whole + 1 : whole;
                for (unsigned c = 0; c < 2; ++c)
                    queue[(used + i) * 2 + c] = int16_t(
                        (int(in[whole * 2 + c]) * (3 - phase) + int(in[next * 2 + c]) * phase) / 3);
            }
            used += frames * 3 / 2;
            buffers.fetch_add(1);
        }
        auto& output = blocks[nextBlock];
        std::memcpy(output.data(), queue.data(), output.size() * 2);
        for (auto s : output)
            if (s)
            {
                nonSilent.store(1);
                break;
            }
        port::watchdog::stage(port::watchdog::Role::AudioOutput, port::watchdog::Stage::AudioOutput);
        const int r = sceAudioOutOutput(channel, output.data());
        port::watchdog::stage(port::watchdog::Role::AudioOutput, port::watchdog::Stage::AudioDump);
        PortAudioDumpWrite(output.data(), 512);
        nextBlock ^= 1;
        const auto calls = outputCalls.fetch_add(1) + 1;
        if (port::watchdog::enabled())
            port::watchdog::progress(port::watchdog::Role::AudioOutput, uint32_t(calls),
                                     uint32_t(buffers.load(std::memory_order_relaxed)));
        if (r < 0)
        {
            outputError.store(r);
            underruns.fetch_add(1);
            running.store(false);
            port::watchdog::stage(port::watchdog::Role::AudioOutput, port::watchdog::Stage::AudioLog);
            std::fprintf(stderr, "[audio] output failed=%08x\n", unsigned(r));
            break;
        }
        if (PortDiagnosticsEnabled() && (calls == 1 || calls % 100 == 0))
        {
            PortAudioMixInfo mix{};
            // Public game audio APIs can mutate these lists between mixer ticks.
            // Copy under their MusyX lock, then release it before log I/O.
            port::watchdog::stage(port::watchdog::Role::AudioOutput, port::watchdog::Stage::AudioStatsLock);
            hwIRQEnterCritical();
            port::watchdog::stage(port::watchdog::Role::AudioOutput, port::watchdog::Stage::AudioStats);
            const int mixed = PortAudioMixStats(&mix);
            hwIRQLeaveCritical();
            port::watchdog::stage(port::watchdog::Role::AudioOutput, port::watchdog::Stage::AudioLog);
            std::fprintf(
                stderr,
                "[audio] outputs=%lu ticks=%lu underruns=%lu non_silent=%d error=%08x mix=%d "
                "studios=%d voices=%d samples=%d env=%u vol=%u peak=%d mixer_us=%llu max_us=%llu\n",
                calls, buffers.load(), underruns.load(), nonSilent.load(),
                unsigned(outputError.load()), mixed, mix.studios, mix.voices, mix.withSample,
                mix.peakEnv, mix.peakVol, mix.busPeak, mixTotalUs.load(), mixMaxUs.load());
        }
        used -= 512;
        std::memmove(queue.data(), queue.data() + 1024, used * 4);
        port::watchdog::stage(port::watchdog::Role::AudioOutput, port::watchdog::Stage::AudioLoop);
    }
    end_audio_watchdog_probe();
    port::watchdog::leave_thread(port::watchdog::Role::AudioOutput);
    return nullptr;
}
}   // namespace
extern "C" {
int PortAudioStart(void)
{
    if (channel >= 0)
        return 1;
    channel =
        sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, 512, 48000, SCE_AUDIO_OUT_MODE_STEREO);
    if (channel < 0)
    {
        std::fprintf(stderr, "[audio] open failed=%08x\n", unsigned(channel));
        return 0;
    }
    int volume[2]{ SCE_AUDIO_VOLUME_0DB, SCE_AUDIO_VOLUME_0DB };
    const int volumeResult =
        sceAudioOutSetVolume(channel,
                             static_cast<SceAudioOutChannelFlag>(SCE_AUDIO_VOLUME_FLAG_L_CH |
                                                                 SCE_AUDIO_VOLUME_FLAG_R_CH),
                             volume);
    if (volumeResult < 0)
    {
        std::fprintf(stderr, "[audio] volume failed=%08x\n", unsigned(volumeResult));
        sceAudioOutReleasePort(channel);
        channel = -1;
        return 0;
    }
    buffers.store(0);
    underruns.store(0);
    nonSilent.store(0);
    outputError.store(0);
    outputCalls.store(0);
    mixTotalUs.store(0);
    mixMaxUs.store(0);
    mixOver2ms.store(0);
    std::fprintf(stderr,
                 "[audio] native stereo 48kHz, double buffer 512 frames, mixer/output on CPU0\n");
    pthread_attr_t attr;
    int error = pthread_attr_init(&attr);
    if (!error)
    {
        error = pthread_attr_setstacksize(&attr, 0x20000);
        running.store(true);
        if (!error)
            error = pthread_create(&thread, &attr, audio_thread, nullptr);
        pthread_attr_destroy(&attr);
    }
    if (error)
    {
        running.store(false);
        sceAudioOutReleasePort(channel);
        channel = -1;
        std::fprintf(stderr, "[audio] pthread create error=%d\n", error);
        return 0;
    }
    threadStarted = true;
    return 1;
}
void PortAudioStop(void)
{
    running.store(false);
    if (threadStarted)
    {
        pthread_join(thread, nullptr);
        threadStarted = false;
    }
    if (channel >= 0)
    {
        sceAudioOutReleasePort(channel);
        channel = -1;
    }
}
void PortAudioUpdate(void) {}
int PortAudioDeviceOpen(void) { return channel >= 0; }
void PortAudioStats(unsigned long* b, unsigned long* u, int* s)
{
    if (b)
        *b = buffers.load();
    if (u)
        *u = underruns.load();
    if (s)
        *s = nonSilent.load();
}
void PortAudioUpdateCost(unsigned long* c, double* mean, double* max, unsigned long* over2ms)
{
    if (c)
        *c = buffers.load();
    if (mean)
        *mean = buffers.load() ? double(mixTotalUs.load()) / buffers.load() / 1000.0 : 0;
    if (max)
        *max = double(mixMaxUs.load()) / 1000.0;
    if (over2ms)
        *over2ms = mixOver2ms.load();
}
}
#endif
