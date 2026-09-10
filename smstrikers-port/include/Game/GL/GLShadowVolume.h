#ifndef _GLSHADOWVOLUME_H_
#define _GLSHADOWVOLUME_H_

#include "NL/nlMath.h"

struct GLShadowVertex;
struct GLShadowEdge;
struct GLShadowFace;
struct GLShadowBoneMap;

class GLShadowVolume
{
public:
    virtual void Render(const nlMatrix4&);
    virtual ~GLShadowVolume();

    /* 0x004 */ unsigned long hashID;
    /* 0x008 */ int numVertices;
    /* 0x00C */ int numEdges;
    /* 0x010 */ int numFaces;
    /* 0x014 */ int numBones;
    /* 0x018 */ void* fileData;
    /* 0x01C */ GLShadowVertex* vertices;
    /* 0x020 */ GLShadowVertex* unposedVertices;
    /* 0x024 */ GLShadowEdge* edges;
    /* 0x028 */ GLShadowFace* faces;
    /* 0x02C */ GLShadowBoneMap* bones;
    /* 0x030 */ nlMatrix4* matrices;
}; // total size: 0x34

#endif // _GLSHADOWVOLUME_H_
