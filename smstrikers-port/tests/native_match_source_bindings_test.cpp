#include "port/native_match_source_bindings.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

using namespace port::directmatch;

static Manifest make_catalog() {
    Manifest m{};
    for (unsigned n=0;n<3;++n) {
        Mesh mesh{};
        mesh.source = {3,0,6,36,12, n==2?0x44ull:0x33ull,
                       static_cast<uint64_t>(n+1), 0};
        mesh.vertices.resize(3*kVertexStride, static_cast<uint8_t>(n));
        mesh.indices={0,1,2};
        m.meshes.push_back(std::move(mesh));
    }
    return m;
}
static SourceLocation source_for(uintptr_t handle, unsigned id) {
    (void)id;
    SourceLocation s{};
    s.displayList=handle;
    s.listData=handle+0x400;
    s.position=handle+0x800;
    s.color=handle+0xc00;
    s.listBytes=32;
    s.positionBytes=36;
    s.colorBytes=12;
    s.drawVertices=3;
    s.primitive=0;
    s.streamCount=2;
    return s;
}

static void lifecycle_contract() {
    Manifest original=make_catalog();
    auto keys=std::vector<SourceIdentity>{
        original.meshes[0].source, original.meshes[1].source, original.meshes[2].source};
    PreparedGpuAssets assets{};
    assert(assets.prepare(std::move(original)));
    NativeSourceBindings registry{};
    const auto a=source_for(0x1000,0);
    const auto b=source_for(0x2000,1);
    assert(registry.register_source(a,keys[0],1,5,6,7,8));
    assert(registry.register_source(b,keys[1],2,5,6,7,8));
    assert(registry.size()==2);
    assert(registry.lookup(a.displayList,a)->meshIndex==kNoNativeMesh);

    registry.resolve(assets);
    auto* first=registry.lookup(a.displayList,a);
    auto* second=registry.lookup(b.displayList,b);
    assert(first && second);
    assert(first->meshIndex==0 && second->meshIndex==1);
    assert(first->generation!=second->generation);
    assert(first->positionRevision==6 && first->validatedEpoch==8);
    const uint64_t oldGeneration=first->generation;
    const auto other=source_for(0x3000,2);
    assert(!registry.lookup(other.displayList,other));

    auto changed=a;
    changed.positionBytes+=4;
    assert(!registry.lookup(a.displayList,changed));
    assert(!registry.lookup(a.displayList,a)); // Permanently invalidated.
    registry.release_after(1);
    assert(!registry.lookup(b.displayList,b));
    assert(registry.size()==1);
    assert(registry.lookup(a.displayList,a)==nullptr);

    // Pointer reuse receives a new generation and a fresh mesh ID.
    assert(registry.register_source(a,keys[2],1,21,22,23,24,&assets));
    assert(registry.lookup(a.displayList,a)->generation>oldGeneration);
    assert(registry.lookup(a.displayList,a)->meshIndex==2);
    assert(registry.lookup(a.displayList,a)->validatedEpoch==24);
    registry.invalidate(a.displayList);
    assert(!registry.lookup(a.displayList,a));
    assert(registry.register_source(a,keys[0],0,30,30,30,30,&assets));
    registry.release_after(0);
    assert(registry.lookup(a.displayList,a));
    assert(registry.lookup(a.displayList,a)->meshIndex==0);
    registry.clear();
    assert(registry.size()==0);
}

static void revision_contract() {
    PreparedGpuAssets assets{};
    assert(assets.prepare(make_catalog()));
    NativeSourceBindings registry{};
    const auto a=source_for(0x5000,0);
    assert(registry.register_source(a,assets.meshes()[0].source,1,
                                    10,20,30,100,&assets));
    auto* entry=registry.lookup(a.displayList,a);
    assert(entry && entry->meshIndex==0);
    uint64_t epoch=100;
    bool rangeMatches=true,contentMatches=true;
    unsigned rangeCalls=0,hashCalls=0;
    auto readEpoch=[&]() noexcept {return epoch;};
    auto ranges=[&](const NativeSourceBinding&) noexcept {
        ++rangeCalls;
        return rangeMatches;
    };
    auto rehash=[&](NativeSourceBinding& binding) noexcept {
        ++hashCalls;
        if(contentMatches) {
            binding.listRevision=epoch;
            binding.positionRevision=epoch;
            binding.colorRevision=epoch;
        }
        return contentMatches;
    };
    for (int i=0;i<500;++i)
        assert(validate_source_revisions(*entry,readEpoch,ranges,rehash));
    assert(rangeCalls==0 && hashCalls==0); // Draw hot path: no scans.

    ++epoch;
    assert(validate_source_revisions(*entry,readEpoch,ranges,rehash));
    assert(rangeCalls==1 && hashCalls==0 && entry->validatedEpoch==epoch);
    rangeMatches=false;
    ++epoch;
    assert(validate_source_revisions(*entry,readEpoch,ranges,rehash));
    assert(hashCalls==1 && entry->validatedEpoch==epoch);

    // An epoch racing validation retries without admitting a stale draw.
    ++epoch;
    auto racingEpoch=[&]() noexcept {
        const auto sampled=epoch;
        if(sampled==103)++epoch;
        return sampled;
    };
    assert(!validate_source_revisions(*entry,racingEpoch,ranges,rehash));
    assert(!entry->invalid);
    assert(validate_source_revisions(*entry,readEpoch,ranges,rehash));

    ++epoch;
    contentMatches=false;
    assert(!validate_source_revisions(*entry,readEpoch,ranges,rehash));
    assert(entry->invalid && entry->meshIndex==kNoNativeMesh);
    assert(!registry.lookup(a.displayList,a));
    const unsigned priorCalls=hashCalls;
    ++epoch;
    assert(!validate_source_revisions(*entry,readEpoch,ranges,rehash));
    assert(hashCalls==priorCalls);
}

static void real_catalog_contract(const char* path) {
    std::ifstream input(path,std::ios::binary);
    assert(input);
    const std::vector<uint8_t> bytes{
        std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    Manifest manifest{};
    assert(parse(bytes,manifest));
    PreparedGpuAssets assets{};
    assert(assets.prepare(std::move(manifest)));
    NativeSourceBindings registry{};
    constexpr uintptr_t base=0x10000;
    for (uint32_t i=0;i<assets.mesh_count();++i) {
        const auto src=source_for(base+uintptr_t(i)*0x10000,i);
        assert(registry.register_source(src,assets.meshes()[i].source,2,
                                        i+1,i+2,i+3,42));
    }
    assert(registry.size()==assets.mesh_count());
    registry.resolve(assets);
    for (uint32_t i=0;i<assets.mesh_count();++i) {
        const auto src=source_for(base+uintptr_t(i)*0x10000,i);
        const auto* entry=registry.lookup(src.displayList,src);
        assert(entry && entry->meshIndex==i);
    }
    // The source lookup must remain intact once GPU uploads have completed
    // and all packed CPU geometry bytes have been freed.
    assets.release_upload_bytes();
    assert(assets.upload_bytes()==0);
    for (uint32_t i=0;i<assets.mesh_count();++i) {
        const auto src=source_for(base+uintptr_t(i)*0x10000,i);
        assert(registry.lookup(src.displayList,src)->meshIndex==i);
    }
    registry.release_after(1);
    assert(registry.size()==0);
    std::printf("native source lifetime PASS: %zu prebound real meshes\n",
                assets.mesh_count());
}

int main(int argc,char** argv) {
    lifecycle_contract();
    revision_contract();
    if(argc>1)real_catalog_contract(argv[1]);
    std::puts("native source binding generation/ABA contract PASS");
    return 0;
}
