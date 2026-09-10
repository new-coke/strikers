#ifndef PPC_GENERIC_TARGIMPL_H
#define PPC_GENERIC_TARGIMPL_H

#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/nubevent.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

void TRKTargetSetInputPendingPtr(void* ptr);

void TRKSwapAndGo();
void TRKTargetSetStopped(unsigned int);
DSError TRKTargetInterrupt(TRKEvent*);
DSError TRKTargetSupportRequest();
void TRKDestructEvent(TRKEvent*);
DSError TRKTargetFlushCache(u8, void* start, void* end);
BOOL TRKTargetStopped(void);
DSError TRKTargetAddStopInfo(TRKBuffer* b);
// DSError TRKTargetAddExceptionInfo(TRKBuffer* b);
void TRKTargetAddExceptionInfo(TRKBuffer* b);
DSError TRKTargetAccessARAM(u32 p1, u32 p2, u32* p3, BOOL read);
DSError TRKTargetAccessMemory(void* data, u32 start, size_t* length, MemoryAccessOptions accessOptions, BOOL read);
DSError TRKTargetAccessDefault(u32 firstRegister, u32 lastRegister, TRKBuffer* b, size_t* registersLengthPtr, BOOL read);
DSError TRKTargetAccessFP(u32 firstRegister, u32 lastRegister, TRKBuffer* b, size_t* registersLengthPtr, BOOL read);
DSError TRKTargetAccessExtended1(u32 firstRegister, u32 lastRegister, TRKBuffer* b, size_t* registersLengthPtr, BOOL read);
DSError TRKTargetAccessExtended2(u32 firstRegister, u32 lastRegister, TRKBuffer* b, size_t* registerStorageSize, BOOL read);
u32 TRKTargetGetPC();
DSError TRKTargetSingleStep(u32 count, BOOL stepOver);
DSError TRKTargetStepOutOfRange(u32 rangeStart, u32 rangeEnd, BOOL stepOver);
u32 TRKTargetStop();
void TRKInterruptHandler();
void TRKPostInterruptEvent(void);

DSError TRKTargetVersions(DSVersions* versions);
DSError TRKTargetSupportMask(u8 mask[32]);

extern ProcessorState_PPC gTRKCPUState;
extern TRKState gTRKState;

#ifdef __cplusplus
}
#endif

#endif /* PPC_GENERIC_TARGIMPL_H */
