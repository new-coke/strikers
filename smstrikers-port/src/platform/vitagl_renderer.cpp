#if defined(PORT_VITA) && defined(STRIKERS_VITA_VITAGL)
#include <atomic>
#include "port/renderer.hpp"
#include "port/scene_shader_variants.hpp"
#include "port/scene_material_alpha.hpp"
#include "port/vitagl_texture.hpp"
#include "port/native_boot_screen.hpp"
#include "port/loading_indicator_state.hpp"
#include "port/loading_indicator_shaders.hpp"
#include <vitaGL.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>
#include <string>
#include <unordered_map>
namespace port::render
{
namespace
{
constexpr unsigned W = 960, H = 544, MB = 1024 * 1024;
// 0 play, 1 cinematic at 30 fps, 2 cinematic uncapped.
std::atomic<int> g_cinematicFrame{ 0 };
constexpr const char* attribs[]{ "a_position", "a_color", "a_normal", "a_uv0", "a_uv1",
                                 "a_uv2",      "a_uv3",   "a_uv4",    "a_uv5", "a_bone" };
constexpr const char* uniforms[]{ "u_mvp",        "u_mv",       "u_bones",         "u_environment",
                                  "u_ambient",    "u_diffuse",  "u_lighting",      "u_lightpos",
                                  "u_lightcolor", "u_specular", "u_specularcolor", "u_controls",
                                  "u_effects",    "u_tint",     "u_shadow",        "u_shadowadd",
                                  "u_fog",        "u_fogcolor", "u_material", "u_alpha",
                                  "u_fogrange", "u_fograngecontrol" };
constexpr unsigned boneUniform = 2;
constexpr unsigned fogRangeTableUniform = 20, fogRangeControlUniform = 21;
constexpr unsigned counts[]{ 4, 4, kSceneBoneRows, 1, 1, 1, 1, 8, 8, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1 };
static_assert(std::size(uniforms) == std::size(counts));
constexpr unsigned smallUniformFloats = 8 * 4;
static_assert([] {
    for (unsigned i = 0; i < std::size(counts); ++i)
        if (i != boneUniform && counts[i] * 4 > smallUniformFloats) return false;
    return counts[boneUniform] * 4 == kSceneBoneFloats;
}());
constexpr unsigned affineProgramBit = 256;
constexpr unsigned rigidProgramBit = 512;
constexpr unsigned fogOffProgramBit = 1024;
constexpr unsigned copyIntensityProgramBit = 2048;
constexpr unsigned copyAlphaProgramBit = 4096;
bool valid_geometry(std::span<const ModelVertex> v, std::span<const uint16_t> i, bool& usesBones)
{
    usesBones = false;
    if (v.empty() || v.size() > 65536 || i.empty() || i.size() > 196608)
        return false;
    for (auto n : i)
        if (n >= v.size())
            return false;
    for (const auto& vertex : v)
    {
        if (!(vertex.boneSlot >= 0.f && vertex.boneSlot <= float(kSceneSkinBoneCount)) ||
            vertex.boneSlot != float(unsigned(vertex.boneSlot)))
            return false;
        usesBones |= vertex.boneSlot > 0.f;
    }
    return true;
}
struct Uniform
{
    GLint location = -1;
    unsigned vectors = 0;
    std::array<float, smallUniformFloats> previous{};
    bool valid = false;
};
struct Program
{
    GLuint id = 0;
    unsigned mask = 63;
    bool generic = false;
    bool vertexFog = false;
    std::array<Uniform, std::size(counts)> uniforms{};
    // Keep the large palette cache once per program, not in every uniform.
    std::array<float, kSceneBoneFloats> previousBones{};
};
struct Texture
{
    GLuint id = 0, fbo = 0, depth = 0;
    unsigned width = 0, height = 0, levels = 1;
    TargetFormat format = TargetFormat::Rgba8;
    uint8_t minAlpha = 255;
    int wrapU = -1, wrapV = -1, filter = -1;
    // Physical over logical size: callers address a scaled target in 960x544.
    float scale = 1.f;
    unsigned logical_width() const { return scale == 1.f ? width : W; }
    unsigned logical_height() const { return scale == 1.f ? height : H; }
    std::array<int, 4> physical(const std::array<int, 4>& r) const
    {
        const auto at = [&](int v) { return int(std::lround(float(v) * scale)); };
        const int x0 = at(r[0]), y0 = at(r[1]);
        return { x0, y0, at(r[0] + r[2]) - x0, at(r[1] + r[3]) - y0 };
    }
};
struct Model
{
    GLuint vbo = 0, ebo = 0;
    size_t bytes = 0;
    unsigned count = 0;
    uint8_t minAlpha = 255;
    bool usesBones = false;
    unsigned projectiveUvChannels = 0;
};
// An upload is owned here until publication in the renderer cache succeeds.
struct TextureLease
{
    Texture& texture;
    bool committed = false;
    ~TextureLease()
    {
        if (committed)
            return;
        if (texture.fbo)
            glDeleteFramebuffers(1, &texture.fbo);
        if (texture.depth)
            glDeleteRenderbuffers(1, &texture.depth);
        if (texture.id)
            glDeleteTextures(1, &texture.id);
    }
};
struct ModelLease
{
    Model& model;
    bool committed = false;
    ~ModelLease()
    {
        if (committed)
            return;
        if (model.vbo)
            glDeleteBuffers(1, &model.vbo);
        if (model.ebo)
            glDeleteBuffers(1, &model.ebo);
    }
};
}   // namespace
struct Renderer::Impl
{
    bool initialized = false, contextInitialized = false, sceneInitialized = false, active = false,
         presented = false, timed = false, startupRetired = false;
    bool affineUv = false, vertexFog = false, fogOffSpecialize = false, fogCurve4Specialize = false;
    bool bc1Direct = false;
    bool fogNoRange = false;
    float renderScale = 1.f;
    float lowScale = 0.f;
    unsigned mainFull = 0, mainLow = 0;
    static constexpr unsigned dynamicHold = 120;
    unsigned dynamicFrames = 0;
    bool cinematic30 = false, frameCinematic = false, frameHalfRate = false;
    unsigned swapInterval = 1, lastPresentVcount = 0;
    bool displayDraw = false;
    bool stagedBlack = false;
    unsigned firstFrames = 0;
    SceUID owner = -1;
    unsigned cullTest = 0;
    int error = 0;
    const char* operation = "none";
    FrameTiming current{}, completed{};
    FrameTimeline::Owner* capture = nullptr;
    struct PresentProfileScope
    {
        bool enabled;
        PresentProfileScope(FrameTimeline::Owner* capture, bool timed)
            : enabled(capture && timed)
        {
            if (enabled)
                vglSetPresentProfile(&capture->provider, +[]() -> uint64_t {
                    return uint64_t(sceKernelGetSystemTimeWide()) * 1000;
                });
        }
        ~PresentProfileScope()
        {
            if (enabled) vglSetPresentProfile(nullptr, nullptr);
        }
    };
    vglUniformCacheStats uniformStart{};
    vglVertexProgramCacheStats vertexProgramStart{};
    bool vertexProgramProfile = false;
    vglVertexStreamStats vertexStreamStart{};
    bool vertexStreamProfile = false;
    bool uniformProfile = false, uniformVerify = false;
    unsigned uniformVerifyFrames = 0;
    std::unordered_map<unsigned, Texture> textures;
    std::unordered_map<unsigned, Model> models;
    struct StreamedModel
    {
        std::vector<ModelVertex> vertices;
        std::vector<uint16_t> indices;
    };
    std::unordered_map<unsigned, StreamedModel> streamedModels;
    std::unordered_map<unsigned, Program> programs;
    std::array<GLuint, 2> vertexShaders{}, referenceVertexShaders{};
    GLuint dynamicVbo = 0, dynamicEbo = 0;
    unsigned main = 0, target = 0, white = 0, nextModel = 1;
    size_t geometryBytes = 0;
    GLuint boundProgram = 0;
    GLuint attributeBuffer = 0;
    bool attributesEnabled = false;
    uint32_t fixedSignature = UINT32_MAX;
    bool viewportValid = false, scissorValid = false;
    std::array<int, 4> lastViewport{}, lastScissor{};
    std::array<GLuint, 6> boundTextures{};
    struct Loading
    {
        bool active = false;
        unsigned background = 0;
        std::array<unsigned, 2> operations{};
        std::array<GLuint, 4> vertices{};
        GLuint indices = 0;
        std::array<std::array<float, 4>, 3> regions{};
        LoadingIndicatorClock clock;
    } loading;
    GLuint loadingProgram = 0;
    GLint loadingRegion = -1;
    void invalidate_fixed()
    {
        attributeBuffer = 0;
        attributesEnabled = false;
        fixedSignature = UINT32_MAX;
        viewportValid = scissorValid = false;
    }
    uint64_t now() { return timed ? sceKernelGetSystemTimeWide() : 0; }
    bool fail(const char* op, int e = GL_INVALID_OPERATION)
    {
        loading.active = false;
        if (!error)
        {
            error = e;
            operation = op;
            std::fprintf(stderr, "[vitagl] failure %s error=%x\n", op, e);
        }
        return false;
    }
    bool check(const char* op)
    {
        const auto e = glGetError();
        return e == GL_NO_ERROR || fail(op, e);
    }
    bool capture_uniform_timing()
    {
        if (!uniformProfile)
            return true;
        vglUniformCacheStats after{};
        vglGetUniformCacheStats(&after);
        auto& result = current.uniforms;
        if (!after.profiling || after.enabled != uniformStart.enabled ||
            after.fallback_uploads < uniformStart.fallback_uploads ||
            after.errors < uniformStart.errors)
            return fail("uniform_counter_reset");
        static_assert(VGL_UNIFORM_COUNTER_COUNT == 8);
        for (unsigned stage = 0; stage < 2; ++stage)
            for (unsigned counter = 0; counter < VGL_UNIFORM_COUNTER_COUNT; ++counter)
            {
                if (after.stage[stage][counter] < uniformStart.stage[stage][counter])
                    return fail("uniform_counter_reset");
                result.stage[stage][counter] = after.stage[stage][counter] - uniformStart.stage[stage][counter];
            }
        result.fallbackUploads = after.fallback_uploads - uniformStart.fallback_uploads;
        result.errors = after.errors - uniformStart.errors;
        result.cacheBytes = after.cache_bytes;
        result.cachePeakBytes = after.cache_peak_bytes;
        result.enabled = after.enabled;
        result.valid = true;
        return true;
    }
    bool capture_vertex_program_timing()
    {
        if (!vertexProgramProfile)
            return true;
        vglVertexProgramCacheStats after{};
        vglGetVertexProgramCacheStats(&after);
        if (!after.profiling || after.enabled != vertexProgramStart.enabled)
            return fail("vertex_program_counter_reset");
        static_assert(VGL_VERTEX_PROGRAM_COUNTER_COUNT == 11);
        auto& result = current.vertexPrograms;
        for (unsigned i = 0; i < VGL_VERTEX_PROGRAM_COUNTER_COUNT; ++i)
        {
            if (after.counters[i] < vertexProgramStart.counters[i])
                return fail("vertex_program_counter_reset");
            result.counters[i] = after.counters[i] - vertexProgramStart.counters[i];
        }
        result.cacheBytes = after.cache_bytes;
        result.cachePeakBytes = after.cache_peak_bytes;
        result.enabled = after.enabled;
        result.valid = true;
        return true;
    }
    bool capture_vertex_stream_timing()
    {
        if (!vertexStreamProfile)
            return true;
        vglVertexStreamStats after{};
        vglGetVertexStreamStats(&after);
        if (!after.profiling || after.enabled != vertexStreamStart.enabled)
            return fail("vertex_stream_counter_reset");
        static_assert(VGL_VERTEX_STREAM_COUNTER_COUNT == 6);
        auto& result = current.vertexStreams;
        for (unsigned i = 0; i < VGL_VERTEX_STREAM_COUNTER_COUNT; ++i)
        {
            if (after.counters[i] < vertexStreamStart.counters[i])
                return fail("vertex_stream_counter_reset");
            result.counters[i] = after.counters[i] - vertexStreamStart.counters[i];
        }
        result.enabled = after.enabled;
        result.valid = true;
        return true;
    }
    void report_uniform_verification()
    {
        if (!uniformVerify)
            return;
        // Verification is mutually exclusive with capture profiling. Report
        // after the frame, with no additional clock or per-draw logging.
        if (++uniformVerifyFrames == 1 || uniformVerifyFrames % 300 == 0)
        {
            vglUniformCacheStats stats{};
            vglGetUniformCacheStats(&stats);
            std::fprintf(stderr, "[vitagl-uniform-verify] frame=%u checks=%llu cache_bytes=%llu\n",
                         uniformVerifyFrames, (unsigned long long)stats.verify_checks,
                         (unsigned long long)stats.cache_bytes);
        }
    }
    bool on_owner() { return owner == sceKernelGetThreadId() || fail("owner_thread"); }
    unsigned target_id() const { return target ? target : main; }
    Texture* surface()
    {
        auto it = textures.find(target_id());
        return it == textures.end() ? nullptr : &it->second;
    }
    struct DisplayScope
    {
        Renderer& renderer;
        Impl& d;
        bool active;
        unsigned target;
        FrameTiming current;
        FrameTimeline::Owner* capture = nullptr;
        GLint drawFramebuffer = 0, readFramebuffer = 0;
        uint32_t texture = 0;
        bool valid = false, pending = false;
        DisplayScope(Renderer& renderer, Impl& d)
          : renderer(renderer), d(d), active(d.active), target(d.target), current(d.current)
        {
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &drawFramebuffer);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
            valid = pending = d.check("display_state");
            // This transaction restores the scene's parent timers. Exclude its
            // isolated bitmap/loading draws from the corresponding children too.
            if (valid && d.capture)
            {
                capture = d.capture;
                d.capture = nullptr;
            }
        }
        void restore()
        {
            if (!pending)
                return;
            if (texture)
                renderer.release_texture(texture);
            d.active = active;
            d.target = target;
            d.current = current;
            if (capture) d.capture = capture;
            glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(readFramebuffer));
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(drawFramebuffer));
            d.invalidate_fixed();
            pending = false;
        }
        ~DisplayScope() { restore(); }
    };
    void discard_loading(Renderer& renderer)
    {
        loading.active = false;
        if (loading.background)
        {
            // The capture may still be the open GXM scene before the first
            // loading tick. Finish on the stable display target, not the FBO
            // whose colour/depth storage is about to be retired.
            GLint drawFramebuffer = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &drawFramebuffer);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glFinish();
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(drawFramebuffer));
        }
        for (auto id : loading.operations)
            renderer.release_texture(id);
        renderer.release_texture(loading.background);
        for (auto id : loading.vertices)
            if (id)
                glDeleteBuffers(1, &id);
        if (loading.indices)
            glDeleteBuffers(1, &loading.indices);
        loading = {};
        invalidate_fixed();
    }
    bool capture_loading_background()
    {
        const auto& background = textures.at(loading.background);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_FRONT);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, background.fbo);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(1, 1, 1, 1);
        // Default framebuffer sources use top-first V. Reverse the FBO
        // destination to keep that top row at texture V=0 for the overlay.
        glBlitFramebuffer(0, 0, W, H, 0, H, W, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glReadBuffer(GL_BACK);
        invalidate_fixed();
        return check("loading_capture");
    }
    bool clear_loading_background()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, textures.at(loading.background).fbo);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(1, 1, 1, 1);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        invalidate_fixed();
        return check("loading_background_clear");
    }
    GLuint compile(GLenum stage, const std::string& source)
    {
        GLuint id = glCreateShader(stage);
        const char* s = source.c_str();
        glShaderSource(id, 1, &s, nullptr);
        glCompileShader(id);
        GLint ok = 0;
        glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            GLint n = 0;
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &n);
            std::vector<char> log(std::max(2, n));
            glGetShaderInfoLog(id, log.size(), nullptr, log.data());
            auto* f = std::fopen("ux0:data/strikersVita/native_shader_compile.log", "a");
            if (f)
            {
                std::fprintf(f, "stage=%x %s\n", stage, log.data());
                std::fclose(f);
            }
            glDeleteShader(id);
            fail("shader_compile");
            return 0;
        }
        return id;
    }
    bool make_program(unsigned key, unsigned mask, bool alpha, bool generic, bool affine, bool usesBones,
                      unsigned copyMode = 0)
    {
        const bool fogOff = (key & fogOffProgramBit) != 0;
        const bool fog = vertexFog && !generic && !fogOff;
        const bool noRange = fogNoRange && !generic && !fogOff;
        const auto source = scene_fragment_shader(mask, alpha, generic, affine, fog, fogOff,
                                                  fogCurve4Specialize, noRange, copyMode);
        if (source.empty())
            return fail("shader_variant_source");
        GLuint fs = compile(GL_CG_FRAGMENT_SHADER_EXT, source);
        if (!fs)
            return false;
        Program p;
        p.mask = mask;
        p.generic = generic;
        p.vertexFog = fog;
        p.id = glCreateProgram();
        const auto reference = referenceVertexShaders[usesBones];
        glAttachShader(p.id, (generic || fogOff) && reference ? reference : vertexShaders[usesBones]);
        glAttachShader(p.id, fs);
        for (unsigned a = 0; a < 10; ++a)
            glBindAttribLocation(p.id, a, attribs[a]);
        glLinkProgram(p.id);
        glDeleteShader(fs);
        GLint ok = 0;
        glGetProgramiv(p.id, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            glDeleteProgram(p.id);
            return fail("shader_link");
        }
        glUseProgram(p.id);
        GLint activeUniforms = 0;
        glGetProgramiv(p.id, GL_ACTIVE_UNIFORMS, &activeUniforms);
        for (unsigned u = 0; u < p.uniforms.size(); ++u)
        {
            auto& q = p.uniforms[u];
            q.location = glGetUniformLocation(p.id, uniforms[u]);
            if (q.location == -1)
            {
                std::string name = std::string(uniforms[u]) + "[0]";
                q.location = glGetUniformLocation(p.id, name.c_str());
            }
            if (q.location == -1)
            {
                if (u == boneUniform && usesBones)
                {
                    glDeleteProgram(p.id);
                    return fail("bone_uniform_missing");
                }
                if (!fogOff && !noRange && (u == fogRangeTableUniform || u == fogRangeControlUniform))
                {
                    glDeleteProgram(p.id);
                    return fail("fog_range_uniform_missing");
                }
                continue;
            }
            if ((fogOff || noRange) && (u == fogRangeTableUniform || u == fogRangeControlUniform))
            {
                glDeleteProgram(p.id);
                return fail("fog_off_uniform");
            }
            if (u == boneUniform && !usesBones)
            {
                glDeleteProgram(p.id);
                return fail("rigid_bone_uniform");
            }
            for (GLint a = 0; a < activeUniforms; ++a)
            {
                char name[96]{};
                GLint size = 0;
                GLenum type = 0;
                glGetActiveUniform(p.id, a, sizeof name, nullptr, &size, &type, name);
                if (glGetUniformLocation(p.id, name) == q.location)
                {
                    if (type != GL_FLOAT_VEC4 || size <= 0)
                    {
                        glDeleteProgram(p.id);
                        return fail("uniform_reflection");
                    }
                    // Dynamic bone indexing can reach the entire palette.
                    // Never silently truncate it to an older shader layout.
                    if (u == boneUniform && unsigned(size) < kSceneBoneRows)
                    {
                        glDeleteProgram(p.id);
                        return fail("bone_uniform_capacity");
                    }
                    if ((u == fogRangeTableUniform || u == fogRangeControlUniform) &&
                        unsigned(size) < counts[u])
                    {
                        glDeleteProgram(p.id);
                        return fail("fog_range_uniform_capacity");
                    }
                    q.vectors = std::min(unsigned(size), counts[u]);
                    break;
                }
            }
            if (!q.vectors)
            {
                glDeleteProgram(p.id);
                return fail("uniform_capacity");
            }
        }
        for (unsigned a = 0; a < 6; ++a)
        {
            char name[20];
            std::snprintf(name, sizeof name, "u_tex%u", a);
            GLint location = glGetUniformLocation(p.id, name);
            if (location != -1)
                glUniform1i(location, a);
        }
        if (!check("program_initialize"))
        {
            glDeleteProgram(p.id);
            return false;
        }
        programs.emplace(key, std::move(p));
        return true;
    }
    bool make_loading_program()
    {
        if (loadingProgram)
            return true;
        struct Lease
        {
            GLuint vertex = 0, fragment = 0, program = 0;
            ~Lease()
            {
                if (vertex) glDeleteShader(vertex);
                if (fragment) glDeleteShader(fragment);
                if (program) glDeleteProgram(program);
            }
        } lease;
        lease.vertex = compile(GL_CG_VERTEX_SHADER_EXT, loadingVertexShader);
        if (!lease.vertex)
            return false;
        lease.fragment = compile(GL_CG_FRAGMENT_SHADER_EXT, loading_fragment_shader());
        if (!lease.fragment)
            return false;
        lease.program = glCreateProgram();
        glAttachShader(lease.program, lease.vertex);
        glAttachShader(lease.program, lease.fragment);
        glBindAttribLocation(lease.program, 0, "a_position");
        glBindAttribLocation(lease.program, 3, "a_uv0");
        glLinkProgram(lease.program);
        GLint ok = 0;
        glGetProgramiv(lease.program, GL_LINK_STATUS, &ok);
        if (!ok)
            return fail("loading_shader_link");
        glUseProgram(lease.program);
        boundProgram = lease.program;
        const auto backdrop = glGetUniformLocation(lease.program, "u_background");
        const auto operations = glGetUniformLocation(lease.program, "u_operations");
        const auto region = glGetUniformLocation(lease.program, "u_region");
        if (backdrop == -1 || operations == -1 || region == -1)
            return fail("loading_uniforms");
        glUniform1i(backdrop, 0);
        glUniform1i(operations, 1);
        if (!check("loading_shader_initialize"))
            return false;
        loadingRegion = region;
        loadingProgram = lease.program;
        lease.program = 0;
        return true;
    }
    bool draw_loading(unsigned quad, unsigned image, const std::array<float, 4>& region)
    {
        if (boundProgram != loadingProgram)
        {
            glUseProgram(loadingProgram);
            boundProgram = loadingProgram;
        }
        const unsigned ids[]{loading.background, loading.operations[image]};
        for (unsigned unit = 0; unit < 2; ++unit)
        {
            if (boundTextures[unit] != ids[unit])
            {
                glActiveTexture(GL_TEXTURE0 + unit);
                glBindTexture(GL_TEXTURE_2D, ids[unit]);
                boundTextures[unit] = ids[unit];
            }
        }
        glUniform4fv(loadingRegion, 1, region.data());
        glBindBuffer(GL_ARRAY_BUFFER, loading.vertices[quad]);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, loading.indices);
        attributes(loading.vertices[quad]);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
        return check("loading_draw");
    }
    bool present_loading(bool icons, unsigned selected)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        invalidate_fixed();
        glViewport(0, 0, W, H);
        glDisable(GL_SCISSOR_TEST);
        Raster state;
        state.depthTest = state.depthWrite = false;
        state.writeAlpha = true;
        raster(state);
        if (!draw_loading(0, 0, {}))
            return false;
        if (icons)
            for (unsigned i = 0; i < 3; ++i)
                if (!draw_loading(i + 1, i == selected, loading.regions[i]))
                    return false;
        vglSwapBuffers(GL_FALSE);
        if (!check("loading_swap"))
            return false;
        const int completion = sceGxmDisplayQueueFinish();
        return completion >= 0 || fail("loading_present_completion", completion);
    }
    void uniform(Program& p, unsigned slot, const float* value)
    {
        auto& u = p.uniforms[slot];
        if (u.location == -1)
            return;
        const auto n = u.vectors * 4 * sizeof(float);
        auto* previous = slot == boneUniform ? p.previousBones.data() : u.previous.data();
        if (!u.valid || std::memcmp(previous, value, n))
        {
            glUniform4fv(u.location, u.vectors, value);
            std::memcpy(previous, value, n);
            u.valid = true;
        }
    }
    void attributes(GLuint buffer)
    {
        // VitaGL captures the VBO name, not its current storage address.
        // Reuploads to the dynamic VBO keep the same attribute contract.
        if (attributeBuffer == buffer && attributesEnabled)
            return;
        constexpr unsigned offsets[]{ 0, 16, 32, 20, 44, 56, 68, 80, 92, 104 };
        constexpr unsigned sizes[]{ 4, 4, 3, 3, 3, 3, 3, 3, 3, 1 };
        for (unsigned a = 0; a < 10; ++a)
        {
            if (!attributesEnabled)
                glEnableVertexAttribArray(a);
            glVertexAttribPointer(a, sizes[a], a == 1 ? GL_UNSIGNED_BYTE : GL_FLOAT,
                                  a == 1 ? GL_TRUE : GL_FALSE, sizeof(ModelVertex),
                                  reinterpret_cast<const void*>(uintptr_t(offsets[a])));
        }
        attributeBuffer = buffer;
        attributesEnabled = true;
    }
    void raster(const Raster& r)
    {
        const uint32_t signature = unsigned(r.depthTest) | (unsigned(r.depthWrite) << 1) |
                                   (r.depthFunction << 2) | (r.cull << 4) | (r.blend << 6) |
                                   (unsigned(r.writeRgb) << 10) | (unsigned(r.writeAlpha) << 11) |
                                   (unsigned(r.primitive) << 12);
        if (signature == fixedSignature)
            return;
        fixedSignature = signature;
        constexpr GLenum depths[]{ GL_ALWAYS, GL_LEQUAL, GL_EQUAL, GL_LESS };
        if (r.depthTest)
        {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(depths[r.depthFunction]);
        }
        else
            glDisable(GL_DEPTH_TEST);
        glDepthMask(r.depthWrite);
        if (r.cull && !r.primitive && cullTest != 1)
        {
            glEnable(GL_CULL_FACE);
            // This VitaGL provider implements back-face winding combinations.
            // Culling CW front faces is equivalent to CCW front + back cull.
            const bool front = (r.cull == 1) != (cullTest == 2);
            glFrontFace(front ? GL_CCW : GL_CW);
            glCullFace(GL_BACK);
        }
        else
            glDisable(GL_CULL_FACE);
        glColorMask(r.writeRgb, r.writeRgb, r.writeRgb, r.writeAlpha);
        if (r.blend == 0 || r.blend == 6)
            glDisable(GL_BLEND);
        else
        {
            glEnable(GL_BLEND);
            glBlendEquation(r.blend == 7 ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD);
            constexpr GLenum src[]{ GL_ONE,       GL_SRC_ALPHA,           GL_ONE, GL_SRC_ALPHA,
                                    GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_ONE, GL_ONE };
            constexpr GLenum dst[]{
                GL_ZERO, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE, GL_ZERO, GL_ONE, GL_ZERO, GL_ONE
            };
            glBlendFunc(src[r.blend], dst[r.blend]);
        }
    }
};
Renderer::Renderer() noexcept : impl_(new (std::nothrow) Impl) {}
Renderer::~Renderer() { shutdown(); }
bool Renderer::initialize(bool timed) noexcept
{
    if (!impl_)
        return false;
    auto& d = *impl_;
    if (d.initialized)
        return d.on_owner();
    d.owner = sceKernelGetThreadId();
    d.error = 0;
    d.operation = "none";
    d.current = d.completed = {};
    d.stagedBlack = false;
    d.firstFrames = 0;
    port::startup::show("PREPARING GRAPHICS");
    d.timed = timed;
    const auto enabled = [](const char* key) {
        const auto* value = std::getenv(key);
        return value && std::strcmp(value, "1") == 0;
    };
    // PortConfigLoad runs before this owner is created. The startup bitmap
    // does not create GL programs, so configure before any program can link.
    const bool uniformCache = enabled("STRIKERS_VITAGL_UNIFORM_CACHE");
    const bool uniformProfile = enabled("STRIKERS_VITAGL_PACKET_PROFILE");
    const bool uniformVerify = enabled("STRIKERS_VITAGL_UNIFORM_VERIFY");
    if (!vglConfigureUniformCache(uniformCache, uniformProfile, uniformVerify))
        return d.fail("uniform_config", glGetError());
    vglUniformCacheStats uniformConfig{};
    vglGetUniformCacheStats(&uniformConfig);
    d.uniformProfile = uniformConfig.profiling;
    d.uniformVerify = uniformConfig.verifying;
    d.uniformVerifyFrames = 0;
    if (uniformCache || uniformProfile || uniformVerify)
        std::fprintf(stderr, "[vitagl-uniform] cache=%u profile=%u verify=%u\n",
                     unsigned(uniformConfig.enabled), unsigned(d.uniformProfile), unsigned(d.uniformVerify));
    const bool vertexProgramCache = enabled("STRIKERS_VITAGL_VERTEX_PROGRAM_CACHE");
    if (!vglConfigureVertexProgramCache(vertexProgramCache, uniformProfile))
        return d.fail("vertex_program_config", glGetError());
    d.vertexProgramProfile = uniformProfile;
    if (vertexProgramCache || d.vertexProgramProfile)
        std::fprintf(stderr, "[vitagl-vertex-program] cache=%u profile=%u\n",
                     unsigned(vertexProgramCache), unsigned(d.vertexProgramProfile));
    const bool interleavedStreams = enabled("STRIKERS_VITAGL_INTERLEAVED_STREAMS");
    if (!vglConfigureInterleavedStreams(interleavedStreams, uniformProfile))
        return d.fail("vertex_stream_config", glGetError());
    d.vertexStreamProfile = uniformProfile;
    if (interleavedStreams || d.vertexStreamProfile)
        std::fprintf(stderr, "[vitagl-vertex-stream] interleaved=%u profile=%u\n",
                     unsigned(interleavedStreams), unsigned(d.vertexStreamProfile));
    d.affineUv = enabled("STRIKERS_VITAGL_AFFINE_UV");
    d.vertexFog = enabled("STRIKERS_VITAGL_VERTEX_FOG");
    d.bc1Direct = enabled("STRIKERS_VITAGL_BC1_DIRECT");
    {
        const char* range = std::getenv("STRIKERS_VITAGL_FOG_RANGE");
        d.fogNoRange = range && std::strcmp(range, "0") == 0;
    }
    {
        const char* scale = std::getenv("STRIKERS_VITAGL_RENDER_SCALE");
        const int percent = scale ? std::atoi(scale) : 100;
        d.renderScale = percent >= 50 && percent < 100 ? float(percent) / 100.f : 1.f;
        const char* dynamic = std::getenv("STRIKERS_VITAGL_DYNAMIC_SCALE");
        const int low = dynamic ? std::atoi(dynamic) : 0;
        d.lowScale = low >= 50 && float(low) / 100.f < d.renderScale ? float(low) / 100.f : 0.f;
    }
    d.fogOffSpecialize = enabled("STRIKERS_VITAGL_FOG_OFF_SPECIALIZE");
    // Generic fallback uses reference depth and projective UV. Keep the other
    // opt-in arithmetic paths unchanged when a non-4 fog curve is submitted.
    d.fogCurve4Specialize = enabled("STRIKERS_VITAGL_FOG_CURVE4_SPECIALIZE") &&
                           !d.vertexFog && !d.affineUv;
    const char* cull = std::getenv("STRIKERS_VITAGL_CULL_TEST");
    d.cullTest = cull ? unsigned(std::clamp(std::atoi(cull), 0, 2)) : 0;
    if (d.cullTest)
        std::fprintf(stderr, "[vitagl] diagnostic cull mode=%u\n", d.cullTest);
    if (!d.contextInitialized)
    {
        // Explicit pools leave the existing game/resource heaps intact.
        SceKernelFreeMemorySizeInfo mem{};
        mem.size = sizeof mem;
        if (sceKernelGetFreeMemorySize(&mem) < 0)
            return d.fail("memory_query");
        const unsigned ram =
            mem.size_user > 24 * MB ? std::min(64 * MB, unsigned(mem.size_user - 24 * MB)) : 0;
        const unsigned cdram =
            mem.size_cdram > 12 * MB ? std::min(48 * MB, unsigned(mem.size_cdram - 12 * MB)) : 0;
        const unsigned slow =
            mem.size_phycont > 8 * MB ? std::min(8 * MB, unsigned(mem.size_phycont - 8 * MB)) : 0;
        std::fprintf(stderr, "[vitagl] pools user=%u cdram=%u phycont=%u KiB; available=%u/%u/%u\n",
                     ram / 1024, cdram / 1024, slow / 1024, unsigned(mem.size_user),
                     unsigned(mem.size_cdram), unsigned(mem.size_phycont));
        vglUseCachedMem(GL_FALSE);
        vglUseExtraMem(GL_TRUE);
        vglSetCircularPoolSize(12 * MB);
        vglSetupScratchMemory(GL_FALSE, GL_TRUE);
        vglSetDisplayBufferCount(3);
        vglSetupDisplayRenderTarget(1);
        vglSetupShaderPatcher(2 * MB, 512 * 1024, 4 * MB);
        vglSetParamBufferSize(8 * MB);
        // VitaGL returns whether it selected a smaller resolution, not success.
        const bool fallback =
            vglInitWithCustomSizes(0, W, H, ram, cdram, slow, 0, SCE_GXM_MULTISAMPLE_NONE);
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        if (!d.check("initialize") || viewport[2] != int(W) || viewport[3] != int(H))
            return d.fail("initialize_viewport");
        std::fprintf(stderr, "[vitagl] context viewport=%dx%d resolution_fallback=%u\n", viewport[2],
                     viewport[3], unsigned(fallback));
        d.contextInitialized = true;
        if (const char* store = std::getenv("STRIKERS_VITAGL_DISPLAY_DEPTH_STORE"); store && !std::strcmp(store, "0"))
            vglStrikersDisplayDepthTransient();
    }
    // The provider retains its context across renderer resource shutdowns.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, W, H);
    d.initialized = true;
    {
        const char* vsync = std::getenv("STRIKERS_VITA_VSYNC");
        const bool vsyncOn = !(vsync && std::strcmp(vsync, "0") == 0);
        vglWaitVblankStart(vsyncOn ? GL_TRUE : GL_FALSE);
        const char* half = std::getenv("STRIKERS_VITA_CINEMATIC_30");
        d.cinematic30 = vsyncOn && half && std::strcmp(half, "0") != 0;
    }
    // VitaGL uses tightly packed RGBA uploads and does not implement the
    // desktop PACK/UNPACK_ALIGNMENT settings. RGB capture rows are 960*3 bytes.
    glGenBuffers(1, &d.dynamicVbo);
    glGenBuffers(1, &d.dynamicEbo);
    d.main = create_target(unsigned(std::lround(W * d.renderScale)),
                           unsigned(std::lround(H * d.renderScale)));
    if (!d.main)
        return false;
    d.textures.at(d.main).scale = d.renderScale;
    if (d.renderScale != 1.f)
        std::fprintf(stderr, "[vitagl] render scale %.2f: main target %ux%u\n", d.renderScale,
                     d.textures.at(d.main).width, d.textures.at(d.main).height);
    d.mainFull = d.main;
    if (d.lowScale)
    {
        d.mainLow = create_target(unsigned(std::lround(W * d.lowScale)),
                                  unsigned(std::lround(H * d.lowScale)));
        if (d.mainLow)
            d.textures.at(d.mainLow).scale = d.lowScale;
        else
            d.lowScale = 0.f;
    }
    TextureUpload white;
    white.width = white.height = white.mipCount = 1;
    white.stride = 8;
    white.storage = TextureStorage::Rgba8;
    white.pixels.assign(32, 255);
    d.white = create_texture(white);
    return d.white && d.check("initialize_state");
}
bool Renderer::initialize_scene_program() noexcept
{
    auto& d = *impl_;
    if (!d.initialized || !d.on_owner())
        return false;
    if (d.sceneInitialized)
        return true;
    if (d.error)
        return false;
    const unsigned standard = d.affineUv ? 160 : 80;
    const unsigned total = 2 + (standard + 8) * (d.fogOffSpecialize ? 2 : 1);
    port::startup::show("PREPARING SHADERS", 0, total);
    unsigned prepared = 0;
    constexpr unsigned masks[]{ 0,  1,  3,  5,  7,  9,  13, 17, 19, 21,
                                25, 33, 35, 37, 39, 41, 43, 49, 53, 57 };
    for (bool usesBones : { true, false })
    {
        const auto source = scene_vertex_shader(d.vertexFog, usesBones);
        if (source.empty())
            return d.fail("shader_variant_source");
        d.vertexShaders[usesBones] = d.compile(GL_CG_VERTEX_SHADER_EXT, source);
        if (!d.vertexShaders[usesBones])
            return false;
        if (d.vertexFog)
        {
            const auto reference = scene_vertex_shader(false, usesBones);
            if (reference.empty())
                return d.fail("shader_variant_source");
            d.referenceVertexShaders[usesBones] = d.compile(GL_CG_VERTEX_SHADER_EXT, reference);
            if (!d.referenceVertexShaders[usesBones])
                return false;
        }
        const unsigned skinningKey = usesBones ? 0 : rigidProgramBit;
        if (!d.make_program(128 | skinningKey, 63, true, true, false, usesBones))
            return false;
        port::startup::show("PREPARING SHADERS", ++prepared, total);
        for (auto mask : masks)
            for (unsigned alpha = 0; alpha < 2; ++alpha)
                for (unsigned fogOff = 0; fogOff < (d.fogOffSpecialize ? 2u : 1u); ++fogOff)
                {
                    const unsigned key = (mask * 2 + alpha) | skinningKey |
                                         (fogOff ? fogOffProgramBit : 0);
                    if (!d.make_program(key, mask, alpha, false, false, usesBones))
                        return false;
                    port::startup::show("PREPARING SHADERS", ++prepared, total);
                    if (d.affineUv)
                    {
                        if (!d.make_program(key | affineProgramBit, mask, alpha, false, true, usesBones))
                            return false;
                        port::startup::show("PREPARING SHADERS", ++prepared, total);
                    }
                    if (mask == 1)
                        for (unsigned mode : { 3u, 4u })
                        {
                            if (!d.make_program(key | (mode == 3 ? copyIntensityProgramBit : copyAlphaProgramBit),
                                                mask, alpha, false, false, usesBones, mode))
                                return false;
                            port::startup::show("PREPARING SHADERS", ++prepared, total);
                        }
                }
    }
    port::startup::show("STARTING GAME");
    d.sceneInitialized = true;
    glUseProgram(0);
    d.boundProgram = 0;
    std::fprintf(stderr, "[vitagl] scene programs=%u affine_uv=%u vertex_fog=%u fog_off_specialize=%u fog_curve4_specialize=%u; Aurora=0 GX=0\n",
                 unsigned(d.programs.size()), unsigned(d.affineUv), unsigned(d.vertexFog),
                 unsigned(d.fogOffSpecialize), unsigned(d.fogCurve4Specialize));
    return true;
}
uint32_t Renderer::create_texture(const TextureUpload& image) noexcept
{
    return create_texture_keyed(image, 0);
}
bool Renderer::create_texture_as(uint32_t id, const TextureUpload& image) noexcept
{
    return id >= kReservedTextures && create_texture_keyed(image, id) == id;
}
uint32_t Renderer::create_texture_keyed(const TextureUpload& image, uint32_t key) noexcept
{
    try
    {
        auto& d = *impl_;
        const size_t bytes = nativegxm::texture_upload_bytes(image);
        if (!d.initialized || !d.on_owner() || !bytes || image.pixels.size() != bytes)
            return 0;
        const auto start = d.now();
        Texture t;
        TextureLease lease{ t };
        t.width = image.width;
        t.height = image.height;
        t.levels = image.mipCount;
        glGenTextures(1, &t.id);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, t.id);
        d.boundTextures[0] = t.id;
        // BC1 levels must be at least 4x4, so the chain ends at the last whole block.
        const bool compressed =
            d.bc1Direct && image.width >= 4 && image.height >= 4 && texture_bc1_opaque(image);
        if (compressed)
        {
            std::vector<uint8_t> blocks;
            unsigned levels = 0;
            for (unsigned level = 0; level < image.mipCount; ++level)
            {
                unsigned width = 0, height = 0;
                if (!texture_level_bc1(image, level, blocks, width, height))
                {
                    d.fail("texture_decode");
                    return 0;
                }
                if (width < 4 || height < 4)
                    break;
                glCompressedTexImage2D(GL_TEXTURE_2D, level, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, width,
                                       height, 0, GLsizei(blocks.size()), blocks.data());
                if (!d.check("texture_upload"))
                    return 0;
                ++levels;
            }
            t.levels = levels;
        }
        size_t linearOffset = 0;
        for (unsigned level = 0; !compressed && level < image.mipCount; ++level)
        {
            TextureLevel decoded;
            const unsigned width = std::max(1u, image.width >> level),
                           height = std::max(1u, image.height >> level);
            const bool linear =
                image.storage == nativegxm::TextureStorage::Rgba8 && !image.swizzled;
            const unsigned stride = (width + 7u) & ~7u;
            const uint8_t* pixels = nullptr;
            if (image.opaqueLuminance || (linear && stride == width))
                pixels = image.pixels.data() + linearOffset;
            else if (texture_level_rgba(image, level, decoded))
                pixels = decoded.rgba.data();
            else
            {
                d.fail("texture_decode");
                return 0;
            }
            if (!image.opaqueLuminance)
                for (size_t i = 3; i < size_t(width) * height * 4; i += 4)
                    t.minAlpha = std::min(t.minAlpha, pixels[i]);
            const GLenum format = image.opaqueLuminance ? GL_LUMINANCE : GL_RGBA;
            glTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0, format, GL_UNSIGNED_BYTE,
                         pixels);
            if (!d.check("texture_upload"))
                return 0;
            if (linear)
                linearOffset += size_t(stride) * height * 4;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        t.levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        if (!d.check("texture_upload"))
        {
            return 0;
        }
        d.textures.emplace(key ? key : t.id, t);
        lease.committed = true;
        d.current.uploadUs += d.now() - start;
        return key ? key : t.id;
    }
    catch (...)
    {
        impl_->fail("texture_allocation", GL_OUT_OF_MEMORY);
        return 0;
    }
}
uint32_t Renderer::create_target(uint32_t width, uint32_t height, TargetFormat format) noexcept
{
    return create_target_internal(width, height, format, true);
}
uint32_t Renderer::create_target_internal(uint32_t width, uint32_t height, TargetFormat format,
                                          bool depth) noexcept
{
    try
    {
        auto& d = *impl_;
        if (!d.initialized || !d.on_owner() || !width || !height || width > W || height > H)
            return 0;
        if (format != TargetFormat::Rgba8 && format != TargetFormat::Rgb565)
        {
            d.fail("target_format", GL_INVALID_ENUM);
            return 0;
        }
        Texture t;
        TextureLease lease{ t };
        t.width = width;
        t.height = height;
        t.format = format;
        t.minAlpha = format == TargetFormat::Rgb565 ? 255 : 0;
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &t.id);
        glBindTexture(GL_TEXTURE_2D, t.id);
        // The bundled VitaGL maps this public RGB565 allocation to a renderable
        // U5U6U5 color surface. Capture samples therefore have opaque alpha.
        const bool rgb565 = format == TargetFormat::Rgb565;
        glTexImage2D(GL_TEXTURE_2D, 0, rgb565 ? GL_RGB : GL_RGBA, width, height, 0,
                     rgb565 ? GL_RGB : GL_RGBA,
                     rgb565 ? GL_UNSIGNED_SHORT_5_6_5 : GL_UNSIGNED_BYTE, nullptr);
        if (!d.check("target_storage"))
            return 0;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenFramebuffers(1, &t.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.id, 0);
        bool ok = true;
        if (depth)
        {
            glGenRenderbuffers(1, &t.depth);
            glBindRenderbuffer(GL_RENDERBUFFER, t.depth);
            // No stencil: a scene opened by a depth clear then skips loading depth.
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, width, height);
            ok = d.check("target_depth");
            if (ok)
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t.depth);
        }
        ok = ok && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        if (ok)
        {
            glDisable(GL_SCISSOR_TEST);
            glColorMask(1, 1, 1, 1);
            glDepthMask(1);
            glClearColor(0, 0, 0, 0);
            glClearDepthf(1);
            glClear(GL_COLOR_BUFFER_BIT | (depth ? GL_DEPTH_BUFFER_BIT : 0));
        }
        glBindFramebuffer(GL_FRAMEBUFFER, d.surface() ? d.surface()->fbo : 0);
        d.invalidate_fixed();
        d.boundTextures[0] = t.id;
        if (!ok || !d.check("target_create"))
        {
            d.fail("target_incomplete");
            return 0;
        }
        d.textures.emplace(t.id, t);
        lease.committed = true;
        return t.id;
    }
    catch (...)
    {
        impl_->fail("target_allocation", GL_OUT_OF_MEMORY);
        return 0;
    }
}
void Renderer::release_texture(uint32_t id) noexcept
{
    if (!impl_ || !impl_->initialized)
        return;
    auto& d = *impl_;
    auto i = d.textures.find(id);
    if (i == d.textures.end())
        return;
    auto t = i->second;
    for (auto& bound : d.boundTextures)
        if (bound == id)
            bound = 0;
    if (t.fbo)
        glDeleteFramebuffers(1, &t.fbo);
    if (t.depth)
        glDeleteRenderbuffers(1, &t.depth);
    glDeleteTextures(1, &t.id);
    d.textures.erase(i);
}
uint32_t Renderer::create_model(std::span<const ModelVertex> vertices,
                                std::span<const uint16_t> indices) noexcept
{
    return admit_model(0, vertices, indices);
}
void Renderer::create_model_as(uint32_t id, std::span<const ModelVertex> vertices,
                               std::span<const uint16_t> indices) noexcept
{
    if (!impl_ || !id)
        return;
    if (impl_->models.count(id) || impl_->streamedModels.count(id))
    {
        impl_->fail("model_reserved_id", GL_INVALID_VALUE);
        return;
    }
    if (admit_model(id, vertices, indices))
        return;
    try
    {
        impl_->streamedModels[id] = { { vertices.begin(), vertices.end() },
                                      { indices.begin(), indices.end() } };
    }
    catch (...)
    {
        impl_->fail("model_allocation", GL_OUT_OF_MEMORY);
    }
}
uint32_t Renderer::admit_model(uint32_t reserved, std::span<const ModelVertex> vertices,
                               std::span<const uint16_t> indices) noexcept
{
    try
    {
        auto& d = *impl_;
        bool usesBones = false;
        if (!valid_geometry(vertices, indices, usesBones) || !d.on_owner() ||
            (reserved && (d.models.count(reserved) || d.streamedModels.count(reserved))))
            return 0;
        const size_t size = vertices.size_bytes() + indices.size_bytes();
        if (size > 48 * MB - d.geometryBytes)
            return 0;
        const auto start = d.now();
        Model m;
        ModelLease lease{ m };
        m.bytes = size;
        m.count = indices.size();
        m.usesBones = usesBones;
        for (const auto& v : vertices)
        {
            m.minAlpha = std::min(m.minAlpha, v.color[3]);
            if (d.affineUv)
                m.projectiveUvChannels |= scene_projective_uv_channels(v);
        }
        glGenBuffers(1, &m.vbo);
        glGenBuffers(1, &m.ebo);
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size_bytes(), vertices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size_bytes(), indices.data(),
                     GL_DYNAMIC_DRAW);
        if (!d.check("model_upload"))
        {
            return 0;
        }
        const unsigned id = reserved ? reserved : d.nextModel++;
        d.models.emplace(id, m);
        lease.committed = true;
        d.geometryBytes += size;
        d.current.uploadUs += d.now() - start;
        return id;
    }
    catch (...)
    {
        impl_->fail("model_allocation", GL_OUT_OF_MEMORY);
        return 0;
    }
}
void Renderer::release_model(uint32_t id) noexcept
{
    if (!impl_)
        return;
    auto& d = *impl_;
    auto it = d.models.find(id);
    if (it == d.models.end())
    {
        d.streamedModels.erase(id);
        return;
    }
    d.geometryBytes -= it->second.bytes;
    if (d.attributeBuffer == it->second.vbo)
        d.attributeBuffer = 0;
    glDeleteBuffers(1, &it->second.vbo);
    glDeleteBuffers(1, &it->second.ebo);
    d.models.erase(it);
}
bool Renderer::draw_scene(uint32_t model, std::span<const ModelVertex> vertices,
                          std::span<const uint16_t> indices, const std::array<float, 16>& mvp,
                          const Raster& r, const SceneMaterial& m) noexcept
{
    auto& d = *impl_;
    if (!d.active || !d.on_owner() || !r.valid())
        return d.fail("draw_lifecycle");
    for (float f : mvp)
        if (!std::isfinite(f))
            return d.fail("draw_matrix");
    if (r.cull == 3 && !r.primitive)
        return true;
    const auto start = d.now();
    int viewportWidth = 0;
    if (!rounded_viewport_width(m.viewport[2], viewportWidth))
        return d.fail("draw_viewport_width", GL_INVALID_VALUE);
    const bool fogRangeEnabled = m.fog[2] > .5f && m.fogRangeControl[0] != 0.f;
    if (fogRangeEnabled)
    {
        std::array<float, 4> expectedControl{};
        if (!fog_range_controls(m.fog[0], m.fog[1], viewportWidth, m.fogRangeControl[2],
                                  expectedControl) || m.fogRangeControl != expectedControl)
            return d.fail("draw_fog_range_control", GL_INVALID_VALUE);
        for (float coefficient : m.fogRangeTable)
            if (!std::isfinite(coefficient) || coefficient < 1.f / 64.f ||
                coefficient > 4095.f / 64.f)
                return d.fail("draw_fog_range_table", GL_INVALID_VALUE);
    }
    const unsigned primitiveSize = r.primitive ? 2 : 3;
    unsigned count = 0;
    GLuint vertexBuffer = d.dynamicVbo;
    uint8_t alpha = 255;
    unsigned projectiveUvChannels = 0;
    bool usesBones = false;
    if (model)
    {
        auto i = d.models.find(model);
        if (i == d.models.end())
        {
            const auto s = d.streamedModels.find(model);
            if (s == d.streamedModels.end())
                return d.fail("draw_model");
            return draw_scene(0, s->second.vertices, s->second.indices, mvp, r, m);
        }
        count = i->second.count;
        if (count % primitiveSize)
            return d.fail("draw_topology");
        alpha = i->second.minAlpha;
        projectiveUvChannels = i->second.projectiveUvChannels;
        usesBones = i->second.usesBones;
        vertexBuffer = i->second.vbo;
        glBindBuffer(GL_ARRAY_BUFFER, i->second.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, i->second.ebo);
    }
    else
    {
        if (indices.size() % primitiveSize)
            return d.fail("draw_topology");
        if (!valid_geometry(vertices, indices, usesBones))
            return d.fail("draw_geometry");
        count = indices.size();
        for (auto& v : vertices)
        {
            alpha = std::min(alpha, v.color[3]);
            if (d.affineUv)
                projectiveUvChannels |= scene_projective_uv_channels(v);
        }
        glBindBuffer(GL_ARRAY_BUFFER, d.dynamicVbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size_bytes(), vertices.data(), GL_STREAM_DRAW);
        if (!d.check("draw_vertex_upload"))
            return false;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d.dynamicEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size_bytes(), indices.data(), GL_STREAM_DRAW);
        if (!d.check("draw_index_upload"))
            return false;
        d.current.dynamicVertexBytes += vertices.size_bytes();
    }
    d.attributes(vertexBuffer);
    const unsigned mask = unsigned(m.controls[0]);
    // GLUD_ConstantColour replaces both TEV colour and alpha for diffuse-only
    // draws. BallBlurCB uses that alpha for its five translucent rotations.
    // Explicit translucent/no-rasterized-alpha data has priority. Light-ramp
    // programs resolve their separate alpha stages in the fragment shader.
    const float rasterAlpha =
        m.effects[1] >= 0 ? m.effects[1] : (m.effects[0] > .5f && mask == 1 ? m.tint[3] : -1.f);
    bool test = m.controls[1] >= 0;
    if (test)
    {
        float minimum = float(alpha) / 255;
        if (rasterAlpha >= 0)
            minimum = rasterAlpha;
        if (m.environment[2] > 0)
            minimum = 0;
        const auto texalpha = [&](unsigned slot) {
            auto it = d.textures.find(m.textures[slot]);
            return it == d.textures.end() ? 1.f : float(it->second.minAlpha) / 255;
        };
        minimum = m.controls[3] >= 2.f
                      ? 0.f
                      : scene_material_minimum_alpha(mask, minimum, texalpha(0), texalpha(1),
                                                       m.effects[2], m.effects[1], m.material[1] > .5f,
                                                       m.alpha, texalpha(2), texalpha(4));
        if (minimum > m.controls[1])
            test = false;
    }
    const unsigned skinningKey = usesBones ? 0 : rigidProgramBit;
    unsigned key = (mask * 2 + unsigned(test)) | skinningKey;
    const bool affine = d.affineUv && scene_affine_uv_eligible(projectiveUvChannels, mask, m.environment);
    if (affine)
        key |= affineProgramBit;
    if (d.fogOffSpecialize && !(m.fog[2] > .5f))
        key |= fogOffProgramBit;
    const bool copyMode = mask == 1 && (m.controls[3] == 3.f || m.controls[3] == 4.f);
    if (copyMode)
        key = (key & ~affineProgramBit) |
              (m.controls[3] == 3.f ? copyIntensityProgramBit : copyAlphaProgramBit);
    auto it = d.programs.find(key);
    if ((m.controls[3] >= 2 && !copyMode) || it == d.programs.end() ||
        it->second.mask != mask ||
        (d.fogCurve4Specialize && m.fog[2] > .5f && m.fog[3] != 4.f) ||
        (d.vertexFog && (fogRangeEnabled || !scene_vertex_fog_eligible(m.fog))))
        it = d.programs.find(128 | skinningKey);
    if (it == d.programs.end())
        return d.fail("draw_program");
    auto& p = it->second;
    if (d.boundProgram != p.id)
    {
        glUseProgram(p.id);
        d.boundProgram = p.id;
    }
    auto* surface = d.surface();
    if (!surface)
        return d.fail("draw_target");
    if (fogRangeEnabled && m.fogRangeControl[2] != .5f * float(surface->logical_width()))
        return d.fail("draw_fog_range_target", GL_INVALID_VALUE);
    // Range fog reads WPOS in physical pixels.
    const float scale = d.displayDraw ? 1.f : surface->scale;
    auto fogRangeControl = m.fogRangeControl;
    if (scale != 1.f)
    {
        fogRangeControl[1] /= scale;
        fogRangeControl[2] *= scale;
    }
    float lighting[]{ float(m.lit), rasterAlpha, float(m.controls[3] == 1), m.effects[3] };
    const float* values[]{ mvp.data(),
                           m.modelView.data(),
                           m.bones[0].data(),
                           m.environment.data(),
                           m.ambient.data(),
                           m.diffuse.data(),
                           lighting,
                           m.lightPosition[0].data(),
                           m.lightColor[0].data(),
                           m.specular[0].data(),
                           m.specularColor[0].data(),
                           m.controls.data(),
                           m.effects.data(),
                           m.tint.data(),
                           m.shadow.data(),
                           m.shadowAdd.data(),
                           m.fog.data(),
                           m.fogColor.data(),
                           m.material.data(),
                           m.alpha.data(),
                           m.fogRangeTable.data(),
                           fogRangeControl.data() };
    static_assert(std::size(values) == std::size(uniforms));
    for (unsigned u = 0; u < std::size(uniforms); ++u)
        d.uniform(p, u, values[u]);
    for (unsigned a = 0; a < 6; ++a)
    {
        if (!(p.mask & (1u << a)) && !p.generic)
            continue;
        unsigned id = m.textures[a] ? m.textures[a] : d.white;
        auto t = d.textures.find(id);
        if (t == d.textures.end() && id >= kReservedTextures)
        {
            // A failed queued creation draws white, as GX samples an unloaded texture.
            id = d.white;
            t = d.textures.find(id);
        }
        if (t == d.textures.end())
            return d.fail("draw_texture");
        auto& tex = t->second;
        const bool sampler =
            tex.wrapU != m.wrapU[a] || tex.wrapV != m.wrapV[a] || tex.filter != m.filter[a];
        if (d.boundTextures[a] != id || sampler)
        {
            glActiveTexture(GL_TEXTURE0 + a);
            if (d.boundTextures[a] != id)
            {
                glBindTexture(GL_TEXTURE_2D, tex.id);
                d.boundTextures[a] = id;
            }
            if (sampler)
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                m.wrapU[a] ? GL_CLAMP_TO_EDGE : GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                m.wrapV[a] ? GL_CLAMP_TO_EDGE : GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                m.filter[a] ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                tex.levels > 1 ? (m.filter[a] ? GL_LINEAR_MIPMAP_LINEAR
                                                              : GL_NEAREST_MIPMAP_NEAREST)
                                               : (m.filter[a] ? GL_LINEAR : GL_NEAREST));
                tex.wrapU = m.wrapU[a];
                tex.wrapV = m.wrapV[a];
                tex.filter = m.filter[a];
            }
        }
    }
    d.raster(r);
    const int h = int(surface->logical_height());
    std::array<int, 4> viewport{ int(std::lround(m.viewport[0])),
                                 h - int(std::lround(m.viewport[1] + m.viewport[3])),
                                 viewportWidth, int(std::lround(m.viewport[3])) };
    if (scale != 1.f)
        viewport = surface->physical(viewport);
    if (!d.viewportValid || viewport != d.lastViewport)
    {
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        d.lastViewport = viewport;
        d.viewportValid = true;
    }
    // Original scissor edges are top-left physical pixel coordinates.
    const int l = std::max(0, int(std::ceil(m.scissor[0]))),
              t = std::max(0, int(std::ceil(m.scissor[1]))),
              right = std::min(int(surface->logical_width()), int(std::ceil(m.scissor[2]))),
              bottom = std::min(h, int(std::ceil(m.scissor[3])));
    std::array<int, 4> scissor{ l, h - bottom, std::max(0, right - l), std::max(0, bottom - t) };
    if (scale != 1.f)
        scissor = surface->physical(scissor);
    if (!d.scissorValid || scissor != d.lastScissor)
    {
        if (l == 0 && t == 0 && right == int(surface->logical_width()) && bottom == h)
            glDisable(GL_SCISSOR_TEST);
        else
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        }
        d.lastScissor = scissor;
        d.scissorValid = true;
    }
    const auto drawCallStart = d.capture && d.timed ? d.now() : 0;
    glDrawElements(r.primitive ? GL_LINES : GL_TRIANGLES, count, GL_UNSIGNED_SHORT, nullptr);
    const auto drawCallEnd = d.capture && d.timed ? d.now() : 0;
    ++d.current.draws;
    if (d.timed)
    {
        if (key & affineProgramBit && !p.generic)
            ++d.current.affineUvDraws;
        else
            ++d.current.projectiveUvDraws;
        if (p.vertexFog && m.fog[2] > .5f)
            ++d.current.vertexFogDraws;
    }
    const auto drawEnd = d.now();
    d.current.drawUs += drawEnd - start;
    if (d.capture && d.timed)
    {
        d.capture->drawPrepareNs += (drawCallStart - start) * 1000;
        d.capture->drawApiNs += (drawCallEnd - drawCallStart) * 1000;
        d.capture->drawSuffixNs += (drawEnd - drawCallEnd) * 1000;
        ++d.capture->timedDrawScopes;
    }
    return d.check("draw");
}
bool Renderer::select_target(uint32_t texture, bool color, bool depth) noexcept
{
    auto& d = *impl_;
    if (!d.active)
        return d.fail("target_lifecycle");
    auto i = d.textures.find(texture ? texture : d.main);
    if (i == d.textures.end() || !i->second.fbo)
        return d.fail("target_id");
    d.invalidate_fixed();
    d.target = texture;
    glBindFramebuffer(GL_FRAMEBUFFER, i->second.fbo);
    if (color || depth)
    {
        glDisable(GL_SCISSOR_TEST);
        GLbitfield bits = 0;
        if (color)
        {
            glColorMask(1, 1, 1, 1);
            glClearColor(0, 0, 0, 0);
            bits |= GL_COLOR_BUFFER_BIT;
        }
        if (depth)
        {
            glDepthMask(1);
            glClearDepthf(1);
            bits |= GL_DEPTH_BUFFER_BIT;
        }
        glClear(bits);
    }
    return d.check("target_select");
}
bool Renderer::clear_color(std::array<float, 4> c, bool depth) noexcept
{
    auto& d = *impl_;
    if (!d.active)
        return false;
    d.invalidate_fixed();
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glClearColor(c[0], c[1], c[2], c[3]);
    GLbitfield bits = GL_COLOR_BUFFER_BIT;
    if (depth)
    {
        glDepthMask(1);
        glClearDepthf(1);
        bits |= GL_DEPTH_BUFFER_BIT;
    }
    glClear(bits);
    return d.check("clear_color");
}
bool Renderer::clear_depth() noexcept
{
    auto& d = *impl_;
    if (!d.active)
        return false;
    d.invalidate_fixed();
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(1);
    glClearDepthf(1);
    glClear(GL_DEPTH_BUFFER_BIT);
    return d.check("clear_depth");
}
bool Renderer::copy_texture(uint32_t dst, std::array<float, 4> rect, bool intensity) noexcept
{
    TextureCopy c{ dst, rect, 0, intensity };
    return copy_textures({ &c, 1 });
}
bool Renderer::copy_textures(std::span<const TextureCopy> copies) noexcept
{
    auto& d = *impl_;
    if (copies.empty())
        return true;
    if (!d.active || !d.on_owner())
        return d.fail("copy_lifecycle");
    auto* source = d.surface();
    if (!source)
        return d.fail("copy_source");
    const auto src = *source;
    // Validate the whole batch before clearing or overwriting any destination.
    // Allow one float ULP at the scaled framebuffer's right/bottom edges.
    const float logicalWidth = float(src.logical_width()), logicalHeight = float(src.logical_height());
    const double sourceWidth = std::nextafter(logicalWidth, logicalWidth + 1);
    const double sourceHeight = std::nextafter(logicalHeight, logicalHeight + 1);
    for (const auto& c : copies)
    {
        auto t = d.textures.find(c.destination);
        if (t == d.textures.end() || !t->second.fbo || c.destination == src.id)
            return d.fail("copy_target");
        const auto& a = c.sourceRect;
        for (float value : a)
            if (!std::isfinite(value))
                return d.fail("copy_extent");
        if (c.clearSource)
            for (float value : c.clearColor)
                if (!std::isfinite(value))
                    return d.fail("copy_clear_color");
        if (a[0] < 0 || a[1] < 0 || a[2] <= 0 || a[3] <= 0 ||
            double(a[0]) + a[2] > sourceWidth || double(a[1]) + a[3] > sourceHeight)
            return d.fail("copy_extent");
        const bool regional = c.destinationRect != std::array<uint32_t, 4>{};
        if (regional && (!c.destinationRect[2] || !c.destinationRect[3]))
            return d.fail("copy_extent");
        const auto width = regional ? c.destinationRect[2] : t->second.width;
        const auto height = regional              ? c.destinationRect[3]
                            : c.destinationHeight ? c.destinationHeight
                                                  : t->second.height;
        const auto x = regional ? c.destinationRect[0] : 0u;
        const auto y = regional ? c.destinationRect[1] : 0u;
        if (x > t->second.width || width > t->second.width - x || y > t->second.height ||
            height > t->second.height - y)
            return d.fail("copy_extent");
    }
    struct RestoreTarget
    {
        Impl& d;
        unsigned target;
        GLuint framebuffer;
        bool pending = true;
        void restore()
        {
            if (!pending)
                return;
            d.target = target;
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            d.invalidate_fixed();
            pending = false;
        }
        ~RestoreTarget() { restore(); }
    } restore{ d, d.target, src.fbo };
    d.invalidate_fixed();
    for (const auto& c : copies)
    {
        auto t = d.textures.find(c.destination);
        const bool regional = c.destinationRect[2] && c.destinationRect[3];
        const auto height = regional              ? c.destinationRect[3]
                            : c.destinationHeight ? c.destinationHeight
                                                  : t->second.height;
        const auto width = regional ? c.destinationRect[2] : t->second.width;
        const auto x = regional ? c.destinationRect[0] : 0u;
        const auto y = regional ? c.destinationRect[1] : 0u;
        // All EFB copies are now GPU-owned textured draws. VitaGL's
        // glBlitFramebuffer closes/restarts a scene; an explicit color copy
        // uses the same validated source/destination coordinates as A8/I8.
        {
            if (d.target != c.destination)
            {
                d.invalidate_fixed();
                d.target = c.destination;
                glBindFramebuffer(GL_FRAMEBUFFER, t->second.fbo);
            }
            if ((c.intensity || c.alphaOnly) && !c.preserveDestination)
            {
                glDisable(GL_SCISSOR_TEST);
                glColorMask(1, 1, 1, 1);
                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT);
                d.invalidate_fixed();
            }
            ModelVertex v[4];
            v[0].position = { -1, 1, 0, 1 };
            v[1].position = { 1, 1, 0, 1 };
            v[2].position = { 1, -1, 0, 1 };
            v[3].position = { -1, -1, 0, 1 };
            float l = c.sourceRect[0] / logicalWidth,
                  r = (c.sourceRect[0] + c.sourceRect[2]) / logicalWidth,
                  top = 1.f - (c.sourceRect[1] + c.sourceRect[3]) / logicalHeight,
                  b = 1.f - c.sourceRect[1] / logicalHeight;
            v[0].uv = { l, top, 1 };
            v[1].uv = { r, top, 1 };
            v[2].uv = { r, b, 1 };
            v[3].uv = { l, b, 1 };
            uint16_t indices[]{ 0, 1, 2, 0, 2, 3 };
            SceneMaterial m;
            m.textures[0] = src.id;
            m.controls = { 1, -1, 1, c.alphaOnly ? 4.f : (c.intensity ? 3.f : 0.f) };
            m.wrapU[0] = m.wrapV[0] = 1;
            // Engine textures use V=0 for their first/top source row. Store
            // that row at GL texture V=0, with unused A8 rows above it.
            m.viewport = { float(x), float(t->second.height - y - height), float(width),
                           float(height) };
            m.scissor = { 0, 0, float(t->second.width), float(t->second.height) };
            Raster raster;
            raster.depthTest = raster.depthWrite = false;
            raster.writeAlpha = true;
            constexpr std::array<float, 16> identity{
                1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
            };
            if (!draw_scene(0, v, indices, identity, raster, m))
                return false;
        }
        if (c.clearSource)
        {
            // GX copy-clear changes only the captured part of the source EFB.
            // Rebind it explicitly: the preceding copy may have used a draw
            // or a blit, and the destination must retain the captured pixels.
            d.target = restore.target;
            glBindFramebuffer(GL_FRAMEBUFFER, src.fbo);
            d.invalidate_fixed();
            const auto& a = c.sourceRect;
            const auto left = std::lround(a[0]);
            const auto right = std::lround(a[0] + a[2]);
            const auto top = std::lround(a[1]);
            const auto bottom = std::lround(a[1] + a[3]);
            std::array<int, 4> region{ int(left), int(src.logical_height()) - int(bottom),
                                       int(right - left), int(bottom - top) };
            if (src.scale != 1.f)
                region = src.physical(region);
            glEnable(GL_SCISSOR_TEST);
            glScissor(region[0], region[1], region[2], region[3]);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDepthMask(GL_TRUE);
            glClearColor(c.clearColor[0], c.clearColor[1], c.clearColor[2], c.clearColor[3]);
            glClearDepthf(1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!d.check("copy_clear"))
                return false;
        }
    }
    restore.restore();
    return d.check("copy_texture");
}
bool Renderer::set_black(bool black) noexcept
{
    if (!impl_)
        return false;
    auto& d = *impl_;
    if (!d.initialized || !d.on_owner())
        return d.fail("black_lifecycle");
    // VISetBlack only stages registers. Ordinary swaps flush the change and
    // decrement nFirstFrame before deciding whether this image is visible.
    d.stagedBlack = black;
    d.firstFrames = black ? 3u : 0u;
    return true;
}
bool Renderer::present_bitmap(const TextureUpload& image,
                               std::array<float, 4> destination) noexcept
{
    if (!impl_)
        return false;
    auto& d = *impl_;
    if (!d.initialized || !d.sceneInitialized || !d.on_owner())
        return d.fail("bitmap_lifecycle");
    for (float value : destination)
        if (!std::isfinite(value))
            return d.fail("bitmap_extent", GL_INVALID_VALUE);
    if (destination[2] <= 0 || destination[3] <= 0)
        return d.fail("bitmap_extent", GL_INVALID_VALUE);

    struct StopLoadingOnFailure
    {
        Renderer& renderer;
        Impl& d;
        bool success = false;
        ~StopLoadingOnFailure()
        {
            if (!success && d.loading.background)
                d.discard_loading(renderer);
        }
    } failure{*this, d};
    Impl::DisplayScope restore{*this, d};
    if (!restore.valid)
        return false;
    restore.texture = create_texture(image);
    if (!restore.texture)
        return d.fail("bitmap_texture");

    const float left = destination[0] / W * 2.f - 1.f;
    const float right = left + destination[2] / W * 2.f;
    const float top = 1.f - destination[1] / H * 2.f;
    const float bottom = top - destination[3] / H * 2.f;
    ModelVertex vertices[4];
    vertices[0].position = { left, top, 0, 1 };
    vertices[1].position = { right, top, 0, 1 };
    vertices[2].position = { right, bottom, 0, 1 };
    vertices[3].position = { left, bottom, 0, 1 };
    vertices[0].uv = { 0, 0, 1 };
    vertices[1].uv = { 1, 0, 1 };
    vertices[2].uv = { 1, 1, 1 };
    vertices[3].uv = { 0, 1, 1 };
    const uint16_t indices[]{ 0, 1, 2, 0, 2, 3 };
    const std::array<float, 16> identity{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    SceneMaterial material;
    material.textures[0] = restore.texture;
    material.controls[0] = 1;
    material.wrapU[0] = material.wrapV[0] = 1;
    Raster raster;
    raster.depthTest = raster.depthWrite = false;
    raster.blend = 1;

    // The quad uses main's display dimensions, but draws only to FBO zero.
    // No main/target pixels or engine-frame lifetime are changed here.
    d.active = true;
    d.target = 0;
    d.invalidate_fixed();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!d.check("bitmap_clear"))
        return false;
    d.displayDraw = true;
    const bool drawn = draw_scene(0, vertices, indices, identity, raster, material);
    d.displayDraw = false;
    if (!drawn)
        return false;
    vglSwapBuffers(GL_FALSE);
    if (!d.check("bitmap_swap"))
        return false;
    // A swap rotates VitaGL's scratch pools and queues scanout asynchronously.
    // Finish that display transaction before releasing the bitmap or returning
    // to a blocking load; subsequent commands use the next provider pool.
    const int completion = sceGxmDisplayQueueFinish();
    if (completion < 0)
        return d.fail("bitmap_present_completion", completion);
    // A DVD/error message replaces the spinner's source image as well as the
    // displayed image. A later timer tick must not resurrect the old backdrop.
    if (d.loading.active && !d.capture_loading_background())
        return false;
    restore.restore();
    if (!d.check("bitmap_restore"))
        return false;
    d.presented = true;
    // DisplayMessage explicitly clears VI black without changing nFirstFrame.
    d.stagedBlack = false;
    if (!d.startupRetired)
        d.startupRetired = port::startup::retire_if_replaced();
    failure.success = true;
    return true;
}
bool Renderer::set_loading(bool begin, const LoadingIndicator& request) noexcept
{
    if (!impl_)
        return false;
    auto& d = *impl_;
    if (!d.initialized || !d.sceneInitialized || !d.on_owner())
        return d.fail("loading_lifecycle");
    if (!begin)
    {
        d.discard_loading(*this);
        return d.check("loading_stop");
    }
    for (const auto& image : request.images)
        if (image.width != 32 || image.height != 32 || image.stride != 32 ||
            image.mipCount != 1 || image.swizzled || image.storage != TextureStorage::Rgba8 ||
            image.pixels.size() != 32 * 32 * 4)
            return d.fail("loading_image", GL_INVALID_VALUE);
    for (const auto& rectangle : request.rectangles)
    {
        for (float value : rectangle)
            if (!std::isfinite(value))
                return d.fail("loading_extent", GL_INVALID_VALUE);
        if (rectangle[2] <= 0 || rectangle[3] <= 0)
            return d.fail("loading_extent", GL_INVALID_VALUE);
    }
    if (!request.phaseRetraces)
        return d.fail("loading_phase", GL_INVALID_VALUE);
    struct Lease
    {
        Renderer& renderer;
        Impl& d;
        bool committed = false;
        ~Lease() { if (!committed) d.discard_loading(renderer); }
    } lease{*this, d};
    // Restore bindings before tearing down a partially allocated background.
    Impl::DisplayScope restore{*this, d};
    if (!restore.valid)
        return false;
    try
    {
        const auto startRetrace = uint32_t(sceDisplayGetVcount());
        // The provider updates its front index before the display callback has
        // necessarily completed. Never snapshot that surface prematurely.
        const int completion = sceGxmDisplayQueueFinish();
        if (completion < 0)
            return d.fail("loading_capture_completion", completion);
        d.discard_loading(*this);
        if (!d.make_loading_program())
            return false;
        const auto freeBefore = vglMemFree(VGL_MEM_ALL);
        // VitaGL supplies an independent hidden depth surface for scissoring:
        // the transient footprint is color + depth, approximately 4 MiB.
        d.loading.background = create_target_internal(W, H, TargetFormat::Rgba8, false);
        if (!d.loading.background)
            return d.fail("loading_background");
        for (unsigned i = 0; i < 2; ++i)
        {
            d.loading.operations[i] = create_texture(request.images[i]);
            if (!d.loading.operations[i])
                return d.fail("loading_operations");
        }
        const unsigned textures[]{d.loading.background, d.loading.operations[0], d.loading.operations[1]};
        glActiveTexture(GL_TEXTURE0);
        for (auto id : textures)
        {
            glBindTexture(GL_TEXTURE_2D, id);
            d.boundTextures[0] = id;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            auto& texture = d.textures.at(id);
            texture.filter = 0;
            texture.wrapU = texture.wrapV = 1;
        }
        glGenBuffers(4, d.loading.vertices.data());
        glGenBuffers(1, &d.loading.indices);
        const uint16_t indices[]{0, 1, 2, 0, 2, 3};
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d.loading.indices);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof indices, indices, GL_STATIC_DRAW);
        for (unsigned quad = 0; quad < 4; ++quad)
        {
            const auto rectangle = quad ? request.rectangles[quad - 1]
                                        : std::array<float, 4>{0, 0, W, H};
            const float left = rectangle[0] / W * 2.f - 1.f;
            const float right = left + rectangle[2] / W * 2.f;
            const float top = 1.f - rectangle[1] / H * 2.f;
            const float bottom = top - rectangle[3] / H * 2.f;
            ModelVertex vertices[4];
            vertices[0].position = {left, top, 0, 1};
            vertices[1].position = {right, top, 0, 1};
            vertices[2].position = {right, bottom, 0, 1};
            vertices[3].position = {left, bottom, 0, 1};
            vertices[0].uv = {0, 0, 1};
            vertices[1].uv = {1, 0, 1};
            vertices[2].uv = {1, 1, 1};
            vertices[3].uv = {0, 1, 1};
            glBindBuffer(GL_ARRAY_BUFFER, d.loading.vertices[quad]);
            glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);
            if (quad)
                d.loading.regions[quad - 1] = {rectangle[0] / W, rectangle[1] / H,
                                               rectangle[2] / (W * 32.f),
                                               rectangle[3] / (H * 32.f)};
        }
        if (!d.check("loading_resources"))
            return false;
        if (request.blackBackground || !d.presented)
        {
            if (!d.clear_loading_background())
                return false;
        }
        else if (!d.capture_loading_background())
            return false;
        if (request.blackBackground && !d.present_loading(false, 0))
            return false;
        restore.restore();
        if (!d.check("loading_restore"))
            return false;
        d.loading.clock.reset(startRetrace, request.waitRetraces, request.phaseRetraces);
        d.loading.active = true;
        lease.committed = true;
        if (request.blackBackground)
            d.presented = true;
        const auto freeAfter = vglMemFree(VGL_MEM_ALL);
        std::fprintf(stderr, "[vitagl] loading resources pool_delta=%lld bytes; wait=%u phase=%u\n",
                     static_cast<long long>(freeBefore) - static_cast<long long>(freeAfter),
                     request.waitRetraces, request.phaseRetraces);
        return true;
    }
    catch (...)
    {
        return d.fail("loading_allocation", GL_OUT_OF_MEMORY);
    }
}
bool Renderer::loading_tick() noexcept
{
    if (!impl_ || !impl_->loading.active)
        return true;
    auto& d = *impl_;
    if (!d.on_owner())
        return false;
    auto nextClock = d.loading.clock;
    auto nextFirst = d.firstFrames;
    const auto step = nextClock.advance(uint32_t(sceDisplayGetVcount()), nextFirst);
    if (!step.present)
    {
        d.loading.clock = nextClock;
        d.firstFrames = nextFirst;
        return true;
    }
    Impl::DisplayScope restore{*this, d};
    const bool ok = restore.valid && (!step.clearBackground || d.clear_loading_background()) &&
                    d.present_loading(step.icons, step.selected);
    restore.restore();
    if (!ok || !d.check("loading_restore"))
    {
        d.discard_loading(*this);
        return false;
    }
    d.loading.clock = nextClock;
    d.firstFrames = nextFirst;
    if (step.clearBackground)
        d.stagedBlack = false;
    d.presented = true;
    if (step.icons && !d.startupRetired)
        d.startupRetired = port::startup::retire_if_replaced();
    return true;
}
bool Renderer::loading_active() const noexcept { return impl_ && impl_->loading.active; }
bool Renderer::begin_frame(FrameTimeline::Owner* capture) noexcept
{
    auto& d = *impl_;
    if (!d.initialized || d.active || !d.on_owner())
        return d.fail("frame_begin");
    d.current = {};
    if (capture)
    {
        d.capture = capture;
        capture->drawTimingEnabled = d.timed;
    }
    if (d.uniformProfile)
        vglGetUniformCacheStats(&d.uniformStart);
    if (d.vertexProgramProfile)
        vglGetVertexProgramCacheStats(&d.vertexProgramStart);
    if (d.vertexStreamProfile)
        vglGetVertexStreamStats(&d.vertexStreamStart);
    d.attributeBuffer = 0;
    d.attributesEnabled = false;
    d.active = true;
    const int cinematicMode = g_cinematicFrame.load(std::memory_order_relaxed);
    d.frameCinematic = cinematicMode != 0;
    d.frameHalfRate = cinematicMode == 1;
    if (d.mainLow)
    {
        d.main = d.dynamicFrames ? d.mainLow : d.mainFull;
        if (d.dynamicFrames)
            --d.dynamicFrames;
    }
    const bool result = select_target(0, true, true);
    if (!result && d.capture) d.capture = nullptr;
    return result;
}
bool Renderer::end_frame(bool present) noexcept
{
    auto& d = *impl_;
    if (!d.active || !d.on_owner())
        return d.fail("frame_end");
    struct CaptureEnd
    {
        Impl& state;
        ~CaptureEnd() { if (state.capture) state.capture = nullptr; }
    } captureEnd{d};
    if (d.capture) d.capture->presentRequested = present && !d.loading.active;
    if (!present || d.loading.active)
    {
        // glSendFrame aborted before submitting the incomplete render list.
        // A swap here would replace the loading screen with the cleared FBO.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        const bool uniformCounters = d.capture_uniform_timing();
        const bool vertexCounters = d.capture_vertex_program_timing();
        const bool counters = d.capture_vertex_stream_timing() && vertexCounters && uniformCounters;
        d.completed = d.current;
        d.active = false;
        return d.check("frame_discard") && counters;
    }
    const auto start = d.now();
    const unsigned nextFirstFrames = d.firstFrames ? d.firstFrames - 1 : 0;
    const bool black = d.stagedBlack && !(d.firstFrames && !nextFirstFrames);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    if (black)
    {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    else
    {
        const auto& main = d.textures.at(d.main);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, main.fbo);
        glBlitFramebuffer(0, 0, main.width, main.height, 0, 0, W, H, GL_COLOR_BUFFER_BIT,
                          main.scale == 1.f ? GL_NEAREST : GL_LINEAR);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!d.check(black ? "present_black" : "present_blit"))
        return false;
    const auto swapStart = d.capture && d.timed ? d.now() : 0;
    if (d.cinematic30)
    {
        const unsigned interval = d.frameHalfRate ? 2u : 1u;
        if (interval != d.swapInterval)
        {
            eglSwapInterval(EGL_NO_DISPLAY, EGLint(interval));
            d.swapInterval = interval;
        }
    }
    {
        Impl::PresentProfileScope profile{d.capture, d.timed};
        vglSwapBuffers(GL_FALSE);
    }
    if (d.mainLow)
    {
        const unsigned vcount = unsigned(sceDisplayGetVcount());
        if (d.lastPresentVcount && vcount - d.lastPresentVcount > d.swapInterval && !d.frameCinematic)
            d.dynamicFrames = d.dynamicHold;
        d.lastPresentVcount = vcount;
    }
    const auto swapEnd = d.capture && d.timed ? d.now() : 0;
    const bool result = d.check("present");
    const auto presentEnd = d.now();
    d.current.queueUs += presentEnd - start;
    if (d.capture && d.timed)
    {
        d.capture->presentPrefixNs += (swapStart - start) * 1000;
        d.capture->presentSwapNs += (swapEnd - swapStart) * 1000;
        d.capture->presentSuffixNs += (presentEnd - swapEnd) * 1000;
        ++d.capture->timedPresentScopes;
    }
    const bool uniformCounters = d.capture_uniform_timing();
    const bool vertexCounters = d.capture_vertex_program_timing();
    const bool counters = d.capture_vertex_stream_timing() && vertexCounters && uniformCounters;
    d.completed = d.current;
    d.active = false;
    if (!result || !counters)
        return false;
    d.firstFrames = nextFirstFrames;
    d.stagedBlack = black;
    if (!black && !d.startupRetired)
        d.startupRetired = port::startup::retire_if_replaced();
    d.presented = true;
    d.report_uniform_verification();
    return true;
}
bool Renderer::frame_active() const noexcept { return impl_ && impl_->active; }
bool Renderer::readback_presented_rgba8(std::vector<uint8_t>& out) noexcept
{
    try
    {
        auto& d = *impl_;
        if (d.active || !d.presented || !d.on_owner())
            return false;
        // GL_FRONT advances when the swap is queued. The provider's fast
        // readback copies CPU pixels without waiting for that display job.
        const int completion = sceGxmDisplayQueueFinish();
        if (completion < 0)
            return d.fail("readback_present_completion", completion);
        out.resize(W * H * 4);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glReadBuffer(GL_FRONT);
        glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
        glReadBuffer(GL_BACK);
        if (!d.check("readback"))
            return false;
        for (unsigned y = 0; y < H / 2; ++y)
            for (unsigned x = 0; x < W * 4; ++x)
                std::swap(out[y * W * 4 + x], out[(H - 1 - y) * W * 4 + x]);
        return true;
    }
    catch (...)
    {
        return false;
    }
}
void Renderer::shutdown() noexcept
{
    if (!impl_ || !impl_->initialized)
        return;
    auto& d = *impl_;
    if (d.capture)
    {
        d.capture->cancelled = true;
        d.capture = nullptr;
    }
    d.discard_loading(*this);
    while (!d.models.empty())
        release_model(d.models.begin()->first);
    d.streamedModels.clear();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    while (!d.textures.empty())
        release_texture(d.textures.begin()->first);
    glUseProgram(0);
    for (auto& [_, p] : d.programs)
        glDeleteProgram(p.id);
    d.programs.clear();
    if (d.loadingProgram)
        glDeleteProgram(d.loadingProgram);
    d.loadingProgram = 0;
    d.loadingRegion = -1;
    for (auto shader : d.vertexShaders)
        if (shader)
            glDeleteShader(shader);
    for (auto shader : d.referenceVertexShaders)
        if (shader)
            glDeleteShader(shader);
    glDeleteBuffers(1, &d.dynamicVbo);
    glDeleteBuffers(1, &d.dynamicEbo);
    d.vertexShaders.fill(0);
    d.referenceVertexShaders.fill(0);
    d.dynamicVbo = d.dynamicEbo = 0;
    d.main = d.target = d.white = d.boundProgram = 0;
    d.mainFull = d.mainLow = 0;
    d.boundTextures.fill(0);
    d.invalidate_fixed();
    d.geometryBytes = 0;
    d.initialized = d.sceneInitialized = d.active = d.presented = false;
    d.startupRetired = false;
    d.stagedBlack = false;
    d.firstFrames = 0;
    d.owner = -1;
}
const char* Renderer::last_error() const noexcept
{
    return impl_ ? impl_->operation : "allocation";
}
int Renderer::error_code() const noexcept { return impl_ ? impl_->error : GL_OUT_OF_MEMORY; }
Renderer::FrameTiming Renderer::completed_timing() const noexcept
{
    return impl_ ? impl_->completed : FrameTiming{};
}
}   // namespace port::render
#endif

extern "C" void PortRenderSetCinematic(int cinematic)
{
    port::render::g_cinematicFrame.store(cinematic, std::memory_order_relaxed);
}
