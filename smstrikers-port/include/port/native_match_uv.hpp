#pragma once
#include "port/native_match_gpu_assets.hpp"
#include <array>
#include <cmath>
#include <span>

namespace port::directmatch {
struct UvAssets {
    std::vector<uint8_t> vertices{};
    std::vector<uint8_t> hasUv{};
};
inline bool parse_uv_assets(std::span<const uint8_t> bytes,uint64_t meshPayloadHash,
                           std::span<const GpuMeshSlice> meshes,UvAssets& out) {
    out={};
    if(bytes.size()<32||bytes.size()>8u*1024u*1024u||meshes.empty()||meshes.size()>kMaxMeshes||
       std::memcmp(bytes.data(),"SMNVUV01",8))return false;
    Reader r{bytes.data(),bytes.size(),8};
    uint32_t version=0,count=0;uint64_t hash=0,parent=0;
    if(!r.u32(version)||!r.u32(count)||!r.u64(hash)||!r.u64(parent)||version!=1||
       count!=meshes.size()||parent!=meshPayloadHash||fnv64(bytes.data()+32,bytes.size()-32)!=hash)return false;
    UvAssets candidate;
    candidate.hasUv.reserve(count);
    for(const auto& mesh : meshes) {
        uint32_t vertices=0,flag=0;uint64_t raw=0,pos=0,col=0;
        if(!r.u32(vertices)||!r.u32(flag)||!r.u64(raw)||!r.u64(pos)||!r.u64(col)||
           vertices!=mesh.vertexCount||flag>1||raw!=mesh.source.rawHash||
           pos!=mesh.source.positionHash||col!=mesh.source.colorHash)return false;
        const size_t n=size_t(vertices)*12u;
        if(r.offset>bytes.size()||n>bytes.size()-r.offset)return false;
        for(size_t i=0;i<n;i+=4) {
            float value=0;std::memcpy(&value,bytes.data()+r.offset+i,4);
            if(!std::isfinite(value))return false;
        }
        candidate.vertices.insert(candidate.vertices.end(),bytes.begin()+r.offset,bytes.begin()+r.offset+n);
        r.offset+=n;candidate.hasUv.push_back(uint8_t(flag));
    }
    if(r.offset!=bytes.size())return false;
    out=std::move(candidate);
    return true;
}
} // namespace port::directmatch
