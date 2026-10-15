#include "port/native_model_sources.hpp"
#include <array>
#include <cassert>
#include <cstdint>

using port::directmatch::ModelSourceRegistry;

int main() {
    ModelSourceRegistry store;
    std::array<uint16_t,6> original{8,2,7,7,2,9};
    assert(store.capture(0x1000,0x2000,2,0,3,original,false));
    original[0]=55;
    const auto* first=store.find(0x1000,0x2000,6,0,3,false);
    assert(first && first->indices[0]==8 && first->selectors.empty());
    const auto gen=first->generation;
    assert(!store.find(0x1000,0x2001,6,0,3,false));
    assert(!store.find(0x1000,0x2000,6,1,3,false));
    assert(!store.find(0x1000,0x2000,6,0,4,false));
    assert(!store.find(0x1000,0x2000,6,0,3,true));
    std::array<uint8_t,6> stitches{0,1,2,3,4,5};
    assert(!store.attach_stitch(0x1000,0x2000,stitches));

    // Recycled display-list address must discard all prior data, including PN
    // selectors; a second registration has a different generation.
    assert(store.capture(0x1000,0x3000,3,3,4,original,true));
    assert(!store.find(0x1000,0x3000,6,3,4,true));
    assert(!store.attach_stitch(0x1000,0x2000,stitches));
    assert(store.attach_stitch(0x1000,0x3000,stitches));
    auto* p=store.find(0x1000,0x3000,6,3,4,true);
    assert(p && p->generation!=gen && p->indices[0]==55);
    assert(p->selectors==std::vector<uint8_t>({3,6,9,12,15,18}));
    const auto stitchedGeneration = p->generation;
    assert(store.attach_stitch(0x1000,0x3000,stitches));
    assert(p->generation == stitchedGeneration); // identical maps keep the resident mesh
    stitches[0]=8;
    assert(p->selectors[0]==3); // no borrowed frame or stitching pointer
    assert(store.attach_stitch(0x1000,0x3000,stitches));
    assert(store.find(0x1000,0x3000,6,3,4,true)->selectors[0]==27);
    assert(p->generation != stitchedGeneration); // changed PN slots invalidate cached geometry
    const auto changedGeneration = p->generation;
    stitches[1]=9;
    assert(!store.attach_stitch(0x1000,0x3000,stitches));
    assert(store.find(0x1000,0x3000,6,3,4,true)->selectors[1]==6);
    assert(p->generation == changedGeneration); // rejected maps do not invalidate valid data
    assert(store.size()==1 && store.occupied_bytes()==18);

    const std::array<uint16_t,3> triangle{2,1,0};
    assert(store.capture(0x5000,0x6000,1,0,2,triangle,false));
    assert(store.size()==2);
    store.release_after(1);
    assert(!store.find(0x1000,0x3000,6,3,4,true));
    assert(store.find(0x5000,0x6000,3,0,2,false));
    store.release_after(0);
    assert(store.size()==0 && store.occupied_bytes()==0);
    assert(!store.capture(0,1,0,0,1,triangle,false));
    assert(!store.capture(1,0,0,0,1,triangle,false));
    assert(!store.capture(1,2,0,4,1,triangle,false));
    assert(!store.capture(1,2,0,0,0,triangle,false));
    assert(!store.capture(1,2,0,0,1,std::span(triangle).subspan(0,2),false));

    for(uintptr_t i=1;i<=ModelSourceRegistry::MaxEntries;++i)
        assert(store.capture(i,i+0x100000,1,0,1,triangle,false));
    assert(!store.capture(0xabc00000,0xabc00001,1,0,1,triangle,false));
    assert(store.size()==ModelSourceRegistry::MaxEntries);
    store.clear();
    assert(!store.size() && !store.occupied_bytes());
}
