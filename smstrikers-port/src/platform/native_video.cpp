#include "port/native_video.h"
#if defined(PORT_VITA)
#include "port/native_video_record.hpp"
#include "port/asset_archive.h"
#include "NL/nlFileGC.h"
#include <psp2/videodec.h>
#include <psp2/sysmodule.h>
#include <psp2/kernel/sysmem.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
namespace {
const unsigned Pitch=704,Rows=768,Alignment=1024*1024;
struct Decoder {
    std::mutex mutex;
    SceAvcdecCtrl control{};
    SceUID reference=-1,raster=-1;
    uint8_t* pixels=nullptr;
    bool moduleOwned=false,library=false,created=false;
    std::vector<uint8_t> elementary;
    unsigned pictures=0;
    ~Decoder(){reset();}
    void reset(){
        if(created)sceAvcdecDeleteDecoder(&control);
        created=false;control={};
        if(reference>=0)sceKernelFreeMemBlock(reference);
        if(raster>=0)sceKernelFreeMemBlock(raster);
        reference=raster=-1;pixels=nullptr;
        if(library)sceVideodecTermLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC);
        library=false;
        if(moduleOwned)sceSysmoduleUnloadModule(SCE_SYSMODULE_AVCDEC);
        moduleOwned=false;
    }
    bool fail(const char* operation,int rc){
        std::fprintf(stderr,"[native-video] %s failed=0x%08x; movie skipped\n",operation,unsigned(rc));reset();return false;
    }
    bool init(){
        if(created)return true;
        int rc;
        if(sceSysmoduleIsLoaded(SCE_SYSMODULE_AVCDEC)!=0){
            rc=sceSysmoduleLoadModule(SCE_SYSMODULE_AVCDEC);if(rc<0)return fail("module",rc);moduleOwned=true;
        }
        SceVideodecQueryInitInfoHwAvcdec initInfo{sizeof(initInfo),Pitch,Rows,1,1};
        rc=sceVideodecInitLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC,&initInfo);if(rc<0)return fail("library",rc);library=true;
        SceAvcdecQueryDecoderInfo query{Pitch,Rows,1};SceAvcdecDecoderInfo info{};
        rc=sceAvcdecQueryDecoderMemSize(SCE_VIDEODEC_TYPE_HW_AVCDEC,&query,&info);
        if(rc<0||!info.frameMemSize||info.frameMemSize>32*Alignment)return fail("memory query",rc);
        const unsigned bytes=(info.frameMemSize+Alignment-1)&~(Alignment-1);
        reference=sceKernelAllocMemBlock("strikers_avc_reference",SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW,bytes,nullptr);
        if(reference<0)return fail("reference memory",reference);
        control.frameBuf.size=bytes;rc=sceKernelGetMemBlockBase(reference,&control.frameBuf.pBuf);
        if(rc<0)return fail("reference pointer",rc);
        raster=sceKernelAllocMemBlock("strikers_avc_nv12",SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW,Alignment,nullptr);
        if(raster<0)return fail("output memory",raster);
        void* base=nullptr;rc=sceKernelGetMemBlockBase(raster,&base);if(rc<0)return fail("output pointer",rc);pixels=static_cast<uint8_t*>(base);
        rc=sceAvcdecCreateDecoder(SCE_VIDEODEC_TYPE_HW_AVCDEC,&control,&query);if(rc<0)return fail("create decoder",rc);created=true;
        std::fprintf(stderr,"[native-video] hardware initialized reference_bytes=%u output_bytes=%u\n",bytes,Alignment);return true;
    }
} decoder;
class NativeMovieFile : public GCFile {
    PortAssetArchive* archive;unsigned entry,size;int status=DVD_STATE_END;
public:
    NativeMovieFile(PortAssetArchive* a,unsigned e,unsigned n):archive(a),entry(e),size(n){}
    ~NativeMovieFile(){port_asset_archive_close(archive);}
    u32 FileSize(unsigned int* p) override{if(p)*p=size;return size;}
    s32 GetReadStatus() override{return status;}
    void ReadAsync(void* target,unsigned long bytes,unsigned long offset) override{
        status=port_asset_archive_read(archive,entry,target,bytes,offset)==long(bytes)?DVD_STATE_END:DVD_STATE_FATAL_ERROR;
    }
    u32 GetDiscPosition() override{return 0;}
};
}
extern "C" int PortNativeVideoEnabled(){const char* flag=std::getenv("STRIKERS_VITA_NATIVE_VIDEO");return flag&&!std::strcmp(flag,"1");}
extern "C" void PortNativeVideoReset(){std::lock_guard<std::mutex> lock(decoder.mutex);decoder.reset();}
extern "C" int PortNativeVideoDecode(const void* data,size_t bytes,void* y,void* u,void* v){
    port::NativeVideoComponent component;if(!PortNativeVideoEnabled()||!y||!u||!v||!port::parse_native_video(data,bytes,component))return -1;
    std::lock_guard<std::mutex> lock(decoder.mutex);if(!decoder.init())return -1;
    const uint8_t endSequence[]={0,0,0,1,10,0x80};
    const size_t esBytes=component.bytes+sizeof endSequence;
    decoder.elementary.resize(esBytes+64);std::memcpy(decoder.elementary.data(),component.elementary,component.bytes);
    std::memcpy(decoder.elementary.data()+component.bytes,endSequence,sizeof endSequence);std::memset(decoder.elementary.data()+esBytes,0,64);
    SceAvcdecAu au{};au.pts={UINT32_MAX,UINT32_MAX};au.dts=au.pts;au.es={decoder.elementary.data(),uint32_t(esBytes)};
    SceAvcdecPicture picture{};picture.size=sizeof picture;picture.frame.pixelType=SCE_AVCDEC_PIXELFORMAT_YUV420_PACKED_RASTER;
    picture.frame.framePitch=Pitch;picture.frame.frameWidth=component.width;picture.frame.frameHeight=component.height;
    const size_t yBytes=size_t(Pitch)*component.height;
    picture.frame.pPicture[0]=decoder.pixels;picture.frame.pPicture[1]=decoder.pixels+yBytes;
    auto* pointer=&picture;SceAvcdecArrayPicture array{0,1,&pointer};
    int rc=sceAvcdecDecode(&decoder.control,&au,&array);
    // THPSimple requires a picture per call. Drain once with an EOS-only AU;
    // never publish stale pixels or attribute a delayed frame to a later one.
    if(rc>=0&&!array.numOfOutput){au.es={decoder.elementary.data()+component.bytes,sizeof endSequence};rc=sceAvcdecDecode(&decoder.control,&au,&array);}
    if(rc<0||array.numOfOutput!=1){decoder.fail("decode/picture",rc);return -1;}
    const auto& frame=picture.frame;
    if(frame.pixelType!=SCE_AVCDEC_PIXELFORMAT_YUV420_PACKED_RASTER||frame.framePitch!=Pitch||
       frame.pPicture[0]!=decoder.pixels||frame.pPicture[1]!=decoder.pixels+yBytes||frame.horizontalSize!=component.width||
       frame.verticalSize!=component.height||frame.frameCropLeftOffset||frame.frameCropRightOffset||frame.frameCropTopOffset||frame.frameCropBottomOffset){
        decoder.fail("output geometry",-1);return -1;
    }
    if(!port::tile_native_nv12(decoder.pixels,yBytes,decoder.pixels+yBytes,yBytes/2,Pitch,component.width,component.height,y,u,v))return -1;
    if(++decoder.pictures==1||!(decoder.pictures%300))std::fprintf(stderr,"[native-video] hardware pictures=%u size=%ux%u\n",decoder.pictures,component.width,component.height);
    return 0;
}
nlFile* PortOpenNativeMovie(const char* source){
    if(!PortNativeVideoEnabled()||!source)return nullptr;
    while(*source=='/'||*source=='\\')++source;
    if(!*source||std::strstr(source,"..")||std::strchr(source,':')||std::strchr(source,'\\'))return nullptr;
    const char* path=std::getenv("STRIKERS_ASSET_ARCHIVE");if(!path||!*path)return nullptr;
    char error[256];auto* archive=port_asset_archive_open(path,error,sizeof error);if(!archive)return nullptr;
    std::string name="files/";name+=source;const int entry=port_asset_archive_find(archive,name.c_str());
    const uint64_t size=entry<0?0:port_asset_archive_size(archive,entry);
    uint8_t header[80];bool valid=size>=80&&size<=256*1024*1024&&!(size%32)&&
        port_asset_archive_read(archive,entry,header,sizeof header,0)==sizeof header&&
        !std::memcmp(header,"THP\0",4)&&port::video_word(header+4)==0x11000&&port::video_word(header+20)>0&&
        port::video_word(header+40)<size&&port::video_word(header+24)<=size-port::video_word(header+40);
    // Verify the first independently decodable component before selecting this file.
    std::vector<uint8_t> first;
    if(valid){const unsigned frameAt=port::video_word(header+40),frameSize=port::video_word(header+24);
        uint8_t frame[16];valid=frameSize>=16&&port_asset_archive_read(archive,entry,frame,sizeof frame,frameAt)==sizeof frame;
        const unsigned videoSize=valid?port::video_word(frame+8):0;
        uint8_t info[28];const unsigned infoAt=port::video_word(header+32);
        valid=valid&&infoAt>=48&&infoAt<=frameAt&&frameAt-infoAt>=28&&
            port_asset_archive_read(archive,entry,info,sizeof info,infoAt)==sizeof info;
        const unsigned components=valid?port::video_word(info):0;
        valid=valid&&components>=1&&components<=2&&info[4]==0&&(components==1||info[5]==1)&&
            videoSize<=1024*1024&&videoSize>=24&&videoSize<=frameSize-8-4*components;
        if(valid){first.resize(videoSize);port::NativeVideoComponent parsed;
            valid=port_asset_archive_read(archive,entry,first.data(),first.size(),frameAt+8+4*components)==long(first.size())&&
                port::parse_native_video(first.data(),first.size(),parsed)&&parsed.width==port::video_word(info+20)&&parsed.height==port::video_word(info+24);}
    }
    if(!valid){port_asset_archive_close(archive);return nullptr;}
    std::fprintf(stderr,"[native-video] selected %s bytes=%u\n",source,unsigned(size));return new NativeMovieFile(archive,entry,size);
}
#else
extern "C" int PortNativeVideoEnabled(){return 0;}
extern "C" void PortNativeVideoReset(){}
extern "C" int PortNativeVideoDecode(const void*,size_t,void*,void*,void*){return -1;}
nlFile* PortOpenNativeMovie(const char*){return nullptr;}
#endif
