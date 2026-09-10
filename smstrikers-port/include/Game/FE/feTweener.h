#ifndef _FETWEENER_H_
#define _FETWEENER_H_

#include "NL/nlDLListContainer.h"
#include "NL/nlMemory.h"

class FETweener
{
public:
    FETweener(const unsigned char arraySize, float* startVals, float* endVals, float duration, float delay,
        float (*tweenFunc)(float, float, float, float), void* applyObj, void (*setterFunc)(void*, float*))
        : m_arraySize(arraySize)
        , m_startTime(0.0f)
    {
        for (unsigned char i = 0; i < m_arraySize; i++)
        {
            m_startVal[i] = startVals[i];
            m_diffVal[i] = endVals[i] - startVals[i];
        }

        m_duration = duration;
        m_tweenFunc = tweenFunc;
        m_applyObj = applyObj;
        m_setterFunc = setterFunc;
        m_nextTween = NULL;
        m_delay = delay;
        m_curTime = 0.0f;
        m_doneFunc = NULL;
        m_tweenActive = 0;
    }

    void setNextTween(FETweener* next);
    void setDoneCallFunc(void (*doneFunc)(void*), void* doneFuncParam);

    /* 0x00 */ unsigned char m_arraySize;                        // size 0x1
    /* 0x01 */ unsigned char m_tweenActive;                      // size 0x1
    /* 0x02 */ unsigned char m_completed;                        // size 0x1
    /* 0x04 */ float m_curTime;                                  // size 0x4
    /* 0x08 */ float m_delay;                                    // size 0x4
    /* 0x0C */ float m_startTime;                                // size 0x4
    /* 0x10 */ float m_startVal[4];                              // size 0x10
    /* 0x20 */ float m_diffVal[4];                               // size 0x10
    /* 0x30 */ float m_duration;                                 // size 0x4
    /* 0x34 */ float (*m_tweenFunc)(float, float, float, float); // size 0x4
    /* 0x38 */ void* m_applyObj;                                 // size 0x4
    /* 0x3C */ void (*m_setterFunc)(void*, float*);              // size 0x4
    /* 0x40 */ FETweener* m_nextTween;                           // size 0x4
    /* 0x44 */ void (*m_doneFunc)(void*);                        // size 0x4
    /* 0x48 */ void* m_doneFuncParam;                            // size 0x4

    virtual ~FETweener();
}; // total size: 0x50

class FETweenManager
{
public:
    FETweenManager();
    ~FETweenManager();
    FETweener* createTween(float* startVals, float* endVals, float duration, float delay, unsigned char arraySize, float (*tweenFunc)(float, float, float, float), void* applyObj, void (*setterFunc)(void*, const float*));
    void Update(float fDeltaT);
    void clearTweens();
    void clearTweensOnObj(void* obj);
    void startTween(FETweener* pTweener);

    /* 0x00 */ nlDLListSlotPool<FETweener*> m_tweenList;       // size 0x1C
    /* 0x1C */ nlDLListSlotPool<FETweener*> m_activeTweenList; // size 0x1C
}; // total size: 0x38

#endif // _FETWEENER_H_
