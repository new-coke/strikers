// Execute the actual application renderer against the VitaGL public API.
// These are host ownership/state/error tests; shaders and pixels need a Vita.
#include "port/renderer.hpp"
#include "port/scene_texture_sampling.hpp"
#include "port/scene_specular.hpp"
#include "port/native_boot_screen.hpp"
#include "port/loading_indicator_state.hpp"
#include <vitaGL.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/display.h>
#include <cassert>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <limits>
#include <cmath>
namespace
{
GLuint next = 1, array = 0, elements = 0, program = 0, drawFb = 0, readFb = 0, texture = 0;
unsigned activeTexture = 0;
std::array<GLuint, 6> textureUnits{};
unsigned swaps = 0, blits = 0, scissors = 0, uniformWrites = 0;
unsigned boneUniformWrites = 0, programCreations = 0;
unsigned shaderCompilations = 0;
unsigned contextInitializations = 0, bufferUploads = 0;
unsigned uniformConfigurations = 0, uniformSnapshots = 0;
bool failUniformConfigure = false;
vglUniformCacheStats uniformStats{};
vglVertexStreamStats streamStats{};
unsigned streamConfigurations=0, streamSnapshots=0;
bool failStreamConfigure=false;
vglVertexProgramCacheStats vertexStats{};
unsigned vertexConfigurations=0, vertexSnapshots=0;
bool failVertexConfigure=false;
unsigned framebufferBinds = 0, clears = 0;
unsigned attributeEnables = 0, attributePointers = 0;
unsigned completionWaits = 0, startupRetireCalls = 0;
unsigned completedSwaps = 0, retraceQueries = 0, memoryQueries = 0;
unsigned readbacks = 0, readBufferChanges = 0;
unsigned gpuFinishes = 0;
unsigned systemClockQueries = 0, presentProfileSets = 0;
PortVglPresentProfile* activePresentProfile = nullptr;
PortVglTimelineClock activePresentClock = nullptr;
std::set<GLuint> unfinishedLoadingTextures;
uint32_t retrace = 0;
int displayCompletionResult = 0;
bool failTexture = false, failClear = false, failSwap = false;
int failTextureLevel = -1;
GLenum failBufferTarget = 0;
bool failDepthStorage = false;
unsigned textureUploads = 0, depthAttachments = 0;
GLuint pendingBitmapTexture = 0;
enum class DisplayContent { None, Black, Scene, Bitmap, Loading };
DisplayContent backContent = DisplayContent::None;
std::vector<DisplayContent> displayContents;
std::map<GLuint, unsigned> surfaceWrites;
std::map<GLuint, GLuint> framebufferColors;
std::map<GLuint, DisplayContent> textureContents;
struct LoadingDraw {
    GLuint background, operations;
    std::array<float, 4> region;
};
std::array<float, 4> loadingRegion{};
std::vector<LoadingDraw> loadingDraws;
std::array<GLuint, 10> attributeBuffers{};
GLenum blendSource = 0, blendDestination = 0, frontFace = 0, cullFace = 0;
GLenum error = GL_NO_ERROR, readMode = GL_BACK;
SceUID thread = 1;
bool failCompile = false, failTarget = false, failInit = false, failDraw = false, failBlit = false;
int failDrawCountdown = -1;
bool omitBoneUniform = false, omitLoadingUniform = false;
bool failRigidCompile = false, failRigidLink = false, forceRigidBoneUniform = false;
bool failFogOffCompile = false, failFogOffLink = false, forceFogOffRangeUniform = false;
std::map<GLuint, std::vector<uint8_t>> buffers;
std::map<GLuint, std::map<GLenum, GLint>> textureParameters;
struct TextureAllocation
{
    GLint internalFormat;
    GLsizei width, height;
    GLenum format, type;
    bool hasData;
    bool operator==(const TextureAllocation&) const = default;
};
std::map<GLuint, TextureAllocation> textureAllocations;
std::set<GLuint> textures, framebuffers, renderbuffers, programs, shaders;
std::map<GLuint, GLenum> shaderStages;
std::map<GLuint, std::string> shaderSources;
std::map<GLuint, std::map<GLenum, std::string>> programSources;
std::map<GLuint, bool> fogOffByProgram;
std::vector<std::array<unsigned, 2>> shaderProgress;
bool program_uses_bones(GLuint id)
{
    return programSources.at(id).at(GL_CG_VERTEX_SHADER_EXT).find("uniform float4 u_bones[") != std::string::npos;
}
bool fog_off_source(const std::string& source)
{
    return source.find("float mask=") != std::string::npos &&
           source.find("uniform float4 u_fog,") == std::string::npos;
}
bool program_fog_off(GLuint id)
{
    return fogOffByProgram.at(id);
}
const char* names[]{ "u_mvp",           "u_mv",       "u_bones",    "u_environment", "u_ambient",
                     "u_diffuse",       "u_lighting", "u_lightpos", "u_lightcolor",  "u_specular",
                     "u_specularcolor", "u_controls", "u_effects",  "u_tint",        "u_shadow",
                     "u_shadowadd",     "u_fog",      "u_fogcolor", "u_material", "u_alpha",
                     "u_fogrange", "u_fograngecontrol" };
unsigned capacities[]{ 4, 4, port::render::kSceneBoneRows, 1, 1, 1, 1, 8, 8, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1 };
static_assert(std::size(capacities) == std::size(names));
enum class FogReflectionFault
{
    None, MissingTable, MissingControl, ArrayName, TableType, TableZero, TableOne,
    TableTwo, ControlZero, MissingReflection, OversizedControl, OversizedTable, ControlType
};
FogReflectionFault fogReflectionFault = FogReflectionFault::None;
unsigned fogTableWrites = 0, fogControlWrites = 0;
std::map<GLuint, std::vector<float>> fogTableByProgram, fogControlByProgram;
bool uniform_present(GLuint id, unsigned index)
{
    if ((index == 16 || index == 17 || index == 20 || index == 21) && program_fog_off(id))
        return forceFogOffRangeUniform && index == 20;
    return index != 2 || program_uses_bones(id) || forceRigidBoneUniform;
}
std::vector<unsigned> reflected_uniforms(GLuint id)
{
    std::vector<unsigned> result;
    for (unsigned i = 0; i < std::size(names); ++i)
        if (uniform_present(id, i))
            result.push_back(i);
    return result;
}
std::array<int, 4> viewport{}, scissor{};
std::array<bool, 4> colorMask{};
bool depthMask = false;
std::array<float, 4> clearColor{};
float clearDepth = 0;
struct ClearCall
{
    GLuint target;
    GLbitfield mask;
    std::array<int, 4> scissor;
    bool scissorEnabled;
    std::array<bool, 4> colorMask;
    bool depthMask;
    std::array<float, 4> color;
    float depth;
};
std::vector<ClearCall> clearCalls;
std::set<GLenum> enabled;
std::vector<std::vector<uint8_t>> submitted;
std::vector<GLuint> submittedTargets;
std::vector<GLenum> submittedPrimitives;
std::vector<std::array<int, 4>> submittedViewports;
std::array<int, 4> lastBlitSource{}, lastBlitDestination{};
GLuint lastBlitReadFb = 0, lastBlitDrawFb = 0;
std::vector<float> bones;
std::vector<float> specularDirections, specularColors, material;
std::vector<float> controls;
std::vector<float> alphaState;
std::map<GLuint,std::vector<float>> lightingByProgram;
std::map<GLuint,std::vector<float>> tintByProgram;
void generate(GLsizei n, GLuint* ids, std::set<GLuint>& live)
{
    while (n--)
    {
        *ids = next++;
        assert(live.insert(*ids++).second);
    }
}
void erase(GLsizei n, const GLuint* ids, std::set<GLuint>& live)
{
    while (n--)
    {
        assert(live.erase(*ids++));
    }
}
void empty()
{
    assert(buffers.empty() && textures.empty() && framebuffers.empty() && renderbuffers.empty() &&
           programs.empty() && shaders.empty());
}
// Numeric oracle for the bundled provider, independent of the renderer's copy
// coordinates: framebuffers.c creates NDC Y=1-2*dstY/H and source V=1-srcY/H.
// misc.c flips the FBO viewport; display viewport Y instead remains negative.
struct CopyCorner
{
    float x, row, u, v;
};
std::array<CopyCorner, 4> provider_blit_corners(
    const std::array<int, 4>& source, const std::array<int, 4>& destination,
    float sourceWidth, float sourceHeight, float destinationHeight, bool display,
    bool sourceDisplay = false)
{
    const auto corner = [&](unsigned x, unsigned y) {
        return CopyCorner{float(destination[x]),
                          display ? float(destination[y])
                                  : destinationHeight - destination[y],
                          source[x] / sourceWidth,
                          sourceDisplay ? source[y] / sourceHeight : 1.f - source[y] / sourceHeight};
    };
    return {corner(0, 1), corner(0, 3), corner(2, 3), corner(2, 1)};
}
std::array<CopyCorner, 4> provider_quad_corners(
    const port::render::ModelVertex (&quad)[4], const std::array<int, 4>& submittedViewport)
{
    std::array<CopyCorner, 4> result{};
    // glViewport uses integer half extents. Exact-equivalence cases below use
    // even dimensions, as do production ScreenGrab and shadow atlas copies.
    for (unsigned i = 0; i < result.size(); ++i)
        result[i] = {submittedViewport[0] + (quad[i].position[0] + 1.f) * (submittedViewport[2] >> 1),
                     submittedViewport[1] + (quad[i].position[1] + 1.f) * (submittedViewport[3] >> 1),
                     quad[i].uv[0], quad[i].uv[1]};
    return result;
}
void assert_same_copy_corners(const std::array<CopyCorner, 4>& a,
                             const std::array<CopyCorner, 4>& b)
{
    for (const auto& corner : a)
    {
        unsigned matches = 0;
        for (const auto& other : b)
            matches += std::fabs(corner.x - other.x) < .00001f &&
                       std::fabs(corner.row - other.row) < .00001f &&
                       std::fabs(corner.u - other.u) < .00001f &&
                       std::fabs(corner.v - other.v) < .00001f;
        assert(matches == 1);
    }
}
void check_loading_retrace_clock()
{
    // Independent transcription of vi_post_cb, loading_indicator and
    // DrawLoadingIndicator: execute each original retrace in order, including
    // black resets between wakeups. Compare the last visible result to folding.
    for (unsigned phaseTicks : {1u, 16u, 19u, 1000u})
        for (unsigned waitTicks : {0u, 1u, 2u, 100u, 120u})
            for (unsigned initialFirst : {0u, 1u, 3u})
                for (uint32_t start : {0u, UINT32_MAX - 300})
                {
                    port::render::LoadingIndicatorClock clock;
                    clock.reset(start, waitTicks, phaseTicks);
                    uint32_t retrace = start, random = 0x61490237u;
                    unsigned first = initialFirst, referenceFirst = initialFirst;
                    unsigned referenceFrame = 0, counter = 0, selected = 0;
                    for (unsigned step = 0; step < 200; ++step)
                    {
                        if (step == 47 || step == 139)
                            first = referenceFirst = 3;
                        random = random * 1664525u + 1013904223u;
                        const auto delta = random % 73;
                        port::render::LoadingAdvance expected;
                        for (unsigned tick = 0; tick < delta; ++tick)
                        {
                            ++referenceFrame;
                            if (referenceFrame < waitTicks)
                                continue;
                            if (!referenceFirst)
                            {
                                expected.present = expected.icons = true;
                                expected.selected = selected;
                                if (++counter >= phaseTicks)
                                {
                                    counter = 0;
                                    if (++selected >= 3)
                                        selected = 0;
                                }
                            }
                            if (referenceFirst && !--referenceFirst)
                            {
                                expected.clearBackground = expected.present = true;
                                expected.icons = false;
                            }
                        }
                        retrace += delta;
                        const auto actual = clock.advance(retrace, first);
                        assert(first == referenceFirst && actual.present == expected.present &&
                               actual.clearBackground == expected.clearBackground &&
                               actual.icons == expected.icons);
                        if (actual.icons)
                            assert(actual.selected == expected.selected);
                    }
                }
    // A full uint32 retrace span must be handled in constant time, including
    // counter wrap and a black-to-icons transition in the same delayed wakeup.
    port::render::LoadingIndicatorClock clock;
    clock.reset(9, 0, 19);
    unsigned first = 3;
    auto result = clock.advance(8, first);
    const uint64_t draws = uint64_t(UINT32_MAX) - 3;
    assert(!first && result.clearBackground && result.present && result.icons &&
           result.selected == ((draws - 1) / 19) % 3);
    result = clock.advance(8, first);
    assert(!result.present && !result.clearBackground && !result.icons);
    result = clock.advance(10, first);
    assert(result.icons && result.selected == ((draws + 1) / 19) % 3);
}
}   // namespace
namespace port::startup
{
bool begin() noexcept { return true; }
void show(const char* label, unsigned current, unsigned total) noexcept
{
    if (std::strcmp(label, "PREPARING SHADERS") == 0)
        shaderProgress.push_back({current, total});
}
bool retire_if_replaced() noexcept { ++startupRetireCalls; return true; }
void shutdown() noexcept {}
}
extern "C" {
SceUID sceKernelGetThreadId() { return thread; }
int sceDisplayGetVcount() { ++retraceQueries; return int(retrace); }
size_t vglMemFree(vglMemType type) { assert(type == VGL_MEM_ALL); ++memoryQueries; return 32u * 1024 * 1024; }
SceInt64 sceKernelGetSystemTimeWide()
{
    ++systemClockQueries;
    static SceInt64 n = 0;
    return ++n;
}
int sceKernelGetFreeMemorySize(SceKernelFreeMemorySizeInfo* m)
{
    m->size_user = 200u * 1024 * 1024;
    m->size_cdram = 80u * 1024 * 1024;
    m->size_phycont = 16u * 1024 * 1024;
    return 0;
}
GLenum glGetError()
{
    auto e = error;
    error = GL_NO_ERROR;
    return e;
}
void vglUseCachedMem(GLboolean) {}
void vglUseExtraMem(GLboolean) {}
void vglSetCircularPoolSize(uint32_t) {}
void vglSetupScratchMemory(GLboolean, GLboolean) {}
void vglSetDisplayBufferCount(int) {}
void vglSetupDisplayRenderTarget(uint8_t) {}
void vglSetupShaderPatcher(uint32_t, uint32_t, uint32_t) {}
void vglSetParamBufferSize(uint32_t) {}
void vglSetShaderCachePath(const char*) {}
GLboolean vglConfigureVertexProgramCache(GLboolean cache, GLboolean profile)
{
    assert(programs.empty());
    ++vertexConfigurations;
    if (failVertexConfigure) { error=GL_INVALID_OPERATION; return GL_FALSE; }
    vertexStats={};vertexStats.enabled=cache;vertexStats.profiling=profile;return GL_TRUE;
}
void vglGetVertexProgramCacheStats(vglVertexProgramCacheStats* out)
{
    ++vertexSnapshots;*out=vertexStats;
}
GLboolean vglConfigureInterleavedStreams(GLboolean enabled, GLboolean profile)
{
    assert(programs.empty());++streamConfigurations;
    if (failStreamConfigure) { error=GL_INVALID_OPERATION; return GL_FALSE; }
    streamStats={};streamStats.enabled=enabled;streamStats.profiling=profile;return GL_TRUE;
}
void vglGetVertexStreamStats(vglVertexStreamStats* out)
{
    ++streamSnapshots;*out=streamStats;
}
GLboolean vglConfigureUniformCache(GLboolean cache, GLboolean profile, GLboolean verify)
{
    assert(programs.empty()); // Configuration precedes every shader link.
    ++uniformConfigurations;
    if (failUniformConfigure || (verify && (!cache || profile)))
    {
        error = GL_INVALID_OPERATION;
        return GL_FALSE;
    }
    uniformStats = {};
    uniformStats.enabled = cache;
    uniformStats.profiling = profile;
    uniformStats.verifying = verify;
    return GL_TRUE;
}
void vglGetUniformCacheStats(vglUniformCacheStats* stats)
{
    ++uniformSnapshots;
    *stats = uniformStats;
}
GLboolean vglInitWithCustomSizes(int, int w, int h, int ram, int cdram, int, int,
                                 SceGxmMultisampleMode)
{
    assert(w == 960 && h == 544 && ram > 0 && cdram > 0);
    ++contextInitializations;
    viewport = failInit ? std::array<int, 4>{} : std::array<int, 4>{ 0, 0, w, h };
    error = failInit ? GL_INVALID_OPERATION : GL_NO_ERROR;
    return GL_FALSE;   // Native-size initialization succeeds with false.
}
void glGetIntegerv(GLenum key, GLint* data)
{
    if (key == GL_VIEWPORT)
        std::copy(viewport.begin(), viewport.end(), data);
    else if (key == GL_FRAMEBUFFER_BINDING)
        *data = drawFb;
    else
    {
        assert(key == GL_READ_FRAMEBUFFER_BINDING);
        *data = readFb;
    }
}
void vglWaitVblankStart(GLboolean) {}
void glPixelStorei(GLenum pname, GLint) { assert(pname == GL_UNPACK_ROW_LENGTH); }
void glGenBuffers(GLsizei n, GLuint* ids)
{
    while (n--)
    {
        *ids = next++;
        assert(buffers.emplace(*ids++, std::vector<uint8_t>{}).second);
    }
}
void glBindBuffer(GLenum t, GLuint id)
{
    assert(!id || buffers.count(id));
    if (t == GL_ARRAY_BUFFER)
        array = id;
    else
    {
        assert(t == GL_ELEMENT_ARRAY_BUFFER);
        elements = id;
    }
}
void glBufferData(GLenum t, GLsizei size, const GLvoid* data, GLenum)
{
    ++bufferUploads;
    auto id = t == GL_ARRAY_BUFFER ? array : elements;
    assert(size > 0 && buffers.count(id));
    auto& v = buffers.at(id);
    if (t == failBufferTarget)
    {
        v.clear();
        error = GL_OUT_OF_MEMORY;
        return;
    }
    v.resize(size);
    if (data)
        std::memcpy(v.data(), data, size);
}
void glDeleteBuffers(GLsizei n, const GLuint* ids)
{
    while (n--)
        assert(buffers.erase(*ids++));
}
void glGetBufferParameteriv(GLenum t, GLenum pname, GLint* v)
{
    assert(pname == GL_BUFFER_SIZE);
    *v = buffers.at(t == GL_ARRAY_BUFFER ? array : elements).size();
}
void glGenTextures(GLsizei n, GLuint* ids) { generate(n, ids, textures); }
void glDeleteTextures(GLsizei n, const GLuint* ids)
{
    for (GLsizei i = 0; i < n; ++i)
    {
        assert(!unfinishedLoadingTextures.count(ids[i]));
        textureAllocations.erase(ids[i]);
        textureContents.erase(ids[i]);
        textureParameters.erase(ids[i]);
        for (auto& binding : textureUnits)
            if (binding == ids[i]) binding = 0;
        if (pendingBitmapTexture == ids[i])
            pendingBitmapTexture = 0;
    }
    erase(n, ids, textures);
}
void glActiveTexture(GLenum unit) {
    assert(unit >= GL_TEXTURE0 && unit < GL_TEXTURE0 + textureUnits.size());
    activeTexture = unit - GL_TEXTURE0;
    texture = textureUnits[activeTexture];
}
void glBindTexture(GLenum, GLuint id)
{
    assert(!id || textures.count(id));
    texture = id;
    textureUnits[activeTexture] = id;
}
void glTexImage2D(GLenum, GLint level, GLint internalFormat, GLsizei w, GLsizei h, GLint,
                  GLenum format, GLenum type, const GLvoid* data)
{
    assert(textures.count(texture) && w > 0 && h > 0);
    ++textureUploads;
    if (level == 0)
        textureAllocations[texture] = {internalFormat, w, h, format, type, data != nullptr};
    if (failTexture || level == failTextureLevel)
        error = GL_OUT_OF_MEMORY;
}
void glTexParameteri(GLenum target, GLenum name, GLint value)
{
    assert(target == GL_TEXTURE_2D && textures.count(texture));
    textureParameters[texture][name] = value;
}
void glGenFramebuffers(GLsizei n, GLuint* ids) { generate(n, ids, framebuffers); }
void glDeleteFramebuffers(GLsizei n, const GLuint* ids) {
    for (GLsizei i = 0; i < n; ++i) framebufferColors.erase(ids[i]);
    erase(n, ids, framebuffers);
}
void glBindFramebuffer(GLenum target, GLuint id)
{
    ++framebufferBinds;
    assert(!id || framebuffers.count(id));
    if (target != GL_READ_FRAMEBUFFER)
        drawFb = id;
    if (target != GL_DRAW_FRAMEBUFFER)
        readFb = id;
}
void glFinish()
{
    // The provider closes the previous scene and opens the currently bound
    // target. The retiring private loading FBO must not become that new scene.
    assert(drawFb == 0);
    ++gpuFinishes;
    unfinishedLoadingTextures.clear();
}
void glFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint id, GLint)
{
    assert(framebuffers.count(drawFb) && textures.count(id));
    framebufferColors[drawFb] = id;
}
void glGenRenderbuffers(GLsizei n, GLuint* ids) { generate(n, ids, renderbuffers); }
void glDeleteRenderbuffers(GLsizei n, const GLuint* ids) { erase(n, ids, renderbuffers); }
void glBindRenderbuffer(GLenum, GLuint id) { assert(renderbuffers.count(id)); }
void glRenderbufferStorage(GLenum, GLenum fmt, GLsizei, GLsizei)
{
    assert(fmt == GL_DEPTH24_STENCIL8);
    if (failDepthStorage)
        error = GL_OUT_OF_MEMORY;
}
void glFramebufferRenderbuffer(GLenum, GLenum attachment, GLenum, GLuint id)
{
    assert(attachment == GL_DEPTH_STENCIL_ATTACHMENT && renderbuffers.count(id));
    ++depthAttachments;
}
GLenum glCheckFramebufferStatus(GLenum)
{
    return failTarget ? GL_INVALID_ENUM : GL_FRAMEBUFFER_COMPLETE;
}
void glEnable(GLenum v) { enabled.insert(v); }
void glDisable(GLenum v) { enabled.erase(v); }
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    colorMask = { bool(r), bool(g), bool(b), bool(a) };
}
void glDepthMask(GLboolean value) { depthMask = value; }
void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { clearColor = { r, g, b, a }; }
void glClearDepthf(GLfloat value) { clearDepth = value; }
void glClear(GLbitfield mask)
{
    ++clears;
    clearCalls.push_back({ drawFb, mask, scissor, bool(enabled.count(GL_SCISSOR_TEST)),
                           colorMask, depthMask, clearColor, clearDepth });
    ++surfaceWrites[drawFb];
    if (!drawFb && (mask & GL_COLOR_BUFFER_BIT))
        backContent = DisplayContent::Black;
    else if (mask & GL_COLOR_BUFFER_BIT)
        textureContents[framebufferColors.at(drawFb)] = DisplayContent::Black;
    if (failClear)
        error = GL_INVALID_OPERATION;
}
GLuint glCreateShader(GLenum stage)
{
    GLuint id;
    generate(1, &id, shaders);
    shaderStages[id] = stage;
    return id;
}
void glDeleteShader(GLuint id) { erase(1, &id, shaders); shaderStages.erase(id); shaderSources.erase(id); }
void glShaderSource(GLuint id, GLsizei count, const GLchar* const* source, const GLint*)
{
    assert(shaders.count(id) && count == 1);
    shaderSources[id] = *source;
}
void glCompileShader(GLuint) { ++shaderCompilations; }
void glGetShaderiv(GLuint shader, GLenum key, GLint* v)
{
    const bool rigid = shaderStages.at(shader) == GL_CG_VERTEX_SHADER_EXT &&
                       shaderSources.at(shader).find("uniform float4 u_bones[") == std::string::npos;
    const bool fogOff = shaderStages.at(shader) == GL_CG_FRAGMENT_SHADER_EXT &&
                        fog_off_source(shaderSources.at(shader));
    *v = key == GL_COMPILE_STATUS ? !(failCompile || (failRigidCompile && rigid) ||
                                     (failFogOffCompile && fogOff)) : 1;
}
void glGetShaderInfoLog(GLuint, GLsizei, GLsizei*, GLchar* log) { *log = 0; }
GLuint glCreateProgram()
{
    GLuint id;
    generate(1, &id, programs);
    ++programCreations;
    return id;
}
void glDeleteProgram(GLuint id) { erase(1, &id, programs); programSources.erase(id); fogOffByProgram.erase(id); }
void glAttachShader(GLuint p, GLuint s) {
    assert(programs.count(p) && shaders.count(s));
    programSources[p][shaderStages.at(s)] = shaderSources.at(s);
}
void glBindAttribLocation(GLuint, GLuint, const GLchar*) {}
void glLinkProgram(GLuint id)
{
    // Sources are immutable after link. Classify once so reflection fault
    // tests do not repeatedly scan the full Cg body for every uniform query.
    fogOffByProgram[id] = fog_off_source(programSources.at(id).at(GL_CG_FRAGMENT_SHADER_EXT));
}
void glGetProgramiv(GLuint id, GLenum p, GLint* v)
{
    const bool usesBones = program_uses_bones(id);
    *v = p == GL_ACTIVE_UNIFORMS ? reflected_uniforms(id).size()
                                 : !((failRigidLink && !usesBones) || (failFogOffLink && program_fog_off(id)));
}
void glUseProgram(GLuint id)
{
    assert(!id || programs.count(id));
    program = id;
}
GLint glGetUniformLocation(GLuint id, const GLchar* name)
{
    if (std::string(name) == "u_background") return -200;
    if (std::string(name) == "u_operations") return -201;
    if (std::string(name) == "u_region") return omitLoadingUniform ? -1 : -202;
    if (std::string(name) == "u_bones" || std::string(name) == "u_bones[0]")
        return omitBoneUniform || (!program_uses_bones(id) && !forceRigidBoneUniform) ? -1 : -4;
    const std::string requested = name;
    if (requested == "u_fogrange" || requested == "u_fogrange[0]")
    {
        if (!uniform_present(id, 20) || fogReflectionFault == FogReflectionFault::MissingTable ||
            (fogReflectionFault == FogReflectionFault::ArrayName && requested == "u_fogrange"))
            return -1;
        return -22;
    }
    if (requested == "u_fograngecontrol" || requested == "u_fograngecontrol[0]")
        return !uniform_present(id, 21) || fogReflectionFault == FogReflectionFault::MissingControl ? -1 : -23;
    for (unsigned i = 0; i < std::size(names); ++i)
        if (std::string(name) == names[i])
            return uniform_present(id, i) ? -int(i + 2) : -1; // Opaque signed handles.
    if (std::string(name).starts_with("u_tex"))
        return -100 - (name[5] - '0');
    return -1;
}
void glGetActiveUniform(GLuint id, GLuint i, GLsizei n, GLsizei*, GLint* size, GLenum* type,
                        GLchar* name)
{
    i = reflected_uniforms(id).at(i);
    assert(i < std::size(names));
    *size = capacities[i];
    *type = GL_FLOAT_VEC4;
    const char* reflected = names[i];
    if (i == 20)
    {
        if (fogReflectionFault == FogReflectionFault::ArrayName) reflected = "u_fogrange[0]";
        if (fogReflectionFault == FogReflectionFault::TableType) *type = GL_INT;
        if (fogReflectionFault == FogReflectionFault::TableZero) *size = 0;
        if (fogReflectionFault == FogReflectionFault::TableOne) *size = 1;
        if (fogReflectionFault == FogReflectionFault::TableTwo) *size = 2;
        if (fogReflectionFault == FogReflectionFault::MissingReflection) reflected = "fixture_missing_reflection";
        if (fogReflectionFault == FogReflectionFault::OversizedTable) *size = 4;
    }
    if (i == 21)
    {
        if (fogReflectionFault == FogReflectionFault::ControlZero) *size = 0;
        if (fogReflectionFault == FogReflectionFault::OversizedControl) *size = 2;
        if (fogReflectionFault == FogReflectionFault::ControlType) *type = GL_FLOAT;
    }
    std::strncpy(name, reflected, n);
}
void glUniform1i(GLint location, GLint unit) {
    assert(location == -100-unit || ((unit == 0 || unit == 1) && location == -200-unit));
}
void glUniform4fv(GLint location, GLsizei count, const GLfloat* values)
{
    if (location == -202) {
        assert(count == 1);
        std::copy_n(values, 4, loadingRegion.begin());
        ++uniformWrites;
        return;
    }
    assert(location <= -2 && location >= -int(std::size(names)+1) && count > 0 &&
           unsigned(count) <= capacities[-location - 2]);
    assert(uniform_present(program, unsigned(-location - 2)));
    ++uniformWrites;
    if (location == -22)
    {
        ++fogTableWrites;
        fogTableByProgram[program].assign(values, values + count * 4);
    }
    if (location == -23)
    {
        ++fogControlWrites;
        fogControlByProgram[program].assign(values, values + count * 4);
    }
    if (location == -4)
    {
        assert(program_uses_bones(program));
        ++boneUniformWrites;
        bones.assign(values, values + count * 4);
    }
    if (location == -11)
        specularDirections.assign(values, values + count * 4);
    if (location == -12)
        specularColors.assign(values, values + count * 4);
    if (location == -20)
        material.assign(values, values + count * 4);
    if (location == -21)
        alphaState.assign(values, values + count * 4);
    if (location == -13)
        controls.assign(values, values + count * 4);
    if (location == -8)
        lightingByProgram[program].assign(values, values + count * 4);
    if (location == -15)
        tintByProgram[program].assign(values, values + count * 4);
}
void glEnableVertexAttribArray(GLuint) { ++attributeEnables; }
void glVertexAttribPointer(GLuint slot, GLint, GLenum, GLboolean, GLsizei, const void*)
{
    assert(buffers.count(array));
    attributeBuffers.at(slot) = array;
    ++attributePointers;
}
void glDepthFunc(GLenum) {}
void glFrontFace(GLenum face) { frontFace = face; }
void glCullFace(GLenum face) { assert(face == GL_BACK); cullFace = face; }
void glBlendEquation(GLenum) {}
void glBlendFunc(GLenum src, GLenum dst)
{
    blendSource = src;
    blendDestination = dst;
}
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) { viewport = { x, y, w, h }; }
void glScissor(GLint x, GLint y, GLsizei w, GLsizei h)
{
    ++scissors;
    scissor = { x, y, w, h };
}
void glDrawElements(GLenum primitive, GLsizei count, GLenum type, const void* offset)
{
    assert(programs.count(program) && (!drawFb || framebuffers.count(drawFb)) &&
           (primitive == GL_TRIANGLES || primitive == GL_LINES) &&
           count % (primitive == GL_LINES ? 2 : 3) == 0 && type == GL_UNSIGNED_SHORT && !offset);
    auto& v = buffers.at(array);
    auto& i = buffers.at(elements);
    for (auto buffer : attributeBuffers)
        assert(buffer == array); // Detect stale VBO captures across all production draws.
    assert(size_t(count) * 2 <= i.size());
    for (int n = 0; n < count; ++n)
    {
        uint16_t index;
        std::memcpy(&index, i.data() + n * 2, 2);
        assert(index < v.size() / sizeof(port::render::ModelVertex));
    }
    submitted.push_back(v);
    submittedTargets.push_back(drawFb);
    submittedPrimitives.push_back(primitive);
    submittedViewports.push_back(viewport);
    ++surfaceWrites[drawFb];
    if (!drawFb)
    {
        if (programSources.at(program).at(GL_CG_FRAGMENT_SHADER_EXT).find("u_operations") != std::string::npos)
        {
            assert(textures.count(textureUnits[0]) && textures.count(textureUnits[1]));
            loadingDraws.push_back({textureUnits[0], textureUnits[1], loadingRegion});
            backContent = loadingRegion[2] > 0 ? DisplayContent::Loading : textureContents.at(textureUnits[0]);
        }
        else
        {
            backContent = DisplayContent::Bitmap;
            pendingBitmapTexture = textureUnits[0];
            assert(textures.count(pendingBitmapTexture));
        }
    }
    else
        textureContents[framebufferColors.at(drawFb)] = DisplayContent::Scene;
    if (failDraw || (failDrawCountdown >= 0 && failDrawCountdown-- == 0))
        error = GL_INVALID_OPERATION;
}
void glBlitFramebuffer(GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint x0, GLint y0, GLint x1, GLint y1,
                       GLbitfield, GLenum)
{
    assert(!readFb || framebuffers.count(readFb));
    if (!readFb) {
        assert(drawFb && readMode == GL_FRONT && completedSwaps == swaps && !displayContents.empty());
        textureContents[framebufferColors.at(drawFb)] = displayContents.back();
    }
    else if (drawFb)
        textureContents[framebufferColors.at(drawFb)] = textureContents.at(framebufferColors.at(readFb));
    lastBlitSource = { sx0, sy0, sx1, sy1 };
    lastBlitDestination = { x0, y0, x1, y1 };
    lastBlitReadFb = readFb;
    lastBlitDrawFb = drawFb;
    ++blits;
    ++surfaceWrites[drawFb];
    if (!drawFb)
        backContent = DisplayContent::Scene;
    if (failBlit)
        error = GL_INVALID_OPERATION;
}
void vglSetPresentProfile(PortVglPresentProfile* profile, PortVglTimelineClock clock)
{
    ++presentProfileSets;
    assert(bool(profile) == bool(clock));
    if (profile) assert(!activePresentProfile);
    activePresentProfile = profile;
    activePresentClock = clock;
}
void vglSwapBuffers(GLboolean)
{
    if (activePresentProfile)
    {
        auto& p = *activePresentProfile;
        auto begin = activePresentClock();
        p.addEntryNs += activePresentClock() - begin;
        ++p.addEntryCalls;
        begin = activePresentClock();
        p.gcNs += activePresentClock() - begin;
        ++p.gcCalls;
    }
    assert(drawFb == 0);
    ++swaps;
    displayContents.push_back(backContent);
    backContent = DisplayContent::None;
    if (failSwap)
        error = GL_INVALID_OPERATION;
}
int sceGxmDisplayQueueFinish()
{
    if (pendingBitmapTexture)
        assert(textures.count(pendingBitmapTexture));
    ++completionWaits;
    if (displayCompletionResult >= 0) completedSwaps = swaps;
    return displayCompletionResult;
}
void glReadBuffer(GLenum mode) { readMode = mode; ++readBufferChanges; }
void glReadPixels(GLint, GLint, GLsizei w, GLsizei h, GLenum, GLenum, GLvoid* dst)
{
    assert(readFb == 0 && readMode == GL_FRONT && completedSwaps == swaps);
    ++readbacks;
    auto* p = static_cast<uint8_t*>(dst);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            auto i = (y * w + x) * 4;
            p[i] = y % 256;
            p[i + 1] = x % 256;
            p[i + 2] = 0;
            p[i + 3] = 255;
        }
}
}
#include "frame_timeline_renderer_cases.inc"
#include "fog_range_renderer_cases.inc"
#include "scene_program_selection_cases.inc"
#include "fog_off_renderer_cases.inc"
#include "fog_curve4_renderer_cases.inc"
int main()
{
    unsetenv("STRIKERS_VITAGL_UNIFORM_CACHE");
    unsetenv("STRIKERS_VITAGL_PACKET_PROFILE");
    unsetenv("STRIKERS_VITAGL_UNIFORM_VERIFY");
    unsetenv("STRIKERS_VITAGL_FOG_OFF_SPECIALIZE");
    unsetenv("STRIKERS_VITAGL_FOG_CURVE4_SPECIALIZE");
    check_loading_retrace_clock();
    using namespace port::render;
    unsetenv("STRIKERS_VITAGL_AFFINE_UV");
    unsetenv("STRIKERS_VITAGL_VERTEX_FOG");
    // Non-unit world light directions must produce the same incoming unit
    // direction as GX; zero/invalid lights must not introduce shader NaNs.
    const float lightColor[]{ .2f, .4f, 1.f, 1.f };
    const auto light = scene_specular_light({ 0, 30, -40, 0 }, lightColor, .8f, 64);
    assert(std::fabs(light.direction[1] - .6f) < .00001f);
    assert(std::fabs(light.direction[2] + .8f) < .00001f);
    assert(light.direction[3] == 64 && std::fabs(light.color[0] - .2f) < .00001f);
    assert((scene_specular_light({}, lightColor, 1, 64).color == std::array<float, 4>{}));
    assert((scene_specular_light({ 0, 0, -1, 0 }, lightColor, 2, 64).color[2] == 1));
    constexpr std::array<float, 16> identity{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    // AvPlayer planes stay single-channel and are copied by GL on its owner.
    // A one-byte allocation also catches accidental RGBA alpha scanning under ASan.
    {
        Renderer r;
        assert(r.initialize());
        TextureUpload plane;
        plane.width = plane.stride = 64;
        plane.height = 32;
        plane.mipCount = 1;
        plane.storage = port::nativegxm::TextureStorage::Intensity8;
        plane.opaqueLuminance = true;
        plane.pixels.assign(64 * 32, 173);
        const auto id = r.create_texture(plane);
        assert(id);
        assert(textureAllocations.at(id).format == GL_LUMINANCE);
        assert(textureAllocations.at(id).internalFormat == GL_LUMINANCE);
        r.release_texture(id);
        plane.storage = port::nativegxm::TextureStorage::Rgba8;
        assert(!r.create_texture(plane));
        r.shutdown();
        empty();
    }
    // Allocation failure must stop before a later mip, attachment, attribute
    // setup or draw can use the provider's missing GPU storage.
    for (int failedLevel : {0, 1})
    {
        Renderer r;
        assert(r.initialize());
        TextureUpload upload;
        upload.width = upload.stride = 16;
        upload.height = 8;
        upload.mipCount = 2;
        upload.storage = port::nativegxm::TextureStorage::Rgba8;
        upload.pixels.resize(16 * 8 * 4 + 8 * 4 * 4, 255);
        const auto before = textureUploads;
        const auto live = textures.size();
        failTextureLevel = failedLevel;
        assert(!r.create_texture(upload));
        failTextureLevel = -1;
        assert(textureUploads == before + unsigned(failedLevel + 1) && textures.size() == live);
        assert(r.error_code() == GL_OUT_OF_MEMORY && std::string(r.last_error()) == "texture_upload");
        r.shutdown();
        empty();
    }
    for (bool depthFailure : {false, true})
    {
        Renderer r;
        assert(r.initialize() && r.begin_frame());
        const auto originalTarget = drawFb;
        const auto beforeClears = clears, beforeAttachments = depthAttachments, beforeBinds = framebufferBinds;
        const auto liveTextures = textures.size(), liveTargets = framebuffers.size();
        failTexture = !depthFailure;
        failDepthStorage = depthFailure;
        assert(!r.create_target(80, 80));
        failTexture = failDepthStorage = false;
        assert(r.error_code() == GL_OUT_OF_MEMORY && clears == beforeClears && depthAttachments == beforeAttachments);
        assert(drawFb == originalTarget && r.frame_active());
        assert(textures.size() == liveTextures && framebuffers.size() == liveTargets);
        if (!depthFailure) assert(framebufferBinds == beforeBinds);
        assert(r.end_frame(false));
        r.shutdown();
        empty();
    }
    for (GLenum failedBuffer : {GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER})
    {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program() && r.begin_frame());
        std::array<ModelVertex, 3> vertices;
        const std::array<uint16_t, 3> indices{0, 1, 2};
        const auto beforeUploads = bufferUploads, beforeAttributes = attributePointers;
        const auto beforeDraws = submitted.size();
        failBufferTarget = failedBuffer;
        assert(!r.draw_scene(0, vertices, indices, identity, Raster{}, SceneMaterial{}));
        failBufferTarget = 0;
        assert(r.error_code() == GL_OUT_OF_MEMORY && submitted.size() == beforeDraws &&
               attributePointers == beforeAttributes);
        assert(bufferUploads == beforeUploads + (failedBuffer == GL_ARRAY_BUFFER ? 1u : 2u));
        assert(r.end_frame(false) && r.completed_timing().dynamicVertexBytes == 0);
        r.shutdown();
        empty();
    }
    {
        Renderer r;
        assert(r.initialize(true) && r.initialize_scene_program());
        std::array<ModelVertex, 3> vertices;
        vertices[1].position[0] = 1;
        vertices[2].position[1] = 1;
        std::array<uint16_t, 3> indices{ 0, 1, 2 };
        auto model = r.create_model(vertices, indices);
        assert(model);
        TextureUpload emptyTexture;
        assert(!r.create_texture(emptyTexture));
        assert(r.begin_frame());
        SceneMaterial m;
        Raster raster;
        assert(r.draw_scene(model, {}, {}, identity, raster, m));
        assert(!enabled.count(GL_SCISSOR_TEST));
        assert(!colorMask[3]);
        auto writes = uniformWrites;
        const auto enables = attributeEnables, pointers = attributePointers;
        assert(r.draw_scene(model, {}, {}, identity, raster, m) && uniformWrites == writes);
        assert(attributeEnables == enables && attributePointers == pointers);
        // All four specular lights and the original gloss weight reach the
        // actual driver; changing the final light must invalidate its cache.
        m.specular[3] = light.direction;
        m.specularColor[3] = light.color;
        m.material[0] = 13.f / 63.f;
        m.alpha = { .25f, .5f, .75f, 6.f };
        assert(r.draw_scene(model, {}, {}, identity, raster, m));
        assert(specularDirections.size() == 16 && specularColors.size() == 16);
        assert(specularDirections[15] == 64 && specularColors[12] == light.color[0]);
        assert(material.size() == 4 && material[0] == 13.f / 63.f);
        assert((alphaState == std::vector<float>{ .25f, .5f, .75f, 6.f }));
        writes = uniformWrites;
        assert(r.draw_scene(model, {}, {}, identity, raster, m) && uniformWrites == writes);
        m.specularColor[3][0] = .9f;
        assert(r.draw_scene(model, {}, {}, identity, raster, m) && uniformWrites == writes + 1);
        assert(specularColors[12] == .9f);
        m.alpha = { 1, 1, 1, 0 };
        m.scissor = { 5, 7, 105, 107 };
        assert(r.draw_scene(0, vertices, indices, identity, raster, m));
        assert((scissor == std::array<int, 4>{ 5, 437, 100, 100 }));
        auto previousScissors = scissors;
        assert(r.draw_scene(0, vertices, indices, identity, raster, m) &&
               scissors == previousScissors);
        // The submitted snapshot survives mutation of the caller's CPU arrays.
        auto before = submitted.back();
        vertices[0].position[0] = 42;
        assert(submitted.back() == before);
        assert(r.draw_scene(0, vertices, indices, identity, raster, m) &&
               submitted.back() != before);
        auto destination = r.create_target(80, 80);
        assert(destination);
        assert(r.copy_texture(destination, { 0, 0, 160, 148 }, true));
        assert(colorMask[3]);
        // Ten silhouettes occupy three independent framebuffer atlas rows.
        // The original copy format is A8: black RGB has nonzero coverage.
        // Row zero must sample the framebuffer top; copied engine V=0 lives
        // at GL texture V=0, and six unused destination rows remain clear.
        for (unsigned row = 0; row < 3; ++row)
        {
            const float y = row * 148.f * 544.f / 448.f;
            const float h = 148.f * 544.f / 448.f;
            TextureCopy copy{destination, {0, y, 240, h}, 74, false, true};
            assert(r.copy_textures({&copy, 1}));
            assert((viewport == std::array<int,4>{0, 0, 80, 74}));
            assert(controls.size() == 4 && controls[3] == 4);
            ModelVertex quad[4];
            assert(submitted.back().size() >= sizeof(quad));
            std::memcpy(quad, submitted.back().data(), sizeof(quad));
            assert(std::fabs(quad[3].uv[1] - (1.f - y / 544.f)) < .00001f);
            assert(std::fabs(quad[0].uv[1] - (1.f - (y + h) / 544.f)) < .00001f);
            assert(quad[0].uv[1] < quad[3].uv[1]);
        }
        // Updated partitions share one framebuffer; the others retain their
        // cached silhouettes. Ten resolves require only one target change
        // and one restoration, and must never clear the shared atlas.
        auto atlas = r.create_target(320,240);
        assert(atlas);
        std::array<TextureCopy,10> copies;
        for(unsigned i=0;i<copies.size();++i) {
            copies[i]={atlas,{float(i%4)*240,float(i/4)*148,240,148},74,false,true};
            copies[i].destinationRect={i%4*80,i/4*80,80,74};
            copies[i].preserveDestination=true;
        }
        auto priorBinds=framebufferBinds, priorClears=clears;
        assert(r.copy_textures(copies));
        assert(framebufferBinds==priorBinds+2 && clears==priorClears);
        assert((viewport==std::array<int,4>{80,160,80,74}));
        r.release_texture(atlas);
        assert(r.draw_scene(model, {}, {}, identity, raster, m));
        assert(!colorMask[3]);
        assert((scissor == std::array<int, 4>{ 5, 437, 100, 100 }));
        raster.blend = 4;
        assert(r.draw_scene(model, {}, {}, identity, raster, m));
        assert(blendSource == GL_DST_COLOR && blendDestination == GL_ZERO);
        raster.blend = 5;
        assert(r.draw_scene(model, {}, {}, identity, raster, m));
        assert(blendSource == GL_ONE_MINUS_DST_COLOR && blendDestination == GL_ONE);
        raster.cull = 1;
        assert(r.draw_scene(model, {}, {}, identity, raster, m));
        assert(frontFace == GL_CCW && cullFace == GL_BACK && enabled.count(GL_CULL_FACE));
        raster.cull = 2;
        assert(r.draw_scene(model, {}, {}, identity, raster, m));
        assert(frontFace == GL_CW && cullFace == GL_BACK);
        auto drawsBeforeCullAll = submitted.size();
        raster.cull = 3;
        assert(r.draw_scene(model, {}, {}, identity, raster, m));
        assert(submitted.size() == drawsBeforeCullAll);
        raster.cull = 0;
        // The actual GL sampler must clamp both ends of the lighting lookup,
        // even though the game's default BumpLocal wrap state is WrapWrap.
        TextureUpload ramp;
        ramp.width = ramp.height = ramp.mipCount = 1;
        ramp.stride = 8;
        ramp.storage = TextureStorage::Rgba8;
        ramp.pixels.assign(32, 255);
        const auto rampTexture = r.create_texture(ramp);
        assert(rampTexture);
        m.controls[0] = 33;
        m.textures[5] = rampTexture;
        for (unsigned wrap = 0; wrap < 4; ++wrap)
        {
            const auto sampling = scene_texture_sampling(5, wrap, true);
            m.wrapU[5] = sampling.wrapU;
            m.wrapV[5] = sampling.wrapV;
            m.filter[5] = sampling.linear;
            assert(r.draw_scene(model, {}, {}, identity, raster, m));
            assert(textureParameters[rampTexture][GL_TEXTURE_WRAP_S] == GL_CLAMP_TO_EDGE);
            assert(textureParameters[rampTexture][GL_TEXTURE_WRAP_T] == GL_CLAMP_TO_EDGE);
        }
        // Ordinary material textures retain their independent U/V wrap bits.
        m.controls[0] = 1;
        m.textures[0] = rampTexture;
        for (unsigned wrap = 0; wrap < 4; ++wrap)
        {
            const auto sampling = scene_texture_sampling(0, wrap, false);
            m.wrapU[0] = sampling.wrapU;
            m.wrapV[0] = sampling.wrapV;
            m.filter[0] = sampling.linear;
            assert(r.draw_scene(model, {}, {}, identity, raster, m));
            assert(textureParameters[rampTexture][GL_TEXTURE_WRAP_S] ==
                   (wrap >= 2 ? GL_CLAMP_TO_EDGE : GL_REPEAT));
            assert(textureParameters[rampTexture][GL_TEXTURE_WRAP_T] ==
                   ((wrap & 1) ? GL_CLAMP_TO_EDGE : GL_REPEAT));
        }
        // Ball motion blur supplies 10-32% alpha in ConstantColour, not in
        // GLUD_Translucent. Verify the uniform actually sent to the GPU and
        // the priority of the explicit-alpha and lit-material paths.
        m.effects[0]=1;m.tint[3]=81.f/255.f;
        assert(r.draw_scene(model, {}, {}, identity, raster, m));
        assert(lightingByProgram.at(program).size()==4 && lightingByProgram.at(program)[1]==81.f/255.f);
        m.effects[1]=.75f;
        assert(r.draw_scene(model, {}, {}, identity, raster, m) && lightingByProgram.at(program)[1]==.75f);
        m.effects[1]=-1;m.controls[0]=33;
        assert(r.draw_scene(model, {}, {}, identity, raster, m) && lightingByProgram.at(program)[1]==-1.f);
        m.effects[0]=0;m.controls[0]=1;
        assert(r.draw_scene(model, {}, {}, identity, raster, m) && lightingByProgram.at(program)[1]==-1.f);
        // A changed last row must survive reflection, the full upload and the
        // per-program cache. Earlier palettes silently stopped at slot nine.
        auto pnVertices = vertices;
        pnVertices[0].boneSlot = float(kSceneSkinBoneCount);
        const auto pnModel = r.create_model(pnVertices, indices);
        assert(pnModel);
        m.bones.back()[3] = 1234;
        assert(r.draw_scene(pnModel, {}, {}, identity, raster, m) && bones.size() == kSceneBoneFloats &&
               bones.back() == 1234);
        const auto writesAfterPalette = uniformWrites;
        assert(r.draw_scene(pnModel, {}, {}, identity, raster, m));
        assert(uniformWrites == writesAfterPalette);
        m.bones.back()[3] = -5678;
        assert(r.draw_scene(pnModel, {}, {}, identity, raster, m));
        assert(uniformWrites == writesAfterPalette + 1 && bones.back() == -5678);
        r.release_model(pnModel);
        assert(r.end_frame() && !r.frame_active() && swaps == 1 && blits >= 1);
        std::vector<uint8_t> pixels;
        const auto beforeReadback = completionWaits;
        assert(r.readback_presented_rgba8(pixels) && pixels.size() == 960 * 544 * 4);
        assert(completionWaits == beforeReadback + 1 && readMode == GL_BACK && drawFb == 0 && readFb == 0);
        assert(pixels[0] == 543 % 256 && pixels[(543 * 960) * 4] == 0);
        // Resource-invalid passes must never blit or swap the cleared FBO.
        // Consecutive aborts keep the last presented frame readable, and a
        // subsequent valid frame resumes presentation without stale state.
        const auto priorBlits = blits;
        for (unsigned aborted = 0; aborted < 100; ++aborted)
        {
            assert(r.begin_frame() && r.end_frame(false) && !r.frame_active());
            assert(swaps == 1 && blits == priorBlits);
        }
        assert(r.readback_presented_rgba8(pixels));
        assert(r.begin_frame() && r.end_frame() && swaps == 2 && blits > priorBlits);
        r.release_model(model);
        r.release_texture(destination);
        r.release_texture(rampTexture);
        r.shutdown();
        empty();
    }
    // Failed display completion must precede every binding change and output
    // allocation. Rejecting an idle/unpresented or open frame does not wait.
    {
        Renderer r;
        assert(r.initialize());
        std::vector<uint8_t> pixels{19, 23, 42};
        const auto original = pixels;
        const auto initialWaits = completionWaits, initialReads = readbacks;
        assert(!r.readback_presented_rgba8(pixels) && pixels == original);
        assert(r.begin_frame());
        assert(!r.readback_presented_rgba8(pixels) && pixels == original);
        assert(completionWaits == initialWaits && readbacks == initialReads);
        assert(r.end_frame() && completedSwaps < swaps);
        // Preserve even independent read/draw bindings on an aborted capture.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, *framebuffers.begin());
        const auto boundRead = readFb, boundDraw = drawFb, mode = readMode;
        const auto binds = framebufferBinds, readChanges = readBufferChanges;
        const auto priorClears = clears, priorSwaps = swaps;
        const auto oldViewport = viewport, oldScissor = scissor;
        displayCompletionResult = -791;
        assert(!r.readback_presented_rgba8(pixels));
        assert(r.error_code() == -791 && std::strcmp(r.last_error(), "readback_present_completion") == 0);
        assert(pixels == original && readbacks == initialReads && completionWaits == initialWaits + 1);
        assert(readFb == boundRead && drawFb == boundDraw && readMode == mode &&
               framebufferBinds == binds && readBufferChanges == readChanges &&
               clears == priorClears && swaps == priorSwaps && viewport == oldViewport && scissor == oldScissor);
        displayCompletionResult = 0;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        assert(r.readback_presented_rgba8(pixels));
        assert(completionWaits == initialWaits + 2 && readbacks == initialReads + 1 &&
               completedSwaps == swaps && readMode == GL_BACK && drawFb == 0 && readFb == 0);
        assert(pixels.size() == 960 * 544 * 4 && pixels[0] == 543 % 256 &&
               pixels[(543 * 960) * 4] == 0 && r.error_code() == -791);
        r.shutdown();
        empty();
    }
    // GX RGB565 captures require an opaque, 16-bit color target. Default scene
    // and coverage targets retain RGBA8 and conservative sampled-alpha bounds.
    {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program());
        const auto source = r.create_target(128, 128);
        const auto rgba = r.create_target(80, 74, TargetFormat::Rgba8);
        const auto rgb565 = r.create_target(320, 224, TargetFormat::Rgb565);
        assert(source && rgba && rgb565);
        assert((textureAllocations.at(source) ==
                TextureAllocation{GL_RGBA, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, false}));
        assert((textureAllocations.at(rgba) ==
                TextureAllocation{GL_RGBA, 80, 74, GL_RGBA, GL_UNSIGNED_BYTE, false}));
        assert((textureAllocations.at(rgb565) ==
                TextureAllocation{GL_RGB, 320, 224, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, false}));
        assert(r.begin_frame() && r.select_target(source));
        const auto sourceFb = drawFb;
        const auto textureCount = textures.size(), framebufferCount = framebuffers.size(),
                   renderbufferCount = renderbuffers.size();
        const auto bindsBefore = framebufferBinds;
        assert(!r.create_target(80, 74, static_cast<TargetFormat>(2)));
        assert(r.error_code() == GL_INVALID_ENUM &&
               std::strcmp(r.last_error(), "target_format") == 0);
        assert(framebufferBinds == bindsBefore && textures.size() == textureCount &&
               framebuffers.size() == framebufferCount && renderbuffers.size() == renderbufferCount);
        assert(drawFb == sourceFb && readFb == sourceFb);
        failTarget = true;
        assert(!r.create_target(64, 64, TargetFormat::Rgb565));
        failTarget = false;
        assert(textures.size() == textureCount && framebuffers.size() == framebufferCount &&
               renderbuffers.size() == renderbufferCount && drawFb == sourceFb && readFb == sourceFb);
        const auto blitsBefore = blits;
        const auto drawsBefore = submitted.size();
        assert(r.copy_texture(rgb565, {0, 0, 128, 128}));
        assert(r.copy_texture(rgba, {0, 0, 128, 128}));
        assert(blits == blitsBefore && submitted.size() == drawsBefore + 2);
        assert(drawFb == sourceFb && readFb == sourceFb);
        assert(r.select_target());
        std::array<ModelVertex, 3> vertices;
        const std::array<uint16_t, 3> indices{0, 1, 2};
        SceneMaterial m;
        m.controls[0] = 1;
        m.controls[1] = .5f;
        m.textures[0] = rgb565;
        assert(r.draw_scene(0, vertices, indices, identity, Raster{}, m));
        const auto opaqueProgram = program;
        m.textures[0] = rgba;
        assert(r.draw_scene(0, vertices, indices, identity, Raster{}, m));
        const auto testedProgram = program;
        assert(testedProgram != opaqueProgram);
        m.controls[1] = -1;
        assert(r.draw_scene(0, vertices, indices, identity, Raster{}, m));
        assert(program == opaqueProgram);
        m.controls[1] = .5f;
        m.textures[0] = rgb565;
        vertices[0].color[3] = 64;
        assert(r.draw_scene(0, vertices, indices, identity, Raster{}, m));
        assert(program == testedProgram);
        // Disabling alpha test preserves the vertex alpha override and normal
        // texture validation, even after a preceding draw needed the test.
        m.controls[1] = -1;
        m.effects[0] = 1;
        m.tint[3] = .25f;
        assert(r.draw_scene(0, vertices, indices, identity, Raster{}, m));
        assert(program == opaqueProgram && lightingByProgram.at(program)[1] == .25f);
        m.effects[1] = .75f;
        assert(r.draw_scene(0, vertices, indices, identity, Raster{}, m));
        assert(program == opaqueProgram && lightingByProgram.at(program)[1] == .75f);
        m.controls[1] = .5f;
        m.effects[0] = 0;
        m.effects[1] = -1;
        assert(r.draw_scene(0, vertices, indices, identity, Raster{}, m));
        assert(program == testedProgram);
        assert(r.end_frame(false));
        r.shutdown();
        empty();
    }
    // Use a clean renderer: error diagnostics retain the first failure, while
    // the preceding target-format regression deliberately already latched one.
    {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program() && r.begin_frame());
        std::array<ModelVertex, 3> vertices;
        const std::array<uint16_t, 3> indices{0, 1, 2};
        SceneMaterial m;
        m.controls[0] = 1;
        m.controls[1] = -1;
        m.textures[0] = std::numeric_limits<uint32_t>::max();
        const auto beforeMissingTexture = submitted.size();
        assert(!r.draw_scene(0, vertices, indices, identity, Raster{}, m));
        assert(submitted.size() == beforeMissingTexture &&
               std::strcmp(r.last_error(), "draw_texture") == 0);
        assert(r.end_frame(false));
        r.shutdown();
        empty();
    }
    // Converted and raw color copies may alternate. Every draw must bind
    // its actual destination, and the caller's non-default source is restored.
    {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program());
        const auto source = r.create_target(128, 128);
        const auto converted = r.create_target(80, 80);
        const auto plain = r.create_target(96, 96);
        assert(source && converted && plain && r.begin_frame());
        assert(r.select_target(converted));
        const auto convertedFb = drawFb;
        assert(r.select_target(source));
        const auto sourceFb = drawFb;
        TextureCopy alpha{ converted, { 0, 0, 128, 128 }, 74, false, true };
        TextureCopy color{ plain, { 0, 0, 128, 128 } };
        color.destinationRect = { 5, 7, 24, 31 };
        color.preserveDestination = true;
        TextureCopy intensity = alpha;
        intensity.alphaOnly = false;
        intensity.intensity = true;
        const std::array<TextureCopy, 3> mixed{ alpha, color, intensity };
        const auto priorDraws = submittedTargets.size();
        assert(r.copy_textures(mixed));
        assert(submittedTargets.size() == priorDraws + 3);
        assert(submittedTargets[priorDraws] == convertedFb &&
               submittedTargets[priorDraws + 1] != convertedFb &&
               submittedTargets[priorDraws + 1] != sourceFb &&
               submittedTargets[priorDraws + 2] == convertedFb);
        assert((submittedViewports[priorDraws + 1] == std::array<int, 4>{ 5, 7, 24, 31 }));
        assert(drawFb == sourceFb && readFb == sourceFb);

        // ScreenGrab copies clear only the captured source rectangle, including
        // alpha and depth. A later converted copy must rebind its destination.
        for (bool convertedGrab : { false, true })
        {
            TextureCopy grab = convertedGrab ? alpha : color;
            grab.sourceRect = { 3.25f, 7.25f, 66.5f, 80.5f };
            grab.clearSource = true;
            grab.clearColor = { .125f, .25f, .5f, .75f };
            const std::array<TextureCopy, 2> captureThenConvert{ grab, intensity };
            const auto clearsBefore = clearCalls.size();
            assert(r.copy_textures(captureThenConvert));
            unsigned sourceClears = 0;
            for (size_t i = clearsBefore; i < clearCalls.size(); ++i)
            {
                const auto& call = clearCalls[i];
                if (call.target != sourceFb)
                    continue;
                ++sourceClears;
                assert(call.mask == (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
                assert(call.scissorEnabled &&
                       (call.scissor == std::array<int, 4>{ 3, 40, 67, 81 }));
                assert((call.colorMask == std::array<bool, 4>{ true, true, true, true }));
                assert(call.depthMask && call.depth == 1 && call.color == grab.clearColor);
            }
            assert(sourceClears == 1 && submittedTargets.back() == convertedFb);
            assert(drawFb == sourceFb && readFb == sourceFb);
        }

        // Reject a malformed second request before the valid first request
        // clears or writes anything, including the plain-color draw path.
        const auto reject = [&](const TextureCopy& invalid) {
            const std::array<TextureCopy, 2> batch{ alpha, invalid };
            const auto bindsBefore = framebufferBinds, clearsBefore = clears,
                       blitsBefore = blits;
            const auto drawsBefore = submittedTargets.size();
            assert(!r.copy_textures(batch));
            assert(framebufferBinds == bindsBefore && clears == clearsBefore &&
                   blits == blitsBefore && submittedTargets.size() == drawsBefore);
            assert(drawFb == sourceFb && readFb == sourceFb);
        };
        TextureCopy invalid = alpha;
        invalid.destination = 0;
        reject(invalid);
        invalid.destination = source;
        reject(invalid);
        invalid = color;
        invalid.sourceRect[0] = -1;
        reject(invalid);
        invalid = color;
        invalid.sourceRect[2] = 0;
        reject(invalid);
        invalid = color;
        invalid.sourceRect[0] = 1;
        reject(invalid);
        invalid = color;
        invalid.sourceRect[1] = std::numeric_limits<float>::quiet_NaN();
        reject(invalid);
        invalid = color;
        invalid.sourceRect[3] = std::numeric_limits<float>::infinity();
        reject(invalid);
        invalid = color;
        invalid.sourceRect[2] = std::numeric_limits<float>::max();
        reject(invalid);
        invalid = alpha;
        invalid.destinationHeight = 81;
        reject(invalid);
        invalid = alpha;
        invalid.destinationRect = { 79, 0, 2, 74 };
        reject(invalid);
        invalid.destinationRect = { 1, 0, 0, 0 };
        reject(invalid);
        invalid = color;
        invalid.clearSource = true;
        invalid.clearColor[2] = std::numeric_limits<float>::quiet_NaN();
        reject(invalid);
        assert(r.end_frame(false));
        r.shutdown();
        empty();
    }
    // A driver failure stops the batch and restores both framebuffer bindings
    // and cached raster/viewport state for all copy modes.
    for (bool drawFailure : { true, false })
    {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program());
        const auto source = r.create_target(128, 128);
        const auto converted = r.create_target(80, 80);
        const auto plain = r.create_target(96, 96);
        assert(source && converted && plain && r.begin_frame() && r.select_target(source));
        const auto sourceFb = drawFb;
        TextureCopy alpha{ converted, { 0, 0, 128, 128 }, 74, false, true };
        TextureCopy color{ plain, { 0, 0, 128, 128 } };
        alpha.clearSource = color.clearSource = true;
        const std::array<TextureCopy, 3> mixed{ alpha, color, alpha };
        const auto priorDraws = submittedTargets.size();
        const auto priorBlits = blits;
        const auto priorClears = clearCalls.size();
        failDrawCountdown = drawFailure ? 0 : 1;
        assert(!r.copy_textures(mixed));
        failDrawCountdown = -1;
        assert(submittedTargets.size() == priorDraws + unsigned(drawFailure ? 1 : 2));
        assert(blits == priorBlits);
        assert(drawFb == sourceFb && readFb == sourceFb && r.error_code() == GL_INVALID_OPERATION);
        unsigned sourceClears = 0;
        for (size_t i = priorClears; i < clearCalls.size(); ++i)
            sourceClears += clearCalls[i].target == sourceFb;
        // A failed copy must never erase the still-needed source pixels.
        assert(sourceClears == unsigned(!drawFailure));
        std::array<ModelVertex, 3> vertices;
        const std::array<uint16_t, 3> indices{ 0, 1, 2 };
        SceneMaterial material;
        material.viewport = material.scissor = { 0, 0, 128, 128 };
        assert(r.draw_scene(0, vertices, indices, identity, Raster{}, material));
        assert(submittedTargets.back() == sourceFb);
        assert((viewport == std::array<int, 4>{ 0, 0, 128, 128 }));
        assert(!colorMask[3] && !enabled.count(GL_SCISSOR_TEST));
        assert(r.end_frame(false));
        r.shutdown();
        empty();
    }
    // Resolve real submitted coordinates through VitaGL's FBO convention.
    // Both raw color and converted copies must put the same source corners at
    // the same engine texture rows, including a nonzero destination region.
    {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program());
        const auto source = r.create_target(128, 128);
        const auto converted = r.create_target(96, 96);
        const auto plain = r.create_target(96, 96);
        assert(source && converted && plain && r.begin_frame());
        const auto mainFb = drawFb;
        assert(r.select_target(source));
        const auto sourceFb = drawFb;
        for (bool regional : { false, true })
        {
            TextureCopy copy{ converted, regional ? std::array<float, 4>{ 3, 7, 66, 81 }
                                                  : std::array<float, 4>{ 0, 0, 128, 128 } };
            copy.alphaOnly = true;
            copy.preserveDestination = true;
            if (regional)
                copy.destinationRect = { 5, 9, 24, 32 };
            assert(r.copy_textures({ &copy, 1 }));
            ModelVertex quad[4];
            assert(submitted.back().size() >= sizeof(quad));
            std::memcpy(quad, submitted.back().data(), sizeof(quad));
            const auto convertedCorners = provider_quad_corners(quad, submittedViewports.back());
            copy.destination = plain;
            copy.alphaOnly = false;
            assert(r.copy_textures({ &copy, 1 }));
            assert(submittedTargets.back() != sourceFb);
            std::memcpy(quad, submitted.back().data(), sizeof(quad));
            const auto plainCorners = provider_quad_corners(quad, submittedViewports.back());
            assert_same_copy_corners(plainCorners, convertedCorners);
            const float left = regional ? 5.f : 0.f, top = regional ? 9.f : 0.f;
            const float right = regional ? 29.f : 96.f, bottom = regional ? 41.f : 96.f;
            const float u0 = regional ? 3.f / 128.f : 0.f, u1 = regional ? 69.f / 128.f : 1.f;
            const float v0 = regional ? 121.f / 128.f : 1.f, v1 = regional ? 40.f / 128.f : 0.f;
            const std::array<CopyCorner, 4> expected{{
                {left, top, u0, v0}, {left, bottom, u0, v1},
                {right, bottom, u1, v1}, {right, top, u1, v0}
            }};
            assert_same_copy_corners(plainCorners, expected);
            assert(drawFb == sourceFb && readFb == sourceFb);
        }
        // Presentation uses the display viewport, where destination row zero
        // already is the screen top. Its original endpoints must stay intact.
        assert(r.end_frame());
        assert(lastBlitReadFb == mainFb && lastBlitDrawFb == 0);
        assert((lastBlitSource == std::array<int, 4>{ 0, 0, 960, 544 }));
        assert((lastBlitDestination == std::array<int, 4>{ 0, 0, 960, 544 }));
        const auto presentedCorners = provider_blit_corners(
            lastBlitSource, lastBlitDestination, 960, 544, 544, true);
        const std::array<CopyCorner, 4> displayed{{
            {0, 0, 0, 1}, {0, 544, 0, 0}, {960, 544, 1, 0}, {960, 0, 1, 1}
        }};
        assert_same_copy_corners(presentedCorners, displayed);
        r.shutdown();
        empty();
    }
    // Exercise program selection in the production driver, including immutable
    // resident metadata, dynamic Q changes, current environment overrides and
    // the reference copy/movie and ill-conditioned-fog programs.
    for (bool affine : {false, true})
        for (bool fog : {false, true})
        {
            setenv("STRIKERS_VITAGL_AFFINE_UV", affine ? "1" : "0", 1);
            setenv("STRIKERS_VITAGL_VERTEX_FOG", fog ? "1" : "0", 1);
            Renderer r;
            const auto progressStart = shaderProgress.size();
            assert(r.initialize(true) && r.initialize_scene_program());
            const unsigned total = affine ? 162 : 82;
            assert(programs.size() == total);
            assert(shaders.size() == (fog ? 4 : 2));
            assert(shaderProgress.size() == progressStart + total + 1);
            for (unsigned step = 0; step <= total; ++step)
                assert((shaderProgress[progressStart + step] == std::array<unsigned, 2>{step, total}));
            const auto creationsBefore = programCreations, boneWritesBefore = boneUniformWrites;
            assert(r.initialize_scene_program() && programCreations == creationsBefore);
            assert(shaderProgress.size() == progressStart + total + 1);
            std::array<ModelVertex, 3> v;
            std::array<uint16_t, 3> i{0, 1, 2};
            auto model = r.create_model(v, i);
            assert(model && r.begin_frame());
            SceneMaterial m;
            m.controls[0] = 1;
            m.fog = {100, 1000, 1, 2};
            Raster raster;
            const auto check_shader = [&](bool expectedAffine, bool expectedFog, bool generic = false) {
                const auto& src = programSources.at(program);
                const auto& fs = src.at(GL_CG_FRAGMENT_SHADER_EXT);
                assert((fs.find("STRIKERS_AFFINE_UV 1") != std::string::npos) == expectedAffine);
                assert((fs.find("STRIKERS_VERTEX_FOG 1") != std::string::npos) == expectedFog);
                assert((src.at(GL_CG_VERTEX_SHADER_EXT).find("STRIKERS_VERTEX_FOG 1") != std::string::npos) == expectedFog);
                assert((fs.find("float mask=u_controls.x;") != std::string::npos) == generic);
                assert(!program_uses_bones(program) && glGetUniformLocation(program, "u_bones") == -1);
            };
            assert(r.draw_scene(model, {}, {}, identity, raster, m));
            check_shader(affine, fog);
            // The cached model is independent of subsequent caller mutation.
            v[1].uv[2] = 2;
            assert(r.draw_scene(model, {}, {}, identity, raster, m));
            check_shader(affine, fog);
            assert(r.draw_scene(0, v, i, identity, raster, m));
            check_shader(false, fog);
            const auto projective = r.create_model(v, i);
            assert(projective && r.draw_scene(projective, {}, {}, identity, raster, m));
            check_shader(false, fog);
            // Mobile texgen replaces this projective source with Q=1.
            m.environment[1] = 1;
            assert(r.draw_scene(projective, {}, {}, identity, raster, m));
            check_shader(affine, fog);
            m.environment[1] = 0;
            assert(r.draw_scene(projective, {}, {}, identity, raster, m));
            check_shader(false, fog);
            // Generic conversion families always retain the reference stages.
            for (float family : {2.f, 3.f, 4.f})
            {
                m.controls[3] = family;
                assert(r.draw_scene(model, {}, {}, identity, raster, m));
                check_shader(false, false, true);
            }
            m.controls[3] = 0;
            m.fog = {10000, 10000.01f, 1, 0};
            assert(r.draw_scene(model, {}, {}, identity, raster, m));
            check_shader(fog ? false : affine, false, fog);
            m.fog = {100, 1000, 1, 2};
            // Dynamic uploads must recover affine eligibility on a later draw.
            v[1].uv[2] = 1;
            assert(r.draw_scene(0, v, i, identity, raster, m));
            check_shader(affine, fog);
            // Recreating resident geometry must not retain the old Q metadata.
            r.release_model(projective);
            auto replaced = r.create_model(v, i);
            assert(replaced && r.draw_scene(replaced, {}, {}, identity, raster, m));
            check_shader(affine, fog);
            assert(r.end_frame());
            const auto t = r.completed_timing();
            assert(t.draws == 12);
            assert(t.affineUvDraws + t.projectiveUvDraws == t.draws);
            assert((t.affineUvDraws > 0) == affine);
            assert((t.vertexFogDraws > 0) == fog);
            assert(programCreations == creationsBefore && boneUniformWrites == boneWritesBefore);
            r.shutdown();
            empty();
        }
    unsetenv("STRIKERS_VITAGL_AFFINE_UV");
    unsetenv("STRIKERS_VITAGL_VERTEX_FOG");
    // Program families follow validated geometry, never material palette
    // contents. Switching dynamic/resident draws must not inherit PN state.
    {
        setenv("STRIKERS_VITAGL_AFFINE_UV", "1", 1);
        setenv("STRIKERS_VITAGL_VERTEX_FOG", "1", 1);
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program());
        std::array<ModelVertex, 3> vertices;
        const std::array<uint16_t, 3> indices{0, 1, 2};
        const auto rigid = r.create_model(vertices, indices);
        vertices[0].boneSlot = float(kSceneSkinBoneCount);
        const auto pn = r.create_model(vertices, indices);
        assert(rigid && pn && r.begin_frame());
        SceneMaterial m;
        m.controls[0] = 1;
        m.fog = {100, 1000, 1, 2};
        auto matrix = identity;
        unsigned draw = 0;
        const auto creations = programCreations;
        const auto check_draw = [&](uint32_t model, bool usesBones) {
            // Changing other vertex uniforms must not restore the absent
            // palette. Changing its final value forces a real PN upload.
            matrix[12] = float(++draw);
            m.lightColor[7][0] = float(draw);
            m.bones.back()[3] = float(draw);
            const auto before = boneUniformWrites;
            assert(r.draw_scene(model, vertices, indices, matrix, Raster{}, m));
            assert(program_uses_bones(program) == usesBones);
            assert((glGetUniformLocation(program, "u_bones") != -1) == usesBones);
            assert(boneUniformWrites == before + unsigned(usesBones));
            if (usesBones)
                assert(bones.size() == kSceneBoneFloats && bones.back() == float(draw));
            return program;
        };
        const auto rigidProgram = check_draw(rigid, false);
        const auto pnProgram = check_draw(0, true);
        assert(rigidProgram != pnProgram);
        for (auto selected : {rigidProgram, pnProgram})
        {
            const auto& sources = programSources.at(selected);
            assert(sources.at(GL_CG_FRAGMENT_SHADER_EXT).find("float mask=1.0;") != std::string::npos);
            assert(sources.at(GL_CG_FRAGMENT_SHADER_EXT).find("STRIKERS_AFFINE_UV 1") != std::string::npos);
            assert(sources.at(GL_CG_VERTEX_SHADER_EXT).find("STRIKERS_VERTEX_FOG 1") != std::string::npos);
        }
        assert(check_draw(rigid, false) == rigidProgram); // source array now PN
        vertices[0].boneSlot = 0;
        assert(check_draw(pn, true) == pnProgram); // source array now rigid
        assert(check_draw(0, false) == rigidProgram);
        r.release_model(pn);
        const auto replacement = r.create_model(vertices, indices);
        assert(replacement && check_draw(replacement, false) == rigidProgram);

        // Missing masks, movie/conversion families and ill-conditioned fog
        // all retain the rigid/PN distinction in the generic fallback key.
        std::array<GLuint, 2> fallbackPrograms{};
        for (unsigned fallback = 0; fallback < 5; ++fallback)
        {
            m.controls[0] = fallback == 0 ? 2.f : 1.f;
            m.controls[3] = fallback >= 1 && fallback <= 3 ? float(fallback + 1) : 0.f;
            m.fog = fallback == 4 ? std::array<float, 4>{10000, 10000.01f, 1, 0}
                                  : std::array<float, 4>{100, 1000, 1, 2};
            for (bool usesBones : {false, true})
            {
                vertices[0].boneSlot = usesBones ? float(kSceneSkinBoneCount) : -0.f;
                const auto selected = check_draw(0, usesBones);
                const auto& sources = programSources.at(selected);
                assert(sources.at(GL_CG_FRAGMENT_SHADER_EXT).find("float mask=u_controls.x;") != std::string::npos);
                assert(sources.at(GL_CG_FRAGMENT_SHADER_EXT).find("STRIKERS_AFFINE_UV 0") != std::string::npos);
                assert(sources.at(GL_CG_VERTEX_SHADER_EXT).find("STRIKERS_VERTEX_FOG 0") != std::string::npos);
                if (fallback == 0)
                    fallbackPrograms[usesBones] = selected;
                else
                    assert(fallbackPrograms[usesBones] == selected);
            }
        }
        assert(fallbackPrograms[0] != fallbackPrograms[1] && programCreations == creations);
        assert(r.end_frame(false));
        r.shutdown();
        empty();
        unsetenv("STRIKERS_VITAGL_AFFINE_UV");
        unsetenv("STRIKERS_VITAGL_VERTEX_FOG");
    }
    // Line topology survives both immutable uploads and dynamic submission.
    // Polygon culling never suppresses lines, including the cull-all mode.
    {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program());
        std::array<ModelVertex, 3> vertices;
        vertices[1].position[0] = 1;
        vertices[2].position[1] = 1;
        const std::array<uint16_t, 2> lineIndices{ 0, 1 };
        const std::array<uint16_t, 3> triangleIndices{ 0, 1, 2 };
        const std::span<const ModelVertex> lineVertices{ vertices.data(), 2 };
        const auto lineModel = r.create_model(lineVertices, lineIndices);
        const auto triangleModel = r.create_model(vertices, triangleIndices);
        assert(lineModel && triangleModel && r.begin_frame());
        Raster lineRaster;
        lineRaster.primitive = 1;
        lineRaster.cull = 3;
        SceneMaterial material;
        auto priorDraws = submitted.size();
        assert(r.draw_scene(lineModel, {}, {}, identity, lineRaster, material));
        assert(submitted.size() == priorDraws + 1 && submittedPrimitives.back() == GL_LINES);
        assert(!enabled.count(GL_CULL_FACE));
        lineRaster.cull = 2;
        assert(r.draw_scene(0, lineVertices, lineIndices, identity, lineRaster, material));
        assert(submittedPrimitives.back() == GL_LINES && !enabled.count(GL_CULL_FACE));
        Raster triangleRaster;
        triangleRaster.cull = 2;
        assert(r.draw_scene(triangleModel, {}, {}, identity, triangleRaster, material));
        assert(submittedPrimitives.back() == GL_TRIANGLES && enabled.count(GL_CULL_FACE));
        assert(r.draw_scene(lineModel, {}, {}, identity, lineRaster, material));
        assert(submittedPrimitives.back() == GL_LINES && !enabled.count(GL_CULL_FACE));
        const auto priorUploads = bufferUploads;
        priorDraws = submitted.size();
        assert(!r.draw_scene(triangleModel, {}, {}, identity, lineRaster, material));
        assert(!r.draw_scene(0, vertices, triangleIndices, identity, lineRaster, material));
        assert(!r.draw_scene(lineModel, {}, {}, identity, triangleRaster, material));
        assert(!r.draw_scene(0, lineVertices, lineIndices, identity, triangleRaster, material));
        lineRaster.primitive = 2;
        assert(!lineRaster.valid() &&
               !r.draw_scene(lineModel, {}, {}, identity, lineRaster, material));
        assert(bufferUploads == priorUploads && submitted.size() == priorDraws);
        assert(r.end_frame(false));
        r.shutdown();
        empty();
    }
    // Restart the same renderer after use of offscreen targets and a driver
    // error. VitaGL retains its context; all owned objects and caches restart.
    {
        Renderer r;
        const auto initializationsBefore = contextInitializations;
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            setenv("STRIKERS_VITAGL_AFFINE_UV", cycle == 1 ? "1" : "0", 1);
            setenv("STRIKERS_VITAGL_VERTEX_FOG", cycle == 1 ? "1" : "0", 1);
            assert(r.initialize() && r.error_code() == 0 && r.initialize_scene_program());
            assert(contextInitializations == initializationsBefore + 1);
            assert(programs.size() == (cycle == 1 ? 162 : 82));
            assert(shaders.size() == (cycle == 1 ? 4 : 2));
            assert((viewport == std::array<int, 4>{ 0, 0, 960, 544 }));
            const auto target = r.create_target(74, 75);
            std::array<ModelVertex, 3> vertices;
            const std::array<uint16_t, 3> indices{ 0, 1, 2 };
            const auto model = r.create_model(vertices, indices);
            assert(target && model && r.begin_frame() && r.select_target(target));
            SceneMaterial material;
            material.viewport = material.scissor = { 0, 0, 74, 75 };
            assert(r.draw_scene(model, {}, {}, identity, Raster{}, material));
            if (!cycle)
            {
                failDraw = true;
                assert(!r.draw_scene(model, {}, {}, identity, Raster{}, material));
                failDraw = false;
                assert(r.error_code() == GL_INVALID_OPERATION);
            }
            assert(r.end_frame(false));
            r.shutdown();
            empty();
        }
    }
    unsetenv("STRIKERS_VITAGL_AFFINE_UV");
    unsetenv("STRIKERS_VITAGL_VERTEX_FOG");
    // An incomplete shader initialization must also allow a same-object retry
    // after shutdown, without retaining the failure or recreating GPU pools.
    {
        Renderer r;
        assert(r.initialize());
        const auto initializationsBefore = contextInitializations;
        failCompile = true;
        assert(!r.initialize_scene_program());
        failCompile = false;
        r.shutdown();
        empty();
        assert(r.initialize() && r.initialize_scene_program() && r.error_code() == 0);
        assert(contextInitializations == initializationsBefore);
        r.shutdown();
        empty();
    }
    // Failing the additional rigid family must unwind the already-prepared
    // PN programs, then permit a complete same-object restart.
    for (unsigned failure = 0; failure < 3; ++failure)
    {
        Renderer r;
        assert(r.initialize());
        const auto initializationsBefore = contextInitializations;
        failRigidCompile = failure == 0;
        failRigidLink = failure == 1;
        forceRigidBoneUniform = failure == 2;
        assert(!r.initialize_scene_program() && programs.size() == 41);
        const char* expected[]{"shader_compile", "shader_link", "rigid_bone_uniform"};
        assert(std::strcmp(r.last_error(), expected[failure]) == 0);
        const auto creations = programCreations;
        assert(!r.initialize_scene_program() && programCreations == creations);
        failRigidCompile = failRigidLink = forceRigidBoneUniform = false;
        r.shutdown();
        empty();
        assert(r.initialize() && r.initialize_scene_program() && !r.error_code());
        assert(programs.size() == 82 && shaders.size() == 2);
        assert(contextInitializations == initializationsBefore);
        r.shutdown();
        empty();
    }
    // Reflection must expose the complete dynamically indexed palette. A
    // missing or truncated u_bones must fail before any draw can use it.
    for (bool missing : {false, true})
    {
        Renderer r;
        assert(r.initialize());
        omitBoneUniform = missing;
        capacities[2] = kSceneBoneRows - 1;
        assert(!r.initialize_scene_program());
        assert(std::strcmp(r.last_error(), missing ? "bone_uniform_missing" : "bone_uniform_capacity") == 0);
        omitBoneUniform = false;
        capacities[2] = kSceneBoneRows;
        r.shutdown();
        empty();
    }
    // Both immutable and dynamic vertices may select the complete palette.
    // Reject non-integral and out-of-range slots before uploading GPU data.
    {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program() && r.begin_frame());
        std::array<ModelVertex, 3> vertices;
        const std::array<uint16_t, 3> indices{0, 1, 2};
        SceneMaterial m;
        for (unsigned slot : {0u, 9u, 10u, 13u, kSceneSkinBoneCount})
        {
            vertices[0].boneSlot = float(slot);
            const auto model = r.create_model(vertices, indices);
            assert(model);
            assert(r.draw_scene(model, {}, {}, identity, Raster{}, m));
            assert(r.draw_scene(0, vertices, indices, identity, Raster{}, m));
            r.release_model(model);
        }
        const auto uploadsBefore = bufferUploads;
        const auto drawsBefore = submitted.size();
        for (float slot : {-1.f, .5f, float(kSceneSkinBoneCount + 1),
                           std::numeric_limits<float>::infinity(),
                           std::numeric_limits<float>::quiet_NaN()})
        {
            vertices[0].boneSlot = slot;
            assert(!r.create_model(vertices, indices));
            assert(!r.draw_scene(0, vertices, indices, identity, Raster{}, m));
        }
        assert(bufferUploads == uploadsBefore && submitted.size() == drawsBefore);
        assert(r.end_frame(false));
        r.shutdown();
        empty();
    }
    // VI blanking is staged; only completed presentations advance the original
    // three-frame counter. Black output never clears the offscreen scene.
    {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program());
        const auto initialSwaps = swaps, initialBlits = blits;
        const auto initialClears = clears, initialBinds = framebufferBinds;
        const auto retired = startupRetireCalls;
        assert(r.set_black(true) && !r.frame_active());
        assert(swaps == initialSwaps && blits == initialBlits && clears == initialClears &&
               framebufferBinds == initialBinds);
        for (unsigned aborted = 0; aborted < 5; ++aborted)
            assert(r.begin_frame() && r.end_frame(false));
        assert(swaps == initialSwaps && blits == initialBlits && startupRetireCalls == retired);
        for (unsigned frame = 0; frame < 3; ++frame)
        {
            assert(r.begin_frame());
            const auto mainFb = drawFb, written = surfaceWrites.at(mainFb);
            assert(r.end_frame());
            assert(surfaceWrites.at(mainFb) == written);
            assert(displayContents.back() == (frame < 2 ? DisplayContent::Black : DisplayContent::Scene));
            assert(startupRetireCalls == retired + unsigned(frame == 2));
            if (frame < 2)
            {
                const auto& clear = clearCalls.back();
                assert(clear.target == 0 && clear.mask == GL_COLOR_BUFFER_BIT && !clear.scissorEnabled);
                assert((clear.color == std::array<float, 4>{0, 0, 0, 1}));
                assert((clear.colorMask == std::array<bool, 4>{true, true, true, true}));
            }
        }
        assert(swaps == initialSwaps + 3 && blits == initialBlits + 1);
        for (unsigned frame = 0; frame < 4; ++frame)
        {
            assert(r.set_black(true) && r.begin_frame() && r.end_frame());
            assert(displayContents.back() == DisplayContent::Black);
        }
        const auto beforeFalse = swaps;
        assert(r.set_black(false) && swaps == beforeFalse);
        assert(r.begin_frame() && r.end_frame() && displayContents.back() == DisplayContent::Scene);
        assert(r.set_black(true));
        r.shutdown();
        empty();
        assert(r.initialize() && r.initialize_scene_program() && r.begin_frame() && r.end_frame());
        assert(displayContents.back() == DisplayContent::Scene);
        r.shutdown();
        empty();
    }
    // Neither an unsuccessful clear nor a failed swap consumes the countdown.
    for (bool afterSwap : {false, true})
    {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program() && r.set_black(true) && r.begin_frame());
        failClear = !afterSwap;
        failSwap = afterSwap;
        assert(!r.end_frame());
        failClear = failSwap = false;
        assert(r.error_code() == GL_INVALID_OPERATION);
        for (unsigned frame = 0; frame < 3; ++frame)
        {
            if (!r.frame_active())
                assert(r.begin_frame());
            assert(r.end_frame());
            assert(displayContents.back() == (frame < 2 ? DisplayContent::Black : DisplayContent::Scene));
        }
        // Successful later calls retain the original error for diagnosis.
        assert(r.error_code() == GL_INVALID_OPERATION);
        r.shutdown();
        empty();
    }
    const auto makeBitmap = [] {
        TextureUpload image;
        image.width = image.stride = 8;
        image.height = 2;
        image.mipCount = 1;
        image.storage = TextureStorage::Rgba8;
        image.pixels.assign(64, 0);
        image.pixels[0] = 255;
        image.pixels[3] = 255;
        image.pixels[62] = 255;
        image.pixels[63] = 255;
        return image;
    };
    // A loading bitmap publishes independently of both idle and active engine
    // frames. The display viewport has the provider's opposite Y direction to
    // rendered FBOs, so verify asymmetric physical corners numerically too.
    {
        Renderer r;
        assert(r.initialize(true) && r.initialize_scene_program());
        auto image = makeBitmap();
        const std::array<float, 4> destination{90, 68, 480, 136};
        const auto textureCount = textures.size(), framebufferCount = framebuffers.size();
        const auto retired = startupRetireCalls, waits = completionWaits, beforeBlits = blits;
        assert(r.set_black(true) && r.present_bitmap(image, destination) && !r.frame_active());
        assert(displayContents.back() == DisplayContent::Bitmap && completionWaits == waits + 1);
        assert(startupRetireCalls == retired + 1 && textures.size() == textureCount &&
               framebuffers.size() == framebufferCount && blits == beforeBlits);
        assert(r.completed_timing().draws == 0 && drawFb == 0 && readFb == 0);
        assert(submittedTargets.back() == 0 &&
               (submittedViewports.back() == std::array<int, 4>{0, 0, 960, 544}));
        ModelVertex quad[4];
        std::memcpy(quad, submitted.back().data(), sizeof quad);
        const std::array<std::array<float, 4>, 4> expectedCorners{{
            {90, 68, 0, 0}, {570, 68, 1, 0}, {570, 204, 1, 1}, {90, 204, 0, 1}
        }};
        for (unsigned corner = 0; corner < 4; ++corner)
        {
            // misc.c glViewport: display y_port=H-y-h/2, y_scale=-h/2.
            const float x = (quad[corner].position[0] + 1.f) * 480.f;
            const float y = 272.f - quad[corner].position[1] * 272.f;
            assert(std::fabs(x - expectedCorners[corner][0]) < .0001f &&
                   std::fabs(y - expectedCorners[corner][1]) < .0001f &&
                   quad[corner].uv[0] == expectedCorners[corner][2] &&
                   quad[corner].uv[1] == expectedCorners[corner][3]);
        }
        std::vector<uint8_t> pixels;
        assert(r.readback_presented_rgba8(pixels));
        assert(r.begin_frame());
        const auto mainFb = drawFb;
        const auto target = r.create_target(80, 74);
        assert(target && r.select_target(target));
        const auto selectedFb = drawFb;
        std::array<ModelVertex, 3> vertices;
        const std::array<uint16_t, 3> indices{0, 1, 2};
        SceneMaterial material;
        material.controls[0] = 1;
        material.tint[0] = .25f;
        material.viewport = {3, 5, 60, 40};
        material.scissor = {10, 12, 43, 50};
        assert(r.draw_scene(0, vertices, indices, identity, Raster{}, material));
        const auto sceneViewport = viewport, sceneScissor = scissor;
        const auto mainWritten = surfaceWrites.at(mainFb), targetWritten = surfaceWrites.at(selectedFb);
        const auto activeTextureCount = textures.size();
        const auto beforeBitmapSwaps = swaps;
        // Preserve independent read and draw bindings, not only target metadata.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, mainFb);
        assert(r.set_black(true));
        for (unsigned message = 0; message < 7; ++message)
        {
            assert(r.present_bitmap(image, destination) && r.frame_active());
            assert(drawFb == selectedFb && readFb == mainFb && textures.size() == activeTextureCount);
            assert(surfaceWrites.at(mainFb) == mainWritten && surfaceWrites.at(selectedFb) == targetWritten);
        }
        assert(swaps == beforeBitmapSwaps + 7 && blits == beforeBlits &&
               r.completed_timing().draws == 0);
        const auto beforeAttributes = attributePointers;
        assert(r.draw_scene(0, vertices, indices, identity, Raster{}, material));
        assert(submittedTargets.back() == selectedFb && viewport == sceneViewport && scissor == sceneScissor);
        assert(attributePointers == beforeAttributes + 10 && tintByProgram.at(program)[0] == .25f);
        const auto beforeDiscard = swaps;
        assert(r.end_frame(false) && !r.frame_active() && swaps == beforeDiscard);
        assert(r.completed_timing().draws == 2 &&
               r.completed_timing().dynamicVertexBytes == vertices.size() * sizeof(ModelVertex) * 2);
        // No explicit false: DisplayMessage removes black but keeps nFirstFrame.
        for (unsigned frame = 0; frame < 3; ++frame)
        {
            assert(r.begin_frame() && r.end_frame());
            assert(displayContents.back() == DisplayContent::Scene);
        }
        assert(r.begin_frame() && r.set_black(true) && r.present_bitmap(image, destination));
        const auto afterBitmap = swaps;
        assert(r.set_black(false) && swaps == afterBitmap && r.frame_active());
        assert(r.end_frame(false) && swaps == afterBitmap);
        r.release_texture(target);
        r.shutdown();
        empty();
    }
    // Every partial display transaction unwinds its temporary texture and
    // restores the open offscreen pass. Failed output does not unstage black.
    for (unsigned failure = 0; failure < 5; ++failure)
    {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program() && r.begin_frame());
        const auto target = r.create_target(80, 74);
        assert(target && r.select_target(target) && r.set_black(true));
        const auto selectedFb = drawFb, written = surfaceWrites.at(selectedFb);
        const auto textureCount = textures.size(), framebufferCount = framebuffers.size();
        const auto retired = startupRetireCalls, beforeSwaps = swaps;
        failTexture = failure == 0;
        failClear = failure == 1;
        failDraw = failure == 2;
        failSwap = failure == 3;
        displayCompletionResult = failure == 4 ? -723 : 0;
        assert(!r.present_bitmap(makeBitmap(), {90, 68, 480, 136}));
        failTexture = failClear = failDraw = failSwap = false;
        displayCompletionResult = 0;
        assert(r.frame_active() && drawFb == selectedFb && readFb == selectedFb &&
               surfaceWrites.at(selectedFb) == written);
        assert(textures.size() == textureCount && framebuffers.size() == framebufferCount &&
               startupRetireCalls == retired);
        assert(swaps == beforeSwaps + unsigned(failure >= 3));
        const int expectedError = failure == 0 ? GL_OUT_OF_MEMORY : failure == 4 ? -723 : GL_INVALID_OPERATION;
        assert(r.error_code() == expectedError);
        for (unsigned frame = 0; frame < 3; ++frame)
        {
            if (!r.frame_active())
                assert(r.begin_frame());
            assert(r.end_frame());
            assert(displayContents.back() == (frame < 2 ? DisplayContent::Black : DisplayContent::Scene));
        }
        assert(r.error_code() == expectedError);
        r.release_texture(target);
        r.shutdown();
        empty();
    }
    const auto makeLoading = [] {
        LoadingIndicator request;
        for (auto& image : request.images) {
            image.width = image.height = image.stride = 32;
            image.mipCount = 1;
            image.storage = TextureStorage::Rgba8;
            image.pixels.assign(32 * 32 * 4, 255);
        }
        for (unsigned i = 0; i < 3; ++i)
            request.rectangles[i] = {432.f + 36.f * i, 382.f * 544.f / 448.f,
                                     48.f, 32.f * 544.f / 448.f};
        request.waitRetraces = 120;
        request.phaseRetraces = 19;
        return request;
    };
    // A private background comes from the completed display, even though the
    // current main has already been cleared and another target is open.
    {
        Renderer r;
        assert(r.initialize(true) && r.initialize_scene_program());
        const auto idleQueries = retraceQueries, idleMemory = memoryQueries;
        assert(r.loading_tick() && !r.loading_active() && r.set_loading(false, {}));
        assert(retraceQueries == idleQueries && memoryQueries == idleMemory);
        assert(r.begin_frame() && r.end_frame() && r.begin_frame());
        const auto mainFb = drawFb;
        const auto target = r.create_target(80, 74);
        assert(target && r.select_target(target));
        const auto selectedFb = drawFb;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, mainFb);
        const auto mainWrites = surfaceWrites.at(mainFb), targetWrites = surfaceWrites.at(selectedFb);
        const auto textureCount = textures.size(), framebufferCount = framebuffers.size();
        const auto bufferCount = buffers.size(), depthCount = renderbuffers.size();
        const auto initialSwaps = swaps, initialWaits = completionWaits;
        const auto request = makeLoading();
        retrace = 1000;
        assert(r.set_loading(true, request) && r.loading_active() && r.frame_active());
        assert(drawFb == selectedFb && readFb == mainFb && readMode == GL_BACK);
        assert(surfaceWrites.at(mainFb) == mainWrites && surfaceWrites.at(selectedFb) == targetWrites);
        assert(swaps == initialSwaps && completionWaits == initialWaits + 1);
        assert(textures.size() == textureCount + 3 && framebuffers.size() == framebufferCount + 1 &&
               buffers.size() == bufferCount + 5 && renderbuffers.size() == depthCount);
        assert(lastBlitReadFb == 0 && lastBlitDrawFb != mainFb && lastBlitDrawFb != selectedFb);
        assert((lastBlitSource == std::array<int, 4>{0, 0, 960, 544}));
        assert((lastBlitDestination == std::array<int, 4>{0, 544, 960, 0}));
        const auto background = framebufferColors.at(lastBlitDrawFb);
        assert(textureContents.at(background) == DisplayContent::Scene);
        // Provider default-source V is top-first, unlike FBO sources. The
        // reversed destination stores top at V=0; the overlay samples V=0 top.
        const auto capture = provider_blit_corners(lastBlitSource, lastBlitDestination, 960, 544, 544, false, true);
        const std::array<CopyCorner, 4> expected{{{0,0,0,0},{0,544,0,1},{960,544,1,1},{960,0,1,0}}};
        assert_same_copy_corners(capture, expected);
        const auto allocationEnd = next, uploadEnd = bufferUploads, memoryEnd = memoryQueries;
        const auto drawStart = loadingDraws.size();
        retrace += 119;
        assert(r.loading_tick() && swaps == initialSwaps && loadingDraws.size() == drawStart);
        ++retrace;
        assert(r.loading_tick() && swaps == initialSwaps + 1 && loadingDraws.size() == drawStart + 4);
        assert(displayContents.back() == DisplayContent::Loading);
        const auto selectedImage = loadingDraws[drawStart + 1].operations;
        const auto normalImage = loadingDraws[drawStart + 2].operations;
        assert(selectedImage != normalImage && loadingDraws[drawStart + 3].operations == normalImage);
        for (unsigned i = 0; i < 3; ++i) {
            const auto& region = loadingDraws[drawStart + 1 + i].region;
            assert(std::fabs(region[0] - request.rectangles[i][0] / 960.f) < .000001f);
            assert(std::fabs(region[1] - request.rectangles[i][1] / 544.f) < .000001f);
            assert(std::fabs(region[2] - 1.f / 640.f) < .000001f);
            assert(std::fabs(region[3] - 1.f / 448.f) < .000001f);
        }
        for (auto id : {background, selectedImage, normalImage}) {
            assert(textureParameters.at(id).at(GL_TEXTURE_MIN_FILTER) == GL_NEAREST);
            assert(textureParameters.at(id).at(GL_TEXTURE_MAG_FILTER) == GL_NEAREST);
            assert(textureParameters.at(id).at(GL_TEXTURE_WRAP_S) == GL_CLAMP_TO_EDGE);
            assert(textureParameters.at(id).at(GL_TEXTURE_WRAP_T) == GL_CLAMP_TO_EDGE);
        }
        retrace += 18;
        assert(r.loading_tick() && loadingDraws[loadingDraws.size() - 3].operations == selectedImage);
        ++retrace;
        assert(r.loading_tick() && loadingDraws[loadingDraws.size() - 3].operations == normalImage &&
               loadingDraws[loadingDraws.size() - 2].operations == selectedImage);
        assert(next == allocationEnd && bufferUploads == uploadEnd && memoryQueries == memoryEnd);
        assert(drawFb == selectedFb && readFb == mainFb && r.frame_active());
        assert(surfaceWrites.at(mainFb) == mainWrites && surfaceWrites.at(selectedFb) == targetWrites);
        const auto beforeMessage = swaps;
        assert(r.present_bitmap(makeBitmap(), {90, 68, 480, 136}));
        assert(swaps == beforeMessage + 1 && textureContents.at(background) == DisplayContent::Bitmap);
        assert(drawFb == selectedFb && readFb == mainFb && r.frame_active());
        ++retrace;
        assert(r.loading_tick() && textureContents.at(background) == DisplayContent::Bitmap);
        assert(surfaceWrites.at(mainFb) == mainWrites && surfaceWrites.at(selectedFb) == targetWrites);
        const auto beforeEnd = swaps;
        assert(r.end_frame() && swaps == beforeEnd && r.completed_timing().draws == 0);
        assert(r.set_loading(false, {}) && !r.loading_active() && swaps == beforeEnd);
        assert(textures.size() == textureCount && framebuffers.size() == framebufferCount &&
               buffers.size() == bufferCount && renderbuffers.size() == depthCount);
        const auto offQueries = retraceQueries, offMemory = memoryQueries;
        assert(r.loading_tick() && retraceQueries == offQueries && memoryQueries == offMemory);
        assert(r.begin_frame() && r.end_frame() && displayContents.back() == DisplayContent::Scene);
        r.release_texture(target);
        r.shutdown();
        empty();
    }
    // ON -> OFF before the first tick still finishes the unsampled capture.
    // It preserves the open application frame and never promotes its contents.
    {
        Renderer r;
        assert(r.initialize(true) && r.initialize_scene_program());
        assert(r.begin_frame() && r.end_frame() && r.begin_frame());
        const auto mainFb = drawFb;
        const auto target = r.create_target(80, 74);
        assert(target && r.select_target(target));
        const auto selectedFb = drawFb;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, mainFb);
        const auto mainWrites = surfaceWrites.at(mainFb), targetWrites = surfaceWrites.at(selectedFb);
        const auto initialSwaps = swaps, initialFinishes = gpuFinishes;
        const auto initialDraws = loadingDraws.size();
        assert(r.set_loading(true, makeLoading()));
        const auto background = framebufferColors.at(lastBlitDrawFb);
        unfinishedLoadingTextures.insert(background);
        assert(gpuFinishes == initialFinishes && swaps == initialSwaps);
        assert(r.set_loading(false, {}) && !r.loading_active());
        assert(gpuFinishes == initialFinishes + 1 && unfinishedLoadingTextures.empty());
        assert(swaps == initialSwaps && loadingDraws.size() == initialDraws);
        assert(r.frame_active() && drawFb == selectedFb && readFb == mainFb);
        assert(surfaceWrites.at(mainFb) == mainWrites && surfaceWrites.at(selectedFb) == targetWrites);
        assert(!textures.count(background));
        assert(r.set_loading(false, {}) && gpuFinishes == initialFinishes + 1);
        assert(r.end_frame(false) && swaps == initialSwaps);
        r.release_texture(target);
        r.shutdown();
        empty();
    }
    // Save/restore clears the displayed XFBs immediately, but consumes neither
    // firstFrames nor icon phase and does not change the staged black flag.
    {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program() && r.set_black(true));
        auto request = makeLoading();
        request.blackBackground = true;
        request.waitRetraces = 0;
        retrace = 2000;
        const auto initialSwaps = swaps;
        const auto initialDraws = loadingDraws.size();
        const auto retired = startupRetireCalls;
        assert(r.set_loading(true, request) && swaps == initialSwaps + 1);
        assert(displayContents.back() == DisplayContent::Black && loadingDraws.size() == initialDraws + 1);
        assert(startupRetireCalls == retired);
        assert(r.loading_tick() && swaps == initialSwaps + 1);
        assert(r.set_loading(false, {}) && swaps == initialSwaps + 1);
        for (unsigned frame = 0; frame < 3; ++frame) {
            assert(r.begin_frame() && r.end_frame());
            assert(displayContents.back() == (frame < 2 ? DisplayContent::Black : DisplayContent::Scene));
            assert(startupRetireCalls == retired + unsigned(frame == 2));
        }
        assert(r.set_black(true));
        request.blackBackground = false;
        assert(r.set_loading(true, request));
        const auto countdownSwaps = swaps;
        for (unsigned tick = 0; tick < 3; ++tick) {
            ++retrace;
            assert(r.loading_tick());
            assert(swaps == countdownSwaps + unsigned(tick == 2));
        }
        assert(displayContents.back() == DisplayContent::Black);
        ++retrace;
        assert(r.loading_tick() && displayContents.back() == DisplayContent::Loading);
        const auto invalid = request;
        request.images[0].pixels.pop_back();
        assert(!r.set_loading(true, request) && !r.loading_active());
        assert(r.set_loading(false, {}));
        assert(r.set_loading(true, invalid));
        r.shutdown();
        empty();
    }
    // A timer-only startup also retains the old startup resource through pure
    // blanking; the first rendered indicator is an application replacement.
    {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program() && r.set_black(true));
        auto request = makeLoading();
        request.waitRetraces = 0;
        const auto retired = startupRetireCalls;
        assert(r.set_loading(true, request));
        for (unsigned tick = 0; tick < 4; ++tick) {
            ++retrace;
            assert(r.loading_tick());
            assert(startupRetireCalls == retired + unsigned(tick == 3));
        }
        r.shutdown();
        empty();
    }
    // Every partial arm restores the open frame and releases transient objects;
    // a lazily created shader may remain cached until renderer shutdown.
    for (unsigned failure = 0; failure < 9; ++failure) {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program() && r.begin_frame() && r.end_frame() && r.begin_frame());
        const auto frame = drawFb, writes = surfaceWrites.at(drawFb), beforeSwaps = swaps;
        const auto textureCount = textures.size(), framebufferCount = framebuffers.size();
        const auto bufferCount = buffers.size(), depthCount = renderbuffers.size(), shaderCount = shaders.size();
        auto request = makeLoading();
        if (failure == 0) request.images[1].pixels.clear();
        if (failure == 1) request.rectangles[2][0] = std::numeric_limits<float>::infinity();
        if (failure == 2) request.phaseRetraces = 0;
        failCompile = failure == 3;
        omitLoadingUniform = failure == 4;
        failTexture = failure == 5;
        failTarget = failure == 6;
        failBlit = failure == 7;
        displayCompletionResult = failure == 8 ? -723 : 0;
        assert(!r.set_loading(true, request) && !r.loading_active() && r.error_code());
        failCompile = omitLoadingUniform = failTexture = failTarget = failBlit = false;
        displayCompletionResult = 0;
        assert(r.frame_active() && drawFb == frame && readFb == frame && surfaceWrites.at(frame) == writes);
        assert(textures.size() == textureCount && framebuffers.size() == framebufferCount && buffers.size() == bufferCount &&
               renderbuffers.size() == depthCount && shaders.size() == shaderCount && swaps == beforeSwaps);
        assert(r.set_loading(false, {}) && r.end_frame(false));
        r.shutdown();
        empty();
    }
    // A failed countdown presentation disarms and preserves the uncommitted
    // final countdown step. Re-arming therefore displays black before icons.
    for (unsigned failure = 0; failure < 4; ++failure) {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program() && r.begin_frame() && r.end_frame() && r.begin_frame());
        const auto frame = drawFb, writes = surfaceWrites.at(drawFb);
        auto request = makeLoading();
        request.waitRetraces = 0;
        assert(r.set_black(true) && r.set_loading(true, request));
        retrace += 2;
        assert(r.loading_tick());
        ++retrace;
        failClear = failure == 0;
        failDraw = failure == 1;
        failSwap = failure == 2;
        displayCompletionResult = failure == 3 ? -723 : 0;
        assert(!r.loading_tick() && !r.loading_active());
        failClear = failDraw = failSwap = false;
        displayCompletionResult = 0;
        assert(r.frame_active() && drawFb == frame && surfaceWrites.at(frame) == writes);
        const auto firstError = r.error_code();
        assert(firstError && r.set_loading(false, {}) && r.set_loading(true, request));
        ++retrace;
        assert(r.loading_tick() && displayContents.back() == DisplayContent::Black);
        ++retrace;
        assert(r.loading_tick() && displayContents.back() == DisplayContent::Loading && r.error_code() == firstError);
        assert(r.set_loading(false, {}) && r.end_frame(false));
        r.shutdown();
        empty();
    }
    // Malformed geometry is rejected before any buffer upload.
    {
        Renderer r;
        assert(r.initialize());
        std::array<ModelVertex, 3> v;
        std::array<uint16_t, 3> i{ 0, 1, 3 };
        assert(!r.create_model(v, i));
        r.shutdown();
        empty();
    }
    // Incomplete targets and shader failure must unwind all owned GL objects.
    failTarget = true;
    {
        Renderer r;
        assert(!r.initialize());
    }
    failTarget = false;
    empty();
    failCompile = true;
    {
        Renderer r;
        assert(r.initialize() && !r.initialize_scene_program());
        assert(!r.initialize_scene_program());
    }
    failCompile = false;
    empty();
    failInit = true;
    {
        Renderer r;
        assert(!r.initialize());
    }
    failInit = false;
    empty();
    {
        Renderer r;
        assert(r.initialize());
        thread = 2;
        assert(!r.begin_frame());
        thread = 1;
    }
    empty();
    std::puts("VitaGL driver: immutable uploads, GL state restoration, exact scissor, uniform "
              "bounds, front-buffer readback, rejection and failure cleanup passed");
    // Init-only exact opt-in, owner deltas on present/discard, and clean restart.
    for (const char* flag : {"0", "yes", "1"})
    {
        setenv("STRIKERS_VITAGL_UNIFORM_CACHE", flag, 1);
        Renderer r;
        assert(r.initialize());
        assert(bool(uniformStats.enabled) == (std::strcmp(flag, "1") == 0));
        const auto snapshots = uniformSnapshots;
        assert(r.begin_frame() && r.end_frame(false));
        assert(!r.completed_timing().uniforms.valid && uniformSnapshots == snapshots);
        r.shutdown();
        empty();
    }
    setenv("STRIKERS_VITAGL_PACKET_PROFILE", "1", 1);
    {
        Renderer r;
        assert(r.initialize());
        uniformStats.stage[0][0] = 100;
        uniformStats.stage[0][1] = 1600;
        uniformStats.cache_bytes = 2048;
        uniformStats.cache_peak_bytes = 4096;
        uniformStats.fallback_uploads = 7;
        assert(r.begin_frame());
        uniformStats.stage[0][0] += 3;
        uniformStats.stage[0][1] += 48;
        uniformStats.stage[1][2] += 2;
        uniformStats.stage[1][3] += 128;
        uniformStats.fallback_uploads += 1;
        assert(r.end_frame(false));
        const auto u = r.completed_timing().uniforms;
        assert(u.valid && u.enabled && u.stage[0][0] == 3 && u.stage[0][1] == 48);
        assert(u.stage[1][2] == 2 && u.stage[1][3] == 128 && u.fallbackUploads == 1);
        assert(u.cacheBytes == 2048 && u.cachePeakBytes == 4096);
        assert(r.begin_frame());
        uniformStats.stage[0][0] += 1;
        assert(r.end_frame());
        assert(r.completed_timing().uniforms.stage[0][0] == 1);
        assert(r.completed_timing().uniforms.stage[0][1] == 0);
        assert(r.begin_frame());
        uniformStats.stage[0][0] = 0;
        assert(!r.end_frame(false) && !r.completed_timing().uniforms.valid);
        assert(std::strcmp(r.last_error(), "uniform_counter_reset") == 0);
        r.shutdown();
        empty();
    }
    setenv("STRIKERS_VITAGL_UNIFORM_VERIFY", "1", 1);
    {
        Renderer r;
        const auto contexts = contextInitializations;
        assert(!r.initialize() && contextInitializations == contexts);
        assert(std::strcmp(r.last_error(), "uniform_config") == 0);
    }
    unsetenv("STRIKERS_VITAGL_PACKET_PROFILE");
    {
        Renderer r;
        assert(r.initialize() && uniformStats.verifying);
        assert(r.begin_frame() && r.end_frame());
        r.shutdown();
        empty();
    }
    unsetenv("STRIKERS_VITAGL_UNIFORM_CACHE");
    unsetenv("STRIKERS_VITAGL_UNIFORM_VERIFY");
    failUniformConfigure = true;
    {
        Renderer r;
        assert(!r.initialize());
    }
    failUniformConfigure = false;
    empty();
    std::puts("Uniform cache configuration and finite owner counter deltas passed");
    for (const char* flag : {"0", "yes", "1"}) {
        setenv("STRIKERS_VITAGL_VERTEX_PROGRAM_CACHE",flag,1);
        Renderer r;assert(r.initialize());
        assert(bool(vertexStats.enabled)==(std::strcmp(flag,"1")==0));
        const auto getters=vertexSnapshots;
        assert(r.begin_frame() && r.end_frame(false));
        assert(!r.completed_timing().vertexPrograms.valid && vertexSnapshots==getters);
        r.shutdown();empty();
    }
    setenv("STRIKERS_VITAGL_PACKET_PROFILE","1",1);
    {
        Renderer r;assert(r.initialize());
        vertexStats.counters[VGL_VERTEX_PROGRAM_ELEMENTS_REQUESTS]=100;
        vertexStats.cache_bytes=216;vertexStats.cache_peak_bytes=432;
        assert(r.begin_frame());
        vertexStats.counters[VGL_VERTEX_PROGRAM_ELEMENTS_REQUESTS]+=3;
        vertexStats.counters[VGL_VERTEX_PROGRAM_CACHE_HITS]+=2;
        assert(r.end_frame(false));
        auto timing=r.completed_timing().vertexPrograms;
        assert(timing.valid && timing.enabled && timing.counters[VGL_VERTEX_PROGRAM_ELEMENTS_REQUESTS]==3);
        assert(timing.counters[VGL_VERTEX_PROGRAM_CACHE_HITS]==2 && timing.cacheBytes==216 && timing.cachePeakBytes==432);
        assert(r.begin_frame());
        vertexStats.counters[VGL_VERTEX_PROGRAM_ARRAYS_REQUESTS]+=2;
        assert(r.end_frame());
        assert(r.completed_timing().vertexPrograms.counters[VGL_VERTEX_PROGRAM_ARRAYS_REQUESTS]==2);
        assert(r.completed_timing().vertexPrograms.counters[VGL_VERTEX_PROGRAM_ELEMENTS_REQUESTS]==0);
        assert(r.begin_frame());vertexStats.counters[VGL_VERTEX_PROGRAM_ARRAYS_REQUESTS]=0;
        assert(!r.end_frame(false));assert(std::strcmp(r.last_error(),"vertex_program_counter_reset")==0);
        r.shutdown();empty();
    }
    unsetenv("STRIKERS_VITAGL_PACKET_PROFILE");unsetenv("STRIKERS_VITAGL_VERTEX_PROGRAM_CACHE");
    failVertexConfigure=true;
    {
        Renderer r;assert(!r.initialize());assert(std::strcmp(r.last_error(),"vertex_program_config")==0);
    }
    failVertexConfigure=false;empty();
    std::puts("Vertex cache init-only opt-in, normal/discard frame deltas, OFF, reset and failure cleanup passed");
    for (const char* flag : {"0", "yes", "1"}) {
        setenv("STRIKERS_VITAGL_INTERLEAVED_STREAMS",flag,1);
        Renderer r;assert(r.initialize());
        assert(bool(streamStats.enabled)==(std::strcmp(flag,"1")==0));
        const auto getters=streamSnapshots;
        assert(r.begin_frame() && r.end_frame(false));
        assert(!r.completed_timing().vertexStreams.valid && streamSnapshots==getters);
        r.shutdown();empty();
    }
    setenv("STRIKERS_VITAGL_PACKET_PROFILE","1",1);
    {
        Renderer r;assert(r.initialize());
        streamStats.counters[VGL_VERTEX_STREAM_ELEMENTS_REQUESTS]=100;
        assert(r.begin_frame());
        streamStats.counters[VGL_VERTEX_STREAM_ELEMENTS_REQUESTS]+=3;
        streamStats.counters[VGL_VERTEX_STREAM_COLLAPSED_REQUESTS]+=2;
        assert(r.end_frame(false));
        auto timing=r.completed_timing().vertexStreams;
        assert(timing.valid && timing.enabled && timing.counters[VGL_VERTEX_STREAM_ELEMENTS_REQUESTS]==3);
        assert(timing.counters[VGL_VERTEX_STREAM_COLLAPSED_REQUESTS]==2);
        assert(r.begin_frame());
        streamStats.counters[VGL_VERTEX_STREAM_FALLBACK_REQUESTS]+=2;
        assert(r.end_frame());
        assert(r.completed_timing().vertexStreams.counters[VGL_VERTEX_STREAM_FALLBACK_REQUESTS]==2);
        assert(r.completed_timing().vertexStreams.counters[VGL_VERTEX_STREAM_ELEMENTS_REQUESTS]==0);
        assert(r.begin_frame());streamStats.counters[VGL_VERTEX_STREAM_FALLBACK_REQUESTS]=0;
        assert(!r.end_frame(false));assert(std::strcmp(r.last_error(),"vertex_stream_counter_reset")==0);
        r.shutdown();empty();
    }
    unsetenv("STRIKERS_VITAGL_PACKET_PROFILE");unsetenv("STRIKERS_VITAGL_INTERLEAVED_STREAMS");
    failStreamConfigure=true;
    {
        Renderer r;assert(!r.initialize());assert(std::strcmp(r.last_error(),"vertex_stream_config")==0);
    }
    failStreamConfigure=false;empty();
    std::puts("Interleaved stream init-only opt-in, normal/discard owner deltas, OFF, reset and failure cleanup passed");
    assert(!activePresentProfile && !presentProfileSets);
    check_frame_timeline_renderer();
    check_fog_range_renderer();
    check_unexpected_material_masks();
    check_fog_off_renderer();
    check_fog_curve4_renderer();
}
