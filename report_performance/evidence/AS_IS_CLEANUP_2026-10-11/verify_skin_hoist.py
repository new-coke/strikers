from pathlib import Path
import subprocess,json,hashlib
base=Path(__file__).resolve().parent
root=next(p for p in base.parents if (p/'smstrikers-port').is_dir())
port=root/'smstrikers-port'
def function(source, signature):
    start=source.index(signature)
    body=source.index('{',start)
    depth=0
    for end in range(body,len(source)):
        if source[end]=='{':depth+=1
        elif source[end]=='}':
            depth-=1
            if depth==0:return source[start:end+1]
    raise ValueError('Unbalanced function')
before=(base/'before/smstrikers-port/src/NL/glx/glxSend.cpp').read_text()
assert hashlib.sha256(before.encode()).hexdigest()=='3dc79a814565a50afa45b3c4408be8c940a44584140e09644b6bb96faa39cb22'
after=(port/'src/NL/glx/glxSend.cpp').read_text()
sig='static void glud_Skin(void* pData, const glModelPacket* pPacket)'
old=function(before,sig).replace('glud_Skin(', 'skin_before(',1)
new=function(after,sig).replace('glud_Skin(', 'skin_after(',1)
copy=function((port/'src/NL/glx/glxMatrix.cpp').read_text(),'void glxCopyMatrix(float (&mo)[3][4], const nlMatrix4& mi)').replace('glxCopyMatrix(','oracle_copy(',1).replace('{','{ ++copies;',1)
mult=function((port/'src/NL/plat/platvmath.cpp').read_text(),'void nlMultMatrices(nlMatrix4& out, const nlMatrix4& a, const nlMatrix4& b)')
source=r'''
#include "NL/gl/glUserData.h"
#include "NL/gl/gl.h"
#include "NL/glx/glxMatrix.h"
#include "NL/platvmath.h"
#include "dolphin/gx/GXTransform.h"
#include "dolphin/mtx.h"
#include <array>
#include <vector>
#include <cassert>
#include <cstdio>
#include <cstring>
struct GLSkinUserData { int reg; float mat[12]; };
static bool g_bFastSkinPath=true,g_bMtxSkinMath=true,glx_IsCoPlanarView;
static u8 glx_InvXpose=1,glx_InvXposeChar=0;
static eGLView prev_view;
static Mtx gx_mview;
static nlMatrix4 mview,identity;
uintptr_t glGetIdentityMatrix(){return reinterpret_cast<uintptr_t>(&identity);}
struct Load { bool normal; u32 slot; std::array<float,12> value{}; };
std::vector<Load> loads;
unsigned concats,copies,concats44;
extern "C" void GXLoadPosMtxImm(const f32 matrix[3][4],u32 slot){
  Load load{false,slot};std::memcpy(load.value.data(),matrix,sizeof(Mtx));loads.push_back(load);
}
extern "C" void GXLoadNrmMtxImm(const f32 matrix[3][4],u32 slot){
  Load load{true,slot};for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)load.value[r*3+c]=matrix[r][c];loads.push_back(load);
}
void oracle_concat(const Mtx a,const Mtx b,Mtx out){++concats;PSMTXConcat(a,b,out);}
void oracle_concat44(const Mtx44 a,const Mtx44 b,Mtx44 out){++concats44;PSMTX44Concat(a,b,out);}
#define PSMTXConcat oracle_concat
#define PSMTX44Concat oracle_concat44
'''+copy+'\n'+mult+'\n#define glxCopyMatrix oracle_copy\n'+old+'\n'+new+r'''
#undef glxCopyMatrix
#undef PSMTXConcat
#undef PSMTX44Concat
int main(){
  struct Payload {u32 count;GLSkinUserData bones[256];} payload{};
  static_assert(offsetof(Payload,bones)==4);
  PSMTX44Identity(identity.e2);
  nlMatrix4 model;
  PSMTX44Identity(model.e2);
  model.e2[0][0]=1.2f;model.e2[1][1]=0.8f;model.e2[2][2]=1.7f;model.e2[3][0]=3.3f;
  PSMTX44Identity(mview.e2);
  mview.e2[0][0]=0.91f;mview.e2[2][0]=0.2f;mview.e2[3][1]=-5.4f;
  oracle_copy(gx_mview,mview);
  for(unsigned i=0;i<256;++i){
    payload.bones[i].reg=int((i%10)*3);
    Mtx bone;PSMTXScale(bone,1.0f+(i%3)*0.25f,2.0f,0.5f);
    bone[0][3]=float(i);bone[1][3]=3.0f;
    std::memcpy(payload.bones[i].mat,bone,sizeof(Mtx));
  }
  unsigned cases=0;
  for(bool coplanar:{false,true})for(bool ownModel:{false,true})for(eGLView view:{GLV_Characters,GLV_Shadowed})for(unsigned count:{0u,1u,16u,64u,256u}){
    glx_IsCoPlanarView=coplanar;prev_view=view;payload.count=count;
    glModelPacket packet{};packet.state.matrix=reinterpret_cast<uintptr_t>(ownModel?&model:&identity);
    loads.clear();concats=copies=concats44=0;skin_before(&payload,&packet);
    const auto expected=loads;const unsigned beforeCopies=copies;
    loads.clear();concats=copies=concats44=0;skin_after(&payload,&packet);
    assert(loads.size()==expected.size()&&loads.size()==count*2);
    for(unsigned i=0;i<loads.size();++i){assert(loads[i].normal==expected[i].normal&&loads[i].slot==expected[i].slot);assert(std::memcmp(loads[i].value.data(),expected[i].value.data(),sizeof(loads[i].value))==0);}
    if(count&&(coplanar||ownModel)){assert(beforeCopies==count&&copies==1);assert(concats==count+(coplanar?0u:1u));assert(concats44==(coplanar?1u:0u));}
    ++cases;
  }
  glx_IsCoPlanarView=true;payload.count=0;glModelPacket empty{};loads.clear();skin_after(&payload,&empty);assert(loads.empty());
  std::printf("PASS: %u old/new skin cases; identical GX matrix bytes, slots and load order; invariant preparation N -> 1; zero-bone invalid-handle guard\n",cases);
}
'''
(base/'skin-hoist-oracle.cpp').write_text(source)
flags=['-fdeclspec','-DTARGET_PC=1','-include',str(port/'include/port/prelude.h'),'-Wno-ignored-attributes','-Wno-macro-redefined','-O1','-g','-UNDEBUG','-fshort-wchar','-fno-strict-aliasing','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(port/'include')]
objects=[]
for name in ['mtx','mtx44','vec']:
    obj=base/(name+'-oracle.o');objects.append(str(obj))
    subprocess.run(['clang','-std=c99',*flags,'-c',str(port/'src/Dolphin/mtx'/f'{name}.c'),'-o',str(obj)],check=True)
exe=base/'skin-hoist-oracle'
subprocess.run(['clang++','-std=c++17',*flags,'-Wno-register','-DTARGET_PC=1',str(base/'skin-hoist-oracle.cpp'),*objects,'-o',str(exe)],check=True)
import os
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
result=subprocess.run([str(exe)],env=env,text=True,stdout=subprocess.PIPE,check=True)
(base/'skin-hoist-oracle.log').write_text(result.stdout)
(base/'skin-hoist-oracle-identity.json').write_text(json.dumps({'scope':'Actual pre-cleanup/current glud_Skin bodies extracted with Vita experimental paths excluded; actual glxCopyMatrix/nlMultMatrices and matrix primitives; ASan/UBSan; host-only, not Vita FPS proof','control_sha256':hashlib.sha256(before.encode()).hexdigest(),'candidate_sha256':hashlib.sha256(after.encode()).hexdigest(),'result':result.stdout},indent=2)+'\n')
print(result.stdout,end='')
