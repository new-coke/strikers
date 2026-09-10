// Field-overwrite watch for the cSAnim morph-table corruption: GetMorphWeight reads m_pNumMorphKeys
// off a pointer that was correct when stored.

// STRIKERS_WATCH_MORPH=1 registers the three morph-table fields of every loaded animation, and
// vi.c's retrace hook polls them once a frame.

// cSAnim objects come from nlMalloc and are reclaimed by arena resets, so a report during a scene
// transition may be reuse rather than corruption.

// STRIKERS_WATCH_MORPH_PROT=1 also makes every page holding a watched field PROT_READ over retraces
// 3000..12000 (STRIKERS_WATCH_MORPH_PROT_FROM/_TO), so the offending store faults and the handler
// logs the writer's pc.

#ifndef PORT_MORPHWATCH_H
#define PORT_MORPHWATCH_H

#ifdef __cplusplus
extern "C" {
#endif

// Snapshots an 8-byte field of `obj`; the same address again replaces the snapshot. No-op unless
// STRIKERS_WATCH_MORPH is set, and `tag` must outlive the run.
void PortMorphWatchRegister(const void* obj, const void* fieldAddr,
                            const char* tag);

void PortMorphWatchPoll(unsigned long frame);

#ifdef __cplusplus
}
#endif

#endif // PORT_MORPHWATCH_H
