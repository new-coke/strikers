#ifndef _NLDEBUG_H_
#define _NLDEBUG_H_

void nlBreak();
#if defined(PORT_VITA)
void nlBreakReason(const char* reason);
#endif
void nlAssertFail(const char* condition, const char* filename, int line, bool bBreak);

#endif // _NLDEBUG_H_
