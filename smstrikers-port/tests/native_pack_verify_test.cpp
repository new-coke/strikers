#include "port/native_graphics.hpp"
#include <psp2/dmac5.h>
#include <psp2/kernel/sysmem.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <unistd.h>

namespace fs=std::filesystem;
namespace {
void* block=nullptr;int failure=0;unsigned calls=0;
constexpr uint8_t abcHash[]{0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
void save(const char* name,const std::vector<uint8_t>& b) {std::ofstream f(name,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());assert(f);}
std::string result() {std::ifstream f("ux0:data/strikersVita/native_asset_verification.json");return {(std::istreambuf_iterator<char>(f)),{}};}
void set(std::vector<uint8_t>& b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b.at(at+i)=uint8_t(n>>(8*i));}
}
extern "C" {
SceUID sceKernelAllocMemBlock(const char*,SceKernelMemBlockType type,SceSize size,SceKernelAllocMemBlockOpt*) {
    assert(!block && type==SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW && size==1024*1024);
    assert(!posix_memalign(&block,64,size));return 99;
}
int sceKernelGetMemBlockBase(SceUID id,void** out){assert(id==99 && block);*out=block;return 0;}
int sceKernelFreeMemBlock(SceUID id){assert(id==99 && block);std::free(block);block=nullptr;return 0;}
int sceSblDmac5HashTransform(SceSblDmac5HashTransformParam* p,SceUInt32 command,SceUInt32 extra) {
    assert(command==(3|0x10) && extra==0x800 && p->src==block && p->length==3);
    auto* context=static_cast<SceSblDmac5HashTransformContext*>(p->ctx);
    assert(context->state[0]==__builtin_bswap32(0x6a09e667u) && !context->length);++calls;
    if(failure)return failure;
    if(!std::memcmp(p->src,"abc",3))std::memcpy(p->dst,abcHash,32);
    else std::memset(p->dst,0,32);
    return 0;
}
}
int main() {
    char tmp[]="/tmp/strikers-native-verify-XXXXXX";assert(mkdtemp(tmp));const auto original=fs::current_path();fs::current_path(tmp);
    fs::create_directories("ux0:data/strikersVita");unsetenv("STRIKERS_ASSET_VERIFY_MANIFEST");
    assert(port::assets::verify_archive_if_requested()==0 && !block);
    std::vector<uint8_t> manifest(96);std::memcpy(manifest.data(),"NSVERIFY",8);
    set(manifest,8,1);set(manifest,12,1024*1024);set(manifest,16,3);set(manifest,24,1);
    std::memcpy(manifest.data()+28,abcHash,32);std::memcpy(manifest.data()+64,abcHash,32);
    save("manifest",manifest);save("source",{'a','b','c'});
    setenv("STRIKERS_ASSET_VERIFY_MANIFEST","manifest",1);setenv("STRIKERS_ASSET_ARCHIVE","source",1);
    assert(port::assets::verify_archive_if_requested()==1 && !block && calls==1);
    assert(result().find("\"complete\":true")!=std::string::npos);
    save("source",{'a','b','d'});assert(port::assets::verify_archive_if_requested()==-1 && !block);
    assert(result().find("\"error\":5")!=std::string::npos);
    save("source",{'a','b','c','x'});assert(port::assets::verify_archive_if_requested()==-1 && !block);
    assert(result().find("\"error\":6")!=std::string::npos);
    save("source",{'a','b'});assert(port::assets::verify_archive_if_requested()==-1 && !block);
    save("source",{'a','b','c'});failure=-55;assert(port::assets::verify_archive_if_requested()==-1 && !block);
    assert(result().find("\"error\":-55")!=std::string::npos);failure=0;
    manifest.resize(95);save("manifest",manifest);assert(port::assets::verify_archive_if_requested()==-1 && !block);
    unsetenv("STRIKERS_ASSET_VERIFY_MANIFEST");unsetenv("STRIKERS_ASSET_ARCHIVE");fs::current_path(original);fs::remove_all(tmp);
    std::puts("Native archive verifier: default-off, expected SHA256 chunks, SDK failure, truncation, mismatch and cleanup passed");
}
