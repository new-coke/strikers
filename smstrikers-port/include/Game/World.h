#ifndef _WORLD_H_
#define _WORLD_H_

#include "NL/nlMath.h"
#include "Game/TerrainTypes.h"
#include "NL/nlAVLTree.h"

#include "Game/World/worldanim.h"
#include "NL/nlDLListContainer.h"

#include "Game/LightObject.h"

class WorldAnimManager;
class TMAnimController;
class SkinnedAnimController;
class DrawableObject;
class DrawableModel;
class nlChunk;
struct glModelPacket;
struct glModel;
class GLShadowVolume;
struct WorldObjectData;
class Event;
class CharacterPhysicsData;

struct WorldHelperChunkData
{
    /* 0x00 */ char m_szName[60];       // size 0x3C
    /* 0x3C */ u32 m_uHashID;   // PORT: 4 bytes on disc
    /* 0x40 */ nlMatrix4 m_worldMatrix; // size 0x40
}; // total size: 0x80

class HelperObject
{
public:
    /* 0x00 */ unsigned long m_uHashID; // size 0x4
    /* 0x04 */ nlMatrix4 m_worldMatrix; // size 0x40
    /* 0x44 */ char m_szName[64];       // size 0x40
}; // total size: 0x84

class World
{
public:
    static u32 m_uCurrentFrameCount;
    static bool sbIsHyperShootToScoreRenderingEnabled;
    static bool sbShowPositiveXNetDuringHyperStrike;
    static const int MAX_NAME_LENGTH = 64;
    static bool sbSkyboxRenderingDisabled;
    static bool sbStadiumRenderingDisabled;

    World(const char* szWorldName);
    bool IsCaptainShootToScorePresentationOn() const;
    void* GetSTSIntensity() const
    {
        return m_pSTSIntensity;
    }
    /* 0x04 */ virtual ~World();

    /* 0x08 */ virtual eTerrain GetTerrainType(const nlVector3& vPosition) const = 0;
    /* 0x10 */ virtual u8 HandleObjectCreation(WorldObjectData* pObjectData);
    /* 0x14 */ virtual void Render();
    /* 0x18 */ virtual void Update(float fDeltaT);
    /* 0x1C */ virtual void UpdateInReplay(float fTimeDelta);
    /* 0x20 */ virtual void FixedUpdate(float fTimeDelta);
    /* 0x24 */ virtual void HandleEvent(Event* pEvent, void* pData);
    /* 0x28 */ virtual void CreateHelperObjFromChunk(nlChunk* chunk);
    /* 0x2C */ virtual bool DoLoad() = 0;
    /* 0x30 */ virtual bool DoInitialize() = 0;

    int CompareNameToGenericName(const char* objName, const char* genericName);
    unsigned long GetHashIdForGenericName(const char* name) const;
    LightObject* GetShadowLight(const nlVector3& vPosition, float fRadius);
    unsigned char RemoveDrawableObject(DrawableObject* pObject);
    bool AddDrawableObject(unsigned long uHashID, DrawableObject* pDrawableObject);
    TMAnimController* CreateUniqueTransformController(const char* szObjName, const char* szHierarchy);
    TMAnimController* CreateUniqueTransformController(DrawableObject* pRootObject, const char* szHierarchy);
    unsigned char AssignSkinnedController(SkinnedAnimController* pController, const char* szObjName);
    SkinnedAnimController* CreateGangedSkinnedController(const char* szHierarchy);
    SkinnedAnimController* CreateUniqueSkinnedController(const char* szObjName, const char* szHierarchy);
    HelperObject* FindHelperObject(unsigned long uHashID);
    DrawableObject* FindDrawableObject(unsigned long uHashID, unsigned long uOCMask);
    DrawableObject* FindDrawableObject(unsigned long uHashID);
    void HandleCameraSwitch();
    bool IsSphereInFrustum(const nlMatrix4& mat, float radius);
    static unsigned char IsSphereInFrustum(const nlVector4* pPlanes, const nlMatrix4& mWorld, float fRadius);
    void ExtractFrustumPlanes();
    void* GetCustomSpecularData(glModelPacket* pPacket, bool bPerm);
    void CreateLightUserData();
    void AssignLightBitmasks();
    void CreateWorldObjFromChunk(nlChunk* pChunk);
    void CreateLightObjFromChunk(nlChunk* pChunk);
    void CreateEmitterObjFromChunk(nlChunk* pChunk);
    bool LoadPhysicsPrimitives(nlChunk* pChunk);
    bool LoadObjectData(const char* szWorldName);
    static unsigned char LoadShadowVolumes(const char* szFileName);
    void AddToHyperSTSDrawables(unsigned long key, DrawableModel* pDrawableModel);
    GLShadowVolume* FindShadowVolumeByID(unsigned long uHashID);
    glModel* FindModelByID(unsigned long uHashID);
    bool LoadGeometry(glModel* gModel, unsigned long uNumModels, bool bMakeDrawables, bool keepTransform, unsigned long* pDrawableObjectHashes, int* pNumObjectsLoaded, bool bTrophy);
    bool LoadGeometry(const char* szWorldName, bool bMakeDrawables, bool keepTransform, unsigned long* pDrawableObjectHashes, int* pNumObjectsLoaded);
    bool Load(bool forfe);
    void DrawCullingInformation(int nNumSubmitted, int nNumDrawn);
    void DrawAdditionalBalls(DrawableObject* pObject);

    /* 0x004 */ WorldAnimManager* m_pWorldAnimManager;
    /* 0x008 */ nlAVLTree<unsigned long, LightObject*, DefaultKeyCompare<unsigned long> > m_lightMap;
    /* 0x01C */ bool m_Locked;
    /* 0x020 */ struct glModel* m_pModels;
    /* 0x024! */ unsigned long m_uNumModels;
    /* 0x028! */ nlDLListContainer<WorldAnimController*> m_animControllerList;
    /* 0x030 */ void* m_pLightData;
    /* 0x034 */ void* m_pPlayerNISLightData;
    /* 0x038 */ void* m_pIntensityPerm;
    /* 0x03C */ void* m_pIntensityData;
    /* 0x040 */ void* m_pSTSIntensity;
    /* 0x044 */ nlAVLTree<unsigned long, DrawableObject*, DefaultKeyCompare<unsigned long> > m_drawableMap;
    /* 0x060! */ nlAVLTree<unsigned long, DrawableObject*, DefaultKeyCompare<unsigned long> > m_hyperSTSDrawableMap;
    /* 0x070 */ nlAVLTree<unsigned long, HelperObject*, DefaultKeyCompare<unsigned long> > m_helperMap;
    /* 0x080 */ nlVector4 m_frustumPlane[6];
    /* 0x0E0 */ mutable char m_WorldNamePrefix[MAX_NAME_LENGTH];
    /* 0x120 */ int m_WorldNameLength;
    /* 0x124 */ u32 m_LightRampTexA;
    /* 0x128 */ u32 m_LightRampTexB;
    /* 0x12C */ u32 m_PlayerLightRampTex;
    /* 0x130 */ u32 m_GlobalLightRampSTSTex;
    /* 0x134 */ CharacterPhysicsData* m_pPhysicsData;
    /* 0x138 */ const LightObject* m_pShadowLight;
}; // total size: 0x13C

inline void World::FixedUpdate(float fTimeDelta)
{
}

inline void World::HandleEvent(Event* pEvent, void* pData)
{
}

#endif // _WORLD_H_
