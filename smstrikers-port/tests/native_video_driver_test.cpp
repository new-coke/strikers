#include "port/native_video.h"
#include "port/asset_archive.h"
#include "port/host.h"
#include "NL/nlFileGC.h"
#include <psp2/videodec.h>
#include <psp2/sysmodule.h>
#include <psp2/kernel/sysmem.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace mock {
constexpr unsigned width = 512, height = 416, mib = 1024 * 1024, guard = 32;
constexpr std::array<std::array<unsigned, 2>, 8> shapes{{
    {16, 16}, {16, 768}, {704, 16}, {64, 64}, {320, 240}, {640, 480}, {704, 768}, {512, 416}}};
unsigned expectedWidth = width, expectedHeight = height, pitch = width, rows = height;
void expect_geometry(unsigned w, unsigned h)
{
    expectedWidth = w; expectedHeight = h;
    pitch = std::max(64u, w); rows = std::max(64u, h);
}
enum class Op { Decode, Stop, Flush };
enum class InitFailure { None, Query, ReferenceAlloc, ReferenceBase, RasterAlloc, RasterBase,
                         StagingAlloc, StagingBase, StagingNullBase, Create };
enum class Damage { None, Format, Pitch, Width, Height, Horizontal, Vertical,
                    CropLeft, CropRight, CropTop, CropBottom, Pointer0, Pointer1 };
struct Reply {
    Op op;
    int rc = 0;
    unsigned outputs = 0;
    unsigned tag = 0;
    Damage damage = Damage::None;
};
std::vector<Reply> script;
size_t next = 0;
std::vector<uint8_t> expectedAU;
std::array<std::vector<uint8_t>, 3> memory;
std::array<bool, 3> allocated{};
std::array<size_t, 3> memoryBytes{};
std::array<unsigned, 3> allocationAttempts{}, allocations{}, releases{}, baseQueries{};
bool module = false, library = false, created = false, stopped = false;
unsigned pendingTag = 0;
unsigned loads = 0, inits = 0, creates = 0, deletes = 0, terms = 0, unloads = 0;
unsigned decodeCalls = 0, stopCalls = 0, flushCalls = 0;
uint64_t clockNs = 0;
unsigned clockCalls = 0;
std::function<void()> clockObserver;
bool failLibrary = false;
InitFailure failInit = InitFailure::None;
unsigned queryBytes = 4 * mib + 1234;
FILE* diagnostics = nullptr;

uint8_t* block_base(unsigned block) { return memory[block].data() + guard; }
bool memory_guarded(unsigned block)
{
    const auto& bytes = memory[block];
    return bytes.size() == memoryBytes[block] + 2 * guard &&
        std::all_of(bytes.begin(), bytes.begin() + guard, [](uint8_t b) { return b == 0xa5; }) &&
        std::all_of(bytes.end() - guard, bytes.end(), [](uint8_t b) { return b == 0xa5; });
}

std::vector<std::string> profile_reports()
{
    assert(diagnostics && !std::fflush(diagnostics));
    const auto bytes = std::ftell(diagnostics);
    assert(bytes >= 0);
    std::rewind(diagnostics);
    std::string text(size_t(bytes), '\0');
    assert(std::fread(text.data(), 1, text.size(), diagnostics) == text.size());
    assert(!std::fseek(diagnostics, 0, SEEK_END));
    std::vector<std::string> rows;
    size_t at = 0;
    while ((at = text.find("[native-video-profile] ", at)) != std::string::npos)
    {
        const auto end = text.find('\n', at);
        assert(end != std::string::npos);
        rows.push_back(text.substr(at, end - at));
        at = end + 1;
    }
    return rows;
}
void contains(const std::string& row, const char* fields)
{
    assert(row.find(fields) != std::string::npos);
}

Reply reply(Op op)
{
    assert(created && next < script.size() && script[next].op == op);
    return script[next++];
}
void schedule(std::initializer_list<Reply> replies)
{
    assert(next == script.size());
    script = replies;
    next = 0;
}
uint8_t y_value(unsigned tag, unsigned x, unsigned y) { return uint8_t(tag + x * 3 + y * 5); }
uint8_t u_value(unsigned tag, unsigned x, unsigned y) { return uint8_t(tag * 7 + x * 11 + y * 13); }
uint8_t v_value(unsigned tag, unsigned x, unsigned y) { return uint8_t(tag * 17 + x * 19 + y * 23); }

SceAvcdecPicture& output(SceAvcdecArrayPicture* array, const Reply& r, unsigned tag)
{
    assert(array && !array->numOfOutput && array->numOfElm == 1 && array->pPicture && array->pPicture[0]);
    auto& p = *array->pPicture[0];
    auto& f = p.frame;
    assert(p.size == sizeof p && f.pixelType == SCE_AVCDEC_PIXELFORMAT_YUV420_PACKED_RASTER);
    assert(f.framePitch == pitch && f.frameWidth == expectedWidth && f.frameHeight == expectedHeight);
    assert(f.pPicture[0] == block_base(1) && !f.pPicture[1]);
    array->numOfOutput = r.outputs;
    if (r.outputs)
    {
        // One packed picture: Y followed by interleaved UV in the same buffer.
        // The second pPicture entry is not a chroma-plane pointer.
        auto* pixels = block_base(1);
        std::fill_n(pixels, memoryBytes[1], 0xcd);
        assert(size_t(pitch) * expectedHeight * 3 / 2 <= memoryBytes[1]);
        for (unsigned y = 0; y < expectedHeight; ++y)
            for (unsigned x = 0; x < expectedWidth; ++x)
                pixels[y * pitch + x] = y_value(tag, x, y);
        auto* uv = pixels + pitch * expectedHeight;
        for (unsigned y = 0; y < expectedHeight / 2; ++y)
            for (unsigned x = 0; x < expectedWidth / 2; ++x)
            {
                uv[y * pitch + x * 2] = u_value(tag, x, y);
                uv[y * pitch + x * 2 + 1] = v_value(tag, x, y);
            }
        f.horizontalSize = expectedWidth;
        f.verticalSize = expectedHeight;
    }
    switch (r.damage)
    {
    case Damage::None: break;
    case Damage::Format: ++f.pixelType; break;
    case Damage::Pitch: ++f.framePitch; break;
    case Damage::Width: ++f.frameWidth; break;
    case Damage::Height: ++f.frameHeight; break;
    case Damage::Horizontal: ++f.horizontalSize; break;
    case Damage::Vertical: ++f.verticalSize; break;
    case Damage::CropLeft: ++f.frameCropLeftOffset; break;
    case Damage::CropRight: ++f.frameCropRightOffset; break;
    case Damage::CropTop: ++f.frameCropTopOffset; break;
    case Damage::CropBottom: ++f.frameCropBottomOffset; break;
    case Damage::Pointer0: f.pPicture[0] = nullptr; break;
    case Damage::Pointer1: f.pPicture[1] = block_base(1) + pitch * expectedHeight; break;
    }
    return p;
}
void be32(std::vector<uint8_t>& out, unsigned value)
{
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(uint8_t(value >> shift));
}
std::vector<uint8_t> geometry_sps(unsigned w, unsigned h)
{
    // Minimal Baseline SPS, already using one reference: only the coded
    // macroblock geometry varies. The production SPS preparer must preserve it.
    std::vector<unsigned> bits;
    const auto value = [&](unsigned v, unsigned n) {
        while (n) bits.push_back((v >> --n) & 1);
    };
    const auto ue = [&](unsigned v) {
        const unsigned code = v + 1;
        unsigned n = 0;
        for (unsigned remaining = code; remaining; remaining >>= 1) ++n;
        value(0, n - 1); value(code, n);
    };
    value(66, 8); value(0, 8); value(32, 8);
    ue(0); ue(0); ue(2); ue(1); value(0, 1);
    ue(w / 16 - 1); ue(h / 16 - 1);
    value(1, 1); value(1, 1); value(0, 1); value(0, 1);
    value(1, 1);
    while (bits.size() % 8) value(0, 1);
    std::vector<uint8_t> nal{0x67};
    unsigned zeros = 0;
    for (size_t i = 0; i < bits.size(); i += 8)
    {
        uint8_t byte = 0;
        for (unsigned j = 0; j < 8; ++j) byte = uint8_t(byte * 2 + bits[i + j]);
        if (zeros == 2 && byte <= 3) { nal.push_back(3); zeros = 0; }
        nal.push_back(byte);
        zeros = byte ? 0 : zeros + 1;
    }
    return nal;
}
std::vector<uint8_t> component(unsigned tag, unsigned w = width, unsigned h = height)
{
    // Actual installed-pack SPS and its independently verified rewrite. The
    // mock asserts every submitted byte, including unchanged PPS/IDR payloads.
    constexpr uint8_t original[] = {0x67, 0x42, 0xc0, 0x20, 0xdc, 0x08, 0x03, 0x5a,
        0x6e, 0x04, 0x04, 0x0a, 0x80, 0, 0, 0x32, 0, 0, 0x0b, 0xb5, 0x47, 0x8c, 0x19, 0xc0};
    constexpr uint8_t patched[] = {0x67, 0x42, 0xc0, 0x20, 0xda, 0x02, 0, 0xd6,
        0x9b, 0x81, 0x01, 0x02, 0xa0, 0, 0, 0x0c, 0x80, 0, 0x02, 0xed, 0x51, 0xe3, 0x06, 0x54};
    std::vector<uint8_t> au{0, 0, 0, 1, 9, 0xf0, 0, 0, 0, 1};
    expectedAU = au;
    if (w == width && h == height)
    {
        au.insert(au.end(), std::begin(original), std::end(original));
        expectedAU.insert(expectedAU.end(), std::begin(patched), std::end(patched));
    }
    else
    {
        const auto sps = geometry_sps(w, h);
        au.insert(au.end(), sps.begin(), sps.end());
        expectedAU.insert(expectedAU.end(), sps.begin(), sps.end());
    }
    const std::vector<uint8_t> payload{0, 0, 0, 1, 0x68, 0xce, 0x02, 0x32, 0xc8,
                                      0, 0, 1, 0x65, 0x88, 0x84, uint8_t(tag), 0x80};
    au.insert(au.end(), payload.begin(), payload.end());
    expectedAU.insert(expectedAU.end(), payload.begin(), payload.end());
    std::vector<uint8_t> record{'W', 'A', 'V', 'C'};
    for (unsigned word : {1u, w, h, unsigned(au.size()), 1u}) be32(record, word);
    record.insert(record.end(), au.begin(), au.end());
    while (record.size() % 4) record.push_back(0);
    return record;
}
struct Planes {
    unsigned w, h;
    std::vector<uint8_t> y, u, v;
    explicit Planes(unsigned frameWidth = width, unsigned frameHeight = height)
        : w(frameWidth), h(frameHeight), y(w * h + guard * 2, 0x5a),
          u(w * h / 4 + guard * 2, 0x5a), v(w * h / 4 + guard * 2, 0x5a)
    {
        for (auto* p : {&y, &u, &v})
        {
            std::fill_n(p->begin(), guard, 0xa5);
            std::fill_n(p->end() - guard, guard, 0xa5);
        }
    }
    uint8_t* Y() { return y.data() + guard; }
    uint8_t* U() { return u.data() + guard; }
    uint8_t* V() { return v.data() + guard; }
    bool guarded() const
    {
        for (const auto* p : {&y, &u, &v})
            if (!std::all_of(p->begin(), p->begin() + guard, [](uint8_t b) { return b == 0xa5; }) ||
                !std::all_of(p->end() - guard, p->end(), [](uint8_t b) { return b == 0xa5; })) return false;
        return true;
    }
    bool untouched() const
    {
        if (!guarded()) return false;
        for (const auto* p : {&y, &u, &v})
            if (!std::all_of(p->begin() + guard, p->end() - guard, [](uint8_t b) { return b == 0x5a; })) return false;
        return true;
    }
    void check(unsigned tag) const
    {
        assert(guarded());
        auto verify = [tag](const auto& bytes, unsigned w, unsigned h, auto value) {
            for (unsigned row = 0; row < h; ++row)
                for (unsigned x = 0; x < w; ++x)
                {
                    const size_t index = ((row / 4) * (w / 8) + x / 8) * 32 + (row % 4) * 8 + x % 8;
                    assert(bytes[guard + index] == value(tag, x, row));
                }
        };
        verify(y, w, h, y_value);
        verify(u, w / 2, h / 2, u_value);
        verify(v, w / 2, h / 2, v_value);
    }
};
int decode(unsigned tag, Planes& planes)
{
    expect_geometry(planes.w, planes.h);
    const auto record = component(tag, planes.w, planes.h);
    const int rc = PortNativeVideoDecode(record.data(), record.size(), planes.Y(), planes.U(), planes.V());
    assert(next == script.size());
    for (unsigned block = 0; block != memory.size(); ++block)
        if (allocated[block]) assert(memory_guarded(block));
    return rc;
}
void empty()
{
    assert(!created && !library && !module && !allocated[0] && !allocated[1] && !allocated[2]);
    assert(loads == unloads && inits == terms && creates == deletes);
    assert(allocations == releases);
}
} // namespace mock

extern "C" {
unsigned long long port_monotonic_ns()
{
    ++mock::clockCalls;
    if (mock::clockObserver) mock::clockObserver();
    return mock::clockNs += 10;
}
int sceSysmoduleIsLoaded(SceSysmoduleModuleId id)
{
    assert(id == SCE_SYSMODULE_AVCDEC);
    return mock::module ? 0 : -1;
}
int sceSysmoduleLoadModule(SceSysmoduleModuleId id)
{
    assert(id == SCE_SYSMODULE_AVCDEC && !mock::module);
    mock::module = true; ++mock::loads; return 0;
}
int sceSysmoduleUnloadModule(SceSysmoduleModuleId id)
{
    assert(id == SCE_SYSMODULE_AVCDEC && mock::module && !mock::library);
    mock::module = false; ++mock::unloads; return 0;
}
int sceVideodecInitLibrary(SceVideodecType type, const SceVideodecQueryInitInfoHwAvcdec* q)
{
    assert(type == SCE_VIDEODEC_TYPE_HW_AVCDEC && q && q->size == sizeof *q);
    assert(q->horizontal == mock::pitch && q->vertical == mock::rows && q->numOfRefFrames == 1 && q->numOfStreams == 1);
    assert(mock::module && !mock::library);
    if (mock::failLibrary) { mock::clockNs += 700; return -77; }
    mock::library = true; ++mock::inits; return 0;
}
int sceVideodecTermLibrary(SceVideodecType type)
{
    assert(type == SCE_VIDEODEC_TYPE_HW_AVCDEC && mock::library && !mock::created &&
           !mock::allocated[0] && !mock::allocated[1] && !mock::allocated[2]);
    mock::library = false; ++mock::terms; return 0;
}
int sceAvcdecQueryDecoderMemSize(SceVideodecType type, const SceAvcdecQueryDecoderInfo* q, SceAvcdecDecoderInfo* info)
{
    assert(type == SCE_VIDEODEC_TYPE_HW_AVCDEC && q && info && !info->frameMemSize);
    assert(q->horizontal == mock::pitch && q->vertical == mock::rows && q->numOfRefFrames == 1);
    if (mock::failInit == mock::InitFailure::Query) return -81;
    info->frameMemSize = mock::queryBytes; return 0;
}
SceUID sceKernelAllocMemBlock(const char* name, SceKernelMemBlockType type, SceSize bytes, SceKernelAllocMemBlockOpt* opt)
{
    const unsigned block = !std::strcmp(name, "strikers_avc_reference") ? 0 :
                           !std::strcmp(name, "strikers_avc_nv12") ? 1 : 2;
    assert(block != 2 || !std::strcmp(name, "strikers_avc_staging"));
    assert(type == (block == 2 ? SCE_KERNEL_MEMBLOCK_TYPE_USER_RW :
                                SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW));
    assert(!opt && !mock::allocated[block]);
    const size_t nv12Bytes = size_t(mock::pitch) * mock::expectedHeight * 3 / 2;
    const size_t expectedBytes = block == 2 ? (nv12Bytes + 4095) / 4096 * 4096 :
        block == 1 ? mock::mib : (mock::queryBytes + mock::mib - 1) / mock::mib * mock::mib;
    assert(bytes == expectedBytes);
    ++mock::allocationAttempts[block];
    const auto failure = block == 2 ? mock::InitFailure::StagingAlloc :
                         block == 1 ? mock::InitFailure::RasterAlloc : mock::InitFailure::ReferenceAlloc;
    if (mock::failInit == failure) return -82;
    mock::memoryBytes[block] = bytes;
    mock::memory[block].assign(size_t(bytes) + mock::guard * 2, 0xa5);
    std::fill_n(mock::block_base(block), bytes, 0x5a);
    ++mock::allocations[block];
    mock::allocated[block] = true; return block + 1;
}
int sceKernelGetMemBlockBase(SceUID id, void** base)
{
    assert(id >= 1 && id <= 3 && mock::allocated[id - 1] && base);
    ++mock::baseQueries[id - 1];
    const auto failure = id == 3 ? mock::InitFailure::StagingBase :
                         id == 2 ? mock::InitFailure::RasterBase : mock::InitFailure::ReferenceBase;
    if (mock::failInit == failure) return -83;
    if (id == 3 && mock::failInit == mock::InitFailure::StagingNullBase) { *base = nullptr; return 0; }
    *base = mock::block_base(id - 1); return 0;
}
int sceKernelFreeMemBlock(SceUID id)
{
    assert(id >= 1 && id <= 3 && mock::allocated[id - 1] && !mock::created);
    assert(mock::memory_guarded(id - 1));
    ++mock::releases[id - 1];
    mock::allocated[id - 1] = false; return 0;
}
int sceAvcdecCreateDecoder(SceVideodecType type, SceAvcdecCtrl* ctrl, const SceAvcdecQueryDecoderInfo* q)
{
    assert(type == SCE_VIDEODEC_TYPE_HW_AVCDEC && mock::library && !mock::created && q->numOfRefFrames == 1);
    assert(q->horizontal == mock::pitch && q->vertical == mock::rows);
    assert(ctrl && !ctrl->handle && ctrl->frameBuf.size == mock::memoryBytes[0] && ctrl->frameBuf.pBuf == mock::block_base(0));
    if (mock::failInit == mock::InitFailure::Create) return -84;
    mock::clockNs += 1000;
    ctrl->handle = 123; mock::created = true; mock::stopped = false; ++mock::creates; return 0;
}
int sceAvcdecDeleteDecoder(SceAvcdecCtrl* ctrl)
{
    assert(ctrl && ctrl->handle == 123 && mock::created && mock::allocated[0] && mock::allocated[1]);
    ctrl->handle = 0; mock::created = false; mock::pendingTag = 0; mock::stopped = false; ++mock::deletes; return 0;
}
int sceAvcdecDecode(const SceAvcdecCtrl* ctrl, const SceAvcdecAu* au, SceAvcdecArrayPicture* pictures)
{
    assert(ctrl && ctrl->handle == 123 && au && !mock::stopped);
    assert(au->pts.upper == UINT32_MAX && au->pts.lower == UINT32_MAX &&
           au->dts.upper == UINT32_MAX && au->dts.lower == UINT32_MAX);
    assert(au->es.size == mock::expectedAU.size());
    assert(!std::memcmp(au->es.pBuf, mock::expectedAU.data(), au->es.size));
    const auto* padding = static_cast<const uint8_t*>(au->es.pBuf) + au->es.size;
    assert(std::all_of(padding, padding + 64, [](uint8_t b) { return !b; }));
    const auto r = mock::reply(mock::Op::Decode);
    mock::clockNs += 100;
    mock::pendingTag = r.tag;
    mock::output(pictures, r, r.tag);
    if (r.rc >= 0 && !r.outputs)
    {
        // These are in/out descriptors. A zero-picture response must not make
        // the next Stop reuse a capacity or a pointer modified by the SDK.
        pictures->pPicture[0]->frame.framePitch = 1;
        pictures->pPicture[0] = nullptr;
        pictures->numOfElm = 0;
    }
    ++mock::decodeCalls; return r.rc;
}
int sceAvcdecDecodeStop(SceAvcdecCtrl* ctrl, SceAvcdecArrayPicture* pictures)
{
    assert(ctrl && ctrl->handle == 123 && !mock::stopped && mock::pendingTag);
    const auto r = mock::reply(mock::Op::Stop);
    mock::clockNs += 200;
    mock::output(pictures, r, mock::pendingTag);
    mock::stopped = true; ++mock::stopCalls; return r.rc;
}
int sceAvcdecDecodeFlush(SceAvcdecCtrl* ctrl)
{
    assert(ctrl && ctrl->handle == 123 && mock::stopped);
    const auto r = mock::reply(mock::Op::Flush);
    mock::clockNs += 300;
    if (!r.rc) { mock::stopped = false; mock::pendingTag = 0; }
    ++mock::flushCalls; return r.rc;
}
PortAssetArchive* port_asset_archive_open(const char*, char*, size_t) { assert(false); return nullptr; }
void port_asset_archive_close(PortAssetArchive*) { assert(false); }
int port_asset_archive_find(const PortAssetArchive*, const char*) { assert(false); return -1; }
uint64_t port_asset_archive_size(const PortAssetArchive*, unsigned) { assert(false); return 0; }
long port_asset_archive_read(PortAssetArchive*, unsigned, void*, size_t, uint64_t) { assert(false); return -1; }
}
nlFile::nlFile() = default;
nlFile::~nlFile() = default;
void GCFile::Read(void*, unsigned) { assert(false); }

// Keep the host prelude/ABI while compiling the complete Vita driver body.
// SDK types and function declarations above come from the installed VitaSDK.
#define PORT_VITA 1
#pragma push_macro("stderr")
#undef stderr
#define stderr mock::diagnostics
#include "../src/platform/native_video.cpp"
#pragma pop_macro("stderr")

void check_staging_pixels()
{
    using namespace mock;
    if (!decoder.stagingPixels)
    {
        assert(decoder.staging == -1 && !decoder.stagingBytes && !allocated[2]);
        return;
    }
    const size_t bytes = size_t(pitch) * expectedHeight * 3 / 2;
    assert(decoder.staging == 3 && allocated[2] && decoder.stagingPixels == block_base(2));
    assert(decoder.stagingBytes == bytes && memoryBytes[2] == (bytes + 4095) / 4096 * 4096);
    // Include horizontal stride padding, but never copy SDK capacity rows or
    // the remainder of the fixed-size non-cacheable hardware allocation.
    assert(!std::memcmp(block_base(1), block_base(2), bytes));
    assert(std::all_of(block_base(2) + bytes, block_base(2) + memoryBytes[2], [](uint8_t b) { return b == 0x5a; }));
    assert(memory_guarded(1) && memory_guarded(2));
}

void check_dimensions()
{
    using namespace mock;
    // Every admitted shape gets the same byte-exact planar picture, including
    // padded SDK rows/strides below 64 and the largest accepted envelope.
    for (const auto shape : shapes)
    {
        const auto initialInits = inits, initialCreates = creates;
        Planes planes(shape[0], shape[1]);
        schedule({{Op::Decode, 0, 0, 83}, {Op::Stop, 0, 1}, {Op::Flush}});
        assert(!decode(83, planes)); planes.check(83);
        check_staging_pixels();
        assert(inits == initialInits + 1 && creates == initialCreates + 1);
        schedule({{Op::Decode, 0, 1, 89}});
        assert(!decode(89, planes)); planes.check(89);
        check_staging_pixels();
        assert(inits == initialInits + 1 && creates == initialCreates + 1);
        PortNativeVideoReset(); empty();
    }

    // THP owns planes sized for the current movie. A different valid SPS/AU
    // cannot silently resize the decoder and write a larger frame into them.
    for (const auto shape : {std::array<unsigned, 4>{512, 416, 640, 416},
                             {512, 416, 512, 480}, {16, 16, 32, 16}})
    {
        Planes original(shape[0], shape[1]);
        schedule({{Op::Decode, 0, 1, 97}});
        assert(!decode(97, original)); original.check(97);
        const auto previousDecodes = decodeCalls, previousCreates = creates;
        Planes unchanged(shape[0], shape[1]);
        auto changed = component(101, shape[2], shape[3]);
        schedule({});
        assert(PortNativeVideoDecode(changed.data(), changed.size(), unchanged.Y(), unchanged.U(), unchanged.V()) < 0);
        assert(unchanged.untouched() && decodeCalls == previousDecodes && creates == previousCreates);
        empty();
    }
    // Reset permits the next movie to use that larger geometry.
    PortNativeVideoReset(); empty();
    Planes larger(640, 480);
    schedule({{Op::Decode, 0, 0, 103}, {Op::Stop, 0, 1}, {Op::Flush}});
    assert(!decode(103, larger)); larger.check(103);
    PortNativeVideoReset(); empty();

    const auto rewrite = [](std::vector<uint8_t>& data, size_t at, unsigned word) {
        for (unsigned byte = 0; byte < 4; ++byte) data[at + byte] = uint8_t(word >> (24 - byte * 8));
    };
    const auto baselineLoads = loads;
    for (const auto& field : {std::array<unsigned, 2>{8, 0}, {8, 15}, {8, 17}, {8, 720}, {8, UINT32_MAX},
                              {12, 0}, {12, 15}, {12, 17}, {12, 784}, {12, UINT32_MAX}, {16, UINT32_MAX},
                              {8, 640}}) // Last case is an admitted envelope whose SPS disagrees.
    {
        auto bad = component(107);
        rewrite(bad, field[0], field[1]);
        Planes rejected;
        assert(PortNativeVideoDecode(bad.data(), bad.size(), rejected.Y(), rejected.U(), rejected.V()) < 0);
        assert(rejected.untouched() && loads == baselineLoads);
        empty();
    }

    // Query/allocation failures retain the original teardown order and never
    // reach Decode or expose pixels from a previous movie's raster.
    for (InitFailure failure : {InitFailure::Query, InitFailure::ReferenceAlloc, InitFailure::ReferenceBase,
                                InitFailure::RasterAlloc, InitFailure::RasterBase, InitFailure::Create})
    {
        failInit = failure;
        Planes rejected(320, 240);
        schedule({});
        assert(decode(109, rejected) < 0 && rejected.untouched());
        empty();
    }
    failInit = InitFailure::None;
    const unsigned validQueryBytes = queryBytes;
    for (unsigned invalid : {0u, 32 * mib + 1, UINT32_MAX})
    {
        queryBytes = invalid;
        Planes rejected;
        schedule({});
        assert(decode(113, rejected) < 0 && rejected.untouched());
        empty();
    }
    queryBytes = validQueryBytes;
    assert(!clockCalls && profile_reports().empty());
}

void check_staging_contracts(bool selected)
{
    using namespace mock;
    empty();
    assert(!clockCalls && profile_reports().empty());
    for (const auto shape : shapes)
    {
        unsetenv("STRIKERS_VITA_NATIVE_VIDEO_STAGING");
        Planes direct(shape[0], shape[1]), staged(shape[0], shape[1]);
        schedule({{Op::Decode, 0, 1, 79}});
        assert(!decode(79, direct)); direct.check(79);
        assert(!decoder.stagingPixels);
        PortNativeVideoReset(); empty();
        setenv("STRIKERS_VITA_NATIVE_VIDEO_STAGING", "1", 1);
        const auto attempts = allocationAttempts[2];
        schedule({{Op::Decode, 0, 0, 79}, {Op::Stop, 0, 1}, {Op::Flush}});
        assert(!decode(79, staged)); staged.check(79); check_staging_pixels();
        assert(direct.y == staged.y && direct.u == staged.u && direct.v == staged.v);
        schedule({{Op::Decode, 0, 1, 81}});
        assert(!decode(81, staged)); staged.check(81); check_staging_pixels();
        assert(allocationAttempts[2] == attempts + 1);
        PortNativeVideoReset(); empty();
    }
    // Only the exact opt-in is accepted, and either decision is retained until
    // the movie is reset. Changing an environment string never reallocates a
    // live decoder or makes a failed optional allocation retry every frame.
    for (const char* flag : {static_cast<const char*>(nullptr), "0", "01", "true"})
    {
        if (flag) setenv("STRIKERS_VITA_NATIVE_VIDEO_STAGING", flag, 1);
        else unsetenv("STRIKERS_VITA_NATIVE_VIDEO_STAGING");
        const auto attempts = allocationAttempts[2];
        Planes planes(16, 16);
        schedule({{Op::Decode, 0, 1, 117}});
        assert(!decode(117, planes)); planes.check(117);
        assert(!decoder.stagingPixels && allocationAttempts[2] == attempts);
        setenv("STRIKERS_VITA_NATIVE_VIDEO_STAGING", "1", 1);
        schedule({{Op::Decode, 0, 1, 119}});
        assert(!decode(119, planes)); planes.check(119);
        assert(!decoder.stagingPixels && allocationAttempts[2] == attempts);
        PortNativeVideoReset(); empty();
        schedule({{Op::Decode, 0, 1, 121}});
        assert(!decode(121, planes)); planes.check(121); check_staging_pixels();
        assert(allocationAttempts[2] == attempts + 1 && decoder.stagingBytes == 1536 && memoryBytes[2] == 4096);
        const auto* address = decoder.stagingPixels;
        setenv("STRIKERS_VITA_NATIVE_VIDEO_STAGING", "0", 1);
        schedule({{Op::Decode, 0, 1, 123}});
        assert(!decode(123, planes)); planes.check(123); check_staging_pixels();
        assert(decoder.stagingPixels == address && allocationAttempts[2] == attempts + 1);
        PortNativeVideoReset(); empty();
        assert(!decoder.stagingPixels && decoder.staging == -1 && !decoder.stagingBytes);
    }

    setenv("STRIKERS_VITA_NATIVE_VIDEO_STAGING", "1", 1);
    for (InitFailure failure : {InitFailure::StagingAlloc, InitFailure::StagingBase, InitFailure::StagingNullBase})
    {
        failInit = failure;
        const auto attempts = allocationAttempts[2], queries = baseQueries[2], freed = releases[2];
        Planes fallback(704, 768);
        schedule({{Op::Decode, 0, 0, 127}, {Op::Stop, 0, 1}, {Op::Flush}});
        assert(!decode(127, fallback)); fallback.check(127);
        assert(!decoder.stagingPixels && decoder.staging == -1 && !decoder.stagingBytes && !allocated[2]);
        assert(allocationAttempts[2] == attempts + 1);
        assert(baseQueries[2] == queries + unsigned(failure != InitFailure::StagingAlloc));
        assert(releases[2] == freed + unsigned(failure != InitFailure::StagingAlloc));
        failInit = InitFailure::None;
        schedule({{Op::Decode, 0, 1, 131}});
        assert(!decode(131, fallback)); fallback.check(131);
        assert(!decoder.stagingPixels && allocationAttempts[2] == attempts + 1);
        PortNativeVideoReset(); empty();
        schedule({{Op::Decode, 0, 1, 137}});
        assert(!decode(137, fallback)); fallback.check(137); check_staging_pixels();
        assert(allocationAttempts[2] == attempts + 2 && decoder.stagingBytes == 811008);
        PortNativeVideoReset(); empty();
    }

    // Rejected records and SDK responses never copy an unaccepted picture.
    Planes previous;
    schedule({{Op::Decode, 0, 1, 139}});
    assert(!decode(139, previous)); previous.check(139); check_staging_pixels();
    const auto retained = memory[2];
    const auto originalAllocations = allocations;
    const auto originalDecodes = decodeCalls;
    auto invalid = component(141);
    invalid[0] = 'X';
    Planes unchanged;
    assert(PortNativeVideoDecode(invalid.data(), invalid.size(), unchanged.Y(), unchanged.U(), unchanged.V()) < 0);
    assert(unchanged.untouched() && allocations == originalAllocations && decodeCalls == originalDecodes);
    assert(memory[2] == retained && decoder.created);
    schedule({{Op::Decode, 0, 0, 143}, {Op::Stop, 0, 1}, {Op::Flush, -43}});
    assert(decode(143, unchanged) < 0 && unchanged.untouched()); empty();
    assert(memory[2] == retained);
    schedule({{Op::Decode, -41, 1, 149}});
    assert(decode(149, unchanged) < 0 && unchanged.untouched()); empty();
    assert(std::all_of(block_base(2), block_base(2) + memoryBytes[2], [](uint8_t b) { return b == 0x5a; }));

    // An independent scalar coordinate oracle covers every byte value in all
    // planes at the real movie geometry; each new picture reuses one allocation.
    const auto attempts = allocationAttempts[2];
    Planes staged;
    for (unsigned tag = 0; tag != 256; ++tag)
    {
        schedule({{Op::Decode, 0, 1, tag}});
        assert(!decode(tag, staged)); staged.check(tag); check_staging_pixels();
        assert(allocationAttempts[2] == attempts + 1 && decoder.stagingBytes == 319488);
    }
    PortNativeVideoReset(); empty();
    unsetenv("STRIKERS_VITA_NATIVE_VIDEO_STAGING");
    Planes direct;
    schedule({{Op::Decode, 0, 1, 255}});
    assert(!decode(255, direct)); direct.check(255);
    assert(direct.y == staged.y && direct.u == staged.u && direct.v == staged.v);
    assert(allocationAttempts[2] == attempts + 1);
    PortNativeVideoReset(); empty();
    if (selected) setenv("STRIKERS_VITA_NATIVE_VIDEO_STAGING", "1", 1);
    assert(!clockCalls && profile_reports().empty());
}

void check_profile()
{
    using namespace mock;
    assert(!clockCalls && profile_reports().empty());
    setenv("STRIKERS_VITAGL_PACKET_PROFILE", "1", 1);
    Planes planes;
    schedule({{Op::Decode, 0, 1, 41}});
    assert(!decode(41, planes));
    assert(clockCalls == 7 && decoder.profile.total.calls == 1 && decoder.profile.total.ns == 1160);
    assert(decoder.profile.children[VideoProfile::Prepare].ns == 10);
    assert(decoder.profile.children[VideoProfile::Init].ns == 1010);
    assert(decoder.profile.children[VideoProfile::Decode].ns == 110);
    assert(decoder.profile.children[VideoProfile::Tile].ns == 10);
    assert(!decoder.profile.children[VideoProfile::Stop].calls && !decoder.profile.children[VideoProfile::Flush].calls);
    assert(profile_reports().empty());

    // The flag is cached until the decoder is reset, including mid-movie changes.
    setenv("STRIKERS_VITAGL_PACKET_PROFILE", "0", 1);
    schedule({{Op::Decode, 0, 1, 43}});
    assert(!decode(43, planes));
    schedule({{Op::Decode, 0, 0, 47}, {Op::Stop, 0, 1}, {Op::Flush}});
    assert(!decode(47, planes));
    assert(clockCalls == 25 && decoder.profile.total.calls == 3 && decoder.profile.total.ns == 2020);
    assert(decoder.profile.children[VideoProfile::Stop].calls == 1 && decoder.profile.children[VideoProfile::Stop].ns == 210);
    assert(decoder.profile.children[VideoProfile::Flush].calls == 1 && decoder.profile.children[VideoProfile::Flush].ns == 310);
    uint64_t childNs = 0;
    for (const auto& child : decoder.profile.children) childNs += child.ns;
    assert(childNs == 1940 && childNs < decoder.profile.total.ns);
    PortNativeVideoReset(); empty();
    auto reports = profile_reports();
    assert(reports.size() == 1);
    contains(reports.back(), "reason=reset first_component=1 last_component=3 components=3 pictures=3 errors=0");
    contains(reports.back(), "total_ns=2020 total_max_ns=1160");
    contains(reports.back(), "prepare_calls=3 prepare_ns=30 prepare_max_ns=10");
    contains(reports.back(), "init_calls=3 init_ns=1030 init_max_ns=1010");
    contains(reports.back(), "decode_calls=3 decode_ns=330 decode_max_ns=110");
    contains(reports.back(), "stop_calls=1 stop_ns=210 stop_max_ns=210");
    contains(reports.back(), "flush_calls=1 flush_ns=310 flush_max_ns=310");
    contains(reports.back(), "tile_calls=3 tile_ns=30 tile_max_ns=10");

    // A new movie observes OFF, without clocks or a new diagnostic report.
    schedule({{Op::Decode, 0, 1, 53}});
    assert(!decode(53, planes));
    PortNativeVideoReset(); empty();
    assert(clockCalls == 25 && profile_reports().size() == 1);

    setenv("STRIKERS_VITAGL_PACKET_PROFILE", "1", 1);
    const auto initialClocks = clockCalls;
    for (unsigned i = 1; i <= 299; ++i)
    {
        schedule({{Op::Decode, 0, 1, 59}});
        assert(!decode(59, planes));
    }
    assert(profile_reports().size() == 1 && decoder.profile.total.calls == 299);
    schedule({{Op::Decode, 0, 1, 61}});
    assert(!decode(61, planes));
    reports = profile_reports();
    assert(reports.size() == 2 && clockCalls - initialClocks == 300 * 7);
    contains(reports.back(), "reason=interval first_component=1 last_component=300 components=300 pictures=300 errors=0");
    contains(reports.back(), "total_ns=49000 total_max_ns=1160");
    assert(!decoder.profile.total.calls && !decoder.profile.children[VideoProfile::Decode].calls);
    schedule({{Op::Decode, 0, 0, 67}, {Op::Stop, 0, 1}, {Op::Flush}});
    assert(!decode(67, planes));
    assert(profile_reports().size() == 2);
    PortNativeVideoReset(); empty();
    reports = profile_reports();
    assert(reports.size() == 3);
    contains(reports.back(), "reason=reset first_component=301 last_component=301 components=1 pictures=1 errors=0");
    contains(reports.back(), "total_ns=700 total_max_ns=700");

    // Error reports close the active phase before diagnostics and teardown.
    schedule({{Op::Decode, 0, 0, 71}, {Op::Stop, -42, 1}});
    Planes rejected;
    assert(decode(71, rejected) < 0 && rejected.untouched()); empty();
    reports = profile_reports();
    assert(reports.size() == 4 && !decoder.profile.configured);
    contains(reports.back(), "reason=error first_component=1 last_component=1 components=1 pictures=0 errors=1");
    contains(reports.back(), "total_ns=1370 total_max_ns=1370");
    contains(reports.back(), "stop_calls=1 stop_ns=210 stop_max_ns=210 flush_calls=0 flush_ns=0 flush_max_ns=0 tile_calls=0");

    failLibrary = true;
    schedule({});
    assert(decode(73, rejected) < 0 && rejected.untouched()); empty();
    failLibrary = false;
    reports = profile_reports();
    assert(reports.size() == 5);
    contains(reports.back(), "total_ns=720 total_max_ns=720");
    contains(reports.back(), "prepare_calls=1 prepare_ns=10 prepare_max_ns=10 init_calls=1 init_ns=710 init_max_ns=710 decode_calls=0");

    auto unsupported = component(79);
    assert(unsupported[34] == 0x67);
    // The component envelope accepts Baseline, then the SPS preparer rejects
    // its reserved bits. This exercises an active Prepare-phase failure.
    unsupported[36] |= 1;
    assert(PortNativeVideoDecode(unsupported.data(), unsupported.size(), rejected.Y(), rejected.U(), rejected.V()) < 0);
    assert(rejected.untouched()); empty();
    reports = profile_reports();
    assert(reports.size() == 6);
    contains(reports.back(), "total_ns=10 total_max_ns=10 prepare_calls=1 prepare_ns=10 prepare_max_ns=10 init_calls=0");
    const auto finalClocks = clockCalls;
    PortNativeVideoReset(); empty();
    assert(clockCalls == finalClocks && profile_reports().size() == 6);
    unsetenv("STRIKERS_VITAGL_PACKET_PROFILE");
}

void check_staging_profile()
{
    using namespace mock;
    empty();
    setenv("STRIKERS_VITA_NATIVE_VIDEO_STAGING", "1", 1);
    setenv("STRIKERS_VITAGL_PACKET_PROFILE", "1", 1);
    const auto initialClocks = clockCalls;
    const auto initialReports = profile_reports().size();
    Planes planes;
    bool before = false, after = false;
    clockObserver = [&] {
        const unsigned call = clockCalls - initialClocks;
        if (call == 6)
        {
            assert(decoder.profile.phase == VideoProfile::Count && planes.untouched());
            assert(decoder.stagingPixels && std::all_of(block_base(2), block_base(2) + memoryBytes[2], [](uint8_t b) { return b == 0x5a; }));
            before = true;
        }
        if (call == 7)
        {
            assert(before && decoder.profile.phase == VideoProfile::Tile);
            planes.check(151); check_staging_pixels();
            // The timing boundary encloses both completed outputs. Adding a
            // deterministic clock cost here must charge Tile, not Init/Decode.
            clockNs += 2000;
            after = true;
        }
    };
    schedule({{Op::Decode, 0, 1, 151}});
    assert(!decode(151, planes));
    clockObserver = {};
    assert(before && after && clockCalls == initialClocks + 7);
    assert(decoder.profile.children[VideoProfile::Tile].ns == 2010 && decoder.profile.total.ns == 3160);
    PortNativeVideoReset(); empty();
    assert(profile_reports().size() == initialReports + 1);
    contains(profile_reports().back(), "tile_calls=1 tile_ns=2010 tile_max_ns=2010");
    unsetenv("STRIKERS_VITAGL_PACKET_PROFILE");
    const auto afterProfileClocks = clockCalls;
    schedule({{Op::Decode, 0, 1, 157}});
    assert(!decode(157, planes)); planes.check(157); check_staging_pixels();
    PortNativeVideoReset(); empty();
    assert(clockCalls == afterProfileClocks && profile_reports().size() == initialReports + 1);
    unsetenv("STRIKERS_VITA_NATIVE_VIDEO_STAGING");
}

int main(int argc, char** argv)
{
    using namespace mock;
    assert(argc == 1 || (argc == 2 && !std::strcmp(argv[1], "--staging")));
    const bool selected = argc == 2;
    diagnostics = std::tmpfile();
    assert(diagnostics);
    unsetenv("STRIKERS_VITAGL_PACKET_PROFILE");
    unsetenv("STRIKERS_VITA_NATIVE_VIDEO");
    if (selected) setenv("STRIKERS_VITA_NATIVE_VIDEO_STAGING", "1", 1);
    else unsetenv("STRIKERS_VITA_NATIVE_VIDEO_STAGING");
    Planes disabled;
    assert(decode(1, disabled) < 0 && disabled.untouched() && !loads);
    setenv("STRIKERS_VITA_NATIVE_VIDEO", "1", 1);

    // An immediately available picture requires no stop/reset and keeps the
    // decoder alive. Consecutive AUs must produce their own distinct pixels.
    schedule({{Op::Decode, 0, 1, 7}});
    Planes direct;
    assert(!decode(7, direct)); direct.check(7);
    assert(bool(decoder.stagingPixels) == selected); check_staging_pixels();
    assert(creates == 1 && !stopCalls && !flushCalls && decoder.pictures == 1 && decoder.decodeAUs == 1);
    schedule({{Op::Decode, 0, 1, 11}});
    assert(!decode(11, direct)); direct.check(11);
    assert(creates == 1 && decoder.pictures == 2);

    // Delayed output is drained without submitting a fabricated or future AU.
    // The following Decode is rejected by the mock unless Flush occurred.
    schedule({{Op::Decode, 0, 0, 19}, {Op::Stop, 0, 1}, {Op::Flush}});
    Planes delayed;
    assert(!decode(19, delayed)); delayed.check(19);
    assert(creates == 1 && stopCalls == 1 && flushCalls == 1 && decoder.decodeAUs == 3);
    schedule({{Op::Decode, 0, 0, 23}, {Op::Stop, 0, 1}, {Op::Flush}});
    assert(!decode(23, delayed)); delayed.check(23);
    assert(creates == 1 && decoder.pictures == 4);

    // Every rejected SDK response leaves caller planes untouched, even if a
    // preceding movie/attempt filled the retained hardware output allocation.
    for (const auto& replies : std::vector<std::vector<Reply>>{
        {{Op::Decode, -41, 1, 29}},
        {{Op::Decode, 0, 2, 29}},
        {{Op::Decode, 0, 0, 29}, {Op::Stop, -42, 1}},
        {{Op::Decode, 0, 0, 29}, {Op::Stop, 0, 0}},
        {{Op::Decode, 0, 0, 29}, {Op::Stop, 0, 2}},
        {{Op::Decode, 0, 0, 29}, {Op::Stop, 0, 1}, {Op::Flush, -43}}})
    {
        assert(next == script.size()); script = replies; next = 0;
        Planes rejected;
        assert(decode(29, rejected) < 0 && rejected.untouched());
        empty();
        assert(!decoder.pictures && !decoder.components && !decoder.decodeAUs &&
               !decoder.stopCalls && !decoder.flushCalls);
    }
    for (Damage damage : {Damage::Format, Damage::Pitch, Damage::Width, Damage::Height,
                          Damage::Horizontal, Damage::Vertical, Damage::CropLeft, Damage::CropRight,
                          Damage::CropTop, Damage::CropBottom, Damage::Pointer0, Damage::Pointer1})
    {
        schedule({{Op::Decode, 0, 0, 31}, {Op::Stop, 0, 1, 0, damage}});
        Planes rejected;
        assert(decode(31, rejected) < 0 && rejected.untouched());
        empty();
    }
    schedule({{Op::Decode, 0, 1, 37}});
    Planes restarted;
    assert(!decode(37, restarted)); restarted.check(37);
    assert(decoder.pictures == 1 && decoder.components == 1 && decoder.decodeAUs == 1);
    PortNativeVideoReset(); empty();
    PortNativeVideoReset(); empty();
    check_dimensions();
    check_staging_contracts(selected);
    check_profile();
    check_staging_profile();
    assert(!std::fclose(diagnostics));
    diagnostics = nullptr;
    std::puts("Native video driver: exact Annex B input, contiguous NV12, immediate/delayed frame identity, "
              "per-movie decoder geometry, guarded planes, Stop/Flush lifecycle, allocation/error rejection, "
              "unchanged failed outputs, movie reset, opt-in staging/fallback/byte identity and gated phase attribution passed");
}
