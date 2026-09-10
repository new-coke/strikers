#include "Game/BasicStadium.h"
#include <stdlib.h>
#include "port/probeobj.h"
extern "C" void OSReport(const char*, ...);
#include "Game/AI/Powerups.h"
#include "Game/Render/NetMesh.h"
#include "Game/Render/CrowdManager.h"
#include "Game/Render/Presentation.h"
#include "Game/Render/StaticModelExplodable.h"
#include "Game/Render/AnimatedModelExplodable.h"
#include "Game/WorldTriggers.h"

#include "Game/Field.h"
#include "Game/Drawable/DrawableModel.h"

#include "NL/nlString.h"
#include "NL/nlPrint.h"
#include "NL/nlFile.h"
#include "NL/nlFormat.h"
#include "NL/gl/glPlat.h"
#include "NL/gl/glState.h"
#include "NL/gl/glTexture.h"
#include "string.h"

#include "Game/World/WorldLoader.h"
#include "Game/Physics/PhysicsNet.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/TrophyTextures.h"

extern const unsigned long eOC_OPTIMIZE_OUT_FROM_GAMEPLAY;
static float g_fSkyboxRotationTime = 1420.0f;
static float g_fCloudRotationTime = 720.0f;

static char szBasicStadiumName[0x40];

const char szBallName[] = "gameplay/ball";
const char szPowerupsName[] = "gameplay/powerups";

static u32 uSkyBoxHashID;
static u32 uCloudsHashID;
static u32 uNetHelperHashID;
static u32 uFieldHelperHashID;
static u32 uPenaltyHelperHashID;
static BasicStadium* pBasicStadiumInstance;
bool g_GoalLightEnabled;

static nlMatrix4 mSkyBoxTM;
static nlMatrix4 mCloudTM;

extern unsigned int nlDefaultSeed;

/**
 * Offset/Address/Size: 0x2D70 | 0x8019EAF0 | size: 0x44
 */
void BasicStadium::BasicStadiumEventHandler(Event* pEvent, void* pUserData)
{
    switch (pEvent->m_uEventID)
    {
    case 5:
        g_GoalLightEnabled = true;
        return;
    case 9:
        g_GoalLightEnabled = false;
        return;
    case 6:
    case 4:
        return;
    }
}

/**
 * Offset/Address/Size: 0x2B64 | 0x8019E8E4 | size: 0x20C
 */
BasicStadium::BasicStadium(const char* szBaseName)
    : World(szBaseName)
{
    m_CameraFlashPositions = NULL;
    m_NumCameraFlashPositions = 0;
    m_fSkyboxRotationAng = 0.0f;
    m_fCloudRotationAng = 0.0f;
    m_bCameraFlashesEnabled = false;
    mpNPCManager = NULL;
    pBasicStadiumInstance = this;

    m_pEventHandler = g_pEventManager->AddEventHandler(BasicStadium::BasicStadiumEventHandler, NULL, 4);

    nlStrNCpy<char>(m_szBaseName, szBaseName, 0x20);
    nlToLower<char>(m_szBaseName);
    if (strstr(m_szBaseName, "mario_") != 0)
    {
        glx_SetFog(0);
    }
    else if (strstr(m_szBaseName, "palace") != 0)
    {
        glx_SetFog(1);
    }
    else if (strstr(m_szBaseName, "dk_daisy") != 0)
    {
        glx_SetFog(2);
    }

    CrowdManager::instance.SetStadium(m_szBaseName);

    g_fSkyboxRotationTime = (strstr(m_szBaseName, "super") != nullptr) ? 800.0f : 1420.0f;

    nlSNPrintf(szBasicStadiumName, 0x40, "Environment/%s/%s", m_szBaseName, m_szBaseName);

    char szTemp[0x100];
    nlStrNCat<char>(szTemp, m_szBaseName, "/skybox back", 0x100);
    uSkyBoxHashID = nlStringLowerHash(szTemp);
    nlStrNCat<char>(szTemp, m_szBaseName, "/skybox clouds", 0x100);
    uCloudsHashID = nlStringLowerHash(szTemp);
    nlStrNCat<char>(szTemp, m_szBaseName, "/NetCorner", 0x100);
    uNetHelperHashID = nlStringLowerHash(szTemp);
    nlStrNCat<char>(szTemp, m_szBaseName, "/FieldCorner", 0x100);
    uFieldHelperHashID = nlStringLowerHash(szTemp);
    nlStrNCat<char>(szTemp, m_szBaseName, "/PenaltyCorner", 0x100);
    uPenaltyHelperHashID = nlStringLowerHash(szTemp);
}

/**
 * Offset/Address/Size: 0x2ABC | 0x8019E83C | size: 0xA8
 */
BasicStadium::~BasicStadium()
{
    g_pEventManager->RemoveEventHandler(m_pEventHandler);
    pBasicStadiumInstance = NULL;

    delete mpNPCManager;

    if (m_CameraFlashPositions != NULL)
    {
        delete[] m_CameraFlashPositions;
    }
}

/**
 * Offset/Address/Size: 0x28F0 | 0x8019E670 | size: 0x1CC
 */
bool BasicStadium::DoLoad()
{
    if (!LoadGeometry(szBasicStadiumName, false, false, 0, 0))
        return false;
    if (!LoadGeometry(szBallName, true, false, 0, 0))
        return false;
    if (!LoadGeometry(szPowerupsName, true, false, 0, 0))
        return false;

    if (!LoadObjectData(szBasicStadiumName))
        return false;

    if (!StaticModelExplodable::LoadGeometry())
        return false;

    StaticModelExplodable::CreateExplodablesFromHelperObjects();

    if (!AnimatedModelExplodable::LoadGeometry())
        return false;

    nlToLower<char>(m_szBaseName);

    char szTemp[0x40];

    nlSNPrintf(szTemp, 0x40, "%s/lightramp", m_szBaseName);
    u32 lightRamp = glGetTexture(szTemp);

    nlSNPrintf(szTemp, 0x40, "%s/playerlightramp", m_szBaseName);
    u32 playerLightRamp = glGetTexture(szTemp);

    if (glTextureLoad(lightRamp))
    {
        m_LightRampTexA = lightRamp; // at +0x124
        m_LightRampTexB = lightRamp; // at +0x128
    }

    if (glTextureLoad(playerLightRamp))
    {
        m_PlayerLightRampTex = playerLightRamp; // at +0x12C
    }

    m_GlobalLightRampSTSTex = glGetTexture("global/lightramp_sts"); // as +0x130

    Presentation::Instance().LoadTrophyModel();
    InitializePowerups();

    NetMesh::s_bAnimatedNetMeshEnabled = true;
    g_GoalLightEnabled = false;

    return true;
}

void BasicStadium::HyperStrikeModelAddHelper(unsigned long hash)
{
    DrawableObject* pObject = FindDrawableObject(hash);
    if (pObject != NULL)
    {
        AddToHyperSTSDrawables(hash, pObject->AsDrawableModel());
    }
}

/**
 * Offset/Address/Size: 0x2E8 | 0x8019C068 | size: 0x2608
 */
bool BasicStadium::DoInitialize()
{
    DrawableObject* pObject;

    FindDrawableObject(nlStringLowerHash("gameplay/ball"));
    m_pSkyboxObject = FindDrawableObject(uSkyBoxHashID);
    m_pCloudsObject = FindDrawableObject(uCloudsHashID);

    if (m_pSkyboxObject != NULL)
    {
        mSkyBoxTM = m_pSkyboxObject->GetWorldMatrix();
    }

    if (m_pCloudsObject != NULL)
    {
        mCloudTM = m_pCloudsObject->GetWorldMatrix();
    }

    pObject = FindDrawableObject(nlStringLowerHash("gameplay/greenshell"));
    pObject->m_uObjectFlags &= 0xFFFFFFFE;
    pObject = FindDrawableObject(nlStringLowerHash("gameplay/redshell"));
    pObject->m_uObjectFlags &= 0xFFFFFFFE;
    pObject = FindDrawableObject(nlStringLowerHash("gameplay/mushroom"));
    pObject->m_uObjectFlags &= 0xFFFFFFFE;
    pObject = FindDrawableObject(nlStringLowerHash("gameplay/banana"));
    pObject->m_uObjectFlags &= 0xFFFFFFFE;
    pObject = FindDrawableObject(nlStringLowerHash("gameplay/blueshell"));
    pObject->m_uObjectFlags &= 0xFFFFFFFE;
    pObject = FindDrawableObject(nlStringLowerHash("gameplay/spikeshell"));
    pObject->m_uObjectFlags &= 0xFFFFFFFE;
    pObject = FindDrawableObject(nlStringLowerHash("gameplay/bobomb"));
    pObject->m_uObjectFlags &= 0xFFFFFFFE;
    pObject = FindDrawableObject(nlStringLowerHash("gameplay/star"));
    pObject->m_uObjectFlags &= 0xFFFFFFFE;

    {
        int counter = 1;
        int nInvisPackets = 0;
        u8 keepLooking = 1;
        DrawableModel* model;
        glModel* pGlModel;
        glModelPacket* pPacket;
        while (keepLooking)
        {
            char szTemp1[256];
            char szTemp2[256];
            nlSNPrintf(szTemp1, 0x100, "/InvisiblePoly0%d", counter);
            nlStrNCat<char>(szTemp2, m_szBaseName, szTemp1, 0x100);
            pObject = FindDrawableObject(nlStringLowerHash(szTemp2));
            if (pObject != NULL)
            {
                model = pObject->AsDrawableModel();
                pGlModel = model->m_pModel;
                for (pPacket = pGlModel->packets; pPacket < pGlModel->packets + pGlModel->numPackets; pPacket++)
                {
                    glSetRasterState(pPacket->state.raster, GLS_DepthWrite, 1);
                    glSetRasterState(pPacket->state.raster, GLS_AlphaTest, 0);
                    glSetRasterState(pPacket->state.raster, GLS_AlphaBlend, 1);
                    ++nInvisPackets;
                }
                counter += 1;
            }
            else
            {
                keepLooking = 0;
            }
        }
        // PORT: Say what the loop found, so an experiment on these packets can tell "hypothesis wrong" from "probe never fired".
        OSReport("[invispoly] %d objects, %d packets\n", counter - 1, nInvisPackets);
    }

    // PORT: Hide named stadium objects.
    {
        const char* hide = getenv("STRIKERS_HIDE_OBJ");
        if (hide != NULL)
        {
            char list[512];
            nlStrNCpy<char>(list, hide, sizeof(list));
            char* p = list;
            while (*p != '\0')
            {
                char* comma = p;
                while (*comma != '\0' && *comma != ',')
                    ++comma;
                const char had = *comma;
                *comma = '\0';

                char full[256];
                char sep[300];
                nlSNPrintf(sep, sizeof(sep), "/%s", p);
                nlStrNCat<char>(full, m_szBaseName, sep, sizeof(full));

                DrawableObject* pHide = FindDrawableObject(nlStringLowerHash(full));
                int nPackets = 0;
                if (pHide != NULL)
                {
                    DrawableModel* m = pHide->AsDrawableModel();
                    if (m != NULL && m->m_pModel != NULL)
                    {
                        glModel* gm = m->m_pModel;
                        for (glModelPacket* pk = gm->packets;
                             pk < gm->packets + gm->numPackets; pk++)
                        {
                            glSetRasterState(pk->state.raster, GLS_ColourWrite, 0);
                            ++nPackets;
                        }
                    }
                }
                OSReport("[hideobj] %s: %s, %d packets\n", full,
                         pHide != NULL ? "found" : "NOT FOUND", nPackets);

                if (had == '\0')
                    break;
                p = comma + 1;
            }
        }
    }

    {
        const char* probeList = getenv("STRIKERS_PROBE_OBJ");
        if (probeList != NULL)
        {
        char plist[512];
        nlStrNCpy<char>(plist, probeList, sizeof(plist));
        char* probe = plist;
        while (*probe != '\0')
        {
            char* pComma = probe;
            while (*pComma != '\0' && *pComma != ',')
                ++pComma;
            const char pMore = *pComma;
            *pComma = '\0';
            char full[256];
            char sep[300];
            // A name containing '/' is taken verbatim, because not every stadium prefixes its objects with its BasicStadium base name.
            if (strchr(probe, '/') != NULL)
            {
                nlStrNCpy<char>(full, probe, sizeof(full));
            }
            else
            {
                nlSNPrintf(sep, sizeof(sep), "/%s", probe);
                nlStrNCat<char>(full, m_szBaseName, sep, sizeof(full));
            }
            DrawableObject* pObj = FindDrawableObject(nlStringLowerHash(full));
            int nPk = 0;
            if (pObj != NULL)
            {
                DrawableModel* m = pObj->AsDrawableModel();
                if (m != NULL && m->m_pModel != NULL)
                {
                    nPk = (int)m->m_pModel->numPackets;
                    PortProbeObjRegister(m->m_pModel->packets, nPk,
                                         (unsigned)sizeof(glModelPacket), full);
                    // The render list may submit shallow copies of these packets.
                    for (int i = 0; i < nPk; i++)
                        PortProbeObjRegisterKey(full, i,
                            (const void*)m->m_pModel->packets[i].streams);
                }
            }
            OSReport("[probeobj] %s: %s, %d packets\n", full,
                     pObj != NULL ? "found" : "NOT FOUND", nPk);
            if (pMore == '\0')
                break;
            probe = pComma + 1;
        }
        }

        const char* hidepk = getenv("STRIKERS_HIDE_PACKET");
        if (hidepk != NULL)
        {
            char list2[512];
            nlStrNCpy<char>(list2, hidepk, sizeof(list2));
            char* q = list2;
            while (*q != '\0')
            {
            char* comma = q;
            while (*comma != '\0' && *comma != ',')
                ++comma;
            const char hadMore = *comma;
            *comma = '\0';
            char buf[256];
            nlStrNCpy<char>(buf, q, sizeof(buf));
            char* colon = buf;
            while (*colon != '\0' && *colon != ':')
                ++colon;
            if (*colon == ':')
            {
                *colon = '\0';
                int idx = atoi(colon + 1);
                char full[256];
                char sep[300];
                if (strchr(buf, '/') != NULL)
                {
                    nlStrNCpy<char>(full, buf, sizeof(full));
                }
                else
                {
                    nlSNPrintf(sep, sizeof(sep), "/%s", buf);
                    nlStrNCat<char>(full, m_szBaseName, sep, sizeof(full));
                }
                DrawableObject* pObj = FindDrawableObject(nlStringLowerHash(full));
                int done = 0;
                if (pObj != NULL)
                {
                    DrawableModel* m = pObj->AsDrawableModel();
                    if (m != NULL && m->m_pModel != NULL &&
                        idx >= 0 && idx < (int)m->m_pModel->numPackets)
                    {
                        glSetRasterState(m->m_pModel->packets[idx].state.raster,
                                         GLS_ColourWrite, 0);
                        done = 1;
                    }
                }
                OSReport("[hidepacket] %s idx %d: %s\n", full, idx,
                         done ? "hidden" : "NOT FOUND");
            }
            if (hadMore == '\0')
                break;
            q = comma + 1;
            }
        }
    }

    HyperStrikeModelAddHelper(nlStringLowerHash("gameplay/ball"));

    HyperStrikeModelAddHelper(nlStringLowerHash("wario_stadium/WarioNet05"));

    HyperStrikeModelAddHelper(nlStringLowerHash("wario_stadium/WarioNet01"));

    HyperStrikeModelAddHelper(nlStringLowerHash("wario_stadium/Goal_Lights_01"));

    HyperStrikeModelAddHelper(nlStringLowerHash("wario_stadium/Goal_Lights_02"));

    HyperStrikeModelAddHelper(nlStringLowerHash("wario_stadium/WarioNet_Reflect_01"));

    HyperStrikeModelAddHelper(nlStringLowerHash("wario_stadium/WarioNet_Reflect_02"));

    HyperStrikeModelAddHelper(nlStringLowerHash("wario_stadium/WarioNet_Reflect_03"));

    HyperStrikeModelAddHelper(nlStringLowerHash("wario_stadium/WarioNet_Reflect_04"));

    HyperStrikeModelAddHelper(nlStringLowerHash("mario_stadium/MarioNet04"));

    HyperStrikeModelAddHelper(nlStringLowerHash("mario_stadium/MarioNet05"));

    HyperStrikeModelAddHelper(nlStringLowerHash("yoshi_stadium/Net04"));

    HyperStrikeModelAddHelper(nlStringLowerHash("yoshi_stadium/Net05"));

    HyperStrikeModelAddHelper(nlStringLowerHash("DK_Daisy/Goal_Front_01"));

    HyperStrikeModelAddHelper(nlStringLowerHash("DK_Daisy/Goal_Front_02"));

    HyperStrikeModelAddHelper(nlStringLowerHash("DK_Daisy/GoldGoal_01"));

    HyperStrikeModelAddHelper(nlStringLowerHash("DK_Daisy/GoldGoal_02"));

    HyperStrikeModelAddHelper(nlStringLowerHash("DK_Daisy/line01"));

    HyperStrikeModelAddHelper(nlStringLowerHash("DK_Daisy/line02"));

    HyperStrikeModelAddHelper(nlStringLowerHash("DK_Daisy/line03"));

    HyperStrikeModelAddHelper(nlStringLowerHash("DK_Daisy/line04"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/goal01"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/goal05"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/posts_02"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/posts_03"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/posts_shadow"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/posts_shadowed_02"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_01"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_02"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_03"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_04"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_05"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_06"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_07"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_08"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_09"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_10"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_11"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_12"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_13"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_14"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_15"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_16"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_17"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_18"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_19"));

    HyperStrikeModelAddHelper(nlStringLowerHash("The_Palace/Rebar_20"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Forbidden_Dome/Goalpost_04"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Forbidden_Dome/Goalpost_05"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Forbidden_Dome/Net_Front_02"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Forbidden_Dome/Net_Front_05"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Forbidden_Dome/Net_Front_06"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Forbidden_Dome/Net_Front_07"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Super_Stadium/goal01"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Super_Stadium/goal02"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Super_Stadium/Goalballs01"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Super_Stadium/Goalballs02"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Super_Stadium/goal_glass01"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Super_Stadium/goal_glass02"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Super_Stadium/object668"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Super_Stadium/object669"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Super_Stadium/object670"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Super_Stadium/object671"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Super_Stadium/object672"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Super_Stadium/object673"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Super_Stadium/Laser01"));

    HyperStrikeModelAddHelper(nlStringLowerHash("Super_Stadium/Laser02"));

    HelperObject* pFieldHelper = FindHelperObject(uFieldHelperHashID);
    if (pFieldHelper != NULL)
    {
        cField::SetFieldDimensions(pFieldHelper->m_worldMatrix.m41, pFieldHelper->m_worldMatrix.m42, 0.0f);
    }

    // Stadium.ini Config
    {
        Config stadConfig(Config::ALLOCATE_HIGH);
        char szKey[256];
        eStadiumID stadiumid = nlSingleton<GameInfoManager>::Instance()->GetStadium();
        stadConfig.LoadFromFile("Stadium.ini");
        nlStrNCat<char>(szKey, m_szBaseName, " pitch", 0x100);
        BasicString<char, Detail::TempStringAllocator> terrain = stadConfig.Get<BasicString<char, Detail::TempStringAllocator> >(szKey, BasicString<char, Detail::TempStringAllocator>("grass"));
        TheWorldLoader.SetStadiumTerrain(stadiumid, terrain.c_str());
        m_TerrainType = TheWorldLoader.GetStadiumTerrain(stadiumid);
        fxSetTerrain(g_TerrainTypeHashes[m_TerrainType]);
    }

    // GoalPostDimensions.ini Config
    {
        Config gpConfig(Config::ALLOCATE_HIGH);
        gpConfig.LoadFromFile("GoalPostDimensions.ini");
        char szKey[256];
        nlStrNCat<char>(szKey, m_szBaseName, " goalpost radius", 0x100);
        float fGoalpostRadius = GetConfigFloat(gpConfig, szKey, 0.0f);
        nlStrNCat<char>(szKey, m_szBaseName, " goalpost offset", 0x100);
        float fGoalpostOffset = GetConfigFloat(gpConfig, szKey, 0.0f);
        nlStrNCat<char>(szKey, m_szBaseName, " net width", 0x100);
        float fNetWidth = GetConfigFloat(gpConfig, szKey, 0.0f);
        nlStrNCat<char>(szKey, m_szBaseName, " net height", 0x100);
        float fNetHeight = GetConfigFloat(gpConfig, szKey, 0.0f);
        cNet::SetNetDimensions(fNetWidth, fNetHeight, fGoalpostRadius, fGoalpostOffset);
        nlStrNCat<char>(szKey, m_szBaseName, " physics net width", 0x100);
        float fPhysNetWidth = GetConfigFloat(gpConfig, szKey, 0.0f);
        nlStrNCat<char>(szKey, m_szBaseName, " physics net height", 0x100);
        float fPhysNetHeight = GetConfigFloat(gpConfig, szKey, 0.0f);
        nlStrNCat<char>(szKey, m_szBaseName, " physics net depth", 0x100);
        float fPhysNetDepth = GetConfigFloat(gpConfig, szKey, 0.0f);
        PhysicsNet::sfPhysicsNetWidth = fPhysNetWidth;
        PhysicsNet::sfPhysicsNetHeight = fPhysNetHeight;
        PhysicsNet::sfPhysicsNetDepth = fPhysNetDepth;
        nlStrNCat<char>(szKey, m_szBaseName, " physics net softness", 0x100);
        float fSoftness = GetConfigFloat(gpConfig, szKey, -1.0f);
        if (fSoftness >= 0.0f)
        {
            PhysicsNet::sfWallSoftness = fSoftness;
        }
        nlStrNCat<char>(szKey, m_szBaseName, " dont use lowest net texture LOD", 0x100);
        bool bDontUseLowest = GetConfigBool(gpConfig, szKey, false);
        NetMesh::SetDontUseLowestNetTextureLOD(bDontUseLowest);
    }

    // PlanarShadow.ini Config
    {
        Config psConfig(Config::ALLOCATE_HIGH);
        psConfig.LoadFromFile("PlanarShadow.ini");
        char szKey[256];
        nlStrNCat<char>(szKey, m_szBaseName, " Shadow Height", 0x100);
        float fShadowHeight = GetConfigFloat(psConfig, szKey, 0.1f);
        SetCoPlanarZ(fShadowHeight);
        nlStrNCat<char>(szKey, m_szBaseName, " Shadow Opacity", 0x100);
        float fShadowOpacity = GetConfigFloat(psConfig, szKey, 0.3f);
        SetPlanarShadowOpacity(fShadowOpacity);
    }

    // PenaltyHelper
    HelperObject* pPenBoxHelper = FindHelperObject(uPenaltyHelperHashID);
    if (pPenBoxHelper != NULL)
    {
        float fy = pPenBoxHelper->m_worldMatrix.m42;
        cField::mfPenaltyBoxX = pPenBoxHelper->m_worldMatrix.m41;
        cField::mfPenaltyBoxY = fy;
    }

    // Camera flash - Pass 1: Count
    typedef nlAVLTreeIterator<unsigned long, HelperObject*, DefaultKeyCompare<unsigned long> > HelperIterator;
    m_NumCameraFlashPositions = 0;
    HelperIterator* iterator = new (nlMalloc(sizeof(HelperIterator), 8, false)) HelperIterator(m_helperMap);
    const char* flashString = "fx_camera_flash";
    HelperObject* pHelper;
    while (iterator->m_NumStackEntries > 0)
    {
        pHelper = iterator->m_Stack[iterator->m_NumStackEntries - 1]->value;
        if (nlStrNICmp<char>(pHelper->m_szName, flashString, nlStrLen<char>(flashString)) == 0)
        {
            m_NumCameraFlashPositions++;
        }
        iterator->Next();
    }
    delete iterator;

    // Camera flash - Pass 2: Extract positions
    m_CameraFlashPositions = (nlVector3*)nlMalloc(m_NumCameraFlashPositions * sizeof(nlVector3), 8, false);
    m_NumCameraFlashPositions = 0;
    iterator = new (nlMalloc(sizeof(HelperIterator), 8, false)) HelperIterator(m_helperMap);
    while (iterator->m_NumStackEntries > 0)
    {
        pHelper = iterator->m_Stack[iterator->m_NumStackEntries - 1]->value;
        if (nlStrNICmp<char>(pHelper->m_szName, flashString, nlStrLen<char>(flashString)) == 0)
        {
            m_CameraFlashPositions[m_NumCameraFlashPositions] = *(nlVector3*)&pHelper->m_worldMatrix.m41;
            m_NumCameraFlashPositions++;
        }
        iterator->Next();
    }
    delete iterator;

    // Extra ball loop
    {
        const DrawableObject* ball = FindDrawableObject(nlStringHash("gameplay/ball"));
        if (ball != NULL)
        {
            for (int i = 0; i < 2; i++)
            {
                DrawableObject* extraBall = ball->Clone();
                extraBall->m_uObjectFlags &= 0xFFFFFFFE;
                BasicString<char, Detail::TempStringAllocator> name = Format(BasicString<char, Detail::TempStringAllocator>("extra_ball_{0}"), i);
                unsigned long extraHash = nlStringHash(name.c_str());
                AddDrawableObject(extraHash, extraBall);
            }
        }
    }

    // Optimization file loading
    LoadListOfGameplayInvisibleModels();

    return true;
}
/**
 * Offset/Address/Size: 0x8C | 0x8019BE0C | size: 0x25C
 */
void BasicStadium::Update(float fTimeDelta)
{
    nlMatrix4 mWorld;
    nlMatrix4 mRot;

    World::Update(fTimeDelta);

    m_fSkyboxRotationAng += fTimeDelta * (6.2831855f / g_fSkyboxRotationTime);
    m_fCloudRotationAng += fTimeDelta * (6.2831855f / g_fCloudRotationTime);

    if (m_fSkyboxRotationAng > 6.2831855f)
    {
        m_fSkyboxRotationAng -= 6.2831855f;
    }

    if (m_fCloudRotationAng > 6.2831855f)
    {
        m_fCloudRotationAng -= 6.2831855f;
    }

    if (m_pSkyboxObject != NULL)
    {
        nlMakeRotationMatrixZ(mRot, m_fSkyboxRotationAng);
        nlMultMatrices(mWorld, mRot, mSkyBoxTM);
        m_pSkyboxObject->m_worldMatrix = mWorld;
    }

    if (m_pCloudsObject != NULL)
    {
        nlMakeRotationMatrixZ(mRot, m_fCloudRotationAng);
        nlMultMatrices(mWorld, mRot, mCloudTM);
        m_pCloudsObject->m_worldMatrix = mWorld;
    }

    if (m_bCameraFlashesEnabled)
    {
        UpdateCameraFlashes(fTimeDelta);
    }
}

/**
 * Offset/Address/Size: 0x48 | 0x8019BDC8 | size: 0x44
 */
void BasicStadium::UpdateInReplay(float fTimeDelta)
{
    World::UpdateInReplay(fTimeDelta);
    mpNPCManager->UpdateNPCs(fTimeDelta);
}

/**
 * Offset/Address/Size: 0x8 | 0x8019BD88 | size: 0x40
 */
void BasicStadium::Render()
{
    World::Render();
    if (!World::sbIsHyperShootToScoreRenderingEnabled)
    {
        mpNPCManager->RenderNPCs();
    }
}

void BasicStadium::ResetObjectsForNewGame()
{
}

void BasicStadium::StartCameraFlashes()
{
    m_fTimeUntilNextCameraFlash = 0.0f;
}

void BasicStadium::UpdateCameraFlashes(float fTimeDelta)
{
    if (m_NumCameraFlashPositions != 0)
    {
        u32 randomValue = nlRandom(m_NumCameraFlashPositions, &nlDefaultSeed);
        m_fTimeUntilNextCameraFlash -= fTimeDelta;
        if (m_fTimeUntilNextCameraFlash < 0.0f)
        {
            EmitCameraFlash(m_CameraFlashPositions[randomValue]);
            m_fTimeUntilNextCameraFlash = 0.01f + nlRandomf(0.05f, &nlDefaultSeed);
        }
    }
}

void BasicStadium::LoadListOfGameplayInvisibleModels()
{
    char szOptBin[80];
    nlSNPrintf(szOptBin, 0x50, "%s-GameplayInvisibleModels.bin", m_szBaseName);
    unsigned long fileSize;
    u32* pCur;
    u32* pData = (u32*)nlLoadEntireFile(szOptBin, &fileSize, 0x20, AllocateEnd);
    if (pData != NULL)
    {
        unsigned long j;
        u32 uFlags = eOC_OPTIMIZE_OUT_FROM_GAMEPLAY;
        pCur = pData;
        fileSize = fileSize >> 2;
        for (j = 0; j < fileSize; j++)
        {
            DrawableObject* pObj = FindDrawableObject(*pCur);
            if (pObj != NULL)
            {
                DrawableModel* pModel = pObj->AsDrawableModel();
                pModel->m_uObjectCreationFlags |= uFlags;
            }
            pCur++;
        }
        nlFree(pData);
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8019BD80 | size: 0x8
 */
BasicStadium* BasicStadium::GetCurrentStadium()
{
    return pBasicStadiumInstance;
}

const unsigned long eOC_OPTIMIZE_OUT_FROM_GAMEPLAY = 0x00008000;
