#ifndef _GRAPHICSLOADER_H_
#define _GRAPHICSLOADER_H_

#include "Game/Loader.h"
#include "NL/gl/gl.h"
#include "Game/Render/RenderShadow.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/Effects/EffectsTemplate.h"
#include "Game/Effects/EffectsGroup.h"
#include "Game/Effects/ParticleSystem.h"

class GraphicsLoader : public Loader
{
public:
    virtual bool StartLoad(LoadingManager* manager);
};

#endif // _GRAPHICSLOADER_H_
