#ifndef _FEANIMATION_H_
#define _FEANIMATION_H_

#include "types.h"

#include "Game/FE/tlInstance.h"
#include "NL/nlDLRing.h"

enum AnimType
{
    eAnimUnknown = 0,
    eAnimPosition = 1,
    eAnimRotation = 2,
    eAnimScale = 3,
    eAnimPivot = 4,
    eAnimColor = 5,
    eAnimOpacity = 6,
};

class FEAnimationKeyframe
{
public:
    /* 0x00 */ float m_fPoint;
    /* 0x04 */ float m_fControl1;
    /* 0x08 */ float m_fControl2;
    /* 0x0C */ float m_fTime;
}; /* size: 0x10 */

typedef struct fAnimationKeyframe
{
    /* 0x00 */ FEAnimationKeyframe pKeyFrameData;
    /* 0x10 */ fAnimationKeyframe* m_next;
    /* 0x14 */ fAnimationKeyframe* m_prev;
} fAnimationKeyframe; /* size: 0x18 */

typedef struct v3AnimationKeyframe
{
    /* 0x00 */ FEAnimationKeyframe pKeyFrameDataX;
    /* 0x10 */ FEAnimationKeyframe pKeyFrameDataY;
    /* 0x20 */ FEAnimationKeyframe pKeyFrameDataZ;
    /* 0x30 */ v3AnimationKeyframe* m_next;
    /* 0x34 */ v3AnimationKeyframe* m_prev;
} v3AnimationKeyframe; /* size: 0x38 */

class FEAnimation /* size >= 0x1C */
{
public:
    virtual ~FEAnimation() { }

    void Update(float fCurrentTime);
    void AnimateTargetAtTimeWithVector3(float fCurrentTime);

    /* 0x04 */ FEAnimation* m_next;
    /* 0x08 */ FEAnimation* m_prev;
    /* 0x0C */ TLInstance* m_pTLInstanceTarget;
    /* 0x10 */ u16 m_cast_type;
    /* 0x12 */ char pad12[2];
    /* 0x14 */ AnimType m_type;
    /* 0x18 */ void* m_DLRingHead;
};

#endif // _FEANIMATION_H_
