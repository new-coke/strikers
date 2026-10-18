#pragma once
#include "port/native_model_vertices.hpp"
#include "port/scene_shaders.hpp"
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

inline std::string scene_vertex_shader(bool vertexFog)
{
    return std::string("#define STRIKERS_VERTEX_FOG ") + (vertexFog ? "1\n" : "0\n") +
           kSceneVertexShader;
}

inline std::string scene_fragment_shader(unsigned mask, bool alpha, bool generic,
                                          bool affineUv, bool vertexFog)
{
    std::string source = kSceneFragmentShader;
    const auto replace = [&](const char* from, const std::string& to) {
        const auto p = source.find(from);
        if (p != std::string::npos)
            source.replace(p, std::char_traits<char>::length(from), to);
    };
    if (!generic)
    {
        replace("float mask=u_controls.x;", "float mask=" + std::to_string(mask) + ".0;");
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
