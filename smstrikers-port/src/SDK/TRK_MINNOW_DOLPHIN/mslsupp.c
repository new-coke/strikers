#include "PowerPC_EABI_Support/MetroTRK/trk.h"

/**
 * Offset/Address/Size: 0x584 | 0x8022A4C0 | size: 0xBC
 */
DSIOResult __read_console(__file_handle file, unsigned char* buffer, size_t* count, __idle_proc idle_fn)
{
    u32 r0;
    size_t countTemp;

    if (GetUseSerialIO() == 0)
        return DS_IOError;

    if (GetTRKConnected() == DS_NoError)
        return DS_IOError;

    countTemp = *count;
    r0 = TRKAccessFile(DSMSG_ReadFile, 0, &countTemp, buffer);
    *count = countTemp;

    switch ((u8)r0)
    {
    case DS_IONoError:
        return DS_IONoError;
    case DS_IOEOF:
        return DS_IOEOF;
    }

    return DS_IOError;
}

/**
 * Offset/Address/Size: 0x4C8 | 0x8022A404 | size: 0xBC
 */
DSIOResult __TRK_write_console(__file_handle file, unsigned char* buffer, size_t* count, __idle_proc idle_fn)
{
    u32 r0;
    size_t countTemp;

    if (GetUseSerialIO() == 0)
        return DS_IOError;

    if (GetTRKConnected() == DS_NoError)
        return DS_IOError;

    countTemp = *count;
    r0 = TRKAccessFile(DSMSG_WriteFile, 1, &countTemp, buffer);
    *count = countTemp;

    switch ((u8)r0)
    {
    case DS_IONoError:
        return DS_IONoError;
    case DS_IOEOF:
        return DS_IOEOF;
    }

    return DS_IOError;
}

/**
 * Offset/Address/Size: 0x444 | 0x8022A380 | size: 0x84
 */
DSIOResult __close_console(__file_handle file)
{
    u32 r0;

    if (GetTRKConnected() == 0)
    {
        return DS_IOError;
    }

    r0 = TRKCloseFile(DSMSG_CloseFile, file);

    switch ((u8)r0)
    {
    case DS_IONoError:
        return DS_IONoError;
    case DS_IOEOF:
        return DS_IOEOF;
    }

    return DS_IOError;
}

/**
 * Offset/Address/Size: 0x390 | 0x8022A2CC | size: 0xB4
 */
DSIOResult __read_file(__file_handle file, unsigned char* buf, size_t* count, __idle_proc idle_fn)
{
    u32 r0;
    size_t countTemp;

    if (GetTRKConnected() == 0)
    {
        return DS_IOError;
    }

    countTemp = *count;
    r0 = TRKAccessFile(DSMSG_ReadFile, file, &countTemp, buf);
    *count = countTemp;

    switch ((u8)r0)
    {
    case DS_IONoError:
        return DS_IONoError;
    case DS_IOEOF:
        return DS_IOEOF;
    }

    return DS_IOError;
}

/**
 * Offset/Address/Size: 0x2DC | 0x8022A218 | size: 0xB4
 */
DSIOResult __write_file(__file_handle file, unsigned char* buf, size_t* count, __idle_proc idle_fn)
{
    u32 r0;
    size_t countTemp;

    if (GetTRKConnected() == 0)
    {
        return DS_IOError;
    }

    countTemp = *count;
    r0 = TRKAccessFile(DSMSG_WriteFile, file, &countTemp, buf);
    *count = countTemp;

    switch ((u8)r0)
    {
    case DS_IONoError:
        return DS_IONoError;
    case DS_IOEOF:
        return DS_IOEOF;
    }

    return DS_IOError;
}

/**
 * Offset/Address/Size: 0x160 | 0x8022A09C | size: 0x17C
 */
DSIOResult __open_file(const char* name, file_modes mode, __file_handle* handle)
{
    u32 r0;
    u8 openMode;
    u8 ioMode;
    u8 binaryIO;
    u8 trk_mode;

    if (GetTRKConnected() == 0)
    {
        return DS_IOError;
    }

    trk_mode = 0;
    openMode = mode.open_mode;
    ioMode = mode.io_mode;
    binaryIO = mode.binary_io;

    switch (openMode)
    {
    case 0:
        trk_mode |= 0x01;
        break;
    case 2:
        trk_mode |= 0x02;
        break;
    case 1:
        trk_mode |= 0x04;
        break;
    }

    switch (ioMode)
    {
    case 1:
        trk_mode |= 0x01;
        break;
    case 2:
        trk_mode |= 0x02;
        break;
    case 6:
        trk_mode |= 0x04;
        break;
    case 3:
        trk_mode |= 0x12;
        break;
    case 7:
        trk_mode |= 0x07;
        break;
    }

    if (binaryIO == 1)
    {
        trk_mode |= 0x08;
    }

    r0 = TRKOpenFile(DSMSG_OpenFile, (u32)name, (u8)trk_mode, handle);

    switch ((u8)r0)
    {
    case DS_IONoError:
        return DS_IONoError;
    case DS_IOEOF:
        return DS_IOEOF;
    }

    return DS_IOError;
}

/**
 * Offset/Address/Size: 0xDC | 0x8022A018 | size: 0x84
 */
DSIOResult __close_file(__file_handle file)
{
    u32 r0;

    if (GetTRKConnected() == 0)
    {
        return DS_IOError;
    }

    r0 = TRKCloseFile(DSMSG_CloseFile, file);

    switch ((u8)r0)
    {
    case DS_IONoError:
        return DS_IONoError;
    case DS_IOEOF:
        return DS_IOEOF;
    }

    return DS_IOError;
}

/**
 * Offset/Address/Size: 0x0 | 0x80229F3C | size: 0xDC
 */
DSIOResult __position_file(__file_handle handle, fpos_t* position, int mode, __idle_proc idle_proc)
{
    u32 r0;
    u32 modeConverted;

    modeConverted = 0;

    if (GetTRKConnected() == 0)
    {
        return DS_IOError;
    }

    if (mode == 0)
    {
        modeConverted = 0;
    }
    else if (mode == 1)
    {
        modeConverted = 1;
    }
    else if (mode == 2)
    {
        modeConverted = 2;
    }

    r0 = TRKPositionFile(DSMSG_PositionFile, handle, position, (u8)modeConverted);

    switch ((u8)r0)
    {
    case DS_IONoError:
        return DS_IONoError;
    case DS_IOEOF:
        return DS_IOEOF;
    }

    return DS_IOError;
}
