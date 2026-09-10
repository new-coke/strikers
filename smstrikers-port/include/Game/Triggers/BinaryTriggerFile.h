#ifndef _BINARYTRIGGERFILE_H_
#define _BINARYTRIGGERFILE_H_

#include "types.h"

struct FILE_HEADER
{
    /* 0x0 */ s8 Thumbprint[4];
    /* 0x4 */ u16 Version;
    /* 0x6 */ u16 AnimCount;
    /* 0x8 */ u16 BytecodeOffset;
    /* 0xA */ u16 reserved;
}; // total size: 0xC

struct ANIM_RECORD
{
    /* 0x0 */ u32 hash;
    /* 0x4 */ u16 TriggerCount;
    /* 0x6 */ u16 TriggerOffset;
}; // total size: 0x8

struct TRIGGER_RECORD
{
    /* 0x0 */ f32 Frame;
    /* 0x4 */ u32 Trigger;
    /* 0x8 */ u32 ScriptFuncOffset;
}; // total size: 0xC

class BinaryTriggerFile
{
public:
    BinaryTriggerFile(const char* FileName);

    /* 0x00 */ unsigned long m_FileSize;
    /* 0x04 */ FILE_HEADER* m_pFileData;
    /* 0x08 */ ANIM_RECORD* m_pFirstAnim;
    /* 0x0C */ TRIGGER_RECORD* m_pFirstTrigger;
    /* 0x10 */ ANIM_RECORD* m_pCurrentAnim;
    /* 0x14 */ u32 m_CurrentTrigger;
}; // total size: 0x18

#endif // _BINARYTRIGGERFILE_H_
