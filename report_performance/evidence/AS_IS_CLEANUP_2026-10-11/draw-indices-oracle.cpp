
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

void draw_before(const glModelPacket* p){u32 i,j;
    if (p->indexBuffer == 0)
    {
        GXBegin(primitives[p->primType], (_GXVtxFmt)gx_vtxfmt, p->numVertices);

        for (i = 0; i < p->numVertices; i++)
        {
            for (j = 0; j < glx_NumIndices; j++)
            {
                GXPosition1x16((u16)i);   // PORT: Aurora's FIFO
            }
        }
        GXEnd();   // PORT: a no-op on console, required by Aurora
    }
    else
    {
        if (glx_NumIndices == 0)
        {
#if defined(PORT_VITA) && !defined(STRIKERS_VITA_GXM_LEAN_RENDER)
            if (!native_model_draw_only(p))
#endif
                GXCallDisplayList(dlGetDisplayList(p->indexBuffer), dlGetSize(p->indexBuffer));
        }
        else if (glx_CompiledDraw && (glx_NumIndices == p->numStreams) && dlIsDisplayList(p->indexBuffer))
        {
#if defined(PORT_VITA) && !defined(STRIKERS_VITA_GXM_LEAN_RENDER)
            if (!native_model_draw_only(p))
#endif
                GXCallDisplayList(dlGetDisplayList(p->indexBuffer), dlGetSize(p->indexBuffer));
        }
        else if (glx_AllowUncompiledDraws && glGetRasterState(p->state.raster, (eGLState)8) != 1)
        {
            if (dlIsDisplayList(p->indexBuffer))
            {
                DisplayList* dl = dlGetStruct(p->indexBuffer);
                GXBegin(primitives[p->primType], (_GXVtxFmt)gx_vtxfmt, p->numVertices);
                for (j = 0; j < p->numVertices; j++)
                {
                    for (i = 0; i < glx_NumIndices; i++)
                    {
                        u16* ptr;
                        if (((u16*)&dl->indices)[1] != 0)
                        {
                            u16 ns = ((u16*)&dl->indices)[0];
                            s32 stride = (ns - 1) * 2 + 1;
                            s32 offset = stride * j;
                            u8* ptr8 = (u8*)dl->list + offset;
                            ptr = (u16*)ptr8;
                            ptr8 = (u8*)ptr;
                            ptr8 += 4;
                            ptr = (u16*)ptr8;
                        }
                        else
                        {
                            u16 ns = ((u16*)&dl->indices)[0];
                            s32 stride = ns * 2;
                            s32 offset = j * stride;
                            u8* ptr8 = (u8*)dl->list + offset;
                            ptr = (u16*)ptr8;
                            ptr8 = (u8*)ptr;
                            ptr8 += 3;
                            ptr = (u16*)ptr8;
                        }
                        // PORT: the display list is big-endian; *ptr would read it host-order and GXPosition1x16 swaps again.
                        GXPosition1x16((u16)(((const u8*)ptr)[0] << 8
                                             | ((const u8*)ptr)[1]));
                    }
                }
                GXEnd();   // PORT: see above
            }
            else
            {
                u16* idxPtr = (u16*)p->indexBuffer;
                GXBegin(primitives[p->primType], (_GXVtxFmt)gx_vtxfmt, p->numVertices);
                for (i = 0; i < p->numVertices; i++)
                {
                    for (j = 0; j < glx_NumIndices; j++)
                    {
                        GXPosition1x16(idxPtr[i]);   // PORT: Aurora's FIFO
                    }
                }
                GXEnd();   // PORT: see above
            }
        }
    }

}
void draw_after(const glModelPacket* p){u32 i,j;
    if (p->indexBuffer == 0)
    {
        GXBegin(primitives[p->primType], (_GXVtxFmt)gx_vtxfmt, p->numVertices);

        for (i = 0; i < p->numVertices; i++)
        {
            for (j = 0; j < glx_NumIndices; j++)
            {
                GXPosition1x16((u16)i);   // PORT: Aurora's FIFO
            }
        }
        GXEnd();   // PORT: a no-op on console, required by Aurora
    }
    else
    {
        DisplayList* dl = nullptr;
        if (glx_NumIndices == 0 ||
            (glx_NumIndices == p->numStreams && (dl = dlGetStruct(p->indexBuffer)) != nullptr))
        {
#if defined(PORT_VITA) && !defined(STRIKERS_VITA_GXM_LEAN_RENDER)
            if (!native_model_draw_only(p))
#endif
            {
                if (dl == nullptr)
                    dl = dlGetStruct(p->indexBuffer);
                GXCallDisplayList(dl != nullptr ? dl->list : nullptr, dl != nullptr ? dl->size : 0);
            }
        }
        else if (glGetRasterState(p->state.raster, (eGLState)8) != 1)
        {
            if ((dl = dlGetStruct(p->indexBuffer)) != nullptr)
            {
                const bool hasColor = dl->indices[1] != 0;
                const u32 stride = (dl->indices[0] - (hasColor ? 1u : 0u)) * 2u + (hasColor ? 1u : 0u);
                const u8* indexBytes = (const u8*)dl->list + 3 + (hasColor ? 1 : 0);
                GXBegin(primitives[p->primType], (_GXVtxFmt)gx_vtxfmt, p->numVertices);
                for (j = 0; j < p->numVertices; j++)
                {
                    // Immutable DL storage: decode once, then emit the same
                    // index for every indexed attribute in the original order.
                    const u16 index = (u16)((indexBytes[0] << 8) | indexBytes[1]);
                    for (i = 0; i < glx_NumIndices; i++)
                        GXPosition1x16(index);
                    indexBytes += stride;
                }
                GXEnd();   // PORT: see above
            }
            else
            {
                u16* idxPtr = (u16*)p->indexBuffer;
                GXBegin(primitives[p->primType], (_GXVtxFmt)gx_vtxfmt, p->numVertices);
                for (i = 0; i < p->numVertices; i++)
                {
                    const u16 index = idxPtr[i];
                    for (j = 0; j < glx_NumIndices; j++)
                    {
                        GXPosition1x16(index);   // PORT: Aurora's FIFO
                    }
                }
                GXEnd();   // PORT: see above
            }
        }
    }

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

#include "/Users/robin994/Documents/Code/strikersVita/smstrikers-port/src/NL/glx/glxDisplayList.cpp"
