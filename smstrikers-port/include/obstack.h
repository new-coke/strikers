#ifndef PORT_FWD_OBSTACK_H
#define PORT_FWD_OBSTACK_H
// Forwarding shim: src/ode is not on the global include path, since its memory.h, matrix.h and array.h would shadow the public copies.
#include "../src/ode/obstack.h"
#endif
