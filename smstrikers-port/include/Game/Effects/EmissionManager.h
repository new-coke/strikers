#ifndef _EMISSIONMANAGER_H_
#define _EMISSIONMANAGER_H_

#include "NL/gl/gl.h"
#include "Game/Effects/EmissionController.h"

void fxSetTerrain(unsigned long terrainID);
u32 fxGetTerrain();

class LoadFrame;
class SaveFrame;

#include "Game/Effects/EffectsGroup.h"

class EffectsLight
{
public:
    /* 0x00 */ nlVector3 m_v3Position;
    /* 0x0C */ f32 m_fRadius;
    /* 0x10 */ nlColour m_Colour;
}; // size: 0x14

struct LingerMessage : public efNode
{
    /* 0x008 */ char szMessage[256];
    /* 0x108 */ int nLingers;
    /* 0x10C */ int nParticles;
}; // size: 0x110

class EmissionManager
{
protected:
    EmissionManager(bool recording = false)
        : m_bRecording(recording)
    {
    }

public:
    static void KillOldest(int num, bool lingeringOnly);
    void Replay(SaveFrame& frame);
    void Replay(LoadFrame& frame);
    static void AddError(const char* format, ...);
    static void ResetLingerers();
    static void Destroy(uintptr_t userData, const EffectsGroup* pEffectsGroup);
    static void DestroyAll(bool exceptPersistent);
    static bool IsPlaying(uintptr_t userData, const EffectsGroup* pEffectsGroup);
    static void Kill(uintptr_t userData, const EffectsGroup* pEffectsGroup);
    static bool IsStillAlive(EmissionController* controller);
    static EmissionController* Create(EffectsGroup* pEffectsGroup, unsigned short id);
    static efList* GetContainer();
    static void Render();
    static void AddEffectsLight(const EffectsLight& light);
    static EffectsLight* GetLight(int index);
    static s32 GetNumLights();
    static void Update(float dt);
    static bool Shutdown();
    static bool Startup(eGLView view);

    static EmissionManager& InstanceForReplayOnly();

    /* 0x00 */ bool m_bRecording;
};

#endif // _EMISSIONMANAGER_H_
