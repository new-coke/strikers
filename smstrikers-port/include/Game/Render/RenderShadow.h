#ifndef _RENDERSHADOW_H_
#define _RENDERSHADOW_H_

#include "types.h"
#include "NL/nlMath.h"
#include "NL/nlPrint.h"
#include "NL/gl/gl.h"
#include "NL/gl/glConstant.h"
#include "NL/gl/glState.h"
#include "NL/gl/glMatrix.h"

struct ProjectedShadowParams
{
    nlVector4 vLight;       // 0x00, size 0x10
    nlVector3 vPosition;    // 0x10, size 0xC
    float fRadius;          // 0x1C, size 0x4
    struct glModel* pModel; // 0x20, size 0x4
    float fWidth;           // 0x24, size 0x4
    float fHeight;          // 0x28, size 0x4
    float fScalar;          // 0x2C, size 0x4
    int nPartitionIndex;    // 0x30, size 0x4
    int nVisibleInterval;   // 0x34, size 0x4
    int nInvisibleInterval; // 0x38, size 0x4
};

struct glModel;

void RenderProjectedShadow(const ProjectedShadowParams& params);
void SetCharacterShadowUpdated(int index, bool updated);
void RenderCharacterIntoTexture(const ProjectedShadowParams& params);
u8 ShouldShadowBeUpdated(const ProjectedShadowParams& params);
int GetShadowPartitionIndex();
void RenderShadowModel(unsigned long flags, glModel* model, uintptr_t matrix);
void ShadowStartup();

extern int MaxProjectedShadows;

#endif // _RENDERSHADOW_H_
