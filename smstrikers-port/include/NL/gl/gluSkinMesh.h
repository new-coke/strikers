#ifndef _GLUSKINMESH_H_
#define _GLUSKINMESH_H_

#include "NL/nlMath.h"

class SkinPairList;
class BoneMapList;

class ShaderSkinMesh
{
public:
    void AttachSkinData(unsigned long, const nlMatrix4*);
    void StitchModel();
};

#endif // _GLUSKINMESH_H_
