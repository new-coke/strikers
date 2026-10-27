#if defined(PORT_VITA)
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#endif
#include "NL/nlDebug.h"

#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include "dolphin/os.h"

/**
 * Offset/Address/Size: 0x0 | 0x801CE948 | size: 0xC
 */
#if defined(PORT_VITA)
// PORT: logging is compiled out, so record why the game aborted.
static const char* s_breakReason = "";
void nlBreakReason(const char* reason) { s_breakReason = reason ? reason : ""; }
#endif
void nlBreak()
{
#if defined(PORT_VITA)
    if (FILE* f = fopen("ux0:data/strikersVita/nlbreak.log", "a"))
    {
        fprintf(f, "nlBreak caller=%p thread=%x reason=%s\n", __builtin_return_address(0),
                (unsigned)sceKernelGetThreadId(), s_breakReason);
        fclose(f);
    }
#endif
    // PORT: the original wrote to address 1 to raise a DSI exception and drop into the devkit debugger.
    fflush(NULL);
    OSReport("nlBreak: unrecoverable error, aborting\n");
    abort();
}
