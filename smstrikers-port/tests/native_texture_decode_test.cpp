#include "port/native_texture_record.hpp"
#include "port/asset_archive.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string_view>
using namespace port::nativegxm;
uint32_t word(std::span<const uint8_t> b,unsigned offset){return uint32_t(b[offset])|(uint32_t(b[offset+1])<<8)|(uint32_t(b[offset+2])<<16)|(uint32_t(b[offset+3])<<24);}
int main(int argc,char** argv){
 TextureSource s;s.width=s.height=4;s.encoding=TextureEncoding::Rgba8;std::vector<uint8_t> encoded(64);
 for(unsigned i=0;i<16;++i){encoded[2*i]=uint8_t(255-i);encoded[2*i+1]=uint8_t(i);encoded[32+2*i]=uint8_t(i+16);encoded[33+2*i]=uint8_t(i+32);}s.pixels=encoded;TextureUpload image;assert(decode_texture(s,image));
 // Independently ordered Morton coordinates for a 4x4 GXM image.
 constexpr unsigned order[]{0,4,1,5,8,12,9,13,2,6,3,7,10,14,11,15};
 for(unsigned i=0;i<16;++i){unsigned original=order[i];assert(image.pixels[i*4]==original&&image.pixels[i*4+1]==original+16&&image.pixels[i*4+2]==original+32&&image.pixels[i*4+3]==255-original);}
 s.encoding=TextureEncoding::Rgb5a3;encoded.resize(32);std::fill(encoded.begin(),encoded.end(),0);encoded[0]=0xFC;encoded[1]=0;s.pixels=encoded;assert(decode_texture(s,image));assert(image.pixels[0]==255&&image.pixels[1]==0&&image.pixels[2]==0&&image.pixels[3]==255);
 s.encoding=TextureEncoding::C8;encoded.resize(32);encoded.assign(32,0);s.pixels=encoded;std::vector<uint8_t> palette{0xFC,0};s.palette=palette;s.paletteEncoding=PaletteEncoding::Rgb5a3;assert(decode_texture(s,image)&&image.pixels[0]==255);encoded[0]=1;s.pixels=encoded;assert(!decode_texture(s,image)&&image.pixels.empty());
 s.palette={};s.paletteEncoding=PaletteEncoding::None;s.encoding=TextureEncoding::Cmpr;s.width=s.height=8;encoded.assign(32,0);for(unsigned i=0;i<4;++i){encoded[i*8]=0xF8;encoded[i*8+2]=0x07;encoded[i*8+3]=0xE0;}s.pixels=encoded;assert(decode_texture(s,image));for(unsigned i=0;i<64;++i)assert(image.pixels[i*4]==255&&image.pixels[i*4+3]==255);
 if(argc<2)return 0;
 char error[256];auto* archive=port_asset_archive_open(argv[1],error,sizeof error);if(!archive){std::fprintf(stderr,"%s\n",error);return 1;}
 unsigned records=0,matched=0,bc1=0;
 for(unsigned i=0;i<port_asset_archive_count(archive);++i){const char* path=port_asset_archive_path(archive,i);if(!std::string_view(path).starts_with("native/v1/textures/"))continue;
  std::vector<uint8_t> record(port_asset_archive_size(archive,i));assert(port_asset_archive_read(archive,i,record.data(),record.size(),0)==long(record.size()));const auto request=std::span(record).subspan(28,word(record,12));TextureUpload expected;assert(parse_texture_record(record,request,expected));
  TextureSource source;source.width=word(request,4);source.height=word(request,8);source.encoding=TextureEncoding(word(request,12));source.paletteEncoding=PaletteEncoding(word(request,16));source.mipCount=word(request,20);source.pixels=request.subspan(36,word(request,28));source.palette=request.subspan(36+word(request,28),word(request,32));
  TextureUpload decoded;assert(decode_texture(source,decoded));++records;
  if(expected.storage==TextureStorage::Rgba8){assert(decoded.pixels.size()==expected.pixels.size());if(decoded.pixels!=expected.pixels){std::fprintf(stderr,"Mismatch %s format=%u %ux%u swizzled=%u\n",path,unsigned(source.encoding),source.width,source.height,expected.swizzled);for(size_t k=0,n=0;k<decoded.pixels.size()&&n<12;++k)if(decoded.pixels[k]!=expected.pixels[k]){std::fprintf(stderr,"byte %zu decoded=%u expected=%u\n",k,decoded.pixels[k],expected.pixels[k]);++n;}return 2;}++matched;}else {assert(expected.storage==TextureStorage::Bc1);++bc1;}
 }
 port_asset_archive_close(archive);std::printf("native decoder: %u real records decoded; %u byte-identical RGBA; %u BC1 validated and decoded\n",records,matched,bc1);assert(records&&matched);
}
