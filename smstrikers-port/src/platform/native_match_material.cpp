#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "native_match_internal.hpp"
#include "port/native_match_transform.hpp"
#include "port/scene_texture_sampling.hpp"
#include "port/scene_specular.hpp"
#include "port/scene_material_alpha.hpp"
#include "NL/gl/glState.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glConstant.h"
#include "NL/nlString.h"
#include "NL/gl/glLightUserData.h"
#include "port/aspect.h"
#include <algorithm>
#include <cstring>
extern "C" int port_region_owns(const void*);

namespace port::directmatch::detail
{
namespace
{
struct ProgramIds
{
    u32 movie, pointlitDirt, crowd, crowdLit, pointlit, unlit2x;
};

struct ConstantHashes
{
    unsigned long lightingRange, crowdFrame, ambient;
};

const ConstantHashes& constant_hashes()
{
    static const ConstantHashes hashes{ nlStringHash("lighting/range"), nlStringHash("crowd/frame"),
                                        nlStringHash("lighting/ambient_colour") };
    return hashes;
}

// Nothing sets constants while a frame is sent, so read them once per frame.
struct FrameConstants
{
    s32 frame = -1;
    nlVector4 lightingRange, crowdFrame, ambient;
};

const FrameConstants& frame_constants()
{
    static FrameConstants c;
    const s32 frame = glGetCurrentFrame();
    if (c.frame != frame)
    {
        const auto& h = constant_hashes();
        c.lightingRange = glConstantGetHashed(h.lightingRange);
        c.crowdFrame = glConstantGetHashed(h.crowdFrame);
        c.ambient = glConstantGetHashed(h.ambient);
        c.frame = frame;
    }
    return c;
}

const ProgramIds& program_ids()
{
    // glGetProgram hashes the name only; these IDs do not depend on GL state or
    // resource markers. Keep all per-packet constants and alpha state live.
    static const ProgramIds ids{ glGetProgram("2d movie"), glGetProgram("3d pointlit dirt"),
                                 glGetProgram("3d crowd"), glGetProgram("3d crowd lit"),
                                 glGetProgram("3d pointlit"), glGetProgram("3d unlit 2x") };
    return ids;
}
} // namespace

void MatchRuntime::observe_material_state(unsigned long flags, const glModelPacket& p)
{
    const auto& ids = program_ids();
    const auto program = p.state.program;
    const auto family = program == ids.movie ? render::SceneAlphaProgram::Movie
        : program == ids.pointlitDirt ? render::SceneAlphaProgram::Dirt
        : (program == ids.crowd || program == ids.crowdLit)
            ? render::SceneAlphaProgram::Crowd : render::SceneAlphaProgram::Standard;
    alphaState.observe(flags, p.state.texconfig, family);
    alphaState.note_draw(p.state.texconfig, userdata(p, GLUD_ConstantColour) != nullptr,
                         userdata(p, GLUD_Translucent) || userdata(p, GLUD_NoRasterizedAlpha),
                         userdata(p, GLUD_EnvDiffuse) != nullptr);
}

const void* MatchRuntime::userdata(const glModelPacket& p, eGLUserData type)
{
    if (!p.userData)
        return nullptr;
    auto handle = reinterpret_cast<const uintptr_t*>(p.userData)[type];
    return handle ? glUserGetData(reinterpret_cast<const void*>(handle)) : nullptr;
}

std::array<float, 4> MatchRuntime::vector4(const nlVector4& v) { return { v.x, v.y, v.z, v.w }; }

void MatchRuntime::transform_point(std::array<float, 4>& out, const float* xyz, const nlMatrix4& m,
                                   bool direction)
{
    for (unsigned k = 0; k < 3; ++k)
        out[k] = xyz[0] * m.e[k] + xyz[1] * m.e[4 + k] + xyz[2] * m.e[8 + k] +
                 (direction ? 0.f : m.e[12 + k]);
}

bool MatchRuntime::prepare_material(eGLView view, const glModelPacket& p,
                                    PreparedMaterial& material, render::PacketProfile::Scope* profile)
{
    using Profile = render::PacketProfile;
    auto& m = material.scene;
    auto& raster = material.raster;
    nlMatrix4 v, projection, mv;
    glViewGetViewMatrix(view, v);
    glViewGetProjectionMatrix(view, projection);
    const auto* state = glViewGetState(view);
    const float sx = 960.f / PortLogicalFrameWidth(), sy = 544.f / 448.f;
    m.viewport = { state->vpX * sx, state->vpY * sy, state->vpWidth * sx, state->vpHeight * sy };
    m.scissor = { m.viewport[0], m.viewport[1], m.viewport[0] + m.viewport[2],
                  m.viewport[1] + m.viewport[3] };
    if (const auto* vp = static_cast<const GLViewportUserData*>(userdata(p, GLUD_Viewport)))
    {
        if (profile) profile->material_count(Profile::ViewportOverrides);
        if (vp->view && vp->projection)
        {
            if (profile) profile->material_count(Profile::ViewportMatrixOverrides);
            v = *reinterpret_cast<const nlMatrix4*>(vp->view);
            projection = *reinterpret_cast<const nlMatrix4*>(vp->projection);
        }
        m.viewport = { vp->x * sx, vp->y * sy, vp->w * sx, vp->h * sy };
        m.scissor = { m.viewport[0], m.viewport[1], m.viewport[0] + m.viewport[2],
                      m.viewport[1] + m.viewport[3] };
    }
    if (const auto* sc = static_cast<const uint16_t*>(userdata(p, GLUD_Scissor)))
    {
        m.scissor = { sc[0] * sx, sc[1] * sy, (sc[0] + sc[2]) * sx, (sc[1] + sc[3]) * sy };
    }
    for (unsigned k = 0; k < 4; ++k)
    {
        if (std::fabs(m.scissor[k]) < .001f)
            m.scissor[k] = 0;
        const float full = (k & 1) ? 544.f : 960.f;
        if (std::fabs(m.scissor[k] - full) < .001f)
            m.scissor[k] = full;
    }
    // Match GLX's matrix-handle contract: identity and unresolved file offsets
    // are not matrices to dereference. Resource/frame allocations are registered.
    const bool matrixOwned =
        p.state.matrix && port_region_owns(reinterpret_cast<const void*>(p.state.matrix));
    if (p.state.matrix != glGetIdentityMatrix() && matrixOwned)
    {
        if (profile) profile->material_count(Profile::ModelConcats);
        nlMultMatrices(mv, *reinterpret_cast<const nlMatrix4*>(p.state.matrix), v);
    }
    else
    {
        if (profile) profile->material_count(Profile::ViewOnlyMatrices);
        mv = v;
    }
    std::memcpy(m.modelView.data(), mv.e, 64);
    auto& matrix = material.matrix;
    matrix = compose_glx_mvp(mv.e, projection.e);
    material.matrixOwned = matrixOwned;
    if (!std::all_of(matrix.begin(), matrix.end(), [](float f) { return std::isfinite(f); }))
    {
        if (profile) profile->material_count(Profile::InvalidMatrices);
        failure = "invalid_matrix";
        ++invalid;
        trace_invalid_matrix(view, p, v, projection, mv, matrix, matrixOwned);
        return false;
    }
    if (profile) profile->material_next(Profile::StateLighting);
    m.environment[0] = userdata(p, GLUD_EnvDiffuse) ? 1 : 0;
    m.environment[1] = userdata(p, GLUD_MobileDiffuse) ? 1 : 0;
    // For 0x11 EnvDiffuse rewrites the same stage after the residual cleanup.
    m.material[3] = alphaState.stage1RasterColor && !(p.state.texconfig == 17 && m.environment[0]);
    const float gloss = glGetTextureState(p.state.texturestate, GLTS_GlossLevel) * (1.f / 63.f);
    m.material[0] = render::scene_material_konst(gloss);
    if ((m.environment[0] || userdata(p, GLUD_OnePassFresnel)) && gloss <= .85f)
        m.environment[2] = gloss > .5f ? 2 : gloss > .25f ? 3 : 4;
    m.environment[3] = 2.f * float(glGetCurrentFrame() % 5) / 5.f - 1.f;
    raster.depthTest = glGetRasterState(p.state.raster, GLS_DepthTest);
    raster.depthWrite = glGetRasterState(p.state.raster, GLS_DepthWrite);
    raster.depthFunction = glGetRasterState(p.state.raster, GLS_DepthFunc);
    raster.cull = glGetRasterState(p.state.raster, GLS_Culling);
    raster.blend = glGetRasterState(p.state.raster, GLS_AlphaBlend);
    const unsigned write = glGetRasterState(p.state.raster, GLS_ColourWrite);
    raster.writeRgb = write & 1;
    raster.writeAlpha = (write == 2) || (write == 3 && view == GLV_ShadowTexture);
    m.controls[0] = p.state.texconfig;
    m.controls[1] = glGetRasterState(p.state.raster, GLS_AlphaTest)
                        ? float(glGetRasterState(p.state.raster, GLS_AlphaTestRef)) / 255.f
                        : -1;
    const auto& ids = program_ids();
    const auto program = p.state.program;
    m.lit = program == ids.pointlit || program == ids.pointlitDirt || program == ids.crowdLit;
    m.material[1] = program == ids.pointlitDirt ? 1.f : 0.f;
    // The light-ramp materials scale their diffuse stage by lighting/range.
    // GameRenderTask defaults that range to 2; GLX selected scale 2 unless it was 1.
    m.controls[2] = (p.state.texconfig & 32)
                        ? (frame_constants().lightingRange.x == 1.f ? 1.f : 2.f)
                        : (program == ids.unlit2x ? 2.f : 1.f);
    m.controls[3] =
        program == ids.movie ? 2.f : (program == ids.crowd || program == ids.crowdLit) ? 1.f : 0.f;
    m.effects[3] = m.controls[3] == 1.f ? frame_constants().crowdFrame.x : 0.f;
    m.effects[2] = render::scene_material_konst(
        glGetTextureState(p.state.texturestate, GLTS_DiffuseLevel) * (1.f / 63.f));
    // Standard shadow-map combiners use K1=ShadowLevel and K2=1-ShadowLevel.
    // shadow/pass*_colour belongs to the separate shadow-volume passes.
    const float shadowLevel = glGetTextureState(p.state.texturestate, GLTS_ShadowLevel) * (1.f / 63.f);
    m.shadow.fill(render::scene_material_konst(shadowLevel));
    m.shadowAdd.fill(render::scene_material_konst(1.f - shadowLevel));
    m.alpha = alphaState.resolve(m.effects[2], m.shadow[0]);
    if (const auto* color = static_cast<const uint8_t*>(userdata(p, GLUD_ConstantColour)))
    {
        m.effects[0] = 1;
        for (unsigned i = 0; i < 4; ++i)
            m.tint[i] = float(color[i]) / 255;
    }
    if (const auto* alpha = static_cast<const float*>(userdata(p, GLUD_Translucent)))
    {
        m.effects[1] = std::clamp(float(int(*alpha * 255.5f)), 0.f, 255.f) / 255.f;
        // Userdata handlers run by slot: ConstantColour (7) overwrites the
        // TEV register loaded by Translucent (5), including its alpha.
        if (m.effects[0] > .5f)
            m.effects[1] = m.tint[3];
    }
    if (userdata(p, GLUD_NoRasterizedAlpha))
        m.effects[1] = 1.f;
    if (m.lit)
    {
        m.ambient = vector4(frame_constants().ambient);
        if (const auto* a = static_cast<const float*>(userdata(p, GLUD_Ambient)))
            std::copy_n(a, 4, m.ambient.begin());
        if (const auto* a = static_cast<const float*>(userdata(p, GLUD_Diffuse)))
            std::copy_n(a, 4, m.diffuse.begin());
    }
    if (const auto* data = static_cast<const uint32_t*>(userdata(p, GLUD_Light)))
    {
        const auto* ls = reinterpret_cast<const GLLightUserData*>(data + 1);
        for (unsigned i = 0; i < std::min(*data, 4u); ++i)
        {
            if (profile) profile->material_count(Profile::PointLights);
            transform_point(m.lightPosition[i], &ls[i].worldPosition.x, v, ls[i].outerRadius == 0);
            m.lightPosition[i][3] =
                ls[i].outerRadius ? 1.f / (.666f * .666f * ls[i].outerRadius * ls[i].outerRadius)
                                  : -1.f;
            for (unsigned k = 0; k < 4; ++k)
                m.lightColor[i][k] = std::clamp(ls[i].colour.c[k] * ls[i].intensity, 0.f, 1.f);
        }
    }
    if (const auto* data = static_cast<const uint32_t*>(userdata(p, GLUD_DirectionalLight)))
    {
        const auto* ls = reinterpret_cast<const GLDirectionalLightUserData*>(data + 1);
        for (unsigned i = 0; i < std::min(*data, 4u); ++i)
        {
            if (profile) profile->material_count(Profile::DirectionalLights);
            transform_point(m.lightPosition[i], &ls[i].direction.x, v, true);
            m.lightPosition[i][3] = -1;
            std::copy_n(ls[i].colour.c, 4, m.lightColor[i].begin());
        }
    }
    if (const auto* data = static_cast<const uint32_t*>(userdata(p, GLUD_Specular)))
    {
        const auto* lights = reinterpret_cast<const GLSpecularUserData*>(data + 1);
        for (unsigned i = 0; i < std::min(*data, 4u); ++i)
        {
            if (profile) profile->material_count(Profile::SpecularLights);
            std::array<float, 4> direction{};
            transform_point(direction, &lights[i].worldDirection.x, v, true);
            const auto light = port::render::scene_specular_light(
                direction, lights[i].colour.c, lights[i].intensity, lights[i].exponent);
            m.specular[i] = light.direction;
            m.specularColor[i] = light.color;
        }
    }
    // Slot 7 is not a light: it holds the slot counts for the shader.
    {
        const auto* point = static_cast<const uint32_t*>(userdata(p, GLUD_Light));
        const auto* directional = static_cast<const uint32_t*>(userdata(p, GLUD_DirectionalLight));
        const auto* specular = static_cast<const uint32_t*>(userdata(p, GLUD_Specular));
        const unsigned lights = std::max(point ? std::min(*point, 4u) : 0u,
                                         directional ? std::min(*directional, 4u) : 0u);
        m.lightPosition[7] = { float(lights), float(specular ? std::min(*specular, 4u) : 0u), 0, 0 };
    }
    m.fog = viewFog;
    m.fogColor = viewFogColor;
    if (userdata(p, GLUD_NoFog))
        m.fog[2] = 0;
    if (m.fog[2] > .5f && viewFogRange.enabled)
    {
        if (!viewFogRange.valid)
            return reject_fog_range("fog_range_state");
        int viewportWidth = 0;
        if (!render::rounded_viewport_width(m.viewport[2], viewportWidth) || viewportWidth <= 0)
            return reject_fog_range("fog_range_viewport");
        // All native view targets are 960 wide; center is global, not relative
        // to a packet viewport or scissor. The owner validates its real target.
        if (!render::fog_range_controls(m.fog[0], m.fog[1], viewportWidth, 480.f,
                                         m.fogRangeControl))
            return reject_fog_range("fog_range_material");
        m.fogRangeTable = viewFogRange.table;
    }
    if (profile) profile->material_next(Profile::TextureResolution);
    for (unsigned semantic = 0; semantic < 6; ++semantic)
        if (p.state.texconfig & (1u << semantic))
        {
            if (profile) profile->material_count(Profile::TextureRequests);
            // Valid while neither handle mappings nor the texture table changed.
            struct HandleMemo
            {
                uintptr_t handle;
                uint32_t id;
                int partition;
                unsigned long epoch;
                uint32_t generation;
            };
            static std::array<HandleMemo, 1024> memo{};
            const uintptr_t handle = p.state.texture[semantic];
            auto& entry = memo[(handle >> 4 ^ handle >> 14) & 1023u];
            int shadowPartition = -1;
            if (entry.handle == handle && entry.id && entry.epoch == glx_TextureHandleEpoch() &&
                entry.generation == textureGeneration)
            {
                m.textures[semantic] = entry.id;
                shadowPartition = entry.partition;
            }
            else
            {
                auto* pTex = glx_GetMaterialTex(handle);
                m.textures[semantic] = texture_for(pTex, &shadowPartition);
                if (m.textures[semantic] && !glx_IsAnimatedTexture(handle))
                    entry = { handle, m.textures[semantic], shadowPartition, glx_TextureHandleEpoch(),
                              textureGeneration };
            }
            if (semantic != 0)
                shadowPartition = -1;
            if (!m.textures[semantic])
            {
                if (profile) profile->material_count(Profile::TextureFailures);
                failure = "material_texture";
                ++invalid;
                return false;
            }
            if (profile) profile->material_count(Profile::TextureResolutions);
            if (shadowPartition >= 0)
                m.material[2] = float(shadowPartition + 1);
            const unsigned wrap =
                glGetTextureState(p.state.texturestate, eGLTextureState(semantic));
            const auto sampling = port::render::scene_texture_sampling(
                semantic, wrap,
                glGetTextureState(p.state.texturestate, eGLTextureState(semantic + 6)) ==
                    GLTF_Linear);
            m.wrapU[semantic] = sampling.wrapU;
            m.wrapV[semantic] = sampling.wrapV;
            m.filter[semantic] = sampling.linear;
        }

    return true;
}
}   // namespace port::directmatch::detail
#endif
