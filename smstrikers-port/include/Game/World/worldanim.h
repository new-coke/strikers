#ifndef _WORLDANIM_H_
#define _WORLDANIM_H_

#include "NL/nlList.h"
#include "NL/nlAVLTree.h"
#include "Game/Inventory.h"
#include "Game/SAnim/pnSAnimController.h"
#include "Game/SAnim.h"

class AnimationSet
{
public:
    /* 0x0 */ cInventory<cSAnim> m_animInventory;
}; // total size: 0x1C

struct glModel;
class World;
class DrawableObject;
class DrawableTmModel;
class DrawableSkinModel;
class GLSkinMesh;

class WorldAnimController
{
public:
    WorldAnimController(const char* szAnimSetAndHierarchyName, World* pWorldContext);
    virtual ~WorldAnimController();
    virtual void Update(float fTimeDelta) { };
    virtual glModel* GetUpdatedModel(unsigned long program, void* pLightData) { return 0; };

    void SetAnimation(const char* szAnimationName, ePlayMode playMode);
    void SetAnimationTime(float fTime);
    float GetAnimationTime();
    float GetAnimationDuration();

public:
    /* 0x04 */ cPoseAccumulator* m_pPoseAccumulator;
    /* 0x08 */ cPN_SAnimController* m_pPoseTree;
    /* 0x0C */ float m_fSpeed;
    /* 0x10 */ AnimationSet* m_pAnimationSet;
    /* 0x14 */ bool m_bIsGanged;
}; // total size: 0x18

class TMAnimController : public WorldAnimController
{
    friend class World;

public:
    TMAnimController(const char* szAnimSetAndHierarchyName, World* pWorldContext);
    virtual ~TMAnimController();
    virtual void Update(float fTimeDelta);
    virtual glModel* GetUpdatedModel();

    void ForceFrameRebuild();
    unsigned char BindGeomToAnimNode(const char* szGeomName, const char* szBoneName);
    unsigned char BindGeomToAnimNode(DrawableTmModel* pTMModel, const char* szBoneName);

protected:
    /* 0x18 */ DrawableObject* m_pRootModel;
    /* 0x1C */ unsigned long m_uLastFrameUpdated;
    /* 0x20 */ World* m_pWorldContext;
}; // total size: 0x24

class SkinnedAnimController : public WorldAnimController
{
public:
    SkinnedAnimController(const char* szAnimSetAndHierarchyName, World* pWorldContext);
    virtual ~SkinnedAnimController();
    virtual void Update(float fTimeDelta);
    virtual glModel* GetUpdatedModel(unsigned long program, void* pLightData)
    {
        UpdateSkinnedMesh(program, pLightData);
        return m_pCachedSkinnedModel;
    }

    void UpdateSkinnedMesh(unsigned long program, void* pLightData);
    void UpdateAnimation(float fTimeDelta, const nlMatrix4& worldMatrix);
    void CreateGLSkinMesh(glModel* pModel);

public:
    /* 0x18 */ GLSkinMesh* m_pSkinMesh;
    /* 0x1C */ glModel* m_pCachedSkinnedModel;
    /* 0x20 */ DrawableSkinModel* m_pSkinModel;

protected:
    /* 0x24 */ unsigned long m_uLastFrameUpdated;
    /* 0x28 */ unsigned long m_uLastFrameUpdatedSkinMesh;
    /* 0x2C */ unsigned char m_bDisabled;
}; // total size: 0x30

class WorldAnimManager
{
public:
    WorldAnimManager();
    ~WorldAnimManager();

    unsigned char LoadHierarchy(const char* szFileName);
    unsigned char LoadAnimationSet(const char* szFileName, const char* szSetName);
    cSHierarchy* FindHierarchy(const char* szName);
    AnimationSet* FindAnimationSet(const char* szAnimationSetName);
    cSAnim* FindAnimation(unsigned long uHashID, const char* szAnimationName);

    /* 0x00 */ cInventory<cSHierarchy>* m_pHierarchyInventory;
    /* 0x04 */ nlAVLTree<unsigned long, AnimationSet*, DefaultKeyCompare<unsigned long> > m_animationSetMap;
}; // total size: 0x18

#endif // _WORLDANIM_H_
