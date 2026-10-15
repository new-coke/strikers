#pragma once
namespace port::nativegxm {
inline constexpr const char* kSceneVertexShader=R"(
void main(float4 a_position:POSITION,float4 a_color:COLOR0,float3 a_normal:NORMAL,
 float3 a_uv0:TEXCOORD0,float3 a_uv1:TEXCOORD1,float3 a_uv2:TEXCOORD2,
 float3 a_uv3:TEXCOORD3,float3 a_uv4:TEXCOORD4,float3 a_uv5:TEXCOORD5,float a_bone:TEXCOORD6,
 uniform float4 u_mvp[4],uniform float4 u_mv[4],uniform float4 u_bones[30],
 uniform float4 u_environment,uniform float4 u_ambient,uniform float4 u_diffuse,uniform float4 u_lighting,
 uniform float4 u_lightpos[8],uniform float4 u_lightcolor[8],uniform float4 u_specular,uniform float4 u_specularcolor,
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
 if(u_lighting.y>=0.0)color.a=u_lighting.y;
 spec=float4(0,0,0,0);if(dot(u_specularcolor.rgb,u_specularcolor.rgb)>0.0){float3 H=normalize(u_specular.xyz+normalize(-eye.xyz));float shine=pow(max(dot(N,H),0.0),max(u_specular.w,1.0));spec=u_specularcolor*shine;}
 position=u_mvp[0]*p.x+u_mvp[1]*p.y+u_mvp[2]*p.z+u_mvp[3]*p.w;position.z=2.0*position.z+position.w;
 uv0=a_uv0;uv1=a_uv1;uv2=a_uv2;uv3=a_uv3;uv4=a_uv4;uv5=u_environment.z<0.0?a_uv5:float3(color.r,color.g,1.0);
 if(u_environment.x>0.5){uv4=float3(N.x*0.5+0.5,0.5-N.y*0.5,1.0);spec=float4(color.a,color.a,color.a,color.a);}
 if(u_environment.y>0.5)uv0=float3(N.x*0.5+0.5+u_environment.w*N.z,0.5-N.y*0.5+u_environment.w*N.z,1.0);
 if(u_lighting.z>0.5){uv0.y+=u_lighting.w;}
 eyeZ=-eye.z;
})";
inline constexpr const char* kSceneFragmentShader=R"(
float4 main(float4 color:COLOR0,float4 spec:COLOR1,float3 uv0:TEXCOORD0,float3 uv1:TEXCOORD1,
 float3 uv2:TEXCOORD2,float3 uv3:TEXCOORD3,float3 uv4:TEXCOORD4,float3 uv5:TEXCOORD5,float eyeZ:TEXCOORD6,float2 pixel:WPOS,
 uniform sampler2D u_tex0:TEXUNIT0,uniform sampler2D u_tex1:TEXUNIT1,uniform sampler2D u_tex2:TEXUNIT2,
 uniform sampler2D u_tex3:TEXUNIT3,uniform sampler2D u_tex4:TEXUNIT4,uniform sampler2D u_tex5:TEXUNIT5,
 uniform float4 u_controls,uniform float4 u_effects,uniform float4 u_tint,uniform float4 u_shadow,uniform float4 u_shadowadd,
 uniform float4 u_fog,uniform float4 u_fogcolor,uniform float4 u_scissor):COLOR {
 if(pixel.x<u_scissor.x||pixel.y<u_scissor.y||pixel.x>=u_scissor.z||pixel.y>=u_scissor.w)discard;
 float mask=u_controls.x;float4 base=color;if(u_effects.x>0.5)base.rgb=u_tint.rgb;
 float4 d=tex2D(u_tex0,uv0.xy/max(abs(uv0.z),0.000001));
 float4 detail=tex2D(u_tex1,uv1.xy/max(abs(uv1.z),0.000001));
 float4 sh=tex2D(u_tex2,uv2.xy/max(abs(uv2.z),0.000001));
 float4 self=tex2D(u_tex3,uv3.xy/max(abs(uv3.z),0.000001));
 float4 gloss=tex2D(u_tex4,uv4.xy/max(abs(uv4.z),0.000001));
 float4 bump=tex2D(u_tex5,uv5.xy/max(abs(uv5.z),0.000001));
 if(fmod(floor(mask/32.0),2.0)>0.5)base.rgb=float3(1.0,1.0,1.0);
 float4 c=base;
 if(fmod(mask,2.0)>0.5)c*=d;
 if(fmod(floor(mask/2.0),2.0)>0.5)c=lerp(detail,d,u_effects.z)*base;
 if(fmod(floor(mask/4.0),2.0)>0.5){if(mask==7.0||mask==39.0)c.rgb*=saturate(sh.rgb*u_shadow.rgb+u_shadowadd.rgb);else c.rgb*=sh.rgb;}
 if(fmod(floor(mask/8.0),2.0)>0.5)c.rgb+=self.rgb;
 if(fmod(floor(mask/16.0),2.0)>0.5)c.rgb+=gloss.rgb*spec.rgb;
 if(fmod(floor(mask/32.0),2.0)>0.5){c.rgb*=bump.rgb*u_controls.z;}
 if(u_controls.w==2.0){float y=d.r,u=detail.r-0.5,v=sh.r-0.5;c=float4(y+1.402*v,y-0.344136*u-0.714136*v,y+1.772*u,1.0);}
 if(u_controls.w==3.0)c=float4(c.r,c.r,c.r,c.r);
 if(u_controls.y>=0.0&&c.a<=u_controls.y)discard;
 if(fmod(floor(mask/32.0),2.0)<0.5)c.rgb*=u_controls.z;
 if(u_fog.z>0.5){float f=saturate((eyeZ-u_fog.x)/max(u_fog.y-u_fog.x,0.001));
  if(u_fog.w==1.0)f=1.0-exp2(-8.0*f);if(u_fog.w==2.0)f=1.0-exp2(-8.0*f*f);
  if(u_fog.w==3.0)f=exp2(-8.0*(1.0-f));if(u_fog.w==4.0)f=exp2(-8.0*(1.0-f)*(1.0-f));c.rgb=lerp(c.rgb,u_fogcolor.rgb,f);}
 return saturate(c);
})";
}
