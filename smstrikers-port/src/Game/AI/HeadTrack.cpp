#include "Game/AI/HeadTrack.h"

#include "math.h"

static f32 CANT_COLLIDE = *(f32*)__float_max;

/**
 * Offset/Address/Size: 0x5B0 | 0x800573B4 | size: 0x70
 */
cHeadTrack::cHeadTrack()
{
    m_m4HeadMatrix.SetIdentity();
    nlVec3Set(m_v3OOI, 0.0f, 0.0f, 0.0f);
    m_bTrackOOI = true;
    m_fHeadSpin = 0.0f;
    m_fHeadTilt = 0.0f;
    m_fDesiredHeadSpin = 0.0f;
    m_fDesiredHeadTilt = 0.0f;
    m_fHeadSpinSeekVel = 0.0f;
    m_fHeadTiltSeekVel = 0.0f;
}

/**
 * Offset/Address/Size: 0x160 | 0x80056F64 | size: 0x450
 */
void cHeadTrack::Update(const nlMatrix4& m4HeadMatrix, const nlMatrix4& m4ConstraintMatrix, float fDeltaT, unsigned short aOOIConstraint, int nHeadSpinMax, int nHeadTiltMax, float fSmoothTime)
{
    nlMatrix4 m4Constrain;
    nlMatrix4 m4WorldSpaceToConstraintSpace;
    nlVector3 v3OOIConstraintSpace;
    int nHeadSpin;
    int nHeadTilt;
    int nAmountOfDeadZoneBehindHeadtrack;

    if (m_bTrackOOI)
    {
        m4Constrain = m4ConstraintMatrix;
        float headM41 = m4HeadMatrix.m41;
        float headM42 = m4HeadMatrix.m42;
        float headM43 = m4HeadMatrix.m43;

        m4Constrain.m41 = headM41;
        m4Constrain.m42 = headM42;
        m4Constrain.m43 = headM43;
        m4Constrain.m44 = 1.0f;

        nlInvertRotTransMatrix(m4WorldSpaceToConstraintSpace, m4Constrain);
        nlMultPosVectorMatrix(v3OOIConstraintSpace, m_v3OOI, m4WorldSpaceToConstraintSpace);

        {
            float x2 = v3OOIConstraintSpace.x * v3OOIConstraintSpace.x;
            float y2 = v3OOIConstraintSpace.y * v3OOIConstraintSpace.y;
            float z2 = v3OOIConstraintSpace.z * v3OOIConstraintSpace.z;
            float invLen = nlRecipSqrt(x2 + y2 + z2, true);
            nlVec3Scale(v3OOIConstraintSpace, invLen);
        }

        nHeadSpin = ((int)(10430.378f * nlATan2f(v3OOIConstraintSpace.z, v3OOIConstraintSpace.y)) << 16) >> 16;
        nHeadTilt = 0x4000 - nlACos(-v3OOIConstraintSpace.x);
        nHeadTilt = (nHeadTilt << 16) >> 16;

        {
            int absSpin = (nHeadSpin < 0) ? -nHeadSpin : nHeadSpin;

            if ((absSpin < (int)(unsigned int)aOOIConstraint) || (m_v3OOI.z > 1.5f))
            {
                nHeadSpin = (int)(float)nHeadSpin;
                nHeadTilt = (int)(0.5f * (float)nHeadTilt);
                nAmountOfDeadZoneBehindHeadtrack = (((int)(unsigned int)aOOIConstraint - nHeadSpinMax) * 3) / 4;

                absSpin = (nHeadSpin < 0) ? -nHeadSpin : nHeadSpin;
                if ((unsigned int)absSpin >= (unsigned int)nHeadSpinMax)
                {
                    if (nHeadSpin > 0)
                    {
                        if (m_fDesiredHeadSpin < 0.0f)
                        {
                            if (nHeadSpin > (nHeadSpinMax + nAmountOfDeadZoneBehindHeadtrack))
                                nHeadSpin = -nHeadSpinMax;
                            else
                                nHeadSpin = nHeadSpinMax;
                        }
                        else
                            nHeadSpin = nHeadSpinMax;
                    }
                    else if (m_fDesiredHeadSpin > 0.0f)
                    {
                        if (nHeadSpin < -(nHeadSpinMax + nAmountOfDeadZoneBehindHeadtrack))
                            nHeadSpin = nHeadSpinMax;
                        else
                            nHeadSpin = -nHeadSpinMax;
                    }
                    else
                        nHeadSpin = -nHeadSpinMax;
                }
            }
            else
            {
                nHeadSpin = 0;
                nHeadTilt = 0;
            }
        }

        if (nHeadTilt > nHeadTiltMax)
            nHeadTilt = nHeadTiltMax;

        if (&m4HeadMatrix != &m4ConstraintMatrix)
        {
            float headM21;
            float headM22;
            headM22 = m4HeadMatrix.m22;
            headM21 = m4HeadMatrix.m21;
            float constraintAtan = nlATan2f(m4Constrain.m22, m4Constrain.m21);
            float headAtan = nlATan2f(headM22, headM21);
            unsigned short spinConstraint = (unsigned short)(int)(10430.378f * constraintAtan);
            unsigned short spinHead = (unsigned short)(int)(10430.378f * headAtan);
            nHeadSpin += (short)(spinConstraint - spinHead);
        }
        m_fDesiredHeadSpin = nHeadSpin;
        m_fDesiredHeadTilt = nHeadTilt;
    }
    else
    {
        m_fDesiredHeadSpin = 0.0f;
        m_fDesiredHeadTilt = 0.0f;
    }

    float spinChange;
    float x;
    float spinVel;
    float omega = 2.0f / fSmoothTime;
    x = omega * fDeltaT;
    float exp = 1.0f / (((0.48f * x * x) + (1.0f + x)) + (x * (0.235f * x * x)));

    spinChange = m_fHeadSpin - m_fDesiredHeadSpin;
    spinVel = m_fHeadSpinSeekVel;

    m_fHeadSpinSeekVel = exp * (spinVel - (omega * (fDeltaT * ((omega * spinChange) + spinVel))));
    m_fHeadSpin = (exp * (spinChange + (fDeltaT * ((omega * spinChange) + spinVel)))) + m_fDesiredHeadSpin;

    float tiltChange = m_fHeadTilt - m_fDesiredHeadTilt;
    float tiltVel = m_fHeadTiltSeekVel;

    m_fHeadTiltSeekVel = exp * (tiltVel - (omega * (fDeltaT * ((omega * tiltChange) + tiltVel))));
    m_fHeadTilt = (exp * (tiltChange + (fDeltaT * ((omega * tiltChange) + tiltVel)))) + m_fDesiredHeadTilt;
}

/**
 * Offset/Address/Size: 0x0 | 0x80056E04 | size: 0x160
 */
void CalcHeadTrackMatrix(unsigned short spin, unsigned short tilt, cPoseAccumulator* cPoseAccumulator, int headNodeIndex)
{
    nlMatrix4 m4Intermediate;
    nlMatrix4 m4RotMatrix;
    nlMatrix4 m4NewHeadMatrix;

    const class nlMatrix4& m4AnimatedHeadMatrix = cPoseAccumulator->GetNodeMatrix(headNodeIndex);

    nlMakeRotationMatrixX(m4RotMatrix, AngUnitsToRad_fromUnsignedShort(spin));
    nlMultMatrices(m4Intermediate, m4RotMatrix, m4AnimatedHeadMatrix);
    nlMakeRotationMatrixZ(m4RotMatrix, AngUnitsToRad_fromUnsignedShort(tilt));
    nlMultMatrices(m4NewHeadMatrix, m4RotMatrix, m4Intermediate);

    cPoseAccumulator->m_NodeMatrices.mData[headNodeIndex] = m4NewHeadMatrix;
}
