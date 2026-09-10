#ifndef _OBJECTBLUR_H_
#define _OBJECTBLUR_H_

#include "NL/nlMath.h"
#include "NL/nlSlotPool.h"
#include "NL/nlDLRing.h"

struct BlurPointEntry
{
    class nlVector3 v3Top;    // offset 0x0, size 0xC
    class nlVector3 v3Bottom; // offset 0xC, size 0xC
}; // total size: 0x18

class BlurHandler
{
public:
    ~BlurHandler() { delete[] m_pointRingBuffer; }

    void operator delete(void* p)
    {
        ((BlurHandler*)p)->m_next = (BlurHandler*)m_BlurHandlerSlotPool.m_FreeList;
        m_BlurHandlerSlotPool.m_FreeList = (SlotPoolEntry*)p;
    }

    void RenderMesh(unsigned long uTexID);
    void Die(float timeToDie);
    void AddViewOrientedPoint(const nlVector3& position, const nlVector3& forwardVector);
    bool ConstructViewOrientedPoints(nlVector3& topPoint, nlVector3& bottomPoint, nlVector3 position, const nlVector3& forwardVector);

    static SlotPool<BlurHandler> m_BlurHandlerSlotPool;

public:
    /* 0x00 */ BlurHandler* m_next;
    /* 0x04 */ BlurHandler* m_prev;
    /* 0x08 */ bool m_bAdditive;
    /* 0x0C */ u32 m_uTexHashID;
    /* 0x10 */ float m_fLineWidth;
    /* 0x14 */ BlurPointEntry m_pointFinal;
    /* 0x2C */ BlurPointEntry* m_pLastPoint;
    /* 0x30 */ bool m_bDying;
    /* 0x34 */ float m_fDyingTimer;
    /* 0x38 */ float m_fTimeToDie;
    /* 0x3C */ int m_maxPositionEntries;
    /* 0x40 */ BlurPointEntry* m_pointRingBuffer;
    /* 0x44 */ int m_nInsertIndex;
    /* 0x48 */ int m_nTrailEndPointer;
}; // total size: 0x4C

class BlurManager
{
public:
    static void Shutdown();
    static void Update(float deltaTime);
    static void DestroyHandler(BlurHandler* handler, float timeToDie);
    static BlurHandler* GetNewHandler(const char* szTextureName, float fLineWidth, int maxPositionEntries, bool bAdditive);

    static BlurHandler* m_activeBlurHandler;
};

#endif // _OBJECTBLUR_H_
