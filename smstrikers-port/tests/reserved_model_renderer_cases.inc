// Reserved-id models from queued creation: admitted ones draw resident, and
// ones that fail admission stream their kept copy like a create_model miss.
namespace
{
void check_reserved_models()
{
    using namespace port::render;
    constexpr std::array<float, 16> identity{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    constexpr uint32_t resident = 0x80000000u, streamed = resident + 1;
    Renderer r;
    assert(r.initialize() && r.initialize_scene_program() && r.begin_frame());
    std::array<ModelVertex, 3> vertices;
    vertices[1].position[0] = 1;
    vertices[2].position[1] = 1;
    const std::array<uint16_t, 3> indices{ 0, 1, 2 };
    r.create_model_as(resident, vertices, indices);
    assert(r.error_code() == 0);
    auto uploads = bufferUploads;
    auto draws = submitted.size();
    assert(r.draw_scene(resident, {}, {}, identity, Raster{}, SceneMaterial{}));
    assert(bufferUploads == uploads && submitted.size() == draws + 1);
    failBufferTarget = GL_ARRAY_BUFFER;
    r.create_model_as(streamed, vertices, indices);
    failBufferTarget = 0;
    assert(r.error_code() == GL_OUT_OF_MEMORY);
    uploads = bufferUploads;
    draws = submitted.size();
    assert(r.draw_scene(streamed, {}, {}, identity, Raster{}, SceneMaterial{}));
    assert(bufferUploads == uploads + 2 && submitted.size() == draws + 1);
    // Ids are never reused, and release drops the kept copy.
    r.create_model_as(resident, vertices, indices);
    assert(r.draw_scene(resident, {}, {}, identity, Raster{}, SceneMaterial{}));
    r.release_model(streamed);
    assert(!r.draw_scene(streamed, {}, {}, identity, Raster{}, SceneMaterial{}));
    assert(r.end_frame(false));
    r.release_model(resident);
    r.shutdown();
    empty();
    std::puts("Reserved models: resident admission, streamed fallback copy, no id reuse and release passed");
}
}
