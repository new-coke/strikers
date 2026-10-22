#include "port/scene_shader_variants.hpp"
namespace
{
struct Curve4Config
{
    const char* request;
    bool affine, vertexFog, off, effective;
};
std::map<FogProgramSignature, GLuint> curve4_program_inventory(const Curve4Config& config)
{
    using namespace port::render;
    std::map<FogProgramSignature, GLuint> result;
    for (GLuint id : programs)
    {
        const auto& stages = programSources.at(id);
        const auto& fs = stages.at(GL_CG_FRAGMENT_SHADER_EXT);
        const bool generic = fs.find("float mask=u_controls.x;") != std::string::npos;
        const bool off = program_fog_off(id), pn = program_uses_bones(id);
        const bool alpha = fs.find("discard;") != std::string::npos;
        const bool affine = fs.find("STRIKERS_AFFINE_UV 1") != std::string::npos;
        unsigned mask = 63, matches = 0;
        if (!generic)
        {
            for (unsigned candidate : fogTestMasks)
                if (fs.find("float mask=" + std::to_string(candidate) + ".0;") != std::string::npos)
                { mask = candidate; ++matches; }
            assert(matches == 1);
        }
        const bool vertexFog = config.vertexFog && !generic && !off;
        assert(stages.at(GL_CG_VERTEX_SHADER_EXT) == scene_vertex_shader(vertexFog, pn));
        const auto reference = scene_fragment_shader(mask, alpha, generic, affine, vertexFog, off, false);
        if (config.effective && !generic && !off)
        {
            assert(fs != reference && fs.find("u_fog.w") == std::string::npos);
            assert(fs.find("f=exp2(-8.0*(1.0-f)*(1.0-f));") != std::string::npos);
        }
        else
            assert(fs == reference); // Every ineffective, generic and OFF source is unchanged.
        assert(result.emplace(FogProgramSignature{mask, alpha, generic, affine, pn, off}, id).second);
    }
    std::set<FogProgramSignature> expected;
    for (unsigned pn : {0u, 1u})
    {
        expected.insert({63, 1, 1, 0, pn, 0});
        for (unsigned mask : fogTestMasks)
            for (unsigned alpha : {0u, 1u})
                for (unsigned off = 0; off < (config.off ? 2u : 1u); ++off)
                    for (unsigned affine = 0; affine < (config.affine ? 2u : 1u); ++affine)
                        expected.insert({mask, alpha, 0, affine, pn, off});
    }
    assert(result.size() == expected.size());
    for (const auto& key : expected) assert(result.count(key) == 1);
    return result;
}
void check_fog_curve4_renderer()
{
    using namespace port::render;
    const Curve4Config configs[]{
        {nullptr, false, false, false, false}, {"0", false, false, false, false},
        {"yes", false, false, false, false}, {"1", false, false, false, true},
        {"1", false, false, true, true}, {"1", true, false, false, false},
        {"1", true, false, true, false}, {"1", false, true, false, false},
        {"1", false, true, true, false}, {"1", true, true, false, false},
        {"1", true, true, true, false}
    };
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    struct CurveCase { float type; bool exact4; };
    const CurveCase curves[]{
        {4, true}, {std::nextafter(4.f, 0.f), false}, {4, true},
        {std::nextafter(4.f, 5.f), false}, {0, false}, {1, false}, {2, false},
        {3, false}, {-1, false}, {5, false}, {nan, false}, {infinity, false}, {-infinity, false}
    };
    struct EnableCase { float value; bool enabled; };
    const EnableCase enables[]{
        {0, false}, {.5f, false}, {std::nextafter(.5f, 0.f), false},
        {std::nextafter(.5f, 1.f), true}, {1, true}, {nan, false},
        {-1, false}, {infinity, true}, {-infinity, false}
    };
    unsigned checkedDraws = 0, rejectedDraws = 0;
    for (const auto& config : configs)
    {
        if (config.request) setenv("STRIKERS_VITAGL_FOG_CURVE4_SPECIALIZE", config.request, 1);
        else unsetenv("STRIKERS_VITAGL_FOG_CURVE4_SPECIALIZE");
        setenv("STRIKERS_VITAGL_AFFINE_UV", config.affine ? "1" : "0", 1);
        setenv("STRIKERS_VITAGL_VERTEX_FOG", config.vertexFog ? "1" : "0", 1);
        setenv("STRIKERS_VITAGL_FOG_OFF_SPECIALIZE", config.off ? "1" : "0", 1);
        Renderer r;
        const auto progress = shaderProgress.size();
        assert(r.initialize());
        // Capture the flag at initialize, even if it changes before prewarming.
        setenv("STRIKERS_VITAGL_FOG_CURVE4_SPECIALIZE", config.effective ? "0" : "1", 1);
        assert(r.initialize_scene_program());
        const unsigned total = 2 + (config.affine ? 160 : 80) * (config.off ? 2 : 1);
        assert(programs.size() == total && shaderProgress.size() == progress + total + 1);
        for (unsigned step = 0; step <= total; ++step)
            assert((shaderProgress[progress + step] == std::array<unsigned, 2>{step, total}));
        const auto ready = curve4_program_inventory(config);
        const auto compiles = shaderCompilations, creations = programCreations;
        assert(r.initialize_scene_program() && shaderProgress.size() == progress + total + 1);
        assert(r.begin_frame());
        FogGpuGeometry geometry;
        unsigned pn = 0;
        const auto materialFor = [](unsigned mask, bool alpha, float enable, float type) {
            SceneMaterial m;
            m.controls = {float(mask), alpha ? 1.f : -1.f, 1.f, 0.f};
            m.fog = {5, 130, enable, type};
            return m;
        };
        const auto draw = [&](const SceneMaterial& m, unsigned mask, bool alpha,
                              bool fogEnabled, bool generic) {
            const FogProgramSignature expected{generic ? 63u : mask, generic ? 1u : unsigned(alpha),
                unsigned(generic), generic ? 0u : unsigned(config.affine), pn,
                unsigned(!generic && config.off && !fogEnabled)};
            const auto submissions = submitted.size();
            const auto tableWrites = fogTableWrites, controlWrites = fogControlWrites;
            assert(geometry.draw(r, m));
            assert(program == ready.at(expected) && submitted.size() == submissions + 1);
            assert(shaderCompilations == compiles && programCreations == creations);
            const auto* bytes = reinterpret_cast<const uint8_t*>(geometry.vertices.data());
            assert(submitted.back() == std::vector<uint8_t>(bytes, bytes + sizeof geometry.vertices));
            if (expected[5])
                assert(fogTableWrites == tableWrites && fogControlWrites == controlWrites);
            else if (fogEnabled && m.fogRangeControl[0] == 1.f)
            {
                assert(fogTableByProgram.at(program) == std::vector<float>(m.fogRangeTable.begin(), m.fogRangeTable.end()));
                assert(fogControlByProgram.at(program) == std::vector<float>(m.fogRangeControl.begin(), m.fogRangeControl.end()));
            }
            ++checkedDraws;
        };
        for (pn = 0; pn < 2; ++pn)
        {
            for (auto& v : geometry.vertices) v.boneSlot = pn ? 1.f : 0.f;
            for (unsigned mask : fogTestMasks)
                for (bool alpha : {false, true})
                    draw(materialFor(mask, alpha, 1, 4), mask, alpha, true, false);
            if (config.effective)
            {
                for (const auto& enable : enables)
                    for (const auto& curve : curves)
                        draw(materialFor(1, true, enable.value, curve.type), 1, true,
                             enable.enabled, enable.enabled && !curve.exact4);
                for (unsigned mask : {2u, 63u, 128u, 256u, 512u, 513u})
                    for (float type : {4.f, 2.f})
                        for (float enable : {0.f, 1.f})
                            draw(materialFor(mask, true, enable, type), mask, true, enable == 1, true);
                for (float movie : {2.f, 3.f, 4.f})
                    for (float type : {4.f, 2.f})
                        for (float enable : {0.f, 1.f})
                        {
                            auto m = materialFor(1, true, enable, type);
                            m.controls[3] = movie;
                            draw(m, 1, true, enable == 1, true);
                            assert(controls[3] == movie);
                        }
            }
            else
                for (float type : {4.f, 2.f, nan})
                    for (float enable : {0.f, 1.f})
                        draw(materialFor(1, true, enable, type), 1, true, enable == 1, false);
            auto m = fog_gpu_material();
            m.controls = {1, 1, 1, 0};
            for (float type : {4.f, 2.f, 4.f})
            {
                m.fog[3] = type;
                draw(m, 1, true, true, config.vertexFog || (config.effective && type == 2));
            }
            const auto tableWrites = fogTableWrites, controlWrites = fogControlWrites;
            m.fogRangeTable[9] += .25f;
            draw(m, 1, true, true, config.vertexFog);
            assert(fogTableWrites == tableWrites + 1 && fogControlWrites == controlWrites);
        }
        if (config.effective)
        {
            auto invalid = fog_gpu_material();
            invalid.fog[3] = 4;
            if (config.off) invalid.fogRangeControl[1] = .5f;
            else invalid.fogRangeTable[0] = 0;
            const auto submissions = submitted.size();
            assert(!geometry.draw(r, invalid) && submitted.size() == submissions);
            assert(std::strcmp(r.last_error(), config.off ? "draw_fog_range_control" : "draw_fog_range_table") == 0);
            assert(shaderCompilations == compiles && programCreations == creations);
            ++rejectedDraws;
            (void)r.end_frame(false);
        }
        else
            assert(r.end_frame(false));
        r.shutdown(); empty();
    }
    unsetenv("STRIKERS_VITAGL_FOG_CURVE4_SPECIALIZE");
    unsetenv("STRIKERS_VITAGL_FOG_OFF_SPECIALIZE");
    unsetenv("STRIKERS_VITAGL_AFFINE_UV");
    unsetenv("STRIKERS_VITAGL_VERTEX_FOG");
    assert(checkedDraws == 1688 && rejectedDraws == 2);
    std::printf("Fog curve4 renderer: %u draws, %u invalid range rejections, 11 init-only configurations, exact keys/sources, transitions and no draw-time compilation passed\n",
                checkedDraws, rejectedDraws);
}
}
