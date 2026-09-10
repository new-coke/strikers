#include "Game/OverlayHandlerDemo.h"
#include "Game/BaseSceneHandler.h"

/**
 * Offset/Address/Size: 0xBC | 0x80105614 | size: 0x58
 */
DemoOverlay::DemoOverlay()
    : BaseOverlayHandler(-1)
{
}

/**
 * Offset/Address/Size: 0x50 | 0x801055A8 | size: 0x6C
 */
DemoOverlay::~DemoOverlay()
{
}

/**
 * Offset/Address/Size: 0x30 | 0x80105588 | size: 0x20
 */
void DemoOverlay::Update(float fDeltaT)
{
    BaseSceneHandler::Update(fDeltaT);
}

/**
 * Offset/Address/Size: 0x0 | 0x80105558 | size: 0x30
 */
void DemoOverlay::SceneCreated()
{
    this->SetVisible(false);
}
