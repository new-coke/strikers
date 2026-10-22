// The generator inserts actual Cg functions between the adapters and oracle.
#include "port/loading_indicator.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using float2 = float __attribute__((ext_vector_type(2)));
using float3 = float __attribute__((ext_vector_type(3)));
using float4 = float __attribute__((ext_vector_type(4)));
float2 make2(float x, float y) { return {x,y}; }
float3 make3(float x, float y, float z) { return {x,y,z}; }
float4 make4(float3 v, float w) { return {v.x,v.y,v.z,w}; }
float dot(float3 a, float3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
using std::floor;
float2 floor(float2 v) { return {floor(v.x),floor(v.y)}; }
float clamp(float v, float a, float b) { return std::clamp(v,a,b); }
float2 clamp(float2 v, float a, float b) { return {clamp(v.x,a,b),clamp(v.y,a,b)}; }
float frac(float v) { return v-floor(v); }
using Pixel = std::array<uint8_t,4>;
struct Texture { unsigned width, height; std::vector<Pixel> pixels; };
using sampler2D = const Texture*;
float4 tex2D(sampler2D texture, float2 uv) {
    const auto x = std::clamp(int(std::floor(uv.x*texture->width)),0,int(texture->width)-1);
    const auto y = std::clamp(int(std::floor(uv.y*texture->height)),0,int(texture->height)-1);
    const auto p = texture->pixels[y*texture->width+x];
    return {p[0]/255.f,p[1]/255.f,p[2]/255.f,p[3]/255.f};
}

// PRODUCTION_SHADERS

unsigned comparisons=0, untouched=0, maximumError=0;
uint32_t randomState=0x72912439u;
uint32_t random_bits() { return randomState=randomState*1664525u+1013904223u; }
int byte(double value) { return int(std::floor(std::clamp(value,0.,255.)+.5)); }
int expected_y(const Pixel& p) {
    // Integer coefficients and division are independent of shader float dot.
    return (16000+257*p[0]+504*p[1]+98*p[2]+500)/1000;
}
std::array<int,2> expected_uv(const Pixel& a,const Pixel& b) {
    const int r=int(a[0])+b[0],g=int(a[1])+b[1],bl=int(a[2])+b[2];
    return {(256000-148*r-291*g+439*bl+1000)/2000,
            (256000+439*r-368*g-71*bl+1000)/2000};
}
Pixel expected_rgb(int y,int u,int v) {
    const double l=(y-16)*1.164;
    return {uint8_t(byte(l+1.596*(v-128))),
            uint8_t(byte(l-.392*(u-128)-.813*(v-128))),
            uint8_t(byte(l+2.017*(u-128))),255};
}
Pixel nearest_pixel(const Texture& background,double x,double y) {
    return background.pixels[std::clamp(int(std::floor(y)),0,int(background.height)-1)*background.width+
                             std::clamp(int(std::floor(x)),0,int(background.width)-1)];
}
Pixel original_pixel(const Texture& background,const uint16_t (&image)[32][32],
                     const std::array<float,4>& rectangle,unsigned x,unsigned y,
                     float2 physicalUV) {
    const auto odd=x|1u;
    const bool ownWrite=(image[y][x]&15)!=0, sharedWrite=(image[y][odd]&15)!=0;
    if(!ownWrite&&!sharedWrite)
        return nearest_pixel(background,double(physicalUV.x)*background.width,
                              double(physicalUV.y)*background.height);
    const auto at=[&](unsigned column) {
        return nearest_pixel(background,rectangle[0]+(column+.5)*double(rectangle[2])/32.,
                              rectangle[1]+(y+.5)*double(rectangle[3])/32.);
    };
    const int luma=ownWrite?int((image[y][x]>>12)*17):expected_y(at(x));
    auto uv=expected_uv(at(x&~1u),at(odd));
    if(sharedWrite) {
        // StorePixel writes the odd pixel's filtered U/V into both neighbours.
        // Its left tap repeats source column zero for the first pair.
        const unsigned previous=odd>=2?odd-2:0;
        for(unsigned c=0;c<2;++c) {
            const unsigned shift=c?4:8;
            const auto nibble=[&](unsigned column) { return int((image[y][column]>>shift)&15)*17; };
            uv[c]=(nibble(previous)+2*nibble(odd-1)+nibble(odd))/4;
        }
    }
    return expected_rgb(luma,uv[0],uv[1]);
}
void compare(float4 actual,const Pixel& expected,unsigned tolerance) {
    for(unsigned channel=0;channel<4;++channel) {
        const auto error=unsigned(std::abs(byte(actual[channel]*255.)-expected[channel]));
        maximumError=std::max(maximumError,error);
        if(error>tolerance) {
            std::fprintf(stderr,"loading shader mismatch channel=%u got=%.8f expected=%u tolerance=%u\n",
                         channel,double(actual[channel]*255.f),expected[channel],tolerance);
            assert(false);
        }
        ++comparisons;
    }
}
int main() {
    // Math boundaries and random values: allow one byte only at float32 versus
    // rational half-byte rounding ties, and require exact neutral black/white.
    assert(rgb_y(make3(0,0,0))==16 && rgb_y(make3(1,1,1))==235);
    const auto neutral=rgb_uv(make3(0,0,0));
    assert(neutral.x==128&&neutral.y==128);
    compare(make4(yuv_rgb(16,make2(128,128)),1),{0,0,0,255},0);
    for(unsigned n=0;n<100000;++n) {
        const Pixel a{uint8_t(random_bits()>>24),uint8_t(random_bits()>>24),uint8_t(random_bits()>>24),255};
        const Pixel b{uint8_t(random_bits()>>24),uint8_t(random_bits()>>24),uint8_t(random_bits()>>24),255};
        const float3 first{a[0]/255.f,a[1]/255.f,a[2]/255.f};
        const float3 second{b[0]/255.f,b[1]/255.f,b[2]/255.f};
        assert(std::abs(int(rgb_y(first))-expected_y(a))<=1);
        const auto uv=rgb_uv((first+second)*.5f);
        const auto expected=expected_uv(a,b);
        assert(std::abs(int(uv.x)-expected[0])<=1&&std::abs(int(uv.y)-expected[1])<=1);
        const int y=random_bits()>>24,u=random_bits()>>24,v=random_bits()>>24;
        compare(make4(yuv_rgb(float(y),make2(float(u),float(v))),1),expected_rgb(y,u,v),1);
    }
    Texture background{960,544,std::vector<Pixel>(960*544)};
    Texture operations{32,32,std::vector<Pixel>(32*32)};
    for(unsigned pattern=0;pattern<8;++pattern) {
        for(unsigned y=0;y<background.height;++y)
            for(unsigned x=0;x<background.width;++x) {
                auto& pixel=background.pixels[y*background.width+x];
                pixel=pattern==0?Pixel{0,0,0,255}:pattern==1?Pixel{255,255,255,255}:
                    Pixel{uint8_t(random_bits()>>24),uint8_t(random_bits()>>24),uint8_t(random_bits()>>24),uint8_t(random_bits()>>24)};
            }
        for(unsigned selected=0;selected<2;++selected) {
            const auto& source=selected?port::loading::selectedPixels:port::loading::normalPixels;
            const auto& encoded=selected?port::loading::selectedOperations:port::loading::normalOperations;
            for(unsigned n=0;n<32*32;++n)
                std::copy_n(encoded.begin()+n*4,4,operations.pixels[n].begin());
            for(bool other:{false,true}) {
                port::loading::Request request;
                request.otherPosition=other;
                const auto rectangles=port::loading::rectangles(request);
                for(const auto& rectangle:rectangles) {
                    const float4 region{rectangle[0]/960.f,rectangle[1]/544.f,
                                         rectangle[2]/(960.f*32),rectangle[3]/(544.f*32)};
                    for(unsigned y=0;y<32;++y)
                        for(unsigned x=0;x<32;++x)
                            for(float offset:{.25f,.5f,.75f}) {
                                const float2 uv{(x+offset)/32.f,(y+offset)/32.f};
                                const float2 screen{(rectangle[0]+uv.x*rectangle[2])/960.f,
                                                    (rectangle[1]+uv.y*rectangle[3])/544.f};
                                const auto actual=native_loading_fragment(uv,screen,&background,&operations,region);
                                const auto expected=original_pixel(background,source,rectangle,x,y,screen);
                                const bool unchanged=!(source[y][x]&15)&&!(source[y][x|1u]&15);
                                // A changed shared UV can amplify a one-byte
                                // rounding tie by at most 2.017 RGB bytes.
                                compare(actual,expected,unchanged?0:4);
                                if(unchanged) {
                                    const auto original=tex2D(&background,screen);
                                    for(unsigned c=0;c<4;++c)assert(actual[c]==original[c]);
                                    ++untouched;
                                }
                            }
                    const float2 uv{.36f,.77f};
                    const auto result=native_loading_fragment(uv,uv,&background,&operations,float4{0,0,0,0});
                    const auto original=tex2D(&background,uv);
                    for(unsigned c=0;c<4;++c)assert(result[c]==original[c]);
                    for(float x:{rectangle[0],rectangle[0]+rectangle[2]})
                        for(float y:{rectangle[1],rectangle[1]+rectangle[3]}) {
                            const float4 vertex{x/480.f-1,1-y/272.f,0,1};
                            float4 position;
                            float2 textureUV,backgroundUV;
                            native_loading_vertex(vertex,make3(.25f,.75f,1),position,textureUV,backgroundUV);
                            assert(position.x==vertex.x&&position.y==vertex.y);
                            assert(textureUV.x==.25f&&textureUV.y==.75f);
                            assert(std::fabs(backgroundUV.x-x/960.f)<.000001f);
                            assert(std::fabs(backgroundUV.y-y/544.f)<.000001f);
                        }
                }
            }
        }
    }
    std::printf("Loading production Cg: %u channel comparisons, %u exact untouched pixels, max quantized error %u byte(s)\n",
                comparisons,untouched,maximumError);
}
