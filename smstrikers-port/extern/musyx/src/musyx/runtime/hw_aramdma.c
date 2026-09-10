#include "musyx/assert.h"
#include "musyx/hardware.h"
#include "musyx/musyx.h"
#include "musyx/platform.h"
#include "musyx/sal.h"
#include "musyx/synthdata.h"

// TODO: Platform specific rewrites

typedef struct STREAM_BUFFER {
  // total size: 0x10
  struct STREAM_BUFFER* next; // offset 0x0, size 0x4
  unsigned long aram;         // offset 0x4, size 0x4
  unsigned long length;       // offset 0x8, size 0x4
  unsigned long allocLength;  // offset 0xC, size 0x4
} STREAM_BUFFER;

#if MUSY_TARGET == MUSY_TARGET_DOLPHIN
#include <dolphin/ar.h>
#include <dolphin/arq.h>
#include <dolphin/os.h>

typedef struct ARAMTransferJob {
  // total size: 0x28
  ARQRequest arq;        // offset 0x0, size 0x20
  void (*callback)(u32); // offset 0x20, size 0x4
  unsigned long user;    // offset 0x24, size 0x4
} ARAMTransferJob;

typedef struct ARAMTransferQueue {
  // total size: 0x284
  ARAMTransferJob queue[16]; // offset 0x0, size 0x280
  vu8 write;                 // offset 0x280, size 0x1
  vu8 valid;                 // offset 0x281, size 0x1
} ARAMTransferQueue;

static u32 aramTop; // size: 0x4
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
static u32 aramWrite; // size: 0x4
#define ARAM_WRITE aramWrite
#else
#define ARAM_WRITE ai->aramWrite
#endif
static u32 aramStream;                        // size: 0x4
static void* (*aramUploadCallback)(u32, u32); // size: 0x4
static u32 aramUploadChunkSize;               // size: 0x4

static ARAMTransferQueue aramQueueLo;
static ARAMTransferQueue aramQueueHi;

static STREAM_BUFFER aramStreamBuffers[64];
static STREAM_BUFFER* aramUsedStreamBuffers;
static STREAM_BUFFER* aramFreeStreamBuffers;
static STREAM_BUFFER* aramIdleStreamBuffers;

static void InitStreamBuffers();

static void aramQueueInit() {
  aramQueueLo.write = aramQueueLo.valid = 0;
  aramQueueHi.write = aramQueueHi.valid = 0;
}

static void aramQueueCallback(unsigned long ptr) {
  u32 i;                        // r31
  ARQRequest* arq;              // r29
  ARAMTransferQueue* aramQueue; // r30

  arq = (ARQRequest*)ptr;
  if (arq->priority == 1) {
    aramQueue = &aramQueueHi;
  } else {
    aramQueue = &aramQueueLo;
  }

  for (i = 0; i < 16; ++i) {
    if (arq == &aramQueue->queue[i].arq && aramQueue->queue[i].callback) {
      aramQueue->queue[i].callback(aramQueue->queue[i].user);
    }
  }

  --aramQueue->valid;
}

void aramUploadData(void* mram, unsigned long aram, unsigned long len, unsigned long highPrio,
                    void (*callback)(size_t), unsigned long user) {
  ARAMTransferQueue* aramQueue; // r31
  int old;                      // r30

  aramQueue = highPrio != 0 ? &aramQueueHi : &aramQueueLo;

  for (;;) {
    old = OSDisableInterrupts();
    if (aramQueue->valid < 16) {
      aramQueue->queue[aramQueue->write].arq.owner = 42;
      aramQueue->queue[aramQueue->write].arq.type = 0;
      aramQueue->queue[aramQueue->write].arq.priority = highPrio != 0 ? 1 : 0;
      aramQueue->queue[aramQueue->write].arq.source = (u32)mram;
      aramQueue->queue[aramQueue->write].arq.dest = aram;
      aramQueue->queue[aramQueue->write].arq.length = len;
      aramQueue->queue[aramQueue->write].arq.callback = aramQueueCallback;
      aramQueue->queue[aramQueue->write].callback = callback;
      aramQueue->queue[aramQueue->write].user = user;
      ARQPostRequest(&aramQueue->queue[aramQueue->write].arq,
                     aramQueue->queue[aramQueue->write].arq.owner,
                     aramQueue->queue[aramQueue->write].arq.type,
                     aramQueue->queue[aramQueue->write].arq.priority,
                     aramQueue->queue[aramQueue->write].arq.source,
                     aramQueue->queue[aramQueue->write].arq.dest,
                     aramQueue->queue[aramQueue->write].arq.length,
                     aramQueue->queue[aramQueue->write].arq.callback);
      ++aramQueue->valid;
      aramQueue->write = (aramQueue->write + 1) % 16;
      OSRestoreInterrupts(old);
      return;
    }
    OSRestoreInterrupts(old);
  }
}

void aramSyncTransferQueue() {
  while (aramQueueLo.valid != 0) {
  }
}

void aramInit(unsigned long length) {
  signed short* tmpMem;   // r30
  unsigned long i;        // r29
  unsigned long aramBase; // r28
#line 173
  MUSY_ASSERT_MSG(length > sizeof(s16) * 640, "ARAM size is too small");

  aramBase = ARGetBaseAddress();

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 1)
  tmpMem = (s16*)salMalloc(sizeof(s16) * 640);
#else
  tmpMem = (s16*)salMallocPhysical(sizeof(s16) * 640);
#endif
#line 182
  MUSY_ASSERT_MSG(tmpMem != NULL, "Could not allocate temporary storage");

  for (i = 0; i < 640; ++i) {
    tmpMem[i] = 0;
  }

  DCFlushRange(tmpMem, sizeof(s16) * 640);
  aramQueueInit();
  aramUploadData(tmpMem, aramBase, sizeof(s16) * 640, 0, NULL, 0);
  aramSyncTransferQueue();
  salFree(tmpMem);
  aramTop = aramBase + length;
  if (aramTop > ARGetSize()) {
    aramTop = ARGetSize();
  }

#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
  aramWrite = aramBase + sizeof(s16) * 640;
#endif
  aramUploadCallback = NULL;
  InitStreamBuffers();
  MUSY_DEBUG("MusyX ARAM handler initialized\n");
}

void aramExit() {}

unsigned long aramGetZeroBuffer() { return ARGetBaseAddress(); }

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
unsigned long aramGetFirstUserAddress() { return ARGetBaseAddress() + 0x500; }

unsigned long aramGetUserBytes(unsigned long totalBytes) { return totalBytes - 0x500; }
#endif

void aramSetUploadCallback(void* (*callback)(unsigned long, unsigned long),
                           unsigned long chunckSize) {
  unsigned long acs; // r30

  if (callback != NULL) {
    chunckSize = (chunckSize + 31) & ~31;
    acs = ARQGetChunkSize();
    aramUploadChunkSize = chunckSize < acs ? acs : chunckSize;
  }

  aramUploadCallback = callback;
}

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
u32 aramGetAvailableBytes(ARAMInfo* ai) {
  if (ai->aramBase == aramGetFirstUserAddress()) {
    return aramStream - ai->aramWrite;
  }
  return ai->aramTop - ai->aramWrite;
}
#endif

void* aramStoreData(void* src, unsigned long len
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
                    ,
                    ARAMInfo* ai
#endif
) {
  unsigned long addr;    // r26
  void* buffer;          // r27
  unsigned long blkSize; // r30
  len = (len + 31) & ~31;
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
#line 266
  MUSY_ASSERT_MSG(aramWrite + len <= aramStream, "Data will not fit in remaining ARAM space");
#elif MUSY_VERSION == MUSY_VERSION_CHECK(2, 0, 1)
  MUSY_ASSERT_MSG(ai->aramWrite + len <= aramStream, "Data will not fit in remaining ARAM space");
  ARGetBaseAddress();
#else
  if (ai->aramBase == aramGetFirstUserAddress()) {
    MUSY_ASSERT_MSG(ai->aramWrite + len <= aramStream, "Data will not fit in remaining ARAM space");
    if ((ai->aramWrite + len) > aramStream) {
      return NULL;
    }
  } else {
    MUSY_ASSERT_MSG(ARAM_WRITE + len <= ai->aramTop, "Data will not fit in remaining ARAM space");
    if ((ai->aramWrite + len) > ai->aramTop) {
      return NULL;
    }
  }
#endif
  addr = ARAM_WRITE;
  if (aramUploadCallback == NULL) {
#line 276
    MUSY_ASSERT_MSG(!((u32)src & 31), "MRAM address is not aligned properly");
    DCFlushRange(src, len);
    aramUploadData(src, ARAM_WRITE, len, 0, NULL, 0);
    ARAM_WRITE += len;
    return (void*)addr;
  }

  while (len != 0) {
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
    aramSyncTransferQueue();
#endif
    blkSize = len >= aramUploadChunkSize ? aramUploadChunkSize : len;
    buffer = (void*)aramUploadCallback((u32)src, blkSize);
#line 297
    MUSY_ASSERT_MSG(!((u32)buffer & 31), "MRAM address is not aligned properly");
    DCFlushRange(buffer, blkSize);

    aramUploadData(buffer, ARAM_WRITE, blkSize, 0, NULL, 0);
    len -= blkSize;
    ARAM_WRITE += blkSize;
    src = (void*)((u32)src + blkSize);
  }

  return (void*)addr;
}

void aramRemoveData(void* aram, unsigned long len
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
                    ,
                    ARAMInfo* ai
#endif
) {
  len = (len + 31) & ~31;
  ARAM_WRITE -= len;
#line 328
  MUSY_ASSERT_MSG((u32)aram == ARAM_WRITE,
                  "Current ARAM address does not match originally allocated one");
}

static void InitStreamBuffers() {
  unsigned long i; // r31
  aramUsedStreamBuffers = NULL;
  aramFreeStreamBuffers = NULL;
  aramIdleStreamBuffers = aramStreamBuffers;
  for (i = 1; i < 64; ++i) {
    aramStreamBuffers[i - 1].next = &aramStreamBuffers[i];
  }
  aramStreamBuffers[i - 1].next = NULL;
  aramStream = aramTop;
}

unsigned char aramAllocateStreamBuffer(unsigned long len) {
  STREAM_BUFFER* sb;     // r30
  STREAM_BUFFER* oSb;    // r31
  STREAM_BUFFER* lastSb; // r28
  u32 minLen;            // r27

  len = (len + 31) & ~31;
  lastSb = oSb = NULL;
  minLen = -1;

  for (sb = aramFreeStreamBuffers; sb != NULL; sb = sb->next) {
    if (sb->allocLength == len) {
      oSb = sb;
      break;
    }

    if (sb->allocLength > len && minLen > sb->allocLength) {
      oSb = sb;
      minLen = sb->allocLength;
    }
    lastSb = sb;
  }

  if (oSb == NULL) {
    if (aramIdleStreamBuffers != NULL && aramStream - len >=
#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 0)
                                             aramWrite
#else
                                             dataARAMDefaultGetInfo()->aramWrite
#endif
    ) {
      oSb = aramIdleStreamBuffers;
      aramIdleStreamBuffers = oSb->next;
      oSb->allocLength = len;
      oSb->length = len;
      aramStream -= len;
      oSb->aram = aramStream;
      oSb->next = aramUsedStreamBuffers;
      aramUsedStreamBuffers = oSb;
    }
  } else {
    if (lastSb != NULL) {
      lastSb->next = oSb->next;
    } else {
      aramFreeStreamBuffers = oSb->next;
    }

    oSb->length = len;
    oSb->next = aramUsedStreamBuffers;
    aramUsedStreamBuffers = oSb;
  }

  if (oSb == NULL) {
    MUSY_DEBUG("No stream buffer slots available or ARAM.\n\n");
    return 0xFF;
  }

  return (oSb - aramStreamBuffers);
}

unsigned long aramGetStreamBufferAddress(unsigned char id, unsigned long* len) {
#line 467
  MUSY_ASSERT_MSG(id != 0xFF, "Stream buffer ID is invalid");

  if (len != NULL) {
    *len = aramStreamBuffers[id].length;
  }

  return aramStreamBuffers[id].aram;
}

void aramFreeStreamBuffer(unsigned char id) {
  struct STREAM_BUFFER* fSb;    // r30
  struct STREAM_BUFFER* sb;     // r31
  struct STREAM_BUFFER* lastSb; // r29
  struct STREAM_BUFFER* nextSb; // r27
  unsigned long minAddr;        // r28

  MUSY_ASSERT_MSG(id != 0xFF, "Stream buffer ID is invalid");
  fSb = &aramStreamBuffers[id];
  lastSb = NULL;
  sb = aramUsedStreamBuffers;

  while (sb != NULL) {
    if (sb == fSb) {
      if (lastSb != NULL) {
        lastSb->next = fSb->next;
      } else {
        aramUsedStreamBuffers = fSb->next;
      }
      break;
    } else {
      lastSb = sb;
      sb = sb->next;
    }
  }

  if (fSb->aram == aramStream) {
    fSb->next = aramIdleStreamBuffers;
    aramIdleStreamBuffers = fSb;
    minAddr = -1;
    sb = aramUsedStreamBuffers;
    while (sb != NULL) {
      if (sb->aram <= minAddr) {
        minAddr = sb->aram;
      }
      sb = sb->next;
    }

    lastSb = NULL;
    sb = aramFreeStreamBuffers;
    while (sb != NULL) {
      nextSb = sb->next;
      if (sb->aram < minAddr) {
        if (lastSb != NULL) {
          lastSb->next = sb->next;
        } else {
          aramFreeStreamBuffers = sb->next;
        }

        sb->next = aramIdleStreamBuffers;
        aramIdleStreamBuffers = sb;
      }
      sb = nextSb;
    }

    aramStream = minAddr != -1 ? minAddr : aramTop;
    return;
  }
  fSb->next = aramFreeStreamBuffers;
  aramFreeStreamBuffers = fSb;
}

#elif MUSY_TARGET == MUSY_TARGET_PC
// typedef struct ARAMTransferJob {
//   // total size: 0x28
//   ARQRequest arq;                  // offset 0x0, size 0x20
//   void (*callback)(unsigned long); // offset 0x20, size 0x4
//   unsigned long user;              // offset 0x24, size 0x4
// } ARAMTransferJob;
//
// typedef struct ARAMTransferQueue {
//   // total size: 0x284
//   ARAMTransferJob queue[16]; // offset 0x0, size 0x280
//   vu8 write;                 // offset 0x280, size 0x1
//   vu8 valid;                 // offset 0x281, size 0x1
// } ARAMTransferQueue;
//
// typedef struct STREAM_BUFFER {
//   // total size: 0x10
//   struct STREAM_BUFFER* next; // offset 0x0, size 0x4
//   unsigned long aram;         // offset 0x4, size 0x4
//   unsigned long length;       // offset 0x8, size 0x4
//   unsigned long allocLength;  // offset 0xC, size 0x4
// } STREAM_BUFFER;
//
// static unsigned long aramTop;                                     // size: 0x4
// static unsigned long aramWrite;                                   // size: 0x4
// static unsigned long aramStream;                                  // size: 0x4
// static void* (*aramUploadCallback)(unsigned long, unsigned long); // size: 0x4
// static unsigned long aramUploadChunkSize;                         // size: 0x4
//
// static ARAMTransferQueue aramQueueLo;
// static ARAMTransferQueue aramQueueHi;

// static STREAM_BUFFER aramStreamBuffers[64];
// static STREAM_BUFFER* aramUsedStreamBuffers;
// static STREAM_BUFFER* aramFreeStreamBuffers;
// static STREAM_BUFFER* aramIdleStreamBuffers;

static void InitStreamBuffers();

static void aramQueueInit() {}

static void aramQueueCallback(unsigned long ptr) {}

/* PORT: the host ARAM, in src/platform/musyx_aram.c. */
extern void PortAramInit(unsigned long length);
extern void PortAramUpload(const void* mram, unsigned long aram, unsigned long len);
extern void PortAramSetUploadCallback(void* (*callback)(u32, u32), unsigned long chunkSize);
extern void* PortAramStoreSample(const void* src, unsigned long len);
extern void PortAramFreeSample(void* p);
extern unsigned char PortAramAllocStreamBuffer(unsigned long len);
extern size_t PortAramStreamBufferAddress(unsigned char id, size_t* len);
extern void PortAramFreeStreamBuffer(unsigned char id);

/* PORT: the callback is void(*)(size_t) in musyx/hardware.h. unsigned long is the same type on LP64 and not on Windows. */
void aramUploadData(void* mram, unsigned long aram, unsigned long len, unsigned long highPrio,
                    void (*callback)(size_t), unsigned long user) {
  /* PORT: a memcpy rather than a DMA, and synchronous, so the completion callback fires here rather than from an interrupt. */
  PortAramUpload(mram, aram, len);
  if (callback != NULL) {
    callback(user);
  }
}

void aramSyncTransferQueue() {}

void aramInit(unsigned long length) { PortAramInit(length); }

void aramExit() {}

/* PORT: offset 0, which musyx_aram.c reserves and leaves zeroed. */
unsigned long aramGetZeroBuffer() { return 0; }

void aramSetUploadCallback(ARAMUploadCallback callback,
                           unsigned long chunckSize) {
  /* PORT: recorded for aramStoreData above; the console did the same. */
  PortAramSetUploadCallback(callback, chunckSize);
}

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
/* PORT: the console's first user address was the ARAM base plus 0x500, the zero buffer MusyX keeps at the bottom of ARAM. */
unsigned long aramGetFirstUserAddress(void) { return 0x500; }
unsigned long aramGetUserBytes(unsigned long totalBytes) { return totalBytes - 0x500; }
#endif

#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)
/* PORT: what the console computed from its own ARAM cursor, without the special case for the default stack. */
u32 aramGetAvailableBytes(ARAMInfo* ai) { return ai->aramTop - ai->aramWrite; }
#endif

/* PORT: the parameter list follows hardware.h's version gate, as the Dolphin definition above does. */
void* aramStoreData(void* src, unsigned long len
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
                    ,
                    ARAMInfo* ai
#endif
) {
  /* PORT: a host heap block, pulled through the game's upload callback as the console pulled it into ARAM. */
  void* p = PortAramStoreSample(src, len);
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  if (p != NULL && ai != NULL) {
    ai->aramWrite += (len + 31) & ~31;
  }
#endif
  return p;
}

void aramRemoveData(void* aram, unsigned long len
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
                    ,
                    ARAMInfo* ai
#endif
) {
  PortAramFreeSample(aram);
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
  if (ai != NULL) {
    ai->aramWrite -= (len + 31) & ~31;
  }
#endif
}

static void InitStreamBuffers() {}

unsigned char aramAllocateStreamBuffer(u32 len) {
  return PortAramAllocStreamBuffer(len);
}

size_t aramGetStreamBufferAddress(u8 id, size_t* len) {
  MUSY_ASSERT_MSG(id != 0xFF, "Stream buffer ID is invalid");
  return PortAramStreamBufferAddress(id, len);
}

void aramFreeStreamBuffer(unsigned char id) {
  MUSY_ASSERT_MSG(id != 0xFF, "Stream buffer ID is invalid");
  PortAramFreeStreamBuffer(id);
}
#endif
