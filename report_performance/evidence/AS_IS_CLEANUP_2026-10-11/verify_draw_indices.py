from pathlib import Path
import subprocess,os,hashlib,json
base=Path(__file__).resolve().parent;root=next(p for p in base.parents if (p/'smstrikers-port').is_dir());port=root/'smstrikers-port'
before=(base/'before/smstrikers-port/src/NL/glx/glxSend.cpp').read_text();after=(port/'src/NL/glx/glxSend.cpp').read_text()
def block(s):
 start=s.index('    if (p->indexBuffer == 0)\n    {\n        GXBegin(')
 end=s.index('\n    if (bIndirect)',start)
 return s[start:end]
source=r'''
#include "NL/glx/glxDisplayList.h"
#include "NL/gl/glMemory.h"
#include "NL/gl/glState.h"
#include "dolphin/gx/GXEnum.h"
#include <vector>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static u32 glx_NumIndices,gx_vtxfmt,raster8;
static bool glx_CompiledDraw=true,glx_AllowUncompiledDraws=true;
static _GXPrimitive primitives[6]={GX_TRIANGLES,GX_TRIANGLESTRIP,GX_TRIANGLEFAN,GX_QUADS,GX_LINES,GX_LINESTRIP};
static std::vector<uint64_t> emitted;
u32 glGetRasterState(unsigned long,eGLState){return raster8;}
void GXBegin(_GXPrimitive p,_GXVtxFmt f,u16 n){emitted.push_back(0x1000000000000000ull|(uint64_t(p)<<32)|(uint64_t(f)<<16)|n);}
void GXEnd(){emitted.push_back(0x2000000000000000ull);}
void GXPosition1x16(u16 i){emitted.push_back(0x3000000000000000ull|i);}
void GXCallDisplayList(void* p,u32 n){emitted.push_back(0x4000000000000000ull|n);for(u32 i=0;i<n;++i)emitted.push_back(((u8*)p)[i]);}
void* glResourceAlloc(unsigned long n,eGLMemory){return std::malloc(n);}
void* glFrameAlloc(unsigned long n,eGLMemory){return std::malloc(n);}
void nlZeroMemory(void* p,unsigned long n){std::memset(p,0,n);}
extern "C" void DCFlushRangeNoSync(void*,u32){}
extern "C" void PPCSync(){}
'''+ '\nvoid draw_before(const glModelPacket* p){u32 i,j;\n'+block(before)+'\n}\nvoid draw_after(const glModelPacket* p){u32 i,j;\n'+block(after)+r'''
}
int main(){
 unsigned cases=0;
 for(unsigned primitive=0;primitive<6;++primitive)for(unsigned streams:{2u,3u,8u})for(bool color:{false,true})for(unsigned vertices:{0u,1u,3u,32u,257u}){
  std::vector<u16> indices(vertices>2?vertices:2);for(unsigned i=0;i<vertices;++i)indices[i]=u16(i*7937+0x1234);
  std::vector<glModelStream> attributes(streams);for(unsigned i=0;i<streams;++i)attributes[i].id=color&&i==streams-1?0x0C:0;
  glModelPacket packet{};packet.numStreams=streams;packet.streams=attributes.data();packet.primType=primitive;packet.numVertices=vertices;packet.indexBuffer=reinterpret_cast<uintptr_t>(indices.data());
  DisplayList* list=dlMakeDisplayList(&packet,false);
  for(unsigned route=0;route<3;++route)for(unsigned indexed:{0u,1u,2u,3u,8u})for(unsigned raster:{0u,1u}){
   packet.indexBuffer=route==0?0:route==1?reinterpret_cast<uintptr_t>(indices.data()):reinterpret_cast<uintptr_t>(list);
   // Zero indexed attributes expects an actual DL whenever indexBuffer exists.
   if(route==1&&indexed==0)continue;
   glx_NumIndices=indexed;raster8=raster;gx_vtxfmt=2;
   emitted.clear();draw_before(&packet);const auto expected=emitted;
   emitted.clear();draw_after(&packet);assert(emitted==expected);++cases;
  }
  std::free(list->list);std::free(list);
 }
 std::printf("PASS: %u actual old/new draw blocks; identical GX opcode/attribute/Begin/End/DL byte sequences; raw/compiled/decoded DL routes and raster skip\n",cases);
}
'''
source += '\n#include \"'+str(port/'src/NL/glx/glxDisplayList.cpp')+'\"\n'
cpp=base/'draw-indices-oracle.cpp';cpp.write_text(source);exe=base/'draw-indices-oracle'
flags=['-std=c++17','-O1','-g','-UNDEBUG','-fdeclspec','-fshort-wchar','-fno-strict-aliasing','-Wno-register','-Wno-ignored-attributes','-Wno-implicit-exception-spec-mismatch','-Wno-macro-redefined','-DTARGET_PC=1','-include',str(port/'include/port/prelude.h'),'-I'+str(port/'include'),'-I'+str(port/'include/msl'),'-fsanitize=address,undefined','-fno-omit-frame-pointer']
subprocess.run(['clang++',*flags,str(cpp),'-o',str(exe)],check=True)
r=subprocess.run([str(exe)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),text=True,stdout=subprocess.PIPE,check=True)
(base/'draw-indices-oracle.log').write_text(r.stdout)
(base/'draw-indices-oracle-identity.json').write_text(json.dumps({'scope':'Actual pre-cleanup/current glx_DrawPacket index dispatch blocks, experimental native hooks excluded, actual current DL builder; ASan/UBSan; host-only, not Vita visual/FPS proof','control_sha256':hashlib.sha256(before.encode()).hexdigest(),'candidate_sha256':hashlib.sha256(after.encode()).hexdigest(),'result':r.stdout},indent=2)+'\n')
print(r.stdout,end='')
