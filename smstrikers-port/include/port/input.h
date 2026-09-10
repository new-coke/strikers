
#ifndef PORT_INPUT_H
#define PORT_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

int  PortInputRecordStart(const char* path);
void PortInputRecordStop(void);
const char* PortInputRecordPath(void);

// A recording's frame numbers are absolute, so one started here is shifted.
int  PortInputReplayStart(const char* path);
void PortInputReplayStop(void);
int  PortInputReplayStatus(unsigned long* count, unsigned long* cursor,
                           unsigned long* firstFrame, unsigned long* lastFrame,
                           const char** path);

// Hold a PAD_BUTTON_* mask on pad 0, merged with the real pad; 0 frames is the default hold.
void PortInputPress(unsigned int buttons, int frames);

// The same for the main stick, at -100..100 per axis; nothing in the environment drives it, so it
// exists for the control channel's `stick`.
void PortInputStick(int x, int y, int frames);

// One of STRIKERS_AUTOPRESS's button names as a mask for the call above, or 0; `len` bounds the
// name, and negative means to the NUL.
unsigned int PortInputButtonFromName(const char* name, int len);

unsigned long PortInputFrame(void);

#ifdef __cplusplus
}
#endif

#endif // PORT_INPUT_H
