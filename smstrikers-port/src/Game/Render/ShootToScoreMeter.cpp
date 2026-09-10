#include "Game/Render/ShootToScoreMeter.h"
#include "Game/Camera/CameraMan.h"
#include "Game/Game.h"
#include "NL/nlString.h"
#include "NL/gl/glDraw3.h"
#include "NL/gl/glState.h"
#include "NL/platvmath.h"

static u32 UnlitProgram = glGetProgram("3d unlit");
static u32 LitProgram = glGetProgram("3d pointlit");
static u32 LightTexture = glGetTexture("global/lightramp");
static u32 BlackTexture = glGetTexture("global/black");
static u32 WhiteTexture = glGetTexture("global/white");
static u32 MeterTexture = glGetTexture("fe/shoot_to_score_track_black");
static u32 CaptainMeterTexture = glGetTexture("fe/capt_shoot_to_score_track");
static u32 CaptainMeterShadowTexture = glGetTexture("fe/capt_shoot_to_score_track_shadow");

u32 NumberTextures[4] = {
    nlStringLowerHash("fe/controller_1_indicator"),
    nlStringLowerHash("fe/controller_2_indicator"),
    nlStringLowerHash("fe/controller_3_indicator"),
    nlStringLowerHash("fe/controller_4_indicator"),
};

ShootToScoreMeter ShootToScoreMeter::instance;

static nlColour sWhiteBarColour = { 255, 255, 255, 255 };
static nlColour sHyperColour = { 181, 240, 255, 255 };
static nlColour sGreenRegionColour = { 5, 150, 5, 255 };
static nlColour sYellowRegionColour = { 200, 200, 10, 255 };
static s32 sfNumBarsInTrail = 20;
static float sfTrailIntensity = 0.6f;
static float sfTrailLengthScale = 0.25f;
static u8 sbMakeSTSMeterOrthographic = 1;

static inline void InterpolateColours(const nlColour& colour0, const nlColour& colour1, float alpha, nlColour& result)
{
    float oneMinusAlpha = 1.0f - alpha;
    result.c[0] = (u8)(s32)(oneMinusAlpha * (float)colour0.c[0] + alpha * (float)colour1.c[0]);
    result.c[1] = (u8)(s32)(oneMinusAlpha * (float)colour0.c[1] + alpha * (float)colour1.c[1]);
    result.c[2] = (u8)(s32)(oneMinusAlpha * (float)colour0.c[2] + alpha * (float)colour1.c[2]);
    result.c[3] = (u8)(s32)(oneMinusAlpha * (float)colour0.c[3] + alpha * (float)colour1.c[3]);
}

static inline void NDCToScreenCoordinates(const nlVector3& v3NormalizedScreenPos, nlVector3& v3ScreenPos)
{
    // PORT: half the frame, which is no longer 640 wide.
    v3ScreenPos.x = 0.5f * glGetOrthographicWidth() * (v3NormalizedScreenPos.x + 1.0f);
    v3ScreenPos.y = 0.5f * glGetOrthographicHeight() * (v3NormalizedScreenPos.y + 1.0f);
    v3ScreenPos.z = -0.1f;
}

static inline float clamp_ge(float x, float limit)
{
    if (x >= limit)
        return x;
    else
        return limit;
}

static inline float clamp_le(float x, float limit)
{
    if (x <= limit)
        return x;
    else
        return limit;
}

void ShootToScoreMeter::DrawIndicatorBar(float angle, const nlColour& colour, const nlMatrix4& meterMatrix, float scale)
{
    glSetCurrentTexture(WhiteTexture, GLTT_Diffuse);
    glSetTextureState(GLTS_DiffuseWrap, 0);
    glSetCurrentTextureState(glHandleizeTextureState());
    cCameraManager::GetDistanceFromCameraToObject(m_v3MeterPosition);

    glQuad3 barQuad;
    float zDepth;
    float angleRadians;
    float scaledMeterWidth = MeterWidth * scale;
    float scaledWhiteBarWidth = 0.039f * scale;
    float scaledWhiteBarHeight = 0.0035f * scale;
    angleRadians = (3.1415927f * angle) / 180.0f;

    nlMatrix4 barMatrix;
    nlMakeRotationMatrixZ(barMatrix, angleRadians);

    float radius = 0.198f * scaledMeterWidth;
    if (sbMakeSTSMeterOrthographic)
    {
        zDepth = -100.0f;
    }
    else
    {
        zDepth = 0.0f;
    }

    s32 angleIndex = (s32)(10430.378f * angleRadians);
    float sine = nlSin((u16)angleIndex);
    float cosine = nlSin((u16)((u16)angleIndex + 0x4000));

    barMatrix.e2[3][0] = radius * cosine;
    barMatrix.e2[3][1] = radius * sine;
    barMatrix.e2[3][2] = zDepth;
    barMatrix.e2[3][3] = 1.0f;
    nlMultMatrices(barMatrix, barMatrix, meterMatrix);

    barQuad.SetupRotatedRectangle(scaledWhiteBarWidth, scaledWhiteBarHeight, barMatrix, false, false);
    barQuad.SetColour(colour);

    eGLView view = GLV_UnsortedPerspective;
    if (sbMakeSTSMeterOrthographic)
    {
        view = GLV_UnsortedOrtho;
    }

    glAttachQuad3(view, 1, &barQuad, true);
}

inline void ShootToScoreMeter::DrawTrailIndicatorBar(int index, float angleDifference, const nlColour& colour, const nlMatrix4& meterMatrix, float scale)
{
    float angle = sfTrailLengthScale * ((float)index * angleDifference) + m_fWhiteBarAngle;
    DrawIndicatorBar(angle, colour, meterMatrix, scale);
}

float ShootToScoreMeter::MeterWidth = 0.4f;

/**
 * Offset/Address/Size: 0x19C8 | 0x80161C28 | size: 0xB0
 */
void ShootToScoreMeter::TurnOnMeter(ShootToScoreMeter::STSMeterType type, float yellowWidth)
{
    m_bMeterVisible = true;
    m_MeterType = type;
    m_fWhiteBarAngle = 0.0f;

    float scaledYellowWidth = 180.0f * (2.0f * yellowWidth);

    m_fWhiteBarPreviousAngle = m_fWhiteBarAngle;
    m_fSavedWhiteBarAngle = m_fWhiteBarAngle;
    mfRumbleAmount = 0.0f;
    m_fGreenBarAngle = 180.0f * g_pGame->m_pGameTweaks->fShootToScorePerfectFirstButtonTime;
    m_fSavedGreenBarAngle = 180.0f * g_pGame->m_pGameTweaks->fShootToScorePerfectFirstButtonTime;
    m_fGreenRegionWidth = 180.0f * (2.0f * g_pGame->m_pGameTweaks->fShootToScorePerfectDistanceTimeMin);
    m_fSavedGreenRegionWidth = 180.0f * (2.0f * g_pGame->m_pGameTweaks->fShootToScorePerfectDistanceTimeMin);
    m_fYellowRegionWidth = scaledYellowWidth;
    m_fSavedYellowRegionWidth = scaledYellowWidth;
    m_fWhiteBarPreviousAngle = m_fWhiteBarAngle;
    mbShowSavedWhiteBar = 0;
    meHyper = STS_NO_HYPER;
}

/**
 * Offset/Address/Size: 0x1990 | 0x80161BF0 | size: 0x38
 */
void ShootToScoreMeter::RumbleMeter(float rumbleIntensity, float xOffset, float zOffset)
{
    mfRumbleAmount = 0.175f * rumbleIntensity;
    m_v3MeterPosition.x = xOffset * mfRumbleAmount + m_v3OriginalMeterPosition.x;
    m_v3MeterPosition.y = m_v3OriginalMeterPosition.y;
    m_v3MeterPosition.z = zOffset * mfRumbleAmount + m_v3OriginalMeterPosition.z;
}

/**
 * Offset/Address/Size: 0x984 | 0x80160BE4 | size: 0x100C
 */
void ShootToScoreMeter::DrawMeter()
{
    glSetRasterState((eGLState)6, 0);
    glSetDefaultState(true);
    glSetRasterState((eGLState)1, 0);
    glSetRasterState((eGLState)0, 0);
    glSetRasterState((eGLState)5, 1);
    glSetRasterState((eGLState)6, 0);
    glSetCurrentRasterState(glHandleizeRasterState());

    if (m_MeterType == REGULAR_SHOOT_TO_SCORE_PHASE1)
    {
        glSetCurrentTexture(MeterTexture, GLTT_Diffuse);
    }
    else
    {
        glSetCurrentTexture(CaptainMeterTexture, GLTT_Diffuse);
    }

    glSetTextureState(GLTS_DiffuseWrap, 0);
    glSetCurrentTextureState(glHandleizeTextureState());

    nlMatrix4 matrix;
    float scale;
    float scaledMeterWidth;
    glQuad3 quad;

    matrix.SetIdentity();
    nlMakeRotationMatrixZ(matrix, 1.5707964f);

    if (sbMakeSTSMeterOrthographic)
    {
        static nlVector3 screenPosition;

        matrix.SetIdentity();
        glViewProjectPoint(GLV_Unshadowed, m_v3MeterPosition, screenPosition);

        NDCToScreenCoordinates(screenPosition, screenPosition);
        screenPosition.y += -20.0f;

        scale = 640.0f;
        scaledMeterWidth = MeterWidth * scale;

        // PORT: 92 and 548 are 320 -/+ 228; 84 and 396 are 240 -/+ 156.
        {
            const float cx = 0.5f * glGetOrthographicWidth();
            const float cy = 0.5f * glGetOrthographicHeight();
            screenPosition.x = clamp_le(clamp_ge(screenPosition.x, cx - 228.0f), cx + 228.0f);
            screenPosition.y = clamp_le(clamp_ge(screenPosition.y, cy - 156.0f), cy + 156.0f);
        }

        matrix.e2[3][0] = screenPosition.x;
        matrix.e2[3][1] = screenPosition.y;
        matrix.e2[3][2] = screenPosition.z;
        matrix.e2[3][3] = 1.0f;
    }
    else
    {
        scale = cCameraManager::GetDistanceFromCameraToObject(m_v3MeterPosition);

        matrix.e2[3][0] = m_v3MeterPosition.x;
        matrix.e2[3][1] = m_v3MeterPosition.y;
        matrix.e2[3][2] = m_v3MeterPosition.z;
        matrix.e2[3][3] = 1.0f;

        scaledMeterWidth = MeterWidth * scale;
    }

    quad.SetupRotatedRectangle(
        scaledMeterWidth,
        scaledMeterWidth,
        matrix,
        m_MeterType == REGULAR_SHOOT_TO_SCORE_PHASE1,
        false);

    eGLView view = GLV_UnsortedPerspective;
    if (sbMakeSTSMeterOrthographic)
    {
        view = GLV_UnsortedOrtho;
    }
    glAttachQuad3(view, 1, &quad, true);

    nlColour green = sGreenRegionColour;
    green.c[3] = (u8)(255.0f * m_fGreenAndYellonRegionIntensity);
    nlColour yellow = sYellowRegionColour;
    nlColour black = { 0, 0, 0, 255 };
    yellow.c[3] = (u8)(255.0f * m_fGreenAndYellonRegionIntensity);
    nlColour hyper = sHyperColour;

    if (meHyper == STS_GOT_HYPER)
    {
        DrawIndicatorBar(m_fWhiteBarAngle, black, matrix, scale);
        DrawIndicatorBar(m_fSavedWhiteBarAngle, black, matrix, scale);
    }
    else
    {
        DrawColouredRegion(
            m_fGreenBarAngle - (0.5 * m_fYellowRegionWidth),
            m_fGreenBarAngle - (0.5f * m_fGreenRegionWidth),
            yellow,
            yellow,
            matrix,
            scale);
        DrawColouredRegion(
            m_fGreenBarAngle + (0.5 * m_fYellowRegionWidth),
            m_fGreenBarAngle + (0.5f * m_fGreenRegionWidth),
            yellow,
            yellow,
            matrix,
            scale);
        DrawColouredRegion(
            m_fGreenBarAngle - (0.5f * m_fGreenRegionWidth),
            m_fGreenBarAngle + (0.5f * m_fGreenRegionWidth),
            green,
            green,
            matrix,
            scale);

        DrawIndicatorBar(m_fWhiteBarAngle, sWhiteBarColour, matrix, scale);

        if (mbShowSavedWhiteBar)
        {
            DrawColouredRegion(
                m_fSavedGreenBarAngle - (0.5 * m_fSavedYellowRegionWidth),
                m_fSavedGreenBarAngle - (0.5f * m_fSavedGreenRegionWidth),
                yellow,
                yellow,
                matrix,
                scale);
            DrawColouredRegion(
                m_fSavedGreenBarAngle + (0.5 * m_fSavedYellowRegionWidth),
                m_fSavedGreenBarAngle + (0.5f * m_fSavedGreenRegionWidth),
                yellow,
                yellow,
                matrix,
                scale);

            if (meHyper == STS_POSSIBLE_HYPER)
            {
                DrawColouredRegion(
                    m_fSavedGreenBarAngle - (0.5f * m_fSavedGreenRegionWidth),
                    m_fSavedGreenBarAngle + (0.5f * m_fSavedGreenRegionWidth),
                    hyper,
                    hyper,
                    matrix,
                    scale);
                DrawIndicatorBar(m_fSavedWhiteBarAngle, black, matrix, scale);
            }
            else
            {
                DrawColouredRegion(
                    m_fSavedGreenBarAngle - (0.5f * m_fSavedGreenRegionWidth),
                    m_fSavedGreenBarAngle + (0.5f * m_fSavedGreenRegionWidth),
                    green,
                    green,
                    matrix,
                    scale);
                DrawIndicatorBar(m_fSavedWhiteBarAngle, sWhiteBarColour, matrix, scale);
            }
        }

        float diffCurrentPrev = m_fWhiteBarPreviousAngle - m_fWhiteBarAngle;

        for (int i = 0; i < sfNumBarsInTrail; i++)
        {
            nlColour fadedColour = sWhiteBarColour;
            fadedColour.c[3] = (u8)(s32)(255.0f * (sfTrailIntensity * (1.0f - ((float)i / (float)sfNumBarsInTrail))));

            DrawTrailIndicatorBar(i, diffCurrentPrev, fadedColour, matrix, scale);
        }
    }

    glSetDefaultState(false);
}

/**
 * Offset/Address/Size: 0x3DC | 0x8016063C | size: 0x5A8
 */
void ShootToScoreMeter::DrawColouredRegion(float startAngle, float endAngle, const nlColour& startColour, const nlColour& endColour, nlMatrix4 meterMatrix, float scale)
{
    glSetCurrentTexture(WhiteTexture, GLTT_Diffuse);
    glSetTextureState(GLTS_DiffuseWrap, 0);
    glSetCurrentTextureState(glHandleizeTextureState());

    float scaledWhiteBarWidth;
    float radius;
    nlVector3 vertexPosition;
    float widthAngle;
    int i;
    glQuad3 barQuad;
    float startFraction;
    float endFraction;
    float innerRadius;
    float outerRadius;

    scaledWhiteBarWidth = 0.039f * scale;
    widthAngle = endAngle - startAngle;
    radius = 0.198f * (MeterWidth * scale);

    float step = 0.125f;
    float sinScale = 10430.378f;
    float pi = 3.1415927f;
    float deg = 180.0f;

    for (i = 0; i < 8; i++)
    {
        startFraction = (float)i * step;
        endFraction = (float)(i + 1) * step;
        innerRadius = radius - scaledWhiteBarWidth / 2.0f;
        outerRadius = radius + scaledWhiteBarWidth / 2.0f;

        float segmentStartAngle = startFraction * widthAngle + startAngle;
        s32 segmentStartAngleIndex = (s32)(sinScale * (pi * segmentStartAngle / deg));
        float segmentStartCosine = nlSin((u16)((u16)segmentStartAngleIndex + 0x4000));
        float segmentStartSine = nlSin((u16)segmentStartAngleIndex);

        float segmentEndAngle = endFraction * widthAngle + startAngle;
        s32 segmentEndAngleIndex = (s32)(sinScale * (pi * segmentEndAngle / deg));
        float segmentEndCosine = nlSin((u16)((u16)segmentEndAngleIndex + 0x4000));
        float segmentEndSine = nlSin((u16)segmentEndAngleIndex);

        float zDepth;
        if (sbMakeSTSMeterOrthographic)
        {
            zDepth = -100.0f;
        }
        else
        {
            zDepth = 0.0f;
        }

        vertexPosition.x = innerRadius * segmentStartCosine;
        vertexPosition.y = innerRadius * segmentStartSine;
        vertexPosition.z = zDepth;
        nlMultPosVectorMatrix(vertexPosition, vertexPosition, meterMatrix);
        barQuad.m_pos[0] = vertexPosition;

        vertexPosition.x = outerRadius * segmentStartCosine;
        vertexPosition.y = outerRadius * segmentStartSine;
        vertexPosition.z = zDepth;
        nlMultPosVectorMatrix(vertexPosition, vertexPosition, meterMatrix);
        barQuad.m_pos[1] = vertexPosition;

        vertexPosition.x = outerRadius * segmentEndCosine;
        vertexPosition.y = outerRadius * segmentEndSine;
        vertexPosition.z = zDepth;
        nlMultPosVectorMatrix(vertexPosition, vertexPosition, meterMatrix);
        barQuad.m_pos[2] = vertexPosition;

        vertexPosition.x = innerRadius * segmentEndCosine;
        vertexPosition.y = innerRadius * segmentEndSine;
        vertexPosition.z = zDepth;
        nlMultPosVectorMatrix(vertexPosition, vertexPosition, meterMatrix);
        barQuad.m_pos[3] = vertexPosition;

        InterpolateColours(startColour, endColour, startFraction, barQuad.m_colour[0]);
        barQuad.m_colour[1] = barQuad.m_colour[0];

        InterpolateColours(startColour, endColour, endFraction, barQuad.m_colour[2]);
        barQuad.m_colour[3] = barQuad.m_colour[2];

        eGLView view = GLV_UnsortedPerspective;
        if (sbMakeSTSMeterOrthographic)
        {
            view = GLV_UnsortedOrtho;
        }

        glAttachQuad3(view, 1, &barQuad, true);
    }
}

/**
 * Offset/Address/Size: 0x394 | 0x801605F4 | size: 0x48
 */
void ShootToScoreMeter::UpdateAndRender(float fDeltaT)
{
    if (m_bMeterVisible == 0)
    {
        return;
    }

    if (m_MeterType == REGULAR_SHOOT_TO_SCORE_PHASE1 || m_MeterType == REGULAR_SHOOT_TO_SCORE_PHASE2)
    {
        DrawMeter();
    }
    else
    {
        DrawCaptainMeter();
    }
}

/**
 * Offset/Address/Size: 0x37C | 0x801605DC | size: 0x18
 */
void ShootToScoreMeter::SetWhiteBarPosition(float position)
{
    m_fWhiteBarPreviousAngle = m_fWhiteBarAngle;
    m_fWhiteBarAngle = 180.0f * position;
}

/**
 * Offset/Address/Size: 0x36C | 0x801605CC | size: 0x10
 */
void ShootToScoreMeter::SetSavedWhiteBarPosition(float position)
{
    m_fSavedWhiteBarAngle = 180.0f * position;
}

/**
 * Offset/Address/Size: 0x35C | 0x801605BC | size: 0x10
 */
void ShootToScoreMeter::SetGreenBarPosition(float position)
{
    m_fGreenBarAngle = 180.0f * position;
}

/**
 * Offset/Address/Size: 0x34C | 0x801605AC | size: 0x10
 */
void ShootToScoreMeter::SetGreenRegionWidth(float width)
{
    m_fGreenRegionWidth = 180.0f * width;
}

/**
 * Offset/Address/Size: 0x0 | 0x80160260 | size: 0x34C
 */
void ShootToScoreMeter::DrawCaptainMeter()
{
    glSetDefaultState(false);
    glSetRasterState(GLS_Culling, 0);
    glSetRasterState(GLS_AlphaBlend, 1);
    glSetRasterState(GLS_AlphaTest, 1);
    glSetRasterState(GLS_AlphaTestRef, 0);
    glSetCurrentRasterState(glHandleizeRasterState());

    glQuad3 quad;
    nlMatrix4 matrix;

    matrix.SetIdentity();
    nlMakeRotationMatrixX(matrix, 1.2217306f);

    for (int i = 0; i < 12; i++)
    {
        glSetCurrentTexture(CaptainMeterShadowTexture, GLTT_Diffuse);
        glSetTextureState(GLTS_DiffuseWrap, 0);
        glSetCurrentTextureState(glHandleizeTextureState());

        float scale = (float)(i + 1) / 12.0f;
        float yOffset = 30.0f * scale;
        float yPosition = 365.0f + (yOffset);
        matrix.e2[3][0] = 0.5f * glGetOrthographicWidth();   // PORT: not 320
        matrix.e2[3][1] = yPosition;
        matrix.e2[3][2] = -500.0f;
        matrix.e2[3][3] = 1.0f;

        nlColour colour = { 255, 255, 255, 255 };
        quad.SetColour(colour);
        quad.SetupRotatedRectangle(400.0f, 400.0f, matrix, false, false);
        glAttachQuad3(GLV_FrontEnd, 1, &quad, true);
    }

    glSetTextureState(GLTS_DiffuseWrap, 0);
    glSetCurrentTextureState(glHandleizeTextureState());

    matrix.e2[3][0] = 0.5f * glGetOrthographicWidth();   // PORT: not 320
    matrix.e2[3][1] = 365.0f;
    matrix.e2[3][2] = -500.0f;
    matrix.e2[3][3] = 1.0f;

    nlColour colour = { 255, 255, 255, 255 };
    quad.SetColour(colour);
    quad.SetupRotatedRectangle(400.0f, 400.0f, matrix, false, false);
    glAttachQuad3(GLV_FrontEnd, 1, &quad, true);

    float whiteBarAngle;
    float whiteBarHeightScaled = 400.0f * (0.039f / MeterWidth);
    float whiteBarWidthScaled = 400.0f * (0.0035f / MeterWidth);
    whiteBarAngle = (3.1415927f * m_fWhiteBarAngle) / 180.0f;

    nlMatrix4 whiteBarMatrix;
    nlMakeRotationMatrixZ(whiteBarMatrix, whiteBarAngle);

    float sine = nlSin((u16)(s32)(10430.378f * whiteBarAngle));
    float cosine = nlSin((u16)((u16)(s32)(10430.378f * whiteBarAngle) + 0x4000));

    whiteBarMatrix.SetRow4_(3, 79.2f * cosine, 79.2f * sine, 0.f, 1.0f);
    nlMultMatrices(whiteBarMatrix, whiteBarMatrix, matrix);

    glSetRasterState(GLS_AlphaBlend, 0);
    glSetCurrentRasterState(glHandleizeRasterState());
    glSetCurrentTexture(WhiteTexture, GLTT_Diffuse);
    glSetTextureState(GLTS_DiffuseWrap, 0);
    glSetCurrentTextureState(glHandleizeTextureState());

    glQuad3 whiteBarQuad;
    whiteBarQuad.SetupRotatedRectangle(whiteBarHeightScaled, whiteBarWidthScaled, whiteBarMatrix, false, false);
    glAttachQuad3(GLV_FrontEnd, 1, &whiteBarQuad, true);
    glSetDefaultState(false);
}
