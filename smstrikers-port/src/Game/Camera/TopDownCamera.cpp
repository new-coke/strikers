#include "Game/Camera/TopDownCamera.h"
#include "NL/nlMath.h"
#include "NL/gl/glMatrix.h"

#include "Game/Ball.h"
#include "Game/Field.h"

static float sfCameraHeight = 21.0f;
static float sfFov = 90.0f;
static bool sbFocusOnBall = false;

extern cBall* g_pBall;

/**
 * Offset/Address/Size: 0x174 | 0x801A6074 | size: 0x60
 */
TopDownCamera::TopDownCamera()
{
    m_Fov = sfFov;
    nlVec3Set(m_Position, 0.0f, -1.0f, 21.0f);
    nlVec3Set(m_Target, 0.0f, 0.0f, 0.0f);
}

/**
 * Offset/Address/Size: 0xBC | 0x801A5FBC | size: 0xB8
 */
void TopDownCamera::Update(float dt)
{
    if (sbFocusOnBall)
    {
        m_Target = g_pBall->m_v3Position;
    }
    else
    {
        nlVec3Set(m_Target, 0.f, 0.f, 0.f);
    }

    m_Fov = 90.0f;

    nlVec3Set(m_Position, m_Target.x, m_Target.y - 1.0f, sfCameraHeight);

    float gx = cField::GetGoalLineX((unsigned int)1);
    if (m_Position.x > gx)
        m_Position.x = gx;

    float ng = -gx;
    if (m_Position.x < ng)
        m_Position.x = ng;
}

/**
 * Offset/Address/Size: 0x5C | 0x801A5F5C | size: 0x60
 */
const nlMatrix4& TopDownCamera::GetViewMatrix() const
{
    nlVector3 up = { 0.0f, 1.0f, 0.0f };
    glMatrixLookAt((nlMatrix4&)m_View, m_Position, m_Target, up);
    return m_View;
}

/**
 * Offset/Address/Size: 0x0 | 0x801A5F00 | size: 0x5C
 */
TopDownCamera::~TopDownCamera()
{
    // empty
}
