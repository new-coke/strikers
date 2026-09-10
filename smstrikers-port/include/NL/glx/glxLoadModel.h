#ifndef _GLXLOADMODEL_H_
#define _GLXLOADMODEL_H_

#include "NL/nlMath.h"
#include "NL/gl/glView.h"
#include "NL/nlFile.h"
#include "NL/nlString.h"

class nlChunk;
struct glModel;
class BoneMapList;
class GLSkinMesh;

glModel* glplatEndLoadModel(void* data, unsigned long size, unsigned long* pNumModels);
bool glplatBeginLoadModel(const char* filename, void (*callback)(void*, unsigned long, void*), void* userData);
glModel* glplatLoadModel(const char* filename, unsigned long* pNumModels);
GLSkinMesh* glx_MakeSkinMesh(nlChunk* outerChunk, glModel* models);
void glSetIgnoreDuplicateModels(bool ignore);

#endif // _GLXLOADMODEL_H_
