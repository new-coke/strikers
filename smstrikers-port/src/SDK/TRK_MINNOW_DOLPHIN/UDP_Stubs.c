#include "PowerPC_EABI_Support/MetroTRK/trk.h"

/**
 * Offset/Address/Size: 0x40 | 0x8022A5BC | size: 0x8
 */
WEAKFUNC int udp_cc_initialize(void* flagOut, __OSInterruptHandler handler)
{
    return -1;
}

/**
 * Offset/Address/Size: 0x38 | 0x8022A5B4 | size: 0x8
 */
WEAKFUNC int udp_cc_shutdown(void)
{
    return -1;
}

/**
 * Offset/Address/Size: 0x30 | 0x8022A5AC | size: 0x8
 */
WEAKFUNC int udp_cc_open(void)
{
    return -1;
}

/**
 * Offset/Address/Size: 0x28 | 0x8022A5A4 | size: 0x8
 */
WEAKFUNC int udp_cc_close(void)
{
    return -1;
}

/**
 * Offset/Address/Size: 0x20 | 0x8022A59C | size: 0x8
 */
WEAKFUNC int udp_cc_read(u8* dest, int size)
{
    return 0;
}

/**
 * Offset/Address/Size: 0x18 | 0x8022A594 | size: 0x8
 */
WEAKFUNC int udp_cc_write(const u8* src, int size)
{
    return 0;
}

/**
 * Offset/Address/Size: 0x10 | 0x8022A58C | size: 0x8
 */
WEAKFUNC int udp_cc_peek(void)
{
    return 0;
}

/**
 * Offset/Address/Size: 0x8 | 0x8022A584 | size: 0x8
 */
WEAKFUNC int udp_cc_pre_continue(void)
{
    return -1;
}

/**
 * Offset/Address/Size: 0x0 | 0x8022A57C | size: 0x8
 */
WEAKFUNC int udp_cc_post_stop(void)
{
    return -1;
}
