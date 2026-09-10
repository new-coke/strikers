#ifndef _THPSIMPLE_H_
#define _THPSIMPLE_H_

#include "NL/nlFile.h"
#include "dolphin/thp/THPBuffer.h"
#include "dolphin/thp/THPInfo.h"

struct THPSimpleControl
{
    /* 0x00 */ nlFile* file;
    /* 0x04 */ char magic[4];
    /* 0x08 */ u32 version;
    /* 0x0C */ u32 bufSize;
    /* 0x10 */ u32 audioMaxSamples;
    /* 0x14 */ f32 frameRate;
    /* 0x18 */ u32 numFrames;
    /* 0x1C */ u32 firstFrameSize;
    /* 0x20 */ u32 movieDataSize;
    /* 0x24 */ u32 compInfoDataOffsets;
    /* 0x28 */ u32 offsetDataOffsets;
    /* 0x2C */ u32 movieDataOffsets;
    /* 0x30 */ u32 finalFrameDataOffsets;
    /* 0x34 */ THPFrameCompInfo compInfo;
    /* 0x48 */ THPVideoInfo videoInfo;
    /* 0x54 */ THPAudioInfo audioInfo;
    /* 0x64 */ void* thpWork;
    /* 0x68 */ int open;
    /* 0x6C */ u8 preFetchState;
    /* 0x6D */ u8 audioState;
    /* 0x6E */ u8 loop;
    /* 0x6F */ u8 audioExist;
    /* 0x70 */ u32 curOffset;
    /* 0x74 */ s32 dvdError;
    /* 0x78 */ u32 readProgress;
    /* 0x7C */ s32 nextDecodeIndex;
    /* 0x80 */ s32 readIndex;
    /* 0x84 */ u32 readSize;
    /* 0x88 */ u32 totalReadFrame;
    /* 0x8C */ f32 curVolume;
    /* 0x90 */ f32 targetVolume;
    /* 0x94 */ f32 deltaVolume;
    /* 0x98 */ s32 rampCount;
    /* 0x9C */ THPReadBuffer readBuffer[16];
    /* 0x15C */ THPTextureSet textureSet;
    /* 0x16C */ THPAudioBuffer audioBuffer[6];
    /* 0x1B4 */ s32 audioDecodeIndex;
    /* 0x1B8 */ s32 audioOutputIndex;
}; // total size: 0x1BC

extern "C" int THPSimpleSetBuffer(unsigned char* buffer);
#endif // _THPSIMPLE_H_
