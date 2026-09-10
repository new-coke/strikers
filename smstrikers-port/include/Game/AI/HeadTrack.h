#ifndef _HEADTRACK_H_
#define _HEADTRACK_H_

#include "NL/nlMath.h"
#include "Game/PoseAccumulator.h"

void CalcHeadTrackMatrix(unsigned short spin, unsigned short tilt, cPoseAccumulator* cPoseAccumulator, int headNodeIndex);

class cHeadTrack
{
public:
    cHeadTrack();
    virtual ~cHeadTrack() { };

    void Update(const nlMatrix4& m4HeadMatrix, const nlMatrix4& m4ConstraintMatrix, float fDeltaT, unsigned short aOOIConstraint, int nHeadSpinMax, int nHeadTiltMax, float fSmoothTime);

    nlMatrix4 m_m4HeadMatrix; // offset 0x4, size 0x40
    nlVector3 m_v3OOI;        // offset 0x44, size 0xC
    bool m_bTrackOOI;         // offset 0x50, size 0x1
    float m_fHeadSpin;        // offset 0x54, size 0x4
    float m_fHeadTilt;        // offset 0x58, size 0x4
    float m_fDesiredHeadSpin; // offset 0x5C, size 0x4
    float m_fDesiredHeadTilt; // offset 0x60, size 0x4
    float m_fHeadSpinSeekVel; // offset 0x64, size 0x4
    float m_fHeadTiltSeekVel; // offset 0x68, size 0x4
}; // total size: 0x6C

#endif // _HEADTRACK_H_
