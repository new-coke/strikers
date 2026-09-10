#ifndef _DRAWABLEMODEL_H_
#define _DRAWABLEMODEL_H_

#include "Game/Drawable/DrawableObj.h"

#include "NL/nlMath.h"
#include "NL/nlAVLTree.h"
#include "NL/gl/glView.h"
#include "NL/gl/glModel.h"

struct glModel;
struct glModelPacket;
class GLShadowVolume;
struct BallShadowParams
{
    float fReferenceHeight;
    float fRadius0;
    float fRadius1;
    int nAlpha0;
    int nAlpha1;
    nlColour colour;
};

void CleanBoundingBoxCache();
void RenderBoundingBox(const glModel* model, const nlMatrix4& matrix);
float GetCoPlanar0Z();
void SetCoPlanarZ(float z);
float GetCoPlanarZ();
void SetPlanarShadowOpacity(float opacity);
float GetPlanarShadowOpacity();
void DrawPlanarShadow(const glModel* model, const nlMatrix4& worldMatrix, float shadowTranslucency, bool ignorePacketMatrices, bool isModelPosed, bool bFieldOnlyShadow, uintptr_t boundingBoxCacheKey);
void GetAABBDimensions(const glModel* model, AABBDimensions& dimensions, uintptr_t boundingBoxCacheKey);

class DrawableModel : public DrawableObject
{
public:
    DrawableModel()
        : m_pModel(NULL)
        , m_pShadowVolume(NULL)
        , m_bVertexAnimated(false)
        , m_bUnknownA5(false)
    {
    }

    DrawableModel(const DrawableModel& other)
        : DrawableObject(other)
        , m_pModel(other.m_pModel)
        , m_pShadowVolume(other.m_pShadowVolume)
        , m_bVertexAnimated(other.m_bVertexAnimated)
        , m_bUnknownA5(other.m_bUnknownA5)
        , pAABBDimensions(other.pAABBDimensions) {
        };
    /* 0x08 */ virtual DrawableObject* Clone() const;
    /* 0x0C */ virtual void Draw();
    /* 0x10 */ virtual void DrawPlanarShadow();
    /* 0x14 */ virtual bool IsDrawableModel() { return true; };
    /* 0x18 */ virtual DrawableModel* AsDrawableModel() { return this; };
    /* 0x1C */ virtual void GetAABBDimensions(AABBDimensions& dims, bool param) const;

    void DrawModel(const nlMatrix4& worldMatrix);

    inline glModel* GetModel() const { return m_pModel; }

    static unsigned char sbBallShadowDisabled;
    static unsigned char GetBallShadowDisabled()
    {
        return sbBallShadowDisabled;
    }
    static void SetBallShadowDisabled(unsigned char disabled)
    {
        sbBallShadowDisabled = disabled;
    }

    /* 0x9C */ glModel* m_pModel;
    /* 0xA0 */ GLShadowVolume* m_pShadowVolume;
    /* 0xA4 */ bool m_bVertexAnimated;
    /* 0xA5 */ bool m_bUnknownA5;
    /* 0xA8 */ AABBDimensions* pAABBDimensions;
};

class DrawableShadow : public DrawableObject
{
public:
    DrawableShadow() { }

    /* 0x0C */ virtual void Draw();

    /* 0x9C */ glModel* m_pModel;
};

// class nlAVLTreeSlotPool<unsigned long, AABBDimensions, DefaultKeyCompare<unsigned long>>
// {
// public:
// };

// class AVLTreeBase<unsigned long, AABBDimensions, BasicSlotPool<AVLTreeEntry<unsigned long, AABBDimensions>>, DefaultKeyCompare<unsigned long>>
// {
// public:
// };

#endif // _DRAWABLEMODEL_H_
