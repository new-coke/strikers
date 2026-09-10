#include "Game/World/worldanim.h"
#include "Game/World.h"
#include "NL/nlString.h"
#include "NL/nlSlotPool.h"
#include "Game/SAnim.h"

/**
 * Offset/Address/Size: 0x5C8 | 0x8019B394 | size: 0x94
 */
WorldAnimManager::WorldAnimManager()
{
    m_pHierarchyInventory = new (nlMalloc(sizeof(cInventory<cSHierarchy>), 8, false)) cInventory<cSHierarchy>();
}

/**
 * Offset/Address/Size: 0x14C | 0x8019AF18 | size: 0x41C
 */
WorldAnimManager::~WorldAnimManager()
{
    typedef nlAVLTreeIterator<unsigned long, AnimationSet*, DefaultKeyCompare<unsigned long> > AnimationSetIterator;

    AnimationSetIterator* iterator = (AnimationSetIterator*)nlMalloc(sizeof(AnimationSetIterator), 8, false);
    new (iterator) AnimationSetIterator(m_animationSetMap);
    while (iterator->IsValid())
    {
        delete iterator->Current()->value;
        iterator->Next();
    }
    delete iterator;
    delete m_pHierarchyInventory;
}

/**
 * Unreferenced in retail, dead-stripped at link. The retail DWARF for this
 * compile unit attests the definition (erased body: this r0, szFileName r4)
 * and worldanim.h's WorldAnimManager class DIE declares it.
 */
unsigned char WorldAnimManager::LoadHierarchy(const char* szFileName)
{
    m_pHierarchyInventory->AddFile((char*)szFileName);
    return true;
}

/**
 * Unreferenced in retail, dead-stripped at link. The retail DWARF for this
 * compile unit attests the definition (erased body: this r27, szFileName r26,
 * szSetName r28; locals AnimationSet* pAnimationSet r1+0x14,
 * char szLowerSetName[32] r1+0x18).
 */
unsigned char WorldAnimManager::LoadAnimationSet(const char* szFileName, const char* szSetName)
{
    AnimationSet* pAnimationSet;
    char szLowerSetName[32];

    nlStrNCpy(szLowerSetName, szSetName, 32);
    nlToLower(szLowerSetName);

    pAnimationSet = new (nlMalloc(sizeof(AnimationSet), 8, false)) AnimationSet();
    pAnimationSet->m_animInventory.AddFile((char*)szFileName);
    m_animationSetMap.Add(nlStringHash(szLowerSetName), pAnimationSet);
    return true;
}

/**
 * Unreferenced in retail, dead-stripped at link. The retail DWARF for this
 * compile unit attests the definition (erased body: this r31, szName r4).
 */
cSHierarchy* WorldAnimManager::FindHierarchy(const char* szName)
{
    return m_pHierarchyInventory->Find((unsigned int)nlStringLowerHash(szName));
}

/**
 * Unreferenced in retail, dead-stripped at link. The retail DWARF for this
 * compile unit attests the definition (erased body: this r31,
 * szAnimationSetName r4; local AnimationSet** ppAnimationSet r1+0x8).
 */
AnimationSet* WorldAnimManager::FindAnimationSet(const char* szAnimationSetName)
{
    AnimationSet** ppAnimationSet = NULL;
    m_animationSetMap.Find(nlStringLowerHash(szAnimationSetName), &ppAnimationSet, NULL);
    if (ppAnimationSet != NULL)
    {
        return *ppAnimationSet;
    }
    return NULL;
}

/**
 * Unreferenced in retail, dead-stripped at link. The retail DWARF for this
 * compile unit attests the definition (erased body: uHashID r0,
 * szAnimationName r5; local AnimationSet** ppAnimationSet r1+0x8).
 */
cSAnim* WorldAnimManager::FindAnimation(unsigned long uHashID, const char* szAnimationName)
{
    AnimationSet** ppAnimationSet = NULL;
    m_animationSetMap.Find(uHashID, &ppAnimationSet, NULL);
    if (ppAnimationSet != NULL)
    {
        return (*ppAnimationSet)->m_animInventory.Find((unsigned int)nlStringLowerHash(szAnimationName));
    }
    return NULL;
}

/**
 * Unreferenced in retail, dead-stripped at link. The retail DWARF for this
 * compile unit attests the definition (erased body: this r31,
 * szAnimSetAndHierarchyName r28, pWorldContext r29; references
 * __vt__19WorldAnimController).
 */
WorldAnimController::WorldAnimController(const char* szAnimSetAndHierarchyName, World* pWorldContext)
{
    m_pPoseAccumulator = NULL;
    m_pPoseTree = NULL;
    m_fSpeed = 1.0f;
    m_pAnimationSet = pWorldContext->m_pWorldAnimManager->FindAnimationSet(szAnimSetAndHierarchyName);
    m_bIsGanged = false;
}

/**
 * Unreferenced in retail, dead-stripped at link. Out-of-line per the retail
 * DWARF (erased body: this r29; references __vt__19WorldAnimController).
 */
WorldAnimController::~WorldAnimController()
{
}

/**
 * Offset/Address/Size: 0x58 | 0x8019AE24 | size: 0xF4
 */
void WorldAnimController::SetAnimation(const char* szAnimationName, ePlayMode playMode)
{
    u32 hash = nlStringLowerHash(szAnimationName);
    cSAnim* anim = m_pAnimationSet->m_animInventory.Find((unsigned int)hash);

    if (m_pPoseTree != NULL)
    {
        delete m_pPoseTree;
    }

    cPN_SAnimController* newController = AllocateSAnimController();
    newController = ::new (newController) cPN_SAnimController(anim, NULL, playMode, NULL, 0, false);
    m_pPoseTree = newController;
}

/**
 * Offset/Address/Size: 0x44 | 0x8019AE10 | size: 0x14
 */
void WorldAnimController::SetAnimationTime(float fTime)
{
    cPN_SAnimController* cntrl = m_pPoseTree;
    cntrl->m_fPrevTime = cntrl->m_fTime;
    cntrl->m_fTime = fTime;
}

/**
 * Offset/Address/Size: 0x38 | 0x8019AE04 | size: 0xC
 */
float WorldAnimController::GetAnimationTime()
{
    return m_pPoseTree->m_fTime;
}

/**
 * Offset/Address/Size: 0x0 | 0x8019ADCC | size: 0x38
 */
float WorldAnimController::GetAnimationDuration()
{
    return (float)m_pPoseTree->m_pSAnim->m_nNumKeys / 30.0f;
}
