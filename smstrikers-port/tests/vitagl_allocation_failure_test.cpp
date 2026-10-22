// The runner inserts the complete production functions. Only allocation, GPU
// surface initialization and deferred retirement are replaced by checked fakes.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned checks = 0;
static void require(bool condition, const char* message)
{
    ++checks;
    if (!condition)
    {
        std::fprintf(stderr, "allocation failure: %s\n", message);
        std::exit(1);
    }
}
#define CHECK(condition) require(bool(condition), #condition)

using GLboolean = bool;
using GLenum = unsigned;
using GLsizei = int;
using GLuint = uintptr_t;
constexpr GLboolean GL_FALSE = false, GL_TRUE = true;
enum : GLenum
{
    GL_OUT_OF_MEMORY = 0x505, GL_STREAM_DRAW, GL_STREAM_COPY, GL_STREAM_READ,
    GL_DYNAMIC_DRAW, GL_DYNAMIC_READ, GL_DYNAMIC_COPY, GL_STATIC_DRAW
};
enum vglMemType { VGL_MEM_RAM, VGL_MEM_VRAM };
static vglMemType VGL_MEM_MAIN = VGL_MEM_VRAM;
constexpr uint32_t OBJ_NOT_USED = UINT32_MAX, FRAME_PURGE_FREQ = 4;
static uint32_t vgl_framecount = 100;
static GLenum vgl_error = 0;
static GLboolean vgl_stream_wants_scratch = GL_TRUE;
static GLboolean vgl_dynamic_wants_scratch = GL_FALSE;

enum
{
    SCE_GXM_MULTISAMPLE_NONE, SCE_GXM_MULTISAMPLE_2X, SCE_GXM_MULTISAMPLE_4X,
    SCE_GXM_DEPTH_STENCIL_FORMAT_DF32M_S8, SCE_GXM_DEPTH_STENCIL_FORMAT_DF32M,
    SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR, SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED,
    SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED
};
constexpr unsigned SCE_GXM_TILE_SIZEX = 32, SCE_GXM_TILE_SIZEY = 32;
constexpr unsigned SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT = 16;
static int msaa_mode = SCE_GXM_MULTISAMPLE_NONE;
struct SceGxmDepthStencilSurface
{
    void* depthData = nullptr;
    void* stencilData = nullptr;
    unsigned format = 0, stride = 0, load = 0, store = 0;
};
struct vbo
{
    void* ptr = nullptr;
    GLboolean scratch = false;
    vglMemType type = VGL_MEM_RAM;
    GLsizei size = 0;
    uint32_t last_frame = OBJ_NOT_USED;
};

struct Allocation
{
    alignas(16) std::array<uint8_t, 65536> bytes{};
    size_t size = 0;
    bool live = false;
    unsigned retired = 0, freed = 0;
};
static std::array<Allocation, 48> allocations;
static unsigned allocationCount = 0, allocationAttempts = 0, failAttempt = 0;
static unsigned writeCount = 0, retireCount = 0, freeCount = 0;
alignas(16) static std::array<uint8_t, 128> scratchPool{};
static uint8_t* circular_data_pool_ptr[1] = {scratchPool.data()};
static uint8_t* circular_data_pool_limit[1] = {scratchPool.data() + scratchPool.size()};
static int vgl_circular_idx = 0;

static Allocation& allocation(void* pointer)
{
    for (unsigned i = 0; i < allocationCount; ++i)
        if (allocations[i].bytes.data() == pointer)
            return allocations[i];
    require(false, "pointer is not a test allocation");
    std::abort();
}
static void* allocate(size_t size)
{
    ++allocationAttempts;
    if (allocationAttempts == failAttempt)
        return nullptr;
    CHECK(allocationCount < allocations.size());
    auto& result = allocations[allocationCount++];
    CHECK(size && size <= result.bytes.size());
    result.size = size;
    result.live = true;
    std::memset(result.bytes.data(), 0x55, size);
    return result.bytes.data();
}
static void* gpu_alloc_mapped_aligned(size_t alignment, size_t size, vglMemType)
{
    CHECK(alignment == SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT);
    return allocate(size);
}
static void* gpu_alloc_mapped(size_t size, vglMemType) { return allocate(size); }
static void vgl_free(void* pointer)
{
    auto& value = allocation(pointer);
    CHECK(value.live && !value.freed);
    value.live = false;
    ++value.freed;
    ++freeCount;
}
static void retire(void* pointer)
{
    require(pointer != nullptr, "NULL must not enter the purge list");
    auto& value = allocation(pointer);
    CHECK(value.live && !value.retired);
    ++value.retired;
    ++retireCount;
}
static void collect()
{
    for (unsigned i = 0; i < allocationCount; ++i)
        if (allocations[i].retired && allocations[i].live)
            vgl_free(allocations[i].bytes.data());
}
static bool writable(void* pointer, size_t size)
{
    if (!pointer)
        return false;
    const uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
    const auto within = [&](const uint8_t* start, size_t available) {
        const uintptr_t begin = reinterpret_cast<uintptr_t>(start);
        return address >= begin && address - begin <= available &&
               size <= available - (address - begin);
    };
    if (within(scratchPool.data(), scratchPool.size()))
        return true;
    for (unsigned i = 0; i < allocationCount; ++i)
        if (allocations[i].live && within(allocations[i].bytes.data(), allocations[i].size))
            return true;
    return false;
}
static void vgl_memset(void* destination, int value, size_t size)
{
    require(writable(destination, size), "CPU memset requires live GPU storage");
    ++writeCount;
    std::memset(destination, value, size);
}
static void vgl_fast_memcpy(void* destination, const void* source, size_t size)
{
    require(writable(destination, size), "CPU memcpy requires live GPU storage");
    CHECK(source != nullptr);
    ++writeCount;
    std::memcpy(destination, source, size);
}
static int vglDepthStencilSurfaceInit(SceGxmDepthStencilSurface* surface, unsigned format,
                                      unsigned, unsigned stride, void* depth, void* stencil)
{
    CHECK(depth && allocation(depth).live);
    if (stencil)
        CHECK(allocation(stencil).live);
    *surface = {depth, stencil, format, stride, 0, 0};
    return 0;
}
static void sceGxmDepthStencilSurfaceSetForceLoadMode(SceGxmDepthStencilSurface* surface, unsigned mode)
{
    surface->load = mode;
}
static void sceGxmDepthStencilSurfaceSetForceStoreMode(SceGxmDepthStencilSurface* surface, unsigned mode)
{
    surface->store = mode;
}
template<class... Args> static void vgl_log(const char*, Args...) {}

// These are the flags used by the native provider. In particular, required OOM
// checks must still execute when optional argument validation is compiled out.
#define SKIP_ERROR_HANDLING 1
#define STORE_DEPTH_STENCIL 1
#define HAVE_SCRATCH_MEMORY 1
#define VGL_ALIGN(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define mark_as_dirty(pointer) retire(pointer)
#define SET_GL_ERROR(error) do { vgl_error = error; return; } while (0);
#define SET_GL_ERROR_WITH_RET(error, result) do { vgl_error = error; return result; } while (0);
#include "utils/circular_pool.h"
#include "vitagl_allocation_provider.inc"

static void same_surface(const SceGxmDepthStencilSurface& left, const SceGxmDepthStencilSurface& right)
{
    CHECK(left.depthData == right.depthData && left.stencilData == right.stencilData);
    CHECK(left.format == right.format && left.stride == right.stride);
    CHECK(left.load == right.load && left.store == right.store);
}
static void check_depth_failures()
{
    unsigned sentinel = 0;
    SceGxmDepthStencilSurface surface{&sentinel, &sentinel, 77, 88, 99, 100};
    const auto before = surface;
    const auto writes = writeCount, allocs = allocationCount, retired = retireCount;
    failAttempt = allocationAttempts + 1;
    init_depth_stencil_buffer(48, 17, &surface, GL_TRUE);
    CHECK(vgl_error == GL_OUT_OF_MEMORY && allocationCount == allocs);
    CHECK(writeCount == writes && retireCount == retired);
    same_surface(surface, before);

    vgl_error = 0;
    const auto freed = freeCount;
    failAttempt = allocationAttempts + 2;
    init_depth_stencil_buffer(48, 17, &surface, GL_TRUE);
    CHECK(vgl_error == GL_OUT_OF_MEMORY && allocationCount == allocs + 1);
    CHECK(writeCount == writes + 1 && retireCount == retired && freeCount == freed + 1);
    CHECK(!allocations[allocs].live && allocations[allocs].freed == 1);
    same_surface(surface, before);

    failAttempt = 0;
    for (unsigned factor : {1u, 2u, 4u})
    {
        vgl_error = 0;
        msaa_mode = factor == 1 ? SCE_GXM_MULTISAMPLE_NONE :
                    factor == 2 ? SCE_GXM_MULTISAMPLE_2X : SCE_GXM_MULTISAMPLE_4X;
        init_depth_stencil_buffer(48, 17, &surface, GL_TRUE);
        CHECK(!vgl_error && surface.depthData && surface.stencilData);
        CHECK(surface.format == SCE_GXM_DEPTH_STENCIL_FORMAT_DF32M_S8);
        CHECK(surface.stride == (factor == 4 ? 128u : 64u));
        CHECK(surface.load == SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED);
        CHECK(surface.store == SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED);
        auto& depth = allocation(surface.depthData);
        CHECK(depth.size == 64 * 32 * 4 * factor);
        CHECK(allocation(surface.stencilData).size == 64 * 32 * factor);
        for (size_t i = 0; i < depth.size; ++i)
            CHECK(depth.bytes[i] == 0x80);
        vgl_free(surface.depthData);
        vgl_free(surface.stencilData);
    }
    msaa_mode = SCE_GXM_MULTISAMPLE_NONE;
    init_depth_stencil_buffer(48, 17, &surface, GL_FALSE);
    CHECK(surface.depthData && !surface.stencilData);
    CHECK(surface.format == SCE_GXM_DEPTH_STENCIL_FORMAT_DF32M);
    vgl_free(surface.depthData);
}
static void check_buffer_failures()
{
    const std::array<uint8_t, 32> source{1, 2, 3, 4, 5};
    vbo buffer;
    const GLuint name = reinterpret_cast<GLuint>(&buffer);
    glNamedBufferData(name, source.size(), source.data(), GL_DYNAMIC_DRAW);
    CHECK(buffer.ptr && !buffer.scratch && !vgl_error);
    CHECK(std::memcmp(buffer.ptr, source.data(), source.size()) == 0);
    const auto old = buffer.ptr;
    buffer.last_frame = vgl_framecount;
    const auto writes = writeCount, retired = retireCount;
    failAttempt = allocationAttempts + 1;
    glNamedBufferData(name, source.size(), source.data(), GL_DYNAMIC_DRAW);
    CHECK(vgl_error == GL_OUT_OF_MEMORY && !buffer.ptr && buffer.size == 0);
    CHECK(buffer.last_frame == OBJ_NOT_USED && writeCount == writes);
    CHECK(retireCount == retired + 1 && allocation(old).retired == 1 && allocation(old).live);

    failAttempt = 0;
    vgl_error = 0;
    glNamedBufferData(name, source.size(), source.data(), GL_DYNAMIC_DRAW);
    CHECK(buffer.ptr && buffer.size == int(source.size()) && !vgl_error);
    CHECK(std::memcmp(buffer.ptr, source.data(), source.size()) == 0);
    const auto unused = buffer.ptr;
    const auto freed = freeCount;
    failAttempt = allocationAttempts + 1;
    glNamedBufferData(name, source.size(), source.data(), GL_DYNAMIC_DRAW);
    CHECK(vgl_error == GL_OUT_OF_MEMORY && !buffer.ptr && buffer.size == 0);
    CHECK(freeCount == freed + 1 && allocation(unused).freed == 1);
    CHECK(retireCount == retired + 1);
    failAttempt = 0;
    vgl_error = 0;
    collect();
    CHECK(allocation(old).freed == 1);
}
static void check_pool_failures()
{
    circular_data_pool_ptr[0] = scratchPool.data() + 1;
    const auto attempts = allocationAttempts, retired = retireCount;
    auto* aligned = vgl_reserve_data_pool(13);
    CHECK(aligned == scratchPool.data() + 16);
    CHECK(circular_data_pool_ptr[0] == scratchPool.data() + 32);
    CHECK(allocationAttempts == attempts && retireCount == retired);

    circular_data_pool_ptr[0] = circular_data_pool_limit[0];
    failAttempt = allocationAttempts + 1;
    auto* failed = vgl_reserve_data_pool(64);
    CHECK(!failed && vgl_error == GL_OUT_OF_MEMORY && retireCount == retired);
    CHECK(circular_data_pool_ptr[0] == circular_data_pool_limit[0]);
    failAttempt = 0;
    vgl_error = 0;
    auto* fallback = vgl_reserve_data_pool(64);
    CHECK(fallback && !vgl_error && retireCount == retired + 1);
    CHECK(allocation(fallback).retired == 1);

    vbo stream;
    const GLuint name = reinterpret_cast<GLuint>(&stream);
    std::array<uint8_t, 16> source{9, 8, 7};
    failAttempt = allocationAttempts + 1;
    const auto writes = writeCount;
    glNamedBufferData(name, source.size(), source.data(), GL_STREAM_DRAW);
    CHECK(!stream.ptr && stream.size == 0 && stream.scratch);
    CHECK(vgl_error == GL_OUT_OF_MEMORY && writeCount == writes);
    CHECK(retireCount == retired + 1);
    failAttempt = 0;
    vgl_error = 0;
    glNamedBufferData(name, source.size(), source.data(), GL_STREAM_DRAW);
    CHECK(stream.ptr && stream.scratch && !vgl_error);
    CHECK(retireCount == retired + 2 && allocation(stream.ptr).retired == 1);
    CHECK(std::memcmp(stream.ptr, source.data(), source.size()) == 0);
    collect();
    CHECK(allocation(fallback).freed == 1 && allocation(stream.ptr).freed == 1);
}
int main(int argc, char** argv)
{
    const char* selected = argc > 1 ? argv[1] : "all";
    const bool all = !std::strcmp(selected, "all");
    CHECK(argc <= 2 && (all || !std::strcmp(selected, "depth") ||
                       !std::strcmp(selected, "buffer") || !std::strcmp(selected, "pool")));
    if (all || !std::strcmp(selected, "depth"))
        check_depth_failures();
    if (all || !std::strcmp(selected, "buffer"))
        check_buffer_failures();
    if (all || !std::strcmp(selected, "pool"))
        check_pool_failures();
    for (unsigned i = 0; i < allocationCount; ++i)
        CHECK(!allocations[i].live && allocations[i].freed == 1);
    std::printf("VitaGL allocation failures (%s): %u checks, %u allocations released exactly once; OOM/retry passed\n",
                selected, checks, allocationCount);
}
