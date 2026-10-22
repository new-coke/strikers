namespace
{
std::string shader_region(const std::string& source, const char* first, const char* last)
{
    const auto begin = source.find(first);
    const auto end = source.find(last, begin);
    assert(begin != std::string::npos && end != std::string::npos && end > begin);
    return source.substr(begin, end - begin);
}
void check_fog_off_shader_sources()
{
    using namespace port::render;
    constexpr unsigned masks[]{0, 1, 3, 5, 7, 9, 13, 17, 19, 21,
                               25, 33, 35, 37, 39, 41, 43, 49, 53, 57};
    unsigned cases = 0;
    for (bool affine : {false, true})
        for (bool vertexFog : {false, true})
            for (unsigned mask : masks)
                for (bool alpha : {false, true})
                {
                    const auto full = scene_fragment_shader(mask, alpha, false, affine, vertexFog);
                    const auto off = scene_fragment_shader(mask, alpha, false, affine, vertexFog, true);
                    assert(!off.empty());
                    assert(full == scene_fragment_shader(mask, alpha, false, affine, vertexFog, false));
                    // Compare every texture/color/alpha operation, rather than a
                    // second transcription of material arithmetic or sampled pixels.
                    assert(shader_region(full, " float mask=", " if(u_fog.z>0.5){") ==
                           shader_region(off, " float mask=", " return saturate(c);"));
                    assert(shader_region(full, "#if STRIKERS_AFFINE_UV\n", "#endif\n") ==
                           shader_region(off, "#if STRIKERS_AFFINE_UV\n", "#endif\n"));
                    assert(shader_region(full, " uniform sampler2D u_tex0:", " uniform float4 u_controls,") ==
                           shader_region(off, " uniform sampler2D u_tex0:", " uniform float4 u_controls,"));
                    assert((off.find("discard;") != std::string::npos) == alpha);
                    assert(off.find("float4 sh=tex2D(u_tex2,SCENE_UV(uv2));") != std::string::npos);
                    assert(off.find("u_controls.w==") == std::string::npos);
                    assert(off.find("return saturate(c);") != std::string::npos);
                    for (const char* dependency : {"scene_fog_", "u_fog", "fragmentPosition", "WPOS", "eyeZ"})
                        assert(off.find(dependency) == std::string::npos);
                    assert(full.find("scene_fog_range_pair") != std::string::npos);
                    assert(full.find("float4 fragmentPosition:WPOS") != std::string::npos);
                    assert(full.find("c.rgb=lerp(c.rgb,u_fogcolor.rgb,f);") != std::string::npos);
                    ++cases;
                }
    assert(cases == 160);
    const auto generic = scene_fragment_shader(63, true, true, false, false);
    assert(generic.find("u_controls.w==2.0?uv1:uv2") != std::string::npos);
    assert(generic.find("y=d.r,u=detail.r-0.5,v=sh.r-0.5") != std::string::npos);
    assert(generic.find("if(u_controls.w==3.0)c=float4(c.r,c.r,c.r,c.r);") != std::string::npos);
    assert(generic.find("if(u_controls.w==4.0)c=float4(d.a,d.a,d.a,d.a);") != std::string::npos);
    assert(scene_fragment_shader(63, true, true, false, false, true).empty());
    std::puts("Fog-OFF shader sources: 160 families preserve every non-fog operation, sampler and alpha branch");
}
}
