#include "Game/SH/SHBackground.h"

#include "NL/nlAlgorithm.h"

/**
 * Offset/Address/Size: 0x16C | 0x800A9A18 | size: 0x44
 */
BackgroundScene::BackgroundScene()
    : BaseSceneHandler()
{
    mPlayMode = PM_USE_SLIDE_DEFAULT;
    mDesiredPlayMode = PM_USE_SLIDE_DEFAULT;
}

/**
 * Offset/Address/Size: 0x110 | 0x800A99BC | size: 0x5C
 */
BackgroundScene::~BackgroundScene()
{
}

void BackgroundScene::ActivatePlayMode()
{
    if (mPlayMode != 3)
    {
        TLSlide* slide = m_pFEPresentation->m_currentSlide;
        switch (mPlayMode)
        {
        case 1:
            slide->m_uPlayMode = TLPM_LOOPING;
            break;
        case 2:
            slide->m_uPlayMode = TLPM_STOP_AT_END;
            break;
        }
        slide->m_time = 0.0f;
        slide->Update(0.0f);
    }
}

/**
 * Offset/Address/Size: 0xA8 | 0x800A9954 | size: 0x68
 */
void BackgroundScene::SceneCreated()
{
    ActivatePlayMode();
}

/**
 * Offset/Address/Size: 0x0 | 0x800A98AC | size: 0xA8
 */
void BackgroundScene::Update(float dt)
{
    if ((mDesiredPlayMode != mPlayMode) && (mPlayMode != PM_USE_SLIDE_DEFAULT))
    {
        TLSlide* slide = m_pFEPresentation->m_currentSlide;
        switch (mPlayMode)
        {
        case PM_LOOPING:
            slide->m_uPlayMode = TLPM_LOOPING;
            break;
        case PM_STOP_AT_END:
            slide->m_uPlayMode = TLPM_STOP_AT_END;
            break;
        }
        slide->m_time = 0.0f;
        slide->Update(0.0f);
    }

    if ((volatile int)mPlayMode == PM_STOPPED)
    {
        (volatile int)1;
    }

    BaseSceneHandler::Update(dt);
}
