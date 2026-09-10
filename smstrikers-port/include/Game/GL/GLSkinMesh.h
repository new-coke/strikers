#ifndef _GLSKINMESH_H_
#define _GLSKINMESH_H_

#include "NL/nlMath.h"

struct SkinVertex
{
    /* 0x00 */ nlVector3 position;
    /* 0x0C */ signed char packed_normal[4];
}; // total size: 0x10

struct SkinPair
{
    /* 0x00 */ unsigned short vertexIndex;
    /* 0x02 */ unsigned short vertexWeight;
}; // total size: 0x4

struct SkinPairList
{
    /* 0x00 */ SkinPairList* m_next;
    /* 0x04 */ unsigned int num;
    /* 0x08 */ SkinPair* pairs;
}; // total size: 0xC

struct MorphDelta
{
    /* 0x00 */ nlVector3 delta;
    /* 0x0C */ int index;
}; // total size: 0x10

struct SkinMatrix
{
    /* 0x00 */ float m[3][4]; // offset 0x0, size 0x30

    void Set(const nlMatrix4& in);
    void Get(nlMatrix4& out) const;
    void Get4x3(float* out) const;
}; // total size: 0x30

void nlMultMatrices(SkinMatrix& out, const SkinMatrix& a, const SkinMatrix& b);

#endif // _GLSKINMESH_H_
