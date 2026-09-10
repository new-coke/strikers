#if defined(VERSION_G4QP01)
#include <dolphin/os.h>
#include <dolphin/ai.h>

#include "__os.h"
#else
#include <dolphin.h>
#include <dolphin/os.h>

#include "__dvd.h"
#include "__os.h"
#endif

#if defined(VERSION_G4QP01)
#define OS_BOOTROM_ADDR 0x81300000

static void* SaveStart = NULL;
static void* SaveEnd = NULL;
static BOOL Prepared;

extern u32 BOOT_REGION_START AT_ADDRESS(0x812FDFF0);
extern u32 BOOT_REGION_END AT_ADDRESS(0x812FDFEC);
extern u32 OS_RESET_CODE AT_ADDRESS(0x800030F0);
extern u8 OS_REBOOT_BOOL AT_ADDRESS(0x800030E2);

extern void* __OSSavedRegionStart;
extern void* __OSSavedRegionEnd;

typedef struct ApploaderHeader {
    char date[16];
    u32 entry;
    u32 size;
    u32 rebootSize;
    u32 reserved2;
} ApploaderHeader;

static ApploaderHeader Header ATTRIBUTE_ALIGN(32);

BOOL IsStreamEnabled(void)
{
    if (DVDGetCurrentDiskID()->streaming)
    {
        return TRUE;
    }
    return FALSE;
}

ASM static void Run(register u32 addr)
{
#ifdef __MWERKS__ // clang-format off
    nofralloc

    sync
    isync
    mtlr addr
    blr
#endif // clang-format on
}

void ReadApploader(OSTime start)
{
    if (!DVDCheckDisk() || OSGetTime() - start > OS_TIMER_CLOCK)
    {
        __OSDoHotReset(0);
    }
}

static void Callback(void)
{
    Prepared = TRUE;
}

void __OSReboot(u32 resetCode, u32 bootDol)
{
    OSContext exceptionContext;
    OSTime start;
    DVDCommandBlock dvdCmd;
    DVDCommandBlock dvdCmd2;
    DVDCommandBlock dvdCmd3;
    u32 numBytes;
    u32 offset;

    OSDisableInterrupts();
    OS_RESET_CODE = resetCode;
    OS_REBOOT_BOOL = TRUE;
    BOOT_REGION_START = (u32)SaveStart;
    BOOT_REGION_END = (u32)SaveEnd;
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    DVDInit();
    DVDSetAutoInvalidation(TRUE);
    DVDResume();

    Prepared = FALSE;
    __DVDPrepareResetAsync(Callback);

    __OSMaskInterrupts(0xFFFFFFE0);
    __OSUnmaskInterrupts(0x400);
    OSEnableInterrupts();

    start = OSGetTime();
    while (Prepared != TRUE)
    {
        ReadApploader(start);
    }

    if (!__OSIsGcam)
    {
        if (IsStreamEnabled())
        {
            AISetStreamVolLeft(0);
            AISetStreamVolRight(0);
            DVDCancelStreamAsync(&dvdCmd, NULL);
            start = OSGetTime();
            while (DVDGetCommandBlockStatus(&dvdCmd))
            {
                ReadApploader(start);
            }

            AISetStreamPlayState(0);
        }
    }

    DVDReadAbsAsyncPrio(&dvdCmd2, Header.date, 32, 0x2440, NULL, 0);

    start = OSGetTime();
    while (DVDGetCommandBlockStatus(&dvdCmd2))
    {
        ReadApploader(start);
    }

    offset = Header.size + 0x20;
    numBytes = OSRoundUp32B(Header.rebootSize);
    DVDReadAbsAsyncPrio(&dvdCmd3, (void*)OS_BOOTROM_ADDR, numBytes, offset + 0x2440, NULL, 0);

    start = OSGetTime();
    while (DVDGetCommandBlockStatus(&dvdCmd3))
    {
        ReadApploader(start);
    }

    ICInvalidateRange((void*)OS_BOOTROM_ADDR, numBytes);
    OSDisableInterrupts();
    ICFlashInvalidate();
    Run(OS_BOOTROM_ADDR);
}

void OSSetSaveRegion(void* start, void* end)
{
    SaveStart = start;
    SaveEnd = end;
}

void OSGetSaveRegion(void** start, void** end)
{
    *start = SaveStart;
    *end = SaveEnd;
}

void OSGetSavedRegion(void** start, void** end)
{
    *start = __OSSavedRegionStart;
    *end = __OSSavedRegionEnd;
}
#else
static void* SaveStart;
static void* SaveEnd;

/**
 * Offset/Address/Size: 0x0 | 0x802576C4 | size: 0x70
 */
void __OSReboot(u32 resetCode, u32 bootDol)
{
    OSContext exceptionContext;
    char* argvToPass;

    OSDisableInterrupts();
    OSSetArenaLo((void*)0x81280000);
    OSSetArenaHi((void*)0x812f0000);
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    argvToPass = NULL;
    __OSBootDol(bootDol, resetCode | 0x80000000, (const char**)&argvToPass);
}

void OSSetSaveRegion(void* start, void* end)
{
    ASSERTMSGLINE(134, (u32)start >= 0x80700000 || start == NULL, "OSSetSaveRegion(): start address should be NULL or higher than 0x80700000\n");
    ASSERTMSGLINE(135, 0x81200000 >= (u32)end || end == NULL, "OSSetSaveRegion(): end address should be NULL or lower than 0x81200000\n");
    ASSERTMSGLINE(136, ((start == NULL) ^ (end == NULL)) == 0, "OSSetSaveRegion(): if either start or end is NULL, both should be NULL\n");

    SaveStart = start;
    SaveEnd = end;
}

/**
 * Offset/Address/Size: 0x70 | 0x80257734 | size: 0x14
 */
void OSGetSaveRegion(void** start, void** end)
{
    *start = SaveStart;
    *end = SaveEnd;
}

void OSGetSavedRegion(void** start, void** end)
{
    *start = __OSRebootParams.regionStart;
    *end = __OSRebootParams.regionEnd;
}
#endif
