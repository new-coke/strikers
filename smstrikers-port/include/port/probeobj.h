// Draw-time probe registry: name a stadium object's packet range at load, so the submit path can
// dump the render state each packet is drawn with.

// STRIKERS_PROBE_OBJ=<name> selects it, named as in the .wld; registration is in BasicStadium and
// the dump in glx_SendFrame_cb. Free when unset.

#ifndef PORT_PROBEOBJ_H
#define PORT_PROBEOBJ_H

#ifdef __cplusplus
extern "C" {
#endif

void PortProbeObjRegister(const void* base, int count, unsigned stride,
                          const char* name);

// A per-packet key too, because the render list can submit a shallow copy that a pointer range test
// misses; the copy keeps the packet's `streams` pointer.
void PortProbeObjRegisterKey(const char* name, int index, const void* key);

int PortProbeObjIndex(const void* p, const void* key, const char** nameOut);

int PortProbeObjShouldLog(unsigned long frame);

// Ambient and material colours last given to GX for channels 0/1, packed RGBA, plus the channel
// count. Defined in glxGX.cpp, where the cache lives.
void gxProbeChanState(unsigned int amb[2], unsigned int mat[2],
                      unsigned int* numChans);

// STRIKERS_PROBE_TEXDUMP=<dir>: a GC-swizzled big-endian RGB565 image out as a de-swizzled PPM
// named <tag>_<index>_<w>x<h>.ppm.
void PortProbeDumpRGB565(const char* tag, int index, const void* data,
                         unsigned w, unsigned h);

void PortProbeDumpCMPR(const char* tag, int index, const void* data,
                       unsigned w, unsigned h);

#ifdef __cplusplus
}
#endif

#endif // PORT_PROBEOBJ_H
