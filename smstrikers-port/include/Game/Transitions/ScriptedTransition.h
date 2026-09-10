#ifndef _SCRIPTEDTRANSITION_H_
#define _SCRIPTEDTRANSITION_H_

#include "NL/nlMath.h"
#include "NL/gl/glDraw2.h"

#include "Game/Sys/simpleparser.h"
#include "Game/Transitions/ScreenTransitionManager.h"

enum eTimeLine
{
    TIME_LINEAR = 0,
    TIME_ACCEL = 1,
    TIME_DECEL = 2,
    NUM_TIME_LINES = 3,
};

class TransitionModifierInterface
{
public:
    TransitionModifierInterface() { };
    virtual ~TransitionModifierInterface() { };
    virtual void InitializeFromParser(SimpleParser* parser) = 0;
    virtual void ApplyModifier(glPoly2& poly, float time) = 0;
    virtual void Cleanup() { };
};

class ScriptedScreenTransition : public ScreenTransition
{
public:
    ScriptedScreenTransition();
    virtual ~ScriptedScreenTransition();
    virtual bool IsFinished() { return m_fCurrentTime > m_fLength; };
    virtual void Reset() { m_fCurrentTime = 0.0f; };
    virtual float Time() const { return m_fCurrentTime / m_fLength; };
    virtual float GetTransitionLength() { return m_fLength; };
    virtual void Update(float dt);
    virtual void Render(eGLView view);
    virtual void Cancel();

    void InitializeFromParser(SimpleParser* parser);
    TransitionModifierInterface* GetModifierFromName(char* pName);

    /* 0x04 */ TransitionModifierInterface** m_pModifiers; // offset 0x4, size 0x4
    /* 0x08 */ int m_nModifiers;                           // offset 0x8, size 0x4
    /* 0x0C */ float m_fLength;                            // offset 0xC, size 0x4
    /* 0x10 */ float m_fCurrentTime;                       // offset 0x10, size 0x4
    /* 0x14 */ u32 m_nTexture;                             // offset 0x14, size 0x4
    /* 0x18 */ eTimeLine m_eTimeLine;                      // offset 0x18, size 0x4
}; // total size: 0x1C

#endif // _SCRIPTEDTRANSITION_H_
