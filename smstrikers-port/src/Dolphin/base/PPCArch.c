#include <dolphin.h>
#include <dolphin/PPCArch.h>

// clang-format off

union FpscrUnion 
{
	f64 f;
	struct 
    {
		u32 fpscr_pad;
		u32 fpscr;
	} u;
};

/**
 * Offset/Address/Size: 0x0 | 0x8023D7BC | size: 0x8
 */
asm u32 PPCMfmsr() 
{
    nofralloc
    mfmsr r3
    blr    
}

/**
 * Offset/Address/Size: 0x8 | 0x8023D7C4 | size: 0x8
 */
asm void PPCMtmsr(register u32 newMSR) 
{
    nofralloc
    mtmsr newMSR
    blr
}

asm u32 PPCOrMsr(register u32 value) 
{
    nofralloc
    mfmsr r4
    or value, r4, value
    blr
}

asm u32 PPCAndMsr(register u32 value) 
{
    nofralloc
    mfmsr r4
    and value, r4, value
    blr
}

asm u32 PPCAndCMsr(register u32 value) 
{
    nofralloc
    mfmsr r4
    andc value, r4, value
    blr
}

/**
 * Offset/Address/Size: 0x10 | 0x8023D7CC | size: 0x8
 */
asm u32 PPCMfhid0() 
{
    nofralloc
    mfspr r3, HID0
    blr
}

/**
 * Offset/Address/Size: 0x18 | 0x8023D7D4 | size: 0x8
 */
asm void PPCMthid0(register u32 newHID0)
{
    nofralloc
    mtspr HID0, newHID0
    blr
}

asm u32 PPCMfhid1() 
{
    nofralloc
    mfspr r3, HID1
    blr
}

/**
 * Offset/Address/Size: 0x20 | 0x8023D7DC | size: 0x8
 */
asm u32 PPCMfl2cr() 
{
    nofralloc
    mfspr r3, L2CR
    blr
}

/**
 * Offset/Address/Size: 0x28 | 0x8023D7E4 | size: 0x8
 */
asm void PPCMtl2cr(register u32 newL2cr) 
{
    nofralloc
    mtspr L2CR, newL2cr
    blr
}

/**
 * Offset/Address/Size: 0x30 | 0x8023D7EC | size: 0x8
 */
asm void PPCMtdec(register u32 newDec) 
{
    nofralloc
    mtdec newDec
    blr
}

asm u32 PPCMfdec() 
{
    nofralloc
    mfdec r3
    blr
}

/**
 * Offset/Address/Size: 0x38 | 0x8023D7F4 | size: 0x8
 */
asm void PPCSync() 
{
    nofralloc
    sc
    blr
}

asm void PPCEieio() 
{
    nofralloc
    mfmsr r5
    rlwinm r6, r5, 0, 17, 15
    mtmsr r6
    mfspr r3, HID0
    ori r4, r3, 0x8
    mtspr HID0, r4
    isync
    eieio
    isync
    mtspr HID0, r3
    mtmsr r5
    isync
    blr
}

/**
 * Offset/Address/Size: 0x40 | 0x8023D7FC | size: 0x14
 */
asm void PPCHalt() 
{
    nofralloc
    sync
loop:
    nop
    li r3, 0
    nop
    b loop
}

asm u32 PPCMfmmcr0() 
{
    nofralloc
    mfspr r3, MMCR0
    blr
}

/**
 * Offset/Address/Size: 0x54 | 0x8023D810 | size: 0x8
 */
asm void PPCMtmmcr0(register u32 newMmcr0) 
{
    nofralloc
    mtspr MMCR0, newMmcr0
    blr
}

asm u32 PPCMfmmcr1() 
{
    nofralloc
    mfspr r3, MMCR1
    blr
}

/**
 * Offset/Address/Size: 0x5C | 0x8023D818 | size: 0x8
 */
asm void PPCMtmmcr1(register u32 newMmcr1) 
{
    nofralloc
    mtspr MMCR1, newMmcr1
    blr
}

asm u32 PPCMfpmc1() 
{
    nofralloc
    mfspr r3, PMC1
    blr
}

/**
 * Offset/Address/Size: 0x64 | 0x8023D820 | size: 0x8
 */
asm void PPCMtpmc1(register u32 newPmc1) 
{
    nofralloc
    mtspr PMC1, newPmc1
    blr
}

asm u32 PPCMfpmc2() 
{
    nofralloc
    mfspr r3, PMC2
    blr
}

/**
 * Offset/Address/Size: 0x6C | 0x8023D828 | size: 0x8
 */
asm void PPCMtpmc2(register u32 newPmc2) 
{
    nofralloc
    mtspr PMC2, newPmc2
    blr
}

asm u32 PPCMfpmc3() 
{
    nofralloc
    mfspr r3, PMC3
    blr
}

/**
 * Offset/Address/Size: 0x74 | 0x8023D830 | size: 0x8
 */
asm void PPCMtpmc3(register u32 newPmc3) 
{
    nofralloc
    mtspr PMC3, newPmc3
    blr
}

asm u32 PPCMfpmc4() 
{
    nofralloc
    mfspr r3, PMC4
    blr
}

/**
 * Offset/Address/Size: 0x7C | 0x8023D838 | size: 0x8
 */
asm void PPCMtpmc4(register u32 newPmc4) 
{
    nofralloc
    mtspr PMC4, newPmc4
    blr
}

asm u32 PPCMfsia() {
    nofralloc
    mfspr r3, SIA
    blr
}

asm void PPCMtsia(register u32 newSia) 
{
    nofralloc
    mtspr SIA, newSia
    blr
}

/**
 * Offset/Address/Size: 0x84 | 0x8023D840 | size: 0x20
 */
u32 PPCMffpscr() 
{
    union FpscrUnion m;

    asm {
        mffs fp31
        stfd fp31, m.f;
    }

    return m.u.fpscr; 
}

/**
 * Offset/Address/Size: 0xA4 | 0x8023D860 | size: 0x28
 */
void PPCMtfpscr(register u32 newFPSCR) 
{
    union FpscrUnion m;

    asm {
        li    r4, 0
        stw   r4, m.u.fpscr_pad;
        stw   newFPSCR, m.u.fpscr
        lfd   fp31, m.f
        mtfsf 0xff, fp31
    }
}

/**
 * Offset/Address/Size: 0xCC | 0x8023D888 | size: 0x8
 */
asm u32 PPCMfhid2() 
{
    nofralloc
    mfspr r3, HID2
    blr
}

/**
 * Offset/Address/Size: 0xD4 | 0x8023D890 | size: 0x8
 */
asm void PPCMthid2(register u32 newhid2) 
{
    nofralloc
    mtspr HID2, newhid2
    blr
}

asm u32 PPCMfwpar() {
    nofralloc
    sync
    mfspr r3, WPAR
    blr
}

/**
 * Offset/Address/Size: 0xDC | 0x8023D898 | size: 0x8
 */
asm void PPCMtwpar(register u32 newwpar) 
{
    nofralloc
    mtspr WPAR, newwpar
    blr
}

asm u32 PPCMfdmaU() 
{
    nofralloc
    mfspr r3, DMA_U
    blr
}

asm u32 PPCMfdmaL() 
{
    nofralloc
    mfspr r3, DMA_L
    blr
}

asm void PPCMtdmaU(register u32 newdmau) 
{
    nofralloc
    mtspr DMA_U, newdmau
    blr
}

asm void PPCMtdmaL(register u32 newdmal) 
{
    nofralloc
    mtspr DMA_L, newdmal
    blr
}

asm u32 PPCMfpvr() 
{
    nofralloc
    mfspr r3, PVR
    blr
}

void PPCEnableSpeculation(void) 
{
    PPCMthid0(PPCMfhid0() & ~HID0_SPD);
}

/**
 * Offset/Address/Size: 0xE4 | 0x8023D8A0 | size: 0x28
 */
void PPCDisableSpeculation(void) 
{
    PPCMthid0(PPCMfhid0() | HID0_SPD);
}

asm void PPCSetFpIEEEMode() 
{

	nofralloc
	mtfsb0 29
	blr
}

/**
 * Offset/Address/Size: 0x10C | 0x8023D8C8 | size: 0x8
 */
asm void PPCSetFpNonIEEEMode() 
{
	nofralloc
	mtfsb1 29
	blr
}

// clang-format on
