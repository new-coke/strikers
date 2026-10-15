// The AuroraConfig fields that have to be chosen before the window exists.

#ifndef PORT_LAUNCH_H
#define PORT_LAUNCH_H

#if defined(PORT_USE_AURORA) || defined(STRIKERS_VITA_NATIVE_RUNTIME)

#if defined(PORT_USE_AURORA)
#include <aurora/aurora.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Apply the window, focus, shader job and cache settings to `cfg`, give it an icon, and place the window with its title bar on screen.
#if defined(PORT_USE_AURORA)
void PortAuroraConfigure(AuroraConfig* cfg);
#endif

void PortFollowRenderScale(unsigned int windowHeight);

void PortSetRenderScale(float scale);

float PortRenderScale(void);

#ifdef __cplusplus
}
#endif

#endif // PORT_USE_AURORA

#endif // PORT_LAUNCH_H
