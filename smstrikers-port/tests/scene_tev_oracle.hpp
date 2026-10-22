// Host-only Cg adapters and a TEV stage interpreter. The test generator appends
// glx_SwitchTexConfig verbatim, followed by the production fragment shader.
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>
#include "port/scene_material_alpha.hpp"

using float2 = float __attribute__((ext_vector_type(2)));
using float3 = float __attribute__((ext_vector_type(3)));
using float4 = float __attribute__((ext_vector_type(4)));
float2 make2(float x, float y) { return {x, y}; }
float3 make3(float x, float y, float z) { return {x, y, z}; }
float4 make4(float x, float y, float z, float w) { return {x, y, z, w}; }
float4 make4(float3 xyz, float w) { return {xyz.x, xyz.y, xyz.z, w}; }
float saturate(float x) { return std::clamp(x, 0.f, 1.f); }
float3 saturate(float3 v) { return {saturate(v.x), saturate(v.y), saturate(v.z)}; }
float4 saturate(float4 v) { return {saturate(v.x), saturate(v.y), saturate(v.z), saturate(v.w)}; }
template<class V> V lerp(V a, V b, float c) { return a * (1.f - c) + b * c; }
using std::abs;
using std::exp2;
using std::floor;
using std::fmod;
using std::max;
struct sampler2D { float4 value; };
float4 tex2D(sampler2D sampler, float2) { return sampler.value; }
struct Discard {};

using u32 = uint32_t;
using s16 = int16_t;
using GXAttr = int;
using GXTevStageID = int;
using GXTexCoordID = int;
using GXTexMapID = int;
using GXChannelID = int;
using GXTevColorArg = int;
using GXTevKColorSel = int;
using GXTevKAlphaSel = int;
using GXTevRegID = int;
using _GXTevAlphaArg = int;
using _GXTevOp = int;
using _GXTevBias = int;
using _GXTevScale = int;
using _GXTevRegID = int;
using _GXTexGenType = int;
using _GXTexGenSrc = int;
using _GXVtxFmt = int;
constexpr int GX_VA_NULL = 0xff;
struct GXColorS10 { s16 r, g, b, a; };
struct glModelPacket { struct { unsigned texconfig; unsigned long program; } state; };
unsigned glx_texconfig, glx_NumIndices;
int glx_normals, glx_allowSpecular, gx_vtxfmt, gx_texattr[6];
int glx_RasterizedAlphaStage, glx_RasterizedAlphaArg, glx_GlossMapStage, glx_GlossMapCoord;
constexpr unsigned glv_MatrixChanged = 1;
constexpr int prog_2d_movie = 1, prog_3d_unlit_2x = 2, prog_3d_crowd = 3,
              prog_3d_crowd_lit = 4, prog_3d_pointlit_dirt = 5, prog_3d_pointlit = 6;
constexpr int GX_TEVSTAGE1 = 1, GX_TEV_KASEL_1 = 0;
int glx_program = 0, glx_tevscale = 0;
bool glx_norasterizedalpha = false, glx_translucent = false, glx_constantcolour = false;
// Draw-only alpha overrides are evaluated on a copy of the stage array below;
// their matching restore callbacks therefore have no persistent work here.
void EnableNoRasterizedAlpha(bool) {}
void EnableTranslucent(bool) {}
struct TevOperation { int op = 0, bias = 0, scale = 0, reg = 0; bool clamp = true; };
struct TevStage
{
    int coord = 0xff, map = 0xff, channel = 0xff;
    // GXInit clears the cached input fields, then initializes stage zero only.
    std::array<int,4> color{0,0,0,0}, alpha{0,0,0,0};
    TevOperation rgb, a;
    int kcolor = 0, kalpha = 0;
    bool alphaWritten = false;
};
std::array<TevStage,16> tev;
int tevCount = 0;
void GXSetTevOrder(int s, int c, int m, int r) { tev[s].coord=c; tev[s].map=m; tev[s].channel=r; }
void GXSetTevColorIn(int s, int a, int b, int c, int d) { tev[s].color={a,b,c,d}; }
void gxSetTevAlphaIn(int s, int a, int b, int c, int d) { tev[s].alpha={a,b,c,d}; tev[s].alphaWritten=true; }
void GXSetTevKColorSel(int s, int k) { tev[s].kcolor=k; }
void GXSetTevKAlphaSel(int s, int k) { tev[s].kalpha=k; }
void gxSetTevColourOp(int s,int op,int bias,int scale,bool clamp,int reg) { tev[s].rgb={op,bias,scale,reg,clamp}; }
void gxSetTevAlphaOp(int s,int op,int bias,int scale,bool clamp,int reg) { tev[s].a={op,bias,scale,reg,clamp}; }
void gxSetNumTevStages(int count) { tevCount=count; }
void gxSetNumTexGens(int) {}
void gxSetTexCoordGen(int,int,int,u32) {}
void GXSetTevColorS10(int,GXColorS10) {}

float constant(int selection, const std::array<float,4>& k, bool alpha)
{
    const int first=alpha?0x1c:0x0c;
    if(selection>=first&&selection<first+4) return k[selection-first];
    assert(selection==0); // GX_TEV_KCSEL_1 / GX_TEV_KASEL_1
    return 1.f;
}
float apply(float a,float b,float c,float d,TevOperation op)
{
    assert(op.op==0||op.op==1);
    float v=d+(op.op==1?-1.f:1.f)*(a*(1.f-c)+b*c);
    v+=op.bias==1?.5f:op.bias==2?-.5f:0.f;
    v*=op.scale==3?.5f:float(1u<<op.scale);
    return op.clamp?saturate(v):v;
}
float4 original_tev(const std::array<sampler2D,6>& textures,float4 raster,float4 spec,
                    const std::array<float,4>& k,float4 tint,float overrideAlpha,bool constantColor,
                    bool noRasterAlpha,bool environmentDiffuse)
{
    // GLX compacts semantic texture bits into consecutive GX slots.
    std::array<float4,6> samples{};
    unsigned slot=0;
    for(unsigned semantic=0;semantic<6;++semantic)
        if(glx_texconfig&(1u<<semantic)) samples[slot++]=textures[semantic].value;
    auto stages=tev;
    // glud_EnvDiffuse(true) selects channel 0 alpha for the gloss stage and
    // replaces its RGB inputs before the per-draw constant/alpha overrides.
    if(environmentDiffuse&&glx_GlossMapStage>=0)
    {
        auto& stage=stages[glx_GlossMapStage];
        stage.channel=4;
        stage.color={15,11,8,(glx_texconfig&32)?15:0};
    }
    if((overrideAlpha>=0||noRasterAlpha)&&glx_RasterizedAlphaStage>=0)
        stages[glx_RasterizedAlphaStage].alpha[glx_RasterizedAlphaArg]=3;
    else if(overrideAlpha<0&&!noRasterAlpha&&constantColor)
    {
        if(glx_texconfig==1) { stages[0].color={15,6,8,15}; stages[0].alpha={7,3,4,7}; }
        else if(glx_texconfig==33) stages[1].color={15,6,8,15};
    }
    std::array<float4,4> reg{};
    // glx_SwitchUserData visits slots 5, 7, 13 in this order. Draw-time
    // priority selects an argument; it does not undo later register writes.
    if(overrideAlpha>=0) reg[3]={overrideAlpha,overrideAlpha,overrideAlpha,overrideAlpha};
    if(constantColor) reg[3]=tint;
    if(noRasterAlpha) reg[3]={1.f,1.f,1.f,1.f};
    for(int stage=0;stage<tevCount;++stage)
    {
        const auto& s=stages[stage];
        const float4 t=s.map<6?samples[s.map]:make4(0,0,0,0);
        const float4 r=s.channel==4?raster:s.channel==5?spec:make4(0,0,0,0);
        float4 result{};
        const auto rgb=[&](int arg,unsigned c) {
            if(arg<8) return reg[arg/2][arg%2?3:c];
            switch(arg) {
            case 8:return t[c]; case 9:return t.a; case 10:return r[c]; case 11:return r.a;
            case 12:return 1.f; case 13:return .5f; case 14:return constant(s.kcolor,k,false);
            default:return 0.f;
            }
        };
        const auto alpha=[&](int arg) {
            if(arg<4) return reg[arg].a;
            switch(arg) { case 4:return t.a; case 5:return r.a; case 6:return constant(s.kalpha,k,true); default:return 0.f; }
        };
        for(unsigned c=0;c<3;++c)
            result[c]=apply(rgb(s.color[0],c),rgb(s.color[1],c),rgb(s.color[2],c),rgb(s.color[3],c),s.rgb);
        result.a=apply(alpha(s.alpha[0]),alpha(s.alpha[1]),alpha(s.alpha[2]),alpha(s.alpha[3]),s.a);
        reg[s.rgb.reg].rgb=result.rgb;
        reg[s.a.reg].a=result.a;
    }
    return make4(reg[stages[tevCount-1].rgb.reg].rgb,reg[stages[tevCount-1].a.reg].a);
}
