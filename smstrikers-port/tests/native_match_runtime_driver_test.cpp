// Exercise the public game hooks and all production frontend modules. The
// graphics sink records submissions; it does not need Vita hardware or assets.
#include "port/native_match_runtime.hpp"
#include "port/deferred_renderer.hpp"
#include "NL/gl/glState.h"
#include "NL/gl/glConstant.h"
#include "NL/gl/glMatrix.h"
#include "NL/glx/glxDisplayList.h"
#include "NL/nlTask.h"
#include "Game/GL/ShaderSkinMesh.h"
#include <cassert>
#include <cstring>
#include <limits>
#include <filesystem>
#include <unistd.h>

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
uint32_t nextId = 1, modelsCreated = 0, retraces = 0;
bool frameActive = false, lastPresent = false, arenaOwns = true;
bool diagnostics = false;
s32 currentFrame = 1;
glView viewState{};
nlMatrix4 viewMatrix{}, projectionMatrix{};
DisplayList displayList{};
constexpr uintptr_t ListHandle = 0x2222;
PlatTexture* fileTexture = nullptr;

void identity(nlMatrix4& matrix)
{
    std::memset(matrix.e, 0, sizeof matrix.e);
    matrix.e[0] = matrix.e[5] = matrix.e[10] = matrix.e[15] = 1;
}
}   // namespace

extern "C" {
int PortDiagnosticsEnabled() { return diagnostics; }
unsigned long long port_monotonic_ns() { return 0; }
unsigned PortLogicalFrameWidth() { return 640; }
void PortNativeRetrace() { ++retraces; }
int port_region_owns(const void*) { return 0; }
}
u8 glx_bLoadingIndicator = 0;
nlTaskManager* nlTaskManager::m_pInstance = nullptr;
s32 glGetCurrentFrame() { return currentFrame; }
const glView* glViewGetState(eGLView) { return &viewState; }
void glViewGetViewMatrix(eGLView, nlMatrix4& out) { out = viewMatrix; }
void glViewGetProjectionMatrix(eGLView, nlMatrix4& out) { out = projectionMatrix; }
uintptr_t glGetIdentityMatrix() { return 1; }
void nlMultMatrices(nlMatrix4&, const nlMatrix4&, const nlMatrix4&) { assert(false); }
void* glUserGetData(const void* handle) { return const_cast<void*>(handle); }
u32 glGetProgram(const char* name)
{
    // Distinct stable IDs preserve the production program comparisons.
    uint32_t hash = 2166136261u;
    while (*name)
        hash = (hash ^ uint8_t(*name++)) * 16777619u;
    return hash;
}
u32 glGetTexture(const char*) { return 77; }
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
        return 63;
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
        result.x = .375f;
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
    return handle == 77 ? fileTexture : nullptr;
}
PlatTexture::~PlatTexture() = default;

namespace port::render
{
struct DeferredRenderer::Impl
{};
DeferredRenderer::DeferredRenderer() noexcept = default;
DeferredRenderer::~DeferredRenderer() = default;
bool DeferredRenderer::initialize(bool) noexcept { return true; }
bool DeferredRenderer::initialize_scene_program() noexcept { return true; }
uint32_t DeferredRenderer::create_texture(const TextureUpload&) noexcept { return nextId++; }
uint32_t DeferredRenderer::create_target(uint32_t, uint32_t) noexcept { return nextId++; }
void DeferredRenderer::release_texture(uint32_t id) noexcept { releasedTextures.push_back(id); }
uint32_t DeferredRenderer::create_model(std::span<const ModelVertex> vertices,
                                        std::span<const uint16_t> indices) noexcept
{
    assert(vertices.size() == 3 && indices.size() == 3);
    lastCreatedVertices.assign(vertices.begin(), vertices.end());
    ++modelsCreated;
    return nextId++;
}
void DeferredRenderer::release_model(uint32_t id) noexcept { releasedModels.push_back(id); }
bool DeferredRenderer::draw_scene(uint32_t model, std::span<const ModelVertex>,
                                  std::span<const uint16_t>, const std::array<float, 16>& matrix,
                                  const Raster& raster, const SceneMaterial& material) noexcept
{
    submitted.push_back({ model, material, raster, matrix });
    return true;
}
bool DeferredRenderer::select_target(uint32_t, bool, bool) noexcept { return true; }
bool DeferredRenderer::copy_texture(uint32_t, std::array<float, 4>, bool) noexcept { return true; }
bool DeferredRenderer::copy_textures(std::span<const TextureCopy>) noexcept { return true; }
bool DeferredRenderer::clear_color(std::array<float, 4>, bool) noexcept { return true; }
bool DeferredRenderer::clear_depth() noexcept { return true; }
bool DeferredRenderer::frame_active() const noexcept { return frameActive; }
bool DeferredRenderer::begin_frame() noexcept
{
    frameActive = true;
    return true;
}
bool DeferredRenderer::end_frame(bool present) noexcept
{
    frameActive = false;
    lastPresent = present;
    return true;
}
bool DeferredRenderer::readback_presented_rgba8(std::vector<uint8_t>&) noexcept { return true; }
void DeferredRenderer::shutdown() noexcept { frameActive = false; }
const char* DeferredRenderer::last_error() const noexcept { return "none"; }
int DeferredRenderer::error_code() const noexcept { return 0; }
DeferredRenderer::FrameTiming DeferredRenderer::completed_timing() const noexcept { return {}; }
}   // namespace port::render

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
    match::on_packet(GLV_Characters, 0, &packet);
    assert(submitted.size() == 1);
    const auto& first = submitted.back();
    assert(first.model == 0 && first.material.textures[0] != 0 && first.material.lit);
    assert(first.material.controls[3] == 1 && first.material.effects[3] == .375f);
    assert(first.material.effects[0] == 1 && first.material.tint[3] == 32.f / 255);
    assert(first.material.shadow[0] == 21.f / 63 && first.material.shadowAdd[0] == 1.f - 21.f / 63);
    assert(first.material.fog[2] == 1 && first.material.viewport[2] == 960);
    assert(first.raster.depthTest && first.raster.depthWrite && !first.raster.writeAlpha);

    userdata[GLUD_Translucent] = reinterpret_cast<uintptr_t>(&alpha);
    userdata[GLUD_NoFog] = reinterpret_cast<uintptr_t>(&alpha);
    match::on_packet(GLV_ShadowTexture, 0, &packet);
    assert(submitted.back().material.effects[1] == 127.f / 255);
    assert(submitted.back().material.fog[2] == 0 && submitted.back().raster.writeAlpha);
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
    packet.primType = 5;
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
        GLSkinUserData bones[2];
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
    match::shutdown();
    match::enable(false);
    fs::current_path(workingDirectory);
    fs::remove_all(temporaryDirectory);
    std::puts("Native match frontend: live material/alpha/depth/fog, resident geometry "
              "invalidation, ownership, rejection and presentation order passed");
}
