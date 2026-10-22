#pragma once
#include <string>
namespace port::render
{
inline constexpr const char* loadingVertexShader = R"cg(
void main(float4 a_position:POSITION, float3 a_uv0:TEXCOORD0,
 out float4 position:POSITION, out float2 uv:TEXCOORD0,
 out float2 background:TEXCOORD1) {
 position=a_position;
 uv=a_uv0.xy;
 background=float2(a_position.x*.5+.5,.5-a_position.y*.5);
}
)cg";
// The native backdrop is RGB. Adapt it with the local OSFatal limited-range
// BT.601 coefficients, round bytes explicitly, and preserve untouched RGB.
inline constexpr const char* loadingColorMath = R"cg(
float byte_round(float x) { return floor(clamp(x,0.0,255.0)+.5); }
float rgb_y(float3 c) { return byte_round(16.0+dot(c,float3(.257,.504,.098))*255.0); }
float2 rgb_uv(float3 c) {
 return float2(byte_round(128.0+dot(c,float3(-.148,-.291,.439))*255.0),
               byte_round(128.0+dot(c,float3(.439,-.368,-.071))*255.0));
}
float3 yuv_rgb(float y,float2 uv) {
 float l=1.164*(y-16.0),u=uv.x-128.0,v=uv.y-128.0;
 return float3(byte_round(l+1.596*v),byte_round(l-.392*u-.813*v),
               byte_round(l+2.017*u))/255.0;
}
)cg";
inline constexpr const char* loadingFragmentBody = R"cg(
float4 main(float2 uv:TEXCOORD0,float2 background:TEXCOORD1,
 uniform sampler2D u_background:TEXUNIT0,
 uniform sampler2D u_operations:TEXUNIT1,
 uniform float4 u_region):COLOR {
 float4 backdrop=tex2D(u_background,background);
 // Zero region is the full-resolution backdrop pass.
 if(u_region.z<=0.0) return backdrop;
 float4 operation=tex2D(u_operations,uv);
 float flags=floor(operation.a*3.0+.5);
 if(flags<.5) return backdrop;
 float2 pixel=clamp(floor(uv*32.0),0.0,31.0);
 float2 center=u_region.xy+(pixel+.5)*u_region.zw;
 float2 even=u_region.xy+float2(floor(pixel.x*.5)*2.0+.5,pixel.y+.5)*u_region.zw;
 float3 first=tex2D(u_background,even).rgb;
 float3 second=tex2D(u_background,even+float2(u_region.z,0.0)).rgb;
 float y=frac(flags*.5)>.25 ? operation.r*255.0 : rgb_y(tex2D(u_background,center).rgb);
 float2 chroma=flags>1.5 ? operation.gb*255.0 : rgb_uv((first+second)*.5);
 return float4(yuv_rgb(y,chroma),1.0);
}
)cg";
inline std::string loading_fragment_shader()
{
    return std::string(loadingColorMath) + loadingFragmentBody;
}
} // namespace port::render
