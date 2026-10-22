#if defined(PORT_VITA)
#include "port/mp4_movie.hpp"
#include "port/asset_archive.h"
#include "port/host.h"
#include <psp2/avplayer.h>
#include <psp2/audioout.h>
#include <psp2/sysmodule.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <pthread.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <mutex>
extern "C" void* memalign(size_t alignment, size_t size);

namespace port::movie
{
namespace
{
struct Player
{
    PortAssetArchive* archive = nullptr;
    unsigned entry = 0;
    uint64_t bytes = 0, lastProgress = 0, openedAt = 0, pictures = 0, lastTimestamp = 0;
    std::string name;
    Info info;
    int handle = -1;
    bool handleValid = false;
    bool threadStarted = false, sound = false;
    SceAvPlayerInitData init{};
    pthread_t thread{};
    std::atomic<bool> running{false};
    std::atomic<int> error{0};
    std::atomic<bool> playRequested{false};
    std::mutex volumeMutex;
    std::array<unsigned short, 128> volumeCurve{};
    bool curveValid = false;
    float volumeStart = 127, volumeTarget = 127;
    uint64_t fadeStart = 0, fadeDuration = 0;
};
Player player;
uint32_t word(const uint8_t* p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
void* allocate(void*, uint32_t alignment, uint32_t size)
{
    alignment = std::max(alignment, uint32_t(sizeof(void*)));
    return alignment && !(alignment & (alignment - 1)) ? memalign(alignment, size) : nullptr;
}
void deallocate(void*, void* p) { std::free(p); }
void* allocate_frame(void*, uint32_t alignment, uint32_t size)
{
    // CPU-read output; no GL context or borrowed GPU texture is involved.
    constexpr uint32_t page = 1024 * 1024;
    if (!size || size > 16 * page || alignment > page || (alignment && (alignment & (alignment - 1))))
        return nullptr;
    const auto block = sceKernelAllocMemBlock("strikers_mp4_frame",
        SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, (size + page - 1) & ~(page - 1), nullptr);
    void* base = nullptr;
    if (block < 0) return nullptr;
    if (sceKernelGetMemBlockBase(block, &base) < 0 || !base)
    {
        sceKernelFreeMemBlock(block);
        return nullptr;
    }
    return base;
}
void deallocate_frame(void*, void* p)
{
    if (!p) return;
    const auto block = sceKernelFindMemBlockByAddr(p, 0);
    if (block >= 0) sceKernelFreeMemBlock(block);
}
int open_file(void* p, const char* name)
{
    auto& d = *static_cast<Player*>(p);
    return name && d.name == name ? 0 : -1;
}
int close_file(void*) { return 0; }
int read_file(void* p, uint8_t* out, uint64_t offset, uint32_t size)
{
    auto& d = *static_cast<Player*>(p);
    if (size > 0x7fffffff || (!out && size)) return -1;
    return int(port_asset_archive_read(d.archive, d.entry, out, size, offset));
}
uint64_t size_file(void* p) { return static_cast<Player*>(p)->bytes; }
float volume_index(const Player& d, uint64_t now)
{
    if (!d.fadeDuration || now - d.fadeStart >= d.fadeDuration) return d.volumeTarget;
    return d.volumeStart + (d.volumeTarget - d.volumeStart) * float(now - d.fadeStart) / float(d.fadeDuration);
}
void* audio_thread(void* opaque)
{
    auto& d = *static_cast<Player*>(opaque);
    int channel = -1;
    unsigned samples = 0, channels = 0;
    // AudioOut may retain a submitted block until the next call. Never retain
    // AvPlayer's borrowed pointer across GetAudioData calls.
    alignas(64) std::array<std::array<uint8_t, 8192>, 2> blocks{};
    unsigned next = 0;
    int previousVolume = -1;
    while (d.running.load(std::memory_order_acquire))
    {
        if (!d.playRequested.load(std::memory_order_acquire))
        { port_sleep_ns(1000000ull); continue; }
        SceAvPlayerFrameInfo frame{};
        if (!sceAvPlayerGetAudioData(d.handle, &frame))
        {
            port_sleep_ns(1000000ull);
            continue;
        }
        const auto& a = frame.details.audio;
        const unsigned count = a.channelCount ? a.size / (2u * a.channelCount) : 0;
        if (!frame.pData || a.sampleRate != 48000 || a.channelCount != 2 ||
            !count || count % 64 || a.size > blocks[0].size() || a.size != count * 4)
        {
            std::fprintf(stderr, "[mp4] invalid audio rate=%u channels=%u bytes=%u samples=%u\n", a.sampleRate, a.channelCount, a.size, count);
            d.error.store(-1, std::memory_order_release);
            break;
        }
        if (channel < 0)
        {
            channel = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, count, 48000, SCE_AUDIO_OUT_MODE_STEREO);
            if (channel < 0) { std::fprintf(stderr, "[mp4] audio open failed rc=0x%08x samples=%u\n", unsigned(channel), count); d.error.store(channel); break; }
            samples = count;
            channels = a.channelCount;
        }
        int gain = 0;
        {
            std::lock_guard<std::mutex> guard(d.volumeMutex);
            const auto index = std::min(127u, unsigned(std::max(0.f, volume_index(d, port_monotonic_ns()))));
            gain = !d.sound ? 0 : d.curveValid ? int(uint32_t(d.volumeCurve[index]) * SCE_AUDIO_VOLUME_0DB / 32768) : SCE_AUDIO_VOLUME_0DB;
        }
        if (gain != previousVolume)
        {
            int volume[2]{gain, gain};
            const int rc = sceAudioOutSetVolume(channel,
                SceAudioOutChannelFlag(SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH), volume);
            if (rc < 0) { std::fprintf(stderr, "[mp4] audio volume failed rc=0x%08x\n", unsigned(rc)); d.error.store(rc); break; }
            previousVolume = gain;
        }
        if (count != samples || a.channelCount != channels)
        {
            std::fprintf(stderr, "[mp4] audio block changed samples=%u expected=%u channels=%u\n", count, samples, a.channelCount);
            d.error.store(-1);
            break;
        }
        std::memcpy(blocks[next].data(), frame.pData, a.size);
        const int rc = sceAudioOutOutput(channel, blocks[next].data());
        next ^= 1;
        if (rc < 0) { std::fprintf(stderr, "[mp4] audio output failed rc=0x%08x\n", unsigned(rc)); d.error.store(rc); break; }
    }
    if (channel >= 0)
    {
        sceAudioOutOutput(channel, nullptr);
        sceAudioOutReleasePort(channel);
    }
    return nullptr;
}
}

void close() noexcept
{
    auto& d = player;
    d.running.store(false, std::memory_order_release);
    if (d.threadStarted) pthread_join(d.thread, nullptr);
    d.threadStarted = false;
    if (d.handleValid)
    {
        sceAvPlayerStop(d.handle);
        sceAvPlayerClose(d.handle); // closes callbacks before the archive is freed
        std::fprintf(stderr, "[mp4] closed pictures=%llu last_timestamp_ms=%llu wall_ms=%llu error=0x%08x\n",
            (unsigned long long)d.pictures, (unsigned long long)d.lastTimestamp,
            (unsigned long long)((port_monotonic_ns() - d.openedAt) / 1000000), unsigned(d.error.load()));
    }
    d.handle = -1;
    d.handleValid = false;
    port_asset_archive_close(d.archive);
    d.archive = nullptr;
    // Keep AVPLAYER mapped for the process lifetime. A failed initialization
    // can leave its controller thread retiring after the public call returns;
    // unloading here can unmap that thread's next instruction (prefetch abort).
    // Player instances and their frame buffers still close per movie.
    d.error.store(0);
    d.pictures = d.lastTimestamp = 0;
    d.name.clear();
    d.playRequested.store(false);
}

bool set_volume(long volume, long milliseconds, const unsigned short* curve) noexcept
{
    auto& d = player;
    if (!d.handleValid || !curve) return false;
    const auto now = port_monotonic_ns();
    std::lock_guard<std::mutex> guard(d.volumeMutex);
    d.volumeStart = volume_index(d, now);
    d.volumeTarget = float(std::clamp(volume, 0l, 127l));
    d.fadeStart = now;
    d.fadeDuration = uint64_t(std::clamp(milliseconds, 0l, 60000l)) * 1000000;
    std::copy_n(curve, 128, d.volumeCurve.begin());
    d.curveValid = true;
    return true;
}

bool open(const char* source, bool sound, bool loop, Info& info) noexcept
{
    close();
    auto& d = player;
    try
    {
        const char* enabled = std::getenv("STRIKERS_VITA_NATIVE_VIDEO");
        const char* backend = std::getenv("STRIKERS_VITA_VIDEO_PLAYER");
        const char* archive = std::getenv("STRIKERS_ASSET_ARCHIVE");
        const char* videoArchive = std::getenv("STRIKERS_VITA_VIDEO_ARCHIVE");
        if (videoArchive && *videoArchive) archive = videoArchive;
        const char* disabled = std::getenv("STRIKERS_NO_MOVIES");
        if (!enabled || std::strcmp(enabled, "1") || (backend && !std::strcmp(backend, "thp")) ||
            (disabled && *disabled && *disabled != '0') || !archive || !*archive || !source)
            return false;
        while (*source == '/') ++source;
        std::string logical(source);
        if (logical.empty() || logical.find("..") != std::string::npos ||
            logical.find_first_of(":\\") != std::string::npos || !logical.ends_with(".thp"))
            return false;
        logical.replace(logical.size() - 4, 4, ".mp4");
        d.name = "native/v2/video/" + logical;
        char error[256];
        d.archive = port_asset_archive_open(archive, error, sizeof error);
        if (!d.archive) return false;
        const int entry = port_asset_archive_find(d.archive, d.name.c_str());
        const int meta = port_asset_archive_find(d.archive, (d.name + ".meta").c_str());
        uint8_t descriptor[32], header[12];
        if (entry < 0 || meta < 0 || port_asset_archive_size(d.archive, meta) != sizeof descriptor ||
            port_asset_archive_read(d.archive, meta, descriptor, sizeof descriptor, 0) != sizeof descriptor ||
            std::memcmp(descriptor, "STMP4001", 8))
        { close(); return false; }
        d.info = {word(descriptor + 8), word(descriptor + 12)};
        const unsigned frames = word(descriptor + 16), num = word(descriptor + 20), den = word(descriptor + 24),
                       audio = word(descriptor + 28);
        d.bytes = port_asset_archive_size(d.archive, entry);
        if (d.info.width < 16 || d.info.width > 704 || d.info.height < 16 || d.info.height > 768 ||
            d.info.width % 16 || d.info.height % 16 || !frames || frames > 100000 ||
            !num || !den || num > uint64_t(den) * 61 || audio > 1 ||
            d.bytes < 32 || d.bytes > 512u * 1024u * 1024u ||
            port_asset_archive_read(d.archive, entry, header, sizeof header, 0) != sizeof header ||
            std::memcmp(header + 4, "ftyp", 4))
        { close(); return false; }
        d.entry = unsigned(entry);
        if (sceSysmoduleIsLoaded(SCE_SYSMODULE_AVPLAYER) != 0)
        {
            if (sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER) < 0) { close(); return false; }
        }
        // The control worker may inspect replacement callbacks asynchronously.
        // Retain the init table with the process-owned player, not on the stack.
        auto& init = d.init;
        init = {};
        init.memoryReplacement = {&d, allocate, deallocate, allocate_frame, deallocate_frame};
        init.fileReplacement = {&d, open_file, close_file, read_file, size_file};
        init.basePriority = 0xa0;
        init.numOutputVideoFrameBuffers = 2;
        init.autoStart = true;
        std::fprintf(stderr, "[mp4] init source=%s bytes=%llu\n", d.name.c_str(), (unsigned long long)d.bytes);
        d.handle = sceAvPlayerInit(&init);
        // On retail Vita this is an opaque pointer (observed 0x81f05ec0),
        // despite VitaSDK's signed-int typedef. The sign bit is not an error
        // test. SCE error results occupy 0x80xxxxxx, below user RAM 0x81000000.
        const uint32_t handleBits = uint32_t(d.handle);
        if (!handleBits || handleBits == UINT32_MAX || (handleBits & 0xff000000u) == 0x80000000u)
        { std::fprintf(stderr, "[mp4] init failed rc=0x%08x\n", unsigned(d.handle)); close(); return false; }
        d.handleValid = true;
        const int add = sceAvPlayerAddSource(d.handle, d.name.c_str());
        if (add < 0)
        { std::fprintf(stderr, "[mp4] source failed rc=0x%08x\n", unsigned(add)); close(); return false; }
        const int looping = sceAvPlayerSetLooping(d.handle, loop);
        if (looping < 0)
        { std::fprintf(stderr, "[mp4] loop failed rc=0x%08x\n", unsigned(looping)); close(); return false; }
        d.sound = sound;
        d.curveValid = false;
        d.fadeDuration = 0;
        d.volumeStart = d.volumeTarget = 127;
        d.openedAt = d.lastProgress = port_monotonic_ns();
        if (audio)
        {
            d.running.store(true, std::memory_order_release);
            if (pthread_create(&d.thread, nullptr, audio_thread, &d)) { close(); return false; }
            d.threadStarted = true;
        }
        info = d.info;
        std::fprintf(stderr, "[mp4] SceAvPlayer selected=%s bytes=%llu size=%ux%u audio=%u loop=%u\n",
            d.name.c_str(), (unsigned long long)d.bytes, info.width, info.height, audio, unsigned(loop));
        return true;
    }
    catch (...) { close(); return false; }
}

int poll(std::array<nativegxm::TextureUpload, 3>& planes, bool& changed) noexcept
{
    changed = false;
    auto& d = player;
    if (!d.handleValid || d.error.load(std::memory_order_acquire)) return -1;
    d.playRequested.store(true, std::memory_order_release);
    try
    {
        SceAvPlayerFrameInfo frame{};
        if (sceAvPlayerGetVideoData(d.handle, &frame))
        {
            const auto w = d.info.width, h = d.info.height;
            // SceAvPlayer supplies tightly packed YUV420 semi-planar output;
            // copy before the next poll can recycle the decoder's frame.
            if (!frame.pData || frame.details.video.width != w || frame.details.video.height != h)
                return -1;
            for (unsigned i = 0; i < 3; ++i)
            {
                auto& plane = planes[i];
                plane.width = i ? w / 2 : w;
                plane.height = i ? h / 2 : h;
                plane.stride = plane.width;
                plane.mipCount = 1;
                plane.storage = nativegxm::TextureStorage::Intensity8;
                plane.swizzled = false;
                plane.opaqueLuminance = true;
                plane.pixels.resize(size_t(plane.width) * plane.height);
            }
            std::memcpy(planes[0].pixels.data(), frame.pData, size_t(w) * h);
            const auto* uv = frame.pData + size_t(w) * h;
            for (size_t i = 0; i < size_t(w) * h / 4; ++i)
            {
                planes[1].pixels[i] = uv[i * 2];
                planes[2].pixels[i] = uv[i * 2 + 1];
            }
            d.lastProgress = port_monotonic_ns();
            d.lastTimestamp = frame.timeStamp;
            ++d.pictures;
            changed = true;
            return 1;
        }
        if (d.pictures && !sceAvPlayerIsActive(d.handle))
        { std::fprintf(stderr, "[mp4] player inactive after pictures=%llu\n", (unsigned long long)d.pictures); return 0; }
        if (port_monotonic_ns() - d.lastProgress > 10000000000ull)
        {
            std::fprintf(stderr, "[mp4] video stalled; stopping playback\n");
            return -1;
        }
        return 1;
    }
    catch (...) { return -1; }
}
}
#endif
