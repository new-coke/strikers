#include "NL/nlDebug.h"

#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include "dolphin/os.h"

/**
 * Offset/Address/Size: 0x0 | 0x801CE948 | size: 0xC
 */
void nlBreak()
{
    // PORT: the original wrote to address 1 to raise a DSI exception and drop into the devkit debugger.
    fflush(NULL);
    OSReport("nlBreak: unrecoverable error, aborting\n");
    abort();
}
