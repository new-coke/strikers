#ifndef _DRAWABLEOBJ_H_
#define _DRAWABLEOBJ_H_

#include "NL/nlMath.h"
#include "NL/gl/gl.h"

#include "Game/World.h"

struct glModel;

class AABBDimensions
{
public:
    /* 0x00 */ nlVector3 mMin;
    /* 0x0C */ nlVector3 mMax;
    /* 0x18 */ nlVector3 mDim;
}; // total size: 0x24

class DrawableObject
{
public:
    DrawableObject();
    /* 0x08 */ virtual ~DrawableObject();
    /* 0x0C */ virtual DrawableObject* Clone() const { return NULL; }
    /* 0x10 */ virtual void Draw() = 0;
    /* 0x14 */ virtual void DrawPlanarShadow() { };
    /* 0x18 */ virtual bool IsDrawableModel() { return false; };
    /* 0x1C */ virtual DrawableModel* AsDrawableModel() { return NULL; }
    /* 0x20 */ virtual void GetAABBDimensions(AABBDimensions& dim, bool param) const;

    nlMatrix4& GetWorldMatrix() const;
    unsigned long GetHashID() const { return m_uHashID; }
    glModel* (*GetCallback() const)(glModel*, eGLView&, unsigned long&)
    {
        return m_CB;
    }

    /* 0x04 */ mutable nlMatrix4 m_worldMatrix;
    /* 0x44 */ mutable bool m_worldMatrixUpToDate;
    /* 0x48 */ nlQuaternion m_orientation;
    /* 0x58 */ nlVector3 m_translation;
    /* 0x64 */ float m_scale;
    /* 0x68 */ float m_translucency;
    /* 0x6C */ AABBDimensions* m_pCachedAABB;
    /* 0x70 */ unsigned long m_uHashID;
    /* 0x74 */ unsigned long m_uRenderLayer;
    /* 0x78 */ unsigned long m_uObjectCreationFlags;
    /* 0x7C */ nlVector3 m_v3Offset;
    /* 0x88 */ World* m_pWorldContext;
    /* 0x8C */ u32 m_uObjectFlags;
    /* 0x90 */ f32 m_fBoundingRadius;
    /* 0x94 */ bool m_bRenderPlanarShadow;
    /* 0x98 */ glModel* (*m_CB)(struct glModel*, enum eGLView&, unsigned long&);
}; // total size: 0x9C

#endif // _DRAWABLEOBJ_H_
