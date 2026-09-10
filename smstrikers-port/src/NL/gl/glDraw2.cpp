#include "NL/gl/glDraw2.h"

#include "NL/nlMath.h"

#include "NL/gl/gl.h"
#include "NL/gl/glState.h"
#include "NL/gl/glView.h"
#include "NL/gl/glStruct.h"
#include "NL/gl/glMatrix.h"

#include "NL/gl/glModel.h"
#include "NL/gl/glUserData.h"

#include "Game/GL/gluMeshWriter.h"

static int StripMap[4] = { 3, 0, 2, 1 };
static int QuadMap[4] = { 0, 1, 2, 3 };
static int TriListMap[6] = { 3, 0, 2, 0, 2, 1 };

static unsigned long _defaultProgram = glGetProgram("2d unlit");

/**
 * Offset/Address/Size: 0x610 | 0x801D7C10 | size: 0x210
 */
bool glPoly2::Attach(eGLView view, int layer, uintptr_t* pMatrixHandle, unsigned long programHandle)
{
    eGLStream streamsDesc[3] = { GLStream_Position, GLStream_Colour, GLStream_Diffuse };
    GLMeshWriter writer;

    if ((programHandle + 0x10000) == 0xFFFF)
    {
        programHandle = _defaultProgram;
    }

    unsigned long program;
    uintptr_t matrix;              // PORT: a matrix handle
    unsigned long texconfig;

    texconfig = gl_GetCurrentStateBundle()->texconfig;
    program = glSetCurrentProgram(programHandle);
    matrix = pMatrixHandle ? *pMatrixHandle : glGetIdentityMatrix();
    matrix = glSetCurrentMatrix(matrix);

    eGLPrimitive prim;
    int* pMap;
    if (glHasQuads())
    {
        prim = GLP_QuadList;
        pMap = QuadMap;
    }
    else
    {
        prim = GLP_TriStrip;
        pMap = StripMap;
    }

    if (writer.Begin(4, prim, texconfig + 2, streamsDesc, false))
    {
        for (int i = 0; i < 4; i++)
        {
            int index = pMap[i];
            writer.Colour(m_colour[index]);
            if (texconfig != 0)
            {
                writer.Texcoord(m_uv[index]);
            }
            float fx = m_pos[index].x;
            float fy = m_pos[index].y;
            float fz = depth;
            nlVector3 pos;
            pos.x = fx;
            pos.y = fy;
            pos.z = fz;
            writer.Position(pos);
        }
        if (!writer.End())
        {
            return false;
        }

        glViewAttachModel(view, layer, writer.GetModel());
    }
    else
    {
        return false;
    }

    glSetCurrentProgram(program);
    glSetCurrentMatrix(matrix);

    return true;
}

/**
 * Offset/Address/Size: 0x33C | 0x801D793C | size: 0x2D4
 */
bool glAttachPoly2(eGLView view, unsigned long numPolys, glPoly2* pPolys, uintptr_t* pMatrixHandle, const void* pUserData)
{
    if (gl_GetCurrentStateBundle()->texconfig == 1)
    {
        unsigned int i;
        unsigned int j;
        unsigned long program;
        uintptr_t matrix;              // PORT: a matrix handle
        eGLStream streamsDesc[3] = { GLStream_Position, GLStream_Colour, GLStream_Diffuse };
        GLMeshWriter writer;
        unsigned int numVerts;
        int* mapArray;
        eGLPrimitive prim;
        glPoly2* pPoly;
        unsigned int k;

        if (glHasQuads())
        {
            mapArray = QuadMap;
            numVerts = 4;
            prim = GLP_QuadList;
        }
        else
        {
            mapArray = TriListMap;
            numVerts = 6;
            prim = GLP_TriList;
        }

        program = glSetCurrentProgram(_defaultProgram);
        matrix = glSetCurrentMatrix(pMatrixHandle ? *pMatrixHandle : glGetIdentityMatrix());

        if (writer.Begin((int)(numVerts * numPolys), prim, 3, streamsDesc, false))
        {
            pPoly = pPolys;
            i = 0;
            while (i < numPolys)
            {
                j = 0;
                while (j < numVerts)
                {
                    k = mapArray[j];
                    writer.Colour(pPoly->m_colour[k]);
                    writer.Texcoord(pPoly->m_uv[k]);

                    nlVector3 pos;
                    nlVec3Set(pos, pPoly->m_pos[k].x, pPoly->m_pos[k].y, pPoly->depth);
                    writer.Position(pos);

                    j++;
                }

                i++;
                pPoly++;
            }

            if (!writer.End())
            {
                return false;
            }

            if (pUserData == NULL)
            {
                glViewAttachModel(view, writer.GetModel());
            }
            else
            {
                // Declare pPacket (the packet iterator) before newModel so MWCC
                // colors it into the lower callee-saved reg r20 and newModel into
                glModelPacket* pPacket;
                glModel* newModel = glModelDupNoStreams(writer.GetModel(), true, false);
                pPacket = newModel->packets;
                while (pPacket < newModel->packets + newModel->numPackets)
                {
                    glUserAttach(pUserData, pPacket, false);
                    pPacket++;
                }
                glViewAttachModel(view, newModel);
            }
        }
        else
        {
            return false;
        }

        glSetCurrentProgram(program);
        glSetCurrentMatrix(matrix);
    }
    else
    {
        glPoly2* pPoly = pPolys;
        unsigned int i = 0;
        while (i < numPolys)
        {
            if (!pPoly->Attach(view, 0, 0, 0xFFFFFFFFu))
            {
                return false;
            }
            i++;
            pPoly++;
        }
    }

    return true;
}

/**
 * Offset/Address/Size: 0x250 | 0x801D7850 | size: 0xEC
 */
void glPoly2::FullCoverage(const nlColour& col, float z)
{
    const float width = glGetOrthographicWidth();
    const float height = glGetOrthographicHeight();
    gl_ScreenInfo* screenInfo = glGetScreenInfo();
    const float border = screenInfo->PixelCentre;

    nlVec2Set(m_pos[0], -border, -border);
    nlVec2Set(m_pos[1], -border, height - border);
    nlVec2Set(m_pos[2], width - border, height - border);
    nlVec2Set(m_pos[3], width - border, -border);

    depth = z;

    nlVec2Set(m_uv[0], 0.0f, 0.0f);
    nlVec2Set(m_uv[1], 0.0f, 1.0f);
    nlVec2Set(m_uv[2], 1.0f, 1.0f);
    nlVec2Set(m_uv[3], 1.0f, 0.0f);

    m_colour[0] = col;
    m_colour[1] = col;
    m_colour[2] = col;
    m_colour[3] = col;
}

/**
 * Offset/Address/Size: 0x1EC | 0x801D77EC | size: 0x64
 */
void glPoly2::SetupRectangle(float x, float y, float w, float h, float z)
{
    f32 bottom_y;
    f32 right_x;

    nlVec2Set(m_pos[0], x, y);
    nlVec2Set(m_pos[1], x, y + h);
    nlVec2Set(m_pos[2], x + w, y + h);
    nlVec2Set(m_pos[3], x + w, y);

    nlVec2Set(m_uv[0], 0.0f, 0.0f);
    nlVec2Set(m_uv[1], 0.0f, 1.0f);
    nlVec2Set(m_uv[2], 1.0f, 1.0f);
    nlVec2Set(m_uv[3], 1.0f, 0.0f);

    if (z != 1e10f)
    {
        depth = z;
    }
}

/**
 * Offset/Address/Size: 0x18 | 0x801D7618 | size: 0x1D4
 */
void glPoly2::SetupRotatedRectangle(float cx, float cy, float w, float h, float angle, float z)
{
    nlVector2 v;
    nlMatrix3 m;
    float f3 = 0.5f;
    f32 temp_f26;
    f32 temp_f25;
    f32 temp_f31;
    f32 temp_f29;

    nlMakeRotationMatrixZ(m, angle);

    temp_f26 = -w * f3;
    temp_f31 = -h * f3;
    nlVec2Set(v, temp_f26, temp_f31);
    nlMultVectorMatrix(v, v, m);
    nlVec2Set(m_pos[0], v.x + cx, v.y + cy);

    temp_f25 = h * f3;
    nlVec2Set(v, temp_f26, temp_f25);
    nlMultVectorMatrix(v, v, m);
    nlVec2Set(m_pos[1], v.x + cx, v.y + cy);

    temp_f29 = w * f3;
    nlVec2Set(v, temp_f29, temp_f25);
    nlMultVectorMatrix(v, v, m);
    nlVec2Set(m_pos[2], v.x + cx, v.y + cy);

    nlVec2Set(v, temp_f29, temp_f31);
    nlMultVectorMatrix(v, v, m);
    nlVec2Set(m_pos[3], v.x + cx, v.y + cy);

    nlVec2Set(m_uv[0], 0.0f, 0.0f);
    nlVec2Set(m_uv[1], 0.0f, 1.0f);
    nlVec2Set(m_uv[2], 1.0f, 1.0f);
    nlVec2Set(m_uv[3], 1.0f, 0.0f);

    if (z != 1e10f)
    {
        depth = z;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x801D7600 | size: 0x18
 */
void glPoly2::SetColour(const nlColour& col)
{
    m_colour[0] = col;
    m_colour[1] = col;
    m_colour[2] = col;
    m_colour[3] = col;
}
