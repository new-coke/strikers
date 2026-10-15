
#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif
int PortConfigLoad(void);

// Init-time master switch. Missing diagnostics keeps the existing configured
// probes/profilers enabled; diagnostics=0 suppresses them before workers start.
// CPU3 budget accounting and renderer optimizations are independent of it.
int PortDiagnosticsEnabled(void);
// Optional minimal whole-frame FPS measurement, identical in both profiles.
// fps_overlay=0 also removes this final measurement/display when diagnostics=0.
int PortFpsOverlayEnabled(void);
// Live-only debug switch; does not modify strikers.ini or diagnostics.
void PortSetFpsOverlayEnabled(int enabled);

// The path actually loaded, or NULL if none was.
const char* PortConfigPath(void);



#ifdef __cplusplus
}
#endif

#endif // PORT_CONFIG_H
