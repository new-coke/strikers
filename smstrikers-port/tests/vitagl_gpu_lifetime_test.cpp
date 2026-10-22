// Only GXM/OS operations and allocation are mocked. The runner inserts the
// production scene, blit, mipmap, retirement and GC function bodies unchanged.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

static void require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "GPU lifetime: %s\n", message); std::exit(1); }
}
#define CHECK(x) require(bool(x), #x)
using GLboolean = bool;
using GLuint = uintptr_t;
using GLenum = unsigned;
using GLint = int;
using GLsizei = int;
using GLbitfield = unsigned;
using SceUID = int;
using SceGxmTextureFormat = unsigned;
using SceGxmTransferFormat = unsigned;
using SceGxmRenderTarget = int;
enum { GL_FALSE, GL_TRUE, GL_NO_ERROR = 0, GL_BACK = 2, GL_LINEAR,
       GL_COLOR_BUFFER_BIT, GL_INVALID_OPERATION, GL_OUT_OF_MEMORY,
       SCE_GXM_CULL_NONE, SCE_GXM_DEPTH_WRITE_DISABLED, SCE_GXM_DEPTH_WRITE_ENABLED,
       SCE_GXM_POLYGON_MODE_TRIANGLE_FILL, SCE_GXM_TEXTURE_FILTER_LINEAR,
       SCE_GXM_TEXTURE_FILTER_POINT, SCE_GXM_STENCIL_FUNC_ALWAYS, SCE_GXM_STENCIL_OP_KEEP,
       SCE_GXM_PRIMITIVE_TRIANGLE_FAN, SCE_GXM_INDEX_FORMAT_U16,
       SCE_GXM_COLOR_FORMAT_A8B8G8R8, SCE_GXM_COLOR_SURFACE_LINEAR };
enum { TEX_UNUSED, TEX_UNINITIALIZED, TEX_VALID };
enum vglMemType { VGL_MEM_VRAM, VGL_MEM_RAM, VGL_MEM_SLOW, VGL_MEM_BUDGET, VGL_MEM_EXTERNAL };
constexpr unsigned SCE_GXM_TEXTURE_BASE_FORMAT_P8 = 0x95000000;
constexpr uint32_t OBJ_NOT_USED = UINT32_MAX;
constexpr int FRAME_PURGE_FREQ = 4, FRAME_PURGE_LIST_SIZE = 64, FRAME_PURGE_RENDERTARGETS_LIST_SIZE = 8;
#define SKIP_SPLASHSCREEN 1
#define SKIP_ERROR_HANDLING 1
#define DISABLE_TILE_CLIPPER 1
#define DISABLE_CIRCULAR_POOL 1
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define VGL_ALIGN(a,b) (((a)+(b)-1)&~((b)-1))
#define SET_GL_ERROR_WITH_VALUE(error, value) do { (void)(value); vgl_error = (error); return; } while (0);
#define SET_GL_ERROR(error) SET_GL_ERROR_WITH_VALUE(error, 0)

struct SceGxmTexture { void* data = nullptr; uint32_t width = 0, height = 0, format = 0; };
struct SceGxmColorSurface { void* data = nullptr; SceGxmTexture backgroundTex; };
struct SceGxmDepthStencilSurface { void* depthData = nullptr; void* stencilData = nullptr; };
struct texture {
    void* data = nullptr;
    void* palette_data = nullptr;
    SceGxmTexture gxm_tex;
    uint32_t last_frame = OBJ_NOT_USED;
    int mip_count = 0, status = TEX_UNINITIALIZED;
    bool use_mips = true;
    unsigned mag_filter = 0, min_filter = 0;
};
struct framebuffer {
    uint32_t width = 64, height = 64;
    bool is_float = false, is_depth_hidden = false;
    SceGxmDepthStencilSurface depthbuffer;
    SceGxmDepthStencilSurface* depthbuffer_ptr = nullptr;
    SceGxmRenderTarget* target = nullptr;
    texture* tex = nullptr;
    SceGxmColorSurface colorbuffer;
};
struct vector4f { float x, y, z, w; };
struct Rect { int x = 0, y = 0, w = 960, h = 544; } gl_viewport;
struct { unsigned value = 0; } query_fence;
struct { int index = 0, vsync = 0; } shared_fb_info;
struct display_queue_callback_data { void* addr; };
struct SceCommonDialogUpdateParam {
    struct { int colorFormat, surfaceType, width, height, strideInPixels; void *colorSurfaceData, *depthSurfaceData; } renderTarget;
    void* displaySyncObject;
};
static framebuffer *active_write_fb = nullptr, *in_use_framebuffer = nullptr, *old_framebuffer = nullptr;
static bool dirty_framebuffer = false, dirty_query = false, needs_scene_reset = true, needs_end_scene = false;
static bool is_fbo_float = false, system_app_mode = false, is_rendering_display = true;
static bool dirty_scissor_state = false, dirty_frag_unifs = false, dirty_vert_unifs = false;
static bool depth_test_state = false, orig_depth_test = false, depth_mask_state = true;
static int polygon_mode_front = 0, polygon_mode_back = 0, legacy_pool_size = 0, vsync_interval = 0, shared_fb = 0;
static float *legacy_pool = nullptr, *legacy_pool_ptr = nullptr;
static float x_port, x_scale, y_port, y_scale, z_port, z_scale;
static void* gxm_context = nullptr;
static int renderTarget = 1, depthStorage = 1;
static SceGxmRenderTarget* gxm_render_target = &renderTarget;
static SceGxmDepthStencilSurface gxm_depth_stencil_surface{&depthStorage, nullptr};
static SceGxmColorSurface gxm_color_surfaces[3];
static void* gxm_sync_objects[3]{};
static void* gxm_color_surfaces_addr[3]{};
static unsigned gxm_back_buffer_index = 1, gxm_front_buffer_index = 0, gxm_display_buffer_count = 3;
static int DISPLAY_WIDTH = 960, DISPLAY_HEIGHT = 544, DISPLAY_STRIDE = 960;
static int NEW_DISPLAY_WIDTH = 0, NEW_DISPLAY_HEIGHT = 0;
static float DISPLAY_WIDTH_FLOAT = 960, DISPLAY_HEIGHT_FLOAT = 544;
static vector4f clear_vertices[1];
static int blit_vertex_program_patched, blit_fragment_program_float_patched, blit_fragment_program_patched;
static uint16_t depth_clear_indices[4]{};
static GLenum display_read_mode = GL_BACK, vgl_error = GL_NO_ERROR;
static uint32_t vgl_framecount = 100;
static int unsafe_allocator_counter = 0;
static bool use_extra_mem = true;
static vglMemType VGL_MEM_MAIN = VGL_MEM_VRAM;
static void* color_table = nullptr;

static std::map<void*, size_t> allocations;
static std::set<void*> openReferences, gpuReferences, transferReferences;
static std::map<void*, unsigned> freed;
static unsigned finishes = 0, transferFinishes = 0, downscales = 0, draws = 0;
static bool failAllocation = false, failReallocation = false;
static int transferResult = 0, downscaleResult = 0;
static bool failDepth = false;
static int targetResult = 0, beginResult = 0;
static unsigned begins = 0, stateCalls = 0;
static SceGxmTexture* fragmentTexture = nullptr;
static void* owner_of(const void* pointer) {
    const auto value = reinterpret_cast<uintptr_t>(pointer);
    for (const auto& [base, size] : allocations)
        if (value >= reinterpret_cast<uintptr_t>(base) && value < reinterpret_cast<uintptr_t>(base) + size) return base;
    return nullptr;
}
static void* allocate(size_t size) {
    if (failAllocation) return nullptr;
    void* p = std::malloc(size);
    CHECK(p && allocations.emplace(p, size).second);
    freed.erase(p);
    std::memset(p, 0, size);
    return p;
}
static void vgl_free(void* p) {
    CHECK(p && allocations.count(p));
    require(!openReferences.count(p) && !gpuReferences.count(p) && !transferReferences.count(p),
            "storage freed while an open scene or GPU operation still references it");
    CHECK(++freed[p] == 1);
    allocations.erase(p);
    std::free(p);
}
static void* vgl_realloc(void* p, size_t size) {
    CHECK(allocations.count(p));
    if (failReallocation) return nullptr;
    auto* next = allocate(size);
    if (!next) return nullptr;
    std::memcpy(next, p, std::min(size, allocations.at(p)));
    vgl_free(p);
    return next;
}
static void vgl_memcpy(void* dst, const void* src, size_t size) { CHECK(dst && src); std::memcpy(dst, src, size); }
static void vgl_memset(void* dst, int byte, size_t size) { CHECK(dst); std::memset(dst, byte, size); }
#define vgl_fast_memcpy vgl_memcpy
#define sceClibMemcpy vgl_memcpy
static void* gpu_alloc_mapped(size_t size, vglMemType) { return allocate(size); }
static uint32_t nearest_po2(uint32_t n) { uint32_t p = 1; while (p < n) p *= 2; return p; }
static uint32_t tex_format_to_bytespp(unsigned) { return 4; }
static unsigned tex_format_to_transfer(unsigned f) { return f; }
static unsigned sceGxmTextureGetFormat(const SceGxmTexture* t) { return t->format; }
static unsigned sceGxmTextureGetWidth(const SceGxmTexture* t) { return t->width; }
static unsigned sceGxmTextureGetHeight(const SceGxmTexture* t) { return t->height; }
static void vglInitLinearTexture(SceGxmTexture* t, void* p, unsigned format, unsigned w, unsigned h, int) {
    CHECK(allocations.count(p));
    *t = {p, w, h, format};
}
static int sceGxmTransferDownscale(unsigned, const void* src, unsigned, unsigned, unsigned w, unsigned h,
                                  int srcStride, unsigned, void* dst, unsigned, unsigned, int dstStride,
                                  void*, unsigned, void*) {
    ++downscales;
    if (downscaleResult < 0) return downscaleResult;
    auto* base = owner_of(src);
    CHECK(base && owner_of(dst) == base && w && h);
    CHECK(reinterpret_cast<uintptr_t>(src) + size_t(srcStride) * h <= reinterpret_cast<uintptr_t>(base) + allocations.at(base));
    CHECK(reinterpret_cast<uintptr_t>(dst) + size_t(dstStride) * (h / 2) <= reinterpret_cast<uintptr_t>(base) + allocations.at(base));
    transferReferences.insert(base);
    return 0;
}
static int sceGxmTransferFinish() {
    ++transferFinishes;
    if (transferResult >= 0) transferReferences.clear();
    return transferResult;
}
static int sceGxmBeginScene(void*, int, void*, void*, void*, void*, SceGxmColorSurface* color,
                             SceGxmDepthStencilSurface*) {
    ++begins;
    if (beginResult) return beginResult;
    CHECK(openReferences.empty());
    if (color->data) { CHECK(allocations.count(color->data)); openReferences.insert(color->data); }
    return 0;
}
static int sceGxmEndScene(void*, void*, void*) {
    gpuReferences.insert(openReferences.begin(), openReferences.end()); openReferences.clear(); return 0;
}
static void sceGxmFinish(void*) { ++finishes; gpuReferences.clear(); }
static int addEntryResult = 0;
static int sceGxmDisplayQueueAddEntry(void*, void*, display_queue_callback_data*) { gpuReferences.clear(); return addEntryResult; }
static bool init_depth_stencil_buffer(uint32_t, uint32_t, SceGxmDepthStencilSurface* p, bool) {
    if (failDepth) return false;
    p->depthData = &depthStorage; return true;
}
static int setup_render_target(SceGxmRenderTarget** t, uint32_t, uint32_t, int) {
    if (targetResult) return targetResult;
    *t = &renderTarget; return 0;
}
static void sceGxmDestroyRenderTarget(void*) {}
static void sceGxmSetFragmentTexture(void*, unsigned, SceGxmTexture* t) { fragmentTexture = t; }
static void sceGxmDraw(void*, int, int, const uint16_t*, int) {
    ++draws;
    if (fragmentTexture && fragmentTexture->data) {
        CHECK(allocations.count(fragmentTexture->data)); openReferences.insert(fragmentTexture->data);
    }
}
static void vector4f_convert_to_local_space(vector4f* v, int x, int y, int w, int h) {
    *v = {float(x), float(x+w), float(y), float(y+h)};
}
#define NOOP(name) template<class... T> static void name(T...) { ++stateCalls; }
NOOP(sceDisplayWaitVblankStartMulti) NOOP(sceSharedFbBegin) NOOP(sceSharedFbEnd)
NOOP(glViewport) NOOP(vglSetViewport) NOOP(change_cull_mode) NOOP(update_scissor_test)
NOOP(invalidate_viewport) NOOP(sceGxmSetCullMode) NOOP(invalidate_depth_test)
NOOP(change_depth_write) NOOP(sceGxmSetFrontDepthBias) NOOP(sceGxmSetBackDepthBias)
NOOP(sceGxmSetFrontPolygonMode) NOOP(sceGxmSetBackPolygonMode)
NOOP(sceGxmSetVertexProgram) NOOP(sceGxmSetFragmentProgram)
NOOP(vglSetTexMagFilter) NOOP(vglSetTexMinFilter) NOOP(vglSetTexMipmapCount)
NOOP(sceGxmSetFrontStencilFunc) NOOP(sceGxmSetBackStencilFunc) NOOP(sceGxmSetVertexStream)
NOOP(validate_depth_test) NOOP(refresh_stencil_settings) NOOP(update_polygon_offset)
NOOP(validate_viewport) NOOP(sceCommonDialogUpdate) NOOP(create_display_render_target)
NOOP(init_display_color_surfaces)

// Instrument only the assignment scheduling point, leaving both producer and
// worker instructions intact. A foreground cannot run while WaitPull blocks it.
static bool workerActive = false, foregroundBlocked = false;
static std::function<void()> onRotation;
struct GCIndex {
    int value = 0;
    operator int() const { return value; }
    GCIndex& operator=(int next) {
        value = next;
        if (workerActive && onRotation) onRotation();
        return *this;
    }
};
static GCIndex frame_purge_idx;
static int frame_elem_purge_idx = 0, frame_rt_purge_idx = 0, frame_purge_clean_idx = 1;
static void* frame_purge_list[FRAME_PURGE_FREQ][FRAME_PURGE_LIST_SIZE]{};
static void* frame_rt_purge_list[FRAME_PURGE_FREQ][FRAME_PURGE_RENDERTARGETS_LIST_SIZE]{};
#define mark_as_dirty(x) frame_purge_list[frame_purge_idx][frame_elem_purge_idx++] = x
static void* gpu_alloc_mapped_temp(size_t bytes) {
    auto* p = allocate(bytes); if (p) { mark_as_dirty(p); } return p;
}
static SceUID gc_mutex[2]{};
static std::array<int, 2> semaphores{};
static unsigned semaphoreCreates = 0, collections = 0, producerCalls = 0, allocationAttempts = 0;
static unsigned allocationReadyAfter = 0;
static int signalResult = 0, waitResult = 0;
static void run_gc();
struct GcIdle {};
static int sceKernelCreateSema(const char*, int, int initial, int maximum, void*) {
    CHECK(semaphoreCreates < 2 && maximum == FRAME_PURGE_FREQ);
    semaphores[semaphoreCreates] = initial; return int(semaphoreCreates++);
}
static int sceKernelSignalSema(int id, int n) {
    if (!workerActive) { ++producerCalls; if (signalResult < 0) return signalResult; }
    CHECK(id >= 0 && id < 2 && n == 1 && semaphores[id] < FRAME_PURGE_FREQ);
    ++semaphores[id];
    if (workerActive && id == 1) ++collections;
    return 0;
}
static int sceKernelWaitSema(int id, int n, void*) {
    CHECK(n == 1);
    if (workerActive) {
        CHECK(id == 0);
        if (!semaphores[id]) throw GcIdle{};
    } else {
        ++producerCalls;
        CHECK(id == 1);
        if (waitResult < 0) return waitResult;
        if (!semaphores[id]) { foregroundBlocked = true; run_gc(); foregroundBlocked = false; }
    }
    CHECK(semaphores[id]); --semaphores[id]; return 0;
}
static int sceKernelExitDeleteThread(int x) { return x; }
static void sceKernelDelayThread(unsigned) { require(false, "GC retry used a time delay instead of completion"); }
static void* vgl_memalign(size_t alignment, size_t size, vglMemType) {
    ++allocationAttempts;
    CHECK(alignment == 16);
    require(!semaphores[0], "allocation retry raced an unfinished GC job");
    return collections >= allocationReadyAfter ? allocate(size) : nullptr;
}
#include "port/frame_timeline.h"
#include "vitagl_gpu_provider.inc"

static void run_gc() {
    CHECK(!workerActive); workerActive = true;
    try { garbage_collector(0, nullptr); } catch (const GcIdle&) {}
    workerActive = false;
}
static void reset() {
    CHECK(allocations.empty() && openReferences.empty() && gpuReferences.empty() && transferReferences.empty());
    std::memset(frame_purge_list, 0, sizeof frame_purge_list);
    std::memset(frame_rt_purge_list, 0, sizeof frame_rt_purge_list);
    onRotation = {};
    frame_purge_idx = 0; frame_elem_purge_idx = frame_rt_purge_idx = 0; frame_purge_clean_idx = 1;
    semaphoreCreates = collections = producerCalls = allocationAttempts = 0;
    initialize_gc_semaphores();
    active_write_fb = in_use_framebuffer = old_framebuffer = nullptr;
    needs_end_scene = false; needs_scene_reset = true; dirty_framebuffer = dirty_query = false;
    vgl_framecount = 100; vgl_error = GL_NO_ERROR; unsafe_allocator_counter = 0;
    failAllocation = failReallocation = false; transferResult = downscaleResult = signalResult = waitResult = 0;
    failDepth = false; targetResult = beginResult = 0;
    allocationReadyAfter = 0; fragmentTexture = nullptr;
}
static texture make_texture(unsigned w = 64, unsigned h = 64) {
    texture t;
    gpu_alloc_texture(w, h, 0, nullptr, &t, 4, nullptr, nullptr, true);
    CHECK(t.data && t.status == TEX_VALID && t.last_frame == OBJ_NOT_USED);
    return t;
}
static framebuffer attach(texture& t) {
    framebuffer f; f.tex = &t; f.width = t.gxm_tex.width; f.height = t.gxm_tex.height;
    f.colorbuffer.data = t.data; return f;
}
static void complete_and_collect() {
    active_write_fb = nullptr;
    glFinish();
    for (int i = 0; i < FRAME_PURGE_FREQ; ++i) vglSwapBuffers(false);
}
static void attachment() {
    reset();
    auto t = make_texture(); auto f = attach(t); auto* p = t.data;
    active_write_fb = &f; scene_reset();
    CHECK(openReferences.count(p) && draws == 0);
    gpu_free_texture_data(&t);
    CHECK(allocations.count(p) && frame_purge_list[0][0] == p);
    complete_and_collect();
    CHECK(allocations.empty() && freed[p] == 1);
}
static void blit() {
    reset();
    auto source = make_texture(), destination = make_texture();
    source.last_frame = 1;
    auto a = attach(source), b = attach(destination);
    glBlitNamedFramebuffer(reinterpret_cast<GLuint>(&a), reinterpret_cast<GLuint>(&b),
                           0, 0, 64, 64, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    CHECK(source.last_frame == 100 && destination.last_frame == 100 && !active_write_fb);
    gpu_free_texture_data(&source); gpu_free_texture_data(&destination);
    complete_and_collect();
    CHECK(allocations.empty());
}
static void gc_interleave() {
    reset();
    auto first = make_texture(), late = make_texture();
    auto* firstData = first.data; auto* lateData = late.data;
    first.last_frame = late.last_frame = vgl_framecount;
    gpu_free_texture_data(&first);
    unsigned racedEnqueues = 0;
    onRotation = [&] {
        if (!foregroundBlocked && late.data) { ++racedEnqueues; gpu_free_texture_data(&late); }
    };
    vglSwapBuffers(false);
    // With the old initial credit, swap returned while this job was pending.
    if (semaphores[0]) run_gc();
    onRotation = {};
    if (late.data) gpu_free_texture_data(&late);
    for (int i = 0; i < FRAME_PURGE_FREQ + 1; ++i) { vglSwapBuffers(false); if (semaphores[0]) run_gc(); }
    require(!allocations.count(lateData), "GC lost an enqueue in the newly rotated bucket");
    CHECK(racedEnqueues == 0 && freed[firstData] == 1 && freed[lateData] == 1);
    CHECK(producerCalls == 2 * (FRAME_PURGE_FREQ + 2));
}
static void unsafe_allocation() {
    reset();
    auto t = make_texture(); auto f = attach(t); active_write_fb = &f; scene_reset();
    const auto oldFinishes = finishes, oldTransfers = transferFinishes;
    gpu_free_texture_data(&t);
    CHECK(!gpu_alloc_mapped_aligned_unsafe(16, 64, VGL_MEM_VRAM));
    CHECK(finishes == oldFinishes && transferFinishes == oldTransfers && collections == 0);
    CHECK(producerCalls == 0 && allocationAttempts == 0 && frame_purge_idx == 0);
    CHECK(needs_end_scene && !needs_scene_reset && active_write_fb == &f && !openReferences.empty());
    complete_and_collect();
    reset();
    allocationReadyAfter = 3;
    auto* recovered = gpu_alloc_mapped_aligned_unsafe(16, 64, VGL_MEM_VRAM);
    CHECK(recovered && collections == 3 && producerCalls == 6);
    vgl_free(recovered);
    reset();
    transferResult = -1;
    CHECK(!gpu_alloc_mapped_aligned_unsafe(16, 64, VGL_MEM_VRAM));
    CHECK(vgl_error == GL_INVALID_OPERATION && !producerCalls && !allocationAttempts && !collections);
    transferResult = 0; vgl_error = GL_NO_ERROR; signalResult = -1;
    CHECK(!gpu_alloc_mapped_aligned_unsafe(16, 64, VGL_MEM_VRAM));
    CHECK(vgl_error == GL_INVALID_OPERATION && !allocationAttempts && !collections);
    signalResult = 0; waitResult = -1; vgl_error = GL_NO_ERROR;
    CHECK(!gpu_alloc_mapped_aligned_unsafe(16, 64, VGL_MEM_VRAM));
    CHECK(vgl_error == GL_INVALID_OPERATION && !allocationAttempts && !collections);
    waitResult = 0; run_gc();
}
static void mips() {
    reset();
    auto t = make_texture(); t.last_frame = 77;
    auto count = transferFinishes;
    gpu_alloc_mipmaps(1, &t);
    CHECK(t.mip_count == 2 && t.last_frame == 77 && transferFinishes == count + 1 && transferReferences.empty());
    gpu_free_texture_data(&t); CHECK(allocations.empty());
    t = make_texture(2048, 4); count = transferFinishes;
    gpu_alloc_mipmaps(1, &t);
    CHECK(transferFinishes == count && transferReferences.empty());
    gpu_free_texture_data(&t); CHECK(allocations.empty());
    t = make_texture(); auto* base = t.data; const auto oldMipCount = t.mip_count;
    failAllocation = failReallocation = true;
    gpu_alloc_mipmaps(1, &t);
    CHECK(vgl_error == GL_OUT_OF_MEMORY && t.data == base && t.mip_count == oldMipCount && allocations.count(base));
    failAllocation = failReallocation = false;
    gpu_free_texture_data(&t); CHECK(allocations.empty());
    texture empty; failAllocation = true; vgl_error = GL_NO_ERROR;
    gpu_alloc_texture(80, 80, 0, nullptr, &empty, 4, nullptr, nullptr, true);
    CHECK(!empty.data && empty.status == TEX_UNINITIALIZED && vgl_error == GL_OUT_OF_MEMORY);
    failAllocation = false;
}
static void scene_failures() {
    for (unsigned failure = 0; failure < 3; ++failure) {
        reset();
        auto t = make_texture(); auto f = attach(t); active_write_fb = &f;
        const auto oldBegins = begins, oldDraws = draws;
        failDepth = failure == 0; targetResult = failure == 1 ? -1 : 0; beginResult = failure == 2 ? -1 : 0;
        scene_reset();
        CHECK(!needs_end_scene && needs_scene_reset && active_write_fb == &f);
        CHECK(vgl_error == (failure == 2 ? GL_INVALID_OPERATION : GL_OUT_OF_MEMORY));
        CHECK(begins == oldBegins + unsigned(failure == 2) && draws == oldDraws && openReferences.empty());
        if (failure == 0) CHECK(!f.depthbuffer_ptr && !f.target);
        failDepth = false; targetResult = beginResult = 0; vgl_error = GL_NO_ERROR;
        scene_reset();
        CHECK(needs_end_scene && !needs_scene_reset && openReferences.count(t.data));
        gpu_free_texture_data(&t); complete_and_collect(); CHECK(allocations.empty());
    }
    reset();
    auto source = make_texture(), destination = make_texture(); auto a = attach(source), b = attach(destination);
    active_write_fb = &a;
    const auto oldBegins = begins, oldDraws = draws, oldState = stateCalls;
    failAllocation = true;
    glBlitNamedFramebuffer(reinterpret_cast<GLuint>(&a), reinterpret_cast<GLuint>(&b),
                           0, 0, 64, 64, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    CHECK(vgl_error == GL_OUT_OF_MEMORY && active_write_fb == &a && !needs_end_scene);
    CHECK(begins == oldBegins && draws == oldDraws && stateCalls == oldState && !frame_elem_purge_idx);
    failAllocation = false; failDepth = true; vgl_error = GL_NO_ERROR;
    glBlitNamedFramebuffer(reinterpret_cast<GLuint>(&a), reinterpret_cast<GLuint>(&b),
                           0, 0, 64, 64, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    CHECK(vgl_error == GL_OUT_OF_MEMORY && active_write_fb == &a && !needs_end_scene && needs_scene_reset);
    CHECK(begins == oldBegins && draws == oldDraws && frame_elem_purge_idx == 1);
    failDepth = false;
    gpu_free_texture_data(&source); gpu_free_texture_data(&destination);
    complete_and_collect(); CHECK(allocations.empty());
}
#ifdef STRIKERS_PRESENT_PROFILE_TEST
#include "frame_timeline_provider_cases.inc"
#endif
int main(int argc, char** argv) {
    const std::string scenario = argc > 1 ? argv[1] : "all";
    const auto run = [&](const char* name, auto test) { if (scenario == "all" || scenario == name) { test(); std::printf("VitaGL GPU lifetime %s: PASS\n", name); } };
    run("attachment", attachment); run("blit", blit); run("gc", gc_interleave);
    run("unsafe", unsafe_allocation); run("mips", mips);
    run("scene", scene_failures);
#ifdef STRIKERS_PRESENT_PROFILE_TEST
    run("profile", present_profile);
#endif
    CHECK(allocations.empty());
}
