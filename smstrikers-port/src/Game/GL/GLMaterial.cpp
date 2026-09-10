#include "Game/GL/GLMaterial.h"
#include "NL/nlMemory.h"

/**
 * Offset/Address/Size: 0x128 | 0x801E7F98 | size: 0x18
 */
GLMaterialList::GLMaterialList()
{
    m_uHashID = -1;
    m_nNumMaterials = 0;
    m_pMaterials = NULL;
}

/**
 * Offset/Address/Size: 0xCC | 0x801E7F3C | size: 0x5C
 */
GLMaterialList::~GLMaterialList()
{
    if (m_pMaterials)
    {
        delete[] m_pMaterials;
    }
}

/**
 * Offset/Address/Size: 0x48 | 0x801E7EB8 | size: 0x84
 */
void GLMaterialList::SetMaterials(int numMats, const GLMaterialEntry* pEntries)
{
    u32 size;

    if (m_pMaterials)
    {
        delete[] m_pMaterials;
    }

    size = numMats * 0xC;
    m_nNumMaterials = numMats;
    m_pMaterials = (GLMaterialEntry*)nlMalloc(size, 8, 0);
    memcpy(m_pMaterials, pEntries, size);
}

/**
 * Offset/Address/Size: 0x0 | 0x801E7E70 | size: 0x48
 */
GLMaterialEntry* GLMaterialList::FindMaterial(unsigned long searchId) const
{
    GLMaterialEntry* current = m_pMaterials;
    GLMaterialEntry* end = current + m_nNumMaterials;
    while (current < end)
    {
        if (current->materialID == searchId)
        {
            return current;
        }
        current++;
    }

    return NULL;
}
