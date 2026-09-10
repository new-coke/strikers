#include "Game/Debug/ShapeRender.h"
#include <string.h>

#include "Game/GL/gluMeshWriter.h"

#include "NL/gl/glDraw2.h"
#include "NL/gl/glLightUserData.h"
#include "NL/gl/glMemory.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glState.h"

ShapeRender g_ShapeRenderer;
static unsigned char g_bWire;
static unsigned long UnlitProgram = glGetProgram("3d unlit");
static unsigned long LitProgram = glGetProgram("3d pointlit");
const u32 WhiteTexture = glGetTexture("global/white");

static unsigned char g_bLit = 1;

/**
 * Offset/Address/Size: 0x149C | 0x801FC72C | size: 0x418
 */
void ShapeRender::CreateHemisphereGeometry(PrimitiveShape& prim)
{
    nlVector3 vNormal;
    nlVector3* pdst;
    nlVector3* ndst;
    nlVector2* tdst;
    int nRing;
    int angle0;
    int angle1;
    int angle;
    int angle90;
    float ring0;
    float ring1;
    float z0;
    float z1;
    int nSegment;
    float x0;
    float y0;
    float x1;
    float y1;
    float lengthSquared;
    float invLen;

    prim.vertCount = 0xA0;
    prim.position = (nlVector3*)glResourceAlloc(sizeof(nlVector3), GLM_VertexData);
    prim.normal = (nlVector3*)glResourceAlloc(sizeof(nlVector3), GLM_VertexData);
    prim.texcoord = (nlVector2*)glResourceAlloc(sizeof(nlVector2), GLM_VertexData);

    pdst = prim.position;
    ndst = prim.normal;
    tdst = prim.texcoord;

    for (nRing = 0; nRing < 5; nRing++)
    {
        float fAngle;

        fAngle = (float)nRing;
        fAngle *= 0.31415927f;
        angle0 = (int)(fAngle * 10430.378f);
        z0 = 0.5f * nlSin((u16)angle0);

        fAngle = (float)(nRing + 1);
        fAngle *= 0.31415927f;
        angle1 = (int)(fAngle * 10430.378f);
        z1 = 0.5f * nlSin((u16)angle1);

        ring0 = nlSin((u16)((u16)angle0 + 0x4000));
        ring1 = nlSin((u16)((u16)angle1 + 0x4000));

        for (nSegment = 0; nSegment < 0x10; nSegment++)
        {
            float fSegmentAngle;

            fSegmentAngle = (float)nSegment;
            angle = (int)((fSegmentAngle *= 0.41887903f) * 10430.378f);

            x0 = 0.5f * (ring0 * nlSin((u16)angle));

            angle90 = (u16)angle + 0x4000;
            y0 = 0.5f * (ring0 * nlSin((u16)angle90));

            x1 = 0.5f * (ring1 * nlSin((u16)angle));
            y1 = 0.5f * (ring1 * nlSin((u16)angle90));

            vNormal.x = x0;
            vNormal.y = y0;
            vNormal.z = z0;

            lengthSquared = vNormal.GetLengthSq3D();
            invLen = nlRecipSqrt(lengthSquared, true);

            pdst->x = x0;
            nlVec3Scale(vNormal, invLen);
            pdst->y = y0;
            pdst->z = z0;
            *ndst = vNormal;

            tdst->x = (float)nSegment / 15.0f;
            tdst->y = (float)nRing / 5.0f;

            vNormal.x = x1;
            vNormal.y = y1;
            vNormal.z = z1;

            lengthSquared = vNormal.GetLengthSq3D();
            invLen = nlRecipSqrt(lengthSquared, true);

            pdst[1].x = x1;
            nlVec3Scale(vNormal, invLen);
            pdst[1].y = y1;
            pdst[1].z = z1;
            ndst[1] = vNormal;

            tdst[1].x = (float)nSegment / 15.0f;
            tdst[1].y = (float)(nRing + 1) / 5.0f;

            pdst += 2;
            ndst += 2;
            tdst += 2;
        }
    }
}

/**
 * Offset/Address/Size: 0x11C0 | 0x801FC450 | size: 0x2DC
 */
void ShapeRender::CreateFlatCylinderEndGeometry(PrimitiveShape& prim)
{
    nlVector3 vNormal;
    int angle;
    int angle90;
    nlVector3* pdst;
    nlVector3* ndst;
    nlVector2* tdst;
    int nSegment;
    float z0;
    float angleFactor;
    float segmentFactor;
    float texDenom;
    float x0;
    float y0;
    float x1;
    float y1;
    float sinAngle;
    float invLen;

    prim.vertCount = 0x20;
    prim.position = (nlVector3*)glResourceAlloc(sizeof(nlVector3), GLM_VertexData);
    prim.normal = (nlVector3*)glResourceAlloc(sizeof(nlVector3), GLM_VertexData);
    prim.texcoord = (nlVector2*)glResourceAlloc(sizeof(nlVector2), GLM_VertexData);

    float half = 0.5f;
    float one = 1.0f;
    z0 = 0.0f;
    angleFactor = 10430.378f;
    segmentFactor = 0.41887903f;
    texDenom = 15.0f;

    pdst = prim.position;
    ndst = prim.normal;
    tdst = prim.texcoord;

    for (nSegment = 0; nSegment < 0x10; nSegment++)
    {
        angle = (int)(angleFactor * ((float)nSegment * segmentFactor));

        sinAngle = nlSin((u16)angle);
        x0 = half * (one * sinAngle);

        angle90 = (u16)angle + 0x4000;
        y0 = half * (one * nlSin((u16)angle90));

        x1 = half * (z0 * nlSin((u16)angle));
        y1 = half * (z0 * nlSin((u16)angle90));

        vNormal.x = x0;
        vNormal.y = y0;
        vNormal.z = z0;

        invLen = nlRecipSqrt(vNormal.GetLengthSq3D(), true);

        pdst->x = x0;
        nlVec3Scale(vNormal, invLen);
        pdst->y = y0;
        pdst->z = z0;
        *ndst = vNormal;

        tdst->x = (float)nSegment / texDenom;
        tdst->y = z0;

        vNormal.x = x1;
        vNormal.y = y1;
        vNormal.z = z0;

        invLen = nlRecipSqrt(vNormal.GetLengthSq3D(), true);

        pdst[1].x = x1;
        pdst[1].y = y1;
        pdst[1].z = z0;
        nlVec3Scale(vNormal, invLen);
        ndst[1] = vNormal;

        tdst[1].x = (float)nSegment / texDenom;
        tdst[1].y = one;

        pdst += 2;
        ndst += 2;
        tdst += 2;
    }
}

/**
 * Offset/Address/Size: 0xE14 | 0x801FC0A4 | size: 0x3AC
 */
void ShapeRender::CreateCylinderGeometry(PrimitiveShape& prim)
{
    nlVector3 vNormal;
    nlVector3* pdst;
    nlVector3* ndst;
    nlVector2* tdst;
    int nRing;
    int angle;
    int angle90;
    float z0;
    float z1;
    float x0;
    float y0;
    float x1;
    float y1;
    float invLen;

    prim.vertCount = 0x40;
    prim.position = (nlVector3*)glResourceAlloc(sizeof(nlVector3), GLM_VertexData);
    prim.normal = (nlVector3*)glResourceAlloc(sizeof(nlVector3), GLM_VertexData);
    prim.texcoord = (nlVector2*)glResourceAlloc(sizeof(nlVector2), GLM_VertexData);

    pdst = prim.position;
    ndst = prim.normal;
    tdst = prim.texcoord;

    for (nRing = 0; nRing < 2; nRing++)
    {
        float ringScale = 0.5f;

        z0 = -0.5f + (float)nRing * ringScale;
        z1 = -0.5f + (float)(nRing + 1) * ringScale;

        nlSin((u16)((u16)(int)(10430.378f * ((float)nRing * ringScale)) + 0x4000));
        nlSin((u16)((u16)(int)(10430.378f * ((float)(nRing + 1) * ringScale)) + 0x4000));

        for (int nSegment = 0; nSegment < 0x10; nSegment++)
        {
            float fSegmentAngle;

            fSegmentAngle = (float)nSegment;
            angle = (int)((fSegmentAngle *= 0.41887903f) * 10430.378f);

            x0 = 0.5f * nlSin((u16)angle);

            angle90 = (u16)angle + 0x4000;
            y0 = 0.5f * nlSin((u16)angle90);

            x1 = 0.5f * nlSin((u16)angle);
            y1 = 0.5f * nlSin((u16)angle90);

            vNormal.x = x0;
            vNormal.y = y0;
            vNormal.z = z0;

            invLen = nlRecipSqrt(nlVec3LengthSquared(vNormal), true);

            pdst->x = x0;
            nlVec3Scale(vNormal, invLen);
            pdst->y = y0;
            pdst->z = z0;
            *ndst = vNormal;

            tdst->x = (float)nSegment / 15.0f;
            tdst->y = (float)nRing / 2.0f;

            vNormal.x = x1;
            vNormal.y = y1;
            vNormal.z = z1;

            invLen = nlRecipSqrt(nlVec3LengthSquared(vNormal), true);

            pdst[1].x = x1;
            nlVec3Scale(vNormal, invLen);
            pdst[1].y = y1;
            pdst[1].z = z1;
            ndst[1] = vNormal;

            tdst[1].x = (float)nSegment / 15.0f;
            tdst[1].y = (float)(nRing + 1) / 2.0f;

            pdst += 2;
            ndst += 2;
            tdst += 2;
        }
    }
}

/**
 * Offset/Address/Size: 0xAC0 | 0x801FBD50 | size: 0x354
 */
void ShapeRender::DrawSpherePrimitive(const nlMatrix4& mat_world, float radius, const nlColour& colour) const
{
    nlMatrix4 mat_hemiTop;
    nlMatrix4 mat_hemiBottom;
    nlMatrix4 mat_rot;
    unsigned long topLitProgram;
    void* pLightData;

    radius = radius / 0.5f;

    nlMakeScaleMatrix(mat_hemiTop, radius, radius, radius);
    nlMakeRotationMatrixX(mat_rot, 3.1415927f);
    nlMultMatrices(mat_hemiBottom, mat_hemiTop, mat_rot);
    nlMultMatrices(mat_hemiTop, mat_hemiTop, mat_world);
    nlMultMatrices(mat_hemiBottom, mat_hemiBottom, mat_world);

    {
        uintptr_t matrix = glAllocMatrix();
        if ((u32)(matrix + 0x10000) != 0xFFFF)
        {
            glSetMatrix(matrix, mat_hemiTop);
        }

        glModelPacket* packet;
        void* pUserData;
        glModel* pModel = glModelDupNoStreams(m_Hemisphere.model, true, false);

        if (g_bLit)
        {
            void* pColourData;
            pUserData = glUserAlloc(GLUD_Diffuse, sizeof(nlFloatColour), false);
            pColourData = glUserGetData(pUserData);

            ((nlFloatColour*)pColourData)->c[0] = (float)colour.c[0] / 255.0f;
            ((nlFloatColour*)pColourData)->c[1] = (float)colour.c[1] / 255.0f;
            ((nlFloatColour*)pColourData)->c[2] = (float)colour.c[2] / 255.0f;
            ((nlFloatColour*)pColourData)->c[3] = (float)colour.c[3] / 255.0f;
        }
        else
        {
            void* pColourData;
            pUserData = glUserAlloc(GLUD_ConstantColour, sizeof(nlColour), false);
            pColourData = glUserGetData(pUserData);
            // PORT: was an eight-byte write into a four-byte glUserAlloc(sizeof(nlColour)) allocation.
            memcpy(pColourData, &colour, sizeof(nlColour));
        }

        packet = pModel->packets;
        pLightData = m_pLightUserData;
        topLitProgram = LitProgram;

        while (packet < &pModel->packets[pModel->numPackets])
        {
            packet->state.matrix = matrix;
            glUserAttach(pUserData, packet, false);

            if (g_bLit && pLightData != NULL)
            {
                packet->state.program = topLitProgram;
                glUserAttach(pLightData, packet, false);
            }

            packet++;
        }

        glViewAttachModel(m_eView, pModel);
    }

    {
        uintptr_t matrix = glAllocMatrix();
        if ((u32)(matrix + 0x10000) != 0xFFFF)
        {
            glSetMatrix(matrix, mat_hemiBottom);
        }

        glModelPacket* packet;
        void* pUserData;
        glModel* pModel = glModelDupNoStreams(m_Hemisphere.model, true, false);

        if (g_bLit)
        {
            void* pColourData;
            pUserData = glUserAlloc(GLUD_Diffuse, sizeof(nlFloatColour), false);
            pColourData = glUserGetData(pUserData);

            ((nlFloatColour*)pColourData)->c[0] = (float)colour.c[0] / 255.0f;
            ((nlFloatColour*)pColourData)->c[1] = (float)colour.c[1] / 255.0f;
            ((nlFloatColour*)pColourData)->c[2] = (float)colour.c[2] / 255.0f;
            ((nlFloatColour*)pColourData)->c[3] = (float)colour.c[3] / 255.0f;
        }
        else
        {
            void* pColourData;
            pUserData = glUserAlloc(GLUD_ConstantColour, sizeof(nlColour), false);
            pColourData = glUserGetData(pUserData);
            // PORT: was an eight-byte write into a four-byte glUserAlloc(sizeof(nlColour)) allocation.
            memcpy(pColourData, &colour, sizeof(nlColour));
        }

        packet = pModel->packets;
        pLightData = m_pLightUserData;
        const unsigned long bottomLitProgram = LitProgram;

        while (packet < &pModel->packets[pModel->numPackets])
        {
            packet->state.matrix = matrix;
            glUserAttach(pUserData, packet, false);

            if (g_bLit && pLightData != NULL)
            {
                packet->state.program = bottomLitProgram;
                glUserAttach(pLightData, packet, false);
            }

            packet++;
        }

        glViewAttachModel(m_eView, pModel);
    }
}

/**
 * Dead-stripped by mwld (MarioSoccerZ.MAP: UNUSED, size 0x1C0).
 */
void ShapeRender::DrawBoxPrimitive(const nlMatrix4& mat_world, float fX, float fY, float fZ, const nlColour& colour) const
{
    nlMatrix4 objMatrix;
    nlMatrix4 mat_out;

    nlMakeScaleMatrix(objMatrix, fX, fY, fZ);
    nlMultMatrices(mat_out, objMatrix, mat_world);

    {
        uintptr_t matrix = glAllocMatrix();
        if ((u32)(matrix + 0x10000) != 0xFFFF)
        {
            glSetMatrix(matrix, mat_out);
        }

        glModelPacket* packet;
        void* pUserData;
        glModel* pModel = glModelDupNoStreams(m_Box.model, true, false);

        if (g_bLit)
        {
            void* pColourData;
            pUserData = glUserAlloc(GLUD_Diffuse, sizeof(nlFloatColour), false);
            pColourData = glUserGetData(pUserData);

            ((nlFloatColour*)pColourData)->c[0] = (float)colour.c[0] / 255.0f;
            ((nlFloatColour*)pColourData)->c[1] = (float)colour.c[1] / 255.0f;
            ((nlFloatColour*)pColourData)->c[2] = (float)colour.c[2] / 255.0f;
            ((nlFloatColour*)pColourData)->c[3] = (float)colour.c[3] / 255.0f;
        }
        else
        {
            void* pColourData;
            pUserData = glUserAlloc(GLUD_ConstantColour, sizeof(nlColour), false);
            pColourData = glUserGetData(pUserData);
            // PORT: was an eight-byte write into a four-byte glUserAlloc(sizeof(nlColour)) allocation.
            memcpy(pColourData, &colour, sizeof(nlColour));
        }

        packet = pModel->packets;
        void* pLightData = m_pLightUserData;
        const unsigned long boxLitProgram = LitProgram;

        while (packet < &pModel->packets[pModel->numPackets])
        {
            packet->state.matrix = matrix;
            glUserAttach(pUserData, packet, false);

            if (g_bLit && pLightData != NULL)
            {
                packet->state.program = boxLitProgram;
                glUserAttach(pLightData, packet, false);
            }

            packet++;
        }

        glViewAttachModel(m_eView, pModel);
    }
}

/**
 * Offset/Address/Size: 0x930 | 0x801FBBC0 | size: 0x190
 */
void ShapeRender::DrawLine3D(const nlVector3& p0, const nlVector3& p1, const nlColour& colour, bool bWithDepth) const
{
    GLMeshWriter writer;

    glSetDefaultState(bWithDepth);
    glSetCurrentMatrix(glGetIdentityMatrix());
    glSetCurrentTexture(WhiteTexture, GLTT_Diffuse);
    glSetCurrentProgram(glGetProgram("3d unlit"));

    const eGLStream streams[3] = { GLStream_Position, GLStream_Colour, GLStream_Diffuse };

    if (writer.Begin(2, GLP_LineList, 3, streams, false))
    {
        nlVector2 uv0;
        nlVector2 uv1;

        writer.Colour(colour);
        uv0.x = 0.0f;
        uv0.y = 0.0f;
        ((GLMeshWriterCore*)&writer)->Texcoord(uv0);
        writer.Vertex(p0);

        writer.Colour(colour);
        uv1.x = 0.0f;
        uv1.y = 0.0f;
        ((GLMeshWriterCore*)&writer)->Texcoord(uv1);
        writer.Vertex(p1);

        if (!writer.End())
        {
            return;
        }

        glViewAttachModel(m_eView, 2, writer.GetModel());
    }
}

/**
 * Dead-stripped by mwld (MarioSoccerZ.MAP: UNUSED, size 0x278).
 */
void ShapeRender::DrawEllipse2D(const nlVector3& p0, float fRadius, float fScaleX, float fScaleY, const nlColour& colour, bool bWithDepth) const
{
    GLMeshWriter mesh;

    glSetDefaultState(bWithDepth);
    glSetCurrentMatrix(glGetIdentityMatrix());
    glSetCurrentTexture(WhiteTexture, GLTT_Diffuse);
    glSetCurrentProgram(UnlitProgram);

    const eGLStream stream_decl[3] = { GLStream_Position, GLStream_Colour, GLStream_Diffuse };

    if (mesh.Begin(31, g_bWire ? GLP_LineStrip : GLP_TriFan, 3, stream_decl, false))
    {
        nlVector3 v3point;
        nlVector2 uv0;

        v3point.z = p0.z;
        v3point.x = p0.x;
        v3point.y = p0.y;

        float fRadians = 0.0f;

        mesh.Colour(colour);
        uv0.x = 0.0f;
        uv0.y = 0.0f;
        ((GLMeshWriterCore*)&mesh)->Texcoord(uv0);
        mesh.Vertex(v3point);

        const float angleScale = 10430.378f;
        int i = 0;
        const float uvZero = 0.0f;
        const float angleStep = 0.20943951f;
        nlVector2 uv1;

        while (i < 30)
        {
            nlSinCos(&v3point.x, &v3point.y, (unsigned short)(int)(angleScale * fRadians));
            v3point.x = p0.x + fScaleX * (v3point.x * fRadius);
            v3point.y = p0.y + fScaleY * (v3point.y * fRadius);

            mesh.Colour(colour);
            uv1.x = uvZero;
            uv1.y = uvZero;
            ((GLMeshWriterCore*)&mesh)->Texcoord(uv1);
            mesh.Vertex(v3point);

            i++;
            fRadians += angleStep;
        }

        if (!mesh.End())
        {
            return;
        }

        glViewAttachModel(m_eView, 2, mesh.GetModel());
    }
}

/**
 * Dead-stripped by mwld (MarioSoccerZ.MAP: UNUSED, size 0x264).
 */
void ShapeRender::DrawCircle2D(const nlVector3& p0, float fRadius, const nlColour& colour, bool bWithDepth) const
{
    DrawEllipse2D(p0, fRadius, 1.0f, 1.0f, colour, bWithDepth);
}

/**
 * Offset/Address/Size: 0x7EC | 0x801FBA7C | size: 0x144
 */
void ShapeRender::DrawRectangle2D(float x, float y, float w, float h, float z, const nlColour& colour, int view) const
{
    glSetDefaultState(false);
    glSetRasterState(GLS_AlphaBlend, 1);
    glSetRasterState(GLS_AlphaTest, 1);
    glSetRasterState(GLS_AlphaTestRef, 0);
    glSetCurrentRasterState(glHandleizeRasterState());
    glSetCurrentTexture(glGetTexture("global/white"), GLTT_Diffuse);

    float right = y + h;
    float bottom = x + w;

    glPoly2 poly;
    poly.m_pos[0].x = x;
    poly.m_pos[0].y = y;

    glPoly2* pPoly = &poly;

    poly.m_pos[1].x = x;
    poly.m_pos[1].y = right;
    poly.m_pos[2].x = bottom;
    poly.m_pos[2].y = right;
    poly.m_pos[3].x = bottom;
    poly.m_pos[3].y = y;

    poly.m_colour[3] = colour;
    poly.m_colour[2] = colour;
    poly.m_colour[1] = colour;
    poly.m_colour[0] = colour;

    poly.depth = z;

    eGLView v = (eGLView)view;
    if (view == -1)
    {
        v = GLV_Debug;
    }
    pPoly->Attach(v, 0, NULL, -1);
}

static inline void BuildShapeMesh(PrimitiveShape& shape)
{
    extern unsigned long UnlitProgram;
    eGLStream stream_decl[4] = { GLStream_Position, GLStream_Colour, GLStream_Normal, GLStream_Diffuse };
    GLMeshWriter mesh;
    nlColour colour;
    nlVector3* pPosition;
    nlVector2* pTexcoord;
    nlVector3* pNormal;

    shape.model = NULL;
    colour.c[0] = 0xFF;
    colour.c[1] = 0xFF;
    colour.c[2] = 0xFF;
    colour.c[3] = 0xFF;

    pPosition = shape.position;
    pNormal = shape.normal;
    pTexcoord = shape.texcoord;

    glSetDefaultState(true);
    glSetCurrentMatrix(glGetIdentityMatrix());
    glSetCurrentTexture(WhiteTexture, GLTT_Diffuse);
    glSetCurrentProgram(UnlitProgram);

    if (mesh.Begin(shape.vertCount, GLP_TriStrip, 4, stream_decl, true))
    {
        int index = 0;
        while (index < shape.vertCount)
        {
            mesh.Colour(colour);
            mesh.Normal(*pNormal);
            mesh.Texcoord(*pTexcoord);
            mesh.Vertex(*pPosition);

            pNormal++;
            pTexcoord++;
            pPosition++;
            index++;
        }

        if (mesh.End())
        {
            shape.model = mesh.GetModel();
        }
    }
}

static inline void* CreateLightData()
{
    void* p = glUserAlloc(GLUD_Light, sizeof(unsigned long) + sizeof(GLLightUserData), true);
    unsigned long* p32 = (unsigned long*)glUserGetData(p);
    *p32++ = 1;
    GLLightUserData* pLight = (GLLightUserData*)(p32);

    pLight->worldPosition.x = 0.5f;
    pLight->worldPosition.y = 0.5f;
    pLight->worldPosition.z = 10.0f;
    pLight->colour.c[0] = 0.5f;
    pLight->colour.c[1] = 0.5f;
    pLight->colour.c[2] = 1.0f;
    pLight->colour.c[3] = 0.0f;
    pLight->intensity = 1.0f;
    pLight->innerRadius = 0.0f;
    pLight->outerRadius = 30.0f;

    return p;
}

static inline void AllocResource(PrimitiveShape* pPrimitive, int nVerts)
{
    pPrimitive->position = (nlVector3*)glResourceAlloc(nVerts * sizeof(nlVector3), GLM_VertexData);
    pPrimitive->normal = (nlVector3*)glResourceAlloc(nVerts * sizeof(nlVector3), GLM_VertexData);
    pPrimitive->texcoord = (nlVector2*)glResourceAlloc(nVerts * sizeof(nlVector2), GLM_VertexData);
    pPrimitive->vertCount = nVerts;
}

static inline void CreateBoxGeometry(PrimitiveShape& prim)
{
    static int ind_vert[24] = {
        0,
        2,
        3,
        1,
        4,
        5,
        7,
        6,
        0,
        1,
        5,
        4,
        1,
        3,
        7,
        5,
        3,
        2,
        6,
        7,
        2,
        0,
        4,
        6,
    };
    static int ind_uv[24] = {
        1,
        3,
        2,
        0,
        0,
        1,
        3,
        2,
        0,
        1,
        3,
        2,
        0,
        1,
        3,
        2,
        0,
        1,
        3,
        2,
        0,
        1,
        3,
        2,
    };
    static nlVector3 data_vert[8] = {
        { -0.5f, -0.5f, -0.5f },
        { 0.5f, -0.5f, -0.5f },
        { -0.5f, 0.5f, -0.5f },
        { 0.5f, 0.5f, -0.5f },
        { -0.5f, -0.5f, 0.5f },
        { 0.5f, -0.5f, 0.5f },
        { -0.5f, 0.5f, 0.5f },
        { 0.5f, 0.5f, 0.5f },
    };
    static nlVector2 data_uv[4] = {
        { 0.0f, 0.0f },
        { 1.0f, 0.0f },
        { 0.0f, 1.0f },
        { 1.0f, 1.0f },
    };
    static nlVector3 data_norm[24] = {
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, -1.0f },

        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f },

        { 0.0f, -1.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f },

        { 1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f },

        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },

        { -1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f },
    };
    static int tri_map[6] = { 0, 1, 2, 3, 0, 2 };

    int i;
    int iQuad;
    nlVector3* pdst = prim.position;
    nlVector3* ndst = prim.normal;
    nlVector2* tdst = prim.texcoord;
    nlVector3* psrc[4];
    nlVector3* nsrc[4];
    nlVector2* tsrc[4];

    for (iQuad = 0; iQuad < 6; iQuad++)
    {
        for (i = 0; i < 4; i++)
        {
            psrc[i] = &data_vert[ind_vert[iQuad * 4 + i]];
            nsrc[i] = &data_norm[iQuad * 4 + i];
            tsrc[i] = &data_uv[ind_uv[iQuad * 4 + i]];
        }

        for (i = 0; i < 6; i += 2)
        {
            *pdst = *psrc[tri_map[i]];
            *ndst = *nsrc[tri_map[i]];
            *tdst = *tsrc[tri_map[i]];

            pdst[1] = *psrc[tri_map[i + 1]];
            ndst[1] = *nsrc[tri_map[i + 1]];
            tdst[1] = *tsrc[tri_map[i + 1]];

            pdst += 2;
            ndst += 2;
            tdst += 2;
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x801FB290 | size: 0x7EC
 */
void ShapeRender::Initialize()
{
    if (!m_Initialized)
    {
        m_Initialized = true;
        AllocResource(&m_Box, 0x24);
        CreateBoxGeometry(m_Box);
        CreateCylinderGeometry(m_Cylinder);
        CreateHemisphereGeometry(m_Hemisphere);
        CreateFlatCylinderEndGeometry(m_FlatCylinderEnd);
        BuildShapeMesh(m_Box);
        BuildShapeMesh(m_Cylinder);
        BuildShapeMesh(m_Hemisphere);
        BuildShapeMesh(m_FlatCylinderEnd);
        m_pLightUserData = CreateLightData();
        m_eView = (eGLView)7;
    }
}
