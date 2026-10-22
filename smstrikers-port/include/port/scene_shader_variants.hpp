#pragma once
#include "port/native_model_vertices.hpp"
#include "port/scene_shaders.hpp"
#include "port/scene_skinning.hpp"
#include <algorithm>
#include <string>

namespace port::render
{
inline bool scene_vertex_fog_eligible(const std::array<float, 4>& fog) noexcept
{
    if (!(fog[2] > .5f))
        return true;
    const float range = fog[1] - fog[0];
    // A narrow range at large coordinates amplifies FP32 cancellation.
    // Retain the reference program in that case, including degenerate ranges.
    return std::isfinite(range) && std::isfinite(fog[0]) && std::isfinite(fog[1]) &&
           range >= 1.f && range >= .01f * std::max(std::fabs(fog[0]), std::fabs(fog[1]));
}

// Cache with immutable geometry; dynamic uploads recompute this from their
// current vertices. Exact equality deliberately rejects almost-unit Q and NaN.
inline unsigned scene_projective_uv_channels(const nativegxm::ModelVertex& v) noexcept
{
    unsigned result = v.uv[2] == 1.f ? 0 : 1;
    for (unsigned i = 0; i < v.extraUv.size(); ++i)
        if (v.extraUv[i][2] != 1.f)
            result |= 2u << i;
    return result;
}

inline bool scene_affine_uv_eligible(unsigned projective, unsigned used,
                                     const std::array<float, 4>& environment) noexcept
{
    // These vertex-generated coordinates always write Q=1. Re-evaluate the
    // overrides per draw: environment state is not immutable model metadata.
    if (environment[0] > .5f)
        projective &= ~16u;
    if (environment[1] > .5f)
        projective &= ~1u;
    if (!(environment[2] < 0.f))
        projective &= ~32u;
    return !(projective & used);
}

inline std::string scene_vertex_shader(bool vertexFog, bool usesBones = true)
{
    std::string source = "#define STRIKERS_SCENE_BONE_ROWS " + std::to_string(kSceneBoneRows) + "\n" +
                         "#define STRIKERS_VERTEX_FOG " + (vertexFog ? "1\n" : "0\n") +
                         kSceneVertexShader;
    if (usesBones)
        return source;
    // Preserve the existing PN source/cache key. Rigid geometry must omit the
    // palette declaration: the provider uploads every reflected vertex uniform.
    const auto replace = [&](const char* from, const char* to) {
        const auto p = source.find(from);
        if (p == std::string::npos)
            return false;
        source.replace(p, std::char_traits<char>::length(from), to);
        return true;
    };
    if (!replace("float a_bone:TEXCOORD6,", "") ||
        !replace("uniform float4 u_bones[STRIKERS_SCENE_BONE_ROWS],", "") ||
        !replace(" int b=(int)a_bone*3;float4 p=a_position;float3 n=a_normal;\n"
                 " if(b>0){p.xyz=float3(dot(u_bones[b],p),dot(u_bones[b+1],p),dot(u_bones[b+2],p));\n"
                 "  n=float3(dot(u_bones[b].xyz,n),dot(u_bones[b+1].xyz,n),dot(u_bones[b+2].xyz,n));}\n",
                 " float4 p=a_position;float3 n=a_normal;\n"))
        return {};
    return source;
}

inline bool scene_specialize_fog_curve4(std::string& source)
{
    constexpr char curves[] =
        "  if(u_fog.w==1.0)f=1.0-exp2(-8.0*f);if(u_fog.w==2.0)f=1.0-exp2(-8.0*f*f);\n"
        "  if(u_fog.w==3.0)f=exp2(-8.0*(1.0-f));if(u_fog.w==4.0)f=exp2(-8.0*(1.0-f)*(1.0-f));";
    const auto first = source.find(curves);
    // A changed/duplicate curve block or another type-dependent operation must
    // fail initialization. Preserve the range kernel and exact type-4 formula.
    if (first == std::string::npos || source.find(curves, first + 1) != std::string::npos ||
        source.find("u_fog.w") < first ||
        source.find("u_fog.w", first + sizeof(curves) - 1) != std::string::npos)
        return false;
    source.replace(first, sizeof(curves) - 1, "  f=exp2(-8.0*(1.0-f)*(1.0-f));");
    return true;
}

inline std::string scene_fragment_shader(unsigned mask, bool alpha, bool generic,
                                          bool affineUv, bool vertexFog, bool fogOff = false,
                                          bool fogCurve4 = false)
{
    std::string source = kSceneFragmentShader;
    const auto replace = [&](const char* from, const std::string& to) {
        const auto p = source.find(from);
        if (p != std::string::npos)
            source.replace(p, std::char_traits<char>::length(from), to);
    };
    if (fogOff)
    {
        if (generic)
            return {};
        // Bind the qualified C17 fog source. A changed/missing/duplicate boundary
        // must fail initialization rather than retain part of the fog program.
        const auto eraseBlock = [&](const char* begin, const char* end) {
            const auto first = source.find(begin);
            if (first == std::string::npos || source.find(begin, first + 1) != std::string::npos)
                return false;
            const auto last = source.find(end, first + std::char_traits<char>::length(begin));
            if (last == std::string::npos || source.find(end) != last ||
                source.find(end, last + 1) != std::string::npos)
                return false;
            source.erase(first, last + std::char_traits<char>::length(end) - first);
            return true;
        };
        const auto replaceOnce = [&](const char* from, const char* to) {
            const auto p = source.find(from);
            if (p == std::string::npos || source.find(from, p + 1) != std::string::npos)
                return false;
            source.replace(p, std::char_traits<char>::length(from), to);
            return true;
        };
        if (!eraseBlock("// One constant-component ladder selects both adjacent coefficients for q in 0..9.\n",
                        "\n}\n") ||
            !eraseBlock(" if(u_fog.z>0.5){\n", "c.rgb=lerp(c.rgb,u_fogcolor.rgb,f);}\n") ||
            !replaceOnce("float eyeZ:TEXCOORD6,", "") ||
            !replaceOnce(" float4 fragmentPosition:WPOS,\n", "") ||
            !replaceOnce(" uniform float4 u_fog,uniform float4 u_fogcolor,uniform float4 u_material,uniform float4 u_alpha,\n"
                         " uniform float4 u_fogrange[3],uniform float4 u_fograngecontrol):COLOR {",
                         " uniform float4 u_material,uniform float4 u_alpha):COLOR {"))
            return {};
        for (const char* dependency : { "scene_fog_", "u_fog", "fragmentPosition", "WPOS", "eyeZ" })
            if (source.find(dependency) != std::string::npos)
                return {};
    }
    if (fogCurve4 && !generic && !fogOff && !scene_specialize_fog_curve4(source))
        return {};
    if (!generic)
    {
        replace("float mask=u_controls.x;", "float mask=" + std::to_string(mask) + ".0;");
        // Keep the standard source and texture-coordinate mapping unchanged.
        replace("u_controls.w==2.0?uv1:uv2", "uv2");
        replace(" if(u_controls.w==2.0){float "
                "y=d.r,u=detail.r-0.5,v=sh.r-0.5;c=float4(y+1.402*v,y-0.344136*u-0.714136*v,y+"
                "1.772*u,1.0);}", "");
        replace(" if(u_controls.w==3.0)c=float4(c.r,c.r,c.r,c.r);", "");
        replace(" if(u_controls.w==4.0)c=float4(d.a,d.a,d.a,d.a);", "");
    }
    if (!alpha)
        replace(" if(u_controls.y>=0.0&&c.a<=u_controls.y)discard;", "");
    return std::string("#define STRIKERS_AFFINE_UV ") + (affineUv ? "1\n" : "0\n") +
           "#define STRIKERS_VERTEX_FOG " + (vertexFog ? "1\n" : "0\n") + source;
}
} // namespace port::render
