#include "Game/Debug/FrameCounter.h"
#include "Game/Debug/TimeRegions.h"
#include "Game/GL/gluMeshWriter.h"
#include "../Sys/tweak.h"

#include "NL/gl/glMatrix.h"
#include "NL/gl/glFont.h"
#include "NL/gl/glState.h"
#include "NL/gl/glView.h"
#include "NL/nlDebugFile.h"
#include "NL/nlMemory.h"
#include "NL/nlPrint.h"
#include "NL/nlString.h"
#include "NL/nlTicker.h"

static u32 UnlitProgram = glGetProgram("3d unlit");
static u32 LitProgram = glGetProgram("3d pointlit");
static u32 LightTexture = glGetTexture("global/lightramp");
static u32 BlackTexture = glGetTexture("global/black");
static u32 WhiteTexture = glGetTexture("global/white");

static const int gl_stream_stride[15] = {
    12, 3, 4, 4, 4, 4, 4, 4, 4, 12, 12, 12, 1, 16, 16
};

int FrameCounter::NUM_FRAMES_TO_AVERAGE_OVER = 30;

static float sfHappiness = 1.0f;
static float sfEyeHeight = 0.2f;
static float sfEyeSeparation = 0.4f;
static float sfSmileyRadius = 15.0f;
static float sfSmileRadius = 0.6f;
static float sfEyeRadius = 0.15f;
static float sfSmileAngle = 90.0f;
static int siHappinessLookback = 60;
static nlColour sMadColour = { 140, 48, 0, 255 };
static nlColour sMediumColour = { 192, 192, 0, 255 };
static nlColour sHappyColour = { 0, 192, 0, 255 };
static nlColour FrameBackgroundColour = { 0, 0, 0, 128 };
static nlColour HighlightedBackgroundColour = { 255, 255, 255, 128 };
static const char* FrameCounterOutputString = "%2.2fFPS %2.2fms [%2.2fms %s, %2.2fms %s]";

nlListContainer<TimeRegion*> TimeRegion::sTimeRegionList;

/**
 * Offset/Address/Size: 0x1374 | 0x801FDF10 | size: 0x7C
 */
FrameCounter::FrameCounter(const char* first, const char* second)
{
    m_FirstName = first;
    m_SecondName = second;

    m_Counter = 0;
    m_NumSlowFrames = 0;
    m_NextHistoryPos = 0;
    m_ContinuousFrameHistoryIndex = 0;

    m_CurrTimer[0] = 0.0f;
    m_CurrTimer[1] = 0.0f;

    m_CurrTimerNum = -1;

    memset(m_FrameHistory, 0, sizeof(m_FrameHistory));
    memset(m_ContinuousFrameHistory, 0, sizeof(m_ContinuousFrameHistory));
}

void FrameCounter::ResetFrameTimers()
{
    m_CurrTimer[0] = 0.0f;
    m_CurrTimer[1] = 0.0f;
    m_CurrTimerNum = -1;
}

unsigned int FrameCounter::FinishCurrTimer()
{
    unsigned int currTick = nlGetTicker();
    if (m_CurrTimerNum != -1)
    {
        m_CurrTimer[m_CurrTimerNum] += nlGetTickerDifference(m_StartTick, currTick);
    }
    return currTick;
}

/**
 * Offset/Address/Size: 0x12F8 | 0x801FDE94 | size: 0x7C
 */
void FrameCounter::StartTimer(int timerNum)
{
    u32 currentTick = nlGetTicker();

    if (m_CurrTimerNum != -1)
    {
        m_CurrTimer[m_CurrTimerNum] += nlGetTickerDifference(m_StartTick, currentTick);
    }

    m_StartTick = currentTick;
    m_CurrTimerNum = timerNum;
}

/**
 * Offset/Address/Size: 0x1078 | 0x801FDC14 | size: 0x280
 */
void FrameCounter::FinishTiming()
{
    u32 currentTick = nlGetTicker();
    if (m_CurrTimerNum != -1)
    {
        m_CurrTimer[m_CurrTimerNum] += nlGetTickerDifference(m_StartTick, currentTick);
    }

    m_Counter++;

    float totalFrameTime = 0.0f;

    totalFrameTime += m_CurrTimer[0];
    m_CurrFrame[0] += m_CurrTimer[0];
    m_ContinuousFrameHistory[0][m_ContinuousFrameHistoryIndex] = m_CurrTimer[0];

    totalFrameTime += m_CurrTimer[1];
    m_CurrFrame[1] += m_CurrTimer[1];
    m_ContinuousFrameHistory[1][m_ContinuousFrameHistoryIndex] = m_CurrTimer[1];

    if (m_Counter >= (u32)NUM_FRAMES_TO_AVERAGE_OVER)
    {
        m_LastFrame[0] = m_CurrFrame[0] / (float)m_Counter;
        m_CurrFrame[0] = 0.0f;
        m_LastFrame[1] = m_CurrFrame[1] / (float)m_Counter;
        m_CurrFrame[1] = 0.0f;

        m_SlowFrameRatio = (float)m_NumSlowFrames / (float)m_Counter;
        m_NumSlowFrames = 0;
        m_Counter = 0;
    }

    m_FrameHistory[m_NextHistoryPos] = totalFrameTime;
    if (totalFrameTime > 17.0f)
    {
        m_NumSlowFrames++;
    }

    m_NextHistoryPos = ((u32)m_NextHistoryPos + 1) % 640;
    m_ContinuousFrameHistoryIndex = (m_ContinuousFrameHistoryIndex + 1) % 200;

    nlListIterator<TimeRegion*> iterator = TimeRegion::sTimeRegionList.Begin();
    while (iterator.IsValid())
    {
        TimeRegion* region = iterator.Current();
        if (region->m_pConditionFunc())
        {
            region->m_unk14++;
            if (totalFrameTime > 17.0f)
            {
                region->m_unk10++;
            }
            region->m_fThreshold += totalFrameTime;
        }

        iterator.Next();
    }

    m_CurrTimer[0] = 0.0f;
    m_CurrTimer[1] = 0.0f;
    m_CurrTimerNum = -1;
}

void FrameCounter::DisplayFrameRate()
{
    char str[64];
    float totalTime = m_LastFrame[0] + m_LastFrame[1];

    nlSNPrintf(str, sizeof(str), FrameCounterOutputString, totalTime == 0.0f ? 0.0f : 1000.0f / totalTime, totalTime, m_LastFrame[0], m_FirstName, m_LastFrame[1], m_SecondName);

    DrawTextRectangle(GLV_Debug, 0.0f, 0.0f, (float)nlStrLen(str), 1.0f, 0.0f, FrameBackgroundColour, true);
    glFontBegin(false);
    glFontPrint(GLV_Debug, 0, 0, FrameBackgroundColour, str);
    glFontEnd();
}

void FrameCounter::DisplaySlowFrameRatio()
{
    char slowFrames[64];
    char fastFrames[64];

    nlSNPrintf(slowFrames, sizeof(slowFrames), "Percentage of frames slower than 17ms:%2.2f%%", 100.0f * m_SlowFrameRatio);
    nlSNPrintf(fastFrames, sizeof(fastFrames), "Percentage of frames that are faster than 17ms: %2.2f%%\n", 100.0 * (1.0f - m_SlowFrameRatio));
    slowFrames[sizeof(slowFrames) - 1] = '\0';
    fastFrames[sizeof(fastFrames) - 1] = '\0';

    GLMeshWriter mesh;
    eGLStream streams[2];
    streams[0] = GLStream_Position;
    streams[1] = GLStream_Colour;

    glSetDefaultState(false);
    glSetCurrentMatrix(glGetIdentityMatrix());
    glSetCurrentProgram(glGetProgram("2d unlit"));
    if (mesh.Begin(4, GLP_TriStrip, 2, streams, false))
    {
        float width = 640.0f * m_SlowFrameRatio;
        mesh.Colour(FrameBackgroundColour);
        mesh.Position(0.0f, 0.0f, 0.0f);
        mesh.Colour(FrameBackgroundColour);
        mesh.Position(0.0f, 1.0f, 0.0f);
        mesh.Colour(HighlightedBackgroundColour);
        mesh.Position(width, 0.0f, 0.0f);
        mesh.Colour(HighlightedBackgroundColour);
        mesh.Position(width, 1.0f, 0.0f);

        if (mesh.End())
        {
            glViewAttachModel(GLV_Debug, mesh.GetModel());
        }
    }

    glFontBegin(false);
    glFontPrint(GLV_Debug, 0, 1, FrameBackgroundColour, slowFrames);
    glFontPrint(GLV_Debug, 0, 2, HighlightedBackgroundColour, fastFrames);
    glFontEnd();
}

/**
 * Offset/Address/Size: 0xED8 | 0x801FDA74 | size: 0x1A0
 */
void FrameCounter::WriteFrameRateStatsToFile(const char* fileName)
{
    char buf[128];
    void* file = nlOpenFileDebug("FrameRateStats.txt", false, true);
    nlWriteLineDebug(file, fileName, false);
    nlWriteLineDebug(file, "============================================\n", false);
    nlWriteLineDebug(file, "First number is percentage of frames that take less than 17ms\n", false);
    nlWriteLineDebug(file, "Second number is average time per frame\n", false);
    nlWriteLineDebug(file, "Third number of frames counted\n\n", false);

    nlListIterator<TimeRegion*> iterator = TimeRegion::sTimeRegionList.Begin();
    while (iterator.IsValid())
    {
        TimeRegion* region = iterator.Current();
        float ratio = (float)region->m_unk10 / (float)region->m_unk14;
        float avgTime = region->m_fThreshold / (float)region->m_unk14;
        float pct = 100.0f * (1.0f - ratio);

        nlSNPrintf(buf, sizeof(buf), "%4.0f%% %4.0f %5d %s\n", pct, avgTime, region->m_unk14, region->m_pName);
        nlWriteLineDebug(file, buf, false);

        iterator.Next();
    }

    nlCloseFileDebug(file);
}

void FrameCounter::DisplayContinuousFrameRate()
{
    float avgTime[2] = { 0.0f, 0.0f };
    float totalTime;

    for (int index = 0; index < 200; index++)
    {
        avgTime[0] += m_ContinuousFrameHistory[0][index];
        avgTime[1] += m_ContinuousFrameHistory[1][index];
    }

    avgTime[0] /= 200.0f;
    avgTime[1] /= 200.0f;
    totalTime = avgTime[0] + avgTime[1];

    glFontBegin(false);
    glFontPrintf(GLV_Debug, 0, 0, FrameCounterOutputString, totalTime == 0.0f ? 0.0f : 1000.0f / totalTime, totalTime, avgTime[0], m_FirstName, avgTime[1], m_SecondName);
    glFontEnd();
}

void FrameCounter::DisplayFrameTicker()
{
    GLMeshWriter m0;
    GLMeshWriter m1;
    const eGLStream streams[2] = { GLStream_Position, GLStream_Colour };

    glSetDefaultState(false);
    glSetCurrentProgram(glGetProgram("2d unlit"));

    if (m0.Begin(8, GLP_LineList, 2, streams, false))
    {
        m0.Colour(255, 255, 0, 255);
        m0.Position(0.0f, 32.0f, 0.0f);
        m0.Colour(255, 255, 0, 255);
        m0.Position(640.0f, 32.0f, 0.0f);
        m0.Colour(64, 64, 64, 255);
        m0.Position(0.0f, 1.25f * 8.333f + 32.0f, 0.0f);
        m0.Colour(64, 64, 64, 255);
        m0.Position(640.0f, 1.25f * 8.333f + 32.0f, 0.0f);
        m0.Colour(255, 0, 0, 255);
        m0.Position(0.0f, 1.25f * 16.666f + 32.0f, 0.0f);
        m0.Colour(255, 0, 0, 255);
        m0.Position(640.0f, 1.25f * 16.666f + 32.0f, 0.0f);
        m0.Colour(0, 255, 0, 255);
        m0.Position(0.0f, 1.25f * 33.333f + 32.0f, 0.0f);
        m0.Colour(0, 255, 0, 255);
        m0.Position(640.0f, 1.25f * 33.333f + 32.0f, 0.0f);

        if (m0.End())
        {
            glViewAttachModel(GLV_Debug, m0.GetModel());
        }
    }

    if (m1.Begin(640, GLP_LineStrip, 2, streams, false))
    {
        for (unsigned int i = 0; i < 640; i++)
        {
            unsigned int historyLoc = (m_NextHistoryPos + i) % 640;
            m1.Colour(255, 255, 255, 255);
            m1.Position((float)i, 1.25f * m_FrameHistory[historyLoc] + 32.0f, 0.0f);
        }

        if (m1.End())
        {
            glViewAttachModel(GLV_Debug, m1.GetModel());
        }
    }
}

/**
 * Offset/Address/Size: 0xC5C | 0x801FD7F8 | size: 0x27C
 */
static void DrawCircle(nlVector3 p0, float fRadius, float fScaleX, nlColour colour)
{
    GLMeshWriter mesh;

    glSetDefaultState(true);
    glSetCurrentMatrix(glGetIdentityMatrix());
    glSetCurrentTexture(WhiteTexture, GLTT_Diffuse);
    glSetCurrentProgram(UnlitProgram);

    const eGLStream stream_decl[3] = { GLStream_Position, GLStream_Colour, GLStream_Diffuse };

    if (mesh.Begin(31, GLP_TriFan, 3, stream_decl, false))
    {
        nlVector3 v3point;

        v3point.z = p0.z;
        v3point.x = p0.x;
        v3point.y = p0.y;

        float fRadians = 0.0f;

        mesh.Colour(colour);
        mesh.Texcoord(0.0f, 0.0f);
        mesh.Vertex(v3point);

        const float angleScale = 10430.378f;
        int i = 0;
        const float uvZero = 0.0f;
        const float angleStep = 0.21666157f;

        while (i < 30)
        {
            nlSinCos(&v3point.x, &v3point.y, (unsigned short)(int)(angleScale * fRadians));
            v3point.x = p0.x + fScaleX * (v3point.x * fRadius);
            v3point.y = p0.y + v3point.y * fRadius;

            mesh.Colour(colour);
            mesh.Texcoord(uvZero, uvZero);
            mesh.Vertex(v3point);

            i++;
            fRadians += angleStep;
        }

        if (!mesh.End())
        {
            return;
        }

        glViewAttachModel(GLV_Debug, 2, mesh.GetModel());
    }
}

/**
 * Offset/Address/Size: 0x904 | 0x801FD4A0 | size: 0x358
 */
static void DrawSmile(nlVector3 p0, float fRadius, float fScaleX, nlColour colour, float fLineThickness)
{
    GLMeshWriter mesh;
    float degrees = sfSmileAngle;

    glSetDefaultState(true);
    glSetCurrentMatrix(glGetIdentityMatrix());
    glSetCurrentTexture(WhiteTexture, GLTT_Diffuse);
    glSetCurrentProgram(UnlitProgram);

    const eGLStream stream_decl[3] = { GLStream_Position, GLStream_Colour, GLStream_Diffuse };
    float yScale = (2.0f * sfHappiness) + -1.0f;

    if (mesh.Begin(20, GLP_TriStrip, 3, stream_decl, false))
    {
        nlVector3 v3point;

        float fRadians = -((3.1415927f * (0.5f * degrees)) / 180.0f);
        v3point.z = p0.z;
        nlSinCos(&v3point.x, &v3point.y, (u16)(int)(10430.378f * fRadians));

        float fXFromAngle = v3point.x * fRadius;
        v3point.x = (fScaleX * fXFromAngle) + p0.x;
        v3point.y = (v3point.y * fRadius) + p0.y;
        float fYFromAngle = v3point.y;
        float fYTop = p0.y + fRadius;
        float middleY = 0.5f * (fYFromAngle + fYTop);

        int i = 0;
        while (i < 10)
        {
            nlSinCos(&v3point.x, &v3point.y, (u16)(int)(10430.378f * fRadians));

            v3point.x = p0.x + fScaleX * (v3point.x * fRadius);
            v3point.y = p0.y + v3point.y * fRadius;

            v3point.y = v3point.y - middleY;
            v3point.y = v3point.y * yScale;
            v3point.y = v3point.y + middleY;

            mesh.Colour(colour);
            mesh.Texcoord(0.0f, 0.0f);
            mesh.Vertex(v3point);

            v3point.y += fLineThickness;

            mesh.Colour(colour);
            mesh.Texcoord(0.0f, 0.0f);
            mesh.Vertex(v3point);

            i++;
            fRadians += ((3.1415927f * degrees) / 180.0f) / 9.0f;
        }

        if (mesh.End() == 0)
        {
            return;
        }

        glViewAttachModel(GLV_Debug, 2, mesh.GetModel());
    }
}

/**
 * Offset/Address/Size: 0x588 | 0x801FD124 | size: 0x37C
 */
static void DrawBrow(const nlVector3& leftEyeCentre, const nlVector3& rightEyeCentre, float distanceAboveEye, float width, float height)
{
    GLMeshWriter m0;
    const eGLStream streams[2] = { GLStream_Position, GLStream_Colour };

    glSetDefaultState(false);
    glSetCurrentProgram(glGetProgram("2d unlit"));

    float yScale = (2.0f * sfHappiness) + -1.0f;

    if (m0.Begin(4, GLP_LineList, 2, streams, false))
    {
        nlVector3 p1;
        nlVector3 p2;

        p1 = leftEyeCentre;
        p2 = leftEyeCentre;

        p1.x = p1.x - width;
        p2.x = p2.x + width;
        p1.y += -distanceAboveEye + (height * yScale);
        p2.y += -distanceAboveEye - (height * yScale);

        m0.Colour(0, 0, 0, 255);
        m0.Position(p1.x, p1.y, p1.z);
        m0.Colour(0, 0, 0, 255);
        m0.Position(p2.x, p2.y, p2.z);

        p1 = rightEyeCentre;
        p2 = rightEyeCentre;

        p1.x = p1.x + width;
        p2.x = p2.x - width;
        p1.y += -distanceAboveEye + (height * yScale);
        p2.y += -distanceAboveEye - (height * yScale);

        m0.Colour(0, 0, 0, 255);
        m0.Position(p1.x, p1.y, p1.z);
        m0.Colour(0, 0, 0, 255);
        m0.Position(p2.x, p2.y, p2.z);

        if (m0.End())
        {
            glViewAttachModel(GLV_Debug, m0.GetModel());
        }
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x801FCB9C | size: 0x588
 */
void FrameCounter::DisplayFrameSmiler()
{
    float happiness = 0.0f;
    unsigned int i;
    for (i = 0; i < (unsigned int)siHappinessLookback; i++)
    {
        float fps = m_FrameHistory[(m_NextHistoryPos - i + 640) % 640] == 0.0f
                      ? 60.0f
                      : 1000.0f / m_FrameHistory[(m_NextHistoryPos - i + 640) % 640];
        happiness += (fps - 30.0f) / 30.0f;
    }
    happiness /= (float)siHappinessLookback;
    if (happiness > 1.0f)
        happiness = 1.0f;
    if (happiness < 0.0f)
        happiness = 0.0f;

    sfHappiness = happiness;

    float circleRadius = sfSmileyRadius;
    float smileRadius = sfSmileRadius * sfSmileyRadius;
    float eyeRadius = sfEyeRadius * sfSmileyRadius;

    nlVector3 circleCentre = { 0, 0, 0 };
    circleCentre.x = 2.0f * sfSmileyRadius;
    circleCentre.y = 480.0f - 3.0f * sfSmileyRadius;

    nlColour black = { 0, 0, 0, 255 };

    nlColour colour;
    if (happiness < 0.5f)
    {
        float alpha = 2.0f * happiness;
        nlColourSet(colour,
            (int)((float)sMediumColour.c[0] * alpha + (float)sMadColour.c[0] * (1.0f - 2.0f * happiness)),
            (int)((float)sMediumColour.c[1] * alpha + (float)sMadColour.c[1] * (1.0f - alpha)),
            (int)((float)sMediumColour.c[2] * alpha + (float)sMadColour.c[2] * (1.0f - alpha)),
            (int)((float)sMediumColour.c[3] * alpha + (float)sMadColour.c[3] * (1.0f - alpha)));
    }
    else
    {
        float alpha = 2.0f * (happiness - 0.5f);
        nlColourSet(colour,
            (int)((float)sHappyColour.c[0] * alpha + (float)sMediumColour.c[0] * (1.0f - 2.0f * (happiness - 0.5f))),
            (int)((float)sHappyColour.c[1] * alpha + (float)sMediumColour.c[1] * (1.0f - alpha)),
            (int)((float)sHappyColour.c[2] * alpha + (float)sMediumColour.c[2] * (1.0f - alpha)),
            (int)((float)sHappyColour.c[3] * alpha + (float)sMediumColour.c[3] * (1.0f - alpha)));
    }

    nlVector3 leftEyeCentre = { 0, 0, 0 };
    nlVector3 rightEyeCentre = { 0, 0, 0 };

    leftEyeCentre.x = -circleRadius * sfEyeSeparation;
    leftEyeCentre.y = -circleRadius * sfEyeHeight;

    rightEyeCentre.x = circleRadius * sfEyeSeparation;
    rightEyeCentre.y = -circleRadius * sfEyeHeight;

    nlVec3Add(leftEyeCentre, leftEyeCentre, circleCentre);
    nlVec3Add(rightEyeCentre, rightEyeCentre, circleCentre);

    DrawCircle(circleCentre, 3.0f + circleRadius, 1.2f, black);
    DrawCircle(circleCentre, circleRadius, 1.2f, colour);
    DrawCircle(leftEyeCentre, eyeRadius, 1.2f, black);
    DrawCircle(rightEyeCentre, eyeRadius, 1.2f, black);
    DrawSmile(circleCentre, smileRadius, 1.2f, black, 3.0f);
    DrawBrow(leftEyeCentre, rightEyeCentre, 3.0f * eyeRadius, 2.0f * eyeRadius, 1.5f * eyeRadius);
}
