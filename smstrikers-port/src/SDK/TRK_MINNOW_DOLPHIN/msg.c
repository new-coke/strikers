#include "PowerPC_EABI_Support/MetroTRK/trk.h"

/**
 * Offset/Address/Size: 0x0 | 0x80224D78 | size: 0x44
 */
DSError TRKMessageSend(TRKBuffer* msg)
{
    DSError writeErr = TRKWriteUARTN(&msg->data, msg->length);
    MWTRACE(1, "MessageSend : cc_write returned %ld\n", writeErr);
    return DS_NoError;
}
