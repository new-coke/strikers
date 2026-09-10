#include "types.h"
#include "TRK_MINNOW_DOLPHIN/Os/dolphin/usr_put.h"

extern void OSReport(char* fmt);

/**
 * Offset/Address/Size: 0x4 | 0x802258B4 | size: 0x88
 */
BOOL usr_puts_serial(const char* msg)
{
    BOOL connect_ = FALSE;
    char c;
    char buf[2];

    while (!connect_ && (c = *msg++) != '\0')
    {
        BOOL connect = GetTRKConnected();

        buf[0] = c;
        buf[1] = '\0';

        SetTRKConnected(FALSE);
        OSReport(buf);

        SetTRKConnected(connect);
        connect_ = FALSE;
    }

    return connect_;
}

/**
 * Offset/Address/Size: 0x0 | 0x802258B0 | size: 0x4
 */
void usr_put_initialize(void)
{
}
