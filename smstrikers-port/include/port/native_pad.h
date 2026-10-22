#ifndef PORT_NATIVE_PAD_H
#define PORT_NATIVE_PAD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Stop owned external actuators before runtime teardown. No allocation or wait.
 * A negative result preserves the pending stop for a later bounded retry. */
int PortPadShutdown(void);

#ifdef __cplusplus
}
#endif
#endif
