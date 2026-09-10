#ifndef _GLMESHWRITER_H_
#define _GLMESHWRITER_H_

#include "types.h"

#include "NL/nlColour.h"
#include "NL/nlMath.h"
#include "NL/gl/glModel.h"

// enum eGLPrimitive
// {
//     eGLPrimitive_0 = 0,
//     eGLPrimitive_1,
//     eGLPrimitive_2,
//     eGLPrimitive_3,
// };

enum eGLPrimitive
{
    GLP_TriList = 0,
    GLP_TriStrip = 1,
    GLP_TriFan = 2,
    GLP_QuadList = 3,
    GLP_LineList = 4,
    GLP_LineStrip = 5,
    GLP_Num = 6,
};

enum eGLStream
{
    GLStream_Position = 0,
    GLStream_Normal = 1,
    GLStream_Colour = 2,
    GLStream_Diffuse = 3,
    GLStream_Detail = 4,
    GLStream_Shadow = 5,
    GLStream_SelfIllum = 6,
    GLStream_Gloss = 7,
    GLStream_BumpLocal = 8,
    GLStream_BasisS = 9,
    GLStream_BasisT = 10,
    GLStream_BasisSxT = 11,
    GLStream_Indices = 12,
    GLStream_Weights = 13,
    GLStream_Position4 = 14,
    GLStream_Num = 15,
    GLStream_Invalid = 16,
};

class GLMeshWriterCore
{
public:
    GLMeshWriterCore();
    /* 0x08*/ ~GLMeshWriterCore();
    /* 0x0C*/ virtual bool Begin(int numVerts, eGLPrimitive prim, int numStreams, const eGLStream* pStreamIDs, bool bPermanent);
    /* 0x10*/ virtual bool End();
    /* 0x14*/ virtual void Colour(const nlColour& c);
    void Colour(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
    /* 0x18*/ virtual void ColourPlat(unsigned long rgba);
    /* 0x1C*/ virtual void Normal(const nlVector3&) = 0;
    /* 0x20*/ virtual void Texcoord(const nlVector2& uv);
    void Texcoord(float u, float v);
    /* 0x24*/ virtual void Vertex(const nlVector3& pos);
    /* 0x28*/ virtual void Vertex(const nlVector4& pos);
    void Position(const nlVector3&);
    void Position(float, float, float);
    glModel* GetModel();

    /* 0x04 */ glModel* pModel;
    /* 0x08 */ glModelStream stream[15]; // relative offset => pos: 08, normal: 0E, color: 14, Diffuse: 1A
    /* 0x64 */ int currentIndex;
    /* 0x68 */ int maximumVerts;
    /* 0x6C */ int elementCount;
};

#endif // _GLMESHWRITER_H_
