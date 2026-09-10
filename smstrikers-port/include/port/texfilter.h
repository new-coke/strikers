// Anisotropic texture filtering: the one graphics setting the retail engine already had and never
// used.

#ifndef PORT_TEXFILTER_H
#define PORT_TEXFILTER_H

#ifdef __cplusplus
extern "C" {
#endif

// The maximum anisotropy to ask the GPU for: 1, 2, 4, 8 or 16.
unsigned int PortTextureAniso(void);

// The index into the retail engine's own `{ GX_ANISO_1, GX_ANISO_2, GX_ANISO_4 }` table: 0 when
// PortTextureAniso() is 1, otherwise 2.
int PortTextureAnisoIndex(void);

#ifdef __cplusplus
}
#endif

#endif // PORT_TEXFILTER_H
