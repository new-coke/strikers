#include "Game/OverlayHandlerSTSX2.h"
#include "Game/BaseSceneHandler.h"
#include "Game/FE/FEAudio.h"
#include "Game/Sys/eventman.h"
#include "NL/nlBundleFile.h"
#include "NL/nlAlgorithm.h"
#include "NL/nlTask.h"

void STSX2Overlay::CreateEventHandler()
{
    CreateEventHandler();
}

void STSX2Overlay::DestroyEventHandler()
{
    DestroyEventHandler();
}

/**
 * Offset/Address/Size: 0x280 | 0x8010705C | size: 0xA8
 */
STSX2Overlay::STSX2Overlay()
    : BaseOverlayHandler(0x102, (ScreenPosition)0xA)
{
    m_EventHandler = NULL;
    m_EventHandler = g_pEventManager->AddEventHandler(EventHandlerFunc, this, 1);
}

/**
 * Offset/Address/Size: 0x1E8 | 0x80106FC4 | size: 0x98
 */
STSX2Overlay::~STSX2Overlay()
{
    if (this->m_EventHandler)
    {
        g_pEventManager->RemoveEventHandler(this->m_EventHandler);
        this->m_EventHandler = NULL;
    }
}

/**
 * Offset/Address/Size: 0x1B8 | 0x80106F94 | size: 0x30
 */
void STSX2Overlay::SceneCreated()
{
    this->SetVisible(false);
}

/**
 * Offset/Address/Size: 0xF0 | 0x80106ECC | size: 0xC8
 */
void STSX2Overlay::Update(float fDeltaT)
{
    if (this->mVisibilityMask & nlTaskManager::m_pInstance->m_CurrState)
    {
        if (!m_bVisible)
        {
            FEAudio::EnableSounds(false);
        }
        BaseSceneHandler::Update(fDeltaT);
        if (m_bVisible)
        {
            TLSlide* CurrentSlide = this->m_pFEPresentation->m_currentSlide;
            if (CurrentSlide != NULL)
            {
                if (CurrentSlide->m_time >= (CurrentSlide->m_start + CurrentSlide->m_duration))
                {
                    SetVisible(false);
                }
            }
        }
        FEAudio::EnableSounds(true);
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80106DDC | size: 0xF0
 */
void STSX2Overlay::EventHandlerFunc(Event* event, void* userData)
{
    STSX2Overlay* self = static_cast<STSX2Overlay*>(userData);

    if (event->m_uEventID == 5)
    {
        EventData* data;
        s32 id = port_event_data_id(&event->m_data);
        if (id == -1)
        {
            nlPrintf("Error: Trying to get event data on event with none!\n");
            data = NULL;
        }
        else
        {
            id = port_event_data_id(&event->m_data);
            if (id != 0x18A)
            {
                nlPrintf("Error: GetData() failed! Data types do not match!\n");
                data = NULL;
            }
            else
            {
                data = &event->m_data;
            }
        }

        u32 val = (*(u16*)((u8*)data + 6) >> 1) & 0x7FFF;
        if (val == 6)
        {
            FEPresentation* pres = self->m_pFEPresentation;
            pres->m_fadeDuration = pres->m_currentSlide->m_start;
            self->m_pFEPresentation->Update(0.0f);
            self->SetVisible(true);
        }
    }
}
