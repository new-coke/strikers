#ifndef _SEBRINGANIMSCRIPT_H_
#define _SEBRINGANIMSCRIPT_H_

#include "Game/Triggers/AnimTagScript.h"

#include "Game/Sys/audio.h"

class SebringAnimTagScriptInterpreter : public AnimTagScriptInterpreter
{
public:
    SebringAnimTagScriptInterpreter()
        : AnimTagScriptInterpreter(10)
    {
        m_FireTriggers = true;
    }

    virtual void DoFunctionCall(unsigned int func);
    virtual void TriggerFired(unsigned long triggerId);

    void PlayCharSfx(const char* szSFXType, NisCharacterClass charIdentifier, const char* szScript);
    void PlayCharSfxWithVol(const char* szSFXType, NisCharacterClass charIdentifier, float fVol, const char* szScript);
    void PlayWorldSfx(const char* szSFXType, const char* szScript);
    void PlayWorldSfxWithVol(const char* szSFXType, float fVol, const char* szScript);
    void StopCharSfx(const char* szSFXType, NisCharacterClass charIdentifier, const char* szScript);
    void StopWorldSfx(const char* szSFXType, const char* szScript);

    /* 0xC0 */ bool m_FireTriggers;
}; // total size: 0xC4

#endif // _SEBRINGANIMSCRIPT_H_
