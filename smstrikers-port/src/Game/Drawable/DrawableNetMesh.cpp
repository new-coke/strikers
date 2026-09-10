#include "Game/Drawable/DrawableNetMesh.h"
#include "Game/GL/gluMeshWriter.h"
#include "Game/Render/ShootToScoreArrow.h"
#include "Game/Replay.h"
#include "Game/Field.h"
#include "Game/Net.h"
#include "Game/World.h"
#include "NL/nlString.h"
#include "NL/gl/gl.h"
#include "NL/gl/glDraw3.h"
#include "NL/gl/glMatrix.h"
#include "NL/gl/glMemory.h"
#include "NL/gl/glState.h"
#include "NL/gl/glUserData.h"
#include "NL/glx/glxDisplayList.h"
#include "dolphin/os/OSCache.h"

static unsigned long UnlitProgram = glGetProgram("3d unlit");
static unsigned long LitProgram = glGetProgram("3d pointlit");
static unsigned long LightTexture = glGetTexture("global/lightramp");
static unsigned long BlackTexture = glGetTexture("global/black");
static unsigned long WhiteTexture = glGetTexture("global/white");

shortVector2* DrawableNetMesh::spTexcoord[2];
unsigned long* DrawableNetMesh::spColour[2];
unsigned short* DrawableNetMesh::spTriIndices[2];
bool DrawableNetMesh::sbStaticInitialized[2] = { };
int DrawableNetMesh::sNumVertices[2];

static unsigned char sbRenderAnimatedNetMesh = 1;
static unsigned char sbAccelerateMeshWriter = 1;
static unsigned char sbCopyVertices = 1;
// PORT: linkable, so the debug menu can drive it. Only the keyword changed.
unsigned char sbUseCheckerTexture = 0;
static unsigned char sbUseDisplayLists = 0;

static int siInvisiblePlaneAlpha;
static unsigned long NetMeshTexture = glGetTexture("global/netmesh");
static const unsigned long CheckerTexture = glGetTexture("global/checkers");

/**
 * Offset/Address/Size: 0xC4C | 0x80114BA8 | size: 0x24
 */
DrawableNetMesh::DrawableNetMesh(bool isPositiveXNet)
{
    miNetIndex = isPositiveXNet ? 0 : 1;
    mNumQuads = 0;
    mNumVertices = 0;
    mbInitialized = false;
}

/**
 * Offset/Address/Size: 0xB80 | 0x80114ADC | size: 0xCC
 */
DrawableNetMesh::~DrawableNetMesh()
{
    if (mbInitialized)
    {
        delete[] mpPosition;
    }

    if (sbStaticInitialized[miNetIndex])
    {
        delete[] spTexcoord[miNetIndex];
        delete[] spTriIndices[miNetIndex];
        delete[] spColour[miNetIndex];
        sbStaticInitialized[miNetIndex] = false;
    }

    mNumQuads = 0;
    mNumVertices = 0;
    mbInitialized = false;
}

/**
 * Offset/Address/Size: 0x91C | 0x80114878 | size: 0x264
 */
void DrawableNetMesh::RenderInvisiblePlanes() const
{
    float goalLineX;
    nlVector2 netDimensions;

    goalLineX = cField::GetGoalLineX(1U);
    netDimensions.x = cNet::m_fNetHeight;
    netDimensions.y = cNet::m_fNetWidth;

    glSetDefaultState(true);
    glSetRasterState(GLS_DepthWrite, 1);
    glSetRasterState(GLS_AlphaBlend, 1);
    glSetRasterState(GLS_Culling, 0);
    glSetCurrentRasterState(glHandleizeRasterState());

    glSetCurrentTexture(WhiteTexture, GLTT_Diffuse);
    glSetTextureState(GLTS_DiffuseWrap, 0);
    glSetCurrentTextureState(glHandleizeTextureState());

    nlMatrix4 matrix;
    nlMakeRotationMatrixY(matrix, 1.5707964f);

    float netPlaneX;

    nlColour c = { 0xFF, 0xFF, 0x00, 0x00 };
    c.c[3] = (u8)siInvisiblePlaneAlpha;

    glQuad3 quad;

    netPlaneX = goalLineX - 0.05f;
    matrix.e2[3][0] = netPlaneX;
    matrix.e2[3][1] = 0.0f;
    matrix.e2[3][2] = 0.5f * netDimensions.x;
    matrix.e2[3][3] = 1.0f;
    quad.SetupRotatedRectangle(netDimensions.x, netDimensions.y, matrix, false, false);
    quad.SetColour(c);
    glAttachQuad3(GLV_InvisiblePlane, 1, &quad, true);

    netPlaneX = 0.05f + goalLineX;
    matrix.e2[3][0] = netPlaneX;
    matrix.e2[3][1] = 0.0f;
    matrix.e2[3][2] = 0.5f * netDimensions.x;
    matrix.e2[3][3] = 1.0f;
    quad.SetupRotatedRectangle(netDimensions.x, netDimensions.y, matrix, false, false);
    quad.SetColour(c);
    glAttachQuad3(GLV_InvisiblePlane, 1, &quad, true);

    netPlaneX = -goalLineX - 0.05f;
    matrix.e2[3][0] = netPlaneX;
    matrix.e2[3][1] = 0.0f;
    matrix.e2[3][2] = 0.5f * netDimensions.x;
    matrix.e2[3][3] = 1.0f;
    quad.SetupRotatedRectangle(netDimensions.x, netDimensions.y, matrix, false, false);
    quad.SetColour(c);
    glAttachQuad3(GLV_InvisiblePlane, 1, &quad, true);

    netPlaneX = 0.05f - goalLineX;
    matrix.e2[3][0] = netPlaneX;
    matrix.e2[3][1] = 0.0f;
    matrix.e2[3][2] = 0.5f * netDimensions.x;
    matrix.e2[3][3] = 1.0f;
    quad.SetupRotatedRectangle(netDimensions.x, netDimensions.y, matrix, false, false);
    quad.SetColour(c);
    glAttachQuad3(GLV_InvisiblePlane, 1, &quad, true);

    glSetDefaultState(false);
}

static const int gl_stream_stride[15] = {
    12, 3, 4, 4, 4, 4, 4, 4, 4, 12, 12, 12, 1, 16, 16
};

/**
 * Offset/Address/Size: 0x4B8 | 0x80114414 | size: 0x464
 */
void DrawableNetMesh::Render() const
{
    if (!sbRenderAnimatedNetMesh || !mbInitialized || !NetMesh::s_bAnimatedNetMeshEnabled)
    {
        return;
    }

    if (World::sbIsHyperShootToScoreRenderingEnabled)
    {
        int netIndex = miNetIndex;
        if ((netIndex == 1 && World::sbShowPositiveXNetDuringHyperStrike) || (netIndex == 0 && !World::sbShowPositiveXNetDuringHyperStrike))
        {
            return;
        }
    }

    eGLStream streamDecl[3] = { GLStream_Position, GLStream_Colour, GLStream_Diffuse };
    GLMeshWriter meshWriter;
    nlVector3* pPosition = mpPosition;
    shortVector2* pTexcoord = spTexcoord[miNetIndex];
    nlColour* pColour;

    glSetDefaultState(true);
    glSetRasterState(GLS_Culling, 0);
    glSetRasterState(GLS_AlphaBlend, 1);
    glSetRasterState(GLS_AlphaTest, 1);
    glSetRasterState(GLS_DepthTest, 1);
    glSetRasterState(GLS_DepthWrite, 0);
    glSetCurrentRasterState(glHandleizeRasterState());
    glSetCurrentMatrix(glGetIdentityMatrix());

    uintptr_t texture = NetMesh::sNetTextureHandle;
    if (sbUseCheckerTexture)
    {
        texture = CheckerTexture;
    }
    glSetCurrentTexture(texture, GLTT_Diffuse);
    glSetCurrentProgram(UnlitProgram);

    unsigned short* pTriIndices = spTriIndices[miNetIndex];

    if (sbAccelerateMeshWriter)
    {
        // PORT: 0x10/0x4A/0x12 are the console record sizes; the host ones are wider.
        glModel* pModel = (glModel*)glFrameAlloc(sizeof(glModel), GLM_Header);
        nlZeroMemory(pModel, sizeof(glModel));
        glModelPacket* pPacket =
            (glModelPacket*)glFrameAlloc(sizeof(glModelPacket), GLM_Header);
        nlZeroMemory(pPacket, sizeof(glModelPacket));
        glModelStream* pStreams =
            (glModelStream*)glFrameAlloc(3 * sizeof(glModelStream), GLM_Header);
        nlZeroMemory(pStreams, 3 * sizeof(glModelStream));

        pModel->id = (u32)-1;
        pModel->numPackets = 1;
        pModel->packets = pPacket;
        pPacket->indexBuffer = (uintptr_t)pTriIndices;
        pPacket->numStreams = 3;
        pPacket->numVertices = (u16)m_unk18;
        pPacket->primType = 1;
        pPacket->streams = pStreams;
        glStateSave(pPacket->state);

        if (sbCopyVertices)
        {
            unsigned long nBytes = (unsigned long)mJolt * (unsigned long)sizeof(nlVector3);
            nlVector3* pNewPosition = (nlVector3*)glFrameAlloc(nBytes, GLM_VertexData);
            memcpy(pNewPosition, pPosition, nBytes);
            pPosition = pNewPosition;
        }
        if (sbUseDisplayLists)
        {
            if ((u32)mNumVertices == 0)
            {
                mNumQuads = pPacket->indexBuffer;
                mNumVertices = (uintptr_t)dlMakeDisplayList(pPacket, true);
            }
            pPacket->indexBuffer = (u32)mNumVertices;
        }
        DCFlushRange(pPosition, (unsigned long)mJolt * (unsigned long)sizeof(nlVector3));

        pStreams[0].id = GLStream_Position;
        pStreams[0].address = (uintptr_t)pPosition;
        pStreams[0].beData = 0;   // PORT: host order
        pStreams[0].stride = (u8)gl_stream_stride[0];
        // PORT: and how long the array is, without this glxSend bounds it by stride * numVertices and the net draws as enormous triangles across the frame.
        pStreams[0].dataSize = (u32)((unsigned long)mJolt * sizeof(nlVector3));
        pColour = (nlColour*)spColour[miNetIndex];
        pStreams[1].id = GLStream_Colour;
        pStreams[1].address = (uintptr_t)pColour;
        pStreams[1].beData = 0;
        pStreams[1].stride = (u8)gl_stream_stride[2];
        // PORT: sNumVertices, not mJolt, this buffer is allocated once behind sbStaticInitialized and does not grow with the particle count.
        pStreams[1].dataSize = (u32)((unsigned long)sNumVertices[miNetIndex] * 4u);
        pStreams[2].id = GLStream_Diffuse;
        pStreams[2].address = (uintptr_t)pTexcoord;
        pStreams[2].beData = 0;
        pStreams[2].stride = (u8)gl_stream_stride[3];
        pStreams[2].dataSize = (u32)((unsigned long)sNumVertices[miNetIndex] * 4u);

        void* pUserDataHandle = glUserAlloc(GLUD_ConstantColour, 4, false);
        pColour = (nlColour*)glUserGetData(pUserDataHandle);
        WorldDarkening& wd = WorldDarkening::Instance();
        float darkScale = 255.0f;
        float darkBase = 1.0f - wd.mPos;
        u8 dark = (u8)(int)(darkScale * darkBase);
        pColour->c[0] = dark;
        pColour->c[1] = dark;
        pColour->c[2] = dark;
        pColour->c[3] = dark;
        glUserAttach(pUserDataHandle, pPacket, false);
        glViewAttachModel(GLV_UnsortedPerspective, pModel);
    }
    else
    {
        if (meshWriter.Begin(m_unk18, GLP_TriStrip, 3, streamDecl, false))
        {
            int i;
            unsigned short* pIndex = pTriIndices;
            for (i = 0; i < ((const volatile DrawableNetMesh*)this)->m_unk18; pIndex++, i++)
            {
                unsigned short index = *pIndex;
                float darkPos = WorldDarkening::Instance().mPos;
                u8 dark = (u8)(int)((1.0f - darkPos) * 255.0);
                shortVector2* pUV = &pTexcoord[index];
                meshWriter.Texcoord(pUV->e[0], pUV->e[1]);
                nlColour c;
                c.c[0] = dark;
                c.c[1] = dark;
                c.c[2] = dark;
                c.c[3] = 0xFF;
                ((GLMeshWriterCore*)&meshWriter)->Colour(c);
                meshWriter.Vertex(pPosition[index]);
            }
            if (!meshWriter.End())
            {
                return;
            }
            glViewAttachModel(GLV_UnsortedPerspective, meshWriter.GetModel());
        }
    }
    RenderInvisiblePlanes();
}

/**
 * Offset/Address/Size: 0x2C0 | 0x8011421C | size: 0x1F8
 */
void DrawableNetMesh::Grab(NetMesh& netMesh)
{
    mpNetMesh = &netMesh;

    if (!netMesh.mbInitialized)
        return;

    if (!mbInitialized)
    {
        int numTriIdx = netMesh.m_NumTriStripIndices;
        mJolt = netMesh.m_NumParticles;
        m_unk18 = numTriIdx;

        int numVerts = mJolt;
        int numIndices = m_unk18;

        mpPosition = (nlVector3*)nlMalloc(numVerts * sizeof(nlVector3), 8, false);

        if (!sbStaticInitialized[miNetIndex])
        {
            spTriIndices[miNetIndex] = (unsigned short*)nlMalloc(numIndices * 2, 8, false);

            int allocSize = numVerts * 4;

            spTexcoord[miNetIndex] = (shortVector2*)nlMalloc(allocSize, 8, false);
            spColour[miNetIndex] = (unsigned long*)nlMalloc(allocSize, 8, false);

            memset(spColour[miNetIndex], 0xFF, allocSize);

            sbStaticInitialized[miNetIndex] = true;
            sNumVertices[miNetIndex] = numVerts;
        }

        mbInitialized = true;
        mJoltCache = 0.0f;
    }

    unsigned short* pTriIndices = spTriIndices[miNetIndex];
    shortVector2* pTexcoord = spTexcoord[miNetIndex];

    for (int i = 0; i < netMesh.m_NumTriStripIndices; i++)
    {
        *pTriIndices++ = netMesh.m_TriStripIndices[i];
    }

    {
        shortVector2* pDst = pTexcoord;
        for (int i = 0; i < netMesh.m_NumParticles; i++)
        {
            mpPosition[i] = netMesh.m_v3Position[i];
            *pDst++ = netMesh.m_v2TextureCoords[i];
        }
    }
}

/**
 * Offset/Address/Size: 0xAC | 0x80114008 | size: 0x214
 */
void DrawableNetMesh::Blend(float blendFactor, const DrawableNetMesh& lhs, const DrawableNetMesh& rhs)
{
    nlVector3* pDst;
    nlVector3* pSrc;

    if (!lhs.mbInitialized || !rhs.mbInitialized)
        return;

    if (!mbInitialized)
    {
        int numTriIdx = ((const volatile DrawableNetMesh*)&lhs)->m_unk18;
        mJolt = ((const volatile DrawableNetMesh*)&lhs)->mJolt;
        m_unk18 = numTriIdx;

        int numVerts = mJolt;
        int numIndices = m_unk18;

        mpPosition = (nlVector3*)nlMalloc(numVerts * sizeof(nlVector3), 8, false);

        if (!sbStaticInitialized[miNetIndex])
        {
            spTriIndices[miNetIndex] = (unsigned short*)nlMalloc(numIndices * 2, 8, false);

            int allocSize = numVerts * 4;

            spTexcoord[miNetIndex] = (shortVector2*)nlMalloc(allocSize, 8, false);
            spColour[miNetIndex] = (unsigned long*)nlMalloc(allocSize, 8, false);

            memset(spColour[miNetIndex], 0xFF, allocSize);

            sbStaticInitialized[miNetIndex] = true;
            sNumVertices[miNetIndex] = numVerts;
        }

        mbInitialized = true;
        mJoltCache = 0.0f;
    }

    float oneMinusBlend = 1.0f - blendFactor;

    int offset;
    int i;
    for (i = 0, offset = 0; i < mJolt; i++, offset += sizeof(nlVector3))
    {
        pSrc = (nlVector3*)((char*)((const volatile DrawableNetMesh*)&lhs)->mpPosition + offset);
        pDst = (nlVector3*)((char*)((volatile DrawableNetMesh*)this)->mpPosition + offset);
        float x = oneMinusBlend * pSrc->x;
        float y = pSrc->y;
        float z = pSrc->z;
        y = oneMinusBlend * y;
        z = oneMinusBlend * z;
        pDst->x = x;
        pDst->y = y;
        pDst->z = z;
    }

    for (int i = 0; i < mJolt; i++)
    {
        pDst = &((volatile DrawableNetMesh*)this)->mpPosition[i];
        pSrc = (nlVector3*)&((const volatile DrawableNetMesh*)&rhs)->mpPosition[i];
        float x = pDst->x + blendFactor * pSrc->x;
        float z = pDst->z + blendFactor * pSrc->z;
        float y = pDst->y + blendFactor * pSrc->y;
        pDst->x = x;
        pDst->y = y;
        pDst->z = z;
    }
}

/**
 * Offset/Address/Size: 0x38 | 0x80113F94 | size: 0x74
 */
void DrawableNetMesh::Replay(LoadFrame& frame)
{
    float joltValue = 0.0f;
    Replayable<0, LoadFrame, float>(frame, joltValue);

    if (joltValue != mJoltCache)
    {
        mJoltCache = joltValue;
        if (mpNetMesh != nullptr)
        {
            if (mJoltCache > 0.0f)
            {
                mpNetMesh->JoltNet(mJoltCache);
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80113F5C | size: 0x38
 */
void DrawableNetMesh::Replay(SaveFrame& frame)
{
    mJoltCache = mpNetMesh->mJolt;
    Replayable<0, SaveFrame, float>(frame, mJoltCache);
}
