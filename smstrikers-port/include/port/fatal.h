// Saying why the game will not start, to somebody with no terminal: stderr, and a message box where
// a display can be opened.

// The display check is not optional, because SDL's box blocks until dismissed and would turn a
// headless failure into a hang. STRIKERS_NO_MESSAGEBOX=1 forces stderr only.

#ifndef PORT_FATAL_H
#define PORT_FATAL_H

#ifdef __cplusplus
extern "C" {
#endif

// Report and exit(1). Never returns; `text` may contain newlines.
void port_fatal(const char* title, const char* text);

#ifdef __cplusplus
}
#endif

#endif // PORT_FATAL_H
