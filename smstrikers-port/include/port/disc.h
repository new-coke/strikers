// Reading a GameCube disc image, so the game data can be a file somebody already has. dvd.c reads
// an extracted directory and that stays the default: it is what a release archive ships.

#ifndef PORT_DISC_H
#define PORT_DISC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PortDisc PortDisc;

// Open `path` as a disc image. Returns NULL if it is not one this build can read, having written a
// finished, user-facing explanation into `err`; which names the format when it recognised one, and
// says what to do about it.
PortDisc* port_disc_open(const char* path, char* err, size_t errsize);
long port_disc_read(PortDisc* disc, void* dst, size_t len,
                    unsigned long long offset);

// "raw", "CISO" or "GCZ", for the startup line. Never NULL.
const char* port_disc_format(const PortDisc* disc);
int port_disc_looks_like_image(const char* path);

void port_disc_close(PortDisc* disc);

#ifdef __cplusplus
}
#endif

#endif // PORT_DISC_H
