#ifndef _FESCENE_H_
#define _FESCENE_H_

#include "types.h"

#include "NL/gl/gl.h"
#include "Game/FE/fePackage.h"
#include "Game/FE/feResourceManager.h"
#include "Game/FE/feSceneResource.h"

class FEScene
{
public:
    FEScene();
    ~FEScene();

    void AllResourcesLoadedCallback();
    bool LoadPackage(const char* szPackageFileName);
    void UnloadPackage();
    void Update(float dt);
    FEPackage* GetPackage() const
    {
        return m_pFEPackage;
    }
    eGLView GetRenderView() const
    {
        return (eGLView)m_uRenderView;
    }
    const nlMatrix4& GetCameraMatrix() const
    {
        return m_matView;
    }

    /* 0x00 */ FEPackage* m_pFEPackage;
    /* 0x04 */ unsigned long m_uHashID;
    /* 0x08 */ bool m_bValid;
    /* 0x0C */ nlMatrix4 m_matView;
    /* 0x4C */ unsigned long m_uRenderView;
    /* 0x50 */ FESceneResource m_feSceneResourceHandle; // size 0x20
}; // total size: 0x70

#endif // _FESCENE_H_
