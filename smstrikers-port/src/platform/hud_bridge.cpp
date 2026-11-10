// The game's own frame numbers, handed to the Java side that draws the performance panel.
//
// WHY THE GAME'S AND NOT ANDROID'S. Android will happily tell an app how often the screen refreshed
// -- Choreographer callbacks, SurfaceFlinger's counters -- and that number is not the game's frame
// rate. It reads 120 on a 120 Hz panel with the game frozen on one image. What this hands over is
// what the port's own limiter measured: PortBenchGetLive, the same numbers the debug overlay draws.
//
// The units are fixed here rather than in Java: microseconds and hundredths, as integers. A long[]
// crosses JNI in one call and needs no object per field, and integers cannot pick up a different
// rounding on the other side.

#if defined(__ANDROID__) && defined(PORT_USE_AURORA)

#include <jni.h>

#include "port/benchmark.h"

namespace {

// The order IS the contract with Medidas.java. Changing an index here does not fail to compile and
// does not fail at runtime: it just relabels the numbers, and the panel keeps looking right while
// showing the wrong thing. Add at the end, never in the middle.
enum
{
    MEDIDA_FPS_CENTESIMOS = 0,   // fps * 100
    MEDIDA_QUADRO_US,            // the whole frame, microseconds
    MEDIDA_OCUPADO_US,           // the part the game spent working
    MEDIDA_OCUPADO_P95_US,
    MEDIDA_PIOR_US,
    MEDIDA_QUADROS,
    MEDIDA_EM_PARTIDA,
    MEDIDA_QUANTIDADE,
};

} // namespace

extern "C" JNIEXPORT jint JNICALL
Java_io_github_newcoke_strikers_Medidas_quantidade(JNIEnv*, jclass)
{
    return (jint)MEDIDA_QUANTIDADE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_newcoke_strikers_Medidas_lerTudo(JNIEnv* env, jclass, jlongArray destino)
{
    if (destino == nullptr || env->GetArrayLength(destino) < (jsize)MEDIDA_QUANTIDADE)
        return JNI_FALSE;

    PortBenchLive live;
    PortBenchGetLive(&live);

    jlong valores[MEDIDA_QUANTIDADE];
    valores[MEDIDA_FPS_CENTESIMOS] = (jlong)(live.fps * 100.0);
    valores[MEDIDA_QUADRO_US] = (jlong)(live.frameMs * 1000.0);
    valores[MEDIDA_OCUPADO_US] = (jlong)(live.busyMs * 1000.0);
    valores[MEDIDA_OCUPADO_P95_US] = (jlong)(live.busyP95Ms * 1000.0);
    valores[MEDIDA_PIOR_US] = (jlong)(live.worstMs * 1000.0);
    valores[MEDIDA_QUADROS] = (jlong)live.frames;
    valores[MEDIDA_EM_PARTIDA] = (jlong)live.matchActive;

    env->SetLongArrayRegion(destino, 0, (jsize)MEDIDA_QUANTIDADE, valores);

    // False before the first frame has been measured, so the panel writes "--" instead of a zero.
    // Zero reads as a measurement; "--" reads as "not yet", and they are different things.
    return live.frameMs > 0.0 ? JNI_TRUE : JNI_FALSE;
}

#endif
