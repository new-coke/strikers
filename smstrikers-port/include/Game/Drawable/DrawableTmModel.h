#ifndef _DRAWABLETMMODEL_H_
#define _DRAWABLETMMODEL_H_

#include "Game/Drawable/DrawableModel.h"
#include "Game/World/worldanim.h"

class WorldAnimController;

class DrawableTmModel : public DrawableModel
{
public:
    DrawableTmModel()
        : m_pAnimController(NULL)
    {
    }

    DrawableTmModel(const DrawableTmModel& other)
        : DrawableModel(other)
        , m_pAnimController(other.m_pAnimController)
        , m_uAnimBoneIndex(other.m_uAnimBoneIndex) {
        };

    const nlMatrix4& GetAnimatedWorldMatrix();

    virtual DrawableObject* Clone() const;
    virtual void Draw();

    /**
     * Offset/Address/Size: 0x0 | 0x80122CE4 | size: 0x24
     */
    virtual void SetAnimation(const char* name, unsigned long playMode)
    {
        m_pAnimController->SetAnimation(name, (ePlayMode)playMode);
    }

    /**
     * Offset/Address/Size: 0x24 | 0x80122D08 | size: 0xC
     */
    virtual void SetAnimationSpeed(float speed)
    {
        m_pAnimController->m_fSpeed = speed;
    }

    /**
     * Offset/Address/Size: 0x30 | 0x80122D14 | size: 0x24
     */
    virtual void GetAnimationTime()
    {
        m_pAnimController->GetAnimationTime();
    }

    /**
     * Offset/Address/Size: 0x54 | 0x80122D38 | size: 0x24
     */
    virtual void SetAnimationTime(float time)
    {
        m_pAnimController->SetAnimationTime(time);
    }

    /* 0xAC */ WorldAnimController* m_pAnimController;
    /* 0xB0 */ unsigned long m_uAnimBoneIndex;
}; // total size: 0xB4

#endif // _DRAWABLETMMODEL_H_
