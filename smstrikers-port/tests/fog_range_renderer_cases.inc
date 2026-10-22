// These cases use the real renderer and explicit host reflection/upload mocks.
// They carry the existing fog-range consumer fixture into the permanent driver.
namespace
{
port::render::SceneMaterial fog_gpu_material()
{
    port::render::SceneMaterial m;
    m.fog = {5, 130, 1, 0};
    for (unsigned i = 0; i < m.fogRangeTable.size(); ++i)
        m.fogRangeTable[i] = 4.f + float(i) / 8;
    assert(port::render::fog_range_controls(5, 130, 960, 480, m.fogRangeControl));
    return m;
}
struct FogGpuGeometry
{
    std::array<port::render::ModelVertex, 3> vertices{};
    std::array<uint16_t, 3> indices{0, 1, 2};
    std::array<float, 16> matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    port::render::Raster raster;
    FogGpuGeometry() { vertices[1].position[0] = 1; vertices[2].position[1] = 1; }
    bool draw(port::render::Renderer& renderer, const port::render::SceneMaterial& m)
    {
        return renderer.draw_scene(0, vertices, indices, matrix, raster, m);
    }
};
void check_fog_range_renderer()
{
    using namespace port::render;
    using Fault = FogReflectionFault;
    unsetenv("STRIKERS_VITAGL_AFFINE_UV");
    unsetenv("STRIKERS_VITAGL_VERTEX_FOG");
    struct ReflectionCase { Fault fault; const char* expected; };
    const ReflectionCase reflectionCases[]{
        {Fault::MissingTable, "fog_range_uniform_missing"},
        {Fault::MissingControl, "fog_range_uniform_missing"},
        {Fault::TableType, "uniform_reflection"}, {Fault::TableZero, "uniform_reflection"},
        {Fault::TableOne, "fog_range_uniform_capacity"}, {Fault::TableTwo, "fog_range_uniform_capacity"},
        {Fault::ControlZero, "uniform_reflection"}, {Fault::MissingReflection, "uniform_capacity"},
        {Fault::ControlType, "uniform_reflection"}
    };
    for (const auto& c : reflectionCases)
    {
        Renderer r;
        fogReflectionFault = c.fault;
        assert(r.initialize() && !r.initialize_scene_program());
        assert(std::strcmp(r.last_error(), c.expected) == 0);
        r.shutdown();
        empty();
    }
    for (Fault fault : {Fault::None, Fault::ArrayName, Fault::OversizedTable, Fault::OversizedControl})
    {
        fogReflectionFault = fault;
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program() && r.begin_frame());
        FogGpuGeometry geometry;
        auto m = fog_gpu_material();
        for (auto& v : geometry.vertices) v.boneSlot = 1;
        m.bones.back()[3] = 17;
        assert(geometry.draw(r, m));
        assert(bones.size() == kSceneBoneFloats && bones.back() == 17);
        assert(fogTableByProgram.at(program) == std::vector<float>(m.fogRangeTable.begin(), m.fogRangeTable.end()));
        assert(fogControlByProgram.at(program) == std::vector<float>(m.fogRangeControl.begin(), m.fogRangeControl.end()));
        const auto tableWrites = fogTableWrites, controlWrites = fogControlWrites;
        assert(geometry.draw(r, m) && fogTableWrites == tableWrites && fogControlWrites == controlWrites);
        m.fogRangeTable[9] += .25f;
        assert(geometry.draw(r, m) && fogTableWrites == tableWrites + 1 && fogControlWrites == controlWrites);
        m.viewport[2] = 321.5f;
        assert(fog_range_controls(5, 130, 322, 480, m.fogRangeControl));
        assert(geometry.draw(r, m) && viewport[2] == 322 && fogControlWrites == controlWrites + 1);
        assert(r.end_frame(false));
        r.shutdown();
        empty();
    }
    fogReflectionFault = Fault::None;
    const char* failures[]{"draw_viewport_width", "draw_fog_range_control", "draw_fog_range_table",
                           "draw_fog_range_target"};
    for (unsigned failure = 0; failure < std::size(failures); ++failure)
    {
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program() && r.begin_frame());
        FogGpuGeometry geometry;
        auto m = fog_gpu_material();
        if (failure == 0) m.viewport[2] = std::numeric_limits<float>::quiet_NaN();
        if (failure == 1) m.fogRangeControl[1] = .5f;
        if (failure == 2) m.fogRangeTable[11] = 0;
        if (failure == 3)
        {
            const auto target = r.create_target(320, 224);
            assert(target && r.select_target(target));
        }
        const auto draws = submitted.size();
        assert(!geometry.draw(r, m) && submitted.size() == draws);
        assert(std::strcmp(r.last_error(), failures[failure]) == 0);
        (void)r.end_frame(false);
        r.shutdown();
        empty();
    }
    std::puts("Fog range reflection, exact owned uploads/cache and invalid draw rejection passed");
}
}
