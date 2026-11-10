package io.github.newcoke.strikers;

/** Native frame timings. Keep the indices in sync with src/platform/hud_bridge.cpp. */
final class FrameMetrics {

    private FrameMetrics() {}

    static final int FPS_TIMES_100 = 0;
    static final int FRAME_US = 1;
    static final int BUSY_US = 2;
    static final int BUSY_P95_US = 3;
    static final int WORST_US = 4;
    static final int FRAME_COUNT = 5;
    static final int MATCH_ACTIVE = 6;
    static final int METRIC_COUNT = 7;

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
