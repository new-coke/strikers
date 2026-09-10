#ifndef _GLVERTEXANIM_H_
#define _GLVERTEXANIM_H_

#include "types.h"

#include "NL/nlMath.h"
#include "NL/gl/glModel.h"
#include "NL/gl/glUserData.h"

enum eGLVertAnimMode
{
    GLVAnimMode_Loop = 0,
    GLVAnimMode_Hold = 1,
    GLVAnimMode_Num = 2,
};

class GLVertexAnim
{
public:
    GLVertexAnim();

    glModel* GetModel(int frame);
    void Update(float dt);
    void Reset();

    /* 0x00 */ u32 m_uHashID;
    /* 0x04 */ s32 m_nNumFrames;
    /* 0x08 */ s32 m_nNumVertices;
    /* 0x0C */ f32 m_fFrameRate;
    /* 0x10 */ eGLVertAnimMode m_eMode;
    /* 0x14 */ bool m_bDone;
    /* 0x18 */ f32 m_fTimeScale;
    /* 0x1C */ f32 m_fFrame;
    /* 0x20 */ nlVector3* m_pVertices;
    /* 0x24 */ glModel* m_pModel;
}; // total size: 0x28

#endif // _GLVERTEXANIM_H_
