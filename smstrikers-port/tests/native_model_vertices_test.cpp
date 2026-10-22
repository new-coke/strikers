#include "port/native_model_vertices.hpp"
#include <cassert>
#include <cstdlib>
#include <limits>
#include <new>
using namespace port::nativegxm;
static size_t allocations;
void* operator new(size_t bytes)
{
    ++allocations;
    if (void* p = std::malloc(bytes ? bytes : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
int main()
{
    const float positions[]{ 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0 };
    const uint8_t color[]{ 255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 64, 255, 255, 255, 255 };
    const int16_t uv[]{ 0, 0, 1024, 0, 0, 1024, 1024, 1024 };
    ModelStreams streams;
    streams.position = { std::span(reinterpret_cast<const uint8_t*>(positions), sizeof(positions)),
                         12, true };
    streams.color = { color, 4, true };
    streams.uv = { std::span(reinterpret_cast<const uint8_t*>(uv), sizeof(uv)), 4, true };
    ModelVertices out;
    // Immediate HUD quads: no legacy display list or shared index source.
    assert(prepare_model_vertices(streams, {}, 4, 3, {}, {}, out));
    assert(out.indices == std::vector<uint16_t>({ 0, 1, 2, 0, 2, 3 }));
    assert(out.vertices[3].uv[0] == 1 && out.vertices[3].uv[1] == 1 &&
           out.vertices[1].color[3] == 128);
    // Goal metal uses Diffuse + Gloss without Detail/Shadow/SelfIllum arrays.
    // These are semantic slots, not a packed list of active texture units.
    const int16_t glossUv[]{ 256, 768, 768, 256, 512, 512, 1024, 256 };
    streams.extraUv[3] = { { reinterpret_cast<const uint8_t*>(glossUv), sizeof(glossUv) },
                           4,
                           true };
    assert(prepare_model_vertices(streams, {}, 4, 3, {}, {}, out));
    assert((out.vertices[0].extraUv[3] == std::array<float, 3>{ .25f, .75f, 1 }));
    assert((out.vertices[1].extraUv[3] == std::array<float, 3>{ .75f, .25f, 1 }));
    assert((out.vertices[0].extraUv[0] == std::array<float, 3>{ 0, 0, 1 }));
    assert((out.vertices[0].uv == std::array<float, 3>{ 0, 0, 1 }));
    streams.extraUv[3] = {};
    const std::array<uint16_t, 3> indices{ 2, 0, 1 };
    std::array<float, 3> minimum{}, maximum{};
    assert(model_position_bounds(streams.position, indices, 3, minimum, maximum));
    assert((minimum == std::array<float, 3>{ 0, 0, 0 }));
    assert((maximum == std::array<float, 3>{ 1, 1, 0 }));
    const std::array<uint16_t, 3> repeatedBounds{ 2, 2, 2 };
    assert(model_position_bounds(streams.position, repeatedBounds, 3, minimum, maximum));
    assert(minimum == maximum && minimum[0] == 0 && minimum[1] == 1);
    assert(prepare_model_vertices(streams, indices, 3, 0, {}, {}, out));
    assert(out.vertices[0].position[1] == 1 && out.vertices[0].color[2] == 255);
    // Three independent indexed-PN selector slots, captured as owned world
    // positions. Source palette and arrays can be reused immediately after.
    std::array<BoneTransform, 2> bones;
    bones[0] = { 99, { 1, 0, 0, 5, 0, 1, 0, 6, 0, 0, 1, 7 } };
    bones[1] = { 102, { 0, -1, 0, 10, 1, 0, 0, 20, 0, 0, 1, 30 } };
    const std::array<uint8_t, 3> selectors{ 99, 102, 99 };
    assert(prepare_model_vertices(streams, indices, 3, 0, selectors, bones, out));
    assert(out.vertices[0].position[0] == 5 && out.vertices[0].position[1] == 7 &&
           out.vertices[0].position[2] == 7);
    assert(out.vertices[1].position[0] == 10 && out.vertices[1].position[1] == 20 &&
           out.vertices[1].position[2] == 30);
    bones[0].matrix[3] = 100;
    assert(out.vertices[0].position[0] == 5);
    auto missing = selectors;
    missing[0] = 105;
    assert(!prepare_model_vertices(streams, indices, 3, 0, missing, bones, out) &&
           out.vertices.empty());
    bones[1].selector = 99;
    assert(!prepare_model_vertices(streams, indices, 3, 0, selectors, bones, out));
    bones[1].selector = 102;
    bones[0].matrix[1] = std::numeric_limits<float>::quiet_NaN();
    assert(!prepare_model_vertices(streams, indices, 3, 0, selectors, bones, out));
    assert(prepare_model_vertices(streams, {}, 4, 1, {}, {}, out));
    assert(out.indices == std::vector<uint16_t>({ 0, 1, 2, 2, 1, 3 }));
    assert(prepare_model_vertices(streams, {}, 4, 2, {}, {}, out));
    assert(out.indices == std::vector<uint16_t>({ 0, 1, 2, 0, 2, 3 }));
    // Independent topology oracle: expand both outputs in triangle order and
    // compare every attribute bit. Repeated source indices with distinct PN
    // slots must remain separate, while unchanged references share storage.
    const std::array<uint16_t, 12> repeated{ 0, 1, 2, 0, 2, 3, 0, 1, 2, 0, 2, 3 };
    std::array<uint8_t, 12> pn{ 99, 99, 99, 99, 99, 99, 102, 102, 102, 102, 102, 102 };
    bones[0] = { 99, { 1, 0, 0, 5, 0, 1, 0, 6, 0, 0, 1, 7 } };
    bones[1] = { 102, { 0, -1, 0, 10, 1, 0, 0, 20, 0, 0, 1, 30 } };
    for (uint8_t primitive = 0; primitive < 4; ++primitive)
    {
        for (bool skinned : { false, true })
        {
            ModelVertices reference, compact;
            const auto slots = skinned ? std::span<const uint8_t>(pn) : std::span<const uint8_t>{};
            assert(
                prepare_model_vertices(streams, repeated, 12, primitive, slots, bones, reference));
            assert(prepare_model_vertices(streams, repeated, 12, primitive, slots, bones, compact,
                                          true));
            assert(compact.vertices.size() == size_t(skinned ? 8 : 4));
            assert(reference.indices.size() == compact.indices.size());
            for (size_t i = 0; i < reference.indices.size(); ++i)
                assert(!std::memcmp(&reference.vertices[reference.indices[i]],
                                    &compact.vertices[compact.indices[i]], sizeof(ModelVertex)));
        }
    }
    // High rigid selectors preserve the original GX row-to-slot mapping in
    // both the CPU reference and resident GPU representations. The shipped
    // Birdo mesh reaches 13 bones; the native contract is bounded at 32.
    for (const unsigned boneCount : { 13u, 32u })
    {
        std::array<BoneTransform, 32> extendedBones{};
        for (unsigned i = 0; i < boneCount; ++i)
            extendedBones[i] = { (i + 1) * 3,
                { 1, 0, 0, float(i + 1), 0, 1, 0, float((i + 1) * 2),
                  0, 0, 1, float((i + 1) * 3) } };
        const std::array<uint8_t, 3> extendedSelectors{ 3, uint8_t(boneCount * 3), 30 };
        ModelVertices cpu, gpu;
        const auto palette = std::span(extendedBones).first(boneCount);
        assert(prepare_model_vertices(streams, indices, 3, 0, extendedSelectors, palette, cpu));
        assert(prepare_model_vertices(streams, indices, 3, 0, extendedSelectors, palette, gpu,
                                      true, false));
        for (unsigned v = 0; v < 3; ++v)
        {
            const auto slot = unsigned(extendedSelectors[v]) / 3;
            assert(gpu.vertices[v].boneSlot == float(slot) && cpu.vertices[v].boneSlot == 0);
            for (unsigned axis = 0; axis < 3; ++axis)
                assert(cpu.vertices[v].position[axis] ==
                       gpu.vertices[v].position[axis] + float(slot * (axis + 1)));
        }
    }
    const std::array<uint16_t, 3> degenerate{ 1, 1, 1 };
    assert(prepare_model_vertices(streams, degenerate, 3, 0, {}, {}, out, true));
    assert(out.vertices.size() == 1 && out.indices == std::vector<uint16_t>({ 0, 0, 0 }));
    // Hash collisions and the full u16 source-index range must preserve the
    // exact primitive/attribute sequence. Changes between frames are fresh.
    std::vector<float> manyPositions(65536 * 3);
    for (size_t i = 0; i < manyPositions.size(); ++i)
        manyPositions[i] = float(int(i % 101) - 50);
    ModelStreams many;
    many.position = { { reinterpret_cast<const uint8_t*>(manyPositions.data()),
                        manyPositions.size() * 4 },
                      12,
                      true };
    std::vector<uint16_t> randomIndices(3000);
    std::vector<uint8_t> randomPn(3000);
    uint32_t seed = 123;
    for (size_t i = 0; i < randomIndices.size(); ++i)
    {
        seed = 1664525u * seed + 1013904223u;
        randomIndices[i] = uint16_t((i % 11) ? seed % 53 : 65535);
        randomPn[i] = (i % 5) ? 99 : 102;
    }
    for (uint8_t primitive = 0; primitive < 4; ++primitive)
    {
        ModelVertices reference, compact;
        assert(prepare_model_vertices(many, randomIndices, 3000, primitive, randomPn, bones,
                                      reference));
        assert(prepare_model_vertices(many, randomIndices, 3000, primitive, randomPn, bones,
                                      compact, true));
        assert(compact.vertices.size() < reference.vertices.size() / 10);
        assert(reference.indices.size() == compact.indices.size());
        for (size_t i = 0; i < reference.indices.size(); ++i)
            assert(!std::memcmp(&reference.vertices[reference.indices[i]],
                                &compact.vertices[compact.indices[i]], sizeof(ModelVertex)));
    }
    manyPositions[0] = std::numeric_limits<float>::quiet_NaN();
    randomIndices[1] = 0;
    assert(!prepare_model_vertices(many, randomIndices, 3000, 0, randomPn, bones, out, true) &&
           out.vertices.empty());
    const uint8_t fixed[]{ 0, 128, 1, 0, 0xfe, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 };
    streams.position = { fixed, 6, false };
    streams.color = {};
    streams.uv = {};
    assert(model_position_bounds(streams.position, {}, 3, minimum, maximum));
    assert((minimum == std::array<float, 3>{ 0, 0, -2 }));
    assert((maximum == std::array<float, 3>{ 1, 1, 0 }));
    auto truncated = streams.position;
    truncated.bytes = truncated.bytes.first(5);
    const auto priorMinimum = minimum;
    assert(!model_position_bounds(truncated, {}, 3, minimum, maximum));
    assert(minimum == priorMinimum);
    assert(prepare_model_vertices(streams, {}, 3, 0, {}, {}, out));
    assert(out.vertices[0].position[0] == .5f && out.vertices[0].position[1] == 1 &&
           out.vertices[0].position[2] == -2);
    auto invalid = indices;
    invalid[0] = 3;
    assert(!prepare_model_vertices(streams, invalid, 3, 0, {}, {}, out));
    assert(!prepare_model_vertices(streams, {}, 4, 0, {}, {}, out));

    // Warm producer storage is reused without retaining prior geometry or PN
    // selectors. Compare the expanded triangles against separately owned draws.
    std::array<float, 12> changingPositions{ 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0 };
    ModelStreams changing;
    changing.position = { { reinterpret_cast<const uint8_t*>(changingPositions.data()),
                             sizeof(changingPositions) }, 12, true };
    changing.color = { color, 4, true };
    ModelVertices reused;
    ModelVertexScratch scratch;
    for (uint8_t primitive = 0; primitive < 4; ++primitive)
        assert(prepare_model_vertices(changing, repeated, repeated.size(), primitive,
                                      pn, bones, reused, true, false, &scratch));
    const auto warmAllocations = allocations;
    for (unsigned frame = 0; frame < 12; ++frame)
    {
        recycle_model_vertex_scratch(reused, scratch);
        changingPositions[0] = float(frame);
        const uint8_t primitive = frame % 4;
        assert(prepare_model_vertices(changing, repeated, repeated.size(), primitive,
                                      pn, bones, reused, true, false, &scratch));
        assert(reused.vertices.size() == 8);
        assert(reused.vertices[0].position[0] == float(frame));
        assert(reused.vertices[0].boneSlot == 33 && reused.vertices[4].boneSlot == 34);
    }
    assert(allocations == warmAllocations);
    ModelVertices owned;
    assert(prepare_model_vertices(changing, repeated, repeated.size(), 3, pn, bones,
                                  owned, true, false));
    for (size_t i = 0; i < owned.indices.size(); ++i)
        assert(!std::memcmp(&owned.vertices[owned.indices[i]],
                            &reused.vertices[reused.indices[i]], sizeof(ModelVertex)));
    const auto ownedPosition = owned.vertices[0].position;
    recycle_model_vertex_scratch(reused, scratch);
    changingPositions[0] = -50;
    assert(prepare_model_vertices(changing, {}, 4, 3, {}, {}, reused, true, false, &scratch));
    assert(reused.vertices.size() == 4 && reused.vertices[0].boneSlot == 0);
    assert(reused.vertices[0].position[0] == -50 && owned.vertices[0].position == ownedPosition);
    auto badIndices = repeated;
    badIndices[4] = 65535;
    assert(!prepare_model_vertices(changing, badIndices, badIndices.size(), 0, {}, {},
                                   reused, true, false, &scratch));
    assert(reused.vertices.empty() && reused.indices.empty());
    const auto recoveryAllocations = allocations;
    assert(prepare_model_vertices(changing, repeated, repeated.size(), 0, {}, {},
                                  reused, true, false, &scratch));
    assert(reused.vertices.size() == 4 && reused.vertices[0].position[0] == -50);
    assert(allocations == recoveryAllocations);
    // An exceptional large draw cannot leave multi-megabyte scratch resident.
    reused.vertices.reserve(4097);
    reused.indices.reserve(4096 * 3 + 1);
    scratch.lookup.reserve(4096 * 2 + 1);
    scratch.remap.reserve(4097);
    recycle_model_vertex_scratch(reused, scratch);
    assert(!reused.vertices.capacity() && !reused.indices.capacity());
    assert(!scratch.lookup.capacity() && !scratch.remap.capacity());

    // Transition outlines are line strips; preserve each segment, including
    // the closing edge, when compacting repeated vertices.
    const std::array<uint16_t, 5> outline{ 0, 1, 2, 3, 0 };
    assert(prepare_model_vertices(changing, outline, outline.size(), 5, {}, {},
                                  reused, true, false, &scratch));
    assert(reused.vertices.size() == 4);
    assert(reused.indices == std::vector<uint16_t>({ 0, 1, 1, 2, 2, 3, 3, 0 }));
    const std::array<uint16_t, 2> segment{ 2, 0 };
    for (uint8_t primitive : { 4, 5 })
    {
        assert(prepare_model_vertices(changing, segment, segment.size(), primitive,
                                      {}, {}, reused, true, false, &scratch));
        assert(reused.indices == std::vector<uint16_t>({ 0, 1 }));
        assert(reused.vertices[0].position[1] == 1 && reused.vertices[1].position[0] == -50);
    }
    assert(!prepare_model_vertices(changing, outline, outline.size(), 4, {}, {}, reused));
    assert(!prepare_model_vertices(changing, {}, 1, 5, {}, {}, reused));
    assert(!prepare_model_vertices(changing, {}, 4, 6, {}, {}, reused));
}
