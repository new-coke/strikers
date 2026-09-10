#ifndef _FERENDER_H_
#define _FERENDER_H_

#include "NL/gl/glMatrixStack.h"
#include "NL/nlColour.h"

void ConvertColour(nlColour&, const nlFloatColour&);

class TLImageInstance;
class TLTextInstance;
class FEScene;
class FEPresentation;
class TLComponentInstance;
class TLSlide;
class TLInstance;

class FERender
{
public:
    static unsigned char RenderImageInstance(const TLImageInstance* pTLImageInstance);
    static void RenderTextInstance(TLTextInstance* textInstance);
    static void RenderScene(FEScene* scene);
    static void RenderPresentation(const FEPresentation* presentation);
    static void RenderComponentInstance(TLComponentInstance* componentInstance);
    static void RenderSlide(const TLSlide* pTLSlide);
    static void RenderTimeLineAsset(TLInstance* pTLInstance, float fCurrentTime);
    static void PopTransformMatrix();
    static void PushTransformMatrix(const TLInstance* instance);
    static void Initialize();
    static void Cleanup();
    static void CalculateCurrentAssetColour(const TLInstance* instance);

    static GLMatrixStack* m_pMatrixStack;
    static FEScene* m_pRenderScene;
};

#endif // _FERENDER_H_
