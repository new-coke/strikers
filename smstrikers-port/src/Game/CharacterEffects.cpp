#include "Game/CharacterEffects.h"

static EffectsTexturing fxTexturing[] = {
    { 0xFFFFFFFF, GLB_None, false, false },
    { glGetTexture("effects/fx_env_ice"), GLB_ScaledAdditive, false, false },
    { glGetTexture("effects/fx_env_fire"), GLB_ScaledAdditive, true, false },
    { glGetTexture("effects/fx_yellow_glow"), GLB_None, false, true },
    { glGetTexture("effects/fx_electrocution"), GLB_None, true, false },
};

/**
 * Offset/Address/Size: 0x0 | 0x8012A7AC | size: 0x14
 */
EffectsTexturing* fxGetTexturing(eEffectsTextureType type)
{
    return &fxTexturing[type];
}
