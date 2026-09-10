#ifndef _GLXGX_H_
#define _GLXGX_H_

#include "dolphin/gx/GXEnum.h"
#include "NL/nlColour.h"

bool gxSetCoPlanar(bool coplanar);
nlColour gxSetChanAmbColour(int chan, const nlColour& color);
nlColour gxSetChanMatColour(int chan, const nlColour& color);
void gxSetTexCoordGen(int dst_coord, _GXTexGenType func, _GXTexGenSrc src_param, unsigned long arg);
void gxSetTevAlphaOp(int stage, _GXTevOp op, _GXTevBias bias, _GXTevScale scale, bool clamp, _GXTevRegID out_reg);
void gxSetTevColourOp(int stage, _GXTevOp op, _GXTevBias bias, _GXTevScale scale, bool clamp, _GXTevRegID out_reg);
u32 gxSetNumTexGens(unsigned long numGens);
u32 gxSetNumTevStages(unsigned long numTEV);
u32 gxSetNumChans(unsigned long numChans);
_GXCullMode gxSetCullMode(_GXCullMode mode);
void gxSetBlendMode(bool bBlend, _GXBlendFactor src_factor, _GXBlendFactor dst_factor, bool bSubtract);
void gxSetAlphaCompare(_GXCompare func, unsigned char ref);
void gxRestoreZMode();
void gxSaveZMode();
void gxSetZMode(bool bTest, _GXCompare func, bool bWrite);
bool gxSetZCompLoc(bool bBefore);
bool gxSetAlphaUpdate(bool bOn);
bool gxSetColourUpdate(bool bOn);
uint gxSetDither(bool dither);
uint gxSetTevAlphaIn(int stage, int component, _GXTevAlphaArg arg);
void gxSetTevAlphaIn(int stage, _GXTevAlphaArg a, _GXTevAlphaArg b, _GXTevAlphaArg c, _GXTevAlphaArg d);
void gxInit();

#endif // _GLXGX_H_
