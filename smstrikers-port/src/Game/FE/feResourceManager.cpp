#include "Game/FE/feFontResource.h"
#include "Game/FE/feResourceManager.h"
#include "Game/FE/feScene.h"
#include "Game/FE/feSceneResource.h"
#include "Game/FE/feTextureResource.h"
#include "Game/Font/fontmanager.h"
#include "NL/nlMemory.h"
#include "NL/nlString.h"
#include "NL/gl/glTexture.h"
#include "NL/nlAVLTree.h"
#include "NL/nlDLListContainer.h"
#include "NL/glx/glxMemory.h"

static nlAVLTreeSlotPool<unsigned long, FEResourceHandle*, DefaultKeyCompare<unsigned long> > s_loadedResourceList(0x100, 0);
static unsigned char* s_pResourceLoadBuffer;
nlDLListSlotPool<FEResourceHandle*> pendingResourceQueue(0x80, 0);
static FEResourceHandle* s_pCurrentResourceBeingLoaded;
FESceneResource* s_pCurrentFESceneResourceContext;
static BundleFile* s_pPermanentBundle;
static BundleFile* s_pOnDemandBundle;
static FESceneResource* s_pPermanentBundleSceneResource;

template <>
FEResourceManager* nlSingleton<FEResourceManager>::s_pInstance = 0;

void FEResourceManager::AddResourceToResourceList(FEResourceHandle* pFEResourceHandle)
{
    s_loadedResourceList.Add(pFEResourceHandle->GetHashID(), pFEResourceHandle);
}

void FEResourceManager::RemoveResourceFromResourceList(FEResourceHandle* pFEResourceHandle)
{
    FEResourceHandle** pLoadedResourceHandle;
    if (!s_loadedResourceList.FindGet(pFEResourceHandle->GetHashID(), &pLoadedResourceHandle))
        return;
    if (*pLoadedResourceHandle != pFEResourceHandle)
        return;
    s_loadedResourceList.Remove(pFEResourceHandle->GetHashID());
}

FEResourceHandle* FEResourceManager::FindExistingResourceInResourceList(FEResourceHandle* pFEResourceHandle)
{
    FEResourceHandle** pPreExistingResourceHandle;
    if (s_loadedResourceList.FindGet(pFEResourceHandle->GetHashID(), &pPreExistingResourceHandle))
    {
        return *pPreExistingResourceHandle;
    }

    return NULL;
}

/**
 * Offset/Address/Size: 0xD48 | 0x8020C888 | size: 0x1C
 */
FEResourceManager::FEResourceManager()
{
}

/**
 * Offset/Address/Size: 0xCEC | 0x8020C82C | size: 0x5C
 */
FEResourceManager::~FEResourceManager()
{
    Cleanup();
}

/**
 * Offset/Address/Size: 0xAF4 | 0x8020C634 | size: 0x1F8
 */
void FEResourceManager::Cleanup()
{
    if (s_pOnDemandBundle != NULL)
    {
        s_pOnDemandBundle->Close();
        delete s_pOnDemandBundle;
        s_pOnDemandBundle = NULL;
    }

    if (s_loadedResourceList.m_NumElements != 0)
    {
        nlPrintf("FEResourceManager: Warning! Manager being destroyed while resources are still loaded!\n");
        nlPrintf("                   Did all the scenes get popped before destroying the FEResourceManager?\n");

        typedef nlAVLTreeIterator<unsigned long, FEResourceHandle*, DefaultKeyCompare<unsigned long> > ResourceIterator;

        ResourceIterator* iterator = new (nlMalloc(sizeof(ResourceIterator), 8, false))
            ResourceIterator(s_loadedResourceList);

        while (iterator->IsValid())
        {
            FEResourceHandle* pFeResourceHandle = iterator->Current()->value;
            nlPrintf(
                "                   Outstanding resource 0x%08x ( type = 0x%08x ) for load\n",
                pFeResourceHandle->m_hashID,
                pFeResourceHandle->m_type);
            iterator->Next();
        }

        delete iterator;
    }

    s_loadedResourceList.Clear();
}

/**
 * Offset/Address/Size: 0x848 | 0x8020C388 | size: 0x2AC
 */
void FEResourceManager::LoadPermanentResourceBundle(const char* szBundleFileName)
{
    nlStrNCpy(m_szPermanentBundleFileName, szBundleFileName, 0x20);

    s_pPermanentBundleSceneResource = new (nlMalloc(sizeof(FESceneResource), 8, false)) FESceneResource();
    s_pPermanentBundleSceneResource->m_pFESceneContext = NULL;
    s_pPermanentBundleSceneResource->m_hashID = nlStringLowerHash("PermanentContext");
    s_pPermanentBundleSceneResource->m_next = NULL;
    s_pPermanentBundleSceneResource->m_prev = NULL;
    s_pPermanentBundleSceneResource->m_type = FERT_SCENE;

    IssueSceneContextSwitch(s_pPermanentBundleSceneResource);

    s_pPermanentBundle = new (nlMalloc(sizeof(BundleFile), 8, false)) BundleFile();
    s_pPermanentBundle->Open(m_szPermanentBundleFileName);

    LoadPermanentTextures();

    s_pPermanentBundle->Close();
    delete s_pPermanentBundle;
    s_pPermanentBundle = NULL;
}

void FEResourceManager::LoadPermanentTextures()
{
    unsigned long uFileLength;
    unsigned long uFileHashID;
    BundleFileDirectoryEntry fileDirectoryEntry;
    unsigned long i;
    unsigned char b;
    unsigned long fileCount = s_pPermanentBundle->GetNumFiles();

    for (i = 0; i < fileCount; i++)
    {
        b = s_pPermanentBundle->GetFileInfoByIndex(i, &fileDirectoryEntry);
        if (!b)
        {
            nlPrintf("FEResourceManager Error: Failed to get file information in permanent bundle!\n");
        }
        else
        {
            uFileLength = fileDirectoryEntry.m_length;
            uFileHashID = fileDirectoryEntry.m_hash;

            FETextureResource* pTextureResource = CreateTextureResourceFromHandle(uFileHashID);

            s_pResourceLoadBuffer = new (0x20, true) unsigned char[uFileLength];
            s_pPermanentBundle->ReadFileByIndex(i, s_pResourceLoadBuffer, uFileLength);
            TextureResourceLoadComplete(NULL, uFileLength, (uintptr_t)pTextureResource);

            delete[] s_pResourceLoadBuffer;
            s_pResourceLoadBuffer = NULL;

            AddResourceToResourceList(pTextureResource);
            PlaceHolderForceTextureValid(pTextureResource);
        }
    }
}

/**
 * Offset/Address/Size: 0x7D0 | 0x8020C310 | size: 0x78
 */
bool FEResourceManager::OpenOnDemandResourceBundle(const char* szBundleFileName)
{
    nlStrNCpy(m_szOnDemandBundleFileName, szBundleFileName, 0x20);

    BundleFile* pBundle = new (nlMalloc(sizeof(BundleFile), 8, false)) BundleFile();
    s_pOnDemandBundle = pBundle;

    if (!pBundle->Open(szBundleFileName))
    {
        return false;
    }
    return true;
}

/**
 * Offset/Address/Size: 0x7CC | 0x8020C30C | size: 0x4
 */
void FEResourceManager::Initialize()
{
}

FETextureResource* FEResourceManager::CreateTextureResourceFromHandle(unsigned long handle)
{
    FETextureResource* pTextureResource = new (8, false) FETextureResource();
    pTextureResource->m_hashID = handle;
    return pTextureResource;
}

/**
 * Offset/Address/Size: 0x72C | 0x8020C26C | size: 0xA0
 */
void FEResourceManager::QueueResourceLoad(FEResourceHandle* pHandle)
{
    pendingResourceQueue.AddEnd(pHandle);
}

/**
 * Offset/Address/Size: 0x5F4 | 0x8020C134 | size: 0x138
 */
void FEResourceManager::UnloadResource(FEResourceHandle* pFeResourceHandle)
{
    switch (pFeResourceHandle->m_type)
    {
    case FERT_TEXTURE:
        RemoveResourceFromResourceList(pFeResourceHandle);
        break;
    case FERT_SCENE:
        glplatResourceRelease(((FESceneResource*)pFeResourceHandle)->m_glResourceMarker);
        break;
    }
}

/**
 * Offset/Address/Size: 0x344 | 0x8020BE84 | size: 0x2B0
 */
void FEResourceManager::UnloadPermanentResourceBundle()
{
    s_pPermanentBundle = new (nlMalloc(sizeof(BundleFile), 8, false)) BundleFile();
    s_pPermanentBundle->Open(m_szPermanentBundleFileName);

    unsigned long fileCount = s_pPermanentBundle->GetNumFiles();
    BundleFileDirectoryEntry fileDirectoryEntry;
    unsigned long fileIndex;
    FEResourceHandle** pLoadedResourceHandle;
    u32 hashtodelete;

    UnloadResource(s_pPermanentBundleSceneResource);

    for (fileIndex = 0; fileIndex < fileCount; fileIndex++)
    {
        s_pPermanentBundle->GetFileInfoByIndex(fileIndex, &fileDirectoryEntry);

        bool found = s_loadedResourceList.FindGet(
            fileDirectoryEntry.m_hash, &pLoadedResourceHandle);
        if (found)
        {
            hashtodelete = (*pLoadedResourceHandle)->GetHashID();
            ::operator delete(*pLoadedResourceHandle);
            s_loadedResourceList.Remove(hashtodelete);
        }
    }

    s_pPermanentBundle->Close();
    delete s_pPermanentBundle;
    s_pPermanentBundle = NULL;

    ::operator delete(s_pPermanentBundleSceneResource);
    s_pPermanentBundleSceneResource = NULL;
}

ResourceResult FEResourceManager::IssueFontLoadRequest(FEFontResource* pFeFontResource)
{
    FEResourceHandle* pFeResourceHandle = (FEResourceHandle*)pFeFontResource;
    nlFont* pExistingFont = FontManager::Instance()->GetFontByHashID(pFeResourceHandle->m_hashID);
    pFeFontResource->SetFontReference(pExistingFont);
    pFeResourceHandle->m_bValid = true;
    return FERR_AlreadyLoaded;
}

/**
 * Offset/Address/Size: 0x29C | 0x8020BDDC | size: 0xA8
 */
void FEResourceManager::TextureResourceLoadComplete(void* buffer, unsigned long uReadSize, uintptr_t uParam)
{
    FEResourceHandle* pHandle = (FEResourceHandle*)uParam;

    glTextureAdd(pHandle->m_hashID, s_pResourceLoadBuffer, uReadSize);

    delete[] s_pResourceLoadBuffer;
    s_pResourceLoadBuffer = NULL;

    ((FETextureResource*)pHandle)->m_glTextureHandle = pHandle->m_hashID;

    AddResourceToResourceList(pHandle);
    PlaceHolderForceTextureValid((FETextureResource*)pHandle);
}

ResourceResult FEResourceManager::IssueTextureLoadRequest(FETextureResource* pFeTextureResource)
{
    BundleFileDirectoryEntry fileDirectoryEntry;
    FETextureResource* pFeExistingTextureResource = (FETextureResource*)FindExistingResourceInResourceList((FEResourceHandle*)pFeTextureResource);

    if ((pFeExistingTextureResource != NULL)
        && (pFeExistingTextureResource->GetResourceType() == pFeTextureResource->GetResourceType()))
    {
        pFeTextureResource->m_glTextureHandle = pFeExistingTextureResource->m_glTextureHandle;
        pFeTextureResource->m_bValid = pFeExistingTextureResource->m_bValid;
        return FERR_AlreadyLoaded;
    }

    if (s_pOnDemandBundle->GetFileInfo(pFeTextureResource->m_hashID, &fileDirectoryEntry, true))
    {
        unsigned char* pResourceLoadBuffer = (unsigned char*)nlMalloc(fileDirectoryEntry.m_length, 0x20, true);
        s_pResourceLoadBuffer = pResourceLoadBuffer;
        s_pOnDemandBundle->ReadFileAsync(
            pFeTextureResource->m_hashID,
            pResourceLoadBuffer,
            fileDirectoryEntry.m_length,
            FEResourceManager::TextureResourceLoadComplete,
            (uintptr_t)pFeTextureResource);
    }

    return FERR_WaitingForResource;
}

ResourceResult FEResourceManager::IssueSceneContextSwitch(FESceneResource* pFeSceneResource)
{
    if ((s_pCurrentFESceneResourceContext != NULL)
        && (s_pCurrentFESceneResourceContext != pFeSceneResource)
        && (s_pPermanentBundleSceneResource != s_pCurrentFESceneResourceContext))
    {
        s_pCurrentFESceneResourceContext->m_pFESceneContext->m_bValid = true;
        s_pCurrentFESceneResourceContext->m_pFESceneContext->AllResourcesLoadedCallback();
    }

    pFeSceneResource->m_glResourceMarker = glplatResourceMark();
    pFeSceneResource->m_bValid = true;
    s_pCurrentFESceneResourceContext = pFeSceneResource;

    return FERR_WaitingForResource;
}

ResourceResult FEResourceManager::IssueResourceLoadRequest(FEResourceHandle* pFeResourceHandle)
{
    ResourceResult resourceRequestResult = FERR_WaitingForResource;

    switch (pFeResourceHandle->m_type)
    {
    case FERT_TEXTURE:
        resourceRequestResult = IssueTextureLoadRequest((FETextureResource*)s_pCurrentResourceBeingLoaded);
        break;

    case FERT_SCENE:
        resourceRequestResult = IssueSceneContextSwitch((FESceneResource*)pFeResourceHandle);
        break;

    case FERT_FONT:
        resourceRequestResult = IssueFontLoadRequest((FEFontResource*)pFeResourceHandle);
        break;

    default:
        break;
    }

    return resourceRequestResult;
}

/**
 * Offset/Address/Size: 0x0 | 0x8020BB40 | size: 0x29C
 */
void FEResourceManager::Update(float dt)
{
    ResourceResult result;
    bool bQueueNextResource = true;

    while (bQueueNextResource)
    {
        if ((s_pCurrentResourceBeingLoaded != NULL) && !s_pCurrentResourceBeingLoaded->IsValid())
        {
            return;
        }

        if (s_pCurrentResourceBeingLoaded != NULL)
        {
            pendingResourceQueue.RemoveStart(NULL);
            s_pCurrentResourceBeingLoaded = NULL;

            if (pendingResourceQueue.IsEmpty())
            {
                s_pCurrentFESceneResourceContext->m_pFESceneContext->m_bValid = true;
                s_pCurrentFESceneResourceContext = NULL;
            }
        }

        if (pendingResourceQueue.IsEmpty())
        {
            return;
        }

        s_pCurrentResourceBeingLoaded = pendingResourceQueue.Begin().m_Curr->entry;
        result = IssueResourceLoadRequest(s_pCurrentResourceBeingLoaded);
        bQueueNextResource = (result == FERR_AlreadyLoaded);
    }
}

void FEResourceManager::PlaceHolderForceTextureValid(FETextureResource* pFETextureResource)
{
    pFETextureResource->m_bValid = true;
}
