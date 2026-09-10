#ifndef _BASESCENEHANDLER_H_
#define _BASESCENEHANDLER_H_

#include "types.h"

#include "Game/FE/fePresentation.h"
#include "Game/FE/feScene.h"

class BaseScreenHandler
{
public:
    virtual ~BaseScreenHandler() = 0;
    virtual u32 fnc1() = 0;  // offset 0x0, size 0x4
    virtual void fnc2() = 0; // offset 0x0, size 0x4
    virtual void fnc3() = 0; // offset 0x0, size 0x4

    /* 0x04 */ BaseScreenHandler* m_next;
    /* 0x08 */ BaseScreenHandler* m_prev;
    /* 0x0C */ TLInstance* m_pTLInstance;
    /* 0x10 */ FEScene* m_pFEScene;
}; // total size: 0x14

class BaseSceneHandler
{
public:
    BaseSceneHandler()
    {
        m_bVisible = true;
        m_pScreenHandlerList = NULL;
        m_pActiveScreenHandler = NULL;
        m_pFEPresentation = NULL;
        m_pFEScene = NULL;
    };
    virtual ~BaseSceneHandler() { };
    virtual void Update(float dt);
    virtual void InitializeSubHandlers() { };
    virtual void AddScreenHandler(BaseScreenHandler* handler);
    virtual void RemoveScreenHandler(BaseScreenHandler* pScreenHandler);
    virtual void SetPresentation(FEPresentation* presentation) { m_pFEPresentation = presentation; };
    virtual void OnActivate();
    virtual void SceneCreated() { };
    virtual void SetVisible(bool visible) { m_bVisible = visible; };

    FEPresentation* GetPresentation() { return m_pFEPresentation; };

    /* 0x04 */ unsigned long m_uHashID;
    /* 0x08 */ bool m_bVisible; // is it 0x0C?
    /* 0x0C */ BaseScreenHandler* m_pScreenHandlerList;
    /* 0x10 */ BaseScreenHandler* m_pActiveScreenHandler;
    /* 0x14 */ FEPresentation* m_pFEPresentation;
    /* 0x18 */ FEScene* m_pFEScene;
}; // total size: 0x1C

#endif // _BASESCENEHANDLER_H_
