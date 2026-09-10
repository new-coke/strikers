#ifndef PORT_MSL_DIRECT_IO_H
#define PORT_MSL_DIRECT_IO_H
// MSL declares fread and fwrite with a `const void*` buffer and the host's take `void*`, so defer to host stdio.
#include <stdio.h>
#endif
