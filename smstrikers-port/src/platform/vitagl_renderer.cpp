#if defined(PORT_VITA) && defined(STRIKERS_VITA_VITAGL)
#include "port/renderer.hpp"
#include "port/scene_shaders.hpp"
#include "port/vitagl_texture.hpp"
#include "port/native_boot_screen.hpp"
#include <vitaGL.h>
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
constexpr const char* attribs[]{ "a_position", "a_color", "a_normal", "a_uv0", "a_uv1",
                                 "a_uv2",      "a_uv3",   "a_uv4",    "a_uv5", "a_bone" };
constexpr const char* uniforms[]{ "u_mvp",        "u_mv",       "u_bones",         "u_environment",
                                  "u_ambient",    "u_diffuse",  "u_lighting",      "u_lightpos",
                                  "u_lightcolor", "u_specular", "u_specularcolor", "u_controls",
                                  "u_effects",    "u_tint",     "u_shadow",        "u_shadowadd",
                                  "u_fog",        "u_fogcolor", "u_material" };
constexpr unsigned counts[]{ 4, 4, 30, 1, 1, 1, 1, 8, 8, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1 };
void replace(std::string& s, const char* a, const std::string& b)
{
    auto p = s.find(a);
    if (p != std::string::npos)
        s.replace(p, std::strlen(a), b);
}
bool valid_geometry(std::span<const ModelVertex> v, std::span<const uint16_t> i)
{
    if (v.empty() || v.size() > 65536 || i.empty() || i.size() % 3 || i.size() > 196608)
        return false;
    for (auto n : i)
        if (n >= v.size())
            return false;
    return true;
}
struct Uniform
{
    GLint location = -1;
    unsigned vectors = 0;
    std::array<float, 120> previous{};
    bool valid = false;
};
struct Program
{
    GLuint id = 0;
    unsigned mask = 63;
    bool generic = false;
    std::array<Uniform, std::size(counts)> uniforms{};
};
struct Texture
{
    GLuint id = 0, fbo = 0, depth = 0;
    unsigned width = 0, height = 0, levels = 1;
    uint8_t minAlpha = 255;
    int wrapU = -1, wrapV = -1, filter = -1;
};
struct Model
{
    GLuint vbo = 0, ebo = 0;
    size_t bytes = 0;
    unsigned count = 0;
    uint8_t minAlpha = 255;
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
    bool initialized = false, sceneInitialized = false, active = false, presented = false,
         timed = false, startupRetired = false;
    SceUID owner = -1;
    unsigned cullTest = 0;
    int error = 0;
    const char* operation = "none";
    FrameTiming current{}, completed{};
    std::unordered_map<unsigned, Texture> textures;
    std::unordered_map<unsigned, Model> models;
    std::unordered_map<unsigned, Program> programs;
    GLuint vertexShader = 0, dynamicVbo = 0, dynamicEbo = 0;
    unsigned main = 0, target = 0, white = 0, nextModel = 1;
    size_t geometryBytes = 0;
    GLuint boundProgram = 0;
    GLuint attributeBuffer = 0;
    bool attributesEnabled = false;
    uint32_t fixedSignature = UINT32_MAX;
    bool viewportValid = false, scissorValid = false;
    std::array<int, 4> lastViewport{}, lastScissor{};
    std::array<GLuint, 6> boundTextures{};
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
    bool on_owner() { return owner == sceKernelGetThreadId() || fail("owner_thread"); }
    unsigned target_id() const { return target ? target : main; }
    Texture* surface()
    {
        auto it = textures.find(target_id());
        return it == textures.end() ? nullptr : &it->second;
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
    bool make_program(unsigned key, unsigned mask, bool alpha, bool generic)
    {
        std::string source = kSceneFragmentShader;
        if (!generic)
        {
            replace(source, "float mask=u_controls.x;",
                    "float mask=" + std::to_string(mask) + ".0;");
            replace(source,
                    " if(u_controls.w==2.0){float "
                    "y=d.r,u=detail.r-0.5,v=sh.r-0.5;c=float4(y+1.402*v,y-0.344136*u-0.714136*v,y+"
                    "1.772*u,1.0);}",
                    "");
            replace(source, " if(u_controls.w==3.0)c=float4(c.r,c.r,c.r,c.r);", "");
            replace(source, " if(u_controls.w==4.0)c=float4(d.a,d.a,d.a,d.a);", "");
        }
        if (!alpha)
            replace(source, " if(u_controls.y>=0.0&&c.a<=u_controls.y)discard;", "");
        GLuint fs = compile(GL_CG_FRAGMENT_SHADER_EXT, source);
        if (!fs)
            return false;
        Program p;
        p.mask = mask;
        p.generic = generic;
        p.id = glCreateProgram();
        glAttachShader(p.id, vertexShader);
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
                continue;
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
    void uniform(Program& p, unsigned slot, const float* value)
    {
        auto& u = p.uniforms[slot];
        if (u.location == -1)
            return;
        const auto n = u.vectors * 4 * sizeof(float);
        if (!u.valid || std::memcmp(u.previous.data(), value, n))
        {
            glUniform4fv(u.location, u.vectors, value);
            std::memcpy(u.previous.data(), value, n);
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
                                   (unsigned(r.writeRgb) << 10) | (unsigned(r.writeAlpha) << 11);
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
        if (r.cull && cullTest != 1)
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
    port::startup::show("PREPARING GRAPHICS");
    d.timed = timed;
    const char* cull = std::getenv("STRIKERS_VITAGL_CULL_TEST");
    d.cullTest = cull ? unsigned(std::clamp(std::atoi(cull), 0, 2)) : 0;
    if (d.cullTest)
        std::fprintf(stderr, "[vitagl] diagnostic cull mode=%u\n", d.cullTest);
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
    d.initialized = true;
    vglWaitVblankStart(GL_TRUE);
    // VitaGL uses tightly packed RGBA uploads and does not implement the
    // desktop PACK/UNPACK_ALIGNMENT settings. RGB capture rows are 960*3 bytes.
    glGenBuffers(1, &d.dynamicVbo);
    glGenBuffers(1, &d.dynamicEbo);
    d.main = create_target(W, H);
    if (!d.main)
        return false;
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
    port::startup::show("PREPARING SHADERS", 0, 41);
    d.vertexShader = d.compile(GL_CG_VERTEX_SHADER_EXT, kSceneVertexShader);
    if (!d.vertexShader)
        return false;
    if (!d.make_program(128, 63, true, true))
        return false;
    unsigned prepared = 1;
    port::startup::show("PREPARING SHADERS", prepared, 41);
    constexpr unsigned masks[]{ 0,  1,  3,  5,  7,  9,  13, 17, 19, 21,
                                25, 33, 35, 37, 39, 41, 43, 49, 53, 57 };
    for (auto mask : masks)
        for (unsigned alpha = 0; alpha < 2; ++alpha)
        {
            if (!d.make_program(mask * 2 + alpha, mask, alpha, false))
                return false;
            port::startup::show("PREPARING SHADERS", ++prepared, 41);
        }
    port::startup::show("STARTING GAME");
    d.sceneInitialized = true;
    glUseProgram(0);
    d.boundProgram = 0;
    std::fprintf(stderr, "[vitagl] scene programs=%u; Aurora=0 GX=0\n",
                 unsigned(d.programs.size()));
    return true;
}
uint32_t Renderer::create_texture(const TextureUpload& image) noexcept
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
        size_t linearOffset = 0;
        for (unsigned level = 0; level < image.mipCount; ++level)
        {
            TextureLevel decoded;
            const unsigned width = std::max(1u, image.width >> level),
                           height = std::max(1u, image.height >> level);
            const bool linear =
                image.storage == nativegxm::TextureStorage::Rgba8 && !image.swizzled;
            const unsigned stride = (width + 7u) & ~7u;
            const uint8_t* pixels = nullptr;
            if (linear && stride == width)
                pixels = image.pixels.data() + linearOffset;
            else if (texture_level_rgba(image, level, decoded))
                pixels = decoded.rgba.data();
            else
            {
                d.fail("texture_decode");
                return 0;
            }
            for (size_t i = 3; i < size_t(width) * height * 4; i += 4)
                t.minAlpha = std::min(t.minAlpha, pixels[i]);
            glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         pixels);
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
        d.textures.emplace(t.id, t);
        lease.committed = true;
        d.current.uploadUs += d.now() - start;
        return t.id;
    }
    catch (...)
    {
        impl_->fail("texture_allocation", GL_OUT_OF_MEMORY);
        return 0;
    }
}
uint32_t Renderer::create_target(uint32_t width, uint32_t height) noexcept
{
    try
    {
        auto& d = *impl_;
        if (!d.initialized || !d.on_owner() || !width || !height || width > W || height > H)
            return 0;
        Texture t;
        TextureLease lease{ t };
        t.width = width;
        t.height = height;
        t.minAlpha = 0;
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &t.id);
        glBindTexture(GL_TEXTURE_2D, t.id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenFramebuffers(1, &t.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.id, 0);
        glGenRenderbuffers(1, &t.depth);
        glBindRenderbuffer(GL_RENDERBUFFER, t.depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                  t.depth);
        bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glDisable(GL_SCISSOR_TEST);
        glColorMask(1, 1, 1, 1);
        glDepthMask(1);
        glClearColor(0, 0, 0, 0);
        glClearDepthf(1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
    try
    {
        auto& d = *impl_;
        if (!valid_geometry(vertices, indices) || !d.on_owner())
            return 0;
        const size_t size = vertices.size_bytes() + indices.size_bytes();
        if (size > 48 * MB - d.geometryBytes)
            return 0;
        const auto start = d.now();
        Model m;
        ModelLease lease{ m };
        m.bytes = size;
        m.count = indices.size();
        for (const auto& v : vertices)
            m.minAlpha = std::min(m.minAlpha, v.color[3]);
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
        const unsigned id = d.nextModel++;
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
        return;
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
    if (r.cull == 3)
        return true;
    const auto start = d.now();
    unsigned count = 0;
    GLuint vertexBuffer = d.dynamicVbo;
    uint8_t alpha = 255;
    if (model)
    {
        auto i = d.models.find(model);
        if (i == d.models.end())
            return d.fail("draw_model");
        count = i->second.count;
        alpha = i->second.minAlpha;
        vertexBuffer = i->second.vbo;
        glBindBuffer(GL_ARRAY_BUFFER, i->second.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, i->second.ebo);
    }
    else
    {
        if (!valid_geometry(vertices, indices))
            return d.fail("draw_geometry");
        count = indices.size();
        for (auto& v : vertices)
            alpha = std::min(alpha, v.color[3]);
        glBindBuffer(GL_ARRAY_BUFFER, d.dynamicVbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size_bytes(), vertices.data(), GL_STREAM_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d.dynamicEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size_bytes(), indices.data(), GL_STREAM_DRAW);
        d.current.dynamicVertexBytes += vertices.size_bytes();
    }
    d.attributes(vertexBuffer);
    const unsigned mask = unsigned(m.controls[0]);
    // GLUD_ConstantColour replaces both TEV colour and alpha for diffuse-only
    // draws. BallBlurCB uses that alpha for its five translucent rotations.
    // Explicit translucent/no-rasterized-alpha data has priority; the lit
    // 0x21 combiner replaces RGB only and retains its rasterized alpha.
    const float rasterAlpha =
        m.effects[1] >= 0 ? m.effects[1] : (m.effects[0] > .5f && mask == 1 ? m.tint[3] : -1.f);
    bool test = m.controls[1] >= 0;
    float minimum = float(alpha) / 255;
    if (rasterAlpha >= 0)
        minimum = rasterAlpha;
    if (m.environment[2] > 0)
        minimum = 0;
    const auto texalpha = [&](unsigned slot) {
        auto it = d.textures.find(m.textures[slot]);
        return it == d.textures.end() ? 1.f : float(it->second.minAlpha) / 255;
    };
    if (mask & 2)
        minimum *= std::min(texalpha(0), texalpha(1));
    else if (mask & 1)
        minimum *= texalpha(0);
    if (minimum > m.controls[1])
        test = false;
    auto it = d.programs.find(mask * 2 + unsigned(test));
    if (m.controls[3] >= 2 || it == d.programs.end())
        it = d.programs.find(128);
    if (it == d.programs.end())
        return d.fail("draw_program");
    auto& p = it->second;
    if (d.boundProgram != p.id)
    {
        glUseProgram(p.id);
        d.boundProgram = p.id;
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
                           m.material.data() };
    for (unsigned u = 0; u < std::size(uniforms); ++u)
        d.uniform(p, u, values[u]);
    for (unsigned a = 0; a < 6; ++a)
    {
        if (!(p.mask & (1u << a)) && !p.generic)
            continue;
        unsigned id = m.textures[a] ? m.textures[a] : d.white;
        auto t = d.textures.find(id);
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
                glBindTexture(GL_TEXTURE_2D, id);
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
    auto* surface = d.surface();
    if (!surface)
        return d.fail("draw_target");
    const int h = surface->height;
    std::array<int, 4> viewport{ int(std::lround(m.viewport[0])),
                                 h - int(std::lround(m.viewport[1] + m.viewport[3])),
                                 int(std::lround(m.viewport[2])), int(std::lround(m.viewport[3])) };
    if (!d.viewportValid || viewport != d.lastViewport)
    {
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        d.lastViewport = viewport;
        d.viewportValid = true;
    }
    // Original scissor edges are top-left physical pixel coordinates.
    const int l = std::max(0, int(std::ceil(m.scissor[0]))),
              t = std::max(0, int(std::ceil(m.scissor[1]))),
              right = std::min(int(surface->width), int(std::ceil(m.scissor[2]))),
              bottom = std::min(h, int(std::ceil(m.scissor[3])));
    std::array<int, 4> scissor{ l, h - bottom, std::max(0, right - l), std::max(0, bottom - t) };
    if (!d.scissorValid || scissor != d.lastScissor)
    {
        if (l == 0 && t == 0 && right == int(surface->width) && bottom == h)
            glDisable(GL_SCISSOR_TEST);
        else
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        }
        d.lastScissor = scissor;
        d.scissorValid = true;
    }
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, nullptr);
    ++d.current.draws;
    d.current.drawUs += d.now() - start;
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
    if (!d.active)
        return d.fail("copy_lifecycle");
    auto* source = d.surface();
    if (!source)
        return false;
    const auto src = *source;
    const auto previous = d.target;
    d.invalidate_fixed();
    for (const auto& c : copies)
    {
        auto t = d.textures.find(c.destination);
        if (t == d.textures.end() || !t->second.fbo || c.destination == src.id)
            return d.fail("copy_target");
        if (c.intensity || c.alphaOnly)
        {
            if (d.target != c.destination)
            {
                d.invalidate_fixed();
                d.target = c.destination;
                glBindFramebuffer(GL_FRAMEBUFFER, t->second.fbo);
            }
            if (!c.preserveDestination)
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
            float l = c.sourceRect[0] / src.width,
                  r = (c.sourceRect[0] + c.sourceRect[2]) / src.width,
                  top = 1.f - (c.sourceRect[1] + c.sourceRect[3]) / src.height,
                  b = 1.f - c.sourceRect[1] / src.height;
            v[0].uv = { l, top, 1 };
            v[1].uv = { r, top, 1 };
            v[2].uv = { r, b, 1 };
            v[3].uv = { l, b, 1 };
            uint16_t indices[]{ 0, 1, 2, 0, 2, 3 };
            SceneMaterial m;
            m.textures[0] = src.id;
            m.controls = { 1, -1, 1, c.alphaOnly ? 4.f : 3.f };
            m.wrapU[0] = m.wrapV[0] = 1;
            // Engine textures use V=0 for their first/top source row. Store
            // that row at GL texture V=0, with unused A8 rows above it.
            const bool regional = c.destinationRect[2] && c.destinationRect[3];
            const auto height = regional              ? c.destinationRect[3]
                                : c.destinationHeight ? c.destinationHeight
                                                      : t->second.height;
            const auto width = regional ? c.destinationRect[2] : t->second.width;
            const auto x = regional ? c.destinationRect[0] : 0u;
            const auto y = regional ? c.destinationRect[1] : 0u;
            if (x > t->second.width || width > t->second.width - x || y > t->second.height ||
                height > t->second.height - y)
                return d.fail("copy_extent");
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
        else
        {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, src.fbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, t->second.fbo);
            glDisable(GL_SCISSOR_TEST);
            glColorMask(1, 1, 1, 1);
            const auto& a = c.sourceRect;
            glBlitFramebuffer(std::lround(a[0]), src.height - std::lround(a[1] + a[3]),
                              std::lround(a[0] + a[2]), src.height - std::lround(a[1]), 0, 0,
                              t->second.width,
                              c.destinationHeight ? c.destinationHeight : t->second.height,
                              GL_COLOR_BUFFER_BIT, GL_LINEAR);
        }
    }
    d.invalidate_fixed();
    d.target = previous;
    glBindFramebuffer(GL_FRAMEBUFFER, d.textures.at(d.target_id()).fbo);
    return d.check("copy_texture");
}
bool Renderer::begin_frame() noexcept
{
    auto& d = *impl_;
    if (!d.initialized || d.active || !d.on_owner())
        return d.fail("frame_begin");
    d.current = {};
    d.attributeBuffer = 0;
    d.attributesEnabled = false;
    d.active = true;
    return select_target(0, true, true);
}
bool Renderer::end_frame(bool present) noexcept
{
    auto& d = *impl_;
    if (!d.active || !d.on_owner())
        return d.fail("frame_end");
    if (!present)
    {
        // glSendFrame aborted before submitting the incomplete render list.
        // A swap here would replace the loading screen with the cleared FBO.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        d.completed = d.current;
        d.active = false;
        return d.check("frame_discard");
    }
    const auto start = d.now();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, d.textures.at(d.main).fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glBlitFramebuffer(0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!d.check("present_blit"))
        return false;
    vglSwapBuffers(GL_FALSE);
    if (!d.startupRetired)
        d.startupRetired = port::startup::retire_if_replaced();
    d.current.queueUs += d.now() - start;
    d.completed = d.current;
    d.active = false;
    d.presented = true;
    return d.check("present");
}
bool Renderer::frame_active() const noexcept { return impl_ && impl_->active; }
bool Renderer::readback_presented_rgba8(std::vector<uint8_t>& out) noexcept
{
    try
    {
        auto& d = *impl_;
        if (d.active || !d.presented || !d.on_owner())
            return false;
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
    while (!d.models.empty())
        release_model(d.models.begin()->first);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    while (!d.textures.empty())
        release_texture(d.textures.begin()->first);
    for (auto& [_, p] : d.programs)
        glDeleteProgram(p.id);
    d.programs.clear();
    if (d.vertexShader)
        glDeleteShader(d.vertexShader);
    glDeleteBuffers(1, &d.dynamicVbo);
    glDeleteBuffers(1, &d.dynamicEbo);
    d.initialized = d.active = d.presented = false;
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
