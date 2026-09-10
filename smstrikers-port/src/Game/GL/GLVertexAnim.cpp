#include "Game/GL/GLVertexAnim.h"

/**
 * Offset/Address/Size: 0x198 | 0x801E7E30 | size: 0x40
 */
GLVertexAnim::GLVertexAnim()
{
    m_uHashID = -1;
    m_nNumFrames = 0;
    m_nNumVertices = 0;
    m_fFrameRate = 30.0f;
    m_eMode = GLVAnimMode_Loop;
    m_bDone = false;
    m_fFrame = 0.0f;
    m_fTimeScale = 1.0f;
    m_pVertices = NULL;
    m_pModel = NULL;
}

/**
 * Offset/Address/Size: 0xBC | 0x801E7D54 | size: 0xDC
 */
glModel* GLVertexAnim::GetModel(int frame)
{
    int actualFrame = (frame < 0) ? (int)m_fFrame : frame;

    glModel* model = glModelDup(m_pModel, true);
    nlVector3* vertices = m_pVertices + actualFrame * m_nNumVertices;

    for (glModelPacket* packet = model->packets; packet < model->packets + model->numPackets; packet++)
    {
        glModelStream* streams = packet->streams;
        glModelStream* endVertexData = streams + packet->numStreams;
        while (streams < endVertexData)
        {
            if (streams->id == 0)
            {
                streams->address = (uintptr_t)vertices;
                streams->beData = 0;   // PORT: host order, see glModelStream
                // PORT: one frame's worth; the model's chunk is larger.
                streams->dataSize =
                    (u32)(m_nNumVertices * sizeof(nlVector3));
                break;
            }
            streams++;
        }
    }

    return model;
}

/**
 * Offset/Address/Size: 0x1C | 0x801E7CB4 | size: 0xA0
 */
void GLVertexAnim::Update(float dt)
{
    if (m_bDone)
    {
        return; // Early return if animation is stopped
    }

    // Update animation progress
    m_fFrame += m_fFrameRate * (m_fTimeScale * dt);

    // Check if animation has completed
    if (m_fFrame >= m_nNumFrames)
    {
        if (m_eMode == 1)
        {
            // Stop animation at the end
            m_bDone = true;
            m_fFrame = m_nNumFrames - 1;
        }
        else
        {
            // Loop animation
            m_fFrame = 0.0f;
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x801E7C98 | size: 0x1C
 */
void GLVertexAnim::Reset()
{
    m_fTimeScale = 1.0f;
    m_fFrame = 0.0f;
    m_bDone = false;
}
