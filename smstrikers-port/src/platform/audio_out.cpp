// SDL3 audio transport for MusyX, adapted from Dusklight's DuskAudioSystem (CC0-1.0,
// https://github.com/TwilitRealm/dusklight).

#include "port/audio.h"
#include "port/host.h"
#include "port/vita_profiler.h"

#if defined(PORT_USE_AURORA)

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>


// salPortNextBuffer() returns the ring slot the DAC would be playing and advances MusyX by one 5 ms
// tick.
extern "C" {
void* salPortNextBuffer(void);
unsigned int salPortBufferBytes(void);
}

namespace {

// 0x280-byte buffers: 160 stereo s16 frames, 5 ms at 32 kHz, the rate salInitAi picks.
constexpr int kSampleRate = 32000;
constexpr int kChannels = 2;

// How far ahead to keep the device fed: 30 ms covers a dropped frame at 60 Hz.
constexpr int kTargetBuffers = 6;

// Ceiling on catch-up, so a long stall does not run the sequencer forward at speed; past this the
// gap is lost.
constexpr int kMaxBuffersPerUpdate = 24;

SDL_AudioStream* s_stream = nullptr;
// Bytes the device pulls at once, queued in addition to kTargetBuffers.
int s_pullBytes = 0;
bool s_ownsSubsystem = false;
bool s_failed = false;
int s_logging = -1;

std::atomic<unsigned long> s_buffers{0};      // ticks handed to the device
std::atomic<unsigned long> s_underruns{0};    // fills that found the queue already empty
std::atomic<bool> s_everNonSilent{false};
bool s_reported = false;

bool logging() {
    if (s_logging < 0) {
        const char* e = getenv("STRIKERS_LOG_AUDIO");
        s_logging = (e != nullptr && *e != '\0') ? 1 : 0;
    }
    return s_logging != 0;
}

std::atomic<unsigned long> s_updateCalls{0};
std::atomic<unsigned long long> s_updateTotalUs{0};
std::atomic<unsigned long long> s_updateMaxUs{0};
std::atomic<unsigned long> s_updateOver2ms{0};


void report() {
    const unsigned long updateCalls = s_updateCalls.load(std::memory_order_relaxed);
    const unsigned long long updateTotalUs = s_updateTotalUs.load(std::memory_order_relaxed);
    const unsigned long long updateMaxUs = s_updateMaxUs.load(std::memory_order_relaxed);
    const unsigned long updateOver2ms = s_updateOver2ms.load(std::memory_order_relaxed);
    if (logging() && updateCalls != 0)
        std::fprintf(stderr,
                     "[port] audio: fill cost over %lu runs: mean %.3f ms, max %.2f ms, "
                     "%lu runs over 2 ms\n",
                     updateCalls, (double)updateTotalUs / (double)updateCalls / 1000.0,
                     (double)updateMaxUs / 1000.0, updateOver2ms);
    const unsigned long buffers = s_buffers.load(std::memory_order_relaxed);
    if (s_reported || !logging() || buffers == 0)
        return;
    s_reported = true;
    const unsigned int frames = salPortBufferBytes() / (kChannels * sizeof(int16_t));
    std::fprintf(stderr,
                 "[port] audio: %lu ticks (%.1f s of output), %lu underruns, %s\n",
                 buffers, (double)buffers * (double)frames / (double)kSampleRate,
                 s_underruns.load(std::memory_order_relaxed),
                 s_everNonSilent.load(std::memory_order_relaxed)
                     ? "output was non-silent"
                     : "output was silent throughout");
}

void recordFillCost(Uint64 startCounter, unsigned long buffersBefore) {
    const double elapsedUs = (double)(SDL_GetPerformanceCounter() - startCounter) * 1000000.0
                           / (double)SDL_GetPerformanceFrequency();
    const unsigned long long us = elapsedUs > 0.0 ? (unsigned long long)elapsedUs : 0;
    s_updateCalls.fetch_add(1, std::memory_order_relaxed);
    s_updateTotalUs.fetch_add(us, std::memory_order_relaxed);

    unsigned long long previousMax = s_updateMaxUs.load(std::memory_order_relaxed);
    while (previousMax < us
           && !s_updateMaxUs.compare_exchange_weak(previousMax, us, std::memory_order_relaxed)) {
    }

    if (us > 2000) {
        const unsigned long over = s_updateOver2ms.fetch_add(1, std::memory_order_relaxed) + 1;
        if (logging() && over <= 40) {
            const unsigned long buffersAfter = s_buffers.load(std::memory_order_relaxed);
            std::fprintf(stderr,
                         "[port] audio: slow fill %.1f ms for %lu ticks at tick %lu\n",
                         (double)us / 1000.0, buffersAfter - buffersBefore, buffersAfter);
        }
    }
}

void audioFillQueue() {
    if (s_stream == nullptr)
        return;

    const unsigned int bufBytes = salPortBufferBytes();
    if (bufBytes == 0)
        return;

    const int queued = SDL_GetAudioStreamQueued(s_stream);
    if (queued < 0)
        return;

    if (queued == 0 && s_buffers.load(std::memory_order_relaxed) != 0)
        s_underruns.fetch_add(1, std::memory_order_relaxed);

    const int target = static_cast<int>(bufBytes) * kTargetBuffers + s_pullBytes;
    int want = (target - queued + static_cast<int>(bufBytes) - 1) / static_cast<int>(bufBytes);
    if (want <= 0)
        return;
    if (want > kMaxBuffersPerUpdate)
        want = kMaxBuffersPerUpdate;

    PortProfilerAudioFillBegin();
    const Uint64 startCounter = SDL_GetPerformanceCounter();
    const unsigned long buffersBefore = s_buffers.load(std::memory_order_relaxed);
    int produced = 0;

    for (int i = 0; i < want; ++i) {
        PortProfilerAudioTickBegin();
        void* pcm = salPortNextBuffer();
        PortProfilerAudioTickEnd();
        if (pcm == nullptr)
            break;

        if (!s_everNonSilent.load(std::memory_order_relaxed)) {
            const int16_t* p = static_cast<const int16_t*>(pcm);
            const unsigned int n = bufBytes / sizeof(int16_t);
            bool nonSilent = false;
            for (unsigned int j = 0; j < n; ++j) {
                if (p[j] != 0) {
                    nonSilent = true;
                    break;
                }
            }
            if (nonSilent && !s_everNonSilent.exchange(true, std::memory_order_relaxed)
                && logging()) {
                std::fprintf(stderr, "[port] audio: first non-silent buffer at tick %lu\n",
                             s_buffers.load(std::memory_order_relaxed));
            }
        }

        // What the device is given, which while a movie is playing is not what the mixer rendered.
        PortAudioDumpWrite(pcm, bufBytes / (kChannels * sizeof(int16_t)));

        if (!SDL_PutAudioStreamData(s_stream, pcm, static_cast<int>(bufBytes)))
            break;
        s_buffers.fetch_add(1, std::memory_order_relaxed);
        ++produced;
    }

    const int finalQueued = SDL_GetAudioStreamQueued(s_stream);
    PortProfilerAudioQueue(finalQueued >= 0 ? finalQueued : queued, produced);
    recordFillCost(startCounter, buffersBefore);
    PortProfilerAudioFillEnd();
}


} // namespace

int PortAudioStart(void) {
    if (s_stream != nullptr)
        return 1;
    if (s_failed)
        return 0;

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "[port] audio: SDL_InitSubSystem failed: %s\n", SDL_GetError());
        s_failed = true;
        return 0;
    }
    s_ownsSubsystem = true;

    SDL_AudioSpec spec;
    std::memset(&spec, 0, sizeof(spec));
    // Host-endian s16: nothing on this path carries console byte order.
    spec.format = SDL_AUDIO_S16;
    spec.channels = kChannels;
    spec.freq = kSampleRate;

    s_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (s_stream == nullptr) {
        std::fprintf(stderr, "[port] audio: no output device (%s); running silent\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        s_ownsSubsystem = false;
        s_failed = true;
        return 0;
    }

#if defined(__SWITCH__)
    // Switch's SDL2 takes a whole device buffer per pull and pads any shortfall with silence.
    {
        SDL_AudioSpec dev;
        int frames = 0;
        std::memset(&dev, 0, sizeof(dev));
        if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(s_stream), &dev, &frames) && frames > 0
            && dev.freq > 0)
            s_pullBytes = static_cast<int>(static_cast<long long>(frames) * kSampleRate / dev.freq)
                          * kChannels * static_cast<int>(sizeof(int16_t));
    }
#endif

    SDL_ResumeAudioStreamDevice(s_stream);
    // atexit, not PortAudioStop: salExitAi is reached only if the game shuts MusyX down, and it
    // does not.
    std::atexit(report);

    if (logging()) {
        SDL_AudioDeviceID dev = SDL_GetAudioStreamDevice(s_stream);
        SDL_AudioSpec got;
        int frames = 0;
        std::memset(&got, 0, sizeof(got));
        if (SDL_GetAudioDeviceFormat(dev, &got, &frames)) {
            std::fprintf(stderr,
                         "[port] audio: device \"%s\" %d Hz %d ch fmt 0x%x, %d-frame buffer; "
                         "feeding %d Hz s16 stereo in %u-byte ticks\n",
                         SDL_GetAudioDeviceName(dev), got.freq, got.channels,
                         (unsigned)got.format, frames, kSampleRate, salPortBufferBytes());
        }
    }
    return 1;
}

int PortAudioDeviceOpen(void) { return s_stream != nullptr ? 1 : 0; }

void PortAudioUpdateCost(unsigned long* calls, double* meanMs, double* maxMs,
                         unsigned long* over2ms) {
    const unsigned long updateCalls = s_updateCalls.load(std::memory_order_relaxed);
    const unsigned long long totalUs = s_updateTotalUs.load(std::memory_order_relaxed);
    if (calls) *calls = updateCalls;
    if (meanMs) *meanMs = updateCalls ? (double)totalUs / (double)updateCalls / 1000.0 : 0.0;
    if (maxMs) *maxMs = (double)s_updateMaxUs.load(std::memory_order_relaxed) / 1000.0;
    if (over2ms) *over2ms = s_updateOver2ms.load(std::memory_order_relaxed);
}

void PortAudioStop(void) {
    if (s_stream != nullptr) {
        report();
        SDL_DestroyAudioStream(s_stream);
        s_stream = nullptr;
    }
    if (s_ownsSubsystem) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        s_ownsSubsystem = false;
    }
}

void PortAudioUpdate(void) {
    audioFillQueue();
}

void PortAudioStats(unsigned long* outBuffers, unsigned long* outUnderruns, int* outEverNonSilent) {
    if (outBuffers != nullptr)
        *outBuffers = s_buffers.load(std::memory_order_relaxed);
    if (outUnderruns != nullptr)
        *outUnderruns = s_underruns.load(std::memory_order_relaxed);
    if (outEverNonSilent != nullptr)
        *outEverNonSilent = s_everNonSilent.load(std::memory_order_relaxed) ? 1 : 0;
}

#else // !PORT_USE_AURORA

int PortAudioStart(void) { return 0; }
void PortAudioStop(void) {}
void PortAudioUpdate(void) {}
void PortAudioStats(unsigned long* outBuffers, unsigned long* outUnderruns, int* outEverNonSilent) {
    if (outBuffers != nullptr)
        *outBuffers = 0;
    if (outUnderruns != nullptr)
        *outUnderruns = 0;
    if (outEverNonSilent != nullptr)
        *outEverNonSilent = 0;
}

#endif
