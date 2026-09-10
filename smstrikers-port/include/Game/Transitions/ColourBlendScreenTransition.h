#ifndef _COLOURBLENDSCREENTRANSITION_H_
#define _COLOURBLENDSCREENTRANSITION_H_

#include "NL/nlMath.h"
#include "Game/Sys/simpleparser.h"

#include "Game/Transitions/ScreenTransitionManager.h"

class ColourBlendScreenTransition : public ScreenTransition
{
public:
    ColourBlendScreenTransition(float length, const nlVector4& startColour, const nlVector4& endColour, u32 texture)
    {
        m_fLength = length;
        m_fCurrentTime = 0.0f;
        m_RGBAstart = startColour;
        m_RGBAend = endColour;
        m_nTexture = texture;
    };

    virtual ~ColourBlendScreenTransition() { };
    virtual void Update(float dt);
    virtual void Render(eGLView view);
    virtual void Reset() { m_fCurrentTime = 0.0f; };
    virtual bool IsFinished() { return m_fCurrentTime > m_fLength; };
    virtual void Cancel() { };
    virtual float Time() const { return m_fCurrentTime / m_fLength; };
    virtual float GetTransitionLength() { return m_fLength; };

    static ColourBlendScreenTransition* GetFromParser(SimpleParser* parser);

    /* 0x04 */ float m_fLength;
    /* 0x08 */ float m_fCurrentTime;
    /* 0x0C */ nlVector4 m_RGBAstart;
    /* 0x1C */ nlVector4 m_RGBAend;
    /* 0x2C */ u32 m_nTexture;
}; // total size: 0x30

#endif // _COLOURBLENDSCREENTRANSITION_H_
