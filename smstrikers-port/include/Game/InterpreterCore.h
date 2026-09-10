#ifndef _INTERPRETERCORE_H_
#define _INTERPRETERCORE_H_

#include "types.h"
#include "NL/nlAlgorithm.h"

struct FunctionEntryPoint
{
    /* 0x0 */ u32 hash;
    /* 0x4 */ u32 offset;

    operator unsigned long() const { return hash; }
}; // total size: 0x8

struct ByteCodeHeader
{
    /* 0x00 */ u32 signature;
    /* 0x04 */ u32 numFunctions;
    /* 0x08 */ u32 dataSegmentSize;
    /* 0x0C */ u32 codeSegmentSize;
    /* 0x10 */ u32 stringSegmentSize;
    /* 0x14 */ FunctionEntryPoint* m_FunctionTable;
    /* 0x18 */ u32* m_DataSegment;
    /* 0x1C */ u16* m_CodeSegment;
    /* 0x20 */ u8* m_StringSegment;
}; // total size: 0x24

class InterpreterCore
{
public:
    InterpreterCore(unsigned int size);

    /* 0x08 */ virtual ~InterpreterCore();
    /* 0x0C */ virtual void DoFunctionCall(unsigned int) = 0;

    void LoadByteCode(void* data);
    void CallFunction(unsigned long hash);
    void CallFunctionAt(unsigned long offset);
    bool FunctionExists(unsigned long hash);
    void Run();
    void StopWithUndo();
    void Step();
    bool IsFinished() const;

protected:
    // PORT: the stack is pointer-width.
    uintptr_t Pop()
    {
        m_SP--;
        return *m_SP;
    }

public:
    /* 0x04 */ uintptr_t m_Return;   // PORT: holds popped values, so pointer-width
    /* 0x08 */ ByteCodeHeader* m_Header;
    /* 0x0C */ uintptr_t* m_StackSegment;
    /* 0x10 */ u16* m_IP;
    /* 0x14 */ uintptr_t* m_SP;
    /* 0x18 */ uintptr_t* m_BP;
    /* 0x1C */ uintptr_t* m_SavedSP;
    /* 0x20 */ u32 m_Stop : 1;
    /* 0x20 */ u32 m_RunState : 2;
}; // total size: 0x24

#endif // _INTERPRETERCORE_H_
