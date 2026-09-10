#include "Game/FE/tlInstance.h"
#include "NL/nlMath.h"

#include "math.h"

/**
 * Offset/Address/Size: 0x0 | 0x8020FF80 | size: 0x18
 */
void TLInstance::SetAssetColour(const nlColour& color)
{
    m_overloadFlags |= 0x10;
    m_overloadedAttributes.colour = color;
}

/**
 * Offset/Address/Size: 0x18 | 0x8020FF98 | size: 0x1C
 */
void TLInstance::SetAssetScale(float x, float y, float z)
{
    this->m_overloadFlags |= 4;
    m_overloadedAttributes.v3Scale.f.x = x;
    m_overloadedAttributes.v3Scale.f.y = y;
    m_overloadedAttributes.v3Scale.f.z = z;
}

/**
 * Offset/Address/Size: 0x34 | 0x8020FFB4 | size: 0x1C
 */
void TLInstance::SetAssetRotation(float x, float y, float z)
{
    this->m_overloadFlags |= 2;
    m_overloadedAttributes.v3Rotation.f.x = x;
    m_overloadedAttributes.v3Rotation.f.y = y;
    m_overloadedAttributes.v3Rotation.f.z = z;
}

/**
 * Offset/Address/Size: 0x50 | 0x8020FFD0 | size: 0x1C
 */
void TLInstance::SetAssetPosition(float x, float y, float z)
{
    this->m_overloadFlags |= 1;
    m_overloadedAttributes.v3Position.f.x = x;
    m_overloadedAttributes.v3Position.f.y = y;
    m_overloadedAttributes.v3Position.f.z = z;
}

/**
 * Offset/Address/Size: 0x6C | 0x8020FFEC | size: 0x8C
 */
bool TLInstance::IsValidAtTime(float fCurrentTime)
{
    float sinceStart;
    float duration;
    float elapsed;
    bool valid;

    valid = true;
    sinceStart = fCurrentTime - m_fStartTime;
    if (!(sinceStart > 0.0001f))
    {
        if (!((float)fabs(sinceStart) <= 0.0001f))
        {
            valid = false;
        }
    }

    if (valid != 0)
    {
        valid = 1;
        duration = m_fDuration;
        elapsed = fCurrentTime - m_fStartTime;
        if (!((m_fDuration - elapsed) > 0.0001f))
        {
            if (!((float)fabs(elapsed - duration) <= 0.0001f))
            {
                valid = false;
            }
        }

        if (valid)
        {
            return true;
        }
    }

    return false;
}

/**
 * Offset/Address/Size: 0xF8 | 0x80210078 | size: 0x38
 */
nlColour& TLInstance::GetColour() const
{
    if (m_overloadFlags & 0x10)
    {
        return const_cast<nlColour&>(m_overloadedAttributes.colour);
    }
    return m_component->GetColour();
}

/**
 * Offset/Address/Size: 0x130 | 0x802100B0 | size: 0x38
 */
feVector3& TLInstance::GetScale() const
{
    if (m_overloadFlags & 0x4)
    {
        return const_cast<feVector3&>(m_overloadedAttributes.v3Scale);
    }
    return m_component->GetScale();
}

/**
 * Offset/Address/Size: 0x168 | 0x802100E8 | size: 0x38
 */
feVector3& TLInstance::GetRotation() const
{
    if (m_overloadFlags & 0x2)
    {
        return const_cast<feVector3&>(m_overloadedAttributes.v3Rotation);
    }
    return m_component->GetRotation();
}

/**
 * Offset/Address/Size: 0x1A0 | 0x80210120 | size: 0x38
 */
feVector3& TLInstance::GetPosition() const
{
    if (m_overloadFlags & 0x1)
    {
        return const_cast<feVector3&>(m_overloadedAttributes.v3Position);
    }
    return m_component->GetPosition();
}

/**
 * Offset/Address/Size: 0x1D8 | 0x80210158 | size: 0x8
 */
nlColour& TLInstance::GetAssetColour() const
{
    return const_cast<nlColour&>(m_overloadedAttributes.colour);
}

/**
 * Offset/Address/Size: 0x1E0 | 0x80210160 | size: 0x8
 */
feVector3& TLInstance::GetAssetScale() const
{
    return const_cast<feVector3&>(m_overloadedAttributes.v3Scale);
}

feVector3& TLInstance::GetAssetRotation() const
{
    return const_cast<feVector3&>(m_overloadedAttributes.v3Rotation);
}

/**
 * Offset/Address/Size: 0x1E8 | 0x80210168 | size: 0x8
 */
feVector3& TLInstance::GetAssetPosition() const
{
    return const_cast<feVector3&>(m_overloadedAttributes.v3Position);
}

// eTimeLineAssetType TLInstance::GetType() const
// {
//     return m_type;
// }
