#include "Game/FE/feSceneManager.h"
#include "Game/FE/feInput.h"
#include "Game/FE/feRender.h"
#include "NL/nlDLRing.h"
#include "NL/nlDLListContainer.h"
#include "NL/nlDebug.h"
#include "NL/nlString.h"

extern FEInput* g_pFEInput;

template <>
FESceneManager* nlSingleton<FESceneManager>::s_pInstance = 0;

SlotPool<PackagePushPopMessage> PackagePushPopMessage::m_PushPopMessageSlotPool(0x14, 0);
nlDLListSlotPool<PackagePushPopMessage*> m_pushPopMessageQueue(0x14, 0);

/**
 * Offset/Address/Size: 0x0 | 0x8020D64C | size: 0xC0
 */
void FESceneManager::Update(float dt)
{
    DLListEntry<BaseSceneHandler*>* headEntry;
    DLListEntry<BaseSceneHandler*>* currentEntry;

    ProcessPushPopQueue();

    if (m_sceneHandlerStack.m_Head == nullptr)
    {
        return;
    }

    currentEntry = nlDLRingGetStart(m_sceneHandlerStack.m_Head);
    headEntry = m_sceneHandlerStack.m_Head;

    while (currentEntry != nullptr)
    {
        if (currentEntry->entry->m_pFEScene->m_bValid != false)
        {
            g_pFEInput->EnableInputIfSceneHasFocus(currentEntry->entry);
            currentEntry->entry->Update(dt);
        }

        if (nlDLRingIsEnd(headEntry, currentEntry) || currentEntry == nullptr)
        {
            currentEntry = nullptr;
        }
        else
        {
            currentEntry = currentEntry->m_next;
        }
    }
}

bool FESceneManager::IsObjectQueuedForPop(BaseSceneHandler* pSceneHandler)
{
    DLListEntry<PackagePushPopMessage*>* msgEntry = nlDLRingGetStart(m_pushPopMessageQueue.m_Head);
    DLListEntry<PackagePushPopMessage*>* msgHead = m_pushPopMessageQueue.m_Head;

    while (msgEntry != NULL)
    {
        PackagePushPopMessage* pMsg = msgEntry->entry;
        if (pMsg->m_pSceneHandler == pSceneHandler && pMsg->m_bPush == false)
        {
            return true;
        }

        if (nlDLRingIsEnd(msgHead, msgEntry) || msgEntry == NULL)
        {
            msgEntry = NULL;
        }
        else
        {
            msgEntry = msgEntry->m_next;
        }
    }

    return false;
}

static inline void FindSceneForPop(
    PackagePushPopMessage* msg, DLListEntry<BaseSceneHandler*>* headEntry, DLListEntry<BaseSceneHandler*>* sceneEntry)
{
    while (sceneEntry != NULL)
    {
        BaseSceneHandler* pSceneHandler = sceneEntry->entry;

        if (!FESceneManager::IsObjectQueuedForPop(pSceneHandler))
        {
            msg->m_pSceneHandler = sceneEntry->entry;
            break;
        }

        if (nlDLRingIsEnd(headEntry, sceneEntry) || sceneEntry == NULL)
        {
            sceneEntry = NULL;
        }
        else
        {
            sceneEntry = sceneEntry->m_next;
        }
    }
}

static inline void FindSceneAndQueuePop(
    PackagePushPopMessage* msg,
    DLListEntry<BaseSceneHandler*>* headEntry,
    DLListEntry<BaseSceneHandler*>* sceneEntry,
    DLListEntry<PackagePushPopMessage*>** queueHead)
{
    DLListEntry<PackagePushPopMessage*>* entry;

    FindSceneForPop(msg, headEntry, sceneEntry);

    entry = NULL;

    m_pushPopMessageQueue.m_Allocator.Allocate(entry);

    if (entry != NULL)
    {
        entry->m_next = NULL;
        entry->m_prev = NULL;
        entry->entry = msg;
    }

    nlDLRingAddEnd(queueHead, entry);
}

static inline void RenderSceneStack(FESceneManager* pSceneManager)
{
    DLListEntry<BaseSceneHandler*>* sceneEntry = nlDLRingGetStart(pSceneManager->m_sceneHandlerStack.m_Head);
    DLListEntry<BaseSceneHandler*>* sceneHead = pSceneManager->m_sceneHandlerStack.m_Head;

    while (sceneEntry != NULL)
    {
        BaseSceneHandler* pSceneHandler = sceneEntry->entry;

        if (pSceneHandler != pSceneManager->m_topMostScene)
        {
            if (!FESceneManager::IsObjectQueuedForPop(sceneEntry->entry))
            {
                if (pSceneHandler->m_pFEScene->m_bValid && pSceneHandler->m_bVisible)
                {
                    FERender::RenderScene(pSceneHandler->m_pFEScene);
                }
            }
        }

        if (nlDLRingIsEnd(sceneHead, sceneEntry) || sceneEntry == NULL)
        {
            sceneEntry = NULL;
        }
        else
        {
            sceneEntry = sceneEntry->m_next;
        }
    }
}

/**
 * Offset/Address/Size: 0xC0 | 0x8020D70C | size: 0x1C4
 */
void FESceneManager::RenderActiveScenes()
{
    if (m_topMostScene != NULL)
    {
        if (!IsObjectQueuedForPop(m_topMostScene))
        {
            if (m_topMostScene->m_pFEScene->m_bValid && m_topMostScene->m_bVisible)
            {
                FERender::RenderScene(m_topMostScene->m_pFEScene);
            }
        }
    }

    RenderSceneStack(this);
}

void FESceneManager::QueueAllScenesPop()
{
    DLListEntry<BaseSceneHandler*>* sceneEntry = nlDLRingGetStart(m_sceneHandlerStack.m_Head);
    DLListEntry<BaseSceneHandler*>* sceneHead = m_sceneHandlerStack.m_Head;

    while (sceneEntry != NULL)
    {
        BaseSceneHandler* pSceneHandler = sceneEntry->entry;
        if (!IsObjectQueuedForPop(pSceneHandler))
        {
            PackagePushPopMessage* message = PackagePushPopMessage::m_PushPopMessageSlotPool.Allocate();
            message->m_szFilename[0] = 0;
            message->m_pSceneHandler = pSceneHandler;
            message->m_bPush = false;
            m_pushPopMessageQueue.AddEnd(message);
        }

        if (nlDLRingIsEnd(sceneHead, sceneEntry) || sceneEntry == NULL)
        {
            sceneEntry = NULL;
        }
        else
        {
            sceneEntry = sceneEntry->m_next;
        }
    }

    m_sceneHandlerStack.Clear();
}

/**
 * Offset/Address/Size: 0x284 | 0x8020D8D0 | size: 0x1A8
 */
void FESceneManager::QueueScenePop()
{
    PackagePushPopMessage* msg;
    DLListEntry<PackagePushPopMessage*>** queueHead;

    msg = NULL;

    PackagePushPopMessage::m_PushPopMessageSlotPool.Allocate(msg);

    msg->m_szFilename[0] = 0;
    msg->m_pSceneHandler = NULL;
    msg->m_bPush = false;

    DLListEntry<BaseSceneHandler*>* sceneEntry = nlDLRingGetStart(m_sceneHandlerStack.m_Head);
    queueHead = &m_pushPopMessageQueue.m_Head;

    FindSceneAndQueuePop(msg, m_sceneHandlerStack.m_Head, sceneEntry, queueHead);
}

/**
 * Offset/Address/Size: 0x42C | 0x8020DA78 | size: 0x114
 */
void FESceneManager::QueueScenePush(BaseSceneHandler* pSceneHandler, const char* szFilename)
{
    PackagePushPopMessage* msg = nullptr;

    PackagePushPopMessage::m_PushPopMessageSlotPool.Allocate(msg);

    msg->m_bPush = true;
    msg->m_pSceneHandler = pSceneHandler;
    nlStrNCpy<char>(msg->m_szFilename, szFilename, 0x40);
    msg->m_pSceneHandler->m_uHashID = nlStringLowerHash(szFilename);

    DLListEntry<PackagePushPopMessage*>* entry = nullptr;

    m_pushPopMessageQueue.m_Allocator.Allocate(entry);

    if (entry != NULL)
    {
        entry->m_next = NULL;
        entry->m_prev = NULL;
        entry->entry = msg;
    }

    nlDLRingAddEnd(&m_pushPopMessageQueue.m_Head, entry);
}

void FESceneManager::LoadScene(
    const char* szFilename,
    BaseSceneHandler* pHandler)
{
    FESceneManager* pSceneManager = FESceneManager::Instance();
    FEScene* pFEScene = new (nlMalloc(sizeof(FEScene), 8, false)) FEScene();
    pFEScene->m_uHashID = nlStringLowerHash(szFilename);

    if (!pFEScene->LoadPackage(szFilename))
    {
        nlPrintf("Error: failed to load package!\n");
        nlBreak();
    }
    else
    {
        pFEScene->m_uRenderView = pSceneManager->m_uDefaultRenderView;
        pHandler->m_pFEScene = pFEScene;
        pHandler->SetPresentation(pFEScene->m_pFEPackage->GetPresentation());
        pHandler->SceneCreated();
        pHandler->InitializeSubHandlers();
    }
}

/**
 * Offset/Address/Size: 0x540 | 0x8020DB8C | size: 0x270
 */
void FESceneManager::ProcessPushPopQueue()
{
    FESceneManager* pSceneManager = this;
    PackagePushPopMessage* pPackagePushPopMessage;

    while (m_pushPopMessageQueue.m_Head != NULL)
    {
        m_pushPopMessageQueue.RemoveStart(&pPackagePushPopMessage);

        if (pPackagePushPopMessage->m_bPush != false)
        {
            pSceneManager->m_sceneHandlerStack.AddStart(pPackagePushPopMessage->m_pSceneHandler);

            LoadScene(
                pPackagePushPopMessage->m_szFilename,
                pPackagePushPopMessage->m_pSceneHandler);
        }
        else
        {
            DLListEntry<BaseSceneHandler*>* headEntry;
            DLListEntry<BaseSceneHandler*>* sceneEntry;

            sceneEntry = nlDLRingGetStart(pSceneManager->m_sceneHandlerStack.m_Head);
            headEntry = pSceneManager->m_sceneHandlerStack.m_Head;

            while (sceneEntry != NULL)
            {
                if (sceneEntry->entry == pPackagePushPopMessage->m_pSceneHandler)
                {
                    nlDLRingIsEnd(headEntry, sceneEntry);
                    nlDLRingRemove(&pSceneManager->m_sceneHandlerStack.m_Head, sceneEntry);
                    pSceneManager->m_sceneHandlerStack.m_Allocator.Free(sceneEntry);
                    break;
                }

                if (nlDLRingIsEnd(headEntry, sceneEntry) || sceneEntry == NULL)
                {
                    sceneEntry = NULL;
                }
                else
                {
                    sceneEntry = sceneEntry->m_next;
                }
            }

            pPackagePushPopMessage->m_pSceneHandler->m_pFEScene->UnloadPackage();

            FEScene* pFEScene = pPackagePushPopMessage->m_pSceneHandler->m_pFEScene;

            delete pPackagePushPopMessage->m_pSceneHandler;
            delete pFEScene;
        }

        PackagePushPopMessage::m_PushPopMessageSlotPool.Delete(pPackagePushPopMessage);
    }
}

/**
 * Offset/Address/Size: 0x7B0 | 0x8020DDFC | size: 0x98
 */
BaseSceneHandler* FESceneManager::GetSceneHandler(unsigned long hashID)
{
    DLListEntry<BaseSceneHandler*>* headEntry;
    DLListEntry<BaseSceneHandler*>* currentEntry;

    currentEntry = nlDLRingGetStart(m_sceneHandlerStack.m_Head);
    headEntry = m_sceneHandlerStack.m_Head;

    while (currentEntry != nullptr)
    {
        if (hashID == currentEntry->entry->m_uHashID)
        {
            return currentEntry->entry;
        }

        if (nlDLRingIsEnd(headEntry, currentEntry) || currentEntry == nullptr)
        {
            currentEntry = nullptr;
        }
        else
        {
            currentEntry = currentEntry->m_next;
        }
    }

    return nullptr;
}

/**
 * Offset/Address/Size: 0x848 | 0x8020DE94 | size: 0x20
 */
void FESceneManager::ForceImmediateStackProcessing()
{
    FORCE_DONT_INLINE;
    ProcessPushPopQueue();
}

/**
 * Offset/Address/Size: 0x868 | 0x8020DEB4 | size: 0xA4
 */
bool FESceneManager::AreAllScenesValid()
{
    DLListEntry<BaseSceneHandler*>* headEntry;
    DLListEntry<BaseSceneHandler*>* currentEntry;

    currentEntry = nlDLRingGetStart(m_sceneHandlerStack.m_Head);
    headEntry = m_sceneHandlerStack.m_Head;

    while (currentEntry != nullptr)
    {
        BaseSceneHandler* sceneHandler = currentEntry->entry;
        if (sceneHandler->m_pFEScene->m_bValid == false)
        {
            return false;
        }

        if (nlDLRingIsEnd(headEntry, currentEntry) || currentEntry == nullptr)
        {
            currentEntry = nullptr;
        }
        else
        {
            currentEntry = currentEntry->m_next;
        }
    }

    return (m_pushPopMessageQueue.m_Head == nullptr);
}

/**
 * Offset/Address/Size: 0x90C | 0x8020DF58 | size: 0xB8
 */
FESceneManager::~FESceneManager()
{
    ForceImmediateStackProcessing();
    FERender::Cleanup();
}

/**
 * Offset/Address/Size: 0x9C4 | 0x8020E010 | size: 0x70
 * 91.25% match - residual r30/r31 swap: target colors the outer `this` to r31
 * and the m_Allocator subobject `this` to r30; MWCC GC/2.5 does the reverse.
 * Same single-pool inlined-ctor coloring wall as FontManager ctor (also 91.25%);
 * appears irreducible with local source (likely an original-compiler-version
 * coloring artifact). FETweenManager (two pools) matches 100% via a different
 * allocation, so it is not a template-shape problem.
 */
FESceneManager::FESceneManager()
    : m_sceneHandlerStack(0x14, 0)
{
    m_uDefaultRenderView = -1;
    m_topMostScene = nullptr;
    FERender::Initialize();
}
