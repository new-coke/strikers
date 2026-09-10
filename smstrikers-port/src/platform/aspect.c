// The port's display aspect ratio. See include/port/aspect.h for the model.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port/aspect.h"

// The shape the game was authored for. 640x480 of design space displayed as 4:3, which is why the
// width below is 480 * aspect and comes out at exactly 640 when the aspect is 4/3.
#define PORT_DESIGN_HEIGHT 480.0f
#define PORT_CONSOLE_ASPECT (4.0f / 3.0f)

// The range the port will present. Below 4:3 the shadow atlas stops fitting (its 4-across grid of
// 160-wide tiles needs a 640-wide frame), which is the hard end; 1.0 leaves room above it.
#define PORT_ASPECT_MIN 1.0f
#define PORT_ASPECT_MAX 3.6f

static float s_windowAspect;   // 0 until main() reports one
static float s_aspect;         // 0 until resolved
static int   s_resolved;
static int   s_follows;        // 1 when the aspect is the window's own
static unsigned int s_generation;

static float ClampAspect(float a)
{
    if (a < PORT_ASPECT_MIN)
        return PORT_ASPECT_MIN;
    if (a > PORT_ASPECT_MAX)
        return PORT_ASPECT_MAX;
    return a;
}

void PortSetWindowAspect(unsigned int width, unsigned int height)
{
    float a;
    float was;

    if (width == 0 || height == 0)
        return;

    s_windowAspect = (float)width / (float)height;

    // In `auto` the window *is* the aspect, so a window that changes shape changes it.
    if (!s_resolved || !s_follows)
        return;

    a = ClampAspect(s_windowAspect);
    if (a == s_aspect)
        return;

    was = s_aspect;
    s_aspect = a;
    s_generation++;

    if (getenv("STRIKERS_LOG_FRUSTUM") != NULL)
        fprintf(stderr, "[aspect] window %ux%u -> %.4f (was %.4f), frame %u\n",
                width, height, (double)a, (double)was, PortLogicalFrameWidth());
}

// Bumped whenever the effective aspect changes.
unsigned int PortAspectGeneration(void)
{
    return s_generation;
}

// "16:9" and "16/9" and "1.7777". Returns 0 on anything else, including "auto", which the caller
// handles, because it is not a value but a promise to keep following the window.
static float ParseAspect(const char* s)
{
    double num = 0.0;
    double den = 0.0;
    char* end = NULL;

    num = strtod(s, &end);
    if (end == s)
        return 0.0f;

    while (*end == ' ')
        end++;

    if (*end == ':' || *end == '/')
    {
        const char* denStart = end + 1;
        den = strtod(denStart, &end);
        if (end == denStart || den <= 0.0)
            return 0.0f;
        num /= den;
        // fall through to the trailing-garbage check below
    }

    while (*end == ' ')
        end++;
    if (*end != '\0')
        return 0.0f;

    return num > 0.0 ? (float)num : 0.0f;
}

float PortTargetAspect(void)
{
    const char* env;
    float a;

    if (s_resolved)
        return s_aspect;

    // The environment is read once; the *value* is not necessarily fixed.
    s_resolved = 1;
    s_aspect = 16.0f / 9.0f;

    env = getenv("STRIKERS_ASPECT");
    if (env == NULL || *env == '\0' || strcmp(env, "auto") == 0
        || strcmp(env, "AUTO") == 0)
    {
        // 16:9 if main() reported nothing, which is also the default window.
        s_follows = 1;
        if (s_windowAspect > 0.0f)
            s_aspect = ClampAspect(s_windowAspect);
        return s_aspect;
    }

    a = ParseAspect(env);
    if (a > PORT_ASPECT_MAX || (a > 0.0f && a < PORT_ASPECT_MIN))
    {
        // Say so. A silently clamped aspect is a picture that letterboxes for no visible reason, so
        // a clamped aspect is reported rather than dropped.
        fprintf(stderr,
                "[aspect] STRIKERS_ASPECT=%.4f is outside [%.2f, %.2f]; "
                "using %.4f.\n",
                (double)a, (double)PORT_ASPECT_MIN, (double)PORT_ASPECT_MAX,
                (double)ClampAspect(a));
    }
    if (a <= 0.0f)
    {
        // Falling back silently to a shape the user did not ask for is how a typo becomes an hour
        // of wondering why the picture is wrong.
        fprintf(stderr,
                "[aspect] STRIKERS_ASPECT=\"%s\" not understood; using 16:9.\n"
                "         Expected W:H (16:9), a decimal (1.85), or auto.\n",
                env);
        return s_aspect;
    }

    s_aspect = ClampAspect(a);
    return s_aspect;
}

unsigned int PortLogicalFrameWidth(void)
{
    // 480 * aspect. PORT_CONSOLE_ASPECT is not in the arithmetic because it cancels: the design box
    // is 640x480 shown as 4:3, and 640 = 480 * 4/3, so the width that reproduces the console is
    // exactly what this yields at 4:3.
    float w = PORT_DESIGN_HEIGHT * PortTargetAspect();
    unsigned int u = (unsigned int)(w + 0.5f);

    u &= ~1u;   // even: GXSetFogRangeAdj and friends take a half-width

    return u;
}

#define PORT_CAMERA_BLEND_MAX 3.0f

static float s_blendOverride = -1.0f;   // < 0 means "derive it from the aspect"

// The debug menu's copy of the STRIKERS_CAMERA_BLEND override.
void PortSetCameraBlendOverride(float t)
{
    s_blendOverride = t;
    s_generation++;
}

float PortGetCameraBlendOverride(void)
{
    return s_blendOverride;
}

float PortCameraAspectBlend(void)
{
    const float a43 = 4.0f / 3.0f;
    const float a169 = 16.0f / 9.0f;
    float t;

    // STRIKERS_CAMERA_BLEND pinned this at 0 or 1 while the derivation was being checked.
    if (s_blendOverride >= 0.0f)
    {
        t = s_blendOverride;
    }
    else
    {
        // Linear in the aspect itself, because the thing the knots respond to; how much extra width
        // is on screen, is linear in it too.
        t = (PortTargetAspect() - a43) / (a169 - a43);
    }

    if (t < 0.0f)
        t = 0.0f;
    else if (t > PORT_CAMERA_BLEND_MAX)
        t = PORT_CAMERA_BLEND_MAX;
    return t;
}

// How much of the console's own horizontal stretch to keep. 1 is the shipped look, 0 is
// geometrically square.
static float s_stretch = 1.0f;

static float PortFrustumStretch(void)
{
    return s_stretch;
}

// The debug menu drives these three at runtime.
float PortGetFrustumStretch(void)
{
    return s_stretch;
}

void PortSetFrustumStretch(float s)
{
    s_stretch = (s < 0.0f) ? 0.0f : (s > 1.0f) ? 1.0f : s;
}

void PortPerspective(float consoleFov, float consoleRatio,
                     float* outFov, float* outRatio)
{
    // STRIKERS_LOG_FRUSTUM: what the game asked for against what it got, and the quickest way to
    // tell which frustum a screenshot was taken under, 1.25 is play, 1.3323944 is a cutscene.
    enum { kLogMax = 16 };
    enum { kLogFovBucketDeg = 5 };
    static int s_log = -1;
    static float s_seen[kLogMax][2];
    static int s_seenCount;

    // The console's stretch, as a factor: 1.0667 in play, 1.0007 in a cutscene.
    const float stretch = 1.0f + PortFrustumStretch()
                                     * (PORT_CONSOLE_ASPECT / consoleRatio - 1.0f);
    const float ratio = PortTargetAspect() / stretch;

    // Hold the vertical field of view: glplatMatrixPerspective's fov is the horizontal one, so it
    // has to move with the ratio to leave m11 alone. tan() rather than the angle, because the angle
    // is not what scales.
    const float scale = ratio / consoleRatio;

    const float fov = 2.0f * atanf(tanf(0.5f * consoleFov) * scale);

    if (outRatio != NULL)
        *outRatio = ratio;
    if (outFov != NULL)
        *outFov = fov;

    if (s_log < 0)
    {
        const char* e = getenv("STRIKERS_LOG_FRUSTUM");
        s_log = (e != NULL && *e != '\0' && *e != '0');
    }
    if (s_log && s_seenCount < kLogMax)
    {
        const float rad2deg = 57.29578f;
        const float bucket = (float)kLogFovBucketDeg
                             * floorf(consoleFov * rad2deg
                                          / (float)kLogFovBucketDeg + 0.5f);
        int seen = 0;
        int i;

        for (i = 0; i < s_seenCount; ++i)
        {
            if (s_seen[i][0] == bucket && s_seen[i][1] == consoleRatio)
                seen = 1;
        }
        if (seen)
            return;

        s_seen[s_seenCount][0] = bucket;
        s_seen[s_seenCount][1] = consoleRatio;
        s_seenCount++;

        fprintf(stderr,
                "[frustum] console %.2f deg at %.4f -> port %.2f deg at %.4f "
                "(display %.4f, stretch %.4f)\n",
                consoleFov * rad2deg, consoleRatio, fov * rad2deg, ratio,
                PortTargetAspect(), stretch);

        // Say so rather than falling silent. The cutscene cameras sweep a wide range of fovs, so a
        // long run fills the table honestly; and a reader who does not know it stopped will read
        // the absence of a frustum as its absence from the frame.
        if (s_seenCount == kLogMax)
            fprintf(stderr, "[frustum] (%d distinct requests logged; "
                            "no more will be)\n", kLogMax);
    }
}

// The aspect at which the frame stops needing help to look cinematic.
#define PORT_CINEMATIC_ASPECT 1.5f

static int s_bars = -1;

int PortDrawCinematicBars(void)
{
    // Derived, and latched on the first frame that asks; which is what STRIKERS_LETTERBOX did too,
    // since it was read once.
    if (s_bars < 0)
        s_bars = (PortTargetAspect() < PORT_CINEMATIC_ASPECT);

    return s_bars;
}

// The debug menu's switch for the bars.
void PortSetDrawCinematicBars(int draw)
{
    s_bars = draw ? 1 : 0;
}
