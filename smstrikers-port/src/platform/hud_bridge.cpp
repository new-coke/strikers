// Exposes native frame timings to the Android HUD in integer units.

#if defined(__ANDROID__) && defined(PORT_USE_AURORA)

#include <jni.h>

#include "port/benchmark.h"

namespace {

// Keep these indices in sync with FrameMetrics.java.
enum
{
    FPS_TIMES_100 = 0,
    FRAME_US,
    BUSY_US,
    BUSY_P95_US,
    WORST_US,
    FRAME_COUNT,
    MATCH_ACTIVE,
    METRIC_COUNT,
};

} // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_newcoke_strikers_FrameMetrics_readNative(JNIEnv* env, jclass, jlongArray destination)
{
    if (destination == nullptr || env->GetArrayLength(destination) != (jsize)METRIC_COUNT)
        return JNI_FALSE;

    PortBenchLive live;
    PortBenchGetLive(&live);

    jlong values[METRIC_COUNT];
    values[FPS_TIMES_100] = (jlong)(live.fps * 100.0);
    values[FRAME_US] = (jlong)(live.frameMs * 1000.0);
    values[BUSY_US] = (jlong)(live.busyMs * 1000.0);
    values[BUSY_P95_US] = (jlong)(live.busyP95Ms * 1000.0);
    values[WORST_US] = (jlong)(live.worstMs * 1000.0);
    values[FRAME_COUNT] = (jlong)live.frames;
    values[MATCH_ACTIVE] = (jlong)live.matchActive;

    env->SetLongArrayRegion(destination, 0, (jsize)METRIC_COUNT, values);

    return live.frameMs > 0.0 ? JNI_TRUE : JNI_FALSE;
}

#endif
