#include "__dsp.h"

/**
 * Offset/Address/Size: 0x0 | 0x80244484 | size: 0x50
 */
void __DSP_debug_printf(const char* fmt, ...)
{
}

DSPTaskInfo* __DSPGetCurrentTask(void)
{
    return __DSP_curr_task;
}
