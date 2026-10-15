#include "port/native_gxm_renderer.hpp"
#include "port/native_match_gpu_assets.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
#include <limits>

using namespace port::nativegxm;
int main(int argc, char** argv) {
    std::array<float,16> matrix{};
    assert(finite_mvp(matrix));
    matrix.fill(std::numeric_limits<float>::max());assert(finite_mvp(matrix));
    for (unsigned n=0;n<matrix.size();++n) {
        for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity()}) {
            matrix[n]=invalid;assert(!finite_mvp(matrix));
            matrix[n]=0;assert(finite_mvp(matrix));
        }
    }
    FrameSlots slots;
    assert(!slots.present());
    for (unsigned n=0;n<9;++n) {
        const auto front=slots.front(), back=slots.back();
        assert(front!=back);
        assert(slots.begin());
        assert(!slots.begin());
        // A failed driver present must leave these slot identities unchanged.
        assert(slots.front()==front && slots.back()==back && slots.active());
        assert(slots.present());
        assert(slots.front()==back && slots.back()==(back+1)%3);
        assert(!slots.present());
    }
    slots.reset();
    assert(!slots.active() && slots.front()==0 && slots.back()==1);
    bool seen[32]{};
    for (uint8_t blend=0;blend<8;++blend) for (unsigned mask=0;mask<4;++mask) {
        Raster r;
        r.blend=blend; r.writeRgb=mask&1; r.writeAlpha=mask&2;
        assert(r.valid() && r.fragment_slot()<32);
        assert(!seen[r.fragment_slot()]); seen[r.fragment_slot()]=true;
    }
    Raster bad; bad.blend=8; assert(!bad.valid());
    bad={}; bad.cull=4; assert(!bad.valid());
    bad={}; bad.depthFunction=4; assert(!bad.valid());
    Mesh m{0,60,0,3,3};
    assert(valid_mesh(m,60,6));
    assert(!valid_mesh(m,59,6) && !valid_mesh(m,60,5));
    m.indexOffset=UINT32_MAX; assert(!valid_mesh(m,SIZE_MAX,SIZE_MAX));
    m={0,60,0,4,3}; assert(!valid_mesh(m,60,8));
    m={UINT32_MAX-3,60,0,3,3}; assert(!valid_mesh(m,UINT32_MAX,6));
    m={0,UINT32_MAX,0,3,UINT32_MAX}; assert(!valid_mesh(m,SIZE_MAX,SIZE_MAX));
    m={0,60,0,3,3}; m.vertexOffset=1; assert(!valid_mesh(m,61,6));
    m={0,60,0,3,3}; m.indexOffset=1; assert(!valid_mesh(m,60,7));
    if (argc>1) {
        std::ifstream f(argv[1],std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});
        port::directmatch::Manifest manifest;
        assert(f && port::directmatch::parse(bytes,manifest));
        const size_t expectedMeshes=manifest.meshes.size();
        port::directmatch::PreparedGpuAssets assets;
        assert(assets.prepare(std::move(manifest)));
        assert(assets.mesh_count()==expectedMeshes);
        for (const auto& slice : assets.meshes()) {
            Mesh native{slice.vertexOffset,slice.vertexBytes,slice.indexOffset,
                        slice.indexCount,slice.vertexCount};
            assert(valid_mesh(native,assets.vertices().size(),assets.indices().size()*2));
            for (uint32_t i=0;i<native.indexCount;++i)
                assert(assets.indices()[native.indexOffset/2+i]<native.vertexCount);
        }
    }
}
