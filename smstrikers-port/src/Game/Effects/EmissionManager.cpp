#include "Game/Effects/EmissionManager.h"
#include "dolphin/types.h"
#include "NL/nlAVLTree.h"
#include "NL/gl/glFont.h"
#include "Game/NisPlayer.h"
#include "Game/Sys/debug.h"
#include "Game/Replay.h"

static class efList* controllers = nullptr;
static class efList* errors = nullptr;
static nlAVLTree<unsigned long, LingerMessage*, DefaultKeyCompare<unsigned long> >* lingerers = nullptr;
static EffectsLight g_EffectsLights[3];

static eGLView defaultView = GLV_Particles;

typedef AVLTreeEntry<unsigned long, LingerMessage*> LMEntry;

struct nlAVLTreeIter
{
    LMEntry** m_Stack;
    unsigned int m_NumStackEntries;
};

/**
 * Offset/Address/Size: 0xE38 | 0x801F9758 | size: 0x20
 */
EmissionManager& EmissionManager::InstanceForReplayOnly()
{
    static EmissionManager instance(true);
    return instance;
}

static int g_nNumLights = 0;
static unsigned short globalIdCounter;
static signed char globalIdCounterInit;

/**
 * Offset/Address/Size: 0xD6C | 0x801F968C | size: 0xCC
 */
bool EmissionManager::Startup(eGLView view)
{
    defaultView = view;

    efList* newControllers = (efList*)nlMalloc(sizeof(efList), 8, false);
    if (newControllers != nullptr)
    {
        newControllers->m_headNode = nullptr;
        newControllers->m_tailNode = nullptr;
        newControllers->m_numNodes = 0;
    }
    controllers = newControllers;

    efList* newErrors = (efList*)nlMalloc(sizeof(efList), 8, false);
    if (newErrors != nullptr)
    {
        newErrors->m_headNode = nullptr;
        newErrors->m_tailNode = nullptr;
        newErrors->m_numNodes = 0;
    }
    errors = newErrors;

    nlAVLTree<unsigned long, LingerMessage*, DefaultKeyCompare<unsigned long> >* newLingerers = (nlAVLTree<unsigned long, LingerMessage*, DefaultKeyCompare<unsigned long> >*)nlMalloc(
        sizeof(nlAVLTree<unsigned long, LingerMessage*, DefaultKeyCompare<unsigned long> >), 8, false);
    new (newLingerers) nlAVLTree<unsigned long, LingerMessage*, DefaultKeyCompare<unsigned long> >();
    lingerers = newLingerers;

    return true;
}

/**
 * Offset/Address/Size: 0xC1C | 0x801F953C | size: 0xF0
 */
bool EmissionManager::Shutdown()
{
    if (controllers->m_headNode != nullptr)
    {
        tDebugPrintManager::Print(DC_RENDER, "EmissionManager being deleted non-empty\n");
    }

    EmissionController* next;
    EmissionController* current;
    current = (EmissionController*)(controllers->m_headNode);

    while (current != nullptr)
    {
        next = (EmissionController*)(current->m_nextNode);
        controllers->Remove(current);
        delete current;
        current = next;
    }

    delete controllers;
    controllers = nullptr;

    LingerMessage* errorCurrent;
    errorCurrent = (LingerMessage*)(errors->m_headNode);

    while (errorCurrent != nullptr)
    {
        LingerMessage* errorNext = (LingerMessage*)(errorCurrent->m_nextNode);
        errors->Remove(errorCurrent);
        delete errorCurrent;
        errorCurrent = errorNext;
    }

    delete errors;
    errors = nullptr;

    ResetLingerers();

    delete lingerers;
    lingerers = nullptr;

    return true;
}

/**
 * Offset/Address/Size: 0x930 | 0x801F9250 | size: 0x2EC
 */
void EmissionManager::Update(float dt)
{
    if (NisPlayer::Instance()->WorldIsFrozen())
    {
        dt = 0.0f;
    }

    EmissionController* p = (EmissionController*)controllers->m_headNode;
    while (p != NULL)
    {
        bool stillAlive = p->Update(dt);
        if (stillAlive)
        {
            p = (EmissionController*)p->m_nextNode;
        }
        else
        {
            EmissionController* toBeDeleted = p;
            p = (EmissionController*)p->m_nextNode;
            controllers->Remove(toBeDeleted);
            delete toBeDeleted;
        }
    }

    // KillOldest inlined
    {
        int num = controllers->m_numNodes - 128;
        float currentBestAge = 0.0f;
        float prevBestAge = currentBestAge;

        while (num > 0)
        {
            EmissionController* toKill = NULL;
            float bestAge = 0.0f;
            EmissionController* current = (EmissionController*)controllers->m_headNode;

            while (current != NULL)
            {
                if ((unsigned int)(current->m_uUserData + 0x21530000) != 0x0000BEEF /* PORT: u32 wrap */)
                {
                    float age = current->m_Age;
                    if (bestAge < age && (prevBestAge == currentBestAge || age < currentBestAge))
                    {
                        bestAge = age;
                        toKill = current;
                        currentBestAge = age;
                    }
                }
                current = (EmissionController*)current->m_nextNode;
            }

            if (toKill == NULL)
            {
                break;
            }

            toKill->Die();
            num--;
        }
    }

    // Lingerers display
    if (lingerers->m_Root != NULL)
    {
        nlColour colour = { 0xFF, 0xFF, 0x40, 0xFF };
        nlAVLTreeIter* iter;
        int y = 3;
        glFontBegin(false);

        nlAVLTree<unsigned long, LingerMessage*, DefaultKeyCompare<unsigned long> >* tree = lingerers;
        iter = (nlAVLTreeIter*)nlMalloc(sizeof(nlAVLTreeIter), 8, false);
        if (iter != NULL)
        {
            unsigned int numEntries = tree->m_NumElements;
            LMEntry* node = tree->m_Root;
            iter->m_Stack = (LMEntry**)nlMalloc((numEntries + 1) * sizeof(void*), 8, false);
            iter->m_NumStackEntries = 0;

            if (node != NULL)
            {
                while (node->node.left != NULL)
                {
                    iter->m_Stack[iter->m_NumStackEntries] = node;
                    iter->m_NumStackEntries++;
                    node = (LMEntry*)node->node.left;
                }
                iter->m_Stack[iter->m_NumStackEntries] = node;
                iter->m_NumStackEntries++;
            }
        }

        while (iter->m_NumStackEntries != 0)
        {
            LMEntry* entry = iter->m_Stack[iter->m_NumStackEntries - 1];
            LingerMessage* l = entry->value;

            glFontPrintf((eGLView)0x21, 0, y, colour, "%s lingers (%d .. %d)", l->szMessage, l->nLingers, l->nParticles);

            iter->m_NumStackEntries--;

            entry = iter->m_Stack[iter->m_NumStackEntries];
            LMEntry* right = (LMEntry*)entry->node.right;
            if (right != NULL)
            {
                while (right->node.left != NULL)
                {
                    iter->m_Stack[iter->m_NumStackEntries] = right;
                    iter->m_NumStackEntries++;
                    right = (LMEntry*)right->node.left;
                }
                iter->m_Stack[iter->m_NumStackEntries] = right;
                iter->m_NumStackEntries++;
            }

            y++;
        }

        if (iter != NULL)
        {
            delete[] iter->m_Stack;
            delete iter;
        }

        glFontEnd();
        ResetLingerers();
    }
}

/**
 * Offset/Address/Size: 0x928 | 0x801F9248 | size: 0x8
 */
s32 EmissionManager::GetNumLights()
{
    return g_nNumLights;
}

/**
 * Offset/Address/Size: 0x8F8 | 0x801F9218 | size: 0x30
 */
EffectsLight* EmissionManager::GetLight(int index)
{
    if (index < 0 || index >= g_nNumLights)
    {
        return nullptr;
    }
    return &g_EffectsLights[index];
}

/**
 * Offset/Address/Size: 0x8A8 | 0x801F91C8 | size: 0x50
 */
void EmissionManager::AddEffectsLight(const EffectsLight& light)
{
    if (g_nNumLights >= 3)
    {
        return;
    }
    g_EffectsLights[g_nNumLights++] = light;
}

/**
 * Offset/Address/Size: 0x85C | 0x801F917C | size: 0x4C
 */
void EmissionManager::Render()
{
    g_nNumLights = 0;
    EmissionController* current = (EmissionController*)(controllers->m_headNode);

    while (current != nullptr)
    {
        current->Render();
        current = (EmissionController*)(current->m_nextNode);
    }
}

/**
 * Offset/Address/Size: 0x854 | 0x801F9174 | size: 0x8
 */
efList* EmissionManager::GetContainer()
{
    return controllers;
}

/**
 * Offset/Address/Size: 0x798 | 0x801F90B8 | size: 0xBC
 */
EmissionController* EmissionManager::Create(EffectsGroup* pEffectsGroup, unsigned short id)
{
    EmissionController* pController;

    if (!globalIdCounterInit)
    {
        globalIdCounter = 1;
        globalIdCounterInit = 1;
    }

    if (id == 0)
    {
        id = globalIdCounter++;
    }

    if (globalIdCounter > 0x7E16)
    {
        globalIdCounter = 0;
    }

    pController = new (nlMalloc(sizeof(EmissionController), 8, false)) EmissionController(pEffectsGroup, id, defaultView);
    controllers->Append(pController);
    return pController;
}

static unsigned long fx_sTerrain;

/**
 * Offset/Address/Size: 0x768 | 0x801F9088 | size: 0x30
 */
bool EmissionManager::IsStillAlive(EmissionController* controller)
{
    EmissionController* current;
    current = (EmissionController*)(controllers->m_headNode);

    while (current != nullptr)
    {
        if (current == controller)
        {
            return true;
        }
        current = (EmissionController*)(current->m_nextNode);
    }

    return false;
}

/**
 * Offset/Address/Size: 0x6EC | 0x801F900C | size: 0x7C
 */
void EmissionManager::Kill(uintptr_t userData, const EffectsGroup* pEffectsGroup)
{
    EmissionController* current;

    current = (EmissionController*)(controllers->m_headNode);

    while (current != nullptr)
    {
        if (pEffectsGroup == nullptr || current->m_pGroup == pEffectsGroup)
        {
            if (userData == current->m_uUserData)
            {
                current->Die();
            }
        }

        current = (EmissionController*)(current->m_nextNode);
    }
}

/**
 * Offset/Address/Size: 0x68C | 0x801F8FAC | size: 0x60
 */
bool EmissionManager::IsPlaying(uintptr_t userData, const EffectsGroup* pEffectsGroup)
{
    EmissionController* current;

    if (controllers == nullptr)
    {
        return false;
    }

    current = (EmissionController*)(controllers->m_headNode);

    while (current != nullptr)
    {
        if (pEffectsGroup == nullptr || current->m_pGroup == pEffectsGroup)
        {
            if (userData == 0 || userData == current->m_uUserData)
            {
                return true;
            }
        }

        current = (EmissionController*)(current->m_nextNode);
    }

    return false;
}

/**
 * Offset/Address/Size: 0x5EC | 0x801F8F0C | size: 0xA0
 */
void EmissionManager::DestroyAll(bool exceptPersistent)
{
    EmissionController* next;
    EmissionController* current;

    if (controllers == nullptr)
    {
        return;
    }

    current = (EmissionController*)(controllers->m_headNode);

    while (current != nullptr)
    {
        next = (EmissionController*)(current->m_nextNode);

        if ((defaultView == current->m_GlView) && ((!exceptPersistent) || ((unsigned int)(current->m_uUserData + 0x21530000) != 0x0000BEEF /* PORT: u32 wrap */)))
        {
            controllers->Remove(current);
            delete current;
        }

        current = next;
    }
}

/**
 * Offset/Address/Size: 0x54C | 0x801F8E6C | size: 0xA0
 */
void EmissionManager::Destroy(uintptr_t userData, const EffectsGroup* pEffectsGroup)
{
    EmissionController* next;
    EmissionController* current;

    if (controllers == nullptr)
    {
        return;
    }

    current = (EmissionController*)(controllers->m_headNode);

    while (current != nullptr)
    {
        next = (EmissionController*)(current->m_nextNode);

        if (((pEffectsGroup == nullptr || current->m_pGroup == pEffectsGroup)) && (userData == current->m_uUserData))
        {
            controllers->Remove(current);
            delete current;
        }

        current = next;
    }
}

/**
 * Offset/Address/Size: 0x520 | 0x801F8E40 | size: 0x2C
 */
void EmissionManager::ResetLingerers()
{
    if (lingerers != nullptr)
    {
        lingerers->DeleteValues();
    }
}

/**
 * Offset/Address/Size: 0x4D0 | 0x801F8DF0 | size: 0x50
 */
void EmissionManager::AddError(const char* format, ...)
{
}

static inline EmissionController* ReplayCreateController(EffectsGroup*& group, unsigned short id)
{
    if (!globalIdCounterInit)
    {
        globalIdCounter = 1;
        globalIdCounterInit = 1;
    }

    if (id == 0)
    {
        id = globalIdCounter++;
    }

    if (globalIdCounter > 0x7E16)
    {
        globalIdCounter = 0;
    }

    EmissionController* controller = new (nlMalloc(sizeof(EmissionController), 8, false)) EmissionController(group, id, defaultView);
    controllers->Append(controller);
    return controller;
}

static inline void ReplayRemoveAllNonPersistent()
{
    if (controllers != nullptr)
    {
        EmissionController* next;
        EmissionController* current = (EmissionController*)controllers->m_headNode;
        while (current != nullptr)
        {
            next = (EmissionController*)current->m_nextNode;
            eGLView glView = (eGLView)current->m_GlView;
            if ((defaultView == glView) && ((unsigned int)(current->m_uUserData + 0x21530000) != 0x0000BEEF /* PORT: u32 wrap */))
            {
                controllers->Remove(current);
                delete current;
            }
            current = next;
        }
    }
}

/**
 * Offset/Address/Size: 0x24C | 0x801F8B6C | size: 0x284
 */
void EmissionManager::Replay(LoadFrame& frame)
{
    int i;

    if (m_bRecording)
    {
        ReplayRemoveAllNonPersistent();
        m_bRecording = false;
    }

    int numEffects = 0;
    Replayable<0>(frame, numEffects);

    efList oldControllers;

    efBaseNode* head;
    efBaseNode* tail;
    int num;
    efList* const ctrl = controllers;

    head = ctrl->m_headNode;
    ctrl->m_headNode = oldControllers.m_headNode;
    oldControllers.m_headNode = head;

    tail = ctrl->m_tailNode;
    ctrl->m_tailNode = oldControllers.m_tailNode;
    oldControllers.m_tailNode = tail;

    num = ctrl->m_numNodes;
    ctrl->m_numNodes = oldControllers.m_numNodes;
    oldControllers.m_numNodes = num;

    i = 0;
    while (i < numEffects)
    {
        unsigned short id;
        Replayable<0>(frame, id);

        EffectsGroup* group = 0;
        /* PORT: uintptr_t, to match what the record side writes. */
        Replayable<0>(frame, (uintptr_t&)group);

        EmissionController* iter = (EmissionController*)oldControllers.m_headNode;
        EmissionController* next;
        unsigned short idCheck = id;
        while (iter != nullptr)
        {
            next = (EmissionController*)iter->m_nextNode;
            if (idCheck == iter->m_Id)
            {
                ::Replayable<0>(frame, *iter);
                oldControllers.Remove(iter);
                controllers->Insert(iter);
                break;
            }
            iter = next;
        }

        if (iter == nullptr)
        {
            iter = ReplayCreateController(group, id);
            ::Replayable<0>(frame, *iter);
        }

        i++;
    }

    EmissionController* next;
    EmissionController* iter = (EmissionController*)oldControllers.m_headNode;
    while (iter != nullptr)
    {
        next = (EmissionController*)iter->m_nextNode;
        if (defaultView != iter->m_GlView)
        {
            oldControllers.Remove(iter);
            controllers->Insert(iter);
        }
        else
        {
            oldControllers.Remove(iter);
            delete iter;
        }
        iter = next;
    }
}

/**
 * Offset/Address/Size: 0x11C | 0x801F8A3C | size: 0x130
 */
void EmissionManager::Replay(SaveFrame& frame)
{
    if (!m_bRecording)
    {
        if (controllers != nullptr)
        {
            EmissionController* current = (EmissionController*)(controllers->m_headNode);
            while (current != nullptr)
            {
                EmissionController* next = (EmissionController*)(current->m_nextNode);
                eGLView glView = (eGLView)current->m_GlView;
                if ((defaultView == glView) && ((unsigned int)(current->m_uUserData + 0x21530000) != 0x0000BEEF /* PORT: u32 wrap */))
                {
                    controllers->Remove(current);
                    delete current;
                }
                current = next;
            }
        }
        m_bRecording = true;
    }

    int numEffects = controllers->m_numNodes;
    Replayable<0>(frame, numEffects);

    EmissionController* current = (EmissionController*)(controllers->m_headNode);
    while (current != nullptr)
    {
        if (defaultView == current->m_GlView)
        {
            unsigned short id = current->m_Id;
            // PORT: pointer-width, to match the read.
        uintptr_t group = (uintptr_t)current->m_pGroup;
            Replayable<0>(frame, id);
            Replayable<0>(frame, group);
            Replayable<0>(frame, *current);
        }
        current = (EmissionController*)(current->m_nextNode);
    }
}

/**
 * Offset/Address/Size: 0x114 | 0x801F8A34 | size: 0x8
 */
u32 fxGetTerrain()
{
    return fx_sTerrain;
}

/**
 * Offset/Address/Size: 0x10C | 0x801F8A2C | size: 0x8
 */
void fxSetTerrain(unsigned long terrainID)
{
    fx_sTerrain = terrainID;
}

/**
 * Offset/Address/Size: 0x0 | 0x801F8920 | size: 0x10C
 */
void EmissionManager::KillOldest(int num, bool lingeringOnly)
{
    float prevBestAge = 0.0f;
    float currentBestAge = 0.0f;

    while (num > 0)
    {
        EmissionController* bestController = nullptr;
        float bestAge = 0.0f;
        EmissionController* current = (EmissionController*)(controllers->m_headNode);

        while (current != nullptr)
        {
            if ((!lingeringOnly || current->IsLingering()) && ((unsigned int)(current->m_uUserData + 0x21530000) != 0x0000BEEF /* PORT: u32 wrap */))
            {
                float age = current->m_Age;
                if ((bestAge < age) && (prevBestAge == currentBestAge || age < currentBestAge))
                {
                    bestAge = age;
                    bestController = current;
                    currentBestAge = age;
                }
            }

            current = (EmissionController*)(current->m_nextNode);
        }

        if (bestController == nullptr)
        {
            break;
        }

        bestController->Die();
        num--;
    }
}
