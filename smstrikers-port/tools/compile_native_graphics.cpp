#include "port/native_graphics.hpp"
#include "port/vitagl_texture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace fs=std::filesystem;
using namespace port;
static std::vector<uint8_t> read(const fs::path& p) {
    if(!fs::is_regular_file(p) || fs::file_size(p)>32u*1024u*1024u)throw std::runtime_error("input extent");
    std::ifstream f(p,std::ios::binary);std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)),{});
    if(b.size()!=fs::file_size(p))throw std::runtime_error("input read");return b;
}
static void set(std::vector<uint8_t>& b,size_t at,uint32_t value) {
    for(unsigned i=0;i<4;++i)b.at(at+i)=uint8_t(value>>(8*i));
}
static std::vector<uint8_t> compile(std::span<const uint8_t> b) {
    if(b.size()<48 || std::memcmp(b.data(),"NSTREQ01",8))throw std::runtime_error("texture request header");
    const size_t pixels=assets::word(b.data()+40),palette=assets::word(b.data()+44);
    if(pixels>b.size()-48 || palette!=b.size()-48-pixels || assets::word(b.data()+8)>8 || assets::word(b.data()+16)>1)
        throw std::runtime_error("texture request extent");
    nativegxm::TextureSource source;
    source.width=assets::word(b.data()+20);source.height=assets::word(b.data()+24);source.mipCount=assets::word(b.data()+28);
    source.encoding=nativegxm::TextureEncoding(assets::word(b.data()+32));
    source.paletteEncoding=nativegxm::PaletteEncoding(assets::word(b.data()+36));
    source.pixels=b.subspan(48,pixels);source.palette=b.subspan(48+pixels,palette);
    std::vector<uint8_t> checked;
    nativegxm::TextureUpload decoded;
    if(!nativegxm::make_texture_request(source,checked) || !nativegxm::decode_texture(source,decoded))
        throw std::runtime_error("texture decode");
    nativegxm::TextureUpload linear=decoded;linear.swizzled=false;linear.stride=(source.width+7u)&~7u;
    const size_t n=nativegxm::texture_upload_bytes(linear);
    if(!n)throw std::runtime_error("texture output extent");
    std::vector<uint8_t> out(64+n);std::memcpy(out.data(),"NSTX0001",8);
    set(out,8,out.size());set(out,12,source.width);set(out,16,source.height);set(out,20,source.mipCount);
    set(out,24,linear.stride);set(out,28,assets::word(b.data()+8));std::memcpy(out.data()+32,b.data()+12,4);
    set(out,36,assets::word(b.data()+16));set(out,40,n);
    size_t at=64;
    for(unsigned level=0;level<source.mipCount;++level) {
        render::TextureLevel image;
        if(!render::texture_level_rgba(decoded,level,image))throw std::runtime_error("texture mip");
        const size_t row=((image.width+7u)&~7u)*4u;
        for(unsigned y=0;y<image.height;++y)
            std::memcpy(out.data()+at+y*row,image.rgba.data()+size_t(y)*image.width*4,size_t(image.width)*4);
        at+=row*image.height;
    }
    const uint64_t hash=nativegxm::native_asset_hash(std::span(out).subspan(64));
    set(out,48,uint32_t(hash));set(out,52,uint32_t(hash>>32));
    nativegxm::TextureUpload verified;
    if(!assets::texture(out,verified))throw std::runtime_error("native texture validation");
    for(unsigned level=0;level<source.mipCount;++level) {
        render::TextureLevel before,after;
        if(!render::texture_level_rgba(decoded,level,before) || !render::texture_level_rgba(verified,level,after) ||
           before.width!=after.width || before.height!=after.height || before.rgba!=after.rgba)
            throw std::runtime_error("native texture changed decoded pixels");
    }
    return out;
}
int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::runtime_error("usage: strikers_compile_native_graphics REQUESTS OUTPUT, or --validate DIRECTORY");
        size_t count=0;
        if(std::string_view(argv[1])=="--validate") {
            for(const auto& entry:fs::recursive_directory_iterator(argv[2])) {
                if(!entry.is_regular_file())continue;
                const auto ext=entry.path().extension().string();
                if(ext!=".glg" && ext!=".glt" && ext!=".nstx")continue;
                const auto b=read(entry.path());bool ok=false;
                if(ext==".glg")ok=assets::validate_model(b);
                else if(ext==".glt")ok=assets::texture_bundle(b);
                else {nativegxm::TextureUpload image;ok=assets::texture(b,image);}
                if(!ok)throw std::runtime_error("invalid native graphics: "+entry.path().string());
                ++count;
            }
        } else {
            fs::create_directories(argv[2]);
            for(const auto& entry:fs::directory_iterator(argv[1])) {
                if(!entry.is_regular_file() || entry.path().extension()!=".txrq")continue;
                const auto out=compile(read(entry.path()));
                const auto path=fs::path(argv[2])/(entry.path().stem().string()+".nstx");
                std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(out.data()),out.size());
                if(!f)throw std::runtime_error("native texture write");++count;
            }
        }
        std::cout<<"{\"processed\":"<<count<<"}\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
