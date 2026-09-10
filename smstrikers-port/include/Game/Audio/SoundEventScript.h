#ifndef _SOUNDEVENTSCRIPT_H_
#define _SOUNDEVENTSCRIPT_H_

#include "Game/InterpreterCore.h"

#include "NL/nlString.h"
#include "NL/nlFile.h"

class SoundEventScript : public InterpreterCore
{
public:
    SoundEventScript()
        : InterpreterCore(0xA)
    {
        pByteCode = NULL;
        unsigned long fileSize = 0;
        pByteCode = nlLoadEntireFile("audio/soundevents.byte_code", &fileSize, 0x20, AllocateStart);
        LoadByteCode(pByteCode);
    }
    /* 0x0C */ virtual void DoFunctionCall(unsigned int func);

    static void CreateInstance();
    static void DestroyInstance();
    static SoundEventScript& Instance();

    void Call(const char* functionName);

    /* 0x24 */ char mCurrentFunction[64]; // size 0x40
    /* 0x64 */ void* pByteCode;

    static SoundEventScript* pInstance;
}; // total size: 0x68

#endif // _SOUNDEVENTSCRIPT_H_
