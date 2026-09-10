#ifndef _NET_H_
#define _NET_H_

#include "NL/nlMath.h"

class cNet
{
public:
    cNet(int nIndex);
    ~cNet();
    float GetGoalLineX() const;
    void GetPostLocation(nlVector3& v3PostPosition, unsigned int uPostNum, float fYAdjust) const;
    static void SetNetDimensions(float fWidth, float fHeight, float fPostRadius, float fPostOffsetFromGoalLine);
    static void ShowNets();
    static float GetNetHeight()
    {
        return m_fNetHeight;
    };
    static float GetNetWidth()
    {
        return m_fNetWidth;
    };
    static float GetPostRadius()
    {
        return m_fNetPostRadius;
    }

    static float m_fNetHeight;
    static float m_fNetWidth;
    static float m_fNetDepth;
    static float m_fNetPostRadius;
    static float m_fNetPostOffsetFromGoalLine;

    /* 0x00 */ s32 m_nIndex;
    /* 0x04 */ f32 m_fDirection;
    /* 0x08 */ nlVector3 m_v3NetLocation;
}; // total size: 0x14

#endif // _NET_H_
