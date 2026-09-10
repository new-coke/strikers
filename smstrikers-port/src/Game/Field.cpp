#include "Game/Field.h"

#include "math.h"

extern cBall* g_pBall;

cNet* cField::mpNet[2];

const float cornerRadius = 3.0f;

nlVector3 cField::mv3FieldPosition = { 20.60211f, 12.0825f, 0.0f };
sSideLinePlane cField::mSidelines[4] = {
    { { 1.0f, 0.0f }, cField::mv3FieldPosition.x },
    { { -1.0f, 0.0f }, cField::mv3FieldPosition.x },
    { { 0.0f, 1.0f }, cField::mv3FieldPosition.y },
    { { 0.0f, -1.0f }, cField::mv3FieldPosition.y }
};

sCornerSegment cField::mCorners[4] = {
    { { cField::mv3FieldPosition.x - cornerRadius, cField::mv3FieldPosition.y - cornerRadius }, 0x0000, 0x4000, cornerRadius },
    { { cornerRadius - mv3FieldPosition.x, cField::mv3FieldPosition.y - cornerRadius }, 0x4000, 0x8000, cornerRadius },
    { { cornerRadius - mv3FieldPosition.x, cornerRadius - mv3FieldPosition.y }, 0x8000, 0xC000, cornerRadius },
    { { cField::mv3FieldPosition.x - cornerRadius, cornerRadius - mv3FieldPosition.y }, 0xC000, 0x0000, cornerRadius }
};

float cField::mfPenaltyBoxX = 13.5f;
float cField::mfPenaltyBoxY = 4.5f;

/**
 * Offset/Address/Size: 0x1CC | 0x800192B0 | size: 0x28
 */
void cField::Init(cNet* net0, cNet* net1)
{
    mpNet[0] = net0;
    mpNet[1] = net1;
    net0->m_v3NetLocation.x = -mv3FieldPosition.x;
    net1->m_v3NetLocation.x = mv3FieldPosition.x;
}

/**
 * Offset/Address/Size: 0x1A4 | 0x80019288 | size: 0x28
 */
float cField::GetGoalLineX(float side)
{
    if (side > 0)
    {
        return mv3FieldPosition.x;
    }
    return -mv3FieldPosition.x;
}

/**
 * Offset/Address/Size: 0x180 | 0x80019264 | size: 0x24
 */
float cField::GetGoalLineX(unsigned int side)
{
    if (side > 0)
    {
        return mv3FieldPosition.x;
    }
    return -mv3FieldPosition.x;
}

/**
 * Offset/Address/Size: 0x154 | 0x80019238 | size: 0x2C
 */
float cField::GetSidelineY(unsigned int side)
{
    if (side > 0)
    {
        return mv3FieldPosition.y;
    }
    return -mv3FieldPosition.y;
}

float cField::GetGroundZ()
{
    return mv3FieldPosition.z;
}

/**
 * Offset/Address/Size: 0x14C | 0x80019230 | size: 0x8
 */
float cField::GetCornerRadius()
{
    return 3.0f;
}

/**
 * Offset/Address/Size: 0x130 | 0x80019214 | size: 0x1C
 */
float cField::GetPenaltyBoxX(unsigned int side)
{
    return (side > 0) ? mfPenaltyBoxX : -mfPenaltyBoxX;
}

/**
 * Offset/Address/Size: 0x128 | 0x8001920C | size: 0x8
 */
float cField::GetPenaltyBoxY()
{
    return mfPenaltyBoxY;
}

/**
 * Offset/Address/Size: 0x108 | 0x800191EC | size: 0x20
 */
cNet* cField::GetNet(float side)
{
    if (side > 0.0f)
    {
        return mpNet[1];
    }
    return mpNet[0];
}

cNet* cField::GetNet(unsigned int index)
{
    return mpNet[index];
}

/**
 * Offset/Address/Size: 0xBC | 0x800191A0 | size: 0x4C
 */
bool cField::IsOnField(const nlVector3& location)
{
    if ((float)fabs(location.x) <= mv3FieldPosition.x)
    {
        if ((float)fabs(location.y) <= mv3FieldPosition.y)
        {
            return true;
        }
    }
    return false;
}

static inline float FixComponent(float component, float half, float fMinDistanceFromWall)
{
    float min = -half + fMinDistanceFromWall;
    float max = half - fMinDistanceFromWall;
    component = (component >= min) ? component : min;
    component = (component <= max) ? component : max;
    return component;
}

/**
 * Offset/Address/Size: 0x2C | 0x80019110 | size: 0x90
 */
void cField::FixOutOfBoundsPosition(nlVector3& v, float fMinDistanceFromWall)
{
    v.x = FixComponent(v.x, GetFieldPosition().x, fMinDistanceFromWall);
    v.y = FixComponent(v.y, GetFieldPosition().y, fMinDistanceFromWall);
}

/**
 * Offset/Address/Size: 0x0 | 0x800190E4 | size: 0x2C
 */
void cField::SetFieldDimensions(float fX, float fY, float fZ)
{
    mpNet[0]->m_v3NetLocation.x = -mv3FieldPosition.x;
    mpNet[1]->m_v3NetLocation.x = mv3FieldPosition.x;
}
