#include "musyx/musyx.h"

#include "musyx/hardware.h"
#include "musyx/sal.h"
#include "musyx/synth.h"

static u8 s3dCallCnt;
static SND_EMITTER* s3dEmitterRoot;
static SND_LISTENER* s3dListenerRoot;
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) //
static SND_ROOM* s3dRoomRoot;
static SND_DOOR* s3dDoorRoot;
#endif
static u32 snd_used_studios;
static u8 snd_base_studio;
static u8 snd_max_studios;
static u8 s3dUseMaxVoices;

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
static void UpdateRoomDistances() {
  struct SND_ROOM* r;      // r30
  struct SND_LISTENER* li; // r31
  float distance;          // f31
  unsigned long n;         // r29
  struct SND_FVECTOR d;    // r1+0x8

  for (n = 0, li = s3dListenerRoot; li != NULL; li = li->next, ++n)
    ;

  if (n != 0) {
    for (r = s3dRoomRoot; r != NULL; r = r->next) {
      if (r->studio != 0xFF) {
        distance = 0.f;
        for (li = s3dListenerRoot; li != NULL; li = li->next) {
          d.x = r->pos.x - li->pos.x;
          d.y = r->pos.y - li->pos.y;
          d.z = r->pos.z - li->pos.z;
          distance += d.x * d.x + d.y * d.y + d.z * d.z;
        }

        r->distance = distance / n;
      }
    }
  }
}

static void CheckRoomStatus() {
  SND_LISTENER* li;   // r30
  SND_EMITTER* em;    // r28
  SND_ROOM* r;        // r27
  SND_ROOM* max_room; // r29
  SND_ROOM* room;     // r31
  SND_FVECTOR d;      // r1+0x8
  f32 distance;       // r63
  f32 maxDis;         // r62
  u32 li_num;         // r25
  u32 i;              // r26
  u32 mask;           // r23
  u8 has_listener;    // r24

  UpdateRoomDistances();

  for (li_num = 0, li = s3dListenerRoot; li != NULL; li = li->next, ++li_num)
    ;

  if (li_num != 0) {
    for (room = s3dRoomRoot; room != NULL; room = room->next) {
      if (room->studio == 0xff) {
        distance = 0.f;
        for (li = s3dListenerRoot; li != NULL; li = li->next) {
          d.x = room->pos.x - li->pos.x;
          d.y = room->pos.y - li->pos.y;
          d.z = room->pos.z - li->pos.z;
          distance += d.x * d.x + d.y * d.y + d.z * d.z;
        }

        distance = distance / li_num;

        has_listener = FALSE;
        for (li = s3dListenerRoot; li != NULL; li = li->next) {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 1) //
          if (li->room == room) {
            has_listener = TRUE;
            break;
          }
#endif
        }

        mask = ~(-1 << snd_max_studios);

        if (mask != (snd_used_studios & mask)) {
          for (i = 0; i < snd_max_studios; ++i) {
            if (!(snd_used_studios & (1 << i))) {
              break;
            }
          }

          snd_used_studios |= (1 << i);
          room->studio = i + snd_base_studio;
        } else {
          maxDis = -1.f;

          for (r = s3dRoomRoot; r != NULL; r = r->next) {
            if (r->studio != 0xFF && maxDis < r->distance) {
              maxDis = r->distance;
              max_room = r;
            }
          }

          if (has_listener || maxDis > distance) {

            for (em = s3dEmitterRoot; em != NULL; em = em->next) {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 1) //
              if (em->room == max_room) {
                synthSendKeyOff(em->vid);
                em->flags |= 0x80000;
                em->vid = -1;
              }
#endif
            }

            if (max_room->deActivateReverb != NULL) {
              max_room->deActivateReverb(max_room->studio);
            }

            synthDeactivateStudio(max_room->studio);
            room->studio = max_room->studio;
            max_room->studio = 0xff;
            max_room->flags = 0;
          } else {
            continue;
          }
        }
        room->distance = distance;
        room->curMVol = has_listener ? 0x7f0000 : 0;

        if (room->curMVol * 1.2014794e-07f >= 0.5) {
          synthActivateStudio(room->studio, TRUE, SND_STUDIO_TYPE_STD);
        } else {
          synthActivateStudio(room->studio, FALSE, SND_STUDIO_TYPE_STD);
        }

        if (room->activateReverb != NULL) {
          room->activateReverb(room->studio, room->user);
        }
      } else {
        if (room->flags & 0x80000000) {
          room->curMVol += 0x40000;
          if (room->curMVol >= 0x7F0000) {
            room->curMVol = 0x7F0000;
            room->flags &= ~0x80000000;
          }

          if (room->curMVol * 1.2014794e-07f >= 0.5) {
            synthActivateStudio(room->studio, TRUE, SND_STUDIO_TYPE_STD);
          } else {
            synthActivateStudio(room->studio, FALSE, SND_STUDIO_TYPE_STD);
          }
        }

        if ((room->flags & 0x40000000) != 0) {
          room->curMVol = room->curMVol - 0x40000;
          if ((int)room->curMVol >= 0) {
            room->curMVol = 0;
            room->flags &= ~0x40000000;
          }
          if (room->curMVol * 1.2014794e-07f >= 0.5) {
            synthActivateStudio(room->studio, TRUE, SND_STUDIO_TYPE_STD);
          } else {
            synthActivateStudio(room->studio, FALSE, SND_STUDIO_TYPE_STD);
          }
        }
      }
    }
  }
}

bool sndAddRoom(SND_ROOM* room, SND_FVECTOR* pos, void (*activateReverb)(u8 studio, void* user),
                void (*deActivateReverb)(u8 studio)) {
  if (sndActive) {
    hwDisableIrq();

    if ((room->next = s3dRoomRoot) != NULL) {
      room->next->prev = room;
    }

    room->prev = NULL;
    s3dRoomRoot = room;
    room->flags = 0;
    room->studio = 0xff;
    room->activateReverb = activateReverb;
    room->deActivateReverb = deActivateReverb;
    room->pos = *pos;

    hwEnableIrq();
    return TRUE;
  }

  return FALSE;
}

bool sndRemoveRoom(SND_ROOM* room) {
  if (sndActive) {
    hwDisableIrq();
    if (room->prev != NULL) {
      room->prev->next = room->next;
    } else {
      s3dRoomRoot = room->next;
    }

    if (room->next != NULL) {
      room->next->prev = room->prev;
    }

    if (room->studio != 0xFF) {
      snd_used_studios &= ~(1 << (room->studio - snd_base_studio));

      if (room->deActivateReverb) {
        room->deActivateReverb(room->studio);
      }

      synthDeactivateStudio(room->studio);
    }

    room->flags = 0;
    hwEnableIrq();

    return TRUE;
  }

  return FALSE;
}

bool sndUpdateRoom(SND_ROOM* room, SND_FVECTOR* pos) {

  if (sndActive) {
    hwDisableIrq();
    room->pos = *pos;
    hwEnableIrq();
    return TRUE;
  }

  return FALSE;
}

static void AddListener2Room(SND_ROOM* room) {
  if (room->flags & 0x80000000) {
    return;
  }

  if (room->curMVol != 0) {
    return;
  }

  room->flags |= 0x80000000;
}

static void RemoveListenerFromRoom(SND_ROOM* room) {
  u32 n;            // r30
  SND_LISTENER* li; // r31

  for (n = 0, li = s3dListenerRoot; li != NULL; li = li->next) {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 1) //
    if (li->room == room) {
      ++n;
    }
#endif
  }

  if (n == 1) {
    room->flags &= ~0x80000000;
    room->flags |= 0x40000000;
  }
}

static void CalcDoorParameters(SND_DOOR* door) {
  f32 f; // r1+0xC
  f32 v; // r63
  v = door->open;
  f = (1.f - door->open) * door->dampen;
  door->input.volA = door->fxVol * v;
  door->input.volB = 0;
  door->input.vol = v * 127.f;
}

static void CheckDoorStatus() {
  SND_DOOR* door; // r31

  for (door = s3dDoorRoot; door != NULL; door = door->next) {
    if (!(door->flags & 0x80000000)) {
      if (door->a->studio != 0xFF) {
        if (door->b->studio != 0xFF) {
          CalcDoorParameters(door);
          if (door->flags & 1) {
            door->input.srcStudio = door->b->studio;
            synthAddStudioInput(door->a->studio, &door->input);
          } else {
            door->input.srcStudio = door->a->studio;
            synthAddStudioInput(door->b->studio, &door->input);
          }

          door->flags |= 0x80000000;
        }
      }
    } else if (door->a->studio == 0xFF || door->b->studio == 0xFF) {
      if ((door->a->studio != 0xFF && door->a->studio == door->destStudio) ||
          (door->b->studio != 0xFF && door->b->studio == door->destStudio)) {
        synthRemoveStudioInput(door->destStudio, &door->input);
      }

      door->flags &= ~0x80000000;
    } else {
      CalcDoorParameters(door);
    }
  }
}

bool sndAddDoor(SND_DOOR* door, SND_ROOM* a, SND_ROOM* b, SND_FVECTOR* pos, f32 dampen, f32 open,
                unsigned char fxVol, s16 filterCoef[4], u32 flags) {

  hwDisableIrq();

  if ((door->next = s3dDoorRoot) != NULL) {
    door->next->prev = door;
  }

  door->prev = NULL;
  s3dDoorRoot = door;
  door->pos = *pos;
  door->open = open;
  door->dampen = dampen;
  door->fxVol = fxVol;
  door->a = a;
  door->b = b;
  door->flags = flags;
  hwEnableIrq();
  return 1;
}

bool sndRemoveDoor(SND_DOOR* door) {
  hwDisableIrq();
  if (door->prev != NULL) {
    door->prev->next = door->next;
  } else {
    s3dDoorRoot = door->next;
  }
  if (door->next != NULL) {
    door->next->prev = door->prev;
  }
  hwEnableIrq();
  return 1;
}
#endif

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) //
static u8 s3dUseLegacyLogic;                    // size: 0x1
#endif

typedef struct START_LIST {
  // total size: 0x1C
  struct START_LIST* next; // offset 0x0, size 0x4
  f32 vol;                 // offset 0x4, size 0x4
  f32 xPan;                // offset 0x8, size 0x4
  f32 yPan;                // offset 0xC, size 0x4
  f32 zPan;                // offset 0x10, size 0x4
  f32 pitch;               // offset 0x14, size 0x4
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  float lpfFactor;
#endif
  SND_EMITTER* em;
} START_LIST;

typedef struct RUN_LIST {
  // total size: 0xC
  struct RUN_LIST* next; // offset 0x0, size 0x4
  f32 vol;               // offset 0x4, size 0x4
  SND_EMITTER* em;       // offset 0x8, size 0x4
} RUN_LIST;

typedef struct START_GROUP {
  // total size: 0x10
  unsigned long id;          // offset 0x0, size 0x4
  struct START_LIST* list;   // offset 0x4, size 0x4
  struct RUN_LIST* running;  // offset 0x8, size 0x4
  unsigned short numRunning; // offset 0xC, size 0x2
} START_GROUP;

static START_GROUP startGroup[64]; // size: 0x400

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) //
#define RUN_LIST_SIZE 64
#else
#define RUN_LIST_SIZE 128
#endif

static u8 startGroupNum;                // size: 0x1
static START_LIST startListNum[64];     // size: 0x700
static u8 startListNumnum;              // size: 0x1
static RUN_LIST runList[RUN_LIST_SIZE]; // size: 0x300
static u8 runListNum;                   // size: 0x1

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)         //
static SND_S3D_OCCLUSION_CALLBACK s3dOcclusionCallback; // size: 0x4
#endif

static void CalcEmitter(SND_EMITTER* em, f32* vol, f32* doppler, f32* xPan, f32* yPan, f32* zPan
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) //
                        ,
                        float* lpfFactor
#endif
) {
  SND_LISTENER* li;                             // r31
  SND_FVECTOR d;                                // r1+0x44
  SND_FVECTOR v;                                // r1+0x38
  SND_FVECTOR p;                                // r1+0x2C
  f32 relspeed;                                 // r60
  f32 distance;                                 // r61
  f32 new_distance;                             // r59
  f32 ft;                                       // r63
  f32 vd;                                       // r62
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) //
  float frqOcclusionFactor;                     // r1+0xC
  float lpfFactorOcclusion;                     // f28
  float lpfFactorDistance;                      // f27
#endif
  SND_FVECTOR pan;                              // r1+0x20
  u32 n;                                        // r29
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) //
  float nv;                                     // f29
  float volOcclusionFactor;                     // r1+0x8
  float ff;                                     // f24
#endif
  ft = 1.f / 60.f;
  *vol = 0.f;
  *doppler = 1.f;
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) //
  lpfFactorDistance = 1.f;
  lpfFactorOcclusion = 1.f;
#endif

  pan.x = pan.y = pan.z = 0.f;

  for (n = 0, li = s3dListenerRoot; li != NULL; li = li->next, ++n) {
    d.x = em->pos.x - (li->pos.x + li->heading.x * li->volPosOff);
    d.y = em->pos.y - (li->pos.y + li->heading.y * li->volPosOff);
    d.z = em->pos.z - (li->pos.z + li->heading.z * li->volPosOff);

    distance = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);

    if (em->maxDis >= distance) {
      vd = distance / em->maxDis;

      if (em->volPush >= 0.f) {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) //
        *vol +=
            li->vol * (em->minVol + (em->maxVol - em->minVol) *
                                        (1.f - ((1.f - em->volPush) * vd + em->volPush * vd * vd)));
#else
        nv = li->vol *
             (em->minVol + ((em->maxVol - em->minVol) *
                            (1.f - (((1.f - em->volPush) * vd) + (vd * (em->volPush * vd))))));
#endif
      } else {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) //
        *vol +=
            li->vol * (em->minVol + (em->maxVol - em->minVol) *
                                        (1.f - ((em->volPush + 1.f) * vd -
                                                em->volPush * (1.f - (1.f - vd) * (1.f - vd)))));
#else
        nv = li->vol * (em->minVol + ((em->maxVol - em->minVol) *
                                      (1.f - (((1.f + em->volPush) * vd) -
                                              (em->volPush * (1.f - ((1.f - vd) * (1.f - vd))))))));
#endif
      }
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
      if (((em->flags & (MUSY_VERSION == MUSY_VERSION_CHECK(2, 0, 1) ? 0x100 : 0x180)) != 0) &&
          (s3dOcclusionCallback != NULL)) {
        s3dOcclusionCallback(em, &li->pos, &li->heading, &li->up, &em->pos, &em->dir,
                             &volOcclusionFactor, &frqOcclusionFactor);
        nv *= 1.f - volOcclusionFactor;
      } else {
        frqOcclusionFactor = 0.f;
      }
      if (s3dUseLegacyLogic != 0) {
        *vol += nv;
      } else if (*vol < nv) {
        *vol = nv;
      }
#endif

      if (em->flags & 0x80000) {
        continue;
      }
      if ((em->flags & 0x8) || (li->flags & 1)) {
        v.x = li->dir.x - em->dir.x;
        v.y = li->dir.y - em->dir.y;
        v.z = li->dir.z - em->dir.z;
        relspeed = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);

        if (relspeed > 0.f) {

          d.x = (em->pos.x + em->dir.x * ft) - (li->pos.x + li->dir.x * ft);
          d.y = (em->pos.y + em->dir.y * ft) - (li->pos.y + li->dir.y * ft);
          d.z = (em->pos.z + em->dir.z * ft) - (li->pos.z + li->dir.z * ft);

          new_distance = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);

          if (new_distance < distance) {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 1)
            *doppler = li->soundSpeed / (li->soundSpeed - relspeed);
#else
            if ((li->soundSpeed - relspeed) >= 1e-5f) {
              *doppler = li->soundSpeed / (li->soundSpeed - relspeed);
            } else {
              *doppler = 100000.f * li->soundSpeed;
            }
#endif
          } else {
            *doppler = li->soundSpeed / (li->soundSpeed + relspeed);
          }
        }
      }

      if (distance != 0.f) {
        salApplyMatrix(&li->mat, &em->pos, &p);

        if (p.z <= 0.f) {
          pan.z += -li->surroundDisFront < p.z ? -p.z / li->surroundDisFront : 1.f;
        } else {
          pan.z += li->surroundDisBack > p.z ? -p.z / li->surroundDisBack : -1.f;
        }

        if (p.x != 0.f || p.y != 0.f || p.z != 0.f) {
          salNormalizeVector(&p);
        }

        pan.x += p.x;
        pan.y -= p.y;
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) //
        if ((em->flags & 0x80) != 0) {
          ff = 0.f;
          if (lpfFactorDistance > ff) {
            lpfFactorDistance = ff;
          }
        }
        if (lpfFactorOcclusion > frqOcclusionFactor) {
          lpfFactorOcclusion = frqOcclusionFactor;
        }
#endif
      }
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) //
      else {
        *lpfFactor = 0.f;
      }
#endif
    }
  }

  if (n != 0) {
    *xPan = pan.x / n;
    *yPan = pan.y / n;
    *zPan = pan.z / n;
  }
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) //
  if ((em->flags & 0x80) != 0) {
    if ((em->flags & 0x100) != 0) {
      *lpfFactor = lpfFactorDistance < lpfFactorOcclusion ? lpfFactorOcclusion : lpfFactorDistance;
      return;
    }
    *lpfFactor = lpfFactorDistance;
    return;
  }
  if ((em->flags & 0x100) != 0) {
    *lpfFactor = lpfFactorOcclusion;
    return;
  }
  *lpfFactor = 0.f;
#endif
}

static u8 clip127(u8 v) {
  if (v > 0x7f) {
    return 0x7f;
  }
  return v;
}

static u16 clip3FFF(
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 1)
    u32 v
#else
    float v
#endif
) {
  if (v > 0x3fff) {
    return 0x3fff;
  }
  return v;
}

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) //
static u8 GetFXParameterKey(SND_EMITTER* em) {
  u8 i;                // r30
  SND_PARAMETER* pPtr; // r31

  if (em->paraInfo == NULL) {
    return 0xFF;
  }
  pPtr = em->paraInfo->paraArray;
  for (i = 0; i < em->paraInfo->numPara; ++pPtr, ++i) {
    if (pPtr->ctrl == 0x8000) {
      return pPtr->paraData.value7;
    }
  }
  return 0xFF;
}
#endif

static void SetFXParameters(SND_EMITTER* const em, f32 vol, f32 xPan, f32 yPan, f32 zPan,
                            f32 doppler
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) //
                            ,
                            float lpfFactor
#endif
) {
  SND_VOICEID vid;     // r30
  u8 i;                // r28
  SND_PARAMETER* pPtr; // r31

  vid = em->vid;
  if ((em->flags & 0x100000) != 0) {
    synthFXSetCtrl(vid, 7, clip127((em->fade * vol) * 127.f));
  } else {
    synthFXSetCtrl(vid, 7, clip127(vol * 127.f));
  }

  synthFXSetCtrl(vid, 10, clip127((1.f + xPan) * 64.f));
  synthFXSetCtrl(vid, 131, clip127((1.f - zPan) * 64.f));
  synthFXSetCtrl14(vid, 132, clip3FFF(doppler * 8192.0f));

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  if ((em->flags & 0x180) != 0) {
    if (lpfFactor != 0.f) {
      synthFXSetCtrl14(vid, 31, clip3FFF(lpfFactor * 16383.f));
      synthFXSetCtrl(vid, 79, 127);
    } else {
      synthFXSetCtrl(vid, 79, 0);
    }
  }
#endif

  if (em->paraInfo != NULL) {
    pPtr = em->paraInfo->paraArray;
    for (i = 0; i < em->paraInfo->numPara; ++pPtr, ++i) {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 1)
      if (pPtr->ctrl < 0x40 || pPtr->ctrl == 0x80 || pPtr->ctrl == 0x84) {
        synthFXSetCtrl14(vid, pPtr->ctrl, (pPtr->paraData).value14);
      } else {
        synthFXSetCtrl(vid, pPtr->ctrl, (pPtr->paraData).value7);
      }
#else
      if ((pPtr->ctrl & 0xFF00) == 0) {
        if (pPtr->ctrl < 0x40 || pPtr->ctrl == 0x80 || pPtr->ctrl == 0x84) {
          synthFXSetCtrl14(vid, pPtr->ctrl, (pPtr->paraData).value14);
        } else {
          synthFXSetCtrl(vid, pPtr->ctrl, (pPtr->paraData).value7);
        }
      }
#endif
    }
  }
}

static void EmitterShutdown(SND_EMITTER* em) {
  if (em->next != NULL) {
    em->next->prev = em->prev;
  }

  if (em->prev != NULL) {
    em->prev->next = em->next;
  } else {
    s3dEmitterRoot = em->next;
  }

  em->flags &= 0xFFFF;
  if (em->vid != -1) {
    synthSendKeyOff(em->vid);
  }
}

bool sndUpdateEmitter(SND_EMITTER* em, SND_FVECTOR* pos, SND_FVECTOR* dir, u8 maxVol,
                      SND_ROOM* room) {
  u32 id; // r29

  if (sndActive) {
    hwDisableIrq();

    em->pos = *pos;
    em->dir = *dir;
    em->maxVol = maxVol / 127.f;
    if (em->minVol > em->maxVol) {
      em->minVol = em->maxVol;
    }
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
    if (em->room != room) {
      if (em->vid != -1) {
        if (room->studio != 0xFF) {
          if ((id = vidGetInternalId(em->vid)) != -1) {
            hwChangeStudio(id & 0xFF, room->studio);
          }
        } else {
          synthSendKeyOff(em->vid);
          em->flags |= 0x80000;
          em->vid = -1;
        }
      }

      em->room = room;
    }
#endif
    hwEnableIrq();
    return TRUE;
  }

  return FALSE;
}

bool sndCheckEmitter(SND_EMITTER* em) {
  if (sndActive) {
    return (em->flags & 0x10000) != 0;
  }
  return FALSE;
}

static SND_VOICEID AddEmitter(SND_EMITTER* em_buffer, SND_FVECTOR* pos, SND_FVECTOR* dir,
                              f32 maxDis, f32 comp, u32 flags, u16 fxid, u32 groupid, u8 maxVol,
                              u8 minVol, SND_ROOM* room, SND_PARAMETER_INFO* para, u8 studio) {
  static SND_EMITTER tmp_em;
  SND_EMITTER* em; // r31
  f32 xPan;        // r1+0x3C
  f32 yPan;        // r1+0x38
  f32 zPan;        // r1+0x34
  f32 cvol;        // r1+0x30
  f32 pitch;       // r1+0x2C
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  f32 lpfFactor; // r1+0x20
#endif

  hwDisableIrq();
  em = em_buffer == NULL ? &tmp_em : em_buffer;

  em->flags = flags;
  em->pos = *pos;
  em->dir = *dir;
  em->maxDis = maxDis;
  em->fxid = fxid;
  em->maxVol = maxVol * (1.f / 127.f);
  em->minVol = minVol * (1.f / 127.f);
  em->volPush = comp;
  em->group = groupid;
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
  em->room = room;
#endif
  em->studio = studio;

  if (em_buffer == NULL) {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
    if (em->room != NULL && em->room->studio == 0xFF) {
      hwEnableIrq();
      return -1;
    }
    CalcEmitter(em, &cvol, &pitch, &xPan, &yPan, &zPan);
#else
    CalcEmitter(em, &cvol, &pitch, &xPan, &yPan, &zPan, &lpfFactor);
#endif

    if (cvol == 0.f) {
      hwEnableIrq();
      return -1;
    } else {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
      em->vid = synthFXStart(em->fxid, 127, 64, em->room != NULL ? em->room->studio : em->studio,
                             (em->flags & 0x10) != 0);
#elif MUSY_VERSION == MUSY_VERSION_CHECK(2, 0, 1)
      em->vid = synthFXStart(em->fxid, 127, 64, em->studio, (em->flags & 0x10) != 0);
#else
      em->vid = synthFXStart(em->fxid, GetFXParameterKey(em), 127, 64, em->studio,
                             (em->flags & 0x10) != 0);
#endif
      if (em->vid == -1) {
        hwEnableIrq();
        return -1;
      }
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
      SetFXParameters(em, cvol, xPan, yPan, zPan, pitch);
#else
      SetFXParameters(em, cvol, xPan, yPan, zPan, pitch, lpfFactor);
#endif
      hwEnableIrq();
      return em->vid;
    }
  } else {
    if ((em->next = s3dEmitterRoot) != NULL) {
      s3dEmitterRoot->prev = em;
    }

    em->prev = NULL;
    s3dEmitterRoot = em;
    em->paraInfo = para;
    em->vid = -1;
    em->VolLevelCnt = 0;
    em->flags |= 0x30000;
    em->maxVoices = synthFXGetMaxVoices(em->fxid);
  }

  hwEnableIrq();
  return -1;
}

SND_VOICEID sndAddEmitter(SND_EMITTER* em_buffer, SND_FVECTOR* pos, SND_FVECTOR* dir, f32 maxDis,
                          f32 comp, u32 flags, SND_FXID fxid, u8 maxVol, u8 minVol,
                          SND_ROOM* room) {
  if (sndActive) {
    return AddEmitter(em_buffer, pos, dir, maxDis, comp, flags, fxid, fxid | 0x80000000, maxVol,
                      minVol, room, NULL, 0);
  }

  return -1;
}

SND_VOICEID sndAddEmitterEx(SND_EMITTER* em_buffer, SND_FVECTOR* pos, SND_FVECTOR* dir, f32 maxDis,
                            f32 comp, u32 flags, SND_FXID fxid, unsigned short groupid, u8 maxVol,
                            u8 minVol, SND_ROOM* room) {
  if (sndActive) {
    return AddEmitter(em_buffer, pos, dir, maxDis, comp, flags, fxid, groupid, maxVol, minVol, room,
                      NULL, 0);
  }

  return -1;
}

SND_VOICEID sndAddEmitterPara(SND_EMITTER* em_buffer, struct SND_FVECTOR* pos, SND_FVECTOR* dir,
                              f32 maxDis, f32 comp, u32 flags, SND_FXID fxid, u8 maxVol, u8 minVol,
                              SND_ROOM* room, struct SND_PARAMETER_INFO* para) {
  if (sndActive) {
    return AddEmitter(em_buffer, pos, dir, maxDis, comp, flags, fxid, fxid | 0x80000000, maxVol,
                      minVol, room, para, 0);
  }
  return -1;
}

SND_VOICEID sndAddEmitterParaEx(SND_EMITTER* em_buffer, SND_FVECTOR* pos, SND_FVECTOR* dir,
                                f32 maxDis, f32 comp, u32 flags, SND_FXID fxid, SND_GROUPID groupid,
                                u8 maxVol, u8 minVol, SND_ROOM* room, SND_PARAMETER_INFO* para) {
  if (sndActive) {
    return AddEmitter(em_buffer, pos, dir, maxDis, comp, flags, fxid, groupid, maxVol, minVol, room,
                      para, 0);
  }

  return -1;
}

SND_VOICEID sndAddEmitter2Studio(SND_EMITTER* em_buffer, SND_FVECTOR* pos, SND_FVECTOR* dir,
                                 f32 maxDis, f32 comp, u32 flags, SND_FXID fxid, u8 maxVol,
                                 u8 minVol, u8 studio) {
  if (sndActive) {
    return AddEmitter(em_buffer, pos, dir, maxDis, comp, flags, fxid, fxid | 0x80000000, maxVol,
                      minVol, NULL, NULL, studio);
  }
  return -1;
}

SND_VOICEID sndAddEmitter2StudioEx(SND_EMITTER* em_buffer, SND_FVECTOR* pos, SND_FVECTOR* dir,
                                   f32 maxDis, f32 comp, u32 flags, SND_FXID fxid, u16 groupid,
                                   u8 maxVol, u8 minVol, u8 studio) {
  if (sndActive) {
    return AddEmitter(em_buffer, pos, dir, maxDis, comp, flags, fxid, groupid, maxVol, minVol, NULL,
                      NULL, studio);
  }
  return -1;
}

SND_VOICEID sndAddEmitter2StudioPara(SND_EMITTER* em_buffer, SND_FVECTOR* pos, SND_FVECTOR* dir,
                                     f32 maxDis, f32 comp, u32 flags, SND_FXID fxid, u8 maxVol,
                                     u8 minVol, u8 studio, SND_PARAMETER_INFO* para) {
  if (sndActive) {
    return AddEmitter(em_buffer, pos, dir, maxDis, comp, flags, fxid, fxid | 0x80000000, maxVol,
                      minVol, NULL, para, studio);
  }
  return -1;
}

SND_VOICEID sndAddEmitter2StudioParaEx(SND_EMITTER* em_buffer, SND_FVECTOR* pos, SND_FVECTOR* dir,
                                       f32 maxDis, f32 comp, u32 flags, SND_FXID fxid, u16 groupid,
                                       u8 maxVol, u8 minVol, u8 studio, SND_PARAMETER_INFO* para) {
  if (sndActive) {
    return AddEmitter(em_buffer, pos, dir, maxDis, comp, flags, fxid, groupid, maxVol, minVol, NULL,
                      para, studio);
  }
  return -1;
}

bool sndRemoveEmitter(SND_EMITTER* em) {
  if (sndActive) {
    hwDisableIrq();
    if (em->flags & 0x10000) {
      EmitterShutdown(em);
    }

    hwEnableIrq();
    return TRUE;
  }

  return FALSE;
}

SND_VOICEID sndEmitterVoiceID(SND_EMITTER* em) {
  SND_VOICEID ret; // r31

  ret = 0xffffffff;
  if (sndActive != FALSE) {
    hwDisableIrq();
    if ((em->flags & 0x10000) != 0) {
      ret = em->vid;
    }
    hwEnableIrq();
  }
  return ret;
}

void s3dKillAllEmitter() {
  struct SND_EMITTER* em;  // r31
  struct SND_EMITTER* nem; // r30

  em = s3dEmitterRoot;
  while (em != NULL) {
    nem = em->next;
    sndRemoveEmitter(em);
    em = nem;
  }
}

void s3dKillEmitterByFXID(FX_TAB* fxTab, unsigned long num) {
  struct SND_EMITTER* em;  // r31
  struct SND_EMITTER* nem; // r29
  unsigned long j;         // r30

  for (em = s3dEmitterRoot; em != NULL; em = nem) {
    nem = em->next;
    for (j = 0; j < num; ++j) {
      if (em->fxid == fxTab[j].id) {
        sndRemoveEmitter(em);
        break;
      }
    }
  }
}

static void MakeListenerMatrix(SND_LISTENER* li) {
  struct SND_FMATRIX mat; // r1+0xC
  salCrossProduct(&li->right, &li->heading, &li->up);
  mat.m[0][0] = li->right.x;
  mat.m[1][0] = li->right.y;
  mat.m[2][0] = li->right.z;
  mat.m[0][1] = li->up.x;
  mat.m[1][1] = li->up.y;
  mat.m[2][1] = li->up.z;
  mat.m[0][2] = -li->heading.x;
  mat.m[1][2] = -li->heading.y;
  mat.m[2][2] = -li->heading.z;
  mat.t[0] = li->pos.x;
  mat.t[1] = li->pos.y;
  mat.t[2] = li->pos.z;
  salInvertMatrix(&li->mat, &mat);
}

bool sndUpdateListener(SND_LISTENER* li, SND_FVECTOR* pos, SND_FVECTOR* dir, SND_FVECTOR* heading,
                       SND_FVECTOR* up, u8 vol, SND_ROOM* room) {
  if (sndActive) {
    hwDisableIrq();
    li->pos = *pos;
    li->dir = *dir;
    li->heading = *heading;
    li->up = *up;

    MakeListenerMatrix(li);
    li->vol = vol / 127.f;

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
    if (room != li->room) {
      if (li->room != NULL) {
        RemoveListenerFromRoom(li->room);
      }

      li->room = room;
      if (room != NULL) {
        AddListener2Room(li->room);
      }
    }
#endif

    hwEnableIrq();
    return TRUE;
  }

  return FALSE;
}

bool sndAddListenerEx(SND_LISTENER* li, SND_FVECTOR* pos, SND_FVECTOR* dir, SND_FVECTOR* heading,
                      SND_FVECTOR* up, f32 front_sur, f32 back_sur, f32 soundSpeed,
                      f32 volPosOffset, u32 flags, u8 vol,

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
                      SND_ROOM* room
#else
                      const SND_LISTENER_EXPARAMETER* exPara
#endif
) {

  if (sndActive) {
    hwDisableIrq();
    if ((li->next = s3dListenerRoot) != NULL) {
      s3dListenerRoot->prev = li;
    }

    li->prev = NULL;
    s3dListenerRoot = li;
    li->pos = *pos;
    li->dir = *dir;
    li->heading = *heading;
    li->up = *up;
    li->surroundDisFront = front_sur;
    li->surroundDisBack = back_sur;
    li->soundSpeed = soundSpeed;
    li->volPosOff = volPosOffset;
    MakeListenerMatrix(li);
    li->flags = flags;
    li->vol = vol / 127.f;
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
    li->room = room;
    if (room != NULL) {
      AddListener2Room(room);
    }
#else
    if (exPara != NULL) {
      li->oneMeter = exPara->oneMeter;
    } else {
      li->oneMeter = 1.f;
    }
#endif
    hwEnableIrq();
    return TRUE;
  }

  return FALSE;
}

bool sndAddListener(SND_LISTENER* li, SND_FVECTOR* pos, SND_FVECTOR* dir, SND_FVECTOR* heading,
                    SND_FVECTOR* up, f32 front_sur, f32 back_sur, f32 soundSpeed, u32 flags, u8 vol,
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
                    SND_ROOM* room
#else
                    const SND_LISTENER_EXPARAMETER* exPara
#endif
) {
  return sndAddListenerEx(li, pos, dir, heading, up, front_sur, back_sur, soundSpeed, 0.f, flags,
                          vol,
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
                          room
#else
                          exPara
#endif
  );
}

bool sndRemoveListener(SND_LISTENER* li) {
  if (sndActive) {

    hwDisableIrq();

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
    if (li->room != NULL) {
      RemoveListenerFromRoom(li->room);
    }
#endif

    if (li->next != NULL) {
      li->next->prev = li->prev;
    }

    if (li->prev != NULL) {
      li->prev->next = li->next;
    } else {
      s3dListenerRoot = li->next;
    }
    hwEnableIrq();
    return TRUE;
  }

  return FALSE;
}

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) //
void sndSet3DEmitterOcclusionCallback(SND_S3D_OCCLUSION_CALLBACK callback) {
  s3dOcclusionCallback = callback;
}
#endif

void ClearStartList() {
  startGroupNum = 0;
  startListNumnum = 0;
  runListNum = 0;
}

void AddRunningEmitter(SND_EMITTER* em, f32 vol) {
  long i;        // r30
  RUN_LIST* rl;  // r29
  RUN_LIST* lrl; // r28

  for (i = 0; i < startGroupNum; ++i) {
    if (em->group == startGroup[i].id) {
      break;
    }
  }

  if (i == startGroupNum) {
    startGroup[i].list = NULL;
    startGroup[i].running = NULL;
    startGroup[i].numRunning = 0;
    startGroup[i].id = em->group;
    ++startGroupNum;
  }

  ++startGroup[i].numRunning;

  lrl = NULL;
  for (rl = startGroup[i].running; rl != NULL; rl = rl->next) {
    if (rl->vol > vol) {
      break;
    }
    lrl = rl;
  }

  if (lrl == NULL) {
    startGroup[i].running = &runList[runListNum];
  } else {
    lrl->next = &runList[runListNum];
  }

  runList[runListNum].next = rl;
  runList[runListNum].em = em;
  runList[runListNum++].vol = vol;
}

bool AddStartingEmitter(SND_EMITTER* em, f32 vol, f32 xPan, f32 yPan, f32 zPan, f32 pitch
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) //
                        ,
                        float lpfFactor
#endif
) {
  long i;         // r30
  START_LIST* sl; // r29

  for (i = 0; i < startGroupNum; ++i) {
    if (em->group == startGroup[i].id) {
      break;
    }
  }

  if (i == startGroupNum) {
    if (startGroupNum == 64) {
      return FALSE;
    }

    startGroup[i].list = NULL;
    startGroup[i].running = NULL;
    startGroup[i].numRunning = 0;
    startGroup[i].id = em->group;
    ++startGroupNum;
  }

  if (startListNumnum == 64) {
    return FALSE;
  }

  sl = startGroup[i].list;

  if (sl != NULL) {
    for (; sl->next != NULL; sl = sl->next) {
      if (sl->vol < vol) {
        break;
      }
    }
    startListNum[startListNumnum].next = sl->next;
    sl->next = &startListNum[startListNumnum];
  } else {
    startListNum[startListNumnum].next = startGroup[i].list;
    startGroup[i].list = &startListNum[startListNumnum];
  }

  startListNum[startListNumnum].em = em;
  startListNum[startListNumnum].pitch = pitch;
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) //
  startListNum[startListNumnum].lpfFactor = lpfFactor;
#endif
  startListNum[startListNumnum].xPan = xPan;
  startListNum[startListNumnum].yPan = yPan;
  startListNum[startListNumnum].zPan = zPan;
  startListNum[startListNumnum++].vol = vol;

  return TRUE;
}

void StartContinousEmitters() {
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  if (s3dUseLegacyLogic != 0) {
#endif
    long i;          // r26
    START_LIST* sl;  // r29
    SND_EMITTER* em; // r31
    f32 dv;          // r63

    for (i = 0; i < startGroupNum; ++i) {
      for (sl = startGroup[i].list; sl != NULL; sl = sl->next) {
        if ((startGroup[i].running != NULL) &&
            !(((s3dUseMaxVoices != '\0' && ((startGroup[i].id & 0x80000000) != 0)) &&
               (startGroup[i].numRunning < startGroup[i].list->em->maxVoices)))) {

          dv = sl->vol - (startGroup[i].running)->vol;
          if (dv <= 0.08f) {
            continue;
          } else if (dv <= 0.15f) {
            if (++sl->em->VolLevelCnt < 20) {
              continue;
            }
          } else {
            sl->em->VolLevelCnt = 0;
          }
        }
        em = sl->em;

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
        if (em->room != NULL && em->room->studio == 0xFF) {
          goto set_flags;
        }
#endif
        if (
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
            (em->vid =
                 synthFXStart(em->fxid, 127, 64, em->room != NULL ? em->room->studio : em->studio,
                              (em->flags & 0x10) != 0))
#elif MUSY_VERSION == MUSY_VERSION_CHECK(2, 0, 1)
          (em->vid = synthFXStart(em->fxid, 127, 64, em->studio, (em->flags & 0x10) != 0))
#else
          (em->vid = synthFXStart(em->fxid, GetFXParameterKey(em), 127, 64, em->studio,
                                  (em->flags & 0x10) != 0))
#endif
            == -1) {
        set_flags:
          if (!(em->flags & 0x2)) {
            em->flags |= 0x40000;
            em->flags &= ~0x20000;
          }
        } else {
          if (!(em->flags & 0x20)) {
            em->flags |= 0x100000;
            em->fade = 0.f;
          } else {
            em->fade = 1.f;
          }
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
          SetFXParameters(em, sl->vol, sl->xPan, sl->yPan, sl->zPan, sl->pitch);
#else
        SetFXParameters(em, sl->vol, sl->xPan, sl->yPan, sl->zPan, sl->pitch, sl->lpfFactor);
#endif
          em->flags &= ~0x20000;
          ++startGroup[i].numRunning;
          if (startGroup[i].running != NULL) {
            startGroup[i].running = startGroup[i].running->next;
          }
        }
      }
    }
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  } else {
    long i;          // r27
    START_LIST* sl;  // r31
    SND_EMITTER* em; // r30
    f32 dv;          // f31
    RUN_LIST* rl;    // r25
    RUN_LIST* lrl;   // r24
    for (i = 0; i < startGroupNum; i++) {
      for (sl = startGroup[i].list; sl != NULL; sl = sl->next) {
        if ((startGroup[i].running != NULL) &&
            ((s3dUseMaxVoices == 0) || (synthCheckFXRealloc(startGroup[i].list->em->fxid) != 0))) {
          dv = sl->vol - startGroup[i].running->vol;
          if (dv <= 0.08f) {
            continue;
          }
          if (dv <= 0.15f) {
            if (++sl->em->VolLevelCnt < 0x14) {
              continue;
            } else {
              goto block_35;
            }
          } else {
            sl->em->VolLevelCnt = 0;
          block_35:
            voiceKillSound(startGroup[i].running->em->vid);
            startGroup[i].running = startGroup[i].running->next;
            --startGroup[i].numRunning;
          }
        }
        em = sl->em;
        if (
#if MUSY_VERSION == MUSY_VERSION_CHECK(2, 0, 1)
            (em->vid = synthFXStart(em->fxid, 127, 64, em->studio, (em->flags & 0x10) != 0))
#else

            (em->vid = synthFXStart(em->fxid, GetFXParameterKey(em), 127, 64, em->studio,
                                    (em->flags & 0x10) != 0))
#endif

            == -1) {
          if ((em->flags & 2) == 0) {
            em->flags |= 0x40000;
            em->flags &= ~0x20000;
          }
        } else {
          if ((em->flags & 0x20) == 0) {
            em->flags |= 0x100000;
            em->fade = 0.f;
          } else {
            em->fade = 1.f;
          }
          SetFXParameters(em, sl->vol, sl->xPan, sl->yPan, sl->zPan, sl->pitch, sl->lpfFactor);
          em->flags &= ~0x20000;
          lrl = NULL;
          for (rl = startGroup[i].running; rl != NULL; rl = rl->next) {
            if (rl->vol >= sl->vol) {
              runList[runListNum].em = em;
              runList[runListNum].vol = sl->vol;
              runList[runListNum].next = rl;
              if (lrl == NULL) {
                startGroup[i].running = &runList[runListNum];
              } else {
                lrl->next = &runList[runListNum];
              }
              ++startGroup[i].numRunning;
              runListNum++;
              break;
            }
          }
        }
      }
    }
  }
#endif
}

void s3dHandle() {
  SND_EMITTER* em;  // r31
  SND_EMITTER* nem; // r30
  f32 vol;          // r1+0x18
  f32 xPan;         // r1+0x14
  f32 yPan;         // r1+0x10
  f32 zPan;         // r1+0xC
  f32 pitch;        // r1+0x8
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  float lpfFactor; // r1+0x8
#endif

  if (s3dCallCnt != 0) {
    --s3dCallCnt;
    return;
  }
  s3dCallCnt = 3;
  ClearStartList();
  em = s3dEmitterRoot;
  for (; em != NULL; em = nem) {
    nem = em->next;
    if ((em->flags & 0x40000) != 0) {
      EmitterShutdown(em);
      continue;
    }
    if ((em->flags & 0x20001) != 0) {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
      CalcEmitter(em, &vol, &pitch, &xPan, &yPan, &zPan);
#else
      CalcEmitter(em, &vol, &pitch, &xPan, &yPan, &zPan, &lpfFactor);
#endif
    }

    if (!(em->flags & 0x80000)) {
      if (em->flags & 0x20000) {
        if (vol == 0.f && em->flags & 0x4) {
          em->flags |= 0x80000;
          em->flags &= ~0x20000;
          goto found_emitter;
        } else if (vol == 0.f && em->flags & 0x40) {
          EmitterShutdown(em);
          continue;
        }

        if (em->flags & 1) {
          if (AddStartingEmitter(em, vol, xPan, yPan, zPan, pitch
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
                                 ,
                                 lpfFactor
#endif
                                 )) {
            continue;
          }
        }
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) //
        else if (em->room == NULL || em->room->studio != 0xFF) {
          if ((em->vid =
                   synthFXStart(em->fxid, 127, 64, em->room != NULL ? em->room->studio : em->studio,
                                (em->flags & 0x10) != 0)) == -1) {
          derp:
            if (!(em->flags & 2)) {
              em->flags |= 0x40000;
              em->flags &= ~0x20000;
            } else {
              continue;
            }
          }
        } else {
          goto derp;
        }
#else
        else {
          if ((em->vid = synthFXStart(em->fxid,
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
                                      GetFXParameterKey(em),
#endif
                                      127, 64, em->studio, (em->flags & 0x10) != 0)) == -1) {
          derp:
            if (!(em->flags & 2)) {
              em->flags |= 0x40000;
              em->flags &= ~0x20000;
            } else {
              continue;
            }
          }
        }
#endif
      } else if ((em->vid = sndFXCheck(em->vid)) == -1) {
        if ((em->flags & 2)) {
          em->flags |= 0x20000;
        } else {
          em->flags |= 0x40000;
        }
      }

    found_emitter:
      if (em->vid != -1) {
        if ((em->flags & 1) != 0) {
          AddRunningEmitter(em, vol);
        }
        if ((vol == 0.f) && ((em->flags & 4) != 0)) {
          synthSendKeyOff(em->vid);
          em->vid = 0xffffffff;
          if ((em->flags & 2)) {
            em->flags |= 0x80000;
          } else {
            em->flags |= 0x40000;
          }
        } else {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
          SetFXParameters(em, vol, xPan, yPan, zPan, pitch);
#else
          SetFXParameters(em, vol, xPan, yPan, zPan, pitch, lpfFactor);
#endif
        }
      }
      if ((em->flags & 0x100000) != 0) {
        em->fade += .3f;
        if (em->fade >= 1.f) {
          em->flags &= ~0x100000;
        }
      }
    } else if (
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) //
        (em->room == NULL || (em->room != NULL && em->room->studio != 0xff)) &&
#endif
        vol != 0.f) {
      em->flags &= ~0x80000;
      em->flags |= 0x20000;
    }
  }
  StartContinousEmitters();
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) //
  CheckRoomStatus();
  CheckDoorStatus();
#endif
}

void sndSetup3DStudios(unsigned char first, unsigned char num) {
  snd_base_studio = first;
  snd_max_studios = num;
}

void sndGet3DParameters(SND_3DINFO* info, SND_FVECTOR* pos, SND_FVECTOR* dir, f32 maxDis, f32 comp,
                        u8 maxVol, u8 minVol
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) //
                        ,
                        SND_ROOM* room
#endif
) {
  f32 xPan;  // r1+0x34
  f32 yPan;  // r1+0x30
  f32 zPan;  // r1+0x2C
  f32 cvol;  // r1+0x28
  f32 pitch; // r1+0x24
  static SND_EMITTER em;
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  float lpfFactor; // r1+0x14
#endif

  hwDisableIrq();

  em.flags = 8;
  em.pos = *pos;
  em.dir = *dir;
  em.maxDis = maxDis;
  em.maxVol = maxVol / 127.f;
  em.minVol = minVol / 127.f;
  em.volPush = comp;
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) //
  em.room = room;
#endif

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0) //
  CalcEmitter(&em, &cvol, &pitch, &xPan, &yPan, &zPan);
#else
  CalcEmitter(&em, &cvol, &pitch, &xPan, &yPan, &zPan, &lpfFactor);
#endif
  info->vol = clip127(cvol * 127.f);
  info->pan = clip127((xPan + 1.f) * 64.f);
  info->span = clip127((1.f - zPan) * 64.f);
  info->doppler = clip3FFF(pitch * 8192.f);
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1) //
  info->lpfFactor = clip3FFF(pitch * 16383.f);
#endif

  hwEnableIrq();
}

void s3dInit(u32 flags) {
  s3dEmitterRoot = NULL;
  s3dListenerRoot = NULL;
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
  s3dRoomRoot = 0;
  s3dDoorRoot = 0;
#endif
  snd_used_studios = 0;
  snd_base_studio = 1;
  snd_max_studios = 3;
  s3dCallCnt = 0;
  s3dUseMaxVoices = ((flags & 2) != 0);
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  s3dUseLegacyLogic = ((flags & 4) != 0);
#endif
}

void s3dExit() {}
