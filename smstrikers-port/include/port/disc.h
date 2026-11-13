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
#ifdef __ANDROID__
// Keeps its own duplicate of the seekable Android document descriptor.
PortDisc* port_disc_open_fd(int fd, char* err, size_t errsize);
#endif
long port_disc_read(PortDisc* disc, void* dst, size_t len,
                    unsigned long long offset);

// "raw", "CISO" or "GCZ", for the startup line. Never NULL.
const char* port_disc_format(const PortDisc* disc);
int port_disc_looks_like_image(const char* path);

typedef int (*PortDiscVisit)(void* user, const char* path, unsigned offset,
                             unsigned length, int isDir);

// Names are not validated: a caller that turns one into a host path has to.
int port_disc_walk(PortDisc* disc, PortDiscVisit visit, void* user, char* err,
                   size_t errsize);

// Supplied by the host when built with PORT_DISC_INFLATE_HOOK instead of STRIKERS_ZLIB.
int port_disc_inflate(unsigned char* dst, unsigned long* dstLen,
                      const unsigned char* src, unsigned long srcLen);

void port_disc_close(PortDisc* disc);

// 1 once STRIKERS_DISC_IN_RAM has copied the whole image into memory.
int port_disc_in_memory(PortDisc* disc);

#ifdef __cplusplus
}
#endif

#endif // PORT_DISC_H
