#include "Game/PoseNode.h"

/**
 * Offset/Address/Size: 0x1FC | 0x801E91FC | size: 0x18
 */
cPoseNode::cPoseNode()
{
    m_numChildren = 0;
}

/**
 * Offset/Address/Size: 0x1E0 | 0x801E91E0 | size: 0x1C
 */
cPoseNode::cPoseNode(int nNumChildren)
{
    m_numChildren = 0;
    m_numChildren = nNumChildren;
}

/**
 * Offset/Address/Size: 0x134 | 0x801E9134 | size: 0xAC
 */
cPoseNode::~cPoseNode()
{
    for (int i = 0; i < m_numChildren; ++i)
    {
        cPoseNode* ch = m_children[i];
        if (ch)
        {
            delete ch; // virtual delete via child's vtable
        }
    }
}

/**
 * Offset/Address/Size: 0x124 | 0x801E9124 | size: 0x10
 */
cPoseNode* cPoseNode::GetChild(int idx)
{
    return m_children[idx];
    // return m_unk_0x04[index]->m_unk_0x04;
}

/**
 * Offset/Address/Size: 0x114 | 0x801E9114 | size: 0x10
 */
cPoseNode* cPoseNode::GetChild(int idx) const
{
    // return m_unk_0x04[index]->m_unk_0x04;
    return m_children[idx];
}

/**
 * Offset/Address/Size: 0x100 | 0x801E9100 | size: 0x14
 */
cPoseNode** cPoseNode::GetChildPtr(int idx)
{
    // return m_unk_0x04[index]->m_unk_0x04;
    return &m_children[idx];
}

int cPoseNode::GetNumChildren() const
{
    return m_numChildren;
}

/**
 * Offset/Address/Size: 0xF0 | 0x801E90F0 | size: 0x10
 */
void cPoseNode::SetChild(int idx, cPoseNode* child)
{
    // m_unk_0x04[index]->m_unk_0x04 = child;

    m_children[idx] = child;
}

/**
 * Offset/Address/Size: 0x44 | 0x801E9044 | size: 0xAC
 */
void cPoseNode::GetRootTrans(nlVector3* out, unsigned short ang)
{
    nlVector3 base;
    float pw = 0.0f;
    base.x = 0.0f;
    base.y = 0.0f;
    base.z = 0.0f;

    BlendRootTrans(&base, 1.0f, &pw);

    float c, s;
    nlSinCos(&s, &c, ang);

    out->x = base.x * c - (base.y * s);
    out->y = base.y * c + (base.x * s);
}

/**
 * Offset/Address/Size: 0x0 | 0x801E9000 | size: 0x44
 */
void cPoseNode::GetRootRot(unsigned short* out)
{
    float fAccumulatedWeight = 0.0f;
    *out = 0;
    BlendRootRot(out, 1.0f, &fAccumulatedWeight);
}
