#ifndef _EVENTMAN_H_
#define _EVENTMAN_H_

#include "types.h"

class EventHandler;
class EventManager;

struct EventData
{
    virtual u32 GetID() { return -1; }
};

// PORT: EventData's id must be read through a pointer or the call devirtualises to the base and always answers -1.
u32 port_event_data_id(EventData* pData);

class Event
{
public:
    /* 0x00 */ Event* m_next;
    /* 0x04 */ Event* m_prev;
    /* 0x08 */ u32 m_uEventID;
    /* 0x0C */ s32 m_nReferenceCount;
    /* 0x10 */ EventData m_data;

    Event()
        : m_next(NULL)
        , m_prev(NULL)
        , m_nReferenceCount(0)
    {
    }

    template <typename T>
    void GetData(T**);
};

typedef void (*EventCallback)(Event*, void*);

class EventHandler
{
public:
    inline void set(EventCallback pEventHandlerFunc, unsigned long uDestinationMask, void* pParam)
    {
        m_pCBFunction = pEventHandlerFunc;
        m_uDestinationMask = uDestinationMask;
        m_pCBParam = pParam;
    }

    /* 0x00 */ EventHandler* m_next;
    /* 0x04 */ EventHandler* m_prev;
    /* 0x08 */ void (*m_pCBFunction)(class Event*, void*);
    /* 0x0C */ void* m_pCBParam;
    /* 0x10 */ unsigned long m_uDestinationMask;
}; // total size: 0x14

class EventManager
{
public:
    EventManager(unsigned long uEventCount, unsigned long uEventSize);
    static void Create(unsigned long uEventCount, unsigned long uEventSize);
    void SetupDestArray();
    EventHandler* AddEventHandler(EventCallback pEventHandlerFunc, void* pParam, unsigned long uDestinationMask);
    void RemoveEventHandler(EventHandler* h);
    void AllocateEvents(unsigned long uEventCount, unsigned long uEventSize);
    void AllocateDestArray(unsigned long count, unsigned long maskDefault);
    Event* GetFreeEvent();
    Event* CreateValidEvent(unsigned long eventID, unsigned long uSize);
    void DispatchEvents();
    void FlushEventQueue();

    /* 0x00 */ bool m_dispatching;
    /* 0x04 */ EventHandler* m_handlers;
    /* 0x08 */ Event* m_free;
    /* 0x0C */ Event* m_keep;
    /* 0x10 */ Event* m_queue;
    /* 0x14 */ Event* m_deferred;
    /* 0x18 */ u32* m_dest;
    /* 0x1C */ char* m_pool;
    /* 0x20 */ u32 m_count;
    /* 0x24 */ u32 m_size;

    virtual ~EventManager();
};

extern EventManager* g_pEventManager;

#endif // _EVENTMAN_H_
