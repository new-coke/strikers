#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "port/native_graphics.hpp"
#include <psp2/dmac5.h>
#include <psp2/kernel/sysmem.h>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

namespace port::assets {
// Explicit installation check only. Normal startup never reads the archive twice.
int verify_archive_if_requested(VerificationProgress progress) noexcept {
    const char* manifestPath=std::getenv("STRIKERS_ASSET_VERIFY_MANIFEST");
    if(!manifestPath || !*manifestPath)return 0;
    const char* path=std::getenv("STRIKERS_ASSET_ARCHIVE");
    const char* resultPath="ux0:data/strikersVita/native_asset_verification.json";
    FILE* manifest=nullptr;FILE* source=nullptr;SceUID memory=-1;
    unsigned checked=0,count=0;int error=-1;bool complete=false;char sha[65]{};
    const auto publish=[&] {
        if(progress)progress(checked,count);
        if(auto* f=std::fopen(resultPath,"w")) {
            std::fprintf(f,"{\"complete\":%s,\"verified_chunks\":%u,\"chunks\":%u,\"error\":%d,\"method\":\"native DMAC5 SHA256 per chunk\",\"expected_archive_sha256\":\"%s\"}\n",complete?"true":"false",checked,count,error,sha);
            std::fclose(f);
        }
    };
    try {
        manifest=std::fopen(manifestPath,"rb");
        source=path?std::fopen(path,"rb"):nullptr;
        if(!manifest || !source)throw 1;
        uint8_t header[64];
        if(std::fread(header,1,sizeof header,manifest)!=sizeof header || std::memcmp(header,"NSVERIFY",8) || word(header+8)!=1 || word(header+12)!=1024u*1024u)throw 2;
        const uint64_t bytes=uint64_t(word(header+16)) | uint64_t(word(header+20))<<32;
        count=word(header+24);
        if(!bytes || count!=(bytes+1024u*1024u-1)/(1024u*1024u) || count>16384 || word(header+60))throw 3;
        constexpr char digits[]="0123456789abcdef";
        for(unsigned i=0;i<32;++i){sha[2*i]=digits[header[28+i]>>4];sha[2*i+1]=digits[header[28+i]&15];}
        memory=sceKernelAllocMemBlock("strikers_asset_verify",SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW,1024u*1024u,nullptr);
        if(memory<0)throw int(memory);
        void* buffer=nullptr;error=sceKernelGetMemBlockBase(memory,&buffer);if(error<0)throw error;
        uint64_t remaining=bytes;publish();
        while(remaining) {
            const size_t n=size_t(std::min<uint64_t>(remaining,1024u*1024u));
            alignas(64) uint8_t actual[32],expected[32];
            if(std::fread(expected,1,32,manifest)!=32 || std::fread(buffer,1,n,source)!=n)throw 4;
            error=sceSblDmac5Sha256Digest(buffer,actual,n);
            if(error<0)throw error;
            if(std::memcmp(actual,expected,32))throw 5;
            remaining-=n;++checked;
            if(!(checked%16))publish();
        }
        if(std::fgetc(source)!=EOF || std::fgetc(manifest)!=EOF || std::ferror(source) || std::ferror(manifest))throw 6;
        complete=true;error=0;
    } catch(int rc) {error=rc?rc:-1;} catch(...) {error=-2;}
    if(source)std::fclose(source);
    if(manifest)std::fclose(manifest);
    if(memory>=0)sceKernelFreeMemBlock(memory);
    publish();return complete?1:-1;
}
} // namespace port::assets
#endif
