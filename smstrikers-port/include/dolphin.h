#ifndef PORT_FWD_DOLPHIN_H
#define PORT_FWD_DOLPHIN_H
// Forwarding shim: the tree includes "dolphin.h" unqualified, which on-console
// resolved via `-i include/dolphin`. We cannot put that directory on the search
// path wholesale because include/dolphin/math.h would shadow the host <math.h>
// and break libc++, so the few unqualified names get explicit forwarders.
#include "dolphin/dolphin.h"
#endif
