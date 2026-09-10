#ifndef _FERESOURCEMANAGER_H_
#define _FERESOURCEMANAGER_H_

#include "types.h"
#include "NL/nlTask.h"
#include "NL/nlSingleton.h"
#include "NL/nlBundleFile.h"

enum eFEResourceType
{
    FERT_UNKNOWN = -1,
    FERT_TEXTURE = 0,
    FERT_FONT = 1,
    FERT_SCENE = 2,
    FERT_RESOURCE_TYPE_COUNT = 3,
};

class FEResourceHandle
{
public:
    FEResourceHandle() { };
    ~FEResourceHandle() { };

    /* 0x00 */ FEResourceHandle* m_next;
    /* 0x04 */ FEResourceHandle* m_prev;

    /* 0x08 */ eFEResourceType m_type;
    /* 0x0C */ unsigned long m_hashID;
    bool IsValid() const
    {
        return m_bValid;
    }

    eFEResourceType GetResourceType() const
    {
        return m_type;
    }

    unsigned long GetHashID() const
    {
        return m_hashID;
    }

    /* 0x10 */ bool m_bValid;
}; // total size: 0x14

enum ResourceResult
{
    FERR_WaitingForResource = 0,
    FERR_AlreadyLoaded = 1,
};

class FESceneResource;
class FETextureResource;
class FEFontResource;
template <typename T>
class DLListEntry;

class FEResourceManager : public nlTask, public nlSingleton<FEResourceManager>
{
public:
    FEResourceManager();
    ~FEResourceManager();

    void Run(float dt)
    {
        Update(dt);
    }

    virtual const char* GetName()
    {
        return "FEResource Manager";
    }

    void Cleanup();
    void LoadPermanentResourceBundle(const char* szBundleFileName);
    static void LoadPermanentTextures();
    bool OpenOnDemandResourceBundle(const char* szBundleFileName);
    void Initialize();
    void Update(float dt);
    void QueueResourceLoad(FEResourceHandle* pHandle);
    void UnloadResource(FEResourceHandle* pFeResourceHandle);
    void UnloadPermanentResourceBundle();
    static void TextureResourceLoadComplete(void* buffer, unsigned long uReadSize, uintptr_t uParam);

private:
    static void PlaceHolderForceTextureValid(FETextureResource* pFETextureResource);
    static ResourceResult IssueResourceLoadRequest(FEResourceHandle* pFeResourceHandle);
    static ResourceResult IssueSceneContextSwitch(FESceneResource* pFeSceneResource);
    static ResourceResult IssueTextureLoadRequest(FETextureResource* pFeTextureResource);
    static ResourceResult IssueFontLoadRequest(FEFontResource* pFeFontResource);
    static FETextureResource* CreateTextureResourceFromHandle(unsigned long handle);
    static FEResourceHandle* FindExistingResourceInResourceList(FEResourceHandle* pFEResourceHandle);
    static void RemoveResourceFromResourceList(FEResourceHandle* pFEResourceHandle);
    static void AddResourceToResourceList(FEResourceHandle* pFEResourceHandle);

protected:
    /* 0x18 */ char m_szPermanentBundleFileName[32];
    /* 0x38 */ char m_szOnDemandBundleFileName[32];
}; // total size: 0x58

#endif // _FERESOURCEMANAGER_H_
