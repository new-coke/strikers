// Unexpected texconfig bits must never masquerade as affine or rigid key bits.
namespace
{
void check_unexpected_material_masks()
{
    using namespace port::render;
    unsigned draws = 0;
    for (bool affine : {false, true})
        for (bool vertexFog : {false, true})
        {
            setenv("STRIKERS_VITAGL_AFFINE_UV", affine ? "1" : "0", 1);
            setenv("STRIKERS_VITAGL_VERTEX_FOG", vertexFog ? "1" : "0", 1);
            Renderer r;
            assert(r.initialize() && r.initialize_scene_program() && r.begin_frame());
            const auto creations = programCreations;
            FogGpuGeometry geometry;
            std::array<GLuint, 2> generic{};
            for (bool pn : {false, true})
                for (unsigned mask : {2u, 63u, 64u, 128u, 129u, 256u, 257u, 512u, 513u, 768u})
                    for (bool fog : {false, true})
                        for (float family : {0.f, 2.f, 3.f, 4.f})
                        {
                            for (auto& v : geometry.vertices) v.boneSlot = pn ? 1.f : 0.f;
                            SceneMaterial m;
                            m.controls[0] = float(mask);
                            m.controls[3] = family;
                            m.fog = {100, 1000, fog ? 1.f : 0.f, 2};
                            assert(geometry.draw(r, m));
                            const auto& sources = programSources.at(program);
                            const auto& fs = sources.at(GL_CG_FRAGMENT_SHADER_EXT);
                            const bool isGeneric = fs.find("float mask=u_controls.x;") != std::string::npos;
                            if (!isGeneric)
                                std::fprintf(stderr, "mask alias: mask=%u affine=%u vertex_fog=%u PN=%u fog=%u family=%.0f\n",
                                             mask, affine, vertexFog, pn, fog, family);
                            assert(isGeneric && program_uses_bones(program) == pn);
                            assert(fs.find("STRIKERS_AFFINE_UV 0") != std::string::npos);
                            assert(sources.at(GL_CG_VERTEX_SHADER_EXT).find("STRIKERS_VERTEX_FOG 0") != std::string::npos);
                            assert(controls.size() == 4 && controls[0] == float(mask) && controls[3] == family);
                            if (!generic[pn]) generic[pn] = program;
                            assert(program == generic[pn]);
                            ++draws;
                        }
            assert(generic[0] != generic[1] && programCreations == creations);
            assert(r.end_frame(false));
            r.shutdown();
            empty();
        }
    unsetenv("STRIKERS_VITAGL_AFFINE_UV");
    unsetenv("STRIKERS_VITAGL_VERTEX_FOG");
    assert(draws == 640);
    std::puts("Unexpected material masks: 640 generic draws, rigid/PN, fog ON/OFF and movie families passed");
}
}
