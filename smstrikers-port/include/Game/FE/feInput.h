#ifndef _FEINPUT_H_
#define _FEINPUT_H_

#include "types.h"
#include "NL/globalpad.h"

class BaseSceneHandler;

enum eFEINPUT_PAD
{
    FE_PAD1_ID = 0,
    FE_PAD2_ID = 1,
    FE_PAD3_ID = 2,
    FE_PAD4_ID = 3,
    FE_PAD5_ID = 4,
    FE_PAD6_ID = 5,
    FE_PAD7_ID = 6,
    FE_PAD8_ID = 7,
    FE_ALL_PADS = 8,
};

struct InputLockEntry
{
    /* 0x00 */ BaseSceneHandler* m_pBaseSceneHandler;
    /* 0x04 */ int m_customID;
}; // total size: 0x8

struct FEInput
{
    // !! this needs to be at the top of the class, otherwise fields to not map correctly to their needed offsets
    /* 0x00 */ InputLockEntry m_nExclusiveInputSceneHashIDStack[4];
    /* 0x20 */ int m_InputLockDepth;
    /* 0x24 */ bool m_bEnableInput[4];
    /* 0x28 */ bool m_bInputAllowed;

    FEInput();
    virtual ~FEInput() { };

    void EnableAnalogToDPadMapping(eFEINPUT_PAD pad, bool enable);
    void SetAutoRepeatParams(eFEINPUT_PAD pad, int button, float initialdelay, float repeatrate);
    void Update(float dt);
    bool HasInputLock(BaseSceneHandler* pRequestingSceneHandler) const;
    void PopExclusiveInputLock(BaseSceneHandler* pRequestingSceneHandler);
    void PushExclusiveInputLock(BaseSceneHandler* pRequestingSceneHandler, int customID);
    void EnableInputIfSceneHasFocus(BaseSceneHandler* pSceneHandler);
    bool JustReleased(eFEINPUT_PAD pad, int button, bool remap, eFEINPUT_PAD* pOutPad);
    bool JustPressed(eFEINPUT_PAD pad, int button, bool remap, eFEINPUT_PAD* pOutPad);
    bool IsAutoPressed(eFEINPUT_PAD pad, int button, bool remap, eFEINPUT_PAD* pOutPad);
    bool IsPressed(eFEINPUT_PAD pad, int button, bool remap, eFEINPUT_PAD* pOutPad);
    bool IsConnected(eFEINPUT_PAD pad);
    cGlobalPad* GetGlobalPad(eFEINPUT_PAD pad) const;
    void Reset();

    static int* m_pRemapArray;
    static void Initialize();
}; // namespace FEInput

extern FEInput* g_pFEInput;

#endif // _FEINPUT_H_
