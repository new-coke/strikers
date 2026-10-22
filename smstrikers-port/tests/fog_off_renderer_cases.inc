// GL calls observe the real renderer's choice; no Cg or pixel output is simulated.
namespace
{
using FogProgramSignature = std::array<unsigned, 6>; // mask, alpha, generic, affine, PN, OFF
constexpr unsigned fogTestMasks[]{0, 1, 3, 5, 7, 9, 13, 17, 19, 21,
                                  25, 33, 35, 37, 39, 41, 43, 49, 53, 57};
std::map<FogProgramSignature, GLuint> fog_program_inventory(bool affine, bool vertexFog)
{
    std::map<FogProgramSignature, GLuint> result;
    for (GLuint id : programs)
    {
        const auto& stages = programSources.at(id);
        const auto& fs = stages.at(GL_CG_FRAGMENT_SHADER_EXT);
        const bool generic = fs.find("float mask=u_controls.x;") != std::string::npos;
        const bool off = program_fog_off(id), pn = program_uses_bones(id);
        unsigned mask = 63, matches = 0;
        if (!generic)
        {
            for (unsigned candidate : fogTestMasks)
                if (fs.find("float mask=" + std::to_string(candidate) + ".0;") != std::string::npos)
                { mask = candidate; ++matches; }
            assert(matches == 1);
        }
        const bool alpha = fs.find("discard;") != std::string::npos;
        const bool affineSource = fs.find("STRIKERS_AFFINE_UV 1") != std::string::npos;
        const bool vertexSource = stages.at(GL_CG_VERTEX_SHADER_EXT).find("STRIKERS_VERTEX_FOG 1") != std::string::npos;
        assert(vertexSource == (vertexFog && !generic && !off));
        if (generic) assert(alpha && !affineSource && !off);
        assert(result.emplace(FogProgramSignature{mask, alpha, generic, affineSource, pn, off}, id).second);
    }
    std::set<FogProgramSignature> expected;
    for (unsigned pn : {0u, 1u})
    {
        expected.insert({63, 1, 1, 0, pn, 0});
        for (unsigned mask : fogTestMasks)
            for (unsigned alpha : {0u, 1u})
                for (unsigned off : {0u, 1u})
                    for (unsigned uv = 0; uv < (affine ? 2u : 1u); ++uv)
                        expected.insert({mask, alpha, 0, uv, pn, off});
    }
    assert(result.size() == expected.size());
    for (const auto& signature : expected) assert(result.count(signature) == 1);
    return result;
}
void check_fog_off_renderer()
{
    using namespace port::render;
    unsetenv("STRIKERS_VITAGL_AFFINE_UV");
    unsetenv("STRIKERS_VITAGL_VERTEX_FOG");
    for (const char* flag : {static_cast<const char*>(nullptr), "0", "yes", "1"})
    {
        if (flag) setenv("STRIKERS_VITAGL_FOG_OFF_SPECIALIZE", flag, 1);
        else unsetenv("STRIKERS_VITAGL_FOG_OFF_SPECIALIZE");
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program());
        assert(programs.size() == (flag && std::strcmp(flag, "1") == 0 ? 162u : 82u));
        r.shutdown(); empty();
    }
    unsigned checkedDraws = 0;
    for (bool affine : {false, true})
        for (bool vertexFog : {false, true})
        {
            setenv("STRIKERS_VITAGL_FOG_OFF_SPECIALIZE", "1", 1);
            setenv("STRIKERS_VITAGL_AFFINE_UV", affine ? "1" : "0", 1);
            setenv("STRIKERS_VITAGL_VERTEX_FOG", vertexFog ? "1" : "0", 1);
            Renderer r;
            const auto progress = shaderProgress.size();
            assert(r.initialize() && r.initialize_scene_program());
            const unsigned total = affine ? 322 : 162;
            assert(programs.size() == total && shaders.size() == (vertexFog ? 4u : 2u));
            assert(shaderProgress.size() == progress + total + 1);
            for (unsigned step = 0; step <= total; ++step)
                assert((shaderProgress[progress + step] == std::array<unsigned, 2>{step, total}));
            const auto ready = fog_program_inventory(affine, vertexFog);
            const auto compiles = shaderCompilations, creations = programCreations;
            // This option is read once during initialize, not during a draw.
            setenv("STRIKERS_VITAGL_FOG_OFF_SPECIALIZE", "0", 1);
            assert(r.initialize_scene_program() && shaderProgress.size() == progress + total + 1);
            std::array<uint32_t, 6> textureIds{};
            for (unsigned slot = 0; slot < textureIds.size(); ++slot)
            {
                TextureUpload image;
                image.width = image.height = image.mipCount = 1;
                image.stride = 8;
                image.storage = TextureStorage::Rgba8;
                image.pixels.assign(32, uint8_t(20 + slot));
                textureIds[slot] = r.create_texture(image);
                assert(textureIds[slot]);
            }
            assert(r.begin_frame());
            FogGpuGeometry geometry;
            const auto materialFor = [&](unsigned mask, bool alpha, float fog) {
                SceneMaterial m;
                m.textures = textureIds;
                m.controls = {float(mask), alpha ? 1.f : -1.f, 1.f, 0.f};
                m.fog = {100, 1000, fog, 4};
                for (unsigned slot = 0; slot < 6; ++slot)
                {
                    m.wrapU[slot] = slot % 2;
                    m.wrapV[slot] = (slot + 1) % 2;
                    m.filter[slot] = slot % 2;
                }
                return m;
            };
            const auto draw = [&](const SceneMaterial& m, const FogProgramSignature& expected) {
                const auto tableWrites = fogTableWrites, controlWrites = fogControlWrites;
                const auto submissions = submitted.size();
                assert(geometry.draw(r, m));
                assert(program == ready.at(expected) && submitted.size() == submissions + 1);
                assert(shaderCompilations == compiles && programCreations == creations);
                const auto* bytes = reinterpret_cast<const uint8_t*>(geometry.vertices.data());
                assert(submitted.back() == std::vector<uint8_t>(bytes, bytes + sizeof geometry.vertices));
                for (unsigned slot = 0; slot < 6; ++slot)
                    if (expected[2] || (expected[0] & (1u << slot)))
                    {
                        assert(textureUnits[slot] == textureIds[slot]);
                        const auto& sampler = textureParameters.at(textureIds[slot]);
                        assert(sampler.at(GL_TEXTURE_WRAP_S) == (m.wrapU[slot] ? GL_CLAMP_TO_EDGE : GL_REPEAT));
                        assert(sampler.at(GL_TEXTURE_WRAP_T) == (m.wrapV[slot] ? GL_CLAMP_TO_EDGE : GL_REPEAT));
                        assert(sampler.at(GL_TEXTURE_MIN_FILTER) == (m.filter[slot] ? GL_LINEAR : GL_NEAREST));
                        assert(sampler.at(GL_TEXTURE_MAG_FILTER) == (m.filter[slot] ? GL_LINEAR : GL_NEAREST));
                    }
                if (expected[5])
                {
                    assert(fogTableWrites == tableWrites && fogControlWrites == controlWrites);
                    for (const char* name : {"u_fog", "u_fogcolor", "u_fogrange", "u_fograngecontrol"})
                        assert(glGetUniformLocation(program, name) == -1);
                }
                ++checkedDraws;
            };
            struct FogCase { float enable; bool enabled; };
            const FogCase fogCases[]{
                {0, false}, {1, true}, {0, false}, {.5f, false},
                {std::nextafter(.5f, 0.f), false}, {std::nextafter(.5f, 1.f), true},
                {-1, false}, {std::numeric_limits<float>::quiet_NaN(), false}
            };
            for (unsigned pn : {0u, 1u})
            {
                for (auto& v : geometry.vertices) v.boneSlot = pn ? 1.f : 0.f;
                for (unsigned mask : fogTestMasks)
                    for (bool alpha : {false, true})
                        for (const auto& fog : fogCases)
                            draw(materialFor(mask, alpha, fog.enable), {mask, alpha, 0, affine, pn, !fog.enabled});
                for (unsigned mask : {2u, 63u, 128u, 256u, 512u, 513u})
                    for (float fog : {0.f, 1.f})
                        draw(materialFor(mask, true, fog), {63, 1, 1, 0, pn, 0});
                for (float family : {2.f, 3.f, 4.f})
                    for (float fog : {0.f, 1.f})
                    {
                        auto m = materialFor(1, true, fog);
                        m.controls[3] = family;
                        draw(m, {63, 1, 1, 0, pn, 0});
                        const auto& fs = programSources.at(program).at(GL_CG_FRAGMENT_SHADER_EXT);
                        assert(fs.find("u_controls.w==2.0?uv1:uv2") != std::string::npos);
                        assert(controls[3] == family);
                    }
                for (unsigned mode = 0; mode < 3; ++mode)
                {
                    auto m = materialFor(1, true, 1);
                    if (mode == 0) m.fog = {10000, 10000.01f, 1, 0};
                    if (mode == 1) m.fog = {1000, 500, 1, 0};
                    if (mode == 2) assert(fog_range_controls(100, 1000, 960, 480, m.fogRangeControl));
                    draw(m, vertexFog ? FogProgramSignature{63, 1, 1, 0, pn, 0}
                                      : FogProgramSignature{1, 1, 0, affine, pn, 0});
                }
                auto m = materialFor(1, false, 0);
                for (auto& v : geometry.vertices) v.uv[2] = 2;
                draw(m, {1, 0, 0, 0, pn, 1});
                m.environment[1] = 1; // Generated UV restores Q=1.
                draw(m, {1, 0, 0, affine, pn, 1});
                for (auto& v : geometry.vertices) v.uv[2] = 1;
                m = materialFor(1, false, .5f);
                m.fog[0] = m.fog[1] = m.fogRangeTable[0] = m.fogRangeControl[0] =
                    std::numeric_limits<float>::quiet_NaN();
                draw(m, {1, 0, 0, affine, pn, 1});
            }
            assert(r.end_frame(false));
            r.shutdown(); empty();
        }
    setenv("STRIKERS_VITAGL_FOG_OFF_SPECIALIZE", "1", 1);
    unsetenv("STRIKERS_VITAGL_AFFINE_UV");
    unsetenv("STRIKERS_VITAGL_VERTEX_FOG");
    for (unsigned failure = 0; failure < 3; ++failure)
    {
        Renderer r;
        assert(r.initialize());
        const auto contexts = contextInitializations;
        failFogOffCompile = failure == 0;
        failFogOffLink = failure == 1;
        forceFogOffRangeUniform = failure == 2;
        assert(!r.initialize_scene_program() && programs.size() == 2);
        const char* reason[]{"shader_compile", "shader_link", "fog_off_uniform"};
        assert(std::strcmp(r.last_error(), reason[failure]) == 0);
        failFogOffCompile = failFogOffLink = forceFogOffRangeUniform = false;
        r.shutdown(); empty();
        assert(r.initialize() && r.initialize_scene_program() && programs.size() == 162);
        assert(contextInitializations == contexts);
        r.shutdown(); empty();
    }
    unsetenv("STRIKERS_VITAGL_FOG_OFF_SPECIALIZE");
    assert(checkedDraws == 2752);
    std::printf("Fog-OFF renderer: %u draws, exact program inventory, sampling, fallbacks, init-only option and failure restart passed\n",
                checkedDraws);
}
}
