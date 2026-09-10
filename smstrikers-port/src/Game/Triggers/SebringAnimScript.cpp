#include "Game/Triggers/SebringAnimScript.h"

#include "Game/CharacterTriggers.h"
#include "Game/Sys/audio.h"
#include "NL/nlDebug.h"

class AnimTriggerCallbackInfo
{
public:
    /* 0x0 */ unsigned long m_uEventID;
    /* 0x4 */ float m_fIntensity;
}; // total size: 0x8

void SebringAnimTagScriptInterpreter::PlayCharSfx(const char* szSFXType, NisCharacterClass charIdentifier, const char* szScript)
{
    Audio::PlayCharSFXbyStr(szSFXType, charIdentifier, 100.0f, -1.0f, false, false, NULL, NULL, NULL);
}

void SebringAnimTagScriptInterpreter::PlayCharSfxWithVol(const char* szSFXType, NisCharacterClass charIdentifier, float fVol, const char* szScript)
{
    Audio::PlayCharSFXbyStr(szSFXType, charIdentifier, fVol, -1.0f, false, false, NULL, NULL, NULL);
}

void SebringAnimTagScriptInterpreter::PlayWorldSfx(const char* szSFXType, const char* szScript)
{
    Audio::PlayWorldSFXbyStr(szSFXType, 100.0f, -1.0f, false, true, NULL, NULL, NULL);
}

void SebringAnimTagScriptInterpreter::PlayWorldSfxWithVol(const char* szSFXType, float fVol, const char* szScript)
{
    Audio::PlayWorldSFXbyStr(szSFXType, fVol, -1.0f, false, true, NULL, NULL, NULL);
}

void SebringAnimTagScriptInterpreter::StopCharSfx(const char* szSFXType, NisCharacterClass charIdentifier, const char* szScript)
{
    Audio::StopCharSFXbyStr(szSFXType, charIdentifier);
}

void SebringAnimTagScriptInterpreter::StopWorldSfx(const char* szSFXType, const char* szScript)
{
    Audio::StopWorldSFXbyStr(szSFXType);
}

/**
 * Offset/Address/Size: 0x3C | 0x801A40B4 | size: 0x1FC
 */
void SebringAnimTagScriptInterpreter::DoFunctionCall(unsigned int func)
{
    switch (func)
    {
    case 0: // PlayCharSfx
    {
        m_SP--;
        const char* script = (const char*)*m_SP;
        m_SP--;
        NisCharacterClass charClass = (NisCharacterClass)*m_SP;
        m_SP--;
        const char* name = (const char*)*m_SP;
        PlayCharSfx(name, charClass, script);
        break;
    }
    case 1: // PlayCharSfxWithVol
    {
        m_SP--;
        const char* script = (const char*)*m_SP;
        m_SP--;
        float fVol = *(float*)m_SP;
        m_SP--;
        NisCharacterClass charClass = (NisCharacterClass)*m_SP;
        m_SP--;
        const char* name = (const char*)*m_SP;
        PlayCharSfxWithVol(name, charClass, fVol, script);
        break;
    }
    case 2: // PlayWorldSfx
    {
        m_SP--;
        const char* script = (const char*)*m_SP;
        m_SP--;
        const char* name = (const char*)*m_SP;
        PlayWorldSfx(name, script);
        break;
    }
    case 3: // PlayWorldSfxWithVol
    {
        m_SP--;
        const char* script = (const char*)*m_SP;
        m_SP--;
        float fVol = *(float*)m_SP;
        m_SP--;
        const char* name = (const char*)*m_SP;
        PlayWorldSfxWithVol(name, fVol, script);
        break;
    }
    case 4: // StopCharSfx
    {
        m_SP--;
        const char* script = (const char*)*m_SP;
        m_SP--;
        NisCharacterClass charClass = (NisCharacterClass)*m_SP;
        m_SP--;
        const char* name = (const char*)*m_SP;
        StopCharSfx(name, charClass, script);
        break;
    }
    case 5: // StopWorldSfx
    {
        m_SP--;
        const char* script = (const char*)*m_SP;
        m_SP--;
        const char* name = (const char*)*m_SP;
        StopWorldSfx(name, script);
        break;
    }
    default:
        nlBreak();
        break;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x801A4078 | size: 0x3C
 */
void SebringAnimTagScriptInterpreter::TriggerFired(unsigned long triggerId)
{
    if (m_FireTriggers)
    {
        AnimTriggerCallbackInfo data;
        data.m_uEventID = triggerId;
        data.m_fIntensity = 100.0f;
        CharacterTriggerHandler((uintptr_t)&data);
    }
}
