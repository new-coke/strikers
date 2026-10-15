#include "port/native_match_gpu_assets.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

using namespace port::directmatch;

static Mesh sample_mesh(uint64_t rawHash, uint64_t positionHash,
                        uint32_t vertexCount, uint8_t color) {
    Mesh mesh{};
    mesh.source = {vertexCount, 0, vertexCount * 2,
                   vertexCount * 12, vertexCount * 4,
                   rawHash, positionHash, 0};
    mesh.vertices.resize(vertexCount * kVertexStride, color);
    mesh.indices = {0, 1, 2};
    if (vertexCount > 3) mesh.indices.insert(mesh.indices.end(), {0, 2, 3});
    return mesh;
}

static void synthetic_contract() {
    Manifest original{};
    original.meshes.push_back(sample_mesh(91, 101, 3, 0x11));
    original.meshes.push_back(sample_mesh(13, 102, 4, 0x22));
    original.meshes.push_back(sample_mesh(13, 103, 3, 0x33));
    const auto keys = std::vector<SourceIdentity>{
        original.meshes[0].source, original.meshes[1].source,
        original.meshes[2].source
    };

    PreparedGpuAssets assets{};
    assert(assets.prepare(std::move(original)));
    assert(assets.mesh_count() == 3);
    assert(assets.upload_bytes() == 60 + 80 + 60 + 6 + 12 + 6);
    const auto& m = assets.meshes();
    assert(m[0].vertexOffset == 0 && m[0].indexOffset == 0);
    assert(m[1].vertexOffset == 60 && m[1].indexOffset == 6);
    assert(m[2].vertexOffset == 140 && m[2].indexOffset == 18);
    assert(m[0].vertexCount == 3 && m[1].indexCount == 6);
    assert(assets.vertices()[60] == 0x22);
    assert(assets.indices()[3] == 0 && assets.indices()[5] == 2);
    for (uint32_t i = 0; i < keys.size(); ++i)
        assert(assets.find(keys[i]) == i);

    auto collision = keys[2];
    collision.positionHash += 1;
    assert(assets.find(collision) == ~0u);
    collision.rawHash = 9999;
    assert(assets.find(collision) == ~0u);

    assets.release_upload_bytes();
    assert(assets.upload_bytes() == 0);
    for (uint32_t i = 0; i < keys.size(); ++i)
        assert(assets.find(keys[i]) == i);
    assert(assets.mesh_count() == 3);
    assets.clear();
    assert(assets.mesh_count() == 0);
}

static void corrupted_input_contract() {
    Manifest empty{};
    PreparedGpuAssets prepared{};
    assert(!prepared.prepare(std::move(empty)));
    Manifest invalid{};
    invalid.meshes.push_back(sample_mesh(2, 3, 3, 0x11));
    invalid.meshes[0].vertices.pop_back();
    assert(!prepared.prepare(std::move(invalid)));
}

static void real_manifest_contract(const char* path) {
    std::ifstream f(path, std::ios::binary);
    assert(f);
    const std::vector<uint8_t> bytes(
        (std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    Manifest manifest{};
    assert(parse(bytes, manifest));
    const size_t meshes = manifest.meshes.size();
    std::vector<SourceIdentity> keys{};
    keys.reserve(meshes);
    for (const auto& mesh : manifest.meshes)
        keys.push_back(mesh.source);

    PreparedGpuAssets assets{};
    assert(assets.prepare(std::move(manifest)));
    assert(assets.mesh_count() == meshes);
    size_t totalVertices = 0, totalIndices = 0;
    for (size_t i = 0; i < meshes; ++i) {
        const auto& mesh = assets.meshes()[i];
        assert(mesh.vertexOffset + mesh.vertexBytes <= assets.vertices().size());
        assert(mesh.indexOffset + mesh.indexBytes <= assets.indices().size() * 2);
        assert(mesh.vertexOffset % 4 == 0 && mesh.indexOffset % 2 == 0);
        assert(assets.find(keys[i]) == i);
        totalVertices += mesh.vertexCount;
        totalIndices += mesh.indexCount;
    }
    assets.release_upload_bytes();
    assert(assets.upload_bytes() == 0);
    assert(assets.find(keys.back()) == meshes - 1);
    std::printf("native match GPU pack PASS: %zu meshes, %zu vertices, %zu triangles\n",
                meshes, totalVertices, totalIndices / 3);
}

int main(int argc, char** argv) {
    synthetic_contract();
    corrupted_input_contract();
    if (argc > 1) real_manifest_contract(argv[1]);
    std::puts("native match GPU asset residency contract PASS");
    return 0;
}
