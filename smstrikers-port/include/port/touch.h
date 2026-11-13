#ifndef PORT_TOUCH_H
#define PORT_TOUCH_H

// The Android overlay sends input through an SDL virtual gamepad.

#ifdef __cplusplus
extern "C" {
#endif

// Applies the latest touch state each frame and detaches the pad for a physical controller.
void PortTouchUpdate(void);

// Non-zero while the virtual pad exists, which is the same as "the overlay should be visible".
int PortTouchActive(void);

#ifdef __cplusplus
}
#endif

#endif // PORT_TOUCH_H
