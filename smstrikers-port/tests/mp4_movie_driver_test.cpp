#include "port/mp4_movie.hpp"
#include "port/asset_archive.h"
#include <psp2/avplayer.h>
#include <psp2/audioout.h>
#include <psp2/sysmodule.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>

namespace mock
{
SceAvPlayerInitData init{};
constexpr SceAvPlayerHandle playerHandle = static_cast<int>(0x81f05ec0u);
std::vector<uint8_t> meta, mp4(70001, 0x5a), frame(64 * 64 * 3 / 2);
std::atomic<uint64_t> now{1};
std::atomic<unsigned> audioPending{0}, audioOutputs{0};
std::atomic<int> audioVolume{-1};
std::array<uint8_t, 4096> pcm{};
bool active = true, nextFrame = false, failInit = false, failSource = false, loaded = false;
bool badShape = false, looping = false;
unsigned opens = 0, closes = 0, creates = 0, destroys = 0, moduleLoads = 0, moduleUnloads = 0;
void setup()
{
    meta.assign(32, 0);
    std::memcpy(meta.data(), "STMP4001", 8);
    const uint32_t words[]{64,64,60,30,1,0};
    for (unsigned i = 0; i < 6; ++i)
        for (unsigned j = 0; j < 4; ++j) meta[8 + i * 4 + j] = words[i] >> (j * 8);
    std::memcpy(mp4.data() + 4, "ftyp", 4);
    for (size_t i = 0; i < frame.size(); ++i) frame[i] = uint8_t(i);
    active = true;
    nextFrame = false;
    badShape = false;
}
}
struct PortAssetArchive {};
extern "C"
{
unsigned long long port_monotonic_ns() { return mock::now.load(); }
void* memalign(size_t alignment, size_t size)
{
    void* result = nullptr;
    return posix_memalign(&result, alignment, size) ? nullptr : result;
}
PortAssetArchive* port_asset_archive_open(const char*, char*, size_t)
{ ++mock::opens; return new PortAssetArchive; }
void port_asset_archive_close(PortAssetArchive* p)
{ if (p) { ++mock::closes; delete p; } }
int port_asset_archive_find(const PortAssetArchive*, const char* name)
{
    if (!std::strcmp(name, "native/v2/video/intromovie.mp4")) return 0;
    if (!std::strcmp(name, "native/v2/video/intromovie.mp4.meta")) return 1;
    return -1;
}
uint64_t port_asset_archive_size(const PortAssetArchive*, unsigned i)
{ return i ? mock::meta.size() : mock::mp4.size(); }
long port_asset_archive_read(PortAssetArchive*, unsigned i, void* out, size_t bytes, uint64_t offset)
{
    const auto& b = i ? mock::meta : mock::mp4;
    if (offset >= b.size()) return 0;
    bytes = std::min(bytes, b.size() - size_t(offset));
    std::memcpy(out, b.data() + offset, bytes);
    return long(bytes);
}
int sceSysmoduleIsLoaded(SceSysmoduleModuleId) { return mock::loaded ? 0 : -1; }
int sceSysmoduleLoadModule(SceSysmoduleModuleId) { mock::loaded = true; ++mock::moduleLoads; return 0; }
int sceSysmoduleUnloadModule(SceSysmoduleModuleId) { mock::loaded = false; ++mock::moduleUnloads; return 0; }
SceUID sceKernelAllocMemBlock(const char*, SceKernelMemBlockType, SceSize, SceKernelAllocMemBlockOpt*) { return -1; }
int sceKernelGetMemBlockBase(SceUID, void**) { return -1; }
int sceKernelFreeMemBlock(SceUID) { return 0; }
SceUID sceKernelFindMemBlockByAddr(const void*, SceSize) { return -1; }
void port_sleep_ns(unsigned long long ns)
{ std::this_thread::sleep_for(std::chrono::nanoseconds(ns)); }
SceAvPlayerHandle sceAvPlayerInit(SceAvPlayerInitData* data)
{
    if (mock::failInit) return -1;
    mock::init = *data;
    ++mock::creates;
    // Generic callbacks must respect SDK-requested alignment.
    void* p = data->memoryReplacement.allocate(nullptr, 64, 101);
    assert(p && !(uintptr_t(p) % 64));
    data->memoryReplacement.deallocate(nullptr, p);
    return mock::playerHandle; // Retail handles can set the signed-int sign bit.
}
int sceAvPlayerAddSource(SceAvPlayerHandle h, const char* name)
{
    assert(h == mock::playerHandle);
    if (mock::failSource) return -1;
    auto& f = mock::init.fileReplacement;
    assert(f.open(f.objectPointer, name) == 0);
    assert(f.open(f.objectPointer, "../../other.mp4") < 0);
    assert(f.size(f.objectPointer) == mock::mp4.size());
    uint8_t bytes[20];
    assert(f.readOffset(f.objectPointer, bytes, 65530, 20) == 20);
    assert(!std::memcmp(bytes, mock::mp4.data() + 65530, 20));
    assert(f.readOffset(f.objectPointer, bytes, mock::mp4.size() - 2, 20) == 2);
    assert(f.readOffset(f.objectPointer, bytes, mock::mp4.size(), 20) == 0);
    return 0;
}
int sceAvPlayerSetLooping(SceAvPlayerHandle, SceBool loop) { mock::looping = loop; return 0; }
int sceAvPlayerStop(SceAvPlayerHandle) { return 0; }
int sceAvPlayerClose(SceAvPlayerHandle)
{
    assert(mock::opens > mock::closes); // archive still alive during callback shutdown
    assert(!mock::init.fileReplacement.close(mock::init.fileReplacement.objectPointer));
    ++mock::destroys;
    return 0;
}
SceBool sceAvPlayerIsActive(SceAvPlayerHandle) { return mock::active; }
SceBool sceAvPlayerGetVideoData(SceAvPlayerHandle, SceAvPlayerFrameInfo* info)
{
    if (!mock::nextFrame) return false;
    mock::nextFrame = false;
    info->pData = mock::frame.data();
    info->details.video.width = mock::badShape ? 128 : 64;
    info->details.video.height = 64;
    info->timeStamp = 33;
    return true;
}
SceBool sceAvPlayerGetAudioData(SceAvPlayerHandle, SceAvPlayerFrameInfo* frame)
{
    if (!mock::audioPending.exchange(0)) return false;
    frame->pData = mock::pcm.data();
    frame->details.audio = {};
    frame->details.audio.channelCount = 2;
    frame->details.audio.sampleRate = 48000;
    frame->details.audio.size = mock::pcm.size();
    return true;
}
int sceAudioOutOpenPort(SceAudioOutPortType, int, int, SceAudioOutMode) { return 1; }
int sceAudioOutSetVolume(int, SceAudioOutChannelFlag, int* volume)
{ assert(volume[0] == volume[1]); mock::audioVolume.store(volume[0]); return 0; }
int sceAudioOutOutput(int, const void* data)
{
    if (data)
    {
        assert(data != mock::pcm.data());
        assert(!std::memcmp(data, mock::pcm.data(), mock::pcm.size()));
        mock::audioOutputs.fetch_add(1);
    }
    return 0;
}
int sceAudioOutReleasePort(int) { return 0; }
}

#define PORT_VITA 1
#include "../src/platform/mp4_movie.cpp"

int main()
{
    using namespace mock;
    setenv("STRIKERS_VITA_NATIVE_VIDEO", "1", 1);
    setenv("STRIKERS_ASSET_ARCHIVE", "archive.psarc", 1);
    port::movie::Info info;
    std::array<port::nativegxm::TextureUpload, 3> planes;
    bool changed = true;
    setup();
    assert(port::movie::open("intromovie.thp", true, true, info));
    assert(looping && info.width == 64 && info.height == 64);
    active = false; // async startup is allowed before the first picture
    assert(port::movie::poll(planes, changed) == 1 && !changed);
    active = true;
    nextFrame = true;
    assert(port::movie::poll(planes, changed) == 1 && changed);
    assert(planes[0].pixels.size() == 4096 && planes[0].opaqueLuminance);
    assert(planes[1].pixels.size() == 1024 && planes[2].pixels.size() == 1024);
    assert(!std::memcmp(planes[0].pixels.data(), frame.data(), 4096));
    for (size_t i = 0; i < 1024; ++i)
    {
        assert(planes[1].pixels[i] == frame[4096 + 2 * i]);
        assert(planes[2].pixels[i] == frame[4096 + 2 * i + 1]);
    }
    const auto owned = planes[0].pixels;
    std::fill(frame.begin(), frame.end(), 0);
    assert(planes[0].pixels == owned);
    active = false;
    assert(port::movie::poll(planes, changed) == 0 && !changed);
    port::movie::close();
    port::movie::close();
    assert(creates == destroys && opens == closes && moduleLoads == 1 && moduleUnloads == 0);

    for (unsigned failure = 0; failure < 5; ++failure)
    {
        setup();
        if (failure == 0) meta[8] = 15;
        if (failure == 1) meta[28] = 2;
        if (failure == 2) meta.pop_back();
        failInit = failure == 3;
        failSource = failure == 4;
        assert(!port::movie::open("intromovie.thp", true, false, info));
        assert(creates == destroys && opens == closes && moduleLoads == 1 && moduleUnloads == 0);
    }
    failInit = failSource = false;
    setup();
    assert(!port::movie::open("../intromovie.thp", true, false, info));
    assert(!port::movie::open("missing.thp", true, false, info));
    assert(port::movie::open("intromovie.thp", false, false, info));
    now += 10000000001ull;
    assert(port::movie::poll(planes, changed) < 0);
    port::movie::close();
    setup();
    assert(port::movie::open("intromovie.thp", true, false, info));
    nextFrame = badShape = true;
    assert(port::movie::poll(planes, changed) < 0 && !changed);
    port::movie::close();
    setup();
    meta[28] = 1; // exercise audio thread startup/join before player teardown
    assert(port::movie::open("intromovie.thp", false, false, info));
    port::movie::poll(planes, changed);
    audioPending.store(1);
    for (unsigned wait = 0; !audioOutputs.load() && wait < 100; ++wait)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(audioOutputs.load() == 1 && audioVolume.load() == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    port::movie::close();
    setup(); meta[28] = 1;
    assert(port::movie::open("intromovie.thp", true, false, info));
    unsigned short curve[128];
    for (unsigned i = 0; i < 128; ++i) curve[i] = unsigned(i * i * 32768ull / (127 * 127));
    assert(port::movie::set_volume(64, 0, curve));
    port::movie::poll(planes, changed);
    audioPending.store(1);
    for (unsigned wait = 0; audioOutputs.load() < 2 && wait < 100; ++wait)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(audioOutputs.load() == 2 && audioVolume.load() == int(unsigned(curve[64]) * SCE_AUDIO_VOLUME_0DB / 32768));
    assert(port::movie::set_volume(0, 1000, curve));
    now += 500000000;
    audioPending.store(1);
    for (unsigned wait = 0; audioOutputs.load() < 3 && wait < 100; ++wait)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(audioOutputs.load() == 3 && audioVolume.load() == int(unsigned(curve[32]) * SCE_AUDIO_VOLUME_0DB / 32768));
    port::movie::close();
    setenv("STRIKERS_VITA_VIDEO_PLAYER", "thp", 1);
    assert(!port::movie::open("intromovie.thp", true, false, info));
    assert(creates == destroys && opens == closes && moduleLoads == 1 && moduleUnloads == 0);
    std::puts("MP4: PSARC offset callbacks, owned YUV, EOF, startup, loop, failures, teardown passed");
}
