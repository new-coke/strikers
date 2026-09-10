#include "Game/Render/GraphicsLoader.h"

GraphicsLoader TheGraphicsLoader;

/**
 * Offset/Address/Size: 0x0 | 0x8015F1DC | size: 0x78
 */
bool GraphicsLoader::StartLoad(LoadingManager* manager)
{
    glLoadModel("debug/debug.glg", NULL);
    glLoadModel("effects/meshes.glg", NULL);
    glLoadTextureBundle("effects/effects.glt");
    ShadowStartup();
    EmissionManager::Startup(GLV_Particles);
    fxLoadTemplateBundle("art/effects/templates.fx");
    fxLoadGroupBundle("art/effects/scripts.fx");
    fxParticleStartup(1536);

    return true;
}
