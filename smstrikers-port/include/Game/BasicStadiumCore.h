#ifndef _BASICSTADIUMCORE_H_
#define _BASICSTADIUMCORE_H_

#include "types.h"

#include "Game/World.h"
#include "Game/Sys/eventman.h"
#include "Game/Render/NPCManager.h"

class BasicStadium : public World
{
public:
    BasicStadium(const char* szBaseName);
    /* 0x04 */ virtual ~BasicStadium();
    /* 0x08 */ virtual eTerrain GetTerrainType(const nlVector3&) const { return m_TerrainType; };
    /* 0x14 */ virtual void Render();
    /* 0x18 */ virtual void Update(float fTimeDelta);
    /* 0x1C */ virtual void UpdateInReplay(float fTimeDelta);
    /* 0x2C */ virtual bool DoLoad();
    /* 0x30 */ virtual bool DoInitialize();

    void HyperStrikeModelAddHelper(unsigned long hash);
    void ResetObjectsForNewGame();
    void StartCameraFlashes();
    void UpdateCameraFlashes(float fTimeDelta);
    void LoadListOfGameplayInvisibleModels();

    static void BasicStadiumEventHandler(Event* pEvent, void* pUserData);
    static BasicStadium* GetCurrentStadium();

    const char* GetBaseName() const
    {
        return m_szBaseName;
    }

    /* 0x13C */ nlVector3* m_CameraFlashPositions;
    /* 0x140 */ s32 m_NumCameraFlashPositions;
    /* 0x144 */ float m_fSkyboxRotationAng;
    /* 0x148 */ float m_fCloudRotationAng;
    /* 0x14C */ DrawableObject* m_pSkyboxObject;
    /* 0x150 */ DrawableObject* m_pCloudsObject;
    /* 0x154 */ DrawableObject* m_pPropObject;
    /* 0x158 */ EventHandler* m_pEventHandler;
    /* 0x15C */ bool m_bCameraFlashesEnabled;
    /* 0x160 */ float m_fTimeUntilNextCameraFlash;
    /* 0x164 */ eTerrain m_TerrainType;
    /* 0x168 */ char m_szBaseName[0x20];
    /* 0x188 */ NPCManager* mpNPCManager;
};

#endif // _BASICSTADIUMCORE_H_
