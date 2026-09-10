#include "Game/EndFrameTask.h"
#include <stdlib.h>
#include "NL/nlTask.h"
#include "Game/GameRenderTask.h"

#include "NL/gl/gl.h"
#include "Game/Debug/FrameCounter.h"

//  */
// void EndFrameTask::GetName()
// {
// }

/**
 * Offset/Address/Size: 0x0 | 0x8016E694 | size: 0x40
 */
void EndFrameTask::Run(float dt)
{
    if (getenv("STRIKERS_LOG_NIS") != NULL)
    {
        static int nStateLog = 0;
        static unsigned int uLastState = 0xFFFFFFFFu;
        unsigned int uState = (unsigned int)nlTaskManager::m_pInstance->m_CurrState;
        if (uState != uLastState || (nStateLog++ % 120) == 0)
        {
            uLastState = uState;
            OSReport("[task] state=0x%x renderWorld=%d\n",
                     uState, (int)g_bRenderWorld);
        }
    }

    glEndFrame();
    g_FrameCounter.StartTimer(1);
    glSendFrame();
    g_FrameCounter.FinishTiming();
}
