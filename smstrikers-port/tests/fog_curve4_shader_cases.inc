namespace
{
std::string curve4_expected_source(const std::string& full)
{
    // Extract the existing type-4 assignment from the reference source. The
    // oracle preserves every byte outside the curve-selection region.
    const auto first = full.find("  if(u_fog.w==1.0)");
    const auto last = full.find("c.rgb=lerp(c.rgb,u_fogcolor.rgb,f);", first);
    const std::string selector = "if(u_fog.w==4.0)";
    const auto selected = full.find(selector, first);
    assert(first != std::string::npos && last != std::string::npos && selected < last);
    const auto assignment = full.substr(selected + selector.size(), last - selected - selector.size());
    assert(assignment == "f=exp2(-8.0*(1.0-f)*(1.0-f));");
    return full.substr(0, first) + "  " + assignment + full.substr(last);
}
void check_fog_curve4_shader_sources()
{
    using namespace port::render;
    constexpr unsigned masks[]{0, 1, 3, 5, 7, 9, 13, 17, 19, 21,
                                25, 33, 35, 37, 39, 41, 43, 49, 53, 57};
    unsigned cases = 0;
    for (bool affine : {false, true})
        for (bool vertexFog : {false, true})
        {
            const auto generic = scene_fragment_shader(63, true, true, affine, vertexFog);
            assert(generic == scene_fragment_shader(63, true, true, affine, vertexFog, false, true));
            for (unsigned mask : masks)
                for (bool alpha : {false, true})
                {
                    const auto full = scene_fragment_shader(mask, alpha, false, affine, vertexFog);
                    assert(full == scene_fragment_shader(mask, alpha, false, affine, vertexFog, false, false));
                    const auto specialized = scene_fragment_shader(mask, alpha, false, affine, vertexFog, false, true);
                    assert(specialized == curve4_expected_source(full));
                    assert(specialized.find("u_fog.w") == std::string::npos);
                    assert(specialized.find("exp2(") == specialized.rfind("exp2("));
                    assert(specialized.find("if(u_fog.z>0.5)") != std::string::npos);
                    assert(specialized.find("rangeAdjust=sqrt(offset*offset+k*k)/k;") != std::string::npos);
                    const auto off = scene_fragment_shader(mask, alpha, false, affine, vertexFog, true);
                    assert(off == scene_fragment_shader(mask, alpha, false, affine, vertexFog, true, true));
                    ++cases;
                }
        }
    const auto full = scene_fragment_shader(1, true, false, false, false);
    auto valid = full;
    assert(scene_specialize_fog_curve4(valid) && valid == curve4_expected_source(full));
    auto changed = full;
    const auto position = changed.find("if(u_fog.w==3.0)");
    assert(position != std::string::npos);
    changed.replace(position, std::strlen("if(u_fog.w==3.0)"), "if(u_fog.w==3.1)");
    for (const auto& bad : {std::string{}, changed, full + full,
                            std::string("u_fog.w;") + full, full + "u_fog.w;", valid})
    {
        auto input = bad;
        assert(!scene_specialize_fog_curve4(input) && input == bad);
    }
    assert(cases == 160);
    std::printf("Fog curve4 sources: %u families, exact reference assignment and unchanged range/material/OFF/generic; six atomic rejection cases passed\n", cases);
}
}
