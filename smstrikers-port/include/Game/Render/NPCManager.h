#ifndef _NPCMANAGER_H_
#define _NPCMANAGER_H_

#include "types.h"

#include "Game/Inventory.h"
#include "Game/Render/Bowser.h"
#include "Game/Render/ChainChomp.h"

struct NPCTemplate
{
    /* 0x00 */ bool loaded;            // offset 0x0, size 0x1
    /* 0x04 */ s32 modelID;            // offset 0x4, size 0x4
    /* 0x08 */ cSHierarchy* hierarchy; // offset 0x8, size 0x4
}; // total size: 0xC

class NPCManager
{
public:
    NPCManager();
    virtual ~NPCManager();

    void UpdateNPCs(float dt);
    void RenderNPCs();
    void UpdateAINPCs(float dt);
    void CreateNPCTemplate(int templateIndex, bool loadTextures);

    /* 0x04 */ cInventory<cSAnim>* mpInventorySAnim;
    /* 0x08 */ cInventory<cSHierarchy>* mpInventorySHierarchy;
    /* 0x0C */ nlListContainer<SkinAnimatedNPC*> mNPCList;
    /* 0x18 */ ChainChomp* mpChainChomp;
    /* 0x1C */ Bowser* mpBowser;
    /* 0x20 */ NPCTemplate mNPCTemplate[7];
}; // total size: 0x74

// class ListContainerBase<SkinAnimatedNPC*, NewAdapter<ListEntry<SkinAnimatedNPC*>>>
// {
// public:
// };

// class nlWalkList<ListEntry<SkinAnimatedNPC*>, ListContainerBase<SkinAnimatedNPC*, NewAdapter<ListEntry<SkinAnimatedNPC*>>>>(ListEntry<SkinAnimatedNPC*>*, ListContainerBase<SkinAnimatedNPC*, NewAdapter<ListEntry<SkinAnimatedNPC*>>>*, void (ListContainerBase<SkinAnimatedNPC*, NewAdapter<ListEntry<SkinAnimatedNPC*>>>
// {
// public:
// };

#endif // _NPCMANAGER_H_
