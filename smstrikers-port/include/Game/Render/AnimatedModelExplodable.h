#ifndef _ANIMATEDMODELEXPLODABLE_H_
#define _ANIMATEDMODELEXPLODABLE_H_

#include "types.h"
#include "NL/nlMath.h"
#include "Game/Render/SidelineExplodable.h"
#include "Game/Render/SkinAnimatedNPC.h"

enum AnimatedModelExplodableCategory
{
    EXPLODABLE_CAMERAGUY = 0,
    EXPLODABLE_STANDUPCAMERA = 1,
    NUM_ANIMATED_MODEL_EXPLODABLE_CATEGORIES = 2,
};

class AnimatedModelExplodable : public SidelineExplodable
{
public:
    static nlList<SidelineExplodableNode> sAnimatedModelExplodableList;
    static ExplodableCategoryData sCategoryData[NUM_ANIMATED_MODEL_EXPLODABLE_CATEGORIES];
    static bool bIsModelLoaded[2];
    static void CleanUp();
    AnimatedModelExplodable(AnimatedModelExplodableCategory category, SkinAnimatedNPC* pAnimatedNPC);
    ~AnimatedModelExplodable();
    virtual ExplodableCategoryData& GetCategoryData() const;
    void SetUnexplodedModelVisibility(bool isVisible);
    virtual const nlMatrix4& GetWorldMatrix() const;
    static bool LoadGeometry();

    /* 0x2C */ SkinAnimatedNPC* mpAnimatedNPC;
    /* 0x30 */ AnimatedModelExplodableCategory mCategory;
}; // total size: 0x34

#endif // _ANIMATEDMODELEXPLODABLE_H_
