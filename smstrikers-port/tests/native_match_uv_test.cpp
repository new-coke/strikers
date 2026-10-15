#include "port/native_match_uv.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <limits>
using namespace port::directmatch;
static void put(std::vector<uint8_t>& b,uint32_t v) {for(unsigned n=0;n<4;++n)b.push_back(uint8_t(v>>(8*n)));}
static void put64(std::vector<uint8_t>& b,uint64_t v) {put(b,uint32_t(v));put(b,uint32_t(v>>32));}
int main(int argc,char** argv) {
    GpuMeshSlice mesh{};mesh.vertexCount=3;mesh.source.rawHash=1;mesh.source.positionHash=2;mesh.source.colorHash=3;
    std::vector<uint8_t> payload;put(payload,3);put(payload,1);put64(payload,1);put64(payload,2);put64(payload,3);
    const float coordinates[9]{0,0,1,1,0,1,0,1,1};
    const auto* raw=reinterpret_cast<const uint8_t*>(coordinates);payload.insert(payload.end(),raw,raw+sizeof(coordinates));
    std::vector<uint8_t> data{'S','M','N','V','U','V','0','1'};
    put(data,1);put(data,1);put64(data,fnv64(payload.data(),payload.size()));put64(data,77);
    data.insert(data.end(),payload.begin(),payload.end());
    const std::array<GpuMeshSlice,1> meshes{mesh};UvAssets uv;
    assert(parse_uv_assets(data,77,meshes,uv));assert(uv.vertices.size()==36&&uv.hasUv==std::vector<uint8_t>{1});
    assert(!parse_uv_assets(data,78,meshes,uv)&&uv.vertices.empty());
    for(size_t n=0;n<data.size();++n) {auto corrupt=data;corrupt[n]^=1;assert(!parse_uv_assets(corrupt,77,meshes,uv));}
    auto other=meshes;other[0].source.positionHash=4;assert(!parse_uv_assets(data,77,other,uv));
    if(argc>2) {
        std::ifstream mf(argv[1],std::ios::binary),uf(argv[2],std::ios::binary);
        std::vector<uint8_t> master((std::istreambuf_iterator<char>(mf)),{}),bytes((std::istreambuf_iterator<char>(uf)),{});
        Manifest manifest;assert(mf&&uf&&parse(master,manifest));
        PreparedGpuAssets assets;assert(assets.prepare(std::move(manifest)));
        Reader header{master.data(),master.size(),16};uint64_t parent=0;assert(header.u64(parent));
        assert(parse_uv_assets(bytes,parent,assets.meshes(),uv));
        assert(uv.hasUv.size()==assets.mesh_count()&&uv.vertices.size()==assets.vertices().size()/20*12);
        for(auto flag : uv.hasUv)assert(flag==1);
    }
}
