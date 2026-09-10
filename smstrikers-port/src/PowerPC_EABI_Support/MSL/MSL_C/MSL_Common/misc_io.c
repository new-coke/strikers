#include "PowerPC_EABI_Support/MSL_C/MSL_Common/misc_io.h"
#include "PowerPC_EABI_Support/MSL_C/MSL_Common/abort_exit.h"

/**
 * Offset/Address/Size: 0x10 | 0x80231A48 | size: 0x10
 */
void clearerr(FILE* file)
{
    file->file_state.eof = 0;
    file->file_state.error = 0;
}

/**
 * Offset/Address/Size: 0x0 | 0x80231A38 | size: 0x10
 */
void __stdio_atexit(void)
{
    __stdio_exit = __close_all;
}
