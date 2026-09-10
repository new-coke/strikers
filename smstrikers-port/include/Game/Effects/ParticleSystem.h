#ifndef _PARTICLESYSTEM_H_
#define _PARTICLESYSTEM_H_

#include "NL/nlMath.h"
#include "NL/nlColour.h"
#include "Game/Effects/efList.h"
#include "Game/Effects/EffectsTemplate.h"
#include "Game/Effects/EffectsGroup.h"
#include "NL/gl/gl.h"

class EffectsLight;

struct ParticleReturn
{
    /* 0x00 */ s16 texcoord[4][2];
    /* 0x10 */ nlVector3 position[4];
    /* 0x40 */ nlColour c;
}; // size: 0x44

class Particle : public efNode
{
public:
    inline Particle();

public:
    /* 0x8, */ f32 timeElapsed;
    /* 0xC, */ f32 lifeSpan;
    /* 0x10 */ nlVector3 position;
    /* 0x1C */ nlVector3 velDir;
    /* 0x28 */ f32 mass;
    /* 0x2C */ f32 size;
    /* 0x30 */ f32 dSize;
    /* 0x34 */ f32 rot;
    /* 0x38 */ f32 dRot;
    /* 0x3C */ f32 velocity;
    /* 0x40 */ f32 acceleration;
    /* 0x44 */ f32 frame;
    /* 0x48 */ f32 FPS;
};

class ParticleSystem : public efNode
{
public:
    ParticleSystem(EffectsTemplate* pTemplate, EffectsSpec* spec);
    ~ParticleSystem();
    static void ClearViews();
    static void AddView(eGLView view);
    void UpdateCoordSys();
    void UpdateCoordSys(nlMatrix4& mCoordSys);
    void CreateNewParticles(int numParticles);
    void UpdateAllParticles(float dt);
    static void UpdateLight(EffectsLight* pLight, Particle* pPart, EffectsTemplate* pTemplate, const nlVector3& viewRight, const nlVector3& viewUp, const nlMatrix4* pCoordSys);
    static void UpdateParticle(ParticleReturn* pReturn, Particle* pPart, EffectsTemplate* pTemplate, const nlVector3& viewRight, const nlVector3& viewUp, const nlMatrix4* pCoordSys);
    void RenderAllParticles(eGLView view);
    void Die();
    bool Update(float dt);
    float GetRemainingTime() const;

    /* 0x08 */ EffectsSpec* m_pSpec;          // offset 0x8, size 0x4
    /* 0x0C */ EffectsTemplate* m_pTemplate;  // offset 0xC, size 0x4
    /* 0x10 */ float m_fElapsedTime;          // offset 0x10, size 0x4
    /* 0x14 */ float m_fNumParticlesToCreate; // offset 0x14, size 0x4
    /* 0x18 */ float m_fDelay;                // offset 0x18, size 0x4
    /* 0x1C */ unsigned long m_uLayer;        // offset 0x1C, size 0x4
    /* 0x20 */ nlVector3 m_Mirror;            // offset 0x20, size 0xC
    /* 0x2C */ nlVector3 m_vVelocity;         // offset 0x2C, size 0xC
    /* 0x38 */ nlVector3 m_vPosition;         // offset 0x38, size 0xC
    /* 0x44 */ nlVector3 m_vForward;          // offset 0x44, size 0xC
    /* 0x50 */ nlVector3 m_vSourcePosition;   // offset 0x50, size 0xC
    /* 0x5C */ bool m_bAmDying;               // offset 0x5C, size 0x1
    /* 0x60 */ efList m_Particles;            // offset 0x60, size 0xC
    /* 0x6C */ unsigned short m_aFacing;      // offset 0x6C, size 0x2
    /* 0x6E */ bool m_bVisible;               // offset 0x6E, size 0x1

    static int m_NumInstances;
    static u8 m_AllowInFront;
    static glModel* (*m_LightingCallback)(glModel*);
    static u8 (*m_Callback)(eGLView, unsigned long, efList&, EffectsTemplate*, nlVector3&, nlVector3&, nlMatrix4*);
    static eGLView m_eViews[8];
    static int m_nNumViews;
    static float m_fAspect;
}; // total size: 0x70

bool fxParticleShutdown();
bool fxParticleStartup(int maxNumParticles);

#endif // _PARTICLESYSTEM_H_
