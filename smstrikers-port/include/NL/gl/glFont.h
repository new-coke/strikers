#ifndef _GLFONT_H_
#define _GLFONT_H_

#include "NL/nlColour.h"
#include "NL/gl/glView.h"

int glFontPrintf(eGLView view, int x, int y, const nlColour& col, const char* format, ...);
int glFontPrintf(eGLView view, int x, int y, const char* format, ...);
int glFontPrint(eGLView view, int virtual_x, int virtual_y, const nlColour& colour, const char* str);
void glFontEnd();
void glFontBegin(bool drop);
void glFontVirtualPosToScreenCoordPos(float x, float y, float& outX, float& outY);
void gl_FontStartup();

#endif // _GLFONT_H_
