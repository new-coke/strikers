#include "Game/GameObjectLighting.h"

#include "Game/GameInfo.h"
#include "Game/Camera/CameraMan.h"
#include "Game/Drawable/DrawableCharacter.h"
#include "NL/gl/glState.h"
#include "NL/gl/glLightUserData.h"
#include "NL/gl/glUserData.h"
#include "types.h"

struct StadiumLightingParams
{
    /* 0x00 */ u32 lightRamp;
    /* 0x04 */ s32 rampStartR;
    /* 0x08 */ s32 rampStartG;
    /* 0x0C */ s32 rampStartB;
    /* 0x10 */ s32 rampEndR;
    /* 0x14 */ s32 rampEndG;
    /* 0x18 */ s32 rampEndB;
    /* 0x1C */ f32 keyLightIntensity;
    /* 0x20 */ f32 fillLightIntensity;
    /* 0x24 */ f32 inGameKeyIntensity;
    /* 0x28 */ f32 inGameFillIntensity;
    /* 0x2C */ f32 inGameKeyRotYDeg;
    /* 0x30 */ f32 inGameKeyRotZDeg;
    /* 0x34 */ f32 inGameFillRotYDeg;
    /* 0x38 */ f32 inGameFillRotZDeg;
}; // total size: 0x3C

struct GameObjectLightArray
{
    u32 numLights;             // +0x00
    GLLightUserData lights[2]; // +0x04
}; // total size: 0x54

StadiumLightingParams gStadiumGameObjectLightingParams[7] = {
    { glGetTexture("MarioStadiumPlayerLightRamp"), 0x30, 0x28, 0x20, 0xAF, 0xAF, 0x9F, 0.85f, 0.3f, 0.8f, 0.25f, 55.0f, 60.0f, 55.0f, -120.0f },
    { glGetTexture("PeachToadStadiumPlayerLightRamp"), 0x28, 0x30, 0x28, 0xAF, 0xAF, 0xAF, 0.85f, 0.3f, 0.9f, 0.25f, 55.0f, 60.0f, 55.0f, -120.0f },
    { glGetTexture("DKDaisyStadiumPlayerLightRamp"), 0x30, 0x28, 0x20, 0xAF, 0xAF, 0xAF, 0.85f, 0.3f, 0.7f, 0.15f, 55.0f, 60.0f, 55.0f, -120.0f },
    { glGetTexture("WarioStadiumPlayerLightRamp"), 0x28, 0x28, 0x30, 0xAF, 0xAF, 0xAF, 0.85f, 0.3f, 0.9f, 0.25f, 55.0f, 60.0f, 55.0f, -120.0f },
    { glGetTexture("YoshiStadiumPlayerLightRamp"), 0x28, 0x30, 0x28, 0xAF, 0xAF, 0xAF, 0.85f, 0.3f, 0.8f, 0.25f, 55.0f, 120.0f, 55.0f, -60.0f },
    { glGetTexture("SuperStadiumPlayerLightRamp"), 0x28, 0x28, 0x30, 0xAF, 0xAF, 0xAF, 0.85f, 0.3f, 0.9f, 0.25f, 55.0f, 60.0f, 55.0f, -120.0f },
    { glGetTexture("ForbiddenDomePlayerLightRamp"), 0x28, 0x28, 0x28, 0xAF, 0xAF, 0xAF, 0.85f, 0.3f, 0.8f, 0.25f, 55.0f, 60.0f, 55.0f, -120.0f }
};

static void* g_pCameraRelativeLightData = NULL;
static void* g_pInGameLightData = NULL;
static PlatTexture* g_pGameObjectLightRamp = NULL;

/**
 * Offset/Address/Size: 0xB0C | 0x8012B348 | size: 0x8
 */
void* GetCameraRelativeLightData()
{
    return g_pCameraRelativeLightData;
}

/**
 * Offset/Address/Size: 0xB04 | 0x8012B340 | size: 0x8
 */
void* GetInGameLightData()
{
    return g_pInGameLightData;
}

bool gAlwaysUseCameraRelativeCharacterLighting;

/**
 * Offset/Address/Size: 0xAFC | 0x8012B338 | size: 0x8
 */
bool AlwaysUseCameraRelativeCharacterLighting()
{
    return gAlwaysUseCameraRelativeCharacterLighting;
}

/**
 * Offset/Address/Size: 0x244 | 0x8012AA80 | size: 0x8B8
 */
void InitializeGameObjectLighting()
{
    GameObjectLightArray* pLightArray;
    GLLightUserData* pLight;
    s32 i;
    f32 deltaB;
    f32 deltaG;
    f32 deltaR;
    StadiumLightingParams* pParams;
    u8* pTextureData;
    PlatTexture* pRampTexture;
    s32 stadium;
    nlMatrix4 matZ;
    nlMatrix4 matY;

    g_pCameraRelativeLightData = glUserAlloc(GLUD_Light, 0x54, true);
    pLightArray = (GameObjectLightArray*)glUserGetData(g_pCameraRelativeLightData);
    pLightArray->numLights = 2;
    pLight = pLightArray->lights;

    for (i = 0; i < 2; i++)
    {
        nlZeroMemory(pLight, 0x28);
        pLight->colour.c[0] = 1.0f;
        pLight->colour.c[1] = 0.0f;
        pLight->colour.c[2] = 0.0f;
        pLight->colour.c[3] = 1.0f;
        pLight->intensity = 1.0f;
        pLight->innerRadius = 0.0f;
        pLight->outerRadius = 0.0f;
        pLight++;
    }

    g_pInGameLightData = glUserAlloc(GLUD_Light, 0x54, true);
    pLightArray = (GameObjectLightArray*)glUserGetData(g_pInGameLightData);
    pLightArray->numLights = 2;
    {
        GLLightUserData* pLight2 = pLightArray->lights;
        s32 j;
        for (j = 0; j < 2; j++)
        {
            nlZeroMemory(pLight2, 0x28);
            pLight2->colour.c[0] = 1.0f;
            pLight2->colour.c[1] = 0.0f;
            pLight2->colour.c[2] = 0.0f;
            pLight2->colour.c[3] = 1.0f;
            pLight2->intensity = 1.0f;
            pLight2->innerRadius = 0.0f;
            pLight2->outerRadius = 0.0f;
            pLight2++;
        }
    }

    pLightArray = (GameObjectLightArray*)glUserGetData(g_pInGameLightData);
    stadium = nlSingleton<GameInfoManager>::Instance()->GetStadium();
    pParams = &gStadiumGameObjectLightingParams[stadium];

    {
        nlVector3 initialDirection = { 1.0f, 0.0f, 0.0f };
        nlVector3 fillDirection;
        nlVector3 keyDirection;

        nlMakeRotationMatrixY(matY, (3.1415927f * pParams->inGameKeyRotYDeg) / 180.0f);
        nlMakeRotationMatrixZ(matZ, (3.1415927f * pParams->inGameKeyRotZDeg) / 180.0f);
        nlMultDirVectorMatrix(keyDirection, initialDirection, matY);
        nlMultDirVectorMatrix(keyDirection, keyDirection, matZ);

        nlMakeRotationMatrixY(matY, (3.1415927f * pParams->inGameFillRotYDeg) / 180.0f);
        nlMakeRotationMatrixZ(matZ, (3.1415927f * pParams->inGameFillRotZDeg) / 180.0f);
        nlMultDirVectorMatrix(fillDirection, initialDirection, matY);
        nlMultDirVectorMatrix(fillDirection, fillDirection, matZ);

        nlVec3Set(pLightArray->lights[0].worldPosition, -keyDirection.x, -keyDirection.y, -keyDirection.z);
        pLightArray->lights[0].intensity = pParams->inGameKeyIntensity;

        nlVec3Set(pLightArray->lights[1].worldPosition, -fillDirection.x, -fillDirection.y, -fillDirection.z);
        pLightArray->lights[1].intensity = pParams->inGameFillIntensity;
    }

    stadium = nlSingleton<GameInfoManager>::Instance()->GetStadium();
    g_pGameObjectLightRamp = glx_CreatePlatTexture();
    pRampTexture = g_pGameObjectLightRamp;
    glx_AddTex(gStadiumGameObjectLightingParams[stadium].lightRamp, pRampTexture);
    g_pGameObjectLightRamp->Create(0x100, 4, GXTex_RGBA8, 1, true, false);

    stadium = nlSingleton<GameInfoManager>::Instance()->GetStadium();
    pRampTexture = g_pGameObjectLightRamp;
    StadiumLightingParams* pRampParams = &gStadiumGameObjectLightingParams[stadium];

    deltaR = (f32)(pRampParams->rampEndR - pRampParams->rampStartR);
    deltaG = (f32)(pRampParams->rampEndG - pRampParams->rampStartG);
    deltaB = (f32)(pRampParams->rampEndB - pRampParams->rampStartB);

    pTextureData = (u8*)pRampTexture->m_LinearData;
    for (i = 0; i < 0x100; i += 8)
    {
        {
            f32 t = (f32)i / 256.0f;
            pTextureData[0] = (u8)(t * deltaR + (f32)pRampParams->rampStartR);
            pTextureData[1] = (u8)(t * deltaG + (f32)pRampParams->rampStartG);
            pTextureData[2] = (u8)(t * deltaB + (f32)pRampParams->rampStartB);
            pTextureData[3] = 0xFF;
        }
        {
            f32 t = (f32)(i + 1) / 256.0f;
            pTextureData[4] = (u8)(t * deltaR + (f32)pRampParams->rampStartR);
            pTextureData[5] = (u8)(t * deltaG + (f32)pRampParams->rampStartG);
            pTextureData[6] = (u8)(t * deltaB + (f32)pRampParams->rampStartB);
            pTextureData[7] = 0xFF;
        }
        {
            f32 t = (f32)(i + 2) / 256.0f;
            pTextureData[8] = (u8)(t * deltaR + (f32)pRampParams->rampStartR);
            pTextureData[9] = (u8)(t * deltaG + (f32)pRampParams->rampStartG);
            pTextureData[10] = (u8)(t * deltaB + (f32)pRampParams->rampStartB);
            pTextureData[11] = 0xFF;
        }
        {
            f32 t = (f32)(i + 3) / 256.0f;
            pTextureData[12] = (u8)(t * deltaR + (f32)pRampParams->rampStartR);
            pTextureData[13] = (u8)(t * deltaG + (f32)pRampParams->rampStartG);
            pTextureData[14] = (u8)(t * deltaB + (f32)pRampParams->rampStartB);
            pTextureData[15] = 0xFF;
        }
        {
            f32 t = (f32)(i + 4) / 256.0f;
            pTextureData[16] = (u8)(t * deltaR + (f32)pRampParams->rampStartR);
            pTextureData[17] = (u8)(t * deltaG + (f32)pRampParams->rampStartG);
            pTextureData[18] = (u8)(t * deltaB + (f32)pRampParams->rampStartB);
            pTextureData[19] = 0xFF;
        }
        {
            f32 t = (f32)(i + 5) / 256.0f;
            pTextureData[20] = (u8)(t * deltaR + (f32)pRampParams->rampStartR);
            pTextureData[21] = (u8)(t * deltaG + (f32)pRampParams->rampStartG);
            pTextureData[22] = (u8)(t * deltaB + (f32)pRampParams->rampStartB);
            pTextureData[23] = 0xFF;
        }
        {
            f32 t = (f32)(i + 6) / 256.0f;
            pTextureData[24] = (u8)(t * deltaR + (f32)pRampParams->rampStartR);
            pTextureData[25] = (u8)(t * deltaG + (f32)pRampParams->rampStartG);
            pTextureData[26] = (u8)(t * deltaB + (f32)pRampParams->rampStartB);
            pTextureData[27] = 0xFF;
        }
        {
            f32 t = (f32)(i + 7) / 256.0f;
            pTextureData[28] = (u8)(t * deltaR + (f32)pRampParams->rampStartR);
            pTextureData[29] = (u8)(t * deltaG + (f32)pRampParams->rampStartG);
            pTextureData[30] = (u8)(t * deltaB + (f32)pRampParams->rampStartB);
            pTextureData[31] = 0xFF;
        }
        pTextureData += 0x20;
    }

    for (i = 1; i < 4; i++)
    {
        memcpy((u8*)g_pGameObjectLightRamp->m_LinearData + (i * 0x400), g_pGameObjectLightRamp->m_LinearData, 0x400);
    }

    g_pGameObjectLightRamp->Swizzle(false);
    g_pGameObjectLightRamp->Prepare();
}

/**
 * Unreferenced helper, dead-stripped at link. Its deferred-emission slot
 * (between UpdateGameObjectLighting and InitializeGameObjectLighting) is what
 * allocates the pi/180 constants into .sdata2 before Init's own literals,
 * matching the target pool order. The target's local-symbol numbering
 * (@684/@685 vs @814/@815) shows the original TU had static code emitted at
 * exactly this position.
 */
static f32 GameObjectDegToRad(f32 deg)
{
    return (3.1415927f * deg) / 180.0f;
}

/**
 * Offset/Address/Size: 0x34 | 0x8012A870 | size: 0x210
 */
void UpdateGameObjectLighting()
{
    if (!DrawableCharacter::sCameraRelativeLighting && !gAlwaysUseCameraRelativeCharacterLighting)
        return;

    static nlVector3 keyLightInViewSpace;
    static nlVector3 fillLightInViewSpace;
    static bool initedLightInViewSpace;
    static s8 init;

    nlVector3 transformedDir;
    nlVector3 viewVec;
    nlMatrix4 viewRotMat;
    nlMatrix4 matZ;
    nlMatrix4 matY;

    void* pLightData = g_pCameraRelativeLightData;

    if (!init)
    {
        initedLightInViewSpace = false;
        init = true;
    }

    if (!initedLightInViewSpace)
    {
        nlVector3 initialDirection = { 1.0f, 0.0f, 0.0f };

        nlMakeRotationMatrixY(matY, 0.7853982f);
        nlMakeRotationMatrixZ(matZ, -0.69813174f);
        nlMultDirVectorMatrix(keyLightInViewSpace, initialDirection, matY);
        nlMultDirVectorMatrix(keyLightInViewSpace, keyLightInViewSpace, matZ);

        nlMakeRotationMatrixY(matY, 0.5235988f);
        nlMakeRotationMatrixZ(matZ, 0.34906587f);
        nlMultDirVectorMatrix(fillLightInViewSpace, initialDirection, matY);
        nlMultDirVectorMatrix(fillLightInViewSpace, fillLightInViewSpace, matZ);

        initedLightInViewSpace = true;
    }

    cCameraManager::GetViewVector(viewVec);

    f32 angle = nlATan2f(viewVec.y, viewVec.x);
    u16 u16Angle = (u16)(s32)(angle * 10430.378f);
    f32 radAngle = (f32)u16Angle * 0.0000958738f;

    nlMakeRotationMatrixZ(viewRotMat, radAngle);

    s32 stadium = nlSingleton<GameInfoManager>::Instance()->GetStadium();
    StadiumLightingParams* params = &gStadiumGameObjectLightingParams[stadium];

    GameObjectLightArray* pLights = (GameObjectLightArray*)glUserGetData(pLightData);

    nlMultDirVectorMatrix(transformedDir, keyLightInViewSpace, viewRotMat);

    nlVec3Set(pLights->lights[0].worldPosition, -transformedDir.x, -transformedDir.y, -transformedDir.z);
    pLights->lights[0].intensity = params->keyLightIntensity;

    nlMultDirVectorMatrix(transformedDir, fillLightInViewSpace, viewRotMat);

    nlVec3Set(pLights->lights[1].worldPosition, -transformedDir.x, -transformedDir.y, -transformedDir.z);
    pLights->lights[1].intensity = params->fillLightIntensity;
}

/**
 * Unreferenced helper, dead-stripped at link. Emitted before
 * UpdateGameObjectLighting (deferred emission runs bottom-to-top), it
 * allocates the ramp constants (1/256 and the s32-to-f32 magic double) at the
 * front of .sdata2, matching the target pool order (target symbols
 * @573/@575, created before Update's @653+).
 */
static f32 GameObjectLightRampT(s32 i)
{
    return 0.00390625f * (f32)i;
}

/**
 * Offset/Address/Size: 0x0 | 0x8012A83C | size: 0x34
 */
u32 GetGameObjectLightRamp()
{
    return gStadiumGameObjectLightingParams[nlSingleton<GameInfoManager>::Instance()->GetStadium()].lightRamp;
}
