// Exercise the public game hooks and all production frontend modules. The
// graphics sink records submissions; it does not need Vita hardware or assets.
#include "port/native_match_runtime.hpp"
#include "port/deferred_renderer.hpp"
#include "port/benchmark.h"
#include "port/loading_indicator.hpp"
#include "NL/gl/glState.h"
#include "NL/gl/glConstant.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glLightUserData.h"
#include "NL/glx/glxDisplayList.h"
#include "NL/glx/glxSwap.h"
#include "NL/nlTask.h"
#include "Game/GL/ShaderSkinMesh.h"
#include "../src/platform/native_match_internal.hpp"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <filesystem>
#include <cstdlib>
#include <new>
#include <tuple>
#include <unistd.h>

namespace port::directmatch::detail
{
struct MatchRuntimeTestAccess
{
    using Bounds = MatchRuntime::RangeBounds;
    using Range = MatchRuntime::Range;
    static Bounds geometry_bounds() { return MatchRuntime::geometryBounds; }
    static Bounds texture_bounds() { return MatchRuntime::mutableTextureBounds; }
    static bool exact_overlap(Range a, Range b) { return MatchRuntime::overlaps(a, b); }
    static uint64_t invalid_count() { return MatchRuntime::invalid; }
    static const char* failure() { return MatchRuntime::failure; }
    static port::render::SceneAlphaState alpha_state() { return MatchRuntime::alphaState; }
    static auto& profile() { return MatchRuntime::packetProfile; }
    static void reset_profile() { MatchRuntime::packetProfile = {}; }
    static bool net_enabled() { return MatchRuntime::netConversion; }
    static auto net_lookup(uint32_t frame, const glModelPacket* p) { return MatchRuntime::netPackets.find(frame,p); }
    static void release_scratch() { MatchRuntime::modelScratch={}; MatchRuntime::vertexScratch={}; }
    static bool scratch_empty() { return MatchRuntime::modelScratch.vertices.empty() && MatchRuntime::modelScratch.indices.empty(); }
    using Material = MatchRuntime::PreparedMaterial;
    static bool material(eGLView view,const glModelPacket& p,Material& m,port::render::PacketProfile::Scope* scope) {
        return MatchRuntime::prepare_material(view,p,m,scope);
    }
};
}   // namespace port::directmatch::detail

namespace
{
using namespace port::render;
struct Submission
{
    uint32_t model;
    SceneMaterial material;
    Raster raster;
    std::array<float, 16> matrix;
};
std::vector<Submission> submitted;
std::vector<uint32_t> releasedModels, releasedTextures;
std::vector<ModelVertex> lastCreatedVertices;
std::vector<ModelVertex> lastDrawVertices;
std::vector<uint16_t> lastDrawIndices;
std::vector<uint32_t> selectedTargets;
std::vector<std::array<uint32_t, 2>> targetDimensions;
std::vector<TargetFormat> targetFormats;
std::vector<TextureCopy> capturedCopies;
unsigned targetsCreated = 0;
uint32_t nextId = 1, modelsCreated = 0, retraces = 0;
bool frameActive = false, lastPresent = false, arenaOwns = true;
bool diagnostics = false;
bool failTarget = false, failCopy = false, failClear = false;
bool failDraw = false, failTextureUpload = false, failTextureRegistration = false,
     failModelRegistration = false;
bool failBlack = false, failBitmap = false, bitmapFrameActive = false;
bool failLoading = false, loadingActive = false;
unsigned loadingCalls = 0, rendererInitializations = 0;
LoadingIndicator loadingDescriptor;
unsigned frameBegins = 0, frameEnds = 0, bitmapCalls = 0;
unsigned logicalFrameWidth = 640;
std::vector<char> presentationEvents;
port::nativegxm::TextureUpload bitmapUpload;
std::array<float, 4> bitmapDestination{};
bool failNextHeapAllocation = false;
uint32_t lastCreatedTexture = 0, lastCreatedModel = 0;
unsigned diffuseLevel = 63;
unsigned programLookups = 0, crowdFrameLookups = 0, rangeLookups = 0, ambientLookups = 0;
float crowdFrame = .375f, lightingRange = 1.f;
std::array<float, 4> lightingAmbient{ 1, 1, 1, 1 };
s32 currentFrame = 1;
glView viewState{};
nlMatrix4 viewMatrix{}, projectionMatrix{};
DisplayList displayList{};
constexpr uintptr_t ListHandle = 0x2222;
PlatTexture* fileTexture = nullptr;
bool shadowFixture = false;
std::array<PlatTexture*, 10> shadowTextures{};
const nlMatrix4* profileOwnedMatrix = nullptr;
port::render::FrameTimeline* lastBeginTimeline = nullptr;
unsigned materialTextureLookups = 0;

void identity(nlMatrix4& matrix)
{
    std::memset(matrix.e, 0, sizeof matrix.e);
    matrix.e[0] = matrix.e[5] = matrix.e[10] = matrix.e[15] = 1;
}

uint32_t fixture_program(const char* name)
{
    // Deliberately independent of the production hash: IDs must come from the
    // API, while fixture construction must not count as a renderer lookup.
    uint32_t hash = 2166136261u;
    while (*name)
        hash = (hash ^ uint8_t(*name++)) * 16777619u;
    return hash;
}
}   // namespace

// Fail precisely after a GPU resource was created, before the frontend owns
// its map entry. The production error path must release that resource.
void* operator new(std::size_t bytes)
{
    if (failNextHeapAllocation)
    {
        failNextHeapAllocation = false;
        throw std::bad_alloc{};
    }
    if (auto* memory = std::malloc(bytes ? bytes : 1))
        return memory;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }

static PortBenchCaptureStatus netCapture{};
static uint64_t netClockCalls=0, netClock=0;
extern "C" {
int PortDiagnosticsEnabled() { return diagnostics; }
void PortBenchGetCaptureStatus(PortBenchCaptureStatus* out) { *out = netCapture; }
unsigned long long port_monotonic_ns() { ++netClockCalls; return netClock+=100; }
unsigned PortLogicalFrameWidth() { return logicalFrameWidth; }
void PortNativeRetrace() { ++retraces; }
int port_region_owns(const void* p) { return profileOwnedMatrix && p==profileOwnedMatrix; }
}
nlTaskManager* nlTaskManager::m_pInstance = nullptr;
s32 glGetCurrentFrame() { return currentFrame; }
u32 glx_GetTargetFPS() { return 60; }
extern u8 glx_bLoadingIndicator;
const glView* glViewGetState(eGLView) { return &viewState; }
void glViewGetViewMatrix(eGLView, nlMatrix4& out) { out = viewMatrix; }
void glViewGetProjectionMatrix(eGLView, nlMatrix4& out) { out = projectionMatrix; }
uintptr_t glGetIdentityMatrix() { return 1; }
void nlMultMatrices(nlMatrix4& out, const nlMatrix4& a, const nlMatrix4& b) {
    assert(profileOwnedMatrix); // Existing fixtures still reject unexpected concatenations.
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c) {
        float sum=0;for(unsigned k=0;k<4;++k)sum+=a.e[r*4+k]*b.e[k*4+c];out.e[r*4+c]=sum;
    }
}
void* glUserGetData(const void* handle) { return const_cast<void*>(handle); }
u32 glGetProgram(const char* name)
{
    ++programLookups;
    return fixture_program(name);
}
u32 glGetTexture(const char* name)
{
    if (shadowFixture)
        for (unsigned i = 0; i < shadowTextures.size(); ++i)
        {
            char expected[40];
            std::snprintf(expected, sizeof expected, "target/pshadow%02u", i);
            if (!std::strcmp(name, expected))
                return 100 + i;
        }
    return 77;
}
u32 glGetRasterState(unsigned long, eGLState state)
{
    switch (state)
    {
    case GLS_DepthTest:
    case GLS_DepthWrite:
    case GLS_DepthFunc:   return 1;
    case GLS_ColourWrite: return 3;
    case GLS_AlphaBlend:  return 1;
    default:              return 0;
    }
}
unsigned long glGetTextureState(unsigned long long, eGLTextureState state)
{
    if (state == GLTS_DiffuseLevel)
        return diffuseLevel;
    if (state == GLTS_ShadowLevel)
        return 21;
    if (state == GLTS_GlossLevel)
        return 42;
    return state < GLTS_DiffuseFilter ? GLTM_ClampClamp : GLTF_Linear;
}
nlVector4 glConstantGet(const char* name)
{
    nlVector4 result;
    nlVec4Set(result, 1, 1, 1, 1);
    if (!std::strcmp(name, "crowd/frame"))
    {
        ++crowdFrameLookups;
        result.x = crowdFrame;
    }
    else if (!std::strcmp(name, "lighting/range"))
    {
        ++rangeLookups;
        result.x = lightingRange;
    }
    else if (!std::strcmp(name, "lighting/ambient_colour"))
    {
        ++ambientLookups;
        nlVec4Set(result, lightingAmbient[0], lightingAmbient[1], lightingAmbient[2], lightingAmbient[3]);
    }
    else if (shadowFixture)
        for (unsigned i = 0; i < shadowTextures.size(); ++i)
        {
            char expected[40];
            std::snprintf(expected, sizeof expected, "target/pshadow_updated%02u", i);
            if (!std::strcmp(name, expected))
            {
                result.x = shadowTextures[i] ? 1.f : 0.f;
                break;
            }
        }
    return result;
}
DisplayList* dlGetStruct(uintptr_t address)
{
    return address == ListHandle ? &displayList : nullptr;
}
bool glxVitaResourceArenaOwns(const void*) { return arenaOwns; }
int glx_GetTexMarkerLevel() { return 3; }
PlatTexture* glx_GetTex(uintptr_t handle, bool, bool)
{
    ++materialTextureLookups;
    if (shadowFixture && handle >= 100 && handle < 100 + shadowTextures.size())
        return shadowTextures[handle - 100];
    return handle == 77 ? fileTexture : nullptr;
}
PlatTexture::~PlatTexture() = default;

// The runtime driver records texture resolution; GLX's cache has separate tests.
void glx_EnableMaterialTextureCache(bool) {}
void glx_InvalidateTextureHandles() {}
PlatTexture* glx_GetMaterialTex(uintptr_t handle) { return glx_GetTex(handle, false, false); }

namespace port::render
{
struct DeferredRenderer::Impl
{};
DeferredRenderer::DeferredRenderer() noexcept = default;
DeferredRenderer::~DeferredRenderer() = default;
bool DeferredRenderer::initialize(bool) noexcept { ++rendererInitializations; return true; }
bool DeferredRenderer::initialize_scene_program() noexcept { return true; }
uint32_t DeferredRenderer::create_texture(const TextureUpload&) noexcept
{
    if (failTextureUpload)
        return 0;
    lastCreatedTexture = nextId++;
    if (failTextureRegistration)
    {
        failTextureRegistration = false;
        failNextHeapAllocation = true;
    }
    return lastCreatedTexture;
}
uint32_t DeferredRenderer::create_target(uint32_t width, uint32_t height, TargetFormat format) noexcept
{
    targetDimensions.push_back({ width, height });
    targetFormats.push_back(format);
    if (failTarget)
        return 0;
    ++targetsCreated;
    return nextId++;
}
void DeferredRenderer::release_texture(uint32_t id) noexcept { releasedTextures.push_back(id); }
uint32_t DeferredRenderer::create_model(std::span<const ModelVertex> vertices,
                                        std::span<const uint16_t> indices) noexcept
{
    assert(vertices.size() == 3 && indices.size() == 3);
    lastCreatedVertices.assign(vertices.begin(), vertices.end());
    ++modelsCreated;
    lastCreatedModel = nextId++;
    if (failModelRegistration)
    {
        failModelRegistration = false;
        failNextHeapAllocation = true;
    }
    return lastCreatedModel;
}
void DeferredRenderer::release_model(uint32_t id) noexcept { releasedModels.push_back(id); }
bool DeferredRenderer::draw_scene(uint32_t model, std::span<const ModelVertex> vertices,
                                  std::span<const uint16_t> indices, const std::array<float, 16>& matrix,
                                  const Raster& raster, const SceneMaterial& material) noexcept
{
    lastDrawVertices.assign(vertices.begin(), vertices.end());
    lastDrawIndices.assign(indices.begin(), indices.end());
    submitted.push_back({ model, material, raster, matrix });
    return !failDraw;
}
bool DeferredRenderer::select_target(uint32_t target, bool, bool) noexcept
{
    selectedTargets.push_back(target);
    return true;
}
bool DeferredRenderer::copy_texture(uint32_t, std::array<float, 4>, bool) noexcept { return true; }
bool DeferredRenderer::copy_textures(std::span<const TextureCopy> copies) noexcept
{
    capturedCopies.insert(capturedCopies.end(), copies.begin(), copies.end());
    return !failCopy;
}
bool DeferredRenderer::clear_color(std::array<float, 4>, bool) noexcept { return !failClear; }
bool DeferredRenderer::clear_depth() noexcept { return !failClear; }
bool DeferredRenderer::set_black(bool black) noexcept
{
    presentationEvents.push_back(black ? 'B' : 'b');
    return !failBlack;
}
bool DeferredRenderer::set_loading(bool begin, const LoadingIndicator& indicator) noexcept
{
    ++loadingCalls;
    loadingActive = begin && !failLoading;
    if (begin && !failLoading)
        loadingDescriptor = indicator;
    return !failLoading;
}
bool DeferredRenderer::present_bitmap(const TextureUpload& image, std::array<float, 4> destination) noexcept
{
    ++bitmapCalls;
    bitmapFrameActive = frameActive;
    presentationEvents.push_back('M');
    bitmapUpload = image;
    bitmapDestination = destination;
    return !failBitmap;
}
bool DeferredRenderer::frame_active() const noexcept { return frameActive; }
static bool mockFrameAsync = false;
bool DeferredRenderer::begin_frame(FrameTimeline* capture, bool asynchronous) noexcept
{
    assert(!asynchronous || !capture);
    mockFrameAsync = asynchronous;
    lastBeginTimeline=capture;
    ++frameBegins;
    frameActive = true;
    return true;
}
bool DeferredRenderer::end_frame(bool present, bool asynchronous) noexcept
{
    assert(asynchronous == mockFrameAsync);
    lastBeginTimeline=nullptr;
    ++frameEnds;
    frameActive = false;
    lastPresent = present;
    return true;
}
bool DeferredRenderer::readback_presented_rgba8(std::vector<uint8_t>&) noexcept { return true; }
void DeferredRenderer::shutdown() noexcept { frameActive = false; loadingActive = false; lastBeginTimeline=nullptr; }
const char* DeferredRenderer::last_error() const noexcept { return "none"; }
int DeferredRenderer::error_code() const noexcept { return 0; }
DeferredRenderer::FrameTiming DeferredRenderer::completed_timing() const noexcept { return {}; }
uint64_t DeferredRenderer::pending_frontend_wait_us() const noexcept { return 0; }
}   // namespace port::render

namespace
{
void check_display_messages()
{
    namespace match = port::directmatch;
    using Access = port::directmatch::detail::MatchRuntimeTestAccess;
    std::array<uint8_t, 320 * 224 / 8> bits{};
    bits[0] = 0x81;
    bits[39] = 0x80;
    bits[40] = 0x02;
    bits.back() = 0x80;
    constexpr uint32_t colour = 0x7f123456u;
    match::shutdown();
    const auto beforeBegins = frameBegins, beforeEnds = frameEnds, beforeRetraces = retraces;
    const auto beforeDraws = submitted.size(), beforeTargets = selectedTargets.size();
    const auto beforeReleases = releasedTextures.size();
    const auto beforeTexture = lastCreatedTexture;
    match::draw_message_bitmap(320, 224, bits.data(), colour, true);
    assert(bitmapCalls && !bitmapFrameActive && !frameActive);
    assert(frameBegins == beforeBegins && frameEnds == beforeEnds && retraces == beforeRetraces);
    assert(submitted.size() == beforeDraws && selectedTargets.size() == beforeTargets);
    assert(releasedTextures.size() == beforeReleases && lastCreatedTexture == beforeTexture);
    assert((bitmapDestination == std::array<float, 4>{ 240, 136, 480, 272 }));
    assert(bitmapUpload.width == 320 && bitmapUpload.height == 224 && bitmapUpload.stride == 320);
    assert(bitmapUpload.mipCount == 1 && bitmapUpload.storage == port::nativegxm::TextureStorage::Rgba8);
    assert(bitmapUpload.pixels.size() == 320 * 224 * 4);
    const auto pixel = [](unsigned x, unsigned y) {
        const auto* p = bitmapUpload.pixels.data() + (y * 320 + x) * 4;
        return std::array<uint8_t, 4>{ p[0], p[1], p[2], p[3] };
    };
    const std::array<uint8_t, 4> foreground{ 0x12, 0x34, 0x56, 0x7f }, blank{};
    assert(pixel(0, 0) == foreground && pixel(7, 0) == foreground && pixel(319, 0) == foreground);
    assert(pixel(1, 1) == foreground && pixel(319, 223) == foreground);
    assert(pixel(1, 0) == blank && pixel(0, 1) == blank && pixel(318, 223) == blank);

    // Execute the production glx hook and the true/message/false sequence used
    // by TransitionTask. The bridge must not close or publish the active scene.
    assert(match::begin_frame());
    viewState.target = GLTG_Staging;
    match::begin_view(GLV_Transitions3D);
    const auto activeBegins = frameBegins, activeEnds = frameEnds, activeRetraces = retraces;
    const auto activeTargets = selectedTargets.size(), activeDraws = submitted.size();
    presentationEvents.clear();
    glxSwapSetBlack(true);
    match::draw_message_bitmap(320, 224, bits.data(), colour, false);
    glxSwapSetBlack(false);
    assert((presentationEvents == std::vector<char>{ 'B', 'M', 'b' }));
    assert(bitmapFrameActive && frameActive && frameBegins == activeBegins && frameEnds == activeEnds);
    assert(retraces == activeRetraces && selectedTargets.size() == activeTargets && submitted.size() == activeDraws);
    assert(bitmapDestination[0] == 240 && bitmapDestination[2] == 480 && bitmapDestination[3] == 272);
    assert(std::fabs(bitmapDestination[1] - 233.142857f) < .0001f);
    assert(match::end_frame() && lastPresent);

    const auto rejected = [&](unsigned width, unsigned height, const uint8_t* data) {
        assert(match::begin_frame());
        const auto invalidBefore = Access::invalid_count();
        const auto callsBefore = bitmapCalls;
        match::draw_message_bitmap(width, height, data, colour, true);
        assert(Access::invalid_count() == invalidBefore + 1 && bitmapCalls == callsBefore);
        assert(!std::strcmp(Access::failure(), "message_bitmap_dimensions"));
        assert(frameActive && match::end_frame() && !lastPresent);
    };
    rejected(0, 1, bits.data());
    rejected(8, 0, bits.data());
    rejected(8, 1, nullptr);
    rejected(15, 1, bits.data());
    rejected(968, 1, bits.data());
    rejected(8, 449, bits.data());
    logicalFrameWidth = 0;
    rejected(8, 1, bits.data());
    logicalFrameWidth = 640;

    assert(match::begin_frame());
    auto invalidBefore = Access::invalid_count();
    failBlack = true;
    glxSwapSetBlack(true);
    failBlack = false;
    assert(Access::invalid_count() == invalidBefore + 1);
    assert(!std::strcmp(Access::failure(), "present_black"));
    assert(match::end_frame() && !lastPresent);

    assert(match::begin_frame());
    invalidBefore = Access::invalid_count();
    failBitmap = true;
    match::draw_message_bitmap(320, 224, bits.data(), colour, true);
    failBitmap = false;
    assert(Access::invalid_count() == invalidBefore + 1);
    assert(!std::strcmp(Access::failure(), "message_bitmap_present"));
    assert(frameActive && match::end_frame() && !lastPresent);

    assert(match::begin_frame());
    invalidBefore = Access::invalid_count();
    const auto callsBeforeAllocationFailure = bitmapCalls;
    failNextHeapAllocation = true;
    match::draw_message_bitmap(320, 224, bits.data(), colour, true);
    assert(!failNextHeapAllocation && bitmapCalls == callsBeforeAllocationFailure);
    assert(Access::invalid_count() == invalidBefore + 1);
    assert(!std::strcmp(Access::failure(), "message_bitmap_allocation"));
    assert(frameActive && match::end_frame() && !lastPresent);

    // Recovery does not clear a previously discarded frame; a new frame does.
    assert(match::begin_frame());
    match::discard_present();
    match::draw_message_bitmap(320, 224, bits.data(), colour, true);
    assert(match::end_frame() && !lastPresent);
    assert(match::begin_frame());
    glxSwapSetBlack(false);
    match::draw_message_bitmap(320, 224, bits.data(), colour, true);
    assert(match::end_frame() && lastPresent);
    match::shutdown();
}

void check_loading_indicator()
{
    namespace match = port::directmatch;
    using Access = port::directmatch::detail::MatchRuntimeTestAccess;
    match::shutdown();
    const auto initializations = rendererInitializations;
    auto calls = loadingCalls;
    assert(match::set_loading({ true, false, false, false, 60, 0 }));
    glxLoadSaveState();
    glxLoadRestoreState();
    assert(loadingCalls == calls && rendererInitializations == initializations);
    assert(!glx_bLoadingIndicator && !loadingActive);

    const auto begins = frameBegins, ends = frameEnds, ticks = retraces;
    glxSwapLoading(true, false);
    assert(glx_bLoadingIndicator && loadingActive && loadingCalls == calls + 1);
    assert(rendererInitializations == initializations + 1);
    assert(loadingDescriptor.waitRetraces == 120 && loadingDescriptor.phaseRetraces == 19);
    assert(!loadingDescriptor.blackBackground);
    assert(frameBegins == begins && frameEnds == ends && retraces == ticks && !frameActive);
    for (unsigned i = 0; i < 2; ++i)
    {
        const auto& image = loadingDescriptor.images[i];
        const auto& expected = i ? port::loading::selectedOperations : port::loading::normalOperations;
        assert(image.width == 32 && image.height == 32 && image.stride == 32 && image.mipCount == 1);
        assert(port::nativegxm::texture_upload_bytes(image) == 4096);
        assert(image.pixels.size() == expected.size());
        assert(std::equal(image.pixels.begin(), image.pixels.end(), expected.begin()));
    }
    const auto mixed = (5 * 32 + 8) * 4;
    assert(loadingDescriptor.images[0].pixels[mixed + 3] == 170); // UV-only neighbour.
    assert(loadingDescriptor.images[1].pixels[mixed + 1] == 123);
    assert(loadingDescriptor.images[1].pixels[mixed + 2] == 140);
    glxLoadSaveState();
    assert(!glx_bLoadingIndicator && !loadingActive);
    glxLoadRestoreState();
    assert(glx_bLoadingIndicator && loadingActive && loadingDescriptor.blackBackground);
    assert(loadingDescriptor.waitRetraces == 0);
    glxSwapLoading(false, false);
    assert(!glx_bLoadingIndicator && !loadingActive);

    assert(match::begin_frame());
    const auto activeBegins = frameBegins, activeEnds = frameEnds, activeTicks = retraces;
    const auto targets = selectedTargets.size(), drawCount = submitted.size();
    logicalFrameWidth = 854;
    glxSwapLoading(true, true);
    logicalFrameWidth = 640;
    assert(loadingActive && glx_bLoadingIndicator && frameActive);
    assert(loadingDescriptor.rectangles[0][0] == 432 && loadingDescriptor.rectangles[2][0] == 504);
    assert(std::fabs(loadingDescriptor.rectangles[0][1] - 444.428571429f) < .001f);
    assert(!loadingDescriptor.blackBackground && loadingDescriptor.waitRetraces == 0);
    assert(frameBegins == activeBegins && frameEnds == activeEnds && retraces == activeTicks);
    assert(selectedTargets.size() == targets && submitted.size() == drawCount);
    glxSwapLoading(false, false);
    assert(match::end_frame() && lastPresent);

    assert(match::begin_frame());
    auto invalidBefore = Access::invalid_count();
    failLoading = true;
    glxSwapLoading(true, false);
    failLoading = false;
    assert(!glx_bLoadingIndicator && !loadingActive && frameActive);
    assert(Access::invalid_count() == invalidBefore + 1);
    assert(!std::strcmp(Access::failure(), "loading_indicator"));
    assert(match::end_frame() && !lastPresent);

    assert(match::begin_frame());
    glxSwapLoading(true, false);
    assert(loadingActive);
    invalidBefore = Access::invalid_count();
    calls = loadingCalls;
    failNextHeapAllocation = true;
    glxSwapLoading(true, true);
    assert(!failNextHeapAllocation && loadingCalls == calls + 1); // Disarm only.
    assert(!glx_bLoadingIndicator && !loadingActive && frameActive);
    assert(Access::invalid_count() == invalidBefore + 1);
    assert(!std::strcmp(Access::failure(), "loading_allocation"));
    assert(match::end_frame() && !lastPresent);

    assert(match::begin_frame());
    invalidBefore = Access::invalid_count();
    assert(!match::set_loading({ true, true, false, false, 0, 0 }));
    assert(Access::invalid_count() == invalidBefore + 1);
    assert(!std::strcmp(Access::failure(), "loading_dimensions"));
    assert(match::end_frame() && !lastPresent);

    assert(match::begin_frame());
    match::discard_present();
    glxSwapLoading(true, false);
    glxSwapLoading(false, false);
    assert(match::end_frame() && !lastPresent);
    match::shutdown();
}

void check_material_program_lookup()
{
    namespace match = port::directmatch;
    using Access = port::directmatch::detail::MatchRuntimeTestAccess;
    using Family = SceneAlphaProgram;
    struct ProgramCase
    {
        const char* name;
        Family family;
        bool lit, dirt;
        float scale;
    };
    constexpr ProgramCase programs[]{
        { "2d movie", Family::Movie, false, false, 1 },
        { "3d pointlit dirt", Family::Dirt, true, true, 1 },
        { "3d crowd", Family::Crowd, false, false, 1 },
        { "3d crowd lit", Family::Crowd, true, false, 1 },
        { "3d pointlit", Family::Standard, true, false, 1 },
        { "3d unlit 2x", Family::Standard, false, false, 2 },
        { "3d unknown", Family::Standard, false, false, 1 },
    };
    float positions[]{ 0, 0, 0, 1, 0, 0, 0, 1, 0 };
    float uv[]{ 0, 0, 1, 0, 0, 1 };
    glModelStream streams[]{ { reinterpret_cast<uintptr_t>(positions), 0, 12, 0, sizeof positions },
                             { reinterpret_cast<uintptr_t>(uv), 3, 8, 0, sizeof uv } };
    glModelPacket packet{};
    packet.numVertices = 3;
    packet.numStreams = 2;
    packet.streams = streams;
    packet.state.matrix = 1;
    for (auto& handle : packet.state.texture)
        handle = 77;
    PlatTexture texture;
    texture.m_Width = texture.m_Height = 8;
    texture.m_Levels = 1;
    texture.m_Format = GXTex_RGBA8;
    texture.m_NativeDataSize = 64;
    texture.m_OwnerTextureLevel = 3;
    texture.m_OwnerResourceLevel = 4;
    TextureUpload upload;
    upload.width = upload.height = 8;
    auto* const previousTexture = fileTexture;
    fileTexture = &texture;
    const auto lookupsBefore = programLookups;
    match::shutdown();
    for (unsigned restart = 0; restart < 2; ++restart)
    {
        assert(match::begin_frame() && match::admit_texture(&texture, upload));
        SceneAlphaState reference;
        for (const auto& program : programs)
        {
            packet.state.program = fixture_program(program.name);
            const auto run = [&](unsigned mask, unsigned long flags, bool accepted) {
                packet.state.texconfig = mask;
                packet.primType = accepted ? 0 : 6;
                crowdFrame += .03125f;
                lightingRange = lightingRange == 1.f ? 2.f : 1.f;
                lightingAmbient[0] += .0625f;
                reference.observe(flags, mask, program.family);
                reference.note_draw(mask, false, false, false);
                const auto before = submitted.size();
                const auto beforeCrowd = crowdFrameLookups, beforeRange = rangeLookups,
                           beforeAmbient = ambientLookups;
                match::on_packet(GLV_Characters, flags, &packet);
                assert(submitted.size() == before + unsigned(accepted));
                const auto state = Access::alpha_state();
                assert(state.program == reference.program && state.selectors == reference.selectors &&
                       state.tail == reference.tail && state.pending == reference.pending);
                assert(crowdFrameLookups == beforeCrowd + unsigned(accepted && program.family == Family::Crowd));
                assert(rangeLookups == beforeRange + unsigned(accepted && (mask & 32)));
                assert(ambientLookups == beforeAmbient + unsigned(accepted && program.lit));
                if (!accepted)
                    return;
                const auto& material = submitted.back().material;
                assert(material.lit == program.lit && material.material[1] == float(program.dirt));
                assert(material.controls[2] == ((mask & 32) ? lightingRange : program.scale));
                assert(material.controls[3] == (program.family == Family::Movie ? 2.f : program.family == Family::Crowd ? 1.f : 0.f));
                assert(material.effects[3] == (program.family == Family::Crowd ? crowdFrame : 0.f));
                assert(material.alpha == reference.resolve(1.f, 85.f / 255.f));
                if (program.lit)
                    assert(material.ambient == lightingAmbient);
            };
            run(0, 0x82, true);
            run(33, 0x82, true);
            // Program-family changes must still take effect before rejecting
            // geometry, and the next draw inherits the partial TEV update.
            run(7, 0x82, false);
            run(0, 0, true);
        }
        assert(match::end_frame());
        match::shutdown();
    }
    assert(programLookups == lookupsBefore);
    crowdFrame = .375f;
    lightingRange = 1.f;
    lightingAmbient = { 1, 1, 1, 1 };
    fileTexture = previousTexture;
}

void check_material_texture_partition()
{
    namespace match = port::directmatch;
    using Access = port::directmatch::detail::MatchRuntimeTestAccess;
    float positions[]{ 0, 0, 0, 1, 0, 0, 0, 1, 0 };
    float uv[]{ 0, 0, 1, 0, 0, 1 };
    glModelStream streams[]{ { reinterpret_cast<uintptr_t>(positions), 0, 12, 0, sizeof positions },
                             { reinterpret_cast<uintptr_t>(uv), 3, 8, 0, sizeof uv } };
    glModelPacket packet{};
    packet.numVertices = 3;
    packet.numStreams = 2;
    packet.streams = streams;
    packet.state.matrix = 1;
    packet.state.texconfig = 1;
    std::array<PlatTexture, 2> aliases;
    for (auto& texture : aliases)
    {
        texture.m_Width = texture.m_Height = 8;
        texture.m_Levels = 1;
        texture.m_Format = GXTex_RGBA8;
        texture.m_NativeDataSize = 64;
    }
    aliases[0].m_OwnerTextureLevel = 0;
    aliases[1].m_OwnerTextureLevel = 3;
    shadowTextures[2] = &aliases[0];
    shadowTextures[7] = &aliases[1];
    shadowFixture = true;
    match::shutdown();
    assert(match::begin_frame());
    const auto copiesBefore = capturedCopies.size();
    match::capture_shadow_atlas();
    assert(capturedCopies.size() == copiesBefore + 2);
    const auto atlas = capturedCopies[copiesBefore].destination;
    assert(atlas && capturedCopies[copiesBefore + 1].destination == atlas);
    const auto draw = [&](uintptr_t handle, uint32_t id, float partition) {
        packet.state.texture[0] = handle;
        const auto before = submitted.size();
        match::on_packet(GLV_Characters, 0, &packet);
        assert(submitted.size() == before + 1);
        assert(submitted.back().material.textures[0] == id);
        assert(submitted.back().material.material[2] == partition);
    };
    draw(102, atlas, 3);
    draw(107, atlas, 8);

    // Replacing one alias removes its atlas metadata; recapturing restores the
    // same shared GPU texture with this PlatTexture's own partition.
    TextureUpload upload;
    upload.width = upload.height = 8;
    assert(match::admit_texture(&aliases[1], upload));
    const auto privateTexture = lastCreatedTexture;
    assert(privateTexture != atlas);
    draw(107, privateTexture, 0);
    match::capture_shadow_atlas();
    assert(releasedTextures.back() == privateTexture);
    draw(107, atlas, 8);

    // Even a resident texture must pass its live header validation.
    aliases[0].m_Width = 0;
    packet.state.texture[0] = 102;
    auto before = submitted.size();
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.size() == before && !std::strcmp(Access::failure(), "material_texture"));
    aliases[0].m_Width = 8;
    draw(102, atlas, 3);

    const auto releasesBefore = releasedTextures.size();
    match::on_texture_release(0);
    assert(releasedTextures.size() == releasesBefore); // The atlas is shared.
    draw(102, atlas, 3);
    packet.state.texture[0] = 107;
    before = submitted.size();
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.size() == before); // Released file texture is not resident.
    match::capture_shadow_atlas();
    draw(107, atlas, 8);
    assert(match::end_frame());
    match::shutdown();
    shadowFixture = false;
    shadowTextures.fill(nullptr);
}

void check_skin_switching()
{
    namespace match = port::directmatch;
    const auto previousList = displayList;
    const auto previousOwnership = arenaOwns;
    float positions[]{ 1, 2, 3, -4, 5, 6, 7, 8, -9 };
    uint16_t indices[]{ 0, 1, 2 };
    const uint8_t stitching[]{ 0, 0, 0 };
    glModelStream streams[]{ { reinterpret_cast<uintptr_t>(positions), 0, 12, 0, sizeof positions },
                             {} };
    streams[1].id = 12;
    glModelPacket packet{};
    packet.numVertices = 3;
    packet.streams = streams;
    packet.state.matrix = 1;
    struct Skin { uint32_t count; GLSkinUserData bone; } skin{};
    skin.bone.reg = -96;
    skin.bone.mat[0] = skin.bone.mat[5] = skin.bone.mat[10] = 1;
    std::array<uintptr_t, GLUD_Num> userdata{};
    userdata[GLUD_Skin] = reinterpret_cast<uintptr_t>(&skin);
    packet.userData = reinterpret_cast<uintptr_t>(userdata.data());
    displayList.list = indices;
    displayList.size = sizeof indices;
    for (bool resident : { false, true })
    {
        match::shutdown();
        arenaOwns = resident;
        assert(match::begin_frame());
        const auto capture = [&](unsigned streamsCount) {
            packet.numStreams = streamsCount;
            packet.indexBuffer = reinterpret_cast<uintptr_t>(indices);
            match::on_display_list_created(&packet, ListHandle, 3);
            packet.indexBuffer = ListHandle;
        };
        const auto unskinned = [&] {
            skin.count = 0; // Invalid skin userdata is irrelevant without PN.
            capture(1);
            const auto before = submitted.size();
            match::on_packet(GLV_Characters, 0, &packet);
            assert(submitted.size() == before + 1 && bool(submitted.back().model) == resident);
            const auto& vertices = resident ? lastCreatedVertices : lastDrawVertices;
            assert(vertices.size() == 3);
            for (unsigned vertex = 0; vertex < 3; ++vertex)
            {
                assert(vertices[vertex].boneSlot == 0);
                for (unsigned axis = 0; axis < 3; ++axis)
                    assert(vertices[vertex].position[axis] == positions[vertex * 3 + axis]);
            }
        };
        unskinned();
        skin.count = 1;
        skin.bone.mat[3] = 11;
        capture(2);
        match::on_stitch_selectors(&packet, stitching, 3);
        const auto pnDraw = submitted.size();
        match::on_packet(GLV_Characters, 0, &packet);
        assert(submitted.size() == pnDraw + 1 && bool(submitted.back().model) == resident);
        assert(submitted.back().material.bones[3][3] == 11);
        const auto& vertices = resident ? lastCreatedVertices : lastDrawVertices;
        assert(vertices.size() == 3 && vertices[0].boneSlot == 1);
        const auto pnModel = submitted.back().model;
        skin.bone.mat[3] = 22;
        match::on_packet(GLV_Characters, 0, &packet);
        assert(submitted.size() == pnDraw + 2 && submitted.back().model == pnModel);
        assert(submitted.back().material.bones[3][3] == 22);
        unskinned();
        assert(submitted[pnDraw].material.bones[3][3] == 11);
        assert(submitted[pnDraw + 1].material.bones[3][3] == 22);
        assert(match::end_frame());
    }
    match::shutdown();
    arenaOwns = previousOwnership;
    displayList = previousList;
}

void check_invalidation_bounds()
{
    namespace match = port::directmatch;
    using Access = port::directmatch::detail::MatchRuntimeTestAccess;
    using Bounds = Access::Bounds;
    using Range = Access::Range;
    const auto empty = [](Bounds b) { return b.first > b.last; };
    const auto equal = [](Bounds a, Bounds b) { return a.first == b.first && a.last == b.last; };

    // Compare real endpoint arithmetic against a wider mathematical interval,
    // including empty spans, the final byte and lengths crossing UINTPTR_MAX.
    constexpr uintptr_t addresses[]{ 0, 1, 7, 16, UINTPTR_MAX - 8, UINTPTR_MAX - 1, UINTPTR_MAX };
    constexpr size_t lengths[]{ 0, 1, 2, 8, SIZE_MAX };
    const auto end = [](Range r) {
        const auto sum = static_cast<unsigned __int128>(r.address) + r.size - 1;
        return std::min(sum, static_cast<unsigned __int128>(UINTPTR_MAX));
    };
    for (const auto a : addresses)
        for (const auto n : lengths)
        {
            Bounds bounds;
            const Range stored{ a, n };
            bounds.include(stored);
            for (const auto b : addresses)
                for (const auto m : lengths)
                {
                    const Range written{ b, m };
                    const bool expected = n && m && a <= end(written) && b <= end(stored);
                    assert(bounds.intersects(written) == expected);
                    assert(Access::exact_overlap(stored, written) == expected);
                }
        }
    Bounds sparse;
    sparse.include({ 100, 10 });
    sparse.include({ 200, 10 });
    sparse.include({ 0, 0 });
    sparse.include({ UINTPTR_MAX, 0 });
    assert(sparse.first == 100 && sparse.last == 209 && sparse.intersects({ 150, 1 }));
    assert(!sparse.intersects({ 99, 1 }) && !sparse.intersects({ 210, 1 }));

    assert(empty(Access::geometry_bounds()) && empty(Access::texture_bounds()));
    diagnostics = false;
    std::array<float, 64> vertexMemory{};
    auto* positions = vertexMemory.data() + 4;
    auto* uv = positions + 9;
    auto* otherPositions = vertexMemory.data() + 32;
    auto* otherUv = otherPositions + 9;
    positions[3] = positions[7] = otherPositions[3] = otherPositions[7] = 1;
    uv[2] = uv[5] = otherUv[2] = otherUv[5] = 1;
    glModelStream streams[]{ { reinterpret_cast<uintptr_t>(positions), 0, 12, 0, 9 * sizeof(float) },
                             { reinterpret_cast<uintptr_t>(uv), 3, 8, 0, 6 * sizeof(float) } };
    glModelStream otherStreams[]{ streams[0], streams[1] };
    otherStreams[0].address = reinterpret_cast<uintptr_t>(otherPositions);
    otherStreams[1].address = reinterpret_cast<uintptr_t>(otherUv);
    uint16_t indices[]{ 0, 1, 2 };
    displayList.list = indices;
    displayList.size = sizeof indices;
    glModelPacket packet{};
    packet.numVertices = 3;
    packet.numStreams = 2;
    packet.streams = streams;
    packet.state.texconfig = 1;
    packet.state.texture[0] = 77;
    packet.state.program = glGetProgram("3d");
    packet.state.matrix = 1;
    const auto register_source = [&] {
        packet.indexBuffer = reinterpret_cast<uintptr_t>(indices);
        match::on_display_list_created(&packet, ListHandle, 3);
        packet.indexBuffer = ListHandle;
    };
    const auto draw = [&] {
        const auto before = submitted.size();
        match::on_packet(GLV_Characters, 0, &packet);
        assert(submitted.size() == before + 1);
        return submitted.back();
    };
    const auto write = [](uintptr_t address, size_t bytes) {
        match::notify_write(reinterpret_cast<const void*>(address), bytes);
    };
    struct TextureMemory
    {
        std::array<uint8_t, 4> before{};
        std::array<uint8_t, 64> pixels{};
        std::array<uint8_t, 16> gap{};
        std::array<uint16_t, 256> palette{};
        std::array<uint8_t, 4> after{};
    } textureMemory;
    PlatTexture generated, native, captured;
    for (auto* texture : { &generated, &native, &captured })
    {
        texture->m_Width = texture->m_Height = 8;
        texture->m_Levels = 1;
        texture->m_Format = GXTex_RGB565;
        texture->m_OwnerTextureLevel = 3;
    }
    generated.m_Format = GXTex_CI8;
    generated.m_SwizzledData = textureMemory.pixels.data();
    generated.m_PaletteData = textureMemory.palette.data();
    generated.m_nPaletteEntries = textureMemory.palette.size();
    native.m_NativeDataSize = 64;
    native.m_OwnerResourceLevel = 4;
    TextureUpload upload;
    upload.width = upload.height = 8;
    assert(match::begin_frame());
    assert(match::admit_texture(&native, upload));
    const auto nativeId = lastCreatedTexture;
    fileTexture = &captured;
    match::capture_texture(77, 0, 0, 8, 8);
    const auto capturedId = capturedCopies.back().destination;
    assert(empty(Access::texture_bounds()));
    fileTexture = &generated;
    register_source();

    // A failed map insertion cannot widen either superset or retain the new
    // GPU resource. The second attempt admits the texture but fails the model.
    failTextureRegistration = true;
    const auto beforeFailures = submitted.size();
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.size() == beforeFailures && releasedTextures.back() == lastCreatedTexture);
    assert(empty(Access::geometry_bounds()) && empty(Access::texture_bounds()));
    assert(!failNextHeapAllocation && match::end_frame() && !lastPresent);
    assert(match::begin_frame());
    failModelRegistration = true;
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.size() == beforeFailures && releasedModels.back() == lastCreatedModel);
    assert(empty(Access::geometry_bounds()) && !empty(Access::texture_bounds()));
    assert(!failNextHeapAllocation && match::end_frame() && !lastPresent);
    assert(match::begin_frame());
    auto current = draw();
    assert(current.model && current.material.textures[0]);
    const auto geometryFirst = reinterpret_cast<uintptr_t>(positions);
    const auto geometryLast = reinterpret_cast<uintptr_t>(uv) + 6 * sizeof(float) - 1;
    const auto textureFirst = reinterpret_cast<uintptr_t>(textureMemory.pixels.data());
    const auto textureLast = reinterpret_cast<uintptr_t>(textureMemory.palette.data()) +
                             sizeof textureMemory.palette - 1;
    assert(Access::geometry_bounds().first == geometryFirst &&
           Access::geometry_bounds().last == geometryLast);
    assert(Access::texture_bounds().first == textureFirst &&
           Access::texture_bounds().last == textureLast);

    // Empty writes, distant writes and the byte immediately outside either
    // range preserve both cached objects. Pointer values are never dereferenced.
    match::notify_write(nullptr, 10);
    match::notify_write(positions, 0);
    write(1, 16);
    write(UINTPTR_MAX - 2, 8);
    write(geometryFirst - 1, 1);
    write(geometryLast + 1, 1);
    write(textureFirst - 1, 1);
    write(textureLast + 1, 1);
    auto next = draw();
    assert(next.model == current.model && next.material.textures[0] == current.material.textures[0]);
    // The gap between pixels and palette is inside the coarse bounds only.
    write(reinterpret_cast<uintptr_t>(textureMemory.gap.data()), textureMemory.gap.size());
    next = draw();
    assert(next.model == current.model && next.material.textures[0] == current.material.textures[0]);
    write(geometryLast, 1);
    next = draw();
    assert(next.model != current.model && next.material.textures[0] == current.material.textures[0]);
    current = next;
    write(textureFirst + textureMemory.pixels.size() - 1, 1);
    next = draw();
    assert(next.model == current.model && next.material.textures[0] != current.material.textures[0]);
    current = next;
    write(textureLast, 1);
    next = draw();
    assert(next.model == current.model && next.material.textures[0] != current.material.textures[0]);
    current = next;
    // A range crossing the first byte, even late in a disjoint batch, is live.
    for (unsigned i = 0; i < 32; ++i)
        write(1, 16);
    write(geometryFirst - 2, 3);
    next = draw();
    assert(next.model != current.model && next.material.textures[0] == current.material.textures[0]);
    current = next;

    packet.streams = otherStreams;
    const auto other = draw();
    packet.streams = streams;
    const auto wideBounds = Access::geometry_bounds();
    write(reinterpret_cast<uintptr_t>(vertexMemory.data() + 25), sizeof(float));
    next = draw();
    assert(next.model == current.model);
    assert(match::end_frame());
    write(geometryFirst, 1);
    assert(match::begin_frame());
    assert(equal(Access::geometry_bounds(), wideBounds));
    packet.streams = otherStreams;
    assert(draw().model == other.model);
    packet.streams = streams;
    current = draw();

    // A full queue still honors exact disjointness; one further notification
    // forces the established full invalidation, including every resident model.
    auto modelReleases = releasedModels.size(), textureReleases = releasedTextures.size();
    for (unsigned i = 0; i < 4096; ++i)
        write(1, 16);
    next = draw();
    assert(next.model == current.model && next.material.textures[0] == current.material.textures[0]);
    assert(releasedModels.size() == modelReleases && releasedTextures.size() == textureReleases);
    for (unsigned i = 0; i < 4097; ++i)
        write(1, 16);
    next = draw();
    assert(next.model != current.model && next.material.textures[0] != current.material.textures[0]);
    assert(releasedModels.size() == modelReleases + 2 && releasedTextures.size() == textureReleases + 1);
    current = next;
    fileTexture = &native;
    assert(draw().material.textures[0] == nativeId);
    fileTexture = &captured;
    assert(draw().material.textures[0] == capturedId);
    fileTexture = &generated;

    // Releasing the owners and reusing the same object/array addresses registers
    // fresh bounds. No pointer identity from the prior generation is trusted.
    match::on_resource_release(2);
    assert(empty(Access::geometry_bounds()));
    match::on_texture_release(0);
    assert(empty(Access::texture_bounds()));
    positions[0] = 7;
    register_source();
    next = draw();
    assert(next.model != current.model && next.material.textures[0] != current.material.textures[0]);
    assert(lastCreatedVertices[0].position[0] == 7);
    current = next;
    write(geometryFirst, 1);
    write(textureLast, 1);
    next = draw();
    assert(next.model != current.model && next.material.textures[0] != current.material.textures[0]);
    assert(match::end_frame());

    // Pending notifications keep their existing drain semantics across shutdown;
    // all cached bounds disappear and the next lifetime starts from its own data.
    write(geometryFirst, sizeof(float));
    match::shutdown();
    assert(empty(Access::geometry_bounds()) && empty(Access::texture_bounds()));
    modelReleases = releasedModels.size();
    textureReleases = releasedTextures.size();
    assert(match::begin_frame());
    assert(releasedModels.size() == modelReleases && releasedTextures.size() == textureReleases);
    register_source();
    next = draw();
    assert(next.model && next.material.textures[0]);
    assert(!Access::geometry_bounds().intersects({ reinterpret_cast<uintptr_t>(otherPositions), 1 }));
    write(geometryLast, 1);
    assert(draw().model != next.model);
    assert(match::end_frame());
    match::shutdown();
    assert(empty(Access::geometry_bounds()) && empty(Access::texture_bounds()));
    fileTexture = nullptr;
}
}   // namespace

#include "frame_timeline_net_cases.inc"
#include "frame_timeline_runtime_cases.inc"
int main()
{
    namespace match = port::directmatch;
    namespace fs = std::filesystem;
    const auto workingDirectory = fs::current_path();
    const auto temporaryDirectory = fs::temp_directory_path() /
        ("strikers-frontend-" + std::to_string(getpid()));
    fs::create_directories(temporaryDirectory / "ux0:data/strikersVita");
    fs::current_path(temporaryDirectory);
    identity(viewMatrix);
    identity(projectionMatrix);
    viewState.vpWidth = 640;
    viewState.vpHeight = 448;
    float positions[]{ 0, 0, 0, 1, 0, 0, 0, 1, 0 };
    float uv[]{ 0, 0, 1, 0, 0, 1 };
    glModelStream streams[]{ { reinterpret_cast<uintptr_t>(positions), 0, 12, 0, sizeof positions },
                             { reinterpret_cast<uintptr_t>(uv), 3, 8, 0, sizeof uv } };
    glModelPacket packet{};
    packet.numVertices = 3;
    packet.numStreams = 2;
    packet.streams = streams;
    packet.state.texconfig = 1;
    packet.state.texture[0] = 77;
    packet.state.program = glGetProgram("3d crowd lit");
    packet.state.matrix = 1;
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.empty());
    match::enable(true);
    assert(match::enabled() && match::uses_own_renderer());

    PlatTexture texture;
    fileTexture = &texture;
    texture.m_Width = texture.m_Height = 8;
    texture.m_Levels = 1;
    texture.m_Format = GXTex_RGBA8;
    texture.m_NativeDataSize = 64;
    texture.m_OwnerTextureLevel = 3;
    texture.m_OwnerResourceLevel = 4;
    TextureUpload upload;
    upload.width = upload.height = 8;
    assert(match::admit_texture(&texture, upload));
    assert(match::begin_frame());
    std::array<uintptr_t, GLUD_Num> userdata{};
    uint8_t tint[]{ 255, 128, 64, 32 };
    float alpha = .5f;
    userdata[GLUD_ConstantColour] = reinterpret_cast<uintptr_t>(tint);
    packet.userData = reinterpret_cast<uintptr_t>(userdata.data());
    match::set_fog(2, 20, true, 1, { .1f, .2f, .3f, 1 });
    const auto programLookupsBeforeFirstPacket = programLookups;
    match::on_packet(GLV_Characters, 0, &packet);
    assert(programLookups == programLookupsBeforeFirstPacket + 6);
    assert(submitted.size() == 1);
    const auto& first = submitted.back();
    assert(first.model == 0 && first.material.textures[0] != 0 && first.material.lit);
    assert(first.material.controls[3] == 1 && first.material.effects[3] == .375f);
    assert(first.material.effects[0] == 1 && first.material.tint[3] == 32.f / 255);
    assert(first.material.shadow[0] == 85.f / 255 && first.material.shadowAdd[0] == 170.f / 255);
    assert(first.material.fog[2] == 1 && first.material.viewport[2] == 960);
    assert(first.raster.depthTest && first.raster.depthWrite && !first.raster.writeAlpha);

    userdata[GLUD_Translucent] = reinterpret_cast<uintptr_t>(&alpha);
    userdata[GLUD_NoFog] = reinterpret_cast<uintptr_t>(&alpha);
    match::on_packet(GLV_ShadowTexture, 0, &packet);
    assert(submitted.back().material.effects[1] == 32.f / 255);
    assert(submitted.back().material.fog[2] == 0 && submitted.back().raster.writeAlpha);
    userdata[GLUD_ConstantColour] = 0;
    match::on_packet(GLV_ShadowTexture, 0, &packet);
    assert(submitted.back().material.effects[1] == 127.f / 255);
    userdata[GLUD_ConstantColour] = reinterpret_cast<uintptr_t>(tint);
    userdata[GLUD_NoRasterizedAlpha] = reinterpret_cast<uintptr_t>(&alpha);
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.back().material.effects[1] == 1);

    // Register the immutable source, then reuse its model until a write or
    // resource release invalidates it. Draw order and per-packet alpha remain live.
    uint16_t indices[]{ 0, 1, 2 };
    displayList.list = indices;
    displayList.size = sizeof indices;
    packet.indexBuffer = reinterpret_cast<uintptr_t>(indices);
    match::on_display_list_created(&packet, ListHandle, 3);
    packet.indexBuffer = ListHandle;
    match::on_packet(GLV_Characters, 0, &packet);
    const auto resident = submitted.back().model;
    assert(resident && modelsCreated == 1);
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.back().model == resident && modelsCreated == 1);
    match::notify_write(positions, sizeof(float));
    match::on_packet(GLV_Characters, 0, &packet);
    assert(releasedModels.back() == resident && modelsCreated == 2);
    assert(submitted.back().model != resident);

    const auto count = submitted.size();
    packet.primType = 6;
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.size() == count);
    packet.primType = 0;
    projectionMatrix.e[0] = std::numeric_limits<float>::quiet_NaN();
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.size() == count);
    identity(projectionMatrix);
    match::on_resource_release(2);
    assert(releasedModels.size() == 2 && releasedTextures.size() == 1);
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.size() == count);

    // A resident skinned mesh keeps its PN selectors, while every draw owns
    // the current pose. Rebuilding after invalidation still validates the
    // decoder palette, including duplicate selectors and invalid registers.
    assert(match::admit_texture(&texture, upload));
    glModelStream skinStreams[]{ streams[0], streams[1], {} };
    skinStreams[2].id = 12;
    packet.streams = skinStreams;
    packet.numStreams = 3;
    packet.indexBuffer = reinterpret_cast<uintptr_t>(indices);
    match::on_display_list_created(&packet, ListHandle, 3);
    packet.indexBuffer = ListHandle;
    const uint8_t stitching[]{ 0, 0, 0 };
    match::on_stitch_selectors(&packet, stitching, 3);
    struct Skin
    {
        uint32_t count;
        GLSkinUserData bones[32];
    } skin{};
    skin.count = 1;
    skin.bones[0].reg = -96;
    skin.bones[0].mat[0] = skin.bones[0].mat[5] = skin.bones[0].mat[10] = 1;
    skin.bones[0].mat[3] = 10;
    userdata[GLUD_Skin] = reinterpret_cast<uintptr_t>(&skin);
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.size() == count + 1 && modelsCreated == 3);
    const auto skinnedModel = submitted.back().model;
    assert(skinnedModel && submitted.back().material.bones[3][3] == 10);
    assert(lastCreatedVertices[0].boneSlot == 1 && lastCreatedVertices[0].position[0] == 0);
    skin.bones[0].mat[3] = 20;
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.size() == count + 2 && modelsCreated == 3);
    assert(submitted.back().model == skinnedModel && submitted.back().material.bones[3][3] == 20);
    assert(submitted[submitted.size() - 2].material.bones[3][3] == 10);
    skin.bones[0].reg = std::numeric_limits<int>::max();
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.size() == count + 2 && modelsCreated == 3);
    skin.bones[0].reg = -96;
    match::notify_write(positions, sizeof(float));
    skin.count = 2;
    skin.bones[1] = skin.bones[0];
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.size() == count + 2 && modelsCreated == 3);
    skin.count = 1;
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.size() == count + 3 && modelsCreated == 4);
    assert(submitted.back().model != skinnedModel && submitted.back().material.bones[3][3] == 20);

    // Retail assets need 10..13 rigid bones (Birdo reaches selector 39), while
    // the existing native userdata contract allows 32. Exercise the complete
    // registry -> palette -> resident GPU-vertex path, with slot 0 reserved.
    for (const unsigned boneCount : { 10u, 11u, 12u, 13u, 32u })
    {
        skin.count = boneCount;
        for (unsigned i = 0; i < boneCount; ++i)
        {
            skin.bones[i] = {};
            skin.bones[i].reg = int(i) * 3 - 96;
            skin.bones[i].mat[0] = skin.bones[i].mat[5] = skin.bones[i].mat[10] = 1;
            skin.bones[i].mat[3] = float(100 + i);
        }
        const uint8_t extendedStitch[]{ 0, uint8_t(boneCount - 1), 9 };
        match::on_stitch_selectors(&packet, extendedStitch, 3);
        const auto beforeExtendedDraw = submitted.size();
        const auto beforeExtendedModel = modelsCreated;
        match::on_packet(GLV_Characters, 0, &packet);
        assert(submitted.size() == beforeExtendedDraw + 1);
        assert(modelsCreated == beforeExtendedModel + 1);
        const auto extendedModel = submitted.back().model;
        assert(lastCreatedVertices[0].boneSlot == 1);
        assert(lastCreatedVertices[1].boneSlot == float(boneCount));
        assert(lastCreatedVertices[2].boneSlot == 10);
        assert(submitted.back().material.bones[boneCount * 3][3] == float(99 + boneCount));
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned col = 0; col < 4; ++col)
                assert(submitted.back().material.bones[row][col] == float(row == col));

        // Reusing the VBO still uploads its current high-slot pose, without
        // mutating a material already owned by the deferred draw.
        const auto originalTranslation = skin.bones[boneCount - 1].mat[3];
        skin.bones[boneCount - 1].mat[3] += 20;
        match::on_packet(GLV_Characters, 0, &packet);
        assert(submitted.size() == beforeExtendedDraw + 2);
        assert(modelsCreated == beforeExtendedModel + 1 && submitted.back().model == extendedModel);
        assert(submitted.back().material.bones[boneCount * 3][3] == originalTranslation + 20);
        assert(submitted[beforeExtendedDraw].material.bones[boneCount * 3][3] == originalTranslation);

        // Validation cannot disappear on a resident hit. All rejected palettes
        // preserve the existing model and produce no additional GPU draw.
        const auto accepted = submitted.size();
        auto& lastBone = skin.bones[boneCount - 1];
        const auto validRegister = lastBone.reg;
        for (const int badRegister : { -99, -95, 0, std::numeric_limits<int>::max(), -96 })
        {
            lastBone.reg = badRegister;
            match::on_packet(GLV_Characters, 0, &packet);
            assert(submitted.size() == accepted && modelsCreated == beforeExtendedModel + 1);
        }
        lastBone.reg = validRegister;
        lastBone.mat[3] = std::numeric_limits<float>::quiet_NaN();
        match::on_packet(GLV_Characters, 0, &packet);
        assert(submitted.size() == accepted && modelsCreated == beforeExtendedModel + 1);
        lastBone.mat[3] = originalTranslation + 20;
        for (const unsigned badCount : { 0u, 33u })
        {
            skin.count = badCount;
            match::on_packet(GLV_Characters, 0, &packet);
            assert(submitted.size() == accepted && modelsCreated == beforeExtendedModel + 1);
        }
        skin.count = boneCount;
        if (boneCount < 32)
        {
            // A different, individually valid register cannot substitute for
            // the high slot already referenced by the resident vertices.
            lastBone.reg = int(boneCount) * 3 - 96;
            match::on_packet(GLV_Characters, 0, &packet);
            assert(submitted.size() == accepted && modelsCreated == beforeExtendedModel + 1);
            lastBone.reg = validRegister;
        }
        skin.count = 1;
        match::on_packet(GLV_Characters, 0, &packet);
        assert(submitted.size() == accepted && modelsCreated == beforeExtendedModel + 1);
        skin.count = boneCount;
        match::on_packet(GLV_Characters, 0, &packet);
        assert(submitted.size() == accepted + 1 && submitted.back().model == extendedModel);
    }

    // Original transition packets use Position4 (14), not the weights stream
    // (13), and line strips must reach the renderer as ordered line segments.
    float outlinePositions[]{ 0, 0, 0, 2, 1, 0, 0, 1, 1, 1, 0, .5f, 0, 1, 0, 1 };
    glModelStream outlineStream{ reinterpret_cast<uintptr_t>(outlinePositions),
                                  14, 16, 0, sizeof outlinePositions };
    glModelPacket outlinePacket = packet;
    outlinePacket.indexBuffer = 0;
    outlinePacket.streams = &outlineStream;
    outlinePacket.numStreams = 1;
    outlinePacket.numVertices = 4;
    outlinePacket.primType = 5;
    const auto beforeOutline = submitted.size();
    match::on_packet(GLV_Transitions3D, 0, &outlinePacket);
    assert(submitted.size() == beforeOutline + 1 && submitted.back().raster.primitive == 1);
    assert(lastDrawIndices == std::vector<uint16_t>({ 0, 1, 1, 2, 2, 3 }));
    assert(lastDrawVertices[0].position[3] == 2 && lastDrawVertices[2].position[3] == .5f);
    outlinePacket.numVertices = 2;
    outlinePacket.primType = 4;
    match::on_packet(GLV_Transitions3D, 0, &outlinePacket);
    assert(lastDrawIndices == std::vector<uint16_t>({ 0, 1 }));
    assert(submitted.size() == beforeOutline + 2 && submitted.back().raster.primitive == 1);
    outlineStream.id = 13;
    match::on_packet(GLV_Transitions3D, 0, &outlinePacket);
    assert(submitted.size() == beforeOutline + 2);

    // ScreenGrab owns the requested physical capture size even when the
    // original texture has different logical dimensions. Equal sizes reuse it.
    texture.m_OwnerTextureLevel = 0;
    texture.m_Format = GXTex_RGB565;
    match::capture_texture(77, 10, 28, 100, 56, false, 320, 224, true);
    assert((targetDimensions.back() == std::array<uint32_t, 2>{ 320, 224 }));
    assert(targetFormats.back() == TargetFormat::Rgb565);
    const auto firstCapture = capturedCopies.back().destination;
    const auto firstCaptureCount = targetsCreated;
    assert((capturedCopies.back().sourceRect == std::array<float, 4>{ 15, 34, 150, 68 }));
    assert(capturedCopies.back().clearSource && !capturedCopies.back().intensity);
    assert((capturedCopies.back().clearColor == std::array<float, 4>{ 0, 0, 0, 64.f / 255.f }));
    const std::array<float, 4> sceneClear{ .125f, .25f, .5f, .75f };
    match::clear_scene(sceneClear);
    match::capture_texture(77, 10, 28, 100, 56, true, 320, 224);
    assert(targetsCreated == firstCaptureCount && capturedCopies.back().destination == firstCapture);
    assert(!capturedCopies.back().clearSource && capturedCopies.back().intensity);
    assert(capturedCopies.back().clearColor == sceneClear);
    match::capture_texture(77, 0, 0, 640, 448, false, 160, 112, true);
    const auto smallerCapture = capturedCopies.back().destination;
    assert(smallerCapture != firstCapture && releasedTextures.back() == firstCapture);
    assert((targetDimensions.back() == std::array<uint32_t, 2>{ 160, 112 }));
    texture.m_Format = GXTex_RGBA8;
    match::capture_texture(77, 0, 0, 640, 448, false, 160, 112);
    const auto resizedCapture = capturedCopies.back().destination;
    assert(resizedCapture != smallerCapture && releasedTextures.back() == smallerCapture);
    assert(targetFormats.back() == TargetFormat::Rgba8);
    // Capturing under a newer marker does not shorten the texture owner's
    // lifetime. Rewinding that marker must preserve the GPU-captured image.
    const auto releasesBeforeRewind = releasedTextures.size();
    match::on_texture_release(0);
    match::on_packet(GLV_Characters, 0, &packet);
    assert(releasedTextures.size() == releasesBeforeRewind);
    assert(submitted.back().material.textures[0] == resizedCapture);

    currentFrame = 60;
    assert(match::end_frame() && lastPresent && retraces == 1);
    // Quiet rendering, including rejected packets and invalid matrices, must
    // not create diagnostic files. Explicit diagnostics still publish status.
    assert(fs::is_empty("ux0:data/strikersVita"));
    assert(match::begin_frame());
    match::discard_present();
    assert(match::end_frame() && !lastPresent && retraces == 2);
    diagnostics = true;
    assert(match::begin_frame());
    assert(match::end_frame() && lastPresent && retraces == 3);
    assert(fs::exists("ux0:data/strikersVita/vitagl_match_status.csv"));

    // A failed resize keeps the previous target, and neither malformed input
    // nor a failed copy/clear can present a partially changed frame.
    assert(match::begin_frame());
    const auto releasesBeforeFailure = releasedTextures.size();
    const auto copiesBeforeFailure = capturedCopies.size();
    failTarget = true;
    match::capture_texture(77, 0, 0, 640, 448, false, 80, 56, true);
    failTarget = false;
    assert(capturedCopies.size() == copiesBeforeFailure &&
           releasedTextures.size() == releasesBeforeFailure);
    assert(match::end_frame() && !lastPresent);
    assert(match::begin_frame());
    const auto targetsBeforeRetry = targetsCreated;
    match::capture_texture(77, 0, 0, 640, 448, false, 160, 112);
    assert(targetsCreated == targetsBeforeRetry && capturedCopies.back().destination == resizedCapture);
    assert(match::end_frame() && lastPresent);
    for (unsigned invalidCapture = 0; invalidCapture < 4; ++invalidCapture)
    {
        assert(match::begin_frame());
        const auto targetsBefore = targetDimensions.size(), copiesBefore = capturedCopies.size();
        if (invalidCapture == 0)
            match::capture_texture(77, 0, 0, 640, 448, false, 160, 0);
        else if (invalidCapture == 1)
            match::capture_texture(77, 639, 0, 2, 448);
        else if (invalidCapture == 2)
            match::capture_texture(77, 0, 448, 640, 1);
        else
        {
            texture.m_Format = GXTex_RGB5A3;
            match::capture_texture(77, 0, 0, 640, 448);
            texture.m_Format = GXTex_RGBA8;
        }
        assert(targetDimensions.size() == targetsBefore && capturedCopies.size() == copiesBefore);
        assert(match::end_frame() && !lastPresent);
    }
    assert(match::begin_frame());
    failCopy = true;
    match::capture_texture(77, 0, 0, 640, 448, false, 160, 112);
    failCopy = false;
    assert(match::end_frame() && !lastPresent);
    for (bool depthOnly : { false, true })
    {
        assert(match::begin_frame());
        failClear = true;
        if (depthOnly)
            match::clear_depth();
        else
            match::clear_scene(sceneClear);
        failClear = false;
        assert(match::end_frame() && !lastPresent);
    }
    assert(match::begin_frame());
    failTarget = true;
    match::capture_shadow_atlas();
    failTarget = false;
    assert(match::end_frame() && !lastPresent);

    assert(match::begin_frame());
    match::notify_write(positions, sizeof(float));
    failModelRegistration = true;
    const auto beforeFailedModel = submitted.size();
    match::on_packet(GLV_Characters, 0, &packet);
    const auto orphanModel = lastCreatedModel;
    assert(!failNextHeapAllocation && !failModelRegistration);
    assert(submitted.size() == beforeFailedModel && releasedModels.back() == orphanModel);
    assert(match::end_frame() && !lastPresent);
    assert(match::begin_frame());
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.back().model && submitted.back().model != orphanModel);
    assert(match::end_frame() && lastPresent);

    glModelPacket materialPacket = packet;
    materialPacket.indexBuffer = 0;
    materialPacket.streams = streams;
    materialPacket.numStreams = 2;
    materialPacket.userData = 0;
    materialPacket.state.program = glGetProgram("3d");
    {
        PlatTexture generated;
        std::array<uint8_t, 8 * 8 * 2> pixels{};
        generated.m_Width = generated.m_Height = 8;
        generated.m_Levels = 1;
        generated.m_Format = GXTex_RGB565;
        generated.m_SwizzledData = pixels.data();
        generated.m_OwnerTextureLevel = 4;
        fileTexture = &generated;
        assert(match::begin_frame());
        failTextureUpload = true;
        const auto beforeFailedUpload = submitted.size();
        match::on_packet(GLV_Characters, 0x82, &materialPacket);
        failTextureUpload = false;
        assert(submitted.size() == beforeFailedUpload);
        assert(match::end_frame() && !lastPresent);
        assert(match::begin_frame());
        failTextureRegistration = true;
        const auto beforeFailedTexture = submitted.size();
        match::on_packet(GLV_Characters, 0x82, &materialPacket);
        const auto orphanTexture = lastCreatedTexture;
        assert(!failNextHeapAllocation && !failTextureRegistration);
        assert(submitted.size() == beforeFailedTexture && releasedTextures.back() == orphanTexture);
        assert(match::end_frame() && !lastPresent);
        assert(match::begin_frame());
        match::on_packet(GLV_Characters, 0, &materialPacket);
        assert(submitted.back().material.textures[0] != orphanTexture);
        assert(match::end_frame() && lastPresent);
        match::on_texture_release(0);
        fileTexture = &texture;
    }
    assert(match::begin_frame());
    failDraw = true;
    match::on_packet(GLV_Characters, 0, &materialPacket);
    failDraw = false;
    assert(match::end_frame() && !lastPresent);

    // Partial original TEV updates happen before geometry rejection and survive
    // frame/view boundaries. Only renderer shutdown resets the inherited state.
    diffuseLevel = 32;
    materialPacket.state.texture[1] = materialPacket.state.texture[5] = 77;
    materialPacket.primType = 6;
    materialPacket.state.texconfig = 3;
    assert(match::begin_frame());
    const auto beforeStateOnly = submitted.size();
    match::on_packet(GLV_Characters, 0x82, &materialPacket);
    assert(submitted.size() == beforeStateOnly);
    materialPacket.primType = 0;
    materialPacket.state.texconfig = 33;
    match::on_packet(GLV_Characters, 0x80, &materialPacket);
    assert(submitted.back().material.alpha[1] == 129.f / 255.f);
    assert(match::end_frame() && lastPresent);

    assert(match::begin_frame());
    viewState.target = GLTG_Staging;
    match::begin_view(GLV_Transitions3D);
    const auto firstTarget = selectedTargets.back();
    const auto previousTargetCount = targetsCreated;
    match::begin_view(GLV_Transitions3D);
    assert(selectedTargets.back() == firstTarget && targetsCreated == previousTargetCount);
    match::on_packet(GLV_Transitions3D, 0, &materialPacket);
    assert(submitted.back().material.alpha[1] == 129.f / 255.f);
    assert(match::end_frame());
    match::shutdown();
    assert(match::begin_frame());
    match::begin_view(GLV_Transitions3D);
    assert(selectedTargets.back() != firstTarget && targetsCreated == previousTargetCount + 1);
    assert(match::admit_texture(&texture, upload));
    match::on_packet(GLV_Transitions3D, 0x82, &materialPacket);
    assert(submitted.back().material.alpha[1] == 1);
    assert(match::end_frame());
    match::shutdown();
    diffuseLevel = 63;
    check_material_program_lookup();
    check_material_texture_partition();
    check_skin_switching();
    check_invalidation_bounds();
    check_display_messages();
    check_loading_indicator();
    {
        FrameTimelineTestOutputs outputPaths;
        check_net_conversion();
        check_frame_timeline_runtime();
    }
    match::enable(false);
    fs::current_path(workingDirectory);
    fs::remove_all(temporaryDirectory);
    std::puts("Native match frontend: live material/alpha/depth/fog, resident geometry "
              "invalidation, ownership, rejection and presentation order passed");
}
