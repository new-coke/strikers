#include "port/scene_shader_variants.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>

namespace
{
float curve(float f, unsigned type)
{
    f = std::clamp(f, 0.f, 1.f);
    switch (type)
    {
    case 1: return 1.f - std::exp2(-8.f * f);
    case 2: return 1.f - std::exp2(-8.f * f * f);
    case 3: return std::exp2(-8.f * (1.f - f));
    case 4: return std::exp2(-8.f * (1.f - f) * (1.f - f));
    default: return f;
    }
}
float interpolate(const std::array<float, 3>& values, const std::array<float, 3>& weights)
{
    return values[0] * weights[0] + values[1] * weights[1] + values[2] * weights[2];
}
}
#include "fog_off_shader_cases.inc"
#include "fog_curve4_shader_cases.inc"
int main()
{
    using namespace port::render;
    port::nativegxm::ModelVertex vertex;
    assert(scene_projective_uv_channels(vertex) == 0);
    std::array<float, 4> environment{ 0, 0, -1, 0 };
    for (unsigned channel = 0; channel < 6; ++channel)
    {
        auto v = vertex;
        auto& q = channel ? v.extraUv[channel - 1][2] : v.uv[2];
        for (float value : { 0.f, -1.f, 2.f, std::nextafter(1.f, 2.f),
                              std::numeric_limits<float>::quiet_NaN() })
        {
            q = value;
            auto bits = scene_projective_uv_channels(v);
            assert(bits == (1u << channel));
            assert(!scene_affine_uv_eligible(bits, 63, environment));
            assert(scene_affine_uv_eligible(bits, 63 ^ (1u << channel), environment));
        }
    }
    assert(!scene_affine_uv_eligible(63, 63, environment));
    environment = { 1, 1, 0, 0 };
    assert(scene_affine_uv_eligible(49, 49, environment));
    assert(!scene_affine_uv_eligible(63, 63, environment));

    // Generate every production family under all independent option pairs.
    constexpr unsigned masks[]{ 0, 1, 3, 5, 7, 9, 13, 17, 19, 21,
                                25, 33, 35, 37, 39, 41, 43, 49, 53, 57 };
    for (bool fog : { false, true })
        for (bool affine : { false, true })
            for (unsigned mask : masks)
                for (bool alpha : { false, true })
                {
                    const auto source = scene_fragment_shader(mask, alpha, false, affine, fog);
                    assert(source.find("float mask=" + std::to_string(mask) + ".0;") != std::string::npos);
                    assert(source.find("u_controls.w==") == std::string::npos);
                    assert((source.find("discard;") != std::string::npos) == alpha);
                    assert(source.find(affine ? "STRIKERS_AFFINE_UV 1" : "STRIKERS_AFFINE_UV 0") != std::string::npos);
                    assert(source.find(fog ? "STRIKERS_VERTEX_FOG 1" : "STRIKERS_VERTEX_FOG 0") != std::string::npos);
                }
    const auto generic = scene_fragment_shader(63, true, true, false, false);
    assert(generic.find("float mask=u_controls.x;") != std::string::npos);
    assert(generic.find("u_controls.w==2.0") != std::string::npos);
    assert(generic.find("u_controls.w==3.0") != std::string::npos);
    assert(generic.find("u_controls.w==4.0") != std::string::npos);
    assert(scene_vertex_shader(true).find("STRIKERS_VERTEX_FOG 1") != std::string::npos);
    for (bool vertexFog : {false, true})
    {
        const auto vertexSource = scene_vertex_shader(vertexFog);
        const auto previousSource = "#define STRIKERS_SCENE_BONE_ROWS " + std::to_string(kSceneBoneRows) + "\n" +
                                    "#define STRIKERS_VERTEX_FOG " + (vertexFog ? "1\n" : "0\n") +
                                    kSceneVertexShader;
        assert(vertexSource == previousSource && scene_vertex_shader(vertexFog, true) == previousSource);
        assert(vertexSource.find("#define STRIKERS_SCENE_BONE_ROWS " +
                                 std::to_string(kSceneBoneRows) + "\n") != std::string::npos);
        assert(vertexSource.find("u_bones[STRIKERS_SCENE_BONE_ROWS]") != std::string::npos);
        const auto rigid = scene_vertex_shader(vertexFog, false);
        assert(!rigid.empty() && rigid.find("a_bone") == std::string::npos &&
               rigid.find("u_bones") == std::string::npos && rigid.find("int b=") == std::string::npos);
        assert(rigid.find(" float4 p=a_position;float3 n=a_normal;\n") != std::string::npos);
        // Everything after the PN transform remains exactly the shipped
        // vertex program, including lighting, texgen, projection and fog.
        assert(rigid.substr(rigid.find(" float4 eye=")) == vertexSource.substr(vertexSource.find(" float4 eye=")));
        assert(rigid.find(vertexFog ? "STRIKERS_VERTEX_FOG 1" : "STRIKERS_VERTEX_FOG 0") != std::string::npos);
    }
    assert(scene_vertex_fog_eligible({100, 1000, 1, 0}));
    assert(!scene_vertex_fog_eligible({10000, 10000.01f, 1, 0}));
    assert(!scene_vertex_fog_eligible({1000, 500, 1, 0}));
    assert(!scene_vertex_fog_eligible({0, std::numeric_limits<float>::infinity(), 1, 0}));

    // Mirror both stage equations using perspective-correct weights. These
    // CPU tests bound algebra/FP32 error in this domain, not Vita GPU pixels.
    std::mt19937 random(0x53545249);
    std::uniform_real_distribution<float> unit(.001f, 1.f), depth(-2000.f, 12000.f);
    float maximum = 0;
    for (unsigned trial = 0; trial < 100000; ++trial)
    {
        const float start = depth(random);
        const float span = trial % 7 ? 100.f + 5000.f * unit(random) : .0001f;
        const float denominator = std::max(span, .001f);
        std::array<float, 3> z{}, normalized{}, weights{};
        float sum = 0;
        for (unsigned i = 0; i < 3; ++i)
        {
            z[i] = start + denominator * (4.f * unit(random) - 1.5f);
            normalized[i] = (z[i] - start) / denominator;
            // Barycentric / clip W, followed by renormalization.
            weights[i] = unit(random) / (.1f + 100.f * unit(random));
            sum += weights[i];
        }
        for (auto& w : weights)
            w /= sum;
        const float reference = (interpolate(z, weights) - start) / denominator;
        const float candidate = interpolate(normalized, weights);
        // Production retains the reference program for ill-conditioned fog.
        if (!scene_vertex_fog_eligible({start, start + span, 1, 0}))
            continue;
        for (unsigned type = 0; type < 5; ++type)
        {
            const float error = std::fabs(curve(reference, type) - curve(candidate, type));
            maximum = std::max(maximum, error);
            assert(error < 1.f / 255.f);
        }
    }
    // Moving a clamp/curve as well would be a material approximation. This
    // triangle crosses both fog boundaries: the middle sample is .25 vs .5.
    assert(std::fabs(curve(.5f, 0) - (.75f * curve(-1.f, 0) + .25f * curve(5.f, 0))) > .2f);
    std::printf("Shader variants: exact UV admission, all source families; FP32 fog max error %.9g\n", maximum);
    check_fog_off_shader_sources();
    check_fog_curve4_shader_sources();
}
