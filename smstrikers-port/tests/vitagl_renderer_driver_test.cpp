// Execute the actual application renderer against the VitaGL public API.
// These are host ownership/state/error tests; shaders and pixels need a Vita.
#include "port/renderer.hpp"
#include "port/scene_texture_sampling.hpp"
#include "port/scene_specular.hpp"
#include <vitaGL.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <cassert>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <limits>
namespace
{
GLuint next = 1, array = 0, elements = 0, program = 0, drawFb = 0, readFb = 0, texture = 0;
unsigned swaps = 0, blits = 0, scissors = 0, uniformWrites = 0;
unsigned framebufferBinds = 0, clears = 0;
unsigned attributeEnables = 0, attributePointers = 0;
std::array<GLuint, 10> attributeBuffers{};
GLenum blendSource = 0, blendDestination = 0, frontFace = 0, cullFace = 0;
GLenum error = GL_NO_ERROR, readMode = GL_BACK;
SceUID thread = 1;
bool failCompile = false, failTarget = false, failInit = false;
std::map<GLuint, std::vector<uint8_t>> buffers;
std::map<GLuint, std::map<GLenum, GLint>> textureParameters;
std::set<GLuint> textures, framebuffers, renderbuffers, programs, shaders;
const char* names[]{ "u_mvp",           "u_mv",       "u_bones",    "u_environment", "u_ambient",
                     "u_diffuse",       "u_lighting", "u_lightpos", "u_lightcolor",  "u_specular",
                     "u_specularcolor", "u_controls", "u_effects",  "u_tint",        "u_shadow",
                     "u_shadowadd",     "u_fog",      "u_fogcolor", "u_material" };
unsigned capacities[]{ 4, 4, 30, 1, 1, 1, 1, 8, 8, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1 };
std::array<int, 4> viewport{}, scissor{};
std::array<bool, 4> colorMask{};
std::set<GLenum> enabled;
std::vector<std::vector<uint8_t>> submitted;
std::vector<float> bones;
std::vector<float> specularDirections, specularColors, material;
std::vector<float> controls;
std::map<GLuint,std::vector<float>> lightingByProgram;
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
}   // namespace
extern "C" {
SceUID sceKernelGetThreadId() { return thread; }
SceInt64 sceKernelGetSystemTimeWide()
{
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
GLboolean vglInitWithCustomSizes(int, int w, int h, int ram, int cdram, int, int,
                                 SceGxmMultisampleMode)
{
    assert(w == 960 && h == 544 && ram > 0 && cdram > 0);
    viewport = failInit ? std::array<int, 4>{} : std::array<int, 4>{ 0, 0, w, h };
    error = failInit ? GL_INVALID_OPERATION : GL_NO_ERROR;
    return GL_FALSE;   // Native-size initialization succeeds with false.
}
void glGetIntegerv(GLenum key, GLint* data)
{
    assert(key == GL_VIEWPORT);
    std::copy(viewport.begin(), viewport.end(), data);
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
    auto id = t == GL_ARRAY_BUFFER ? array : elements;
    assert(size > 0 && buffers.count(id));
    auto& v = buffers.at(id);
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
void glDeleteTextures(GLsizei n, const GLuint* ids) { erase(n, ids, textures); }
void glActiveTexture(GLenum) {}
void glBindTexture(GLenum, GLuint id)
{
    assert(!id || textures.count(id));
    texture = id;
}
void glTexImage2D(GLenum, GLint, GLint, GLsizei w, GLsizei h, GLint, GLenum, GLenum, const GLvoid*)
{
    assert(textures.count(texture) && w > 0 && h > 0);
}
void glTexParameteri(GLenum target, GLenum name, GLint value)
{
    assert(target == GL_TEXTURE_2D && textures.count(texture));
    textureParameters[texture][name] = value;
}
void glGenFramebuffers(GLsizei n, GLuint* ids) { generate(n, ids, framebuffers); }
void glDeleteFramebuffers(GLsizei n, const GLuint* ids) { erase(n, ids, framebuffers); }
void glBindFramebuffer(GLenum target, GLuint id)
{
    ++framebufferBinds;
    assert(!id || framebuffers.count(id));
    if (target != GL_READ_FRAMEBUFFER)
        drawFb = id;
    if (target != GL_DRAW_FRAMEBUFFER)
        readFb = id;
}
void glFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint id, GLint)
{
    assert(framebuffers.count(drawFb) && textures.count(id));
}
void glGenRenderbuffers(GLsizei n, GLuint* ids) { generate(n, ids, renderbuffers); }
void glDeleteRenderbuffers(GLsizei n, const GLuint* ids) { erase(n, ids, renderbuffers); }
void glBindRenderbuffer(GLenum, GLuint id) { assert(renderbuffers.count(id)); }
void glRenderbufferStorage(GLenum, GLenum fmt, GLsizei, GLsizei)
{
    assert(fmt == GL_DEPTH24_STENCIL8);
}
void glFramebufferRenderbuffer(GLenum, GLenum attachment, GLenum, GLuint id)
{
    assert(attachment == GL_DEPTH_STENCIL_ATTACHMENT && renderbuffers.count(id));
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
void glDepthMask(GLboolean) {}
void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat) {}
void glClearDepthf(GLfloat) {}
void glClear(GLbitfield) { ++clears; }
GLuint glCreateShader(GLenum)
{
    GLuint id;
    generate(1, &id, shaders);
    return id;
}
void glDeleteShader(GLuint id) { erase(1, &id, shaders); }
void glShaderSource(GLuint id, GLsizei, const GLchar* const*, const GLint*)
{
    assert(shaders.count(id));
}
void glCompileShader(GLuint) {}
void glGetShaderiv(GLuint, GLenum key, GLint* v)
{
    *v = key == GL_COMPILE_STATUS ? !failCompile : 1;
}
void glGetShaderInfoLog(GLuint, GLsizei, GLsizei*, GLchar* log) { *log = 0; }
GLuint glCreateProgram()
{
    GLuint id;
    generate(1, &id, programs);
    return id;
}
void glDeleteProgram(GLuint id) { erase(1, &id, programs); }
void glAttachShader(GLuint p, GLuint s) { assert(programs.count(p) && shaders.count(s)); }
void glBindAttribLocation(GLuint, GLuint, const GLchar*) {}
void glLinkProgram(GLuint) {}
void glGetProgramiv(GLuint, GLenum p, GLint* v) { *v = p == GL_ACTIVE_UNIFORMS ? std::size(names) : 1; }
void glUseProgram(GLuint id)
{
    assert(!id || programs.count(id));
    program = id;
}
GLint glGetUniformLocation(GLuint, const GLchar* name)
{
    for (unsigned i = 0; i < std::size(names); ++i)
        if (std::string(name) == names[i])
            return -int(i + 2); // VitaGL non-strict locations are opaque signed handles.
    if (std::string(name).starts_with("u_tex"))
        return -100 - (name[5] - '0');
    return -1;
}
void glGetActiveUniform(GLuint, GLuint i, GLsizei n, GLsizei*, GLint* size, GLenum* type,
                        GLchar* name)
{
    assert(i < std::size(names));
    *size = capacities[i];
    *type = GL_FLOAT_VEC4;
    std::strncpy(name, names[i], n);
}
void glUniform1i(GLint location, GLint unit) { assert(location == -100-unit); }
void glUniform4fv(GLint location, GLsizei count, const GLfloat* values)
{
    assert(location <= -2 && location >= -int(std::size(names)+1) && count > 0 &&
           unsigned(count) <= capacities[-location - 2]);
    ++uniformWrites;
    if (location == -4)
        bones.assign(values, values + count * 4);
    if (location == -11)
        specularDirections.assign(values, values + count * 4);
    if (location == -12)
        specularColors.assign(values, values + count * 4);
    if (location == -20)
        material.assign(values, values + count * 4);
    if (location == -13)
        controls.assign(values, values + count * 4);
    if (location == -8)
        lightingByProgram[program].assign(values, values + count * 4);
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
    assert(programs.count(program) && framebuffers.count(drawFb) && primitive == GL_TRIANGLES &&
           count % 3 == 0 && type == GL_UNSIGNED_SHORT && !offset);
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
}
void glBlitFramebuffer(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)
{
    assert(framebuffers.count(readFb));
    ++blits;
}
void vglSwapBuffers(GLboolean)
{
    assert(drawFb == 0);
    ++swaps;
}
void glReadBuffer(GLenum mode) { readMode = mode; }
void glReadPixels(GLint, GLint, GLsizei w, GLsizei h, GLenum, GLenum, GLvoid* dst)
{
    assert(readFb == 0 && readMode == GL_FRONT);
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
int main()
{
    using namespace port::render;
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
        assert(r.draw_scene(model, {}, {}, identity, raster, m));
        assert(specularDirections.size() == 16 && specularColors.size() == 16);
        assert(specularDirections[15] == 64 && specularColors[12] == light.color[0]);
        assert(material.size() == 4 && material[0] == 13.f / 63.f);
        writes = uniformWrites;
        assert(r.draw_scene(model, {}, {}, identity, raster, m) && uniformWrites == writes);
        m.specularColor[3][0] = .9f;
        assert(r.draw_scene(model, {}, {}, identity, raster, m) && uniformWrites == writes + 1);
        assert(specularColors[12] == .9f);
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
        m.bones[29][3] = 1234;
        assert(r.draw_scene(model, {}, {}, identity, raster, m) && bones.size() == 120 &&
               bones[119] == 1234);
        assert(r.end_frame() && !r.frame_active() && swaps == 1 && blits >= 1);
        std::vector<uint8_t> pixels;
        assert(r.readback_presented_rgba8(pixels) && pixels.size() == 960 * 544 * 4);
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
}
