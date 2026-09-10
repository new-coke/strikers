#ifndef _ANIMTAGSCRIPT_H_
#define _ANIMTAGSCRIPT_H_

#include "NL/nlSlotPool.h"
#include "Game/Inventory.h"
#include "Game/InterpreterCore.h"
#include "Game/SAnim.h"

class AnimTagScriptInterpreter;

struct AnimTagInfo
{
    /* 0x0 */ unsigned long Trigger;
    /* 0x4 */ unsigned long ScriptFuncOffset;
}; // total size: 0x8

struct AnimTagCBInfo
{
    /* 0x0 */ AnimTagScriptInterpreter* pAnimTagScript;
    /* 0x4 */ AnimTagInfo ScriptInfo;
}; // total size: 0xC

class AnimTagScriptInterpreter : protected InterpreterCore
{
public:
    AnimTagScriptInterpreter(unsigned int stackSize)
        : InterpreterCore(stackSize)
        , m_AnimTagSlotPool(16, 16)
    {
        m_BytecodeCount = 0;
    }
    virtual ~AnimTagScriptInterpreter()
    {
        SlotPoolBase::BaseFreeBlocks(&m_AnimTagSlotPool, sizeof(AnimTagCBInfo));
        for (int i = 0; i < m_BytecodeCount; i++)
        {
            nlFree(m_ppBytecode[i]);
        }
    }
    /* 0x0C */ virtual void DoFunctionCall(unsigned int) = 0;
    /* 0x10 */ virtual void TriggerFired(unsigned long) { }

    static void AnimControllerCB(uintptr_t ref)
    {
        AnimTagCBInfo* pInfo = (AnimTagCBInfo*)ref;
        unsigned long address = pInfo->ScriptInfo.ScriptFuncOffset;
        AnimTagScriptInterpreter* pScript = pInfo->pAnimTagScript;
        if ((u32)(address + 0x10000) != 0xFFFF)
        {
            pScript->CallFunctionAt(address);
        }
        pScript->TriggerFired(pInfo->ScriptInfo.Trigger);
    }

    u8 SetupAnimationTriggers(const char* TriggerFileName, cInventory<cSAnim>* pAnimInventory);

    /* 0x24 */ SlotPool<AnimTagCBInfo> m_AnimTagSlotPool;
    /* 0x3C */ void* m_ppBytecode[32];
    /* 0xBC */ int m_BytecodeCount;
}; // total size: 0xC0

#endif // _ANIMTAGSCRIPT_H_
