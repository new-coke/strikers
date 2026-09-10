#include "Game/Drawable/DrawableTmModel.h"

static bool g_bEnableDrawableTMModel = true;

const nlMatrix4& DrawableTmModel::GetAnimatedWorldMatrix()
{
    return m_pAnimController->m_pPoseAccumulator->GetNodeMatrix(m_uAnimBoneIndex);
}

/**
 * Offset/Address/Size: 0xD4 | 0x80122B00 | size: 0x1E4
 */
DrawableObject* DrawableTmModel::Clone() const
{
    DrawableTmModel* pClone = new (nlMalloc(sizeof(DrawableTmModel), 8, false)) DrawableTmModel(*this);

    pClone->m_uObjectFlags |= 0x1;
    pClone->m_uObjectFlags |= 0x4;

    return pClone;
}

/**
 * Offset/Address/Size: 0x70 | 0x80122A9C | size: 0x64
 */
void DrawableTmModel::Draw()
{
    if (!g_bEnableDrawableTMModel)
    {
        return;
    }

    m_uObjectFlags |= 0x4;

    if (m_pAnimController != NULL)
    {
        DrawModel(m_pAnimController->m_pPoseAccumulator->GetNodeMatrix(m_uAnimBoneIndex));
    }
}
