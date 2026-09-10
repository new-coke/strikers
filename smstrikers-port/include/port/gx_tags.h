#ifndef PORT_GX_TAGS_H
#define PORT_GX_TAGS_H
// GENERATED from Aurora's GX headers; do not edit by hand.

// Aurora declares the GX types as anonymous structs with a typedef name, while game code uses the
// decomp's struct tags; one typedef per tag reconciles them.

// Must come after Aurora's headers, so it includes them and is force-included via port/prelude.h.

#include <dolphin/gx.h>

typedef GXBlendFactor _GXBlendFactor;
typedef GXCompare _GXCompare;
typedef GXCullMode _GXCullMode;
typedef GXLightID _GXLightID;
typedef GXPrimitive _GXPrimitive;
typedef GXProjectionType _GXProjectionType;
typedef GXRenderModeObj _GXRenderModeObj;
typedef GXTevAlphaArg _GXTevAlphaArg;
typedef GXTevBias _GXTevBias;
typedef GXTevOp _GXTevOp;
typedef GXTevRegID _GXTevRegID;
typedef GXTevScale _GXTevScale;
typedef GXTexFmt _GXTexFmt;
typedef GXTexGenSrc _GXTexGenSrc;
typedef GXTexGenType _GXTexGenType;
typedef GXTexMtxType _GXTexMtxType;
typedef GXTexObj _GXTexObj;
typedef GXTlut _GXTlut;
typedef GXTlutObj _GXTlutObj;
typedef GXVtxFmt _GXVtxFmt;

#endif // PORT_GX_TAGS_H
