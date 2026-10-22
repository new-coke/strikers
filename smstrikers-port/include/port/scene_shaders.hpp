#pragma once
namespace port::render
{
inline constexpr const char* kSceneVertexShader = R"(
void main(float4 a_position:POSITION,float4 a_color:COLOR0,float3 a_normal:NORMAL,
 float3 a_uv0:TEXCOORD0,float3 a_uv1:TEXCOORD1,float3 a_uv2:TEXCOORD2,
 float3 a_uv3:TEXCOORD3,float3 a_uv4:TEXCOORD4,float3 a_uv5:TEXCOORD5,float a_bone:TEXCOORD6,
 uniform float4 u_mvp[4],uniform float4 u_mv[4],uniform float4 u_bones[STRIKERS_SCENE_BONE_ROWS],
 uniform float4 u_environment,uniform float4 u_ambient,uniform float4 u_diffuse,uniform float4 u_lighting,
#if STRIKERS_VERTEX_FOG
 uniform float4 u_fog,
#endif
 uniform float4 u_lightpos[8],uniform float4 u_lightcolor[8],uniform float4 u_specular[4],uniform float4 u_specularcolor[4],
 out float4 position:POSITION,out float4 color:COLOR0,out float4 spec:COLOR1,
 out float3 uv0:TEXCOORD0,out float3 uv1:TEXCOORD1,out float3 uv2:TEXCOORD2,
 out float3 uv3:TEXCOORD3,out float3 uv4:TEXCOORD4,out float3 uv5:TEXCOORD5,out float eyeZ:TEXCOORD6) {
 int b=(int)a_bone*3;float4 p=a_position;float3 n=a_normal;
 if(b>0){p.xyz=float3(dot(u_bones[b],p),dot(u_bones[b+1],p),dot(u_bones[b+2],p));
  n=float3(dot(u_bones[b].xyz,n),dot(u_bones[b+1].xyz,n),dot(u_bones[b+2].xyz,n));}
 float4 eye=u_mv[0]*p.x+u_mv[1]*p.y+u_mv[2]*p.z+u_mv[3]*p.w;
 float3 eyeNormal=u_mv[0].xyz*n.x+u_mv[1].xyz*n.y+u_mv[2].xyz*n.z;
 float3 N=eyeNormal*rsqrt(max(dot(eyeNormal,eyeNormal),0.000001));
 float4 lit=u_ambient;
 if(u_lighting.x>0.5){
 for(int i=0;i<8;++i){float3 L=u_lightpos[i].w<0.0?u_lightpos[i].xyz:u_lightpos[i].xyz-eye.xyz;
  float d2=dot(L,L);float diffuse=max(dot(N,L*rsqrt(max(d2,0.000001))),0.0);
  float attenuation=u_lightpos[i].w>0.0?1.0/(1.0+d2*u_lightpos[i].w):1.0;
  lit+=u_lightcolor[i]*diffuse*attenuation;}}
 color=a_color;if(u_lighting.x>0.5)color.rgb=saturate(lit.rgb)*a_color.rgb*u_diffuse.rgb;
 if(u_environment.z>0.0)color.a=pow(1.0-max(N.z,0.0),u_environment.z);
 float rasterAlpha=color.a;
 if(u_lighting.y>=0.0)color.a=u_lighting.y;
 // GXInitSpecularDir uses the normalized incoming light direction, a fixed
 // view half-vector and GX_AF_SPEC's rational attenuation, not a power lobe.
 spec=float4(0,0,0,0);
 for(int j=0;j<4;++j){if(dot(u_specularcolor[j].rgb,u_specularcolor[j].rgb)>0.0){
  float3 L=-u_specular[j].xyz;float3 halfVector=L+float3(0.0,0.0,1.0);
  float3 H=halfVector*rsqrt(max(dot(halfVector,halfVector),0.000001));
  float cosine=dot(N,L)>=0.0?max(dot(N,H),0.0):0.0;
  float cosine2=cosine*cosine;float exponentHalf=0.5*u_specular[j].w;
  float denominator=exponentHalf+(1.0-exponentHalf)*cosine2;
  float shine=denominator>0.0?cosine2/denominator:(cosine2>0.0?1.0:0.0);
  spec+=u_specularcolor[j]*shine;}}
 spec=saturate(spec);
 // Only RGB1 is lit in GLX. ALPHA1 reads the absent CLR1 stream (zero).
 spec.a=0.0;
 position=u_mvp[0]*p.x+u_mvp[1]*p.y+u_mvp[2]*p.z+u_mvp[3]*p.w;position.z=2.0*position.z+position.w;
 uv0=a_uv0;uv1=a_uv1;uv2=a_uv2;uv3=a_uv3;uv4=a_uv4;uv5=u_environment.z<0.0?a_uv5:float3(color.r,color.g,1.0);
 if(u_environment.x>0.5){uv4=float3(N.x*0.5+0.5,0.5-N.y*0.5,1.0);spec=float4(rasterAlpha,rasterAlpha,rasterAlpha,rasterAlpha);}
 if(u_environment.y>0.5)uv0=float3(N.x*0.5+0.5+u_environment.w*N.z,0.5-N.y*0.5+u_environment.w*N.z,1.0);
 if(u_lighting.z>0.5){uv0.y+=u_lighting.w;}
#if STRIKERS_VERTEX_FOG
 // Only the affine part commutes with perspective-correct interpolation.
 // Keep saturation and nonlinear curves in the fragment stage.
 eyeZ=(-eye.z-u_fog.x)/max(u_fog.y-u_fog.x,0.001);
#else
 eyeZ=-eye.z;
#endif
})";
// The movie TEV stages share coordinate 1 for both chroma planes. Standard
// materials keep coordinate 2 for the texture in the shadow slot.
inline constexpr const char* kSceneFragmentShader = R"(
#if STRIKERS_AFFINE_UV
#define SCENE_UV(uv) ((uv).xy)
#else
#define SCENE_UV(uv) ((uv).xy/max(abs((uv).z),0.000001))
#endif
// One constant-component ladder selects both adjacent coefficients for q in 0..9.
// Preserve the terminal pair and the signed zero of floor(q); no coefficient 10.
float3 scene_fog_range_pair(float q,float4 r0,float4 r1,float4 r2) {
 if(q<1.0)return float3(r0.x,r0.y,q==0.0?q:0.0);
 if(q<2.0)return float3(r0.y,r0.z,1.0);
 if(q<3.0)return float3(r0.z,r0.w,2.0);
 if(q<4.0)return float3(r0.w,r1.x,3.0);
 if(q<5.0)return float3(r1.x,r1.y,4.0);
 if(q<6.0)return float3(r1.y,r1.z,5.0);
 if(q<7.0)return float3(r1.z,r1.w,6.0);
 if(q<8.0)return float3(r1.w,r2.x,7.0);
 if(q<9.0)return float3(r2.x,r2.y,8.0);
 return float3(r2.y,r2.y,9.0);
}
float4 main(float4 color:COLOR0,float4 spec:COLOR1,float3 uv0:TEXCOORD0,float3 uv1:TEXCOORD1,
 float3 uv2:TEXCOORD2,float3 uv3:TEXCOORD3,float3 uv4:TEXCOORD4,float3 uv5:TEXCOORD5,float eyeZ:TEXCOORD6,
 float4 fragmentPosition:WPOS,
 uniform sampler2D u_tex0:TEXUNIT0,uniform sampler2D u_tex1:TEXUNIT1,uniform sampler2D u_tex2:TEXUNIT2,
 uniform sampler2D u_tex3:TEXUNIT3,uniform sampler2D u_tex4:TEXUNIT4,uniform sampler2D u_tex5:TEXUNIT5,
 uniform float4 u_controls,uniform float4 u_effects,uniform float4 u_tint,uniform float4 u_shadow,uniform float4 u_shadowadd,
 uniform float4 u_fog,uniform float4 u_fogcolor,uniform float4 u_material,uniform float4 u_alpha,
 uniform float4 u_fogrange[3],uniform float4 u_fograngecontrol):COLOR {
 float mask=u_controls.x;float4 base=color;
 if(mask==1.0&&u_effects.x>0.5&&u_effects.y<0.0)base.rgb=u_tint.rgb;
 float2 diffuseUv=SCENE_UV(uv0);
 if(u_material.z>0.5){float partition=u_material.z-1.0;diffuseUv=diffuseUv*float2(0.25,0.3333333333)+float2(fmod(partition,4.0)*0.25,floor(partition/4.0)*0.3333333333);}
 float4 d=tex2D(u_tex0,diffuseUv);
 float4 detail=tex2D(u_tex1,SCENE_UV(uv1));
 float4 sh=tex2D(u_tex2,SCENE_UV(u_controls.w==2.0?uv1:uv2));
 float4 self=tex2D(u_tex3,SCENE_UV(uv3));
 float4 gloss=tex2D(u_tex4,SCENE_UV(uv4));
 float4 bump=tex2D(u_tex5,SCENE_UV(uv5));
 if(fmod(floor(mask/32.0),2.0)>0.5)base.rgb=float3(1.0,1.0,1.0);
 float4 c=base;
 if(fmod(mask,2.0)>0.5)c*=d;
 if(fmod(floor(mask/2.0),2.0)>0.5)c=lerp(detail,d,u_effects.z)*base;
 // ConstantColour cleanup leaves stage 1 as RASC*TEXC until the next
 // texconfig switch. All used stage-1 raster channels are null except 0x11.
 if(u_material.w>0.5&&mask>1.0){
  if(mask==35.0||mask==39.0||mask==43.0)c.rgb=detail.rgb*(1.0-u_effects.z);
  else c.rgb=float3(0.0,0.0,0.0);}
 // These programs scale and clamp the light lookup before the diffuse stage.
 // Other programs (including 0x21) scale the diffuse result instead.
 if(fmod(floor(mask/32.0),2.0)>0.5){
  if(mask==49.0||mask==53.0||mask==57.0||mask==39.0||mask==43.0||(mask==35.0&&u_material.y<0.5))c.rgb*=saturate(bump.rgb*u_controls.z);
  else if(mask==37.0)c.rgb*=bump.rgb;
  else c.rgb=saturate(c.rgb*bump.rgb*u_controls.z);}
 if(mask==35.0&&u_material.y>0.5)c=float4(saturate(d.rgb*saturate(detail.rgb+u_effects.z)*bump.rgb*u_controls.z),d.a*base.a);
 if(mask==35.0&&u_material.y>0.5&&u_material.w>0.5)c.rgb=float3(0.0,0.0,0.0);
 if(fmod(floor(mask/4.0),2.0)>0.5){if(mask==7.0||mask==39.0)c.rgb*=saturate(sh.rgb*u_shadow.rgb+u_shadowadd.rgb);else c.rgb*=sh.rgb;}
 if(mask==37.0)c.rgb=saturate(c.rgb*u_controls.z);
 if(fmod(floor(mask/8.0),2.0)>0.5&&mask!=57.0&&!(u_material.w>0.5&&(mask==9.0||mask==25.0)))c.rgb+=self.rgb;
 if(fmod(floor(mask/16.0),2.0)>0.5){
  float3 reflection=gloss.rgb*spec.rgb;
  if(mask==57.0)reflection*=self.rgb;
  if(mask==49.0||mask==53.0||mask==57.0)c.rgb+=reflection*u_material.x;
  else c.rgb+=reflection;}
 // Alpha has its own TEV stages: several light-ramp programs deliberately
 // ignore vertex alpha, and 0x13 retains diffuse alpha during detail blending.
 if(mask==0.0)c.a=color.a*u_alpha.x;
 if(mask==19.0)c.a=d.a*color.a;
 if(mask==33.0||mask==49.0)c.a=d.a*(u_effects.y>=0.0?u_effects.y:u_alpha.y);
 if(mask==53.0||mask==57.0)c.a=d.a*u_alpha.y;
 if(mask==35.0)c.a=u_material.y>0.5?d.a:lerp(detail.a,d.a,u_effects.z);
 if(mask==43.0)c.a=lerp(detail.a,d.a,u_effects.z);
 // 0x27 selects COLOR_NULL at its alpha modulation stage. 0x29 adds
 // its retained stage-2 konst; the selectors persist across view/frame resets.
 if(mask==39.0)c.a=lerp(detail.a,d.a,u_effects.z)*(u_effects.y>=0.0?u_effects.y:0.0);
 if(mask==41.0)c.a=saturate(d.a*(u_effects.y>=0.0?u_effects.y:u_alpha.y)+u_alpha.z*u_alpha.z);
 // Original 0x15/0x25 never replace stage 2's alpha inputs. Preserve the
 // finite set of preceding operations without translating arbitrary TEV.
 if(mask==21.0||mask==37.0){
  float previous=mask==21.0?d.a*color.a:u_alpha.x*u_alpha.x;
  float textureAlpha=mask==21.0?gloss.a:sh.a;
  float rasterAlpha=mask==21.0?spec.a:0.0;
  c.a=0.0;
  if(u_alpha.w==0.0)c.a=2.0*previous;
  else if(u_alpha.w==1.0)c.a=previous;
  else if(u_alpha.w==2.0)c.a=previous*rasterAlpha;
  else if(u_alpha.w==3.0)c.a=previous+textureAlpha;
  else if(u_alpha.w==4.0)c.a=previous+u_alpha.z*u_alpha.z;
  else if(u_alpha.w==5.0)c.a=textureAlpha;
  else if(u_alpha.w==6.0)c.a=previous+textureAlpha*(1.0-u_alpha.z);
  else if(u_alpha.w==8.0)c.a=u_alpha.z*u_alpha.z;
  c.a=saturate(c.a);}
 if(mask==33.0&&u_effects.x>0.5&&u_effects.y<0.0)c.rgb=saturate(d.rgb*u_tint.rgb*u_controls.z);
 if(u_controls.w==2.0){float y=d.r,u=detail.r-0.5,v=sh.r-0.5;c=float4(y+1.402*v,y-0.344136*u-0.714136*v,y+1.772*u,1.0);}
 if(u_controls.w==3.0)c=float4(c.r,c.r,c.r,c.r);
 if(u_controls.w==4.0)c=float4(d.a,d.a,d.a,d.a);
 if(u_controls.y>=0.0&&c.a<=u_controls.y)discard;
 if(fmod(floor(mask/32.0),2.0)<0.5)c.rgb*=u_controls.z;
 if(u_fog.z>0.5){
  float rangeAdjust=1.0;
  if(u_fograngecontrol.x>0.5){
   float offset=(fragmentPosition.x-u_fograngecontrol.z)*u_fograngecontrol.y;
   float q=clamp(9.0-9.0*abs(offset),0.0,9.0);
   float3 rangePair=scene_fog_range_pair(q,u_fogrange[0],u_fogrange[1],u_fogrange[2]);
   float k=lerp(rangePair.x,rangePair.y,q-rangePair.z);
   rangeAdjust=sqrt(offset*offset+k*k)/k;
  }
#if STRIKERS_VERTEX_FOG
  // eyeZ is the interpolated unsaturated affine base fog, not raw depth.
  float affineFog=eyeZ;
  if(u_fograngecontrol.x>0.5)
   affineFog=eyeZ*rangeAdjust+u_fograngecontrol.w*(rangeAdjust-1.0);
  float f=saturate(affineFog);
#else
  float fogDepth=eyeZ;
  if(u_fograngecontrol.x>0.5)fogDepth*=rangeAdjust;
  float f=saturate((fogDepth-u_fog.x)/max(u_fog.y-u_fog.x,0.001));
#endif
  if(u_fog.w==1.0)f=1.0-exp2(-8.0*f);if(u_fog.w==2.0)f=1.0-exp2(-8.0*f*f);
  if(u_fog.w==3.0)f=exp2(-8.0*(1.0-f));if(u_fog.w==4.0)f=exp2(-8.0*(1.0-f)*(1.0-f));c.rgb=lerp(c.rgb,u_fogcolor.rgb,f);}
 return saturate(c);
})";
}   // namespace port::render
