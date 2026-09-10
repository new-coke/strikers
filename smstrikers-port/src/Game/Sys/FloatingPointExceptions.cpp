// PORT: this file reads PowerPC registers out of OSContext, which do not exist on a host.
#if !defined(PORT_USE_AURORA)

#include "Game/Sys/FloatingPointExceptions.h"

#include "dolphin/os.h"
#include "dolphin/os/OSError.h"

enum
{
    BIT_2 = 0x20000000, // Invalid operation summary
    BIT_3 = 0x10000000, // Overflow
    BIT_4 = 0x08000000, // Underflow
    BIT_5 = 0x04000000, // Zero division
    BIT_6 = 0x02000000, // Inexact

    BIT_7 = 0x01000000,  // SNaN
    BIT_8 = 0x00800000,  // Infinity - Infinity
    BIT_9 = 0x00400000,  // Infinity / Infinity
    BIT_10 = 0x00200000, // 0 / 0
    BIT_11 = 0x00100000, // Infinity * 0
    BIT_12 = 0x00080000, // Invalid compare

    BIT_21 = 0x00000400, // Software request
    BIT_22 = 0x00000200, // Invalid square root
    BIT_23 = 0x00000100  // Invalid integer convert
};

/**
 * Offset/Address/Size: 0x38 | 0x801ACCCC | size: 0x198
 */
static void FloatingPointErrorHandler(u16 /*error*/, OSContext* ctx, u32 /*dsisr*/, u32 /*dar*/)
{
    u32 fpscr = ctx->fpscr;
    u32 mask = ((fpscr << 0x16) & 0x3E000000) | 0x01F80000 | 0x700;
    fpscr &= mask;

    if (fpscr & BIT_2)
    {
        OSReport("FPE: Invalid operation: ");
        if (fpscr & BIT_7)
            OSReport("SNaN\n");
        if (fpscr & BIT_8)
            OSReport("Infinity - Infinity\n");
        if (fpscr & BIT_9)
            OSReport("Infinity / Infinity\n");
        if (fpscr & BIT_10)
            OSReport("0 / 0\n");
        if (fpscr & BIT_11)
            OSReport("Infinity * 0\n");
        if (fpscr & BIT_12)
            OSReport("Invalid compare\n");
        if (fpscr & BIT_21)
            OSReport("Software request\n");
        if (fpscr & BIT_22)
            OSReport("Invalid square root\n");
        if (fpscr & BIT_23)
            OSReport("Invalid integer convert\n");
    }

    // Other FP exceptions
    if (fpscr & BIT_3)
        OSReport("FPE: Overflow\n");
    if (fpscr & BIT_4)
        OSReport("FPE: Underflow\n");
    if (fpscr & BIT_5)
        OSReport("FPE: Zero division\n");
    if (fpscr & BIT_6)
        OSReport("FPE: Inexact result\n");

    ctx->srr0 += 4;
}

/**
 * Offset/Address/Size: 0x0 | 0x801ACC94 | size: 0x38
 */
void InstallFloatingPointExceptionHandler()
{
    __OSFpscrEnableBits = 0x31F80210;
    OSSetErrorHandler((unsigned short)0x10, (OSErrorHandler)FloatingPointErrorHandler);
}

#endif // !PORT_USE_AURORA
