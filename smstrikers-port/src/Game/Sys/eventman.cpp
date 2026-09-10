#include "Game/Sys/eventman.h"
#include "dolphin/os.h"
#include <stdio.h>
#include <stdlib.h>

#include "NL/nlString.h"
#include "NL/nlDLRing.h"
#include "NL/nlMemory.h"

EventManager* g_pEventManager = 0;

/**
 * Offset/Address/Size: 0x488 | 0x801FAE08 | size: 0x94
 */
EventManager::~EventManager()
{
    if (m_pool)
    {
        delete (u32*)m_pool;
    }

    if (m_dest)
    {
        delete[] m_dest;
        m_dest = 0;
    }

    if (m_handlers)
    {
        nlDeleteDLRing(&m_handlers);
    }
}

void EventManager::AllocateEvents(unsigned long uEventCount, unsigned long uEventSize)
{
    m_count = uEventCount;
    m_size = uEventSize;

    u32 total = m_size * m_count;
    m_pool = (char*)nlMalloc(total, 8, false);

    nlPrintf("Event Manager: Allocating %d events of size %d. Total = %d bytes\n",
        m_count,
        m_size,
        total);

    for (u32 i = 0; i < uEventCount; i++)
    {
        Event* e = new (m_pool + m_size * i) Event();
        nlDLRingAddEnd(&m_free, e);
    }
}

EventManager::EventManager(unsigned long uEventCount, unsigned long uEventSize)
    : m_dispatching(false)
    , m_handlers(NULL)
    , m_free(NULL)
    , m_keep(NULL)
    , m_queue(NULL)
    , m_deferred(NULL)
    , m_dest(NULL)
    , m_pool(NULL)
{
    AllocateEvents(uEventCount, uEventSize);
    SetupDestArray();
}

void EventManager::FlushEventQueue()
{
    Event* pEvent;
    while (m_keep != NULL && m_keep->m_next->m_nReferenceCount == 0)
    {
        pEvent = m_keep->m_next;
        nlDLRingRemove<Event>(&m_keep, pEvent);
        nlDLRingAddEnd<Event>(&m_free, pEvent);
    }
}

/**
 * Offset/Address/Size: 0x370 | 0x801FACF0 | size: 0x118
 */
void EventManager::Create(unsigned long uEventCount, unsigned long uEventSize)
{
    EventManager* m = new (nlMalloc(sizeof(EventManager), 8, false)) EventManager(uEventCount, uEventSize);
    g_pEventManager = m;
}

/**
 * Offset/Address/Size: 0x30C | 0x801FAC8C | size: 0x64
 */
#pragma push
#pragma optimization_level 1
#pragma optimize_for_size on
#pragma scheduling off
EventHandler* EventManager::AddEventHandler(EventCallback pEventHandlerFunc, void* pParam, unsigned long uDestinationMask)
{
    EventHandler* eventHandler = (EventHandler*)nlMalloc(sizeof(EventHandler), 8, 0);
    EventHandler** head = &m_handlers;

    eventHandler->m_pCBFunction = pEventHandlerFunc;
    EventHandler* element = eventHandler;
    eventHandler->m_uDestinationMask = uDestinationMask;
    eventHandler->m_pCBParam = pParam;
    nlDLRingAddEnd<EventHandler>(head, element);
    return eventHandler;
}
#pragma pop

/**
 * Offset/Address/Size: 0x2D4 | 0x801FAC54 | size: 0x38
 */
void EventManager::RemoveEventHandler(EventHandler* h)
{
    nlDLRingRemove<EventHandler>(&m_handlers, h);
    delete h;
}

/**
 * Offset/Address/Size: 0x1C4 | 0x801FAB44 | size: 0x110
 */
void EventManager::AllocateDestArray(unsigned long count, unsigned long maskDefault)
{
    m_dest = (u32*)nlMalloc(count * sizeof(u32), 8, false);
    for (u32 i = 0; i < count; ++i)
    {
        m_dest[i] = maskDefault;
    }
}

/**
 * Offset/Address/Size: 0xF4 | 0x801FAA74 | size: 0xD0
 */
// PORT: pool occupancy, measured rather than guessed at.
static unsigned long port_ev_ring_len(const Event* head)
{
    unsigned long n = 0;
    if (head != NULL)
    {
        const Event* it = head->m_next;
        for (;;)
        {
            ++n;
            if (it == head || n > 65536)
                break;
            it = it->m_next;
        }
    }
    return n;
}

// snprintf returns the length it would have written, so advance by what actually fitted.
static void port_ev_cat_name(char* buf, size_t cap, size_t* pos,
                             const char* name, unsigned long count)
{
    if (*pos + 1 >= cap)
        return;
    int n = snprintf(buf + *pos, cap - *pos, " %s[%lu]:", name, count);
    if (n <= 0)
        return;
    *pos = (*pos + (size_t)n < cap - 1) ? *pos + (size_t)n : cap - 1;
}

static void port_ev_cat_id(char* buf, size_t cap, size_t* pos, unsigned long id)
{
    if (*pos + 1 >= cap)
        return;
    int n = snprintf(buf + *pos, cap - *pos, " %lx", id);
    if (n <= 0)
        return;
    *pos = (*pos + (size_t)n < cap - 1) ? *pos + (size_t)n : cap - 1;
}

static void port_ev_dump(const char* why, const EventManager* m)
{
    // The ids, in the order the rings hold them: what the peak is made of is the whole question.
    char line[1024];
    size_t pos = 0;
    line[0] = '\0';
    static const char* const kRing[3] = { "queue", "deferred", "keep" };
    const Event* const heads[3] = { m->m_queue, m->m_deferred, m->m_keep };
    for (int r = 0; r < 3; ++r)
    {
        port_ev_cat_name(line, sizeof(line), &pos, kRing[r],
                         port_ev_ring_len(heads[r]));
        const Event* head = heads[r];
        if (head != NULL)
        {
            const Event* it = head->m_next;
            for (unsigned n = 0; n < 200 && pos + 8 < sizeof(line); ++n)
            {
                port_ev_cat_id(line, sizeof(line), &pos,
                               (unsigned long)it->m_uEventID);
                if (it == head)
                    break;
                it = it->m_next;
            }
        }
    }
    line[sizeof(line) - 1] = '\0';
    OSReport("[eventman] %s: free %lu of %u;%s\n", why,
             port_ev_ring_len(m->m_free), (unsigned)m->m_count, line);
}

static bool port_ev_logging(void)
{
    static int s_on = -1;
    if (s_on < 0)
    {
        const char* e = getenv("STRIKERS_LOG_EVENTS");
        s_on = (e != NULL && *e != '\0' && *e != '0') ? 1 : 0;
    }
    return s_on != 0;
}

Event* EventManager::CreateValidEvent(unsigned long eventID, unsigned long uSize)
{
    if (uSize > m_size)
    {
        nlPrintf("Event Manager: Size mismatch on event creation (%d vs %d)!\n", m_size, uSize); // @326
        // PORT: nlPrintf is a no-op here, and the unchecked caller is about to dereference this NULL.
        OSReport("[eventman] event 0x%x wants %u bytes, slot holds %u; "
                 "returning NULL, caller will fault\n",
                 (unsigned)eventID, (unsigned)uSize, (unsigned)m_size);
        return NULL;
    }

    Event* e = GetFreeEvent();

    if (!e)
    {
        // PORT: nlPrintf (in GetFreeEvent) is a no-op here, and the unchecked caller is about to dereference this NULL.
        OSReport("[eventman] pool exhausted (%u events outstanding); "
                 "returning NULL, caller will fault\n", (unsigned)m_count);
        port_ev_dump("exhausted", this);
        return NULL;
    }

    e->m_uEventID = eventID; // stw at +0x08

    // PORT: Off unless asked for: the walk is O(pool) and this is called a hundred times a second.
    if (port_ev_logging())
    {
        static unsigned long s_peak = 0;
        // The three live rings, because the free ring is the long one and must not bound a peak measurement.
        unsigned long outstanding = port_ev_ring_len(m_queue)
                                  + port_ev_ring_len(m_deferred)
                                  + port_ev_ring_len(m_keep);
        if (outstanding > s_peak)
        {
            s_peak = outstanding;
            port_ev_dump("new peak", this);
        }
    }

    return e;
}

Event* EventManager::GetFreeEvent()
{
    Event* e;
    if (!m_free)
    {
        nlPrintf("Event Manager: There are no more free events in the free event list!\n"); // @293
        e = NULL;
    }
    else
    {
        e = nlDLRingRemoveStart(&m_free);
        if (m_dispatching)
        {
            nlDLRingAddEnd(&m_deferred, e);
        }
        else
        {
            nlDLRingAddEnd(&m_queue, e);
        }
    }
    return e;
}

/**
 * Offset/Address/Size: 0x0 | 0x801FA980 | size: 0xF4
 */
void EventManager::DispatchEvents()
{
    m_dispatching = 1; // stb 1 at +0
    while (m_queue != 0)
    {
        Event* e = nlDLRingRemoveStart(&m_queue); // free->queue ring
        if (m_handlers != 0)
        {
            EventHandler* it = nlDLRingGetStart(m_handlers);
            do
            {
                u32 mask = m_dest[e->m_uEventID]; // dest[type]
                if ((it->m_uDestinationMask & mask) != 0)
                {
                    void (*fn)(Event*, void*) = it->m_pCBFunction;
                    void* ud = it->m_pCBParam;
                    (*fn)(e, ud); // bctrl
                }

                if (it == m_handlers)
                    break;

                it = it->m_next;
            } while (1);
        }
        if (e->m_nReferenceCount != 0)
        {
            nlDLRingAddEnd(&m_keep, e);
        }
        else
        {
            nlDLRingAddEnd(&m_free, e);
        }
    }

    m_dispatching = 0;
    m_queue = m_deferred;
    m_deferred = NULL;
}
