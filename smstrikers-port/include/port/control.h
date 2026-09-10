// STRIKERS_CONTROL=<path>: drive a headless run from a text file that is appended to while the run
// is going. Every line is acknowledged on stderr with the frame it ran on, so a script waits for
// its own ack rather than sleeping and guessing.

#ifndef PORT_CONTROL_H
#define PORT_CONTROL_H

#include "port/overlay.h"   // PortDebugCommand, for the shared `cmd` parser

#ifdef __cplusplus
extern "C" {
#endif

// Read and run whatever has been appended since the last call; inert when STRIKERS_CONTROL is
// unset. Called once a frame from PortOverlayDraw, inside Aurora's frame, which is where `shot` has
// to ask for its readback.
void PortControlPoll(void);

// `op[,a,b,c,f0..f3][=string]`: STRIKERS_DEBUG_CMD's syntax without the `@frame`, which is also the
// control channel's `cmd` line. Returns 0 if there was no number. Here because this file is in the
// Aurora-off scan.
int PortDebugParseCommand(const char* spec, PortDebugCommand* out);

#ifdef __cplusplus
}
#endif

#endif // PORT_CONTROL_H
