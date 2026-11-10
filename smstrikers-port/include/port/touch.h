#ifndef PORT_TOUCH_H
#define PORT_TOUCH_H

// On-screen controls, for the one platform where the game has no buttons to press.
//
// The controls are DRAWN on the Java side, in OverlayView, as an ordinary Android view over the
// game's surface. Not here, and not in ImGui: the overlay has to respect the window insets, follow
// the player's own settings, and stay crisp at any density -- all of which Android's canvas does
// for free and an ImGui draw list does not.
//
// What is left on this side is the other half. Nothing here reaches into PAD: the state that
// arrives from Java drives an SDL virtual joystick declared as a gamepad, so Aurora's PAD sees an
// ordinary controller arriving and maps it to a port through exactly the code path a real one goes
// through. Remapping, deadzones, the button prompts and the port assignment all keep working, and
// none of them had to learn what a finger is.
//
// A no-op on every other platform, and on Android too whenever a real controller is connected: the
// pad is detached and PortTouchActive() goes to zero, which is how the overlay knows to take itself
// off the screen.

#ifdef __cplusplus
union SDL_Event;

extern "C" {

// Kept so the call site in main.cpp is the same on every platform; the fingers never reach here.
void PortTouchEvent(const SDL_Event* event);
#endif

// Once a frame, from the game loop. Attaches the virtual pad the first time, hands it the state the
// overlay last sent, and detaches it when a real controller shows up.
void PortTouchDraw(void);

// Non-zero while the virtual pad exists, which is the same as "the overlay should be visible".
int PortTouchActive(void);

#ifdef __cplusplus
}
#endif

#endif // PORT_TOUCH_H
