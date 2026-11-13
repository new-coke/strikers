package io.github.newcoke.strikers;

/** Native frame timings. Keep the indices in sync with src/platform/android/hud_bridge.cpp. */
final class FrameMetrics {

    private FrameMetrics() {}

    static final int FPS_TIMES_100 = 0;
    static final int FRAME_US = 1;
    static final int BUSY_US = 2;
    static final int METRIC_COUNT = 3;

    private static native boolean readNative(long[] destination);

    private static boolean nativeUnavailable;

    static boolean read(long[] destination) {
        if (nativeUnavailable) return false;
        try {
            return readNative(destination);
        } catch (UnsatisfiedLinkError e) {
            nativeUnavailable = true;
            return false;
        }
    }
}
