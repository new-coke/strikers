#ifndef _POSENODE_H_
#define _POSENODE_H_

#include "NL/nlMath.h"
#include "Game/Replay.h"
#include "Game/PoseAccumulator.h"

class cPoseNode
{
public:
    cPoseNode();
    cPoseNode(int nNumChildren);

    /* 0x08 */ virtual ~cPoseNode();
    /* 0x14 */ virtual void Evaluate(int, float, cPoseAccumulator*) const = 0;
    /* 0x10 */ virtual void Evaluate(float, cPoseAccumulator*) const = 0;
    /* 0x18 */ virtual cPoseNode* Update(float) = 0;
    /* 0x1C */ virtual int GetType() = 0;
    /* 0x20 */ virtual void BlendRootTrans(nlVector3* outBase, float weight, float* scratch) = 0;
    /* 0x24 */ virtual void BlendRootRot(unsigned short* outRot, float weight, float* scratch) = 0;

    void GetRootRot(unsigned short* out);
    void GetRootTrans(nlVector3* out, unsigned short ang);
    void SetChild(int idx, cPoseNode* child);
    int GetNumChildren() const;
    cPoseNode** GetChildPtr(int idx);
    cPoseNode* GetChild(int idx) const;
    cPoseNode* GetChild(int idx);

    template <typename T>
    void Replay(T& frame);

    template <typename T>
    void ReplayNumChildren(T& frame);

private:
    cPoseNode*& GetChildRef(int);

public:
    /* 0x4 */ cPoseNode* m_children[3];
    /* 0x10 */ int m_numChildren;
}; // total size: 0x14

template <typename T>
inline void cPoseNode::Replay(T& frame)
{
    ReplayNumChildren(frame);
    for (int i = 0; i < m_numChildren; i++)
    {
        ReplayablePolymorphic<0>(frame, GetChildRef(i));
    }
}

template <typename T>
inline void cPoseNode::ReplayNumChildren(T& frame)
{
    Replayable<0>(frame, m_numChildren);
}

inline cPoseNode*& cPoseNode::GetChildRef(int i)
{
    return m_children[i];
}

#endif // _POSENODE_H_
