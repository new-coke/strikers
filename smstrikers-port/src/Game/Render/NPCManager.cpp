#include "Game/Render/NPCManager.h"

#include "Game/Game.h"
#include "Game/SHierarchy.h"
#include "Game/GameInfo.h"
#include "Game/Render/AnimatedModelExplodable.h"
#include "Game/Render/CameraGuy.h"
#include "Game/WorldManager.h"
#include "NL/nlFile.h"
#include "NL/nlFileGC.h"
#include "NL/nlMemory.h"
#include "NL/nlString.h"
#include "NL/gl/gl.h"

struct NPCTemplateInfo
{
    /* 0x00 */ s32 id;
    /* 0x04 */ const char* modelFilename;
    /* 0x08 */ const char* textureFilename;
    /* 0x0C */ const char* hierarchyFilename;
    /* 0x10 */ const char* hierarchyName;
    /* 0x14 */ const char* animFilename;
    /* 0x18 */ u8 loadAnimsVirtual;
};

static const NPCTemplateInfo gNPCTemplateInfo[] = {
    { 0, "characters/cameraguy/cameraguy.glg", "characters/cameraguy/cameraguy.glt", "art/animation/cameraguy.shier", "cameraguy", "art/animation/cameraguy.sanim", false },
    { 1, "characters/standupcamera/standupcamera.glg", "characters/standupcamera/standupcamera.glt", "art/animation/standupcamera.shier", "standupcamera", "art/animation/standupcamera.sanim", false },
    { 2, "characters/medic/medic.glg", "characters/medic/medic.glt", "art/animation/medic.shier", "medic", "art/animation/medic.sanim", false },
    { 3, "characters/securityguard/securityguard.glg", "characters/securityguard/securityguard.glt", "art/animation/securityguard.shier", "securityguard", "art/animation/securityguard.sanim", false },
    { 4, "characters/blimp/blimp.glg", "characters/blimp/blimp.glt", "art/animation/blimp.shier", "blimp", "art/animation/blimp.sanim", false },
    { 5, "characters/chainchomp/chainchomp.glg", "characters/chainchomp/chainchomp.glt", "art/animation/chainchomp.shier", "chainchomp", "art/animation/chainchomp.sanim", false },
    { 6, "characters/bowser/bowser.glg", "characters/bowser/bowser.glt", "art/animation/bowser.shier", "bowser", "art/animation/bowser.sanim", true },
};

struct glModelData
{
    /* 0x00 */ u32 pad;
    /* 0x04 */ s32 numModels;
};

/**
 * Offset/Address/Size: 0x8AC | 0x80166770 | size: 0xB3C
 */
NPCManager::NPCManager()
    : mpInventorySAnim(NULL)
    , mpInventorySHierarchy(NULL)
{
    mpInventorySHierarchy = new (nlMalloc(sizeof(cInventory<cSHierarchy>), 8, false)) cInventory<cSHierarchy>();
    mpInventorySAnim = new (nlMalloc(sizeof(cInventory<cSAnim>), 8, false)) cInventory<cSAnim>();

    mNPCTemplate[0].loaded = false;
    mNPCTemplate[1].loaded = false;
    mNPCTemplate[2].loaded = false;
    mNPCTemplate[3].loaded = false;
    mNPCTemplate[4].loaded = false;
    mNPCTemplate[5].loaded = false;
    mNPCTemplate[6].loaded = false;

    World* world = WorldManager::s_World;

    typedef nlAVLTreeIterator<unsigned long, HelperObject*, DefaultKeyCompare<unsigned long> > HelperIterator;
    HelperIterator* stack = new (nlMalloc(sizeof(HelperIterator), 8, false)) HelperIterator(world->m_helperMap);

    while (stack->m_NumStackEntries > 0)
    {
        if (world->CompareNameToGenericName(stack->m_Stack[stack->m_NumStackEntries - 1]->value->m_szName, "cameraguy") == 0)
        {
            CreateNPCTemplate(0, true);
            cSAnim* foundAnim = mpInventorySAnim->Find("camera_idle");
            CameraGuy* guy = new (nlMalloc(sizeof(CameraGuy), 8, false)) CameraGuy(*mNPCTemplate[0].hierarchy, mNPCTemplate[0].modelID);
            guy->Init();
            guy->SetIdleAnim(*foundAnim);
            guy->mWorldMatrix = stack->m_Stack[stack->m_NumStackEntries - 1]->value->m_worldMatrix;
            ListEntry<SkinAnimatedNPC*>* listEntry = (ListEntry<SkinAnimatedNPC*>*)nlMalloc(8, 8, false);
            if (listEntry != NULL)
            {
                listEntry->next = NULL;
                listEntry->entry = guy;
            }
            nlListAddStart<ListEntry<SkinAnimatedNPC*> >(&mNPCList.m_Head, listEntry, &mNPCList.m_Tail);
            new (nlMalloc(sizeof(AnimatedModelExplodable), 8, false)) AnimatedModelExplodable(EXPLODABLE_CAMERAGUY, guy);
        }
        else if (world->CompareNameToGenericName(stack->m_Stack[stack->m_NumStackEntries - 1]->value->m_szName, "standupcamera") == 0)
        {
            CreateNPCTemplate(1, true);
            cSAnim* foundAnim = mpInventorySAnim->Find("standupcamera_idle");
            CameraGuy* guy = new (nlMalloc(sizeof(CameraGuy), 8, false)) CameraGuy(*mNPCTemplate[1].hierarchy, mNPCTemplate[1].modelID);
            guy->Init();
            guy->SetIdleAnim(*foundAnim);
            guy->mWorldMatrix = stack->m_Stack[stack->m_NumStackEntries - 1]->value->m_worldMatrix;
            ListEntry<SkinAnimatedNPC*>* listEntry = (ListEntry<SkinAnimatedNPC*>*)nlMalloc(8, 8, false);
            if (listEntry != NULL)
            {
                listEntry->next = NULL;
                listEntry->entry = guy;
            }
            nlListAddStart<ListEntry<SkinAnimatedNPC*> >(&mNPCList.m_Head, listEntry, &mNPCList.m_Tail);
            new (nlMalloc(sizeof(AnimatedModelExplodable), 8, false)) AnimatedModelExplodable(EXPLODABLE_STANDUPCAMERA, guy);
        }
        else if (world->CompareNameToGenericName(stack->m_Stack[stack->m_NumStackEntries - 1]->value->m_szName, "medic") == 0)
        {
            CreateNPCTemplate(2, true);
            cSAnim* foundAnim = mpInventorySAnim->Find("medic_idle");
            SkinAnimatedNPC* npc = new (nlMalloc(sizeof(SkinAnimatedNPC), 8, false)) SkinAnimatedNPC(*mNPCTemplate[2].hierarchy, mNPCTemplate[2].modelID);
            npc->SetAnimState(*foundAnim, 0.2f, (ePlayMode)0);
            npc->mWorldMatrix = stack->m_Stack[stack->m_NumStackEntries - 1]->value->m_worldMatrix;
            ListEntry<SkinAnimatedNPC*>* listEntry = (ListEntry<SkinAnimatedNPC*>*)nlMalloc(8, 8, false);
            if (listEntry != NULL)
            {
                listEntry->next = NULL;
                listEntry->entry = npc;
            }
            nlListAddStart<ListEntry<SkinAnimatedNPC*> >(&mNPCList.m_Head, listEntry, &mNPCList.m_Tail);
        }
        else if (world->CompareNameToGenericName(stack->m_Stack[stack->m_NumStackEntries - 1]->value->m_szName, "securityguard") == 0)
        {
            CreateNPCTemplate(3, true);
            cSAnim* foundAnim = mpInventorySAnim->Find("securityguard_idle");
            SkinAnimatedNPC* npc = new (nlMalloc(sizeof(SkinAnimatedNPC), 8, false)) SkinAnimatedNPC(*mNPCTemplate[3].hierarchy, mNPCTemplate[3].modelID);
            npc->SetAnimState(*foundAnim, 0.2f, (ePlayMode)0);
            npc->mWorldMatrix = stack->m_Stack[stack->m_NumStackEntries - 1]->value->m_worldMatrix;
            ListEntry<SkinAnimatedNPC*>* listEntry = (ListEntry<SkinAnimatedNPC*>*)nlMalloc(8, 8, false);
            if (listEntry != NULL)
            {
                listEntry->next = NULL;
                listEntry->entry = npc;
            }
            nlListAddStart<ListEntry<SkinAnimatedNPC*> >(&mNPCList.m_Head, listEntry, &mNPCList.m_Tail);
        }
        else if (world->CompareNameToGenericName(stack->m_Stack[stack->m_NumStackEntries - 1]->value->m_szName, "blimp") == 0)
        {
            CreateNPCTemplate(4, true);
            cSAnim* foundAnim = mpInventorySAnim->Find("blimp_idle");
            SkinAnimatedNPC* npc = new (nlMalloc(sizeof(SkinAnimatedNPC), 8, false)) SkinAnimatedNPC(*mNPCTemplate[4].hierarchy, mNPCTemplate[4].modelID);
            npc->SetAnimState(*foundAnim, 0.2f, (ePlayMode)0);
            npc->mWorldMatrix = stack->m_Stack[stack->m_NumStackEntries - 1]->value->m_worldMatrix;
            ListEntry<SkinAnimatedNPC*>* listEntry = (ListEntry<SkinAnimatedNPC*>*)nlMalloc(8, 8, false);
            if (listEntry != NULL)
            {
                listEntry->next = NULL;
                listEntry->entry = npc;
            }
            nlListAddStart<ListEntry<SkinAnimatedNPC*> >(&mNPCList.m_Head, listEntry, &mNPCList.m_Tail);
        }

        stack->Next();
    }

    delete stack;

    CreateNPCTemplate(5, true);
    PhysicsNPC* chainPhysics = new (nlMalloc(sizeof(PhysicsNPC), 8, false)) PhysicsNPC(g_pGame->m_pGameTweaks->fChainChompRadius);
    ChainChomp* chainChomp = new (nlMalloc(sizeof(ChainChomp), 8, false)) ChainChomp(*mNPCTemplate[5].hierarchy, mNPCTemplate[5].modelID, *chainPhysics, mpInventorySAnim);
    mpChainChomp = chainChomp;
    chainPhysics->SetCallbackFunction(&ChainChomp::CollisionCallback);

    if (nlSingleton<GameInfoManager>::Instance()->mIsInStrikers101Mode)
        CreateNPCTemplate(6, false);
    else
        CreateNPCTemplate(6, true);

    PhysicsNPC* bowserPhysics = new (nlMalloc(sizeof(PhysicsNPC), 8, false)) PhysicsNPC(g_pGame->m_pGameTweaks->fBowserRadius);
    Bowser* bowser = new (nlMalloc(sizeof(Bowser), 8, false)) Bowser(*mNPCTemplate[6].hierarchy, mNPCTemplate[6].modelID, *bowserPhysics, mpInventorySAnim);
    mpBowser = bowser;
    bowserPhysics->SetCallbackFunction(&Bowser::CollisionCallback);
}

static inline void DestroyNPCList(nlListContainer<SkinAnimatedNPC*>* npcList)
{
    ListEntry<SkinAnimatedNPC*>* npcEntry = npcList->m_Head;
    while (npcEntry != NULL)
    {
        delete npcEntry->entry;
        npcEntry = npcEntry->next;
    }

    npcList->Clear();
}

/**
 * Offset/Address/Size: 0x4D8 | 0x8016639C | size: 0x3D4
 */
NPCManager::~NPCManager()
{
    DestroyNPCList(&mNPCList);

    delete mpBowser;
    delete mpChainChomp;

    delete mpInventorySHierarchy;
    delete mpInventorySAnim;
}

/**
 * Offset/Address/Size: 0x47C | 0x80166340 | size: 0x5C
 */
void NPCManager::UpdateNPCs(float dt)
{
    ListEntry<SkinAnimatedNPC*>* current = mNPCList.m_Head;
    while (current != nullptr)
    {
        current->entry->Update(dt);
        current = current->next;
    }
}

/**
 * Offset/Address/Size: 0x430 | 0x801662F4 | size: 0x4C
 */
void NPCManager::RenderNPCs()
{
    ListEntry<SkinAnimatedNPC*>* current = mNPCList.m_Head;
    while (current != nullptr)
    {
        current->entry->Render();
        current = current->next;
    }
}

/**
 * Offset/Address/Size: 0x3D0 | 0x80166294 | size: 0x60
 */
void NPCManager::UpdateAINPCs(float dt)
{
    mpChainChomp->Update(dt);
    mpBowser->Update(dt);
}

/**
 * Offset/Address/Size: 0x0 | 0x80165EC4 | size: 0x3D0
 */
void NPCManager::CreateNPCTemplate(int templateIndex, bool loadTextures)
{
    int len;
    void* pMem;

    if (mNPCTemplate[templateIndex].loaded)
    {
        return;
    }

    glModel* model;
    if (loadTextures)
    {
        glLoadTextureBundle(gNPCTemplateInfo[templateIndex].textureFilename);
        model = glLoadModel(gNPCTemplateInfo[templateIndex].modelFilename, NULL);
    }
    else
    {
        model = NULL;
    }

    mpInventorySHierarchy->AddFile((char*)gNPCTemplateInfo[templateIndex].hierarchyFilename);

    if (gNPCTemplateInfo[templateIndex].loadAnimsVirtual)
    {
        pMem = nlLoadEntireFileToVirtualMemory(
            gNPCTemplateInfo[templateIndex].animFilename,
            &len,
            0x10000,
            NULL,
            AllocateStart);
        mpInventorySAnim->AddFile((char*)pMem, len);
    }
    else
    {
        mpInventorySAnim->AddFile((char*)gNPCTemplateInfo[templateIndex].animFilename);
    }

    mNPCTemplate[templateIndex].modelID = model == NULL ? -1 : ((glModelData*)model)->numModels;

    cInventory<cSHierarchy>* searchInv = mpInventorySHierarchy;
    u32 hash = nlStringHash(gNPCTemplateInfo[templateIndex].hierarchyName);
    mNPCTemplate[templateIndex].hierarchy = searchInv->Find((unsigned int)hash);
    mNPCTemplate[templateIndex].loaded = true;
}
