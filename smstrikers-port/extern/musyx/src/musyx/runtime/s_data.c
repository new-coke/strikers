
#include "musyx/assert.h"
#include "musyx/hardware.h"
#include "musyx/s3d.h"
#include "musyx/seq.h"
#include "musyx/synth.h"
#include "musyx/synthdata.h"

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
static GSTACK gs[128];
#define GS_CURRENT gs
#define GS_GSI gs
static s16 sp;
#define SP_CURRENT sp
#define SP_GSI sp
#else
static GSTACK_INST gsDefault;
#define GS_CURRENT gsCurrent->gs
#define GS_GSI gsi->gs
#define SP_CURRENT gsCurrent->sp
#define SP_GSI gsi->sp
#endif

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
#if MUSY_VERSION == MUSY_VERSION_CHECK(2, 0, 3)
static GSTACK_INST* gsRoot;
static GSTACK_INST* gsCurrent;
static unsigned long gsNextID;
#else
static unsigned long gsNextID;
static GSTACK_INST* gsCurrent;
static GSTACK_INST* gsRoot;
#endif
static void dataInitStackInstance(GSTACK_INST* inst, unsigned long id, unsigned long aramBase,
                                  unsigned long aramSize) {
  inst->id = id;
  inst->sp = 0;
  inst->aramInfo.aramBase = aramBase;
  inst->aramInfo.aramWrite = aramBase;
  inst->aramInfo.aramTop = aramBase + aramSize;
  inst->next = gsRoot;
  gsRoot = inst;
}
#endif

void dataInitStack(
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
    unsigned long aramBase, unsigned long aramSize
#endif
) {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
  sp = 0;
#else
  gsRoot = NULL;
  dataInitStackInstance(&gsDefault, -2, aramBase, aramSize);
  gsNextID = 0;
  gsCurrent = &gsDefault;
#endif
}

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
ARAMInfo* dataARAMGetInfo() { return &gsCurrent->aramInfo; }

ARAMInfo* dataARAMDefaultGetInfo() { return &gsDefault.aramInfo; }
#endif

static MEM_DATA* GetPoolAddr(u16 id, MEM_DATA* m) {
  while (m->nextOff != 0xFFFFFFFF) {
    if (m->id == id) {
      return m;
    }

    m = (MEM_DATA*)((u8*)m + m->nextOff);
  }
  return NULL;
}

static MEM_DATA* GetMacroAddr(u16 id, POOL_DATA* pool) {
  return pool == NULL ? NULL : GetPoolAddr(id, (MEM_DATA*)((u8*)pool + pool->macroOff));
}

static MEM_DATA* GetCurveAddr(u16 id, POOL_DATA* pool) {
  return pool == NULL ? NULL : GetPoolAddr(id, (MEM_DATA*)((u8*)pool + pool->curveOff));
}
static MEM_DATA* GetKeymapAddr(u16 id, POOL_DATA* pool) {
  return pool == NULL ? NULL : GetPoolAddr(id, (MEM_DATA*)((u8*)pool + pool->keymapOff));
}
static MEM_DATA* GetLayerAddr(u16 id, POOL_DATA* pool) {
  return pool == NULL ? NULL : GetPoolAddr(id, (MEM_DATA*)((u8*)pool + pool->layerOff));
}

static void InsertData(u16 id, void* data, u8 dataType, u32 remove) {
  MEM_DATA* m; // r30

  switch (dataType) {
  case 0:
    if (!remove) {
      if ((m = GetMacroAddr(id, data)) != NULL) {
        dataInsertMacro(id, &m->data.cmd);

      } else {
        dataInsertMacro(id, NULL);
      }
    } else {
      dataRemoveMacro(id);
    }
    break;
  case 2: {
    id |= 0x4000;
    if (!remove) {
      if ((m = GetKeymapAddr(id, data)) != NULL) {
        dataInsertKeymap(id, &m->data.map);
      } else {
        dataInsertKeymap(id, NULL);
      }
    } else {
      dataRemoveKeymap(id);
    }
  } break;
  case 3: {
    id |= 0x8000;
    if (!remove) {
      if ((m = GetLayerAddr(id, data)) != NULL) {
        dataInsertLayer(id, &m->data.layer.entry, m->data.layer.num);
      } else {
        dataInsertLayer(id, NULL, 0);
      }
    } else {
      dataRemoveLayer(id);
    }
  } break;
  case 4:
    if (!remove) {
      if ((m = GetCurveAddr(id, data)) != NULL) {
        dataInsertCurve(id, &m->data.tab);
      } else {
        dataInsertCurve(id, NULL);
      }
    } else {
      dataRemoveCurve(id);
    }
    break;
  case 1:
    if (!remove) {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
      dataAddSampleReference(id);
#else
      dataAddSampleReference(id, &gsCurrent->aramInfo);
#endif
    } else {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
      dataRemoveSampleReference(id);
#else
      dataRemoveSampleReference(id, &gsCurrent->aramInfo);
#endif
    }
    break;
  }
}

static void ScanIDList(u16* ref, void* data, u8 dataType, u32 remove) {
  u16 id; // r30

  while (*ref != 0xFFFF) {
    if ((*ref & 0x8000)) {
      id = *ref & 0x3fff;
      while (id <= ref[1]) {
        InsertData(id, data, dataType, remove);
        ++id;
      }
      ref += 2;

    } else {
      InsertData(*ref++, data, dataType, remove);
    }
  }
}

static void ScanIDListReverse(u16* refBase, void* data, u8 dataType, u32 remove) {
  s16 id;
  u16* ref;

  if (*refBase != 0xffff) {
    ref = refBase;
    while (*ref != 0xffff) {
      ref++;
    }
    ref--;

    while (ref >= refBase) {
      if (ref != refBase) {
        if ((ref[-1] & 0x8000) != 0) {
          id = *ref;
          while (id >= (s16)(ref[-1] & 0x3fff)) {
            InsertData(id, data, dataType, remove);
            id--;
          }
          ref -= 2;
        } else {
          InsertData(*ref, data, dataType, remove);
          ref--;
        }
      } else {
        InsertData(*ref, data, dataType, remove);
        ref--;
      }
    }
  }
}

static void InsertMacros(unsigned short* ref, void* pool) { ScanIDList(ref, pool, 0, 0); }

static void InsertCurves(unsigned short* ref, void* pool) { ScanIDList(ref, pool, 4, 0); }

static void InsertKeymaps(unsigned short* ref, void* pool) { ScanIDList(ref, pool, 2, 0); }

static void InsertLayers(unsigned short* ref, void* pool) { ScanIDList(ref, pool, 3, 0); }

static void RemoveMacros(unsigned short* ref) { ScanIDList(ref, NULL, 0, 1); }

static void RemoveCurves(unsigned short* ref) { ScanIDList(ref, NULL, 4, 1); }

static void RemoveKeymaps(unsigned short* ref) { ScanIDList(ref, NULL, 2, 1); }

static void RemoveLayers(unsigned short* ref) { ScanIDList(ref, NULL, 3, 1); }

static void InsertSamples(u16* ref, void* samples, void* sdir) {
  samples = hwTransAddr(samples);
  if (dataInsertSDir((SDIR_DATA*)sdir, samples)) {
    ScanIDList(ref, sdir, 1, 0);
  }
}

static void RemoveSamples(unsigned short* ref, void* sdir) {
  ScanIDListReverse(ref, NULL, 1, 1);
  dataRemoveSDir(sdir);
}

static void InsertFXTab(unsigned short gid, FX_DATA* fd) { dataInsertFX(gid, fd->fx, fd->num); }

static void RemoveFXTab(unsigned short gid) { dataRemoveFX(gid); }

void sndSetSampleDataUploadCallback(void* (*callback)(u32, u32), u32 chunckSize) {
  hwSetSaveSampleCallback(callback, chunckSize);
}

bool sndPushGroup(void* prj_data, u16 gid, void* samples, void* sdir, void* pool) {
  GROUP_DATA* g; // r31
  MUSY_ASSERT_MSG(prj_data != NULL, "Project data pointer is NULL");
  MUSY_ASSERT_MSG(sdir != NULL, "Sample directory pointer is NULL");

  if (sndActive && SP_CURRENT < 128) {
    g = prj_data;

    while (g->nextOff != 0xFFFFFFFF) {
      if (g->id == gid) {
        GS_CURRENT[SP_CURRENT].gAddr = g;
        GS_CURRENT[SP_CURRENT].prjAddr = prj_data;
        GS_CURRENT[SP_CURRENT].sdirAddr = sdir;
        InsertSamples((u16*)((u8*)prj_data + g->sampleOff), samples, sdir);
        InsertMacros((u16*)((u8*)prj_data + g->macroOff), pool);
        InsertCurves((u16*)((u8*)prj_data + g->curveOff), pool);
        InsertKeymaps((u16*)((u8*)prj_data + g->keymapOff), pool);
        InsertLayers((u16*)((u8*)prj_data + g->layerOff), pool);
        if (g->type == 1) {
          InsertFXTab(gid, (FX_DATA*)((u8*)prj_data + g->data.song.normpageOff));
        }
        hwSyncSampleMem();
        ++SP_CURRENT;
        return TRUE;
      }

      g = (GROUP_DATA*)((u8*)prj_data + g->nextOff);
    }
  }

  MUSY_DEBUG("Group ID=%d could not be pushed.\n", gid);
  return FALSE;
}

/*










*/
bool sndPopGroup() {
  GROUP_DATA* g;
  SDIR_DATA* sdir;
  void* prj;
  FX_DATA* fd;

  MUSY_ASSERT_MSG(sndActive != FALSE, "Sound system is not initialized.");
  MUSY_ASSERT_MSG(SP_CURRENT != 0, "Soundstack is empty.");
  g = GS_CURRENT[--SP_CURRENT].gAddr;
  prj = GS_CURRENT[SP_CURRENT].prjAddr;
  sdir = GS_CURRENT[SP_CURRENT].sdirAddr;
  hwDisableIrq();

  if (g->type == 1) {
    fd = (FX_DATA*)((u8*)prj + g->data.song.normpageOff);
    s3dKillEmitterByFXID(fd->fx, fd->num);
  } else {
    seqKillInstancesByGroupID(g->id);
  }

  synthKillVoicesByMacroReferences((u16*)((u8*)prj + g->macroOff));
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  synthKillVoicesBySampleReferences((u16*)((u8*)prj + g->sampleOff));
#endif
  hwEnableIrq();
  RemoveSamples((u16*)((u8*)prj + g->sampleOff), sdir);
  RemoveMacros((u16*)((u8*)prj + g->macroOff));
  RemoveCurves((u16*)((u8*)prj + g->curveOff));
  RemoveKeymaps((u16*)((u8*)prj + g->keymapOff));
  RemoveLayers((u16*)((u8*)prj + g->layerOff));
  if (g->type == 1) {
    RemoveFXTab(g->id);
  }
  return 1;
}

/*












*/

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
/* PORT: the literal was the console's sizeof; the pointers inside are wider here. */
u32 sndStackGetSize() { return sizeof(GSTACK_INST); }

u32 sndStackAdd(void* stackWorkMem, u32 aramBase, u32 aramSize) {
  GSTACK_INST* gs; // r31
  u32 id;          // r30

  MUSY_ASSERT_MSG(sndActive != FALSE, "Sound system is not initialized.");
  MUSY_ASSERT_MSG((aramBase & 31) == 0, "ARAM area base must be 32-byte aligned.");
  MUSY_ASSERT_MSG((aramSize & 31) == 0, "ARAM area size must be a multiple of 32 bytes.");
  do {
    for (;;) {
      id = gsNextID;
      gsNextID = id + 1;
      if (id == -2) {
        continue;
      }
      if (id == -1) {
        continue;
      }
      break;
    }
    for (gs = gsRoot; gs != NULL; gs = gs->next) {
      if (gs->id == id) {
        break;
      }
    }
  } while (gs != NULL);
  dataInitStackInstance(stackWorkMem, id, aramBase, aramSize);
  return id;
}

u32 sndStackRemove(u32 id) {
  GSTACK_INST* gs;  // r31
  GSTACK_INST* lgs; // r29

  MUSY_ASSERT_MSG(sndActive != FALSE, "Sound system is not initialized.");
  MUSY_ASSERT_MSG(id != -2, "Default sound stack cannot be removed.");

  for (lgs = NULL, gs = gsRoot; gs != NULL; lgs = gs, gs = gs->next) {
    if (gs->id == id) {
      MUSY_ASSERT_MSG(gs->sp == 0, "Sound stack is not empty.");
      if (lgs == NULL) {
        gsRoot = gs->next;
      } else {
        lgs->next = gs->next;
      }
      return 1;
    }
  }
  return 0;
}

u32 sndStackSetCurrent(u32 id) {
  GSTACK_INST* gs; // r31

  MUSY_ASSERT_MSG(sndActive != FALSE, "Sound system is not initialized.");
  for (gs = gsRoot; gs != NULL; gs = gs->next) {
    if (gs->id == id) {
      gsCurrent = gs;
      return 1;
    }
  }
  return 0;
}

u32 sndStackGetARAMAddressRange(u32 id, u32* start, u32* end) {
  GSTACK_INST* gs; // r31

  MUSY_ASSERT_MSG(sndActive != FALSE, "Sound system is not initialized.");
  for (gs = gsRoot; gs != NULL; gs = gs->next) {
    if (gs->id == id) {
      *start = gs->aramInfo.aramBase;
      *end = gs->aramInfo.aramTop;
      return 1;
    }
  }
  return 0;
}

u32 sndStackGetAvailableSampleMemory(unsigned long id) {
  GSTACK_INST* gs; // r31

  MUSY_ASSERT_MSG(sndActive != FALSE, "Sound system is not initialized.");
  for (gs = gsRoot; gs != NULL; gs = gs->next) {
    if (gs->id == id) {
      return hwGetAvailableSampleMemory(&gs->aramInfo);
    }
  }
  return 0;
}
#endif

u32 seqPlaySong(u16 sgid, u16 sid, void* arrfile, SND_PLAYPARA* para, u8 irq_call, u8 studio) {
  int i;
  GROUP_DATA* g;
  PAGE* norm;
  PAGE* drum;
  MIDISETUP* midiSetup;
  u32 seqId;
  void* prj;
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  GSTACK_INST* gsi;
#endif
  MUSY_ASSERT_MSG(sndActive != FALSE, "Sound system is not initialized.");

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  for (gsi = gsRoot; gsi != NULL; gsi = gsi->next) {
#endif
    for (i = 0; i < SP_GSI; ++i) {
      if (GS_GSI[i].gAddr->id != sgid) {
        continue;
      }

      if (GS_GSI[i].gAddr->type == 0) {
        g = GS_GSI[i].gAddr;
        prj = GS_GSI[i].prjAddr;
        norm = (PAGE*)((size_t)prj + g->data.song.normpageOff);
        drum = (PAGE*)((size_t)prj + g->data.song.drumpageOff);
        midiSetup = (MIDISETUP*)((size_t)prj + g->data.song.midiSetupOff);
        while (midiSetup->songId != 0xFFFF) {
          if (midiSetup->songId == sid) {
            if (irq_call != 0) {
              seqId = seqStartPlay(norm, drum, midiSetup, arrfile, para, studio, sgid);
            } else {
              hwDisableIrq();
              seqId = seqStartPlay(norm, drum, midiSetup, arrfile, para, studio, sgid);
              hwEnableIrq();
            }
            return seqId;
          }

          ++midiSetup;
        }

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 1)
        MUSY_DEBUG("Song ID=%d is not in group ID=%d.", sid, sgid);
#else
      MUSY_DEBUG("Song ID=%d is not in group ID=%d.\n", sid, sgid);
#endif
        return 0xffffffff;
      } else {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 1)
        MUSY_DEBUG("Group ID=%d is no songgroup.", sgid);
#else
      MUSY_DEBUG("Group ID=%d is no songgroup.\n", sgid);
#endif
        return 0xffffffff;
      }
    }
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  }
#endif

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
  MUSY_DEBUG("Group ID=%d is not on soundstack.", sgid);
#else
  MUSY_DEBUG("Group ID=%d is not on any soundstack.\n", sgid);
#endif
  return 0xffffffff;
}

#if MUSY_VERSION == MUSY_VERSION_CHECK(2, 0, 3)
/* PORT: static, so C99 emits it. */
static inline u32 _seqPlaySong(u16 sgid, u16 sid, void* arrfile, SND_PLAYPARA* para, u8 irq_call, u8 studio) {
  int i;
  GROUP_DATA* g;
  PAGE* norm;
  PAGE* drum;
  MIDISETUP* midiSetup;
  u32 seqId;
  void* prj;
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  GSTACK_INST* gsi;
#endif
  MUSY_ASSERT_MSG(sndActive != FALSE, "Sound system is not initialized.");

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  for (gsi = gsRoot; gsi != NULL; gsi = gsi->next) {
#endif
    for (i = 0; i < SP_GSI; ++i) {
      if (GS_GSI[i].gAddr->id != sgid) {
        continue;
      }

      if (GS_GSI[i].gAddr->type == 0) {
        g = GS_GSI[i].gAddr;
        prj = GS_GSI[i].prjAddr;
        norm = (PAGE*)((size_t)prj + g->data.song.normpageOff);
        drum = (PAGE*)((size_t)prj + g->data.song.drumpageOff);
        midiSetup = (MIDISETUP*)((size_t)prj + g->data.song.midiSetupOff);
        while (midiSetup->songId != 0xFFFF) {
          if (midiSetup->songId == sid) {
            if (irq_call != 0) {
              seqId = seqStartPlay(norm, drum, midiSetup, arrfile, para, studio, sgid);
            } else {
              hwDisableIrq();
              seqId = seqStartPlay(norm, drum, midiSetup, arrfile, para, studio, sgid);
              hwEnableIrq();
            }
            return seqId;
          }

          ++midiSetup;
        }

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 1)
        MUSY_DEBUG("Song ID=%d is not in group ID=%d.", sid, sgid);
#else
      MUSY_DEBUG("Song ID=%d is not in group ID=%d.\n", sid, sgid);
#endif
        return 0xffffffff;
      } else {
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 1)
        MUSY_DEBUG("Group ID=%d is no songgroup.", sgid);
#else
      MUSY_DEBUG("Group ID=%d is no songgroup.\n", sgid);
#endif
        return 0xffffffff;
      }
    }
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  }
#endif

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
  MUSY_DEBUG("Group ID=%d is not on soundstack.", sgid);
#else
  MUSY_DEBUG("Group ID=%d is not on any soundstack.\n", sgid);
#endif
  return 0xffffffff;
}
#endif


u32 sndSeqPlayEx(u16 sgid, u16 sid, void* arrfile, SND_PLAYPARA* para, u8 studio) {
#if MUSY_VERSION == MUSY_VERSION_CHECK(2, 0, 3)
  return _seqPlaySong(sgid, sid, arrfile, para, 0, studio);
#else
  return seqPlaySong(sgid, sid, arrfile, para, 0, studio);
#endif
}
