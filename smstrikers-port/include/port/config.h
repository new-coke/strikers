
#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif
int PortConfigLoad(void);

// The path actually loaded, or NULL if none was.
const char* PortConfigPath(void);

#ifdef __cplusplus
}
#endif

#endif // PORT_CONFIG_H
