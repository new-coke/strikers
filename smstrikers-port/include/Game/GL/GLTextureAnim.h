#ifndef _GLTEXTUREANIM_H_
#define _GLTEXTUREANIM_H_

#include "types.h"

struct GLAnimTex // size: 8 bytes
{
    /* 0x00 */ u32 textureHandle;
    /* 0x04 */ f32 time;
};

class GLTextureAnim
{
public:
    GLTextureAnim();
    ~GLTextureAnim();

    void SetNumTextures(int numTextures);
    void SetFrame(int frameIndex);
    void SetTexture(int frameIndex, const GLAnimTex& animTex);
    GLAnimTex* GetTexture(int frameIndex);
    u32 GetTextureHandle(float time);
    void Update(float dt);

    /* 0x00 */ s32 m_unk_0x00;
    /* 0x04 */ s32 m_frameCount;
    /* 0x08 */ u32 m_mode;      // ?? 0=loop, 1=ping-pong, 2=once, 3=reverse
    /* 0x0C */ s32 m_direction; // ?? 1=forward, -1=backward (for ping-pong)
    /* 0x10 */ bool m_isStopped;
    /* 0x14 */ u32 m_currentFrame;
    /* 0x18 */ f32 m_currentTime;
    /* 0x1C */ GLAnimTex* m_frames;
};

#endif // _GLTEXTUREANIM_H_
