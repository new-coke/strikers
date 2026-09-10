#ifndef _GLAPPATTACH_H_
#define _GLAPPATTACH_H_

#include "NL/gl/glView.h"
#include "NL/gl/glUserData.h"

class PlatTexture;

extern PlatTexture* ResolvedWhiteTexture;
extern PlatTexture* ResolvedBlackTexture;

void glplatAttachPacket(eGLView view, unsigned long layer, const glModelPacket* pPacket);
glModelPacket* glplatModifyPacket(eGLView view, const glModelPacket* pPacket);
void glAppStartup();
void* glAppGetOnePassFresnelUserData();
void* glAppGetNoFogUserData();
void* glAppGetCoPlanarUserData();
char* gld_ViewName(int view);

#endif // _GLAPPATTACH_H_
