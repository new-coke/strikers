
#include "Game/Drawable/DrawableObj.h"
#include "NL/nlMath.h"

// nlMatrix4 m3Ident = { 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f };

/**
 * Offset/Address/Size: 0x124 | 0x8011FD94 | size: 0x6C
 */
DrawableObject::DrawableObject()
{
    m_worldMatrixUpToDate = true;
    m_scale = 1.f;

    m_pCachedAABB = NULL;

    m_uObjectCreationFlags = 0;
    m_pWorldContext = NULL;
    m_uObjectFlags = 0;
    m_bRenderPlanarShadow = false;

    m_orientation.z = 0.f;
    m_orientation.y = 0.f;
    m_orientation.x = 0.f;
    m_orientation.w = 1.f;

    nlVec3Set(m_translation, 0.f, 0.f, 0.f);

    m_translucency = 1.f;
    m_CB = NULL;
    m_uObjectFlags = (m_uObjectFlags | 1);
}

/**
 * Offset/Address/Size: 0xBC | 0x8011FD2C | size: 0x68
 */
DrawableObject::~DrawableObject()
{
    if (m_pCachedAABB != NULL)
    {
        delete m_pCachedAABB;
    }
}

/**
 * Offset/Address/Size: 0x2C | 0x8011FC9C | size: 0x90
 */
nlMatrix4& DrawableObject::GetWorldMatrix() const
{
    if (!m_worldMatrixUpToDate)
    {
        nlMatrix4 rot_mtx;
        nlQuatToMatrix(rot_mtx, m_orientation);

        rot_mtx.e2[3][0] = m_translation.x;
        rot_mtx.e2[3][1] = m_translation.y;
        rot_mtx.e2[3][2] = m_translation.z;
        rot_mtx.e2[3][3] = 1.0f;

        nlMatrix4 scale_mtx;
        float s = m_scale;
        nlMakeScaleMatrix(scale_mtx, s, s, s);
        nlMultMatrices(m_worldMatrix, scale_mtx, rot_mtx);
        m_worldMatrixUpToDate = true;
    }
    return m_worldMatrix;
}

/**
 * Offset/Address/Size: 0x0 | 0x8011FC70 | size: 0x2C
 */
void DrawableObject::GetAABBDimensions(AABBDimensions& dim, bool param) const
{
    dim.mMin.x = 0.0f;
    dim.mMin.y = 0.0f;
    dim.mMin.z = 0.0f;
    dim.mMax.x = 0.0f;
    dim.mMax.y = 0.0f;
    dim.mMax.z = 0.0f;
    dim.mDim.x = 0.0f;
    dim.mDim.y = 0.0f;
    dim.mDim.z = 0.0f;
}

//  */
// void DrawableObject::DrawPlanarShadow()
// {
// }
