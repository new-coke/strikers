#ifndef _MODELTRANSITION_H_
#define _MODELTRANSITION_H_

#include "NL/nlMath.h"
#include "NL/nlAVLTree.h"
#include "Game/Sys/simpleparser.h"

#include "Game/Transitions/TransLight.h"
#include "Game/Transitions/ScreenTransitionManager.h"

#include "Game/PoseAccumulator.h"
#include "Game/Effects/EmissionController.h"

#include "Game/SHierarchy.h"
#include "Game/SAnim.h"

struct TransitionModelStore
{
    glModel* pModels;
    u32 nModels;
};

class ModeledScreenTransition : public ScreenTransition
{
public:
    ModeledScreenTransition();

    virtual ~ModeledScreenTransition();
    virtual void Update(float deltaTime);
    virtual void Render(eGLView view);
    virtual bool IsFinished();
    virtual float Time() const;
    virtual void Reset();
    virtual void Cancel();
    virtual float GetTransitionLength();

    void DoSanityCheck();
    void RenderOutline() const;
    ModeledScreenTransition* LoadFromParser(SimpleParser* parser);
    void CreateInstance(TransitionModelStore& modelInfo);
    void Load(const char* szName);
    void FixupModel();

    /* 0x04 */ glModel* m_pModels;                   // offset 0x4, size 0x4
    /* 0x08 */ unsigned long m_nModels;                        // offset 0x8, size 0x4
    /* 0x0C */ cSHierarchy* m_pSkeleton;             // offset 0xC, size 0x4
    /* 0x10 */ cSAnim* m_pAnim;                      // offset 0x10, size 0x4
    /* 0x14 */ char* m_pSkelFile;                    // offset 0x14, size 0x4
    /* 0x18 */ char* m_pAnimFile;                    // offset 0x18, size 0x4
    /* 0x1C */ u32 m_nTexture;                       // offset 0x1C, size 0x4
    /* 0x20 */ cPoseAccumulator* m_pPoseAccumulator; // offset 0x20, size 0x4
    /* 0x24 */ cPN_SAnimController* m_pPoseTree;     // offset 0x24, size 0x4
    /* 0x28 */ nlMatrix4 m_mWorldMatrix;             // offset 0x28, size 0x40
    /* 0x68 */ bool m_bScreenGrab;                   // offset 0x68, size 0x1
    /* 0x69 */ bool m_bEnableGrab;                   // offset 0x69, size 0x1
    /* 0x6C */ int* m_pModelMap;                     // offset 0x6C, size 0x4
    /* 0x70 */ TransitionLight* m_pLight;            // offset 0x70, size 0x4
    /* 0x74 */ u32 m_nProgram;                       // offset 0x74, size 0x4
    /* 0x78 */ char m_EffectName[64];                // offset 0x78, size 0x40
    /* 0xB8 */ EmissionController** m_Effects;       // offset 0xB8, size 0x4
    /* 0xBC */ bool m_RenderOutline;                 // offset 0xBC, size 0x1
    /* 0xBD */ nlColour m_OutlineColour;             // offset 0xBD, size 0x4

    static eGLView s_3DView;
    static nlAVLTree<unsigned long, TransitionModelStore, DefaultKeyCompare<unsigned long> > g_ModelInventory;
}; // total size: 0xC4

// class ScreenTransition
// {
// public:
//     void DoSanityCheck();
//     ~ScreenTransition();
//     void CutTime() const;
// };

// class nlMatrix4
// {
// public:
//     void GetTranslation() const;
// };

// class nlAVLTree<unsigned long, TransitionModelStore, DefaultKeyCompare<unsigned long>>
// {
// public:
//     void ~nlAVLTree();
// };

// class AVLTreeBase<unsigned long, TransitionModelStore, NewAdapter<AVLTreeEntry<unsigned long, TransitionModelStore>>, DefaultKeyCompare<unsigned long>>
// {
// public:
//     void DeleteEntry(AVLTreeEntry<unsigned long, TransitionModelStore>*);
//     void AllocateEntry(void*, void*);
//     void CompareKey(void*, AVLTreeNode*);
//     void CompareNodes(AVLTreeNode*, AVLTreeNode*);
//     void CastUp(AVLTreeNode*) const;
//     void PostorderTraversal(AVLTreeEntry<unsigned long, TransitionModelStore>*, void (AVLTreeBase<unsigned long, TransitionModelStore, NewAdapter<AVLTreeEntry<unsigned long, TransitionModelStore>>, DefaultKeyCompare<unsigned long>>::*)(AVLTreeEntry<unsigned long, TransitionModelStore>*));
//     void DestroyTree(void (AVLTreeBase<unsigned long, TransitionModelStore, NewAdapter<AVLTreeEntry<unsigned long, TransitionModelStore>>, DefaultKeyCompare<unsigned long>>::*)(AVLTreeEntry<unsigned long, TransitionModelStore>*));
//     void Clear();
//     void ~AVLTreeBase();
// };

#endif // _MODELTRANSITION_H_
