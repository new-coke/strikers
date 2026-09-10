#include "Game/FE/FELoader.h"

#include "NL/gl/gl.h"
#include "Game/FE/feManager.h"

FELoader TheFELoader;

/**
 * Offset/Address/Size: 0x0 | 0x80094C44 | size: 0x30
 */
bool FELoader::StartLoad(LoadingManager* manager)
{
    glLoadTextureBundle("fe/fe.glt");
    FrontEnd::Initialize();
    return true;
}
