#include "Game/Render/AnimatedModelExplodable.h"

nlList<SidelineExplodableNode> AnimatedModelExplodable::sAnimatedModelExplodableList((SidelineExplodableNode*)NULL, (SidelineExplodableNode*)NULL);

ExplodableCategoryData AnimatedModelExplodable::sCategoryData[NUM_ANIMATED_MODEL_EXPLODABLE_CATEGORIES] = {
    ExplodableCategoryData(
        "environment/Sideline_Objects/camera_base",
        "environment/Sideline_Objects/camera_d",
        NULL),
    ExplodableCategoryData(
        "environment/Sideline_Objects/standupcamera_base",
        "environment/Sideline_Objects/standupcamera_d",
        NULL),
};

bool AnimatedModelExplodable::bIsModelLoaded[2];

/**
 * Offset/Address/Size: 0x1C4 | 0x80158B30 | size: 0xAC
 */
void AnimatedModelExplodable::CleanUp()
{
    long zero = 0;
    SlotPoolBase* pPool = &SidelineExplodableNode::sSidelineExplodableNodeSlotPool;
    SidelineExplodableNode** pTail = &sAnimatedModelExplodableList.m_pEnd;
    SidelineExplodableNode* node;
    while ((node = sAnimatedModelExplodableList.m_pStart) != NULL)
    {
        nlListRemoveStart<SidelineExplodableNode>(&sAnimatedModelExplodableList.m_pStart, pTail);
        SidelineExplodable* pExplodable = node->mpExplodable;
        if (pExplodable != NULL)
        {
            SidelineExplodableManager::RemoveSidelineExplodable(pExplodable);
            delete node->mpExplodable;
        }
        node->mpExplodable = (SidelineExplodable*)zero;
        ((SlotPoolEntry*)node)->next = pPool->m_FreeList;
        pPool->m_FreeList = (SlotPoolEntry*)node;
    }
}

/**
 * Offset/Address/Size: 0xA8 | 0x80158A14 | size: 0x11C
 */
AnimatedModelExplodable::AnimatedModelExplodable(AnimatedModelExplodableCategory category, SkinAnimatedNPC* pAnimatedNPC)
{
    mpAnimatedNPC = pAnimatedNPC;
    mCategory = category;

    if (!bIsModelLoaded[category])
    {
        sCategoryData[category].LoadGeometry();
        bIsModelLoaded[category] = 1;
    }

    Initialize(GetCategoryData().mNumFragmentModels);

    SidelineExplodableNode* node = SidelineExplodableNode::sSidelineExplodableNodeSlotPool.Allocate();

    if (node != NULL)
    {
        node->mpExplodable = NULL;
        node->next = NULL;
    }

    node->mpExplodable = this;
    nlListAddEnd<SidelineExplodableNode>(&sAnimatedModelExplodableList.m_pStart, &sAnimatedModelExplodableList.m_pEnd, node);
}

/**
 * Offset/Address/Size: 0x48 | 0x801589B4 | size: 0x60
 */
AnimatedModelExplodable::~AnimatedModelExplodable()
{
}

/**
 * Offset/Address/Size: 0x30 | 0x8015899C | size: 0x18
 */
ExplodableCategoryData& AnimatedModelExplodable::GetCategoryData() const
{
    return sCategoryData[mCategory];
}

/**
 * Offset/Address/Size: 0x24 | 0x80158990 | size: 0xC
 */
const nlMatrix4& AnimatedModelExplodable::GetWorldMatrix() const
{
    return mpAnimatedNPC->mWorldMatrix;
}

/**
 * Offset/Address/Size: 0x18 | 0x80158984 | size: 0xC
 */
void AnimatedModelExplodable::SetUnexplodedModelVisibility(bool isVisible)
{
    mpAnimatedNPC->mbIsVisible = isVisible;
}

/**
 * Offset/Address/Size: 0x0 | 0x8015896C | size: 0x18
 */
bool AnimatedModelExplodable::LoadGeometry()
{
    bIsModelLoaded[0] = false;
    bIsModelLoaded[1] = false;
    return true;
}
