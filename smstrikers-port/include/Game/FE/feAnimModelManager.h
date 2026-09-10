#ifndef _FEANIMMODELMANAGER_H_
#define _FEANIMMODELMANAGER_H_

#include "types.h"
#include "NL/nlSingleton.h"
#include "Game/Inventory.h"
#include "Game/FE/feBasic3dModel.h"
#include "Game/SAnim.h"

class FEAnimModel;
class cSAnim;

class FEAnimModelManager : public nlSingleton<FEAnimModelManager>
{
public:
    virtual ~FEAnimModelManager();
    FEAnimModelManager();

    void Update(float dt);
    void Initialize();

    // Inlined into Update() in the original (see dwarf.txt). Defined in the .cpp.
    void CheckTweakModelSwitch();
    void TweakModelPosition();

    // ---------------------------------------------------------------------------
    // RECONSTRUCTED inline helpers. These methods are "Erased" (inlined) in the
    // original TU (see dwarf.txt). The signatures come from DWARF; the bodies are
    // best-effort reconstructions and are NOT byte-verified against the target.
    // Added so future decomp runs of callers (AnimInventory, CharacterTemplate,
    // FrontEndTask, ...) can inline them. REVISE each body before trusting it.
    //
    // NOTE: character-class params are typed `int` to avoid pulling the heavy
    // Game/Character.h into this header; the real type is `eCharacterClass`.
    // ---------------------------------------------------------------------------

    cSAnim* GetAnim(unsigned int animHashID)
    {
        return mAnimInventory.Find(animHashID);
    }

    // REVISE: advance the tweak-model cursor. This branch matches the inlined
    // copy currently in Update(); the wrap-to-head fallback is assumed.
    void GetNextTweakModel()
    {
        if (mCurrentTweakModel == NULL)
            mCurrentTweakModel = mFEAnimModelTweakList;
        else
            mCurrentTweakModel = mCurrentTweakModel->next;
    }

    // REVISE: step the tweak-model cursor backwards (mirror of GetNextTweakModel,
    // unconfirmed -- the real one may walk to the list tail on wrap).
    void GetPrevTweakModel()
    {
        if (mCurrentTweakModel == NULL)
            mCurrentTweakModel = mFEAnimModelTweakList;
        else
            mCurrentTweakModel = mCurrentTweakModel->prev;
    }

    // REVISE: insert a model at the head of the doubly-linked tweak list.
    void AddModelToTweakList(FEBasic3dModel* model)
    {
        model->prev = NULL;
        model->next = mFEAnimModelTweakList;
        if (mFEAnimModelTweakList != NULL)
            mFEAnimModelTweakList->prev = model;
        mFEAnimModelTweakList = model;
    }

    // REVISE: unlink a model from the tweak list.
    void RemoveModelFromTweakList(FEBasic3dModel* femodel)
    {
        if (femodel->prev != NULL)
            femodel->prev->next = femodel->next;
        else
            mFEAnimModelTweakList = femodel->next;
        if (femodel->next != NULL)
            femodel->next->prev = femodel->prev;
    }

    // REVISE: register a loaded model in the per-class slot array.
    void RegisterLoaded(int cc, FEAnimModel* animmodel)
    {
        mLoadedModels[cc] = animmodel;
    }

    // REVISE: true if a model for this class slot is already loaded.
    u8 IsAlreadyLoaded(int cc) const
    {
        return mLoadedModels[cc] != NULL;
    }

    // REVISE: PURE PLACEHOLDER. The original builds a swap-texture identifier from
    // the two character classes (DWARF shows a char[64] name buffer + a hash), so
    // the real body almost certainly formats a string and hashes it. Replace this.
    static unsigned long BuildSwapTextureID(int sidekickcc, int captaincc)
    {
        return ((unsigned long)captaincc << 16) | (unsigned long)sidekickcc;
    }

    /* 0x04 */ FEAnimModel* mLoadedModels[22];
    /* 0x5C */ void* mLightData;
    /* 0x60 */ void* mSpecularData;
    /* 0x64 */ FEBasic3dModel* mFEAnimModelTweakList;
    /* 0x68 */ FEBasic3dModel* mCurrentTweakModel;
    /* 0x6C */ cInventory<cSAnim> mAnimInventory;
    /* 0x88 */ u8 mTweak3dModels;
};

#endif // _FEANIMMODELMANAGER_H_
