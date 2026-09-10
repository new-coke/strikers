#ifndef _EFFECTSGROUP_H_
#define _EFFECTSGROUP_H_

#include "types.h"
#include "NL/nlMath.h"

#include "Game/Effects/EffectsTemplate.h"
#include "Game/Sys/simpleparser.h"

enum eFXBinding
{
    FXBind_Emitter = 0,
    FXBind_Joint = 1,
    FXBind_Object = 2,
    FXBind_Num = 3,
};
enum eJointBinding
{
    JB_Normal = 0,
    JB_Ascend = 1,
    JB_Num = 2,
};

class EffectsTerrainSpec
{
public:
    EffectsTerrainSpec();
    ~EffectsTerrainSpec();

    unsigned long GetHashID() const;
    bool HasTerrain(unsigned long terrainID) const;

    unsigned long* m_pTerrainIDs;
    unsigned long m_uNumTerrains;
};

class EffectsSpec
{
public:
    EffectsSpec();

    /* 0x00 */ u32 m_uHashID;
    /* 0x04 */ EffectsTemplate* m_pTemplate;
    /* 0x08 */ eFXBinding m_eAttach;
    /* 0x0C */ u32 m_uJointID;
    /* 0x10 */ f32 m_fDelay;
    /* 0x14 */ u32 m_uLayer;
    /* 0x18 */ eJointBinding m_eJointBinding;
    /* 0x1C */ f32 m_fJointVelocity;
    /* 0x20 */ bool m_bInFront;
    /* 0x21 */ bool m_bGround;
    /* 0x22 */ bool m_bLight;
    /* 0x24 */ f32 m_fOffset;
    /* 0x28 */ nlVector3 m_vLocalOffset;
    /* 0x34 */ EffectsTerrainSpec* m_pTerrainSpec;
    /* 0x38 */ f32 m_fLingerStart;
    /* 0x3C */ f32 m_fLingerEnd;
};

struct UserEffectInfo
{
    nlVector3* pv3Position;
    nlVector3* pv3Direction;
};

class UserEffectSpec
{
public:
    virtual ~UserEffectSpec();
    virtual void Update(float dt, UserEffectInfo* info);
    virtual void Render(const nlVector3** pVectors, s32 view);
    virtual u8 IsFinished();
};

class EffectsGroup
{
public:
    EffectsGroup();
    ~EffectsGroup();

    void SetSpecs(int numSpecs, EffectsSpec* specs);
    void SetUserSpecs(int numSpecs, UserEffectSpec** specs);
    bool IsPersistent() const;
    unsigned long GetHashID() const { return m_hashID; }

    /* 0x00 */ u32 m_hashID;
    /* 0x04 */ EffectsSpec* m_specs;
    /* 0x08 */ int m_numSpecs;
    /* 0x0C */ bool m_isLingering;
    /* 0x10 */ UserEffectSpec** m_userSpecsPtr;
    /* 0x14 */ s32 m_userSpecs;
}; // size 0x18

EffectsGroup* fxGetGroup(const char* groupName);
bool fxUnloadGroups();
bool fxLoadGroupBundle(void* data, unsigned long size);
bool fxLoadGroupBundle(const char* filename);

static EffectsGroup* parse_group(SimpleParser* parser);
static EffectsTerrainSpec* parse_terrain_spec(SimpleParser* parser);
static bool parse_spec(SimpleParser* parser, EffectsSpec& spec);

// class nlAVLTree<unsigned long, EffectsTerrainSpec*, DefaultKeyCompare<unsigned long>>
// {
// public:
// };

// class EffectsSpec
// {
// public:
// };

// class EffectsTerrainSpec
// {
// public:
// };

// class AVLTreeBase<unsigned long, EffectsGroup*, NewAdapter<AVLTreeEntry<unsigned long, EffectsGroup*>>, DefaultKeyCompare<unsigned long>>
// {
// public:
//     void PostorderTraversal(AVLTreeEntry<unsigned long, EffectsGroup*>*, void (AVLTreeBase<unsigned long, EffectsGroup*,
//     NewAdapter<AVLTreeEntry<unsigned long, EffectsGroup*>>, DefaultKeyCompare<unsigned long>>::*)(AVLTreeEntry<unsigned long,
//     EffectsGroup*>*)); void DestroyTree(void (AVLTreeBase<unsigned long, EffectsGroup*, NewAdapter<AVLTreeEntry<unsigned long,
//     EffectsGroup*>>, DefaultKeyCompare<unsigned long>>::*)(AVLTreeEntry<unsigned long, EffectsGroup*>*)); void Clear(); void
//     ~AVLTreeBase(); void DeleteEntry(AVLTreeEntry<unsigned long, EffectsGroup*>*); void DeleteValue(AVLTreeEntry<unsigned long,
// };

// class AVLTreeBase<unsigned long, EffectsTerrainSpec*, NewAdapter<AVLTreeEntry<unsigned long, EffectsTerrainSpec*>>,
// DefaultKeyCompare<unsigned long>>
// {
// public:
//     void PostorderTraversal(AVLTreeEntry<unsigned long, EffectsTerrainSpec*>*, void (AVLTreeBase<unsigned long, EffectsTerrainSpec*,
//     NewAdapter<AVLTreeEntry<unsigned long, EffectsTerrainSpec*>>, DefaultKeyCompare<unsigned long>>::*)(AVLTreeEntry<unsigned long,
//     EffectsTerrainSpec*>*)); void DestroyTree(void (AVLTreeBase<unsigned long, EffectsTerrainSpec*, NewAdapter<AVLTreeEntry<unsigned
// };

// class nlAVLTree<unsigned long, EffectsGroup*, DefaultKeyCompare<unsigned long>>
// {
// public:
// };

// class NewAdapter<AVLTreeEntry<unsigned long, EffectsGroup*>>
// {
// public:
// };

// class NewAdapter<AVLTreeEntry<unsigned long, EffectsTerrainSpec*>>
// {
// public:
// };

// class DLListContainerBase<UserEffectSpec*, NewAdapter<DLListEntry<UserEffectSpec*>>>
// {
// public:
// };

// class nlWalkDLRing<DLListEntry<UserEffectSpec*>, DLListContainerBase<UserEffectSpec*,
// NewAdapter<DLListEntry<UserEffectSpec*>>>>(DLListEntry<UserEffectSpec*>*, DLListContainerBase<UserEffectSpec*,
// NewAdapter<DLListEntry<UserEffectSpec*>>>*, void (DLListContainerBase<UserEffectSpec*, NewAdapter<DLListEntry<UserEffectSpec*>>>
// {
// public:
// };

// class nlWalkRing<DLListEntry<UserEffectSpec*>, DLListContainerBase<UserEffectSpec*,
// NewAdapter<DLListEntry<UserEffectSpec*>>>>(DLListEntry<UserEffectSpec*>*, DLListContainerBase<UserEffectSpec*,
// NewAdapter<DLListEntry<UserEffectSpec*>>>*, void (DLListContainerBase<UserEffectSpec*, NewAdapter<DLListEntry<UserEffectSpec*>>>
// {
// public:
// };

#endif // _EFFECTSGROUP_H_
