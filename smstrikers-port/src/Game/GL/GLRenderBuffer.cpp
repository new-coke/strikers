#include "Game/GL/GLRenderBuffer.h"

#include "NL/nlMemory.h"

bool GLRenderBuffer::m_bInstance = false;

GLRenderBuffer glRenderBuffer;

GLRenderBuffer::GLRenderBuffer()
{
    m_bInstance = true;
    m_bEnabled = false;
    m_bSending = false;
    m_bExclusive = false;
    m_uAllocated = 0;
}

/**
 * Offset/Address/Size: 0x0 | 0x801E7FB0 | size: 0x124
 */
GLRenderBuffer::~GLRenderBuffer()
{
    DLListEntry<GLDrawableData*>* head;
    DLListEntry<GLDrawableData*>* current = nlDLRingGetStart<DLListEntry<GLDrawableData*> >(m_drawableData.m_Head);
    head = m_drawableData.m_Head;

    while (current != 0)
    {
        nlFree(current->entry);

        if (nlDLRingIsEnd<DLListEntry<GLDrawableData*> >(head, current) || current == 0)
        {
            current = 0;
        }
        else
        {
            current = current->m_next;
        }
    }

    m_drawableData.Clear();
}
