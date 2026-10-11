
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
void oracle_copy(float (&mo)[3][4], const nlMatrix4& mi)
{ ++copies;
    // Row 0: copy from nlMatrix4 row 0 to target row 0
    mo[0][0] = mi.e2[0][0]; // offset 0x00
    mo[0][1] = mi.e2[1][0]; // offset 0x04
    mo[0][2] = mi.e2[2][0]; // offset 0x08
    mo[0][3] = mi.e2[3][0]; // offset 0x0C

    // Row 1: copy from nlMatrix4 row 1 to target row 1
    mo[1][0] = mi.e2[0][1]; // offset 0x10
    mo[1][1] = mi.e2[1][1]; // offset 0x14
    mo[1][2] = mi.e2[2][1]; // offset 0x18
    mo[1][3] = mi.e2[3][1]; // offset 0x1C

    // Row 2: copy from nlMatrix4 row 2 to target row 2
    mo[2][0] = mi.e2[0][2]; // offset 0x20
    mo[2][1] = mi.e2[1][2]; // offset 0x24
    mo[2][2] = mi.e2[2][2]; // offset 0x28
    mo[2][3] = mi.e2[3][2]; // offset 0x2C
}
void nlMultMatrices(nlMatrix4& out, const nlMatrix4& a, const nlMatrix4& b)
{
    // PSMTX44Concat already protects either input aliasing the output. A
    // second scratch matrix here prevents a tail call on the ARM hot path.
    PSMTX44Concat(a.e2, b.e2, out.e2);
}
#define glxCopyMatrix oracle_copy
static void skin_before(void* pData, const glModelPacket* pPacket)
{
    float mSkinConcat[3][4];
    float mNormFast[3][4];
    nlMatrix4 nlMat;
    float modelViewMtx[3][4];
    float gxMat[3][4];
    float mNorm[3][4];
    u32 numMatrices;
    u32 i;
    int slot;
    GLSkinUserData* pSkin;
    u8 bInvXpose;

    numMatrices = *(u32*)pData;
    pSkin = (GLSkinUserData*)((u8*)pData + 4);

    if (prev_view == GLV_Characters)
        bInvXpose = glx_InvXposeChar;
    else
        bInvXpose = glx_InvXpose;

#if defined(PORT_VITA) && !defined(STRIKERS_VITA_GXM_LEAN_RENDER)
    // First prepared-Characters component: publish native frame parameters at
    // the original skin slot, before the draw. Other userdata, materials and
    // passes keep their current callbacks and current GX state dependencies.
    if (aurora::gx::fifo::can_write_native_pn_palette() && prev_view == GLV_Characters &&
        (glx_program == prog_3d_pointlit || glx_program == prog_3d_pointlit_dirt) &&
        g_bFastSkinPath && g_bMtxSkinMath && !glx_IsCoPlanarView &&
        pPacket->state.matrix == glGetIdentityMatrix() && numMatrices > 0 &&
        numMatrices <= aurora::gx::fifo::NativePnPaletteCapacity)
    {
        using aurora::gx::fifo::NativePnMatrix;
        std::array<NativePnMatrix, aurora::gx::fifo::NativePnPaletteCapacity> palette;
        bool valid = true;
        for (i = 0; i < numMatrices; ++i)
        {
            const int matrixId = pSkin[i].reg + 99;
            if (matrixId < 0 || matrixId > 27 || matrixId % 3 != 0)
            {
                valid = false;
                break;
            }
            palette[i].slot = static_cast<u32>(matrixId / 3);
        }
        const port::SkinMatrixResult* prepared = nullptr;
        if (valid && s_SkinPacketView >= 0 && s_SkinPacketView == SkinPacketViewIndex(prev_view))
            prepared = s_SkinPackets.find(reinterpret_cast<uintptr_t>(pPacket), pSkin, numMatrices,
                                          gx_mview, bInvXpose != 0);
        for (i = 0; valid && i < numMatrices; ++i)
        {
            const float* position;
            const float* normal;
            if (prepared != nullptr)
            {
                position = &prepared[i].position[0][0];
                normal = bInvXpose ? &prepared[i].normal[0][0] : position;
            }
            else
            {
                PSMTXConcat(gx_mview, *(const Mtx*)pSkin[i].mat, mSkinConcat);
                position = &mSkinConcat[0][0];
                normal = position;
                if (bInvXpose)
                {
                    valid = PSMTXInvXpose(mSkinConcat, mNormFast) != 0;
                    normal = &mNormFast[0][0];
                }
            }
            if (!valid) break;
            memcpy(palette[i].position.data(), position, sizeof(Mtx));
            for (unsigned row = 0; row < 3; ++row)
                memcpy(palette[i].normal.data() + row * 3, normal + row * 4, 3 * sizeof(float));
        }
        if (valid && aurora::gx::fifo::write_native_pn_palette(palette.data(), numMatrices))
        {
            if (prepared != nullptr) s_SkinPacketStats[s_SkinPacketView].usedMatrices += numMatrices;
            return;
        }
        // No native write has executed on rejection. Keep the original GX
        // loads, including unusual matrix IDs, slow math and recording modes.
    }
#endif

    if (g_bFastSkinPath && g_bMtxSkinMath && !glx_IsCoPlanarView && pPacket->state.matrix == glGetIdentityMatrix())
    {
#if defined(PORT_VITA)
        if (s_SkinPacketView >= 0 && s_SkinPacketView == SkinPacketViewIndex(prev_view))
        {
            const port::SkinMatrixResult* prepared = s_SkinPackets.find(
                reinterpret_cast<uintptr_t>(pPacket), pSkin, numMatrices, gx_mview, bInvXpose != 0);
            if (prepared != nullptr)
            {
                for (i = 0; i < numMatrices; ++i)
                {
                    const u32 matrixSlot = static_cast<u32>(pSkin[i].reg + 99);
                    GXLoadPosMtxImm(prepared[i].position, matrixSlot);
                    GXLoadNrmMtxImm(bInvXpose ? prepared[i].normal : prepared[i].position, matrixSlot);
                }
                s_SkinPacketStats[s_SkinPacketView].usedMatrices += numMatrices;
                return;
            }
            ++s_SkinPacketStats[s_SkinPacketView].fallbacks;
        }
#endif
        for (i = 0; i < numMatrices; i++, pSkin++)
        {
            PSMTXConcat(gx_mview, *(const Mtx*)pSkin->mat, mSkinConcat);
            slot = pSkin->reg + 99;
            GXLoadPosMtxImm(mSkinConcat, (u32)slot);
            if (bInvXpose)
            {
                PSMTXInvXpose(mSkinConcat, mNormFast);
                GXLoadNrmMtxImm(mNormFast, (u32)slot);
            }
            else
            {
                GXLoadNrmMtxImm(mSkinConcat, (u32)slot);
            }
        }
    }
    else
    {
        for (i = 0; i < numMatrices; i++, pSkin++)
        {
            if (g_bMtxSkinMath && !glx_IsCoPlanarView)
            {
                glxCopyMatrix(gxMat, *(nlMatrix4*)pPacket->state.matrix);
                PSMTXConcat(gxMat, gx_mview, modelViewMtx);
                PSMTXConcat(modelViewMtx, *(const Mtx*)pSkin->mat, mSkinConcat);
            }
            else
            {
                nlMultMatrices(nlMat, *(nlMatrix4*)pPacket->state.matrix, mview);
                glxCopyMatrix(modelViewMtx, nlMat);
                PSMTXConcat(modelViewMtx, *(const Mtx*)pSkin->mat, mSkinConcat);
            }
            slot = pSkin->reg + 99;
            GXLoadPosMtxImm(mSkinConcat, (u32)slot);
            if (bInvXpose)
            {
                PSMTXInvXpose(mSkinConcat, mNorm);
                GXLoadNrmMtxImm(mNorm, (u32)slot);
            }
            else
            {
                GXLoadNrmMtxImm(mSkinConcat, (u32)slot);
            }
        }
    }
}
static void skin_after(void* pData, const glModelPacket* pPacket)
{
    float mSkinConcat[3][4];
    float mNormFast[3][4];
    nlMatrix4 nlMat;
    float modelViewMtx[3][4];
    float gxMat[3][4];
    float mNorm[3][4];
    u32 numMatrices;
    u32 i;
    int slot;
    GLSkinUserData* pSkin;
    u8 bInvXpose;

    numMatrices = *(u32*)pData;
    pSkin = (GLSkinUserData*)((u8*)pData + 4);

    if (prev_view == GLV_Characters)
        bInvXpose = glx_InvXposeChar;
    else
        bInvXpose = glx_InvXpose;

#if defined(PORT_VITA) && !defined(STRIKERS_VITA_GXM_LEAN_RENDER)
    // First prepared-Characters component: publish native frame parameters at
    // the original skin slot, before the draw. Other userdata, materials and
    // passes keep their current callbacks and current GX state dependencies.
    if (aurora::gx::fifo::can_write_native_pn_palette() && prev_view == GLV_Characters &&
        (glx_program == prog_3d_pointlit || glx_program == prog_3d_pointlit_dirt) &&
        !glx_IsCoPlanarView &&
        pPacket->state.matrix == glGetIdentityMatrix() && numMatrices > 0 &&
        numMatrices <= aurora::gx::fifo::NativePnPaletteCapacity)
    {
        using aurora::gx::fifo::NativePnMatrix;
        std::array<NativePnMatrix, aurora::gx::fifo::NativePnPaletteCapacity> palette;
        bool valid = true;
        for (i = 0; i < numMatrices; ++i)
        {
            const int matrixId = pSkin[i].reg + 99;
            if (matrixId < 0 || matrixId > 27 || matrixId % 3 != 0)
            {
                valid = false;
                break;
            }
            palette[i].slot = static_cast<u32>(matrixId / 3);
        }
        const port::SkinMatrixResult* prepared = nullptr;
        if (valid && s_SkinPacketView >= 0 && s_SkinPacketView == SkinPacketViewIndex(prev_view))
            prepared = s_SkinPackets.find(reinterpret_cast<uintptr_t>(pPacket), pSkin, numMatrices,
                                          gx_mview, bInvXpose != 0);
        for (i = 0; valid && i < numMatrices; ++i)
        {
            const float* position;
            const float* normal;
            if (prepared != nullptr)
            {
                position = &prepared[i].position[0][0];
                normal = bInvXpose ? &prepared[i].normal[0][0] : position;
            }
            else
            {
                PSMTXConcat(gx_mview, *(const Mtx*)pSkin[i].mat, mSkinConcat);
                position = &mSkinConcat[0][0];
                normal = position;
                if (bInvXpose)
                {
                    valid = PSMTXInvXpose(mSkinConcat, mNormFast) != 0;
                    normal = &mNormFast[0][0];
                }
            }
            if (!valid) break;
            memcpy(palette[i].position.data(), position, sizeof(Mtx));
            for (unsigned row = 0; row < 3; ++row)
                memcpy(palette[i].normal.data() + row * 3, normal + row * 4, 3 * sizeof(float));
        }
        if (valid && aurora::gx::fifo::write_native_pn_palette(palette.data(), numMatrices))
        {
            if (prepared != nullptr) s_SkinPacketStats[s_SkinPacketView].usedMatrices += numMatrices;
            return;
        }
        // No native write has executed on rejection. Keep the original GX
        // loads, including unusual matrix IDs, slow math and recording modes.
    }
#endif

    if (!glx_IsCoPlanarView && pPacket->state.matrix == glGetIdentityMatrix())
    {
#if defined(PORT_VITA)
        if (s_SkinPacketView >= 0 && s_SkinPacketView == SkinPacketViewIndex(prev_view))
        {
            const port::SkinMatrixResult* prepared = s_SkinPackets.find(
                reinterpret_cast<uintptr_t>(pPacket), pSkin, numMatrices, gx_mview, bInvXpose != 0);
            if (prepared != nullptr)
            {
                for (i = 0; i < numMatrices; ++i)
                {
                    const u32 matrixSlot = static_cast<u32>(pSkin[i].reg + 99);
                    GXLoadPosMtxImm(prepared[i].position, matrixSlot);
                    GXLoadNrmMtxImm(bInvXpose ? prepared[i].normal : prepared[i].position, matrixSlot);
                }
                s_SkinPacketStats[s_SkinPacketView].usedMatrices += numMatrices;
                return;
            }
            ++s_SkinPacketStats[s_SkinPacketView].fallbacks;
        }
#endif
        for (i = 0; i < numMatrices; i++, pSkin++)
        {
            PSMTXConcat(gx_mview, *(const Mtx*)pSkin->mat, mSkinConcat);
            slot = pSkin->reg + 99;
            GXLoadPosMtxImm(mSkinConcat, (u32)slot);
            if (bInvXpose)
            {
                PSMTXInvXpose(mSkinConcat, mNormFast);
                GXLoadNrmMtxImm(mNormFast, (u32)slot);
            }
            else
            {
                GXLoadNrmMtxImm(mSkinConcat, (u32)slot);
            }
        }
    }
    else
    {
        // These inputs belong to this packet/view and do not change while GX
        // loads the bones. Retain the original co-planar math path and order.
        if (numMatrices != 0)
        {
            if (!glx_IsCoPlanarView)
            {
                glxCopyMatrix(gxMat, *(nlMatrix4*)pPacket->state.matrix);
                PSMTXConcat(gxMat, gx_mview, modelViewMtx);
            }
            else
            {
                nlMultMatrices(nlMat, *(nlMatrix4*)pPacket->state.matrix, mview);
                glxCopyMatrix(modelViewMtx, nlMat);
            }
        }
        for (i = 0; i < numMatrices; i++, pSkin++)
        {
            PSMTXConcat(modelViewMtx, *(const Mtx*)pSkin->mat, mSkinConcat);
            slot = pSkin->reg + 99;
            GXLoadPosMtxImm(mSkinConcat, (u32)slot);
            if (bInvXpose)
            {
                PSMTXInvXpose(mSkinConcat, mNorm);
                GXLoadNrmMtxImm(mNorm, (u32)slot);
            }
            else
            {
                GXLoadNrmMtxImm(mSkinConcat, (u32)slot);
            }
        }
    }
}
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
