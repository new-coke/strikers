#ifndef _FEBASIC3DMODEL_H_
#define _FEBASIC3DMODEL_H_

#include "types.h"
#include "Game/FE/feLibObject.h"
#include "NL/gl/gl.h"

class FEBasic3dModel
{
public:
    virtual ~FEBasic3dModel();

    /* 0x04 */ FEBasic3dModel* next;
    /* 0x08 */ FEBasic3dModel* prev;
    /* 0x0C */ u8 mIsValid;
    /* 0x0D */ u8 mVisible;
    /* 0x0E */ u8 mIsCopy;
    /* 0x10 */ eGLView mTargetView;
    /* 0x14 */ feVector3 mPosition;
    /* 0x20 */ feVector3 mRotation;
}; // total size: 0x2C

#endif // _FEBASIC3DMODEL_H_
