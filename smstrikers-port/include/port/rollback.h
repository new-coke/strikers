// Rollback: the simulation's state as one snapshot per tick, kept in a ring, restored on demand.

#ifndef PORT_ROLLBACK_H
#define PORT_ROLLBACK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// The simulation heap: allocations inside a scope come from it, and a snapshot copies it whole.
void PortSimHeapEnter(void);
void PortSimHeapLeave(void);
// Presentation the tick calls into allocates from the region, outside what a rollback restores.
void PortSimHeapPause(void);
void PortSimHeapResume(void);
int PortSimHeapPaused(void);
int PortSimHeapOwns(const void* p);
size_t PortSimHeapUsed(void);

// Static storage the simulation uses outside its heap; each file registers its own, idempotently.
void PortSimState(void* p, size_t size);
// Drops a registration whose storage is going away; every saved tick is forgotten with it.
void PortSimStateForget(void* p);

// A static slot pool the simulation holds slots of; outside a tick it lends from a twin.
struct SlotPoolBase;
void PortSimSlotPool(struct SlotPoolBase* pool);

// State that is not one span: `save` appends bytes, `load` reads them back and returns the count.
typedef void (*PortSimStateSave)(void (*append)(const void* p, size_t size));
typedef size_t (*PortSimStateLoad)(const unsigned char* in);
void PortSimStateHook(PortSimStateSave save, PortSimStateLoad load);

// Rollback ring: the state at the top of tick `tick`.
enum
{
    PORT_ROLLBACK_TICKS = 16
};
// Ticks from here name diagnostics' slots, apart from the ring a rollback restores from.
#define PORT_ROLLBACK_SCRATCH 0xFFFFFFF0u
void PortRollbackSave(unsigned int tick);
int PortRollbackHas(unsigned int tick);
void PortRollbackLoad(unsigned int tick);
void PortRollbackReset(void);

// Every machine's input is in through this tick; offline every tick that ran is.
unsigned int PortRollbackConfirmed(void);
void PortRollbackSetConfirmed(unsigned int tick);

// A physics object is going away; sounds that follow it stay where it was.
void PortSoundsForgetObject(const void* obj);

// Non-zero while ticks are being run again after a load; sounds, rumble and effects stay quiet.
int PortRollbackResimulating(void);
void PortRollbackSetResimulating(int on);

#ifdef __cplusplus
}

// A simulation heap scope for the code that builds the match's simulation objects.
struct PortSimHeapScope
{
    PortSimHeapScope() { PortSimHeapEnter(); }
    ~PortSimHeapScope() { PortSimHeapLeave(); }
    PortSimHeapScope(const PortSimHeapScope&) = delete;
    PortSimHeapScope& operator=(const PortSimHeapScope&) = delete;
};

struct PortSimHeapPauseScope
{
    PortSimHeapPauseScope() { PortSimHeapPause(); }
    ~PortSimHeapPauseScope() { PortSimHeapResume(); }
    PortSimHeapPauseScope(const PortSimHeapPauseScope&) = delete;
    PortSimHeapPauseScope& operator=(const PortSimHeapPauseScope&) = delete;
};
#endif

#endif   // PORT_ROLLBACK_H
