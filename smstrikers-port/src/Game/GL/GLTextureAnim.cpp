#include "Game/GL/GLTextureAnim.h"
#include "NL/nlMemory.h"

/**
 * Offset/Address/Size: 0x2E4 | 0x801E7C6C | size: 0x2C
 */
GLTextureAnim::GLTextureAnim()
{
    m_unk_0x00 = -1;
    m_frameCount = 0;
    m_mode = 0;
    m_direction = 1;
    m_currentFrame = 0;
    m_frames = nullptr;
    m_isStopped = false;
}

/**
 * Offset/Address/Size: 0x288 | 0x801E7C10 | size: 0x5C
 */
GLTextureAnim::~GLTextureAnim()
{
    // Free texture array if it exists
    if (m_frames != nullptr)
    {
        delete[] m_frames;
    }
}

/**
 * Offset/Address/Size: 0x214 | 0x801E7B9C | size: 0x74
 */
void GLTextureAnim::SetNumTextures(int numTextures)
{
    if (m_frames != nullptr)
    {
        delete[] m_frames;
    }

    // Allocate new texture array
    u32 size = numTextures * 8; // 8 bytes per GLAnimTex
    m_frames = (GLAnimTex*)nlMalloc(size, 8, 0);

    m_frameCount = numTextures;
    m_direction = 1;
    m_currentFrame = 0;
    m_currentTime = 0.0f;
}

/**
 * Offset/Address/Size: 0x1FC | 0x801E7B84 | size: 0x18
 */
void GLTextureAnim::SetFrame(int frameIndex)
{
    m_currentFrame = frameIndex % m_frameCount;
}

/**
 * Offset/Address/Size: 0x1DC | 0x801E7B64 | size: 0x20
 */
void GLTextureAnim::SetTexture(int frameIndex, const GLAnimTex& animTex)
{
    GLAnimTex* textureArray = m_frames;
    u32 offset = frameIndex * 8;
    GLAnimTex* targetTex = (GLAnimTex*)((u8*)textureArray + offset);

    targetTex->textureHandle = animTex.textureHandle;
    targetTex->time = animTex.time;
}

/**
 * Offset/Address/Size: 0x1C0 | 0x801E7B48 | size: 0x1C
 */
GLAnimTex* GLTextureAnim::GetTexture(int frameIndex)
{
    GLAnimTex* textureArray = m_frames;

    // If frameIndex is negative, use current frame
    if (frameIndex < 0)
    {
        frameIndex = m_currentFrame;
    }

    u32 offset = frameIndex * 8;
    return (GLAnimTex*)((u8*)textureArray + offset);
}

/**
 * Offset/Address/Size: 0x120 | 0x801E7AA8 | size: 0xA0
 */
u32 GLTextureAnim::GetTextureHandle(float time)
{
    u32 texture = 0;
    GLAnimTex* frame = m_frames;
    int index = 0;
    float delta = time;

    while (true)
    {
        int actualIndex = (index < 0) ? m_currentFrame : index;

        delta -= frame[actualIndex].time;
        texture = frame[actualIndex].textureHandle;

        if (delta <= 0.0f)
        {
            break;
        }

        switch (m_mode)
        {
        case 0:
        {
            index++;
            if (index >= m_frameCount)
            {
                index = 0;
            }
            break;
        }
        case 2:
        {
            index++;
            if (index >= m_frameCount)
            {
                return texture;
            }
            break;
        }
        }
    }

    return texture;
}

/**
 * Offset/Address/Size: 0x0 | 0x801E7988 | size: 0x120
 */
void GLTextureAnim::Update(float dt)
{
    s32 backwardFrame;
    s32 advancedFrame;
    s32 nextFrame;
    s32 forwardFrame;
    s32 frameCount;

    if (m_isStopped || m_frameCount < 2)
    {
        return;
    }

    m_currentTime += dt;
    GLAnimTex* frame = m_frames + m_currentFrame;

    if (m_currentTime >= frame->time)
    {
        m_currentTime = 0.0f;
        switch (m_mode)
        {
        case 0:
            nextFrame = m_currentFrame + 1;
            m_currentFrame = nextFrame;
            if (nextFrame >= m_frameCount)
            {
                m_currentFrame = 0;
                return;
            }
            break;
        case 1:
            if (m_direction > 0)
            {
                forwardFrame = m_currentFrame + 1;
                m_currentFrame = forwardFrame;
                if (forwardFrame >= m_frameCount)
                {
                    m_currentFrame -= 2;
                    m_direction = -1;
                    return;
                }
            }
            else
            {
                backwardFrame = m_currentFrame - 1;
                m_currentFrame = backwardFrame;
                if (backwardFrame < 0)
                {
                    m_currentFrame = 1;
                    m_direction = 1;
                    return;
                }
            }
            break;
        case 2:
            advancedFrame = m_currentFrame + 1;
            m_currentFrame = advancedFrame;
            frameCount = m_frameCount;
            if (advancedFrame >= frameCount)
            {
                m_currentFrame = frameCount - 1;
                return;
            }
            break;
        }
    }
}
