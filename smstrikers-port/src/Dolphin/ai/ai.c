#include <dolphin/ai.h>
#include <dolphin/gx.h>
#include <dolphin/os.h>
#include <dolphin/hw_regs.h>

#include "__gx.h"

#ifdef DEBUG
const char* __AIVersion = "<< Dolphin SDK - AI\tdebug build: Apr  5 2004 03:56:18 (0x2301) >>";
#elif defined(VERSION_G4QP01)
const char* __AIVersion = "<< Dolphin SDK - AI\trelease build: Apr 17 2003 12:33:54 (0x2301) >>";
#else
const char* __AIVersion = "<< Dolphin SDK - AI\trelease build: Apr  5 2004 04:15:02 (0x2301) >>";
#endif

static AISCallback __AIS_Callback;
static AIDCallback __AID_Callback;
static u8* __CallbackStack;
static u8* __OldStack;
static BOOL __AI_init_flag;
static BOOL __AID_Active;
static OSTime bound_32KHz;
static OSTime bound_48KHz;
static OSTime min_wait;
static OSTime max_wait;
static OSTime buffer;

typedef struct
{
    OSTime t_start;
    OSTime t1;
    OSTime t2;
    OSTime t3;
    OSTime t4;
    OSTime t_end;
} STRUCT_TIMELOG;
STRUCT_TIMELOG profile;

OSTime __ai_src_time_start;
OSTime __ai_src_time_end;

// prototypes
void __AI_DEBUG_set_stream_sample_rate(u32 rate);
STRUCT_TIMELOG* __ai_src_get_time(void);

static void __AI_set_stream_sample_rate(u32 rate);
static void __AIDHandler(__OSInterrupt interrupt, OSContext* context);
static void __AISHandler(__OSInterrupt interrupt, OSContext* context);
static void __AICallbackStackSwitch(void* cb);
static void __AI_SRC_INIT(void);

/**
 * Offset/Address/Size: 0x0 | 0x8023AEE8 | size: 0x44
 */
AIDCallback AIRegisterDMACallback(AIDCallback callback)
{
    AIDCallback old_callback;
    BOOL old;

    old_callback = __AID_Callback;
    old = OSDisableInterrupts();
    __AID_Callback = callback;
    OSRestoreInterrupts(old);
    return old_callback;
}

/**
 * Offset/Address/Size: 0x44 | 0x8023AF2C | size: 0x88
 */
void AIInitDMA(u32 start_addr, u32 length)
{
    BOOL old;

    old = OSDisableInterrupts();
    __DSPRegs[24] = (__DSPRegs[24] & 0xFFFFFC00) | (start_addr >> 16);
    __DSPRegs[25] = (__DSPRegs[25] & 0xFFFF001F) | (start_addr & 0xFFFF);
    ASSERTMSGLINE(316, (length & 0x1F) == 0, "AIStartDMA: length must be multiple of 32 bytes");
    __DSPRegs[27] = (__DSPRegs[27] & 0xFFFF8000) | ((length >> 5) & 0xFFFF);
    OSRestoreInterrupts(old);
}

BOOL AIGetDMAEnableFlag(void)
{
    return (__DSPRegs[27] & (1 << 15)) >> 15;
}

/**
 * Offset/Address/Size: 0xCC | 0x8023AFB4 | size: 0x18
 */
void AIStartDMA(void)
{
    __DSPRegs[27] = __DSPRegs[27] | 0x8000;
}

/**
 * Offset/Address/Size: 0xE4 | 0x8023AFCC | size: 0x18
 */
void AIStopDMA(void)
{
    __DSPRegs[27] = __DSPRegs[27] & ~0x8000;
}

u32 AIGetDMABytesLeft(void)
{
    return (__DSPRegs[29] & 0x7FFF) << 5;
}

/**
 * Offset/Address/Size: 0xFC | 0x8023AFE4 | size: 0x1C
 */
u32 AIGetDMAStartAddr(void)
{
    return ((__DSPRegs[24] << 16) & 0x03FF0000) | (__DSPRegs[25] & 0xFFE0);
}

u32 AIGetDMALength(void)
{
    return (__DSPRegs[27] & 0x7FFF) << 5;
}

BOOL AICheckInit(void)
{
    return __AI_init_flag;
}

AISCallback AIRegisterStreamCallback(AISCallback callback)
{
    AISCallback old_callback;
    BOOL old;

    old_callback = __AIS_Callback;
    old = OSDisableInterrupts();
    __AIS_Callback = callback;
    OSRestoreInterrupts(old);
    return old_callback;
}

u32 AIGetStreamSampleCount(void)
{
    return __AIRegs[2];
}

void AIResetStreamSampleCount(void)
{
    __AIRegs[0] = (__AIRegs[0] & ~0x20) | 0x20;
}

void AISetStreamTrigger(u32 trigger)
{
    __AIRegs[3] = trigger;
}

u32 AIGetStreamTrigger(void)
{
    return __AIRegs[3];
}

/**
 * Offset/Address/Size: 0x118 | 0x8023B000 | size: 0xD8
 */
void AISetStreamPlayState(u32 state)
{
    BOOL old;
    u8 vol_left;
    u8 vol_right;

    if (state != AIGetStreamPlayState())
    {
        if (AIGetStreamSampleRate() == 0 && state == AI_STREAM_START)
        {
            vol_left = AIGetStreamVolRight();
            vol_right = AIGetStreamVolLeft();
            AISetStreamVolRight(0);
            AISetStreamVolLeft(0);
            old = OSDisableInterrupts();
            __AI_SRC_INIT();
            OLD_SET_REG_FIELD(0, __AIRegs[0], 1, 5, 1);
            OLD_SET_REG_FIELD(0, __AIRegs[0], 1, 0, AI_STREAM_START);
            OSRestoreInterrupts(old);
            AISetStreamVolLeft(vol_left);
            AISetStreamVolRight(vol_right);
            return;
        }
        OLD_SET_REG_FIELD(653, __AIRegs[0], 1, 0, state);
    }
}

/**
 * Offset/Address/Size: 0x1F0 | 0x8023B0D8 | size: 0x10
 */
u32 AIGetStreamPlayState(void)
{
    return __AIRegs[0] & 1;
}

/**
 * Offset/Address/Size: 0x200 | 0x8023B0E8 | size: 0xE0
 */
void AISetDSPSampleRate(u32 rate)
{
    BOOL old;
    u32 play_state;
    u32 afr_state;
    u8 vol_left;
    u8 vol_right;

    if (rate != AIGetDSPSampleRate())
    {
        __AIRegs[0] = (__AIRegs[0] & 0xFFFFFFBF);
        if (rate == AI_SAMPLERATE_32KHZ)
        {
            vol_left = AIGetStreamVolLeft();
            vol_right = AIGetStreamVolRight();
            play_state = AIGetStreamPlayState();
            afr_state = AIGetStreamSampleRate();
            AISetStreamVolLeft(0U);
            AISetStreamVolRight(0U);
            old = OSDisableInterrupts();
            __AI_SRC_INIT();
            OLD_SET_REG_FIELD(0, __AIRegs[0], 1, 5, 1);
            OLD_SET_REG_FIELD(743, __AIRegs[0], 1, 1, afr_state);
            OLD_SET_REG_FIELD(744, __AIRegs[0], 1, 0, play_state);
            __AIRegs[0] |= 0x40;
            OSRestoreInterrupts(old);
            AISetStreamVolLeft(vol_left);
            AISetStreamVolRight(vol_right);
        }
    }
}

/**
 * Offset/Address/Size: 0x2E0 | 0x8023B1C8 | size: 0x14
 */
u32 AIGetDSPSampleRate(void)
{
    return GET_REG_FIELD(__AIRegs[0], 1, 6) ^ 1;
}

void AISetStreamSampleRate(u32 rate)
{
    if (rate == AI_SAMPLERATE_48KHZ)
    {
        __AI_set_stream_sample_rate(rate);
        return;
    }
#if DEBUG
    OSReport("AISetStreamSampleRate(): OBSOLETED. Only 48KHz streaming from disk is supported!\n");
#endif
}

void __AI_DEBUG_set_stream_sample_rate(u32 rate)
{
    __AI_set_stream_sample_rate(rate);
}

/**
 * Offset/Address/Size: 0x2F4 | 0x8023B1DC | size: 0xD4
 */
static void __AI_set_stream_sample_rate(u32 rate)
{
    BOOL old;
    u32 play_state;
    u8 vol_left;
    u8 vol_right;
    u32 dsp_src_state;

    if (rate != AIGetStreamSampleRate())
    {
        play_state = AIGetStreamPlayState();
        vol_left = AIGetStreamVolLeft();
        vol_right = AIGetStreamVolRight();
        AISetStreamVolRight(0);
        AISetStreamVolLeft(0);
        dsp_src_state = __AIRegs[0] & 0x40;
        OLD_SET_REG_FIELD(0, __AIRegs[0], 1, 6, 0);
        old = OSDisableInterrupts();
        __AI_SRC_INIT();
        __AIRegs[0] |= dsp_src_state;
        OLD_SET_REG_FIELD(0, __AIRegs[0], 1, 5, 1);
        OLD_SET_REG_FIELD(887, __AIRegs[0], 1, 1, rate);
        OSRestoreInterrupts(old);
        AISetStreamPlayState(play_state);
        AISetStreamVolLeft(vol_left);
        AISetStreamVolRight(vol_right);
    }
}

/**
 * Offset/Address/Size: 0x3C8 | 0x8023B2B0 | size: 0x10
 */
u32 AIGetStreamSampleRate(void)
{
    return GET_REG_FIELD(__AIRegs[0], 1, 1);
}

/**
 * Offset/Address/Size: 0x3D8 | 0x8023B2C0 | size: 0x1C
 */
void AISetStreamVolLeft(u8 vol)
{
    OLD_SET_REG_FIELD(945, __AIRegs[1], 8, 0, vol);
}

/**
 * Offset/Address/Size: 0x3F4 | 0x8023B2DC | size: 0x10
 */
u8 AIGetStreamVolLeft(void)
{
    return GET_REG_FIELD(__AIRegs[1], 8, 0);
}

/**
 * Offset/Address/Size: 0x404 | 0x8023B2EC | size: 0x1C
 */
void AISetStreamVolRight(u8 vol)
{
    OLD_SET_REG_FIELD(986, __AIRegs[1], 8, 8, vol);
}

/**
 * Offset/Address/Size: 0x420 | 0x8023B308 | size: 0x10
 */
u8 AIGetStreamVolRight(void)
{
    return (__AIRegs[1] & (0xFF << 8)) >> 8;
}

/**
 * Offset/Address/Size: 0x430 | 0x8023B318 | size: 0x16C
 */
void AIInit(u8* stack)
{
    if (__AI_init_flag != TRUE)
    {
        OSRegisterVersion(__AIVersion);

        bound_32KHz = OSNanosecondsToTicks(31524);
        bound_48KHz = OSNanosecondsToTicks(42024);
        min_wait = OSNanosecondsToTicks(42000);
        max_wait = OSNanosecondsToTicks(63000);
        buffer = OSNanosecondsToTicks(3000);
        AISetStreamVolRight(0);
        AISetStreamVolLeft(0);
        AISetStreamTrigger(0);
        AIResetStreamSampleCount();
        __AI_set_stream_sample_rate(AI_SAMPLERATE_48KHZ);
        AISetDSPSampleRate(AI_SAMPLERATE_32KHZ);
#if DEBUG
        OSReport("AIInit(): DSP is 32KHz\n");
#endif
        __AIS_Callback = NULL;
        __AID_Callback = NULL;
        __CallbackStack = stack;
        if (stack)
        {
            ASSERTMSGLINE(1107, ((u32)stack & 7) == 0, "AIInit: stack must be 8-byte aligned");
        }
        __OSSetInterruptHandler(5, __AIDHandler);
        __OSUnmaskInterrupts(0x04000000);
        __OSSetInterruptHandler(8, __AISHandler);
        __OSUnmaskInterrupts(0x800000);
        __AI_init_flag = TRUE;
    }
}

/**
 * Offset/Address/Size: 0x59C | 0x8023B484 | size: 0xC
 */
void AIReset(void)
{
    __AI_init_flag = FALSE;
}

/**
 * Offset/Address/Size: 0x5A8 | 0x8023B490 | size: 0x7C
 */
static void __AISHandler(__OSInterrupt interrupt, OSContext* context)
{
    OSContext exceptionContext;

    __AIRegs[0] |= 8;
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    if (__AIS_Callback)
    {
        __AIS_Callback(__AIRegs[2]);
    }
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

/**
 * Offset/Address/Size: 0x624 | 0x8023B50C | size: 0xAC
 */
static void __AIDHandler(__OSInterrupt interrupt, OSContext* context)
{
    OSContext exceptionContext;
    u16 tmp;

    tmp = __DSPRegs[5];
    tmp = (tmp & ~0xA0) | 8;
    __DSPRegs[5] = tmp;
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    if (__AID_Callback && !__AID_Active)
    {
        __AID_Active = TRUE;
        if (__CallbackStack)
        {
            __AICallbackStackSwitch(__AID_Callback);
        }
        else
        {
            __AID_Callback();
        }
        __AID_Active = FALSE;
    }
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

// clang-format off
/**
 * Offset/Address/Size: 0x6D0 | 0x8023B5B8 | size: 0x58
 */
static asm void __AICallbackStackSwitch(register void* cb) {
    nofralloc
    mflr r0
    stw r0, 0x4(r1)
    stwu r1, -0x18(r1)
    stw r31, 0x14(r1)
    mr r31, r3
    lis r5, __OldStack@ha
    addi r5, r5, __OldStack@l
    stw r1, 0x0(r5)
    lis r5, __CallbackStack@ha
    addi r5, r5, __CallbackStack@l
    lwz r1, 0x0(r5)
    subi r1, r1, 0x8
    mtlr r31
    blrl
    lis r5, __OldStack@ha
    addi r5, r5, __OldStack@l
    lwz r1, 0x0(r5)
    lwz r0, 0x1c(r1)
    lwz r31, 0x14(r1)
    addi r1, r1, 0x18
    mtlr r0
    blr
}
// clang-format on

/**
 * Offset/Address/Size: 0x728 | 0x8023B610 | size: 0x1E4
 */
void __AI_SRC_INIT(void)
{
    OSTime rising_32khz = 0;
    OSTime rising_48khz = 0;
    OSTime diff = 0;
    OSTime t1 = 0;
    OSTime temp;
    u32 temp0;
    u32 temp1;
    u32 done = 0;
    u32 volume = 0;
    u32 Init_Cnt = 0;
    u32 walking = 0;

    walking = 0;
    Init_Cnt = 0;
    temp = 0;

#if DEBUG
    profile.t_start = OSGetTime();
#endif

    while (!done)
    {
        OLD_SET_REG_FIELD(0, __AIRegs[0], 1, 5, 1);
        OLD_SET_REG_FIELD(0, __AIRegs[0], 1, 1, 0);
        OLD_SET_REG_FIELD(0, __AIRegs[0], 1, 0, AI_STREAM_START);
        temp0 = __AIRegs[2];
        while (temp0 == __AIRegs[2])
        {
        }
        rising_32khz = OSGetTime();
        OLD_SET_REG_FIELD(0, __AIRegs[0], 1, 1, 1);
        OLD_SET_REG_FIELD(0, __AIRegs[0], 1, 0, AI_STREAM_START);
        temp1 = __AIRegs[2];
        while (temp1 == __AIRegs[2])
        {
        }
        rising_48khz = OSGetTime();
        diff = rising_48khz - rising_32khz;
        OLD_SET_REG_FIELD(0, __AIRegs[0], 1, 1, 0);
        OLD_SET_REG_FIELD(0, __AIRegs[0], 1, 0, AI_STREAM_STOP);
        if (diff < bound_32KHz - buffer)
        {
            temp = min_wait;
            done = 1;
            Init_Cnt++;
        }
        else if (diff >= bound_32KHz + buffer && diff < bound_48KHz - buffer)
        {
            temp = max_wait;
            done = 1;
            Init_Cnt++;
        }
        else
        {
            done = 0;
            walking = 1;
            Init_Cnt++;
        }
    }
    while (rising_48khz + temp > OSGetTime())
    {
    }
#if DEBUG
    profile.t_end = OSGetTime();
#endif
}

STRUCT_TIMELOG* __ai_src_get_time(void)
{
#if DEBUG
    return &profile;
#else
    return NULL;
#endif
}
