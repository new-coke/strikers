package io.github.newcoke.strikers;

/** Sends overlay input to the SDL virtual gamepad in src/platform/touch.cpp. */
final class TouchGamepad {

    private TouchGamepad() {}

    // The native bridge maps these bits to SDL buttons.
    static final int A = 1 << 0;
    static final int B = 1 << 1;
    static final int X = 1 << 2;
    static final int Y = 1 << 3;
    static final int Z = 1 << 4;
    static final int START = 1 << 5;
    static final int UP = 1 << 6;
    static final int DOWN = 1 << 7;
    static final int LEFT = 1 << 8;
    static final int RIGHT = 1 << 9;
    // Xbox-style controls with no GameCube equivalent.
    static final int LB = 1 << 10;
    static final int L3 = 1 << 11;
    static final int R3 = 1 << 12;
    static final int BACK = 1 << 13;

    private static native void sendNative(int buttons, int leftTrigger, int rightTrigger,
                                          int lx, int ly, int cx, int cy);

    private static native boolean nativeIsActive();

    // Retry a false result because SDL creates the gamepad after the loop starts.
    private static boolean nativeUnavailable;

    static boolean isActive() {
        if (nativeUnavailable) return false;
        try {
            return nativeIsActive();
        } catch (UnsatisfiedLinkError e) {
            nativeUnavailable = true;
            return false;
        }
    }

    static void sendState(int buttons, int leftTrigger, int rightTrigger,
                          int lx, int ly, int cx, int cy) {
        if (nativeUnavailable) return;
        try {
            sendNative(buttons, leftTrigger, rightTrigger, lx, ly, cx, cy);
        } catch (UnsatisfiedLinkError e) {
            nativeUnavailable = true;
        }
    }
}
