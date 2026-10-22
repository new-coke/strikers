#include "port/native_video.h"
#if defined(PORT_VITA)
#include "port/native_video_record.hpp"
#include "port/native_video_sps.hpp"
#include "port/asset_archive.h"
#include "port/host.h"
#include "NL/nlFileGC.h"
#include <psp2/videodec.h>
#include <psp2/sysmodule.h>
#include <psp2/kernel/sysmem.h>
#include <cstdio>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

// VitaSDK's stub exports these entry points but its public header omits the
// declarations. Stop drains pictures; Flush resets the stream for the next AU.
// ABI/use: fish47/FFmpeg-vita, ce9de7392afdad13783bc60e17b7726edfc56961,
// libavcodec/vitadec_video.c (public Decode path, before the internal API port).
extern "C" int sceAvcdecDecodeStop(SceAvcdecCtrl*, SceAvcdecArrayPicture*);
extern "C" int sceAvcdecDecodeFlush(SceAvcdecCtrl*);
namespace
{
const unsigned MinimumDecoderDimension = 64, Alignment = 1024 * 1024;
// These movie-component intervals are independent of benchmark CSV samples.
// The total contains its disjoint children and the small gaps between calls;
// it begins after component validation/locking and excludes profile/outcome
// logs and teardown. Init counts both cold setup and the cached init() check;
// cold setup includes its existing single hardware-initialized diagnostic.
struct VideoProfile
{
    enum Phase { Prepare, Init, Decode, Stop, Flush, Tile, Count };
    struct Stat
    {
        unsigned calls = 0;
        uint64_t ns = 0, maximum = 0;
        void add(uint64_t elapsed)
        {
            ++calls;
            ns += elapsed;
            if (elapsed > maximum) maximum = elapsed;
        }
    };
    bool configured = false, enabled = false, active = false;
    unsigned first = 0, last = 0, pictures = 0, errors = 0;
    Phase phase = Count;
    uint64_t start = 0, boundary = 0;
    std::array<Stat, Count> children{};
    Stat total{};

    void begin(unsigned component)
    {
        if (!configured)
        {
            const auto* flag = std::getenv("STRIKERS_VITAGL_PACKET_PROFILE");
            enabled = flag && !std::strcmp(flag, "1");
            configured = true;
        }
        if (!enabled) return;
        if (!total.calls) first = component;
        last = component;
        active = true;
        phase = Prepare;
        start = boundary = port_monotonic_ns();
    }
    void next(Phase nextPhase)
    {
        if (!active) return;
        const auto now = port_monotonic_ns();
        if (phase != Count) children[phase].add(now - boundary);
        phase = nextPhase;
        boundary = now;
    }
    void report(const char* reason)
    {
        if (!enabled || !total.calls) return;
        std::fprintf(stderr,
            "[native-video-profile] reason=%s first_component=%u last_component=%u components=%u pictures=%u errors=%u "
            "total_ns=%llu total_max_ns=%llu "
            "prepare_calls=%u prepare_ns=%llu prepare_max_ns=%llu init_calls=%u init_ns=%llu init_max_ns=%llu "
            "decode_calls=%u decode_ns=%llu decode_max_ns=%llu stop_calls=%u stop_ns=%llu stop_max_ns=%llu "
            "flush_calls=%u flush_ns=%llu flush_max_ns=%llu tile_calls=%u tile_ns=%llu tile_max_ns=%llu\n",
            reason, first, last, total.calls, pictures, errors,
            (unsigned long long)total.ns, (unsigned long long)total.maximum,
            children[Prepare].calls, (unsigned long long)children[Prepare].ns, (unsigned long long)children[Prepare].maximum,
            children[Init].calls, (unsigned long long)children[Init].ns, (unsigned long long)children[Init].maximum,
            children[Decode].calls, (unsigned long long)children[Decode].ns, (unsigned long long)children[Decode].maximum,
            children[Stop].calls, (unsigned long long)children[Stop].ns, (unsigned long long)children[Stop].maximum,
            children[Flush].calls, (unsigned long long)children[Flush].ns, (unsigned long long)children[Flush].maximum,
            children[Tile].calls, (unsigned long long)children[Tile].ns, (unsigned long long)children[Tile].maximum);
        first = last = pictures = errors = 0;
        children = {};
        total = {};
    }
    void finish(bool picture)
    {
        if (!active) return;
        const auto now = port_monotonic_ns();
        if (phase != Count) children[phase].add(now - boundary);
        total.add(now - start);
        pictures += picture;
        errors += !picture;
        active = false;
        phase = Count;
        // One aggregate line after the measured interval; no per-frame I/O.
        if (total.calls == 300) report("interval");
    }
    void reset()
    {
        finish(false);
        report(errors ? "error" : "reset");
        *this = {};
    }
};
struct Decoder
{
    std::mutex mutex;
    SceAvcdecCtrl control{};
    SceUID reference = -1, raster = -1, staging = -1;
    uint8_t* pixels = nullptr;
    uint8_t* stagingPixels = nullptr;
    size_t stagingBytes = 0;
    bool moduleOwned = false, library = false, created = false;
    unsigned width = 0, height = 0, pitch = 0;
    std::vector<uint8_t> elementary;
    unsigned pictures = 0, components = 0, decodeAUs = 0, stopCalls = 0, flushCalls = 0;
    VideoProfile profile;
    ~Decoder() { reset(); }
    void reset()
    {
        profile.reset();
        if (created)
            sceAvcdecDeleteDecoder(&control);
        created = false;
        control = {};
        if (reference >= 0)
            sceKernelFreeMemBlock(reference);
        if (raster >= 0)
            sceKernelFreeMemBlock(raster);
        if (staging >= 0)
            sceKernelFreeMemBlock(staging);
        reference = raster = staging = -1;
        pixels = stagingPixels = nullptr;
        stagingBytes = 0;
        width = height = pitch = 0;
        pictures = components = decodeAUs = stopCalls = flushCalls = 0;
        if (library)
            sceVideodecTermLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC);
        library = false;
        if (moduleOwned)
            sceSysmoduleUnloadModule(SCE_SYSMODULE_AVCDEC);
        moduleOwned = false;
    }
    bool fail(const char* operation, int rc)
    {
        profile.finish(false);
        std::fprintf(stderr, "[native-video] %s failed=0x%08x component=%u decode_au=%u stop_calls=%u flush_calls=%u; movie skipped\n",
                     operation, unsigned(rc), components, decodeAUs, stopCalls, flushCalls);
        reset();
        return false;
    }
    void init_staging()
    {
        const char* flag = std::getenv("STRIKERS_VITA_NATIVE_VIDEO_STAGING");
        if (!flag || std::strcmp(flag, "1"))
            return;
        // This is CPU-only storage. The hardware still writes the original
        // non-cacheable raster; no decoder pointer or packed NV12 layout changes.
        // Init runs once per movie geometry, including a failed optional attempt.
        const unsigned page = 4096;
        if (!pitch || !height || height > Alignment / pitch)
            return;
        const size_t yBytes = size_t(pitch) * height;
        if (yBytes / 2 > Alignment - yBytes)
            return;
        const size_t bytes = yBytes + yBytes / 2;
        const unsigned allocationBytes = unsigned((bytes + page - 1) & ~(size_t(page) - 1));
        const SceUID block = sceKernelAllocMemBlock("strikers_avc_staging",
                                                  SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
                                                  allocationBytes, nullptr);
        void* base = nullptr;
        int rc = block < 0 ? block : sceKernelGetMemBlockBase(block, &base);
        if (rc >= 0 && !base)
            rc = -EINVAL;
        if (rc < 0)
        {
            if (block >= 0)
                sceKernelFreeMemBlock(block);
            std::fprintf(stderr, "[native-video-staging] active=0 bytes=%u allocation_bytes=%u fallback_rc=0x%08x\n",
                         unsigned(bytes), allocationBytes, unsigned(rc));
            return;
        }
        staging = block;
        stagingPixels = static_cast<uint8_t*>(base);
        stagingBytes = bytes;
        std::fprintf(stderr, "[native-video-staging] active=1 bytes=%u allocation_bytes=%u\n",
                     unsigned(bytes), allocationBytes);
    }
    bool init(unsigned frameWidth, unsigned frameHeight)
    {
        if (created)
        {
            // THP's destination planes belong to one fixed-size movie. A new
            // geometry requires its normal close/reset before another decode.
            if (width != frameWidth || height != frameHeight)
                return fail("stream geometry change", -EINVAL);
            return true;
        }
        width = frameWidth;
        height = frameHeight;
        // WAVC already requires 16-pixel alignment. Use the public decoder's
        // minimum 64-pixel capacity (FFmpeg-vita do_update_dimension), while
        // keeping the picture's actual dimensions and pixels unchanged.
        pitch = width < MinimumDecoderDimension ? MinimumDecoderDimension : width;
        const unsigned rows = height < MinimumDecoderDimension ? MinimumDecoderDimension : height;
        int rc;
        if (sceSysmoduleIsLoaded(SCE_SYSMODULE_AVCDEC) != 0)
        {
            rc = sceSysmoduleLoadModule(SCE_SYSMODULE_AVCDEC);
            if (rc < 0)
                return fail("module", rc);
            moduleOwned = true;
        }
        SceVideodecQueryInitInfoHwAvcdec initInfo{ sizeof(initInfo), pitch, rows, 1, 1 };
        rc = sceVideodecInitLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC, &initInfo);
        if (rc < 0)
            return fail("library", rc);
        library = true;
        SceAvcdecQueryDecoderInfo query{ pitch, rows, 1 };
        SceAvcdecDecoderInfo info{};
        rc = sceAvcdecQueryDecoderMemSize(SCE_VIDEODEC_TYPE_HW_AVCDEC, &query, &info);
        if (rc < 0 || !info.frameMemSize || info.frameMemSize > 32 * Alignment)
            return fail("memory query", rc);
        const unsigned bytes = (info.frameMemSize + Alignment - 1) & ~(Alignment - 1);
        reference = sceKernelAllocMemBlock("strikers_avc_reference",
                                           SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, bytes,
                                           nullptr);
        if (reference < 0)
            return fail("reference memory", reference);
        control.frameBuf.size = bytes;
        rc = sceKernelGetMemBlockBase(reference, &control.frameBuf.pBuf);
        if (rc < 0)
            return fail("reference pointer", rc);
        raster = sceKernelAllocMemBlock("strikers_avc_nv12",
                                        SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, Alignment,
                                        nullptr);
        if (raster < 0)
            return fail("output memory", raster);
        void* base = nullptr;
        rc = sceKernelGetMemBlockBase(raster, &base);
        if (rc < 0)
            return fail("output pointer", rc);
        pixels = static_cast<uint8_t*>(base);
        init_staging();
        rc = sceAvcdecCreateDecoder(SCE_VIDEODEC_TYPE_HW_AVCDEC, &control, &query);
        if (rc < 0)
            return fail("create decoder", rc);
        created = true;
        std::fprintf(stderr,
                     "[native-video] hardware initialized reference_bytes=%u output_bytes=%u "
                     "width=%u height=%u pitch=%u capacity_rows=%u\n",
                     bytes, Alignment, width, height, pitch, rows);
        return true;
    }
} decoder;
class NativeMovieFile : public GCFile
{
    PortAssetArchive* archive;
    unsigned entry, size;
    int status = DVD_STATE_END;

  public:
    NativeMovieFile(PortAssetArchive* a, unsigned e, unsigned n) : archive(a), entry(e), size(n) {}
    ~NativeMovieFile() { port_asset_archive_close(archive); }
    u32 FileSize(unsigned int* p) override
    {
        if (p)
            *p = size;
        return size;
    }
    s32 GetReadStatus() override { return status; }
    void ReadAsync(void* target, unsigned long bytes, unsigned long offset) override
    {
        status = port_asset_archive_read(archive, entry, target, bytes, offset) == long(bytes)
                     ? DVD_STATE_END
                     : DVD_STATE_FATAL_ERROR;
    }
    u32 GetDiscPosition() override { return 0; }
};
}   // namespace
extern "C" int PortNativeVideoEnabled()
{
    const char* flag = std::getenv("STRIKERS_VITA_NATIVE_VIDEO");
    return flag && !std::strcmp(flag, "1");
}
extern "C" void PortNativeVideoReset()
{
    std::lock_guard<std::mutex> lock(decoder.mutex);
    decoder.reset();
}
extern "C" int PortNativeVideoDecode(const void* data, size_t bytes, void* y, void* u, void* v)
{
    port::NativeVideoComponent component;
    if (!PortNativeVideoEnabled() || !y || !u || !v ||
        !port::parse_native_video(data, bytes, component))
        return -1;
    std::lock_guard<std::mutex> lock(decoder.mutex);
    decoder.profile.begin(decoder.components + 1);
    port::NativeVideoSpsStatus sps;
    if (!port::prepare_native_video_au(component, decoder.elementary, sps))
    {
        decoder.profile.finish(false);
        std::fprintf(stderr, "[native-video] SPS preparation failed component=%u reason=%s; movie skipped\n",
                     decoder.components + 1, sps.error ? sps.error : "unknown");
        decoder.reset();
        return -1;
    }
    decoder.profile.next(VideoProfile::Init);
    if (!decoder.init(component.width, component.height))
        return -1;
    decoder.profile.next(VideoProfile::Count);
    ++decoder.components;
    const size_t esBytes = decoder.elementary.size();
    try { decoder.elementary.resize(esBytes + 64); }
    catch (...) { decoder.fail("AU memory", -ENOMEM); return -1; }
    std::memset(decoder.elementary.data() + esBytes, 0, 64);
    SceAvcdecAu au{};
    au.pts = { UINT32_MAX, UINT32_MAX };
    au.dts = au.pts;
    au.es = { decoder.elementary.data(), uint32_t(esBytes) };
    SceAvcdecPicture picture{};
    const size_t yBytes = size_t(decoder.pitch) * component.height;
    auto* pointer = &picture;
    SceAvcdecArrayPicture array{};
    const auto preparePicture = [&] {
        picture = {};
        picture.size = sizeof picture;
        picture.frame.pixelType = SCE_AVCDEC_PIXELFORMAT_YUV420_PACKED_RASTER;
        picture.frame.framePitch = decoder.pitch;
        picture.frame.frameWidth = component.width;
        picture.frame.frameHeight = component.height;
        // Packed NV12 is one contiguous picture. UV follows pitch*height Y
        // bytes; pPicture[1] is not a separate chroma-plane pointer.
        picture.frame.pPicture[0] = decoder.pixels;
        pointer = &picture;
        array = { 0, 1, &pointer };
    };
    preparePicture();
    ++decoder.decodeAUs;
    decoder.profile.next(VideoProfile::Decode);
    const int decodeRc = sceAvcdecDecode(&decoder.control, &au, &array);
    decoder.profile.next(VideoProfile::Count);
    const unsigned decodeOutputs = array.numOfOutput;
    int rc = decodeRc, stopRc = 0, flushRc = 0;
    unsigned stopOutputs = 0;
    bool stopped = false;
    // THPSimple requires the picture for this component before it advances.
    // Stop drains that AU without submitting a synthetic or future picture.
    if (rc >= 0 && !array.numOfOutput)
    {
        preparePicture();
        stopped = true;
        ++decoder.stopCalls;
        decoder.profile.next(VideoProfile::Stop);
        rc = stopRc = sceAvcdecDecodeStop(&decoder.control, &array);
        decoder.profile.next(VideoProfile::Count);
        stopOutputs = array.numOfOutput;
    }
    if (rc < 0 || array.numOfOutput != 1)
    {
        decoder.profile.finish(false);
        const char* reason = rc < 0 ? "decoder-error" : array.numOfOutput ? "multiple-pictures" : "zero-pictures";
        std::fprintf(stderr, "[native-video] decode failed reason=%s component=%u decode_au=%u stop_calls=%u flush_calls=%u "
                     "decode_rc=0x%08x decode_outputs=%u stop_called=%u stop_rc=0x%08x stop_outputs=%u; movie skipped\n",
                     reason, decoder.components, decoder.decodeAUs, decoder.stopCalls, decoder.flushCalls,
                     unsigned(decodeRc), decodeOutputs, unsigned(stopped), unsigned(stopRc), stopOutputs);
        decoder.reset();
        return -1;
    }
    const auto& frame = picture.frame;
    if (frame.pixelType != SCE_AVCDEC_PIXELFORMAT_YUV420_PACKED_RASTER ||
        frame.framePitch != decoder.pitch || frame.pPicture[0] != decoder.pixels ||
        frame.pPicture[1] || frame.frameWidth != component.width || frame.frameHeight != component.height ||
        frame.horizontalSize != component.width ||
        frame.verticalSize != component.height || frame.frameCropLeftOffset ||
        frame.frameCropRightOffset || frame.frameCropTopOffset || frame.frameCropBottomOffset)
    {
        decoder.fail("output geometry", -1);
        return -1;
    }
    if (stopped)
    {
        // Every WAVC component carries SPS/PPS and an independent IDR, so no
        // references are needed across this reset. The output raster belongs
        // to us. Reset before publishing, so a Flush error leaves THP planes
        // untouched and cannot expose a prior or partially accepted picture.
        ++decoder.flushCalls;
        decoder.profile.next(VideoProfile::Flush);
        flushRc = sceAvcdecDecodeFlush(&decoder.control);
        decoder.profile.next(VideoProfile::Count);
        if (flushRc < 0)
        {
            decoder.fail("decode flush", flushRc);
            return -1;
        }
    }
    decoder.profile.next(VideoProfile::Tile);
    const uint8_t* tileSource = decoder.pixels;
    if (decoder.stagingPixels)
    {
        const size_t bytes = yBytes + yBytes / 2;
        if (bytes != decoder.stagingBytes)
        {
            decoder.fail("staging extent", -EINVAL);
            return -1;
        }
        // Include both the bulk copy and the unchanged tiler in the Tile phase.
        std::memcpy(decoder.stagingPixels, decoder.pixels, bytes);
        tileSource = decoder.stagingPixels;
    }
    if (!port::tile_native_nv12(tileSource, yBytes, tileSource + yBytes, yBytes / 2, decoder.pitch,
                                component.width, component.height, y, u, v))
    {
        decoder.fail("output layout", -EINVAL);
        return -1;
    }
    decoder.profile.finish(true);
    if (++decoder.pictures == 1 || !(decoder.pictures % 300))
        std::fprintf(stderr, "[native-video] hardware pictures=%u size=%ux%u components=%u decode_au=%u stop_calls=%u flush_calls=%u "
                     "sps=%u sps_patched=%u decode_rc=0x%08x decode_outputs=%u stop_called=%u stop_rc=0x%08x stop_outputs=%u flush_rc=0x%08x\n",
                     decoder.pictures, component.width, component.height, decoder.components, decoder.decodeAUs,
                     decoder.stopCalls, decoder.flushCalls, sps.sps, sps.patched, unsigned(decodeRc), decodeOutputs,
                     unsigned(stopped), unsigned(stopRc), stopOutputs, unsigned(flushRc));
    return 0;
}
nlFile* PortOpenNativeMovie(const char* source)
{
    if (!PortNativeVideoEnabled() || !source)
        return nullptr;
    while (*source == '/' || *source == '\\')
        ++source;
    if (!*source || std::strstr(source, "..") || std::strchr(source, ':') ||
        std::strchr(source, '\\'))
        return nullptr;
    const char* path = std::getenv("STRIKERS_ASSET_ARCHIVE");
    if (!path || !*path)
        return nullptr;
    char error[256];
    auto* archive = port_asset_archive_open(path, error, sizeof error);
    if (!archive)
        return nullptr;
    std::string name = "files/";
    name += source;
    const int entry = port_asset_archive_find(archive, name.c_str());
    const uint64_t size = entry < 0 ? 0 : port_asset_archive_size(archive, entry);
    uint8_t header[80];
    bool valid =
        size >= 80 && size <= 256 * 1024 * 1024 && !(size % 32) &&
        port_asset_archive_read(archive, entry, header, sizeof header, 0) == sizeof header &&
        !std::memcmp(header, "THP\0", 4) && port::video_word(header + 4) == 0x11000 &&
        port::video_word(header + 20) > 0 && port::video_word(header + 40) < size &&
        port::video_word(header + 24) <= size - port::video_word(header + 40);
    // Verify the first independently decodable component before selecting this file.
    std::vector<uint8_t> first;
    if (valid)
    {
        const unsigned frameAt = port::video_word(header + 40),
                       frameSize = port::video_word(header + 24);
        uint8_t frame[16];
        valid = frameSize >= 16 && port_asset_archive_read(archive, entry, frame, sizeof frame,
                                                           frameAt) == sizeof frame;
        const unsigned videoSize = valid ? port::video_word(frame + 8) : 0;
        uint8_t info[28];
        const unsigned infoAt = port::video_word(header + 32);
        valid = valid && infoAt >= 48 && infoAt <= frameAt && frameAt - infoAt >= 28 &&
                port_asset_archive_read(archive, entry, info, sizeof info, infoAt) == sizeof info;
        const unsigned components = valid ? port::video_word(info) : 0;
        valid = valid && components >= 1 && components <= 2 && info[4] == 0 &&
                (components == 1 || info[5] == 1) && videoSize <= 1024 * 1024 && videoSize >= 24 &&
                videoSize <= frameSize - 8 - 4 * components;
        if (valid)
        {
            first.resize(videoSize);
            port::NativeVideoComponent parsed;
            valid = port_asset_archive_read(archive, entry, first.data(), first.size(),
                                            frameAt + 8 + 4 * components) == long(first.size()) &&
                    port::parse_native_video(first.data(), first.size(), parsed) &&
                    parsed.width == port::video_word(info + 20) &&
                    parsed.height == port::video_word(info + 24);
        }
    }
    if (!valid)
    {
        port_asset_archive_close(archive);
        return nullptr;
    }
    std::fprintf(stderr, "[native-video] selected %s bytes=%u\n", source, unsigned(size));
    return new NativeMovieFile(archive, entry, size);
}
#else
extern "C" int PortNativeVideoEnabled() { return 0; }
extern "C" void PortNativeVideoReset() {}
extern "C" int PortNativeVideoDecode(const void*, size_t, void*, void*, void*) { return -1; }
nlFile* PortOpenNativeMovie(const char*) { return nullptr; }
#endif
