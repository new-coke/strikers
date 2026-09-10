#ifndef PORT_MSL_ANSI_FILES_H
#define PORT_MSL_ANSI_FILES_H
// MSL's FILE internals redefine fpos_t and off_t against the host's, so defer to host stdio; the tree spells MSL's underlying `_FILE` directly, so alias it.
#include <stdio.h>
typedef FILE _FILE;
#endif
