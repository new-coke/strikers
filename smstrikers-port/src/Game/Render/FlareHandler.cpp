#include "Game/Render/FlareHandler.h"

#include "Game/Game.h"

#include "NL/nlMath.h"
#include "NL/nlMemory.h"
#include "NL/gl/glView.h"

#include "Game/GL/gluMeshWriter.h"

FlareHandler FlareHandler::instance;

/**
 * Offset/Address/Size: 0x818 | 0x8016A93C | size: 0x4
 */
void FlareHandler::Initialize()
{
    // EMPTY
}

/**
 * Offset/Address/Size: 0x7B8 | 0x8016A8DC | size: 0x60
 */
void FlareHandler::Cleanup()
{
    while (halos.m_headNode != 0)
    {
        efBaseNode* obj = halos.Remove();
        delete obj;
    }

    while (glows.m_headNode != 0)
    {
        efBaseNode* obj = glows.Remove();
        delete obj;
    }
}

/**
 * Offset/Address/Size: 0x6C4 | 0x8016A7E8 | size: 0xF4
 */
void FlareHandler::AddHalo(const nlMatrix4& mat)
{
    FlareStruct* p = new (nlMalloc(sizeof(FlareStruct), 8, false)) FlareStruct();

    p->type = Flare_Halo;

    nlVector3 out, dir;
    nlVec3Set(dir, 1.0f, 0.0f, 0.0f);
    nlMultDirVectorMatrix(out, dir, mat);

    float xx = out.x * out.x;
    float yy = out.y * out.y;
    float zz = out.z * out.z;
    p->size = nlSqrt(xx + yy + zz, true);

    p->colour.c[0] = 0xFF;
    p->colour.c[1] = 0xFF;
    p->colour.c[2] = 0xFF;
    p->colour.c[3] = 0xFF;

    p->worldPosition = *(nlVector3*)&mat.m41;

    halos.Insert(p);
}

/**
 * Offset/Address/Size: 0x5D0 | 0x8016A6F4 | size: 0xF4
 */
void FlareHandler::AddGlow(const nlMatrix4& mat)
{
    FlareStruct* p = new (nlMalloc(sizeof(FlareStruct), 8, false)) FlareStruct();

    p->type = Flare_Glow;

    nlVector3 out, dir;
    nlVec3Set(dir, 1.0f, 0.0f, 0.0f);
    nlMultDirVectorMatrix(out, dir, mat);

    float xx = out.x * out.x;
    float yy = out.y * out.y;
    float zz = out.z * out.z;
    p->size = nlSqrt(xx + yy + zz, true);

    p->colour.c[0] = 0xFF;
    p->colour.c[1] = 0xFF;
    p->colour.c[2] = 0xFF;
    p->colour.c[3] = 0xFF;

    p->worldPosition = *(nlVector3*)&mat.m41;

    glows.Insert(p);
}

/**
 * Offset/Address/Size: 0x290 | 0x8016A3B4 | size: 0x340
 */
void FlareHandler::AddFace(const FlareStruct* pFlare, GLMeshWriter* pMeshWriter)
{
    float sn;
    float cs;
    nlVector2 uv0;
    nlVector2 uv1;
    nlVector2 uv2;
    nlVector2 uv3;
    nlVector3 position;
    nlVector3 viewPosition;
    nlVector3 v[4];

    position = pFlare->worldPosition;
    nlMultPosVectorMatrix(viewPosition, position, viewMatrix);
    if (viewPosition.y == 0.0f)
    {
        sn = 0.0f;
        cs = 0.5f * pFlare->size;
    }
    else
    {
        nlSinCos(&sn, &cs, (u16)((s32)(8192.0f * (viewPosition.x / viewPosition.y)) + 0x2000));
        sn *= (pFlare->size * 0.5f);
        cs *= (pFlare->size * 0.5f);
    }

    nlVector3 a;
    a.x = cs * viewRight.x + sn * viewUp.x;
    a.y = cs * viewRight.y + sn * viewUp.y;
    a.z = cs * viewRight.z + sn * viewUp.z;

    nlVector3 b;
    b.x = (-sn) * viewRight.x + cs * viewUp.x;
    b.y = (-sn) * viewRight.y + cs * viewUp.y;
    b.z = (-sn) * viewRight.z + cs * viewUp.z;

    nlVec3Set(v[0], position.x + a.x + b.x, position.y + a.y + b.y, position.z + a.z + b.z);
    nlVec3Set(v[1], position.x - a.x + b.x, position.y - a.y + b.y, position.z - a.z + b.z);
    nlVec3Set(v[2], position.x - a.x - b.x, position.y - a.y - b.y, position.z - a.z - b.z);
    nlVec3Set(v[3], position.x + a.x - b.x, position.y + a.y - b.y, position.z + a.z - b.z);

    uv0.x = 0.0f;
    uv0.y = 0.0f;
    pMeshWriter->Texcoord(uv0);
    pMeshWriter->Colour(pFlare->colour);
    pMeshWriter->Vertex(v[0]);

    uv1.x = 1.0f;
    uv1.y = 0.0f;
    pMeshWriter->Texcoord(uv1);
    pMeshWriter->Colour(pFlare->colour);
    pMeshWriter->Vertex(v[1]);

    uv2.x = 1.0f;
    uv2.y = 1.0f;
    pMeshWriter->Texcoord(uv2);
    pMeshWriter->Colour(pFlare->colour);
    pMeshWriter->Vertex(v[2]);

    uv3.x = 0.0f;
    uv3.y = 1.0f;
    pMeshWriter->Texcoord(uv3);
    pMeshWriter->Colour(pFlare->colour);
    pMeshWriter->Vertex(v[3]);
}

/**
 * Offset/Address/Size: 0x0 | 0x8016A124 | size: 0x290
 */
void FlareHandler::Render()
{
    eGLStream stream_decl[3] = { GLStream_Position, GLStream_Colour, GLStream_Diffuse }; // sp8

    if ((g_pGame->mbCaptainShotToScoreOn == false) && ((halos.m_headNode != NULL) || (glows.m_headNode != NULL)))
    {
        glViewGetViewMatrix(GLV_Unshadowed, viewMatrix);
        nlVec3Set(viewRight, viewMatrix.m11, viewMatrix.m21, viewMatrix.m31); // this->unk40 = this->unk0;        // this->unk44 = this->unk10;        // this->unk48 = this->unk20;
        nlVec3Set(viewUp, viewMatrix.m12, viewMatrix.m22, viewMatrix.m32);
        glSetDefaultState(1);
        glSetCurrentProgram(glGetProgram("3d unlit"));
        glSetRasterState(GLS_DepthWrite, 0U);
        glSetRasterState(GLS_Culling, 0U);
        glSetRasterState(GLS_AlphaBlend, 3U);
        glSetRasterState(GLS_AlphaTest, 1U);
        glSetCurrentRasterState(glHandleizeRasterState());
        glSetCurrentTexture(glGetTexture("global/white"), GLTT_Diffuse);

        if (halos.m_headNode != NULL)
        {
            GLMeshWriter writer;
            glSetCurrentTexture(glGetTexture("global/flare_halo"), (eGLTextureType)0);
            if (writer.Begin(halos.m_numNodes * 4, (eGLPrimitive)3, 3, stream_decl, 0) != 0)
            {
                FlareStruct* halo = (FlareStruct*)halos.m_headNode;
                while (halo != NULL)
                {
                    AddFace(halo, &writer);
                    halo = (FlareStruct*)halo->m_nextNode;
                }

                if (writer.End())
                {
                    glViewAttachModel((eGLView)0x13, writer.GetModel());
                }
            }
        }

        if (glows.m_headNode != NULL)
        {
            GLMeshWriter writer;
            glSetCurrentTexture(glGetTexture("global/flare_glow"), (eGLTextureType)0);
            if (writer.Begin(glows.m_numNodes * 4, (eGLPrimitive)3, 3, stream_decl, 0) != 0)
            {

                FlareStruct* glow = (FlareStruct*)glows.m_headNode;
                while (glow != NULL)
                {
                    AddFace(glow, &writer);
                    glow = (FlareStruct*)glow->m_nextNode;
                }

                if (writer.End())
                {
                    glViewAttachModel((eGLView)0x13, writer.GetModel());
                }
            }
        }
    }
}
