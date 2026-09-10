#ifndef _SCREENTRANSITIONMANAGER_H_
#define _SCREENTRANSITIONMANAGER_H_

#include "NL/gl/glView.h"
#include "NL/nlSingleton.h"
#include "NL/nlVector.h"
#include "NL/nlAVLTree.h"
#include "NL/nlBasicString.h"

class ScreenTransition
{
public:
    /* 0x08 */ virtual ~ScreenTransition() { };
    /* 0x0C */ virtual void Update(float) = 0;
    /* 0x10 */ virtual void Render(eGLView) = 0;
    /* 0x14 */ virtual bool IsFinished() = 0;
    /* 0x18 */ virtual float Time() const = 0;
    /* 0x1C */ virtual float CutTime() const { return 0.0f; };
    /* 0x20 */ virtual void Reset() = 0;
    /* 0x24 */ virtual void Cancel() = 0;
    /* 0x28 */ virtual float GetTransitionLength() = 0;
    /* 0x2C */ virtual void DoSanityCheck() { };
};

class ScreenTransitionCallback
{
public:
    virtual inline void TransitionFinished();
    virtual inline void TransitionProgressed(float fDeltaT);
    virtual inline void Cut();
    virtual inline void SequenceSwitch();
};

class ScreenTransitionManager : public nlSingleton<ScreenTransitionManager>
{
public:
    ScreenTransitionManager();
    virtual ~ScreenTransitionManager();
    void Render(float dt);
    void CancelAllTransitions();
    void DeleteAllTransitions();
    void AddTransitionToMap(char* name, ScreenTransition* pTransition);
    void EnableRandomTransition(const char* filter);
    void SelectRandomTransition(const char* filter);
    float GetSelectedTransitionCutTime() const;
    void EnableSelectedTransition();
    void AddTransitions(char* loadedData, unsigned long fileSize);

    /* 0x04 */ ScreenTransition* m_pActiveTransition;
    /* 0x08 */ nlAVLTree<unsigned long, ScreenTransition*, DefaultKeyCompare<unsigned long> > m_TransitionMap;

    /* 0x1C */ eGLView m_eView;
    /* 0x20 */ ScreenTransitionCallback* m_pCallback;
    /* 0x24 */ ScreenTransition* m_SelectedTransition;
    /* 0x28 */ float m_fCurrentTime;
    /* 0x2C */ float m_fCurrentLength;
    // /* 0x30 */ Vector<ScreenTransition, DefaultAllocator> m_Transitions;
    /* 0x30 */ Vector<BasicString<char, Detail::TempStringAllocator>, DefaultAllocator> m_Transitions;
    /* 0x3C */ bool m_Cut;
}; // total size: 0x40

// class nlAVLTree<unsigned long, ScreenTransition*, DefaultKeyCompare<unsigned long> >
// {
// public:
// };

// class BasicString < char, Detail
// {
// public:
// };

// class Vector < BasicString < char, Detail
// {
// public:
// };

// class AVLTreeBase<unsigned long, ScreenTransition*, NewAdapter<AVLTreeEntry<unsigned long, ScreenTransition*> >, DefaultKeyCompare<unsigned long> >
// {
// public:
// };

// class NewAdapter<AVLTreeEntry<unsigned long, ScreenTransition*> >
// {
// public:
// };

#endif // _SCREENTRANSITIONMANAGER_H_
