#include "Game/Sys/eventman.h"

/**
 * Offset/Address/Size: 0x0 | 0x801A42B0 | size: 0x118
 */
void EventManager::SetupDestArray()
{
    AllocateDestArray(0x6A, (unsigned long)-1);

    u32* d = m_dest;
    unsigned long one = 1;    // r4
    unsigned long v23 = 0x17; // r3
    unsigned long v22 = 0x16; // r0

    // Offsets / indices are written in the exact order from the asm.
    d[39] = one; // +0x9C
    d[43] = one; // +0xAC
    d[37] = one; // +0x94
    d[38] = one; // +0x98
    d[41] = one; // +0xA4
    d[42] = one; // +0xA8

    d[31] = v23; // +0x7C
    d[32] = v23; // +0x80
    d[45] = v23; // +0xB4
    d[33] = v23; // +0x84
    d[36] = v23; // +0x90
    d[34] = v23; // +0x88

    d[35] = v22; // +0x8C

    d[44] = one; // +0xB0
    d[46] = v23; // +0xB8

    d[60] = one; // +0xF0
    d[61] = one; // +0xF4
    d[62] = one; // +0xF8

    d[13] = one; // +0x34
    d[14] = one; // +0x38
    d[15] = v23; // +0x3C
    d[16] = v22; // +0x40
    d[17] = v22; // +0x44
    d[18] = v22; // +0x48
    d[19] = v22; // +0x4C
    d[20] = one; // +0x50

    d[27] = v22; // +0x6C

    d[63] = one; // +0xFC
    d[64] = one; // +0x100
    d[65] = one; // +0x104
    d[66] = v22; // +0x108

    d[47] = one; // +0xBC
    d[48] = one; // +0xC0
    d[51] = one; // +0xCC

    // Tail block: 0x15C..0x1A4 (indices 87..105) all = 0x16
    d[87] = v22;  // +0x15C
    d[88] = v22;  // +0x160
    d[89] = v22;  // +0x164
    d[90] = v22;  // +0x168
    d[91] = v22;  // +0x16C
    d[92] = v22;  // +0x170
    d[93] = v22;  // +0x174
    d[94] = v22;  // +0x178
    d[95] = v22;  // +0x17C
    d[96] = v22;  // +0x180
    d[97] = v22;  // +0x184
    d[98] = v22;  // +0x188
    d[99] = v22;  // +0x18C
    d[100] = v22; // +0x190
    d[101] = v22; // +0x194
    d[102] = v22; // +0x198
    d[103] = v22; // +0x19C
    d[104] = v22; // +0x1A0
    d[105] = v22; // +0x1A4
}
