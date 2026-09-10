#ifndef PORT_MSL_FILE_POS_H
#define PORT_MSL_FILE_POS_H
// MSL declares fseek with an `unsigned long` offset and the host uses `long`, so defer to host stdio.
#include <stdio.h>
#endif
