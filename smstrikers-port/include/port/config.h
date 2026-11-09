
#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
int PortConfigLoad(void);

// The path actually loaded, or NULL if none was.
const char* PortConfigPath(void);

// The loaded strikers.ini's folder, else the executable's: where the logs go. Returns 0 on success.
int PortConfigDir(char* buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif // PORT_CONFIG_H
