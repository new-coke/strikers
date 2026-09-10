#ifndef _TRANSITIONSEQUENCE_H_
#define _TRANSITIONSEQUENCE_H_

#include "NL/nlMath.h"
#include "Game/Sys/simpleparser.h"

#include "Game/Transitions/ScreenTransitionManager.h"

struct TransitionSounds
{
    /* 0x0 */ char* soundStr;
    /* 0x4 */ bool stopAtEnd;
}; // total size: 0x8

class TransitionSequence : public ScreenTransition
{
public:
    TransitionSequence();

    virtual ~TransitionSequence();
    virtual void Update(float dt);
    virtual void Render(eGLView glView);
    virtual void Reset();
    virtual bool IsFinished();
    virtual void Cancel();
    virtual float Time() const;
    virtual float CutTime() const { return m_CutAt; };
    virtual float GetTransitionLength();

    void DoSanityCheck();
    void Initialize(SimpleParser* parser);

    /* 0x4, */ class ScreenTransition** m_pTransitions;
    /* 0x8, */ unsigned short m_nNumTransitions;
    /* 0xC, */ char* m_pPlaying;
    /* 0x10 */ float* m_pEarly;
    /* 0x14 */ TransitionSounds* m_pSound;
    /* 0x18 */ float m_Time;
    /* 0x1C */ float m_CutAt;
}; // total size: 0x20

#endif // _TRANSITIONSEQUENCE_H_
