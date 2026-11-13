// Android reads a snapshot without touching the benchmark's game-thread state.
#if defined(__ANDROID__) && defined(PORT_USE_AURORA)

#include <atomic>
#include <jni.h>
#include <mutex>

#include "port/benchmark.h"

namespace {
std::atomic<bool> sampleRequested{false};
std::mutex snapshotMutex;
PortBenchLive snapshot{};
}

// Called on the game thread after the benchmark records the frame.
void PortAndroidPublishFrameMetrics()
{
    if (!sampleRequested.exchange(false, std::memory_order_relaxed))
        return;

    PortBenchLive live;
    PortBenchGetLive(&live);
    std::lock_guard<std::mutex> lock(snapshotMutex);
    snapshot = live;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_newcoke_strikers_FrameMetrics_readNative(JNIEnv* env, jclass, jlongArray destination)
{
    if (destination == nullptr || env->GetArrayLength(destination) != 3)
        return JNI_FALSE;

    sampleRequested.store(true, std::memory_order_relaxed);
    PortBenchLive live;
    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        live = snapshot;
    }
    if (live.frameMs <= 0.0)
        return JNI_FALSE;

    // Order matches FrameMetrics.java.
    const jlong values[] = {
        (jlong)(live.fps * 100.0),
        (jlong)(live.frameMs * 1000.0),
        (jlong)(live.busyMs * 1000.0),
    };
    env->SetLongArrayRegion(destination, 0, 3, values);
    return JNI_TRUE;
}

#endif
