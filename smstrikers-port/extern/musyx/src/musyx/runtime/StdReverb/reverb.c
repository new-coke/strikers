#include <math.h>
#include <string.h>

#include "musyx/musyx.h"

#include "musyx/sal.h"

static void DLsetdelay(_SND_REVHI_DELAYLINE* delayline, s32 len) {
  delayline->outPoint = delayline->inPoint - (len * sizeof(f32));
  while (delayline->outPoint < 0) {
    delayline->outPoint += delayline->length;
  }
}

static void DLcreate(_SND_REVHI_DELAYLINE* delayline, s32 length) {
  delayline->length = (s32)length * sizeof(f32);
  delayline->inputs = (f32*)salMalloc(length * sizeof(f32));
  memset(delayline->inputs, 0, length * sizeof(length));
  delayline->lastOutput = 0.f;
  DLsetdelay(delayline, length >> 1);
  delayline->inPoint = 0;
  delayline->outPoint = 0;
}

static void DLdelete(_SND_REVHI_DELAYLINE* delayline) { salFree(delayline->inputs); }
bool ReverbHICreate(_SND_REVHI_WORK* rev, f32 coloration, f32 time, f32 mix, f32 damping,
                    f32 preDelay, f32 crosstalk) {
  static int lens[] = {1789, 1999, 2333, 433, 149, 47, 73, 67};
  unsigned char i; // r31
  unsigned char k; // r29
  if (coloration < 0.f || coloration > 1.f || time < 0.01f || time > 10.f || mix < 0.f ||
      mix > 1.f || crosstalk < 0.f || crosstalk > 1.f || damping < 0.f || damping > 1.f ||
      preDelay < 0.f || preDelay > 0.1f) {
    return FALSE;
  }

  memset(rev, 0, sizeof(_SND_REVHI_WORK));

  for (k = 0; k < 3; ++k) {
    for (i = 0; i < 3; ++i) {
      DLcreate(&rev->C[i + k * 3], lens[i] + 2);
      DLsetdelay(&rev->C[i + k * 3], lens[i]);
      rev->combCoef[i + k * 3] = powf(10.f, (lens[i] * -3) / (32000.f * time));
    }

    for (i = 0; i < 2; ++i) {
      DLcreate(&rev->AP[i + k * 3], lens[i + 3] + 2);
      DLsetdelay(&rev->AP[i + k * 3], lens[i + 3]);
    }
    DLcreate(&rev->AP[k * 3 + 2], lens[k + 5] + 2);
    DLsetdelay(&rev->AP[k * 3 + 2], lens[k + 5]);
    rev->lpLastout[k] = 0.f;
  }

  rev->allPassCoeff = coloration;
  rev->level = mix;
  rev->crosstalk = crosstalk;
  rev->damping = damping;
  if (rev->damping < 0.05f) {
    rev->damping = 0.05f;
  }

  rev->damping = 1.f - (rev->damping * 0.8f + 0.05f);
  if (preDelay != 0.f) {
    rev->preDelayTime = preDelay * 32000.f;
    for (i = 0; i < 3; ++i) {
      rev->preDelayLine[i] = (f32*)salMalloc(rev->preDelayTime * sizeof(f32));
      memset(rev->preDelayLine[i], 0, rev->preDelayTime * sizeof(f32));
      rev->preDelayPtr[i] = rev->preDelayLine[i];
    }
  } else {
    rev->preDelayTime = 0;
    for (i = 0; i < 3; ++i) {
      rev->preDelayPtr[i] = NULL;
      rev->preDelayLine[i] = NULL;
    }
  }

  return TRUE;
}

bool ReverbHIModify(struct _SND_REVHI_WORK* rv, float coloration, float time, float mix,
                    float damping, float preDelay, float crosstalk) {
  u8 i; // r30

  if (coloration < 0.f || coloration > 1.f || time < 0.01f || time > 10.f || mix < 0.f ||
      mix > 1.f || crosstalk < 0.f || crosstalk > 1.f || damping < 0.f || damping > 1.f ||
      preDelay < 0.f || preDelay > 100.f) {
    return FALSE;
  }

  rv->allPassCoeff = coloration;
  rv->level = mix;
  rv->crosstalk = crosstalk;
  rv->damping = damping;
  if (rv->damping < 0.05f) {
    rv->damping = 0.05f;
  }

  rv->damping = 1.f - (rv->damping * 0.8f + 0.05f);

  for (i = 0; i < 9; ++i) {
    DLdelete(&rv->AP[i]);
  }

  for (i = 0; i < 9; ++i) {
    DLdelete(&rv->C[i]);
  }

  if (rv->preDelayTime != 0) {
    for (i = 0; i < 3; ++i) {
      salFree(rv->preDelayLine[i]);
    }
  }

  return ReverbHICreate(rv, coloration, time, mix, damping, preDelay, crosstalk);
}

#ifdef __MWERKS__
static const float value0_3 = 0.3f;
static const float value0_6 = 0.6f;
static const double i2fMagic = 4.503601774854144E15;

/* clang-format off */
/* TODO: Properly rework these to use input arguments and cleanup ASM */
static asm void DoCrossTalk(s32* a, s32* b, f32 start, f32 end) {
  nofralloc
  stwu r1, -0x30(r1)
  stfd f14, 0x28(r1)
  lis r5, i2fMagic@ha
  lfd f0, i2fMagic@l(r5)
  lis r5, 0x4330
  stw r5, 0x8(r1)
  stw r5, 0x10(r1)
  stw r5, 0x18(r1)
  stw r5, 0x20(r1)
  ps_merge00 f3, f2, f1
  ps_merge00 f4, f1, f2
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
  lis r5, value0_6@ha
  lfs f5, value0_6@l(r5)
#endif
  li r5, 0x4f
  mtctr r5
  li r10, -0x8
  li r11, -0x4
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
  ps_muls0 f4, f4, f5
#endif
  lwz r6, 0x0(r3)
  lwz r7, 0x0(r4)
  xoris r6, r6, 0x8000
  lwz r8, 0x4(r3)
  xoris r7, r7, 0x8000
  lwz r9, 0x4(r4)
  xoris r8, r8, 0x8000
  stw r6, 0xc(r1)
  xoris r9, r9, 0x8000
  stw r7, 0x14(r1)
  stw r8, 0x1c(r1)
  stw r9, 0x24(r1)
  lfd f5, 0x8(r1)
  lfd f6, 0x10(r1)
  fsubs f5, f5, f0
  lfd f7, 0x18(r1)
  fsubs f6, f6, f0
  lfd f8, 0x20(r1)
  fsubs f7, f7, f0
  fsubs f8, f8, f0
L_00000710:
  ps_merge00 f9, f5, f6
  lwzu r6, 0x8(r3)
  ps_merge00 f10, f7, f8
  lwzu r7, 0x8(r4)
  xoris r6, r6, 0x8000
  lwz r8, 0x4(r3)
  ps_mul f11, f9, f3
  xoris r7, r7, 0x8000
  ps_mul f12, f9, f4
  lwz r9, 0x4(r4)
  ps_mul f13, f10, f3
  xoris r8, r8, 0x8000
  ps_mul f14, f10, f4
  stw r6, 0xc(r1)
  ps_sum0 f11, f11, f11, f11
  xoris r9, r9, 0x8000
  ps_sum0 f12, f12, f12, f12
  stw r7, 0x14(r1)
  ps_sum0 f13, f13, f13, f13
  stw r8, 0x1c(r1)
  ps_sum0 f14, f14, f14, f14
  stw r9, 0x24(r1)
  fctiw f11, f11
  lfd f5, 0x8(r1)
  fctiw f12, f12
  lfd f6, 0x10(r1)
  fctiw f13, f13
  fsubs f5, f5, f0
  fctiw f14, f14
  lfd f7, 0x18(r1)
  stfiwx f11, r10, r3
  fsubs f6, f6, f0
  stfiwx f12, r10, r4
  lfd f8, 0x20(r1)
  stfiwx f13, r11, r3
  fsubs f7, f7, f0
  stfiwx f14, r11, r4
  fsubs f8, f8, f0
  bdnz L_00000710
  ps_merge00 f9, f5, f6
  addi r3, r3, 0x8
  ps_merge00 f10, f7, f8
  addi r4, r4, 0x8
  ps_mul f11, f9, f3
  ps_mul f12, f9, f4
  ps_mul f13, f10, f3
  ps_mul f14, f10, f4
  ps_sum0 f11, f11, f11, f11
  ps_sum0 f12, f12, f12, f12
  ps_sum0 f13, f13, f13, f13
  ps_sum0 f14, f14, f14, f14
  fctiw f11, f11
  fctiw f12, f12
  fctiw f13, f13
  fctiw f14, f14
  stfiwx f11, r10, r3
  stfiwx f12, r10, r4
  stfiwx f13, r11, r3
  stfiwx f14, r11, r4
  lfd f14, 0x28(r1)
  addi r1, r1, 0x30
  blr  
}
#else
/* clang-format on */
static void DoCrossTalk(s32* a, s32* b, f32 start, f32 end) {
  /* PORT: `end` is the direct term and `start` the crossed one. */
  s32 i;
  for (i = 0; i < 160; ++i) {
    f32 l = (f32)a[i];
    f32 r = (f32)b[i];
    a[i] = (s32)(end * l + start * r);
    b[i] = (s32)(start * l + end * r);
  }
}
#endif

#ifdef __MWERKS__
/* clang-format off */
static asm void HandleReverb(s32* sptr, SND_AUX_REVERBHI* rv, s32 k) {
  nofralloc
  stwu r1, -0xc0(r1)
  stmw r14, 0x8(r1)
  stfd f14, 0x60(r1)
  stfd f15, 0x68(r1)
  stfd f16, 0x70(r1)
  stfd f17, 0x78(r1)
  stfd f18, 0x80(r1)
  stfd f19, 0x88(r1)
  stfd f20, 0x90(r1)
  stfd f21, 0x98(r1)
  stfd f22, 0xa0(r1)
  stfd f23, 0xa8(r1)
  stfd f24, 0xb0(r1)
  stfd f25, 0xb8(r1)
  stw r5, 0x50(r1)
  stw r4, 0x54(r1)
  lis r31, value0_3@ha
  lfs f6, value0_3@l(r31)
  lis r31, value0_6@ha
  lfs f9, value0_6@l(r31)
  lis r31, i2fMagic@ha
  lfd f5, i2fMagic@l(r31)
  lfs f2, 0x168(r4)
  lfs f15, 0x1a0(r4)
  lfs f8, 0x19c(r4)
  fmuls f3, f8, f9
  fsubs f4, f9, f3
  slwi r30, r5, 1
  add r30, r30, r5
  mulli r31, r30, 0x14
  addi r29, r4, 0xb4
  add r29, r29, r31
  addi r27, r4, 0x0
  add r27, r27, r31
  slwi r31, r30, 2
  add r31, r31, r4
  lfs f22, 0x16c(r31)
  lfs f23, 0x170(r31)
  lfs f24, 0x174(r31)
  slwi r31, r5, 2
  add r31, r31, r4
  lfs f25, 0x190(r31)
  lwz r31, 0x1a4(r4)
  lis r30, 0x4330
  stw r30, 0x58(r1)
  addi r22, r31, -0x1
  slwi r22, r22, 2
  slwi r28, r5, 2
  add r28, r28, r4
  cmpwi cr7, r31, 0x0
  lwz r21, 0x0(r29)
  lwz r20, 0x4(r29)
  lwz r19, 0x14(r29)
  lwz r18, 0x18(r29)
  lwz r17, 0x28(r29)
  lwz r16, 0x2c(r29)
  lfs f16, 0x10(r29)
  lfs f17, 0x24(r29)
  lfs f18, 0x38(r29)
  lwz r25, 0x8(r29)
  lwz r24, 0x1c(r29)
  lwz r23, 0x30(r29)
  lwz r4, 0xc(r29)
  lwz r5, 0x20(r29)
  lwz r6, 0x34(r29)
  lwz r12, 0x0(r27)
  lwz r11, 0x4(r27)
  lwz r10, 0x14(r27)
  lwz r9, 0x18(r27)
  lwz r8, 0x28(r27)
  lwz r7, 0x2c(r27)
  lfs f19, 0x10(r27)
  lfs f20, 0x24(r27)
  lfs f21, 0x38(r27)
  lwz r15, 0x8(r27)
  lwz r14, 0x1c(r27)
  lwz r30, 0x0(r3)
  xoris r30, r30, 0x8000
  stw r30, 0x5c(r1)
  lfd f12, 0x58(r1)
  fsubs f12, f12, f5
  li r31, 0x9f
  mtctr r31
L_00000954:
  fmr f13, f12
  beq cr7, L_00000984
  lwz r30, 0x1ac(r28)
  lwz r29, 0x1b8(r28)
  add r31, r22, r30
  addi r29, r29, 0x4
  lfs f13, -0x4(r29)
  cmpw r29, r31
  stfs f12, -0x4(r29)
  bne+ L_00000980
  mr r29, r30
L_00000980:
  stw r29, 0x1b8(r30)
L_00000984:
  fmadds f8, f22, f16, f13
  lwzu r29, 0x4(r3)
  fmadds f9, f23, f17, f13
  stfsx f8, r4, r21
  addi r21, r21, 0x4
  stfsx f9, r5, r19
  lfsx f14, r4, r20
  addi r20, r20, 0x4
  lfsx f17, r5, r18
  cmpw r21, r25
  cmpw cr1, r20, r25
  addi r19, r19, 0x4
  addi r18, r18, 0x4
  fmr f16, f14
  cmpw cr5, r19, r24
  fadds f14, f14, f17
  cmpw cr6, r18, r24
  bne+ L_000009D0
  li r21, 0x0
L_000009D0:
  fmadds f8, f24, f18, f13
  bne+ cr1, L_000009DC
  li r20, 0x0
L_000009DC:
  stfsx f8, r6, r17
  addi r17, r17, 0x4
  bne+ cr5, L_000009EC
  li r19, 0x0
L_000009EC:
  lfsx f18, r6, r16
  addi r16, r16, 0x4
  cmpw r17, r23
  bne+ cr6, L_00000A00
  li r18, 0x0
L_00000A00:
  fadds f14, f14, f18
  cmpw cr1, r16, r23
  lwz r26, 0xc(r27)
  fmadds f9, f2, f19, f14
  bne+ L_00000A18
  li r17, 0x0
L_00000A18:
  bne+ cr1, L_00000A20
  li r16, 0x0
L_00000A20:
  xoris r29, r29, 0x8000
  stfsx f9, r26, r12
  fnmsubs f14, f2, f9, f19
  addi r12, r12, 0x4
  lfsx f19, r26, r11
  cmpw cr5, r12, r15
  addi r11, r11, 0x4
  lwz r26, 0x20(r27)
  cmpw cr6, r11, r15
  fmadds f8, f2, f20, f14
  bne+ cr5, L_00000A50
  li r12, 0x0
L_00000A50:
  stw r29, 0x5c(r1)
  stfsx f8, r26, r10
  fnmsubs f14, f2, f8, f20
  addi r10, r10, 0x4
  bne+ cr6, L_00000A68
  li r11, 0x0
L_00000A68:
  lfsx f20, r26, r9
  cmpw r10, r14
  fmuls f14, f14, f6
  addi r9, r9, 0x4
  cmpw cr1, r9, r14
  lfd f10, 0x58(r1)
  fmadds f14, f15, f25, f14
  bne+ L_00000A8C
  li r10, 0x0
L_00000A8C:
  lwz r26, 0x34(r27)
  fmadds f9, f2, f21, f14
  fmr f25, f14
  bne+ cr1, L_00000AA0
  li r9, 0x0
L_00000AA0:
  stfsx f9, r26, r8
  fnmsubs f14, f2, f9, f21
  lwz r31, 0x30(r27)
  fmuls f8, f4, f12
  lfsx f21, r26, r7
  addi r8, r8, 0x4
  addi r7, r7, 0x4
  fmadds f14, f3, f14, f8
  cmpw cr5, r8, r31
  cmpw cr6, r7, r31
  fctiwz f14, f14
  bne+ cr5, L_00000AD4
  li r8, 0x0
L_00000AD4:
  bne+ cr6, L_00000ADC
  li r7, 0x0
L_00000ADC:
  li r31, -0x4
  fsubs f12, f10, f5
  stfiwx f14, r3, r31
  bdnz L_00000954
  fmr f13, f12
  beq cr7, L_00000B1C
  lwz r30, 0x1ac(r28)
  lwz r29, 0x1b8(r28)
  add r31, r22, r30
  addi r29, r29, 0x4
  lfs f13, -0x4(r29)
  cmpw r29, r31
  stfs f12, -0x4(r29)
  bne+ L_00000B18
  mr r29, r30
L_00000B18:
  stw r29, 0x1b8(r30)
L_00000B1C:
  fmadds f8, f22, f16, f13
  fmadds f9, f23, f17, f13
  stfsx f8, r4, r21
  addi r21, r21, 0x4
  stfsx f9, r5, r19
  lfsx f14, r4, r20
  addi r20, r20, 0x4
  lfsx f17, r5, r18
  cmpw r21, r25
  cmpw cr1, r20, r25
  addi r19, r19, 0x4
  addi r18, r18, 0x4
  fmr f16, f14
  cmpw cr5, r19, r24
  fadds f14, f14, f17
  cmpw cr6, r18, r24
  bne+ L_00000B64
  li r21, 0x0
L_00000B64:
  fmadds f8, f24, f18, f13
  bne+ cr1, L_00000B70
  li r20, 0x0
L_00000B70:
  stfsx f8, r6, r17
  addi r17, r17, 0x4
  bne+ cr5, L_00000B80
  li r19, 0x0
L_00000B80:
  lfsx f18, r6, r16
  addi r16, r16, 0x4
  cmpw r17, r23
  bne+ cr6, L_00000B94
  li r18, 0x0
L_00000B94:
  fadds f14, f14, f18
  cmpw cr1, r16, r23
  lwz r26, 0xc(r27)
  fmadds f9, f2, f19, f14
  bne+ L_00000BAC
  li r17, 0x0
L_00000BAC:
  bne+ cr1, L_00000BB4
  li r16, 0x0
L_00000BB4:
  stfsx f9, r26, r12
  fnmsubs f14, f2, f9, f19
  addi r12, r12, 0x4
  lfsx f19, r26, r11
  cmpw cr5, r12, r15
  addi r11, r11, 0x4
  lwz r26, 0x20(r27)
  cmpw cr6, r11, r15
  fmadds f8, f2, f20, f14
  bne+ cr5, L_00000BE0
  li r12, 0x0
L_00000BE0:
  stfsx f8, r26, r10
  fnmsubs f14, f2, f8, f20
  addi r10, r10, 0x4
  bne+ cr6, L_00000BF4
  li r11, 0x0
L_00000BF4:
  lfsx f20, r26, r9
  cmpw r10, r14
  fmuls f14, f14, f6
  addi r9, r9, 0x4
  cmpw cr1, r9, r14
  fmadds f14, f15, f25, f14
  bne+ L_00000C14
  li r10, 0x0
L_00000C14:
  lwz r26, 0x34(r27)
  lwz r5, 0x50(r1)
  lwz r4, 0x54(r1)
  fmadds f9, f2, f21, f14
  fmr f25, f14
  bne+ cr1, L_00000C30
  li r9, 0x0
L_00000C30:
  stfsx f9, r26, r8
  fnmsubs f14, f2, f9, f21
  lwz r29, 0x30(r27)
  fmuls f8, f4, f12
  lfsx f21, r26, r7
  addi r8, r8, 0x4
  addi r7, r7, 0x4
  fmadds f14, f3, f14, f8
  cmpw cr5, r8, r29
  cmpw cr6, r7, r29
  fctiwz f14, f14
  bne+ cr5, L_00000C64
  li r8, 0x0
L_00000C64:
  bne+ cr6, L_00000C6C
  li r7, 0x0
L_00000C6C:
  slwi r30, r5, 1
  add r30, r30, r5
  mulli r31, r30, 0x14
  stfiwx f14, r0, r3
  addi r29, r4, 0xb4
  add r29, r29, r31
  stw r21, 0x0(r29)
  stw r20, 0x4(r29)
  stw r19, 0x14(r29)
  stw r18, 0x18(r29)
  stw r17, 0x28(r29)
  stw r16, 0x2c(r29)
  stfs f16, 0x10(r29)
  stfs f17, 0x24(r29)
  stfs f18, 0x38(r29)
  stw r12, 0x0(r27)
  stw r11, 0x4(r27)
  stw r10, 0x14(r27)
  stw r9, 0x18(r27)
  stw r8, 0x28(r27)
  stw r7, 0x2c(r27)
  stfs f19, 0x10(r27)
  stfs f20, 0x24(r27)
  stfs f21, 0x38(r27)
  slwi r31, r5, 2
  add r31, r31, r4
  stfs f25, 0x190(r31)
  lfd f14, 0x60(r1)
  lfd f15, 0x68(r1)
  lfd f16, 0x70(r1)
  lfd f17, 0x78(r1)
  lfd f18, 0x80(r1)
  lfd f19, 0x88(r1)
  lfd f20, 0x90(r1)
  lfd f21, 0x98(r1)
  lfd f22, 0xa0(r1)
  lfd f23, 0xa8(r1)
  lfd f24, 0xb0(r1)
  lfd f25, 0xb8(r1)
  lmw r14, 0x8(r1)
  addi r1, r1, 0xc0
  blr
}
/* clang-format on */
#else
/* PORT: the asm above, in C, including the console's pre-delay bug. */
static f32 DLpush(_SND_REVHI_DELAYLINE* dl, f32 v) {
  f32 out;
  *(f32*)((u8*)dl->inputs + dl->inPoint) = v;
  dl->inPoint += (s32)sizeof(f32);
  if (dl->inPoint == dl->length) {
    dl->inPoint = 0;
  }
  out = *(f32*)((u8*)dl->inputs + dl->outPoint);
  dl->outPoint += (s32)sizeof(f32);
  if (dl->outPoint == dl->length) {
    dl->outPoint = 0;
  }
  return out;
}

static void HandleReverb(s32* sptr, SND_AUX_REVERBHI* rv, s32 k) {
  _SND_REVHI_WORK* w = &rv->rv;
  _SND_REVHI_DELAYLINE* C = &w->C[k * 3];
  _SND_REVHI_DELAYLINE* AP = &w->AP[k * 3];
  const f32 g = w->allPassCoeff;
  const f32 damp = w->damping;
  const f32 wet = w->level * 0.6f;
  const f32 dry = 0.6f - (w->level * 0.6f);
  f32 lp = w->lpLastout[k];
  s32 i;
  s32 j;

  for (i = 0; i < 160; ++i) {
    const f32 x = (f32)sptr[i];
    f32 in = x;
    f32 sum = 0.f;
    f32 y;

    if (w->preDelayTime != 0) {
      /* One sample, not preDelayTime of them: the console never advanced this pointer. */
      f32* p = w->preDelayPtr[k];
      in = *p;
      *p = x;
    }

    for (j = 0; j < 3; ++j) {
      C[j].lastOutput = DLpush(&C[j], w->combCoef[k * 3 + j] * C[j].lastOutput + in);
      sum += C[j].lastOutput;
    }

    y = sum;
    for (j = 0; j < 2; ++j) {
      const f32 v = g * AP[j].lastOutput + y;
      y = AP[j].lastOutput - g * v;
      AP[j].lastOutput = DLpush(&AP[j], v);
    }

    /* The damping one-pole sits here, between all-pass 2 and all-pass 3. */
    y = y * 0.3f + damp * lp;
    lp = y;

    {
      const f32 v = g * AP[2].lastOutput + y;
      y = AP[2].lastOutput - g * v;
      AP[2].lastOutput = DLpush(&AP[2], v);
    }

    sptr[i] = (s32)(wet * y + dry * x);
  }

  w->lpLastout[k] = lp;
}
#endif

void ReverbHICallback(s32* left, s32* right, s32* surround, SND_AUX_REVERBHI* rev) {
  u8 i;
  for (i = 0; i < 3; ++i) {
    switch (i) {
    case 0:
      if (rev->rv.crosstalk != 0.f) {
        DoCrossTalk(left, right, rev->rv.crosstalk * 0.5f, 1.f - (rev->rv.crosstalk * 0.5f));
      }
      HandleReverb(left, rev, 0);
      break;
    case 1:
      HandleReverb(right, rev, 1);
      break;
    case 2:
      HandleReverb(surround, rev, 2);
      break;
    }
  }
}

void ReverbHIFree(_SND_REVHI_WORK* rv) {
  u8 i;
  for (i = 0; i < 9; ++i) {
    DLdelete(&rv->AP[i]);
  }
  for (i = 0; i < 9; ++i) {
    DLdelete(&rv->C[i]);
  }
  if (rv->preDelayTime != 0) {
    for (i = 0; i < 3; ++i) {
      salFree(rv->preDelayLine[i]);
    }
  }
}
