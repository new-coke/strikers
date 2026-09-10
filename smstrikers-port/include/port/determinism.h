
// Game/main.cpp seeds the RNG from OSGetTick(), which is why the demo's stadium and teams change;
// STRIKERS_SEED pins it.

// nlTaskManager::RunAllTasks takes its delta from nlGetTicker, the host clock, so two runs diverge
// as soon as one loads a texture a millisecond slower; STRIKERS_FIXED_DT drives that clock from the
// retrace count, as the console did.

// Nothing spin-waits on nlGetTicker, so that cannot hang, but the loader's "blocked for N ms" then
// reports virtual milliseconds and a run that misses its frame rate plays in slow motion.

#ifndef PORT_DETERMINISM_H
#define PORT_DETERMINISM_H

#ifdef __cplusplus
extern "C" {
#endif

int PortFixedTimestep(void);

// Retrace count times the fixed frame period, in OSGetTick units. Meaningless unless
// PortFixedTimestep().
unsigned int PortVirtualTicker(void);

// Returns 0 when STRIKERS_SEED is unset, leaving `out` untouched.
int PortFixedSeed(unsigned int* out);

#ifdef __cplusplus
}
#endif

#endif // PORT_DETERMINISM_H
