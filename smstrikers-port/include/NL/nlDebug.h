#ifndef _NLDEBUG_H_
#define _NLDEBUG_H_

void nlBreak();
void nlAssertFail(const char* condition, const char* filename, int line, bool bBreak);

#endif // _NLDEBUG_H_
