#ifndef _GLMATERIAL_H_
#define _GLMATERIAL_H_

#include "types.h"

struct GLMaterialEntry // size = 0xC
{
    /* 0x00 */ u32 materialID;
    /* 0x04 */ u32 packetIndex;
    /* 0x08 */ u32 numPackets;
}; // total size: 0xC

class GLMaterialList
{
public:
    GLMaterialList();
    ~GLMaterialList();

    void SetMaterials(int numMats, const GLMaterialEntry* pEntries);
    GLMaterialEntry* FindMaterial(unsigned long searchId) const;

    /* 0x00 */ u32 m_uHashID;
    /* 0x04 */ s32 m_nNumMaterials;
    /* 0x08 */ GLMaterialEntry* m_pMaterials;
};

#endif // _GLMATERIAL_H_
