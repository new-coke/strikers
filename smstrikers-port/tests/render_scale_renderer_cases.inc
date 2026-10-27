// Render scale: a smaller main target addressed in 960x544 logical
// pixels, filtered up to the display at present.
namespace
{
void check_render_scale()
{
    using namespace port::render;
    constexpr std::array<float, 16> identity{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    for (const char* value : { "75", "49", "100", "abc" })
    {
        setenv("STRIKERS_VITAGL_RENDER_SCALE", value, 1);
        const bool scaled = std::strcmp(value, "75") == 0;
        Renderer r;
        assert(r.initialize() && r.initialize_scene_program() && r.begin_frame());
        std::array<ModelVertex, 3> vertices;
        vertices[1].position[0] = 1;
        vertices[2].position[1] = 1;
        const std::array<uint16_t, 3> indices{ 0, 1, 2 };
        SceneMaterial m;
        assert(r.draw_scene(0, vertices, indices, identity, Raster{}, m));
        assert((submittedViewports.back() ==
                (scaled ? std::array<int, 4>{ 0, 0, 720, 408 } : std::array<int, 4>{ 0, 0, 960, 544 })));
        // The top-left logical quarter maps to the physical top-left quarter.
        m.scissor = { 0, 0, 480, 272 };
        assert(r.draw_scene(0, vertices, indices, identity, Raster{}, m));
        assert((scissor ==
                (scaled ? std::array<int, 4>{ 0, 204, 360, 204 } : std::array<int, 4>{ 0, 272, 480, 272 })));
        // Copies still take logical source rectangles on the scaled target.
        const auto destination = r.create_target(80, 80);
        assert(destination && r.copy_texture(destination, { 0, 0, 960, 544 }, false));
        assert(!r.copy_texture(destination, { 0, 0, 961, 544 }, false));
        r.release_texture(destination);
        r.shutdown();
        empty();
        assert(r.initialize() && r.initialize_scene_program() && r.begin_frame() && r.end_frame());
        assert((lastBlitSource ==
                (scaled ? std::array<int, 4>{ 0, 0, 720, 408 } : std::array<int, 4>{ 0, 0, 960, 544 })));
        assert((lastBlitDestination == std::array<int, 4>{ 0, 0, 960, 544 }));
        assert(lastBlitFilter == GLenum(scaled ? GL_LINEAR : GL_NEAREST));
        r.shutdown();
        empty();
    }
    unsetenv("STRIKERS_VITAGL_RENDER_SCALE");
    std::puts("Render scale: physical viewport/scissor, logical copies and filtered present passed");
}
}
