#ifndef _GLTARGET_H_
#define _GLTARGET_H_

void glGrabFrameBufferToTexture(unsigned long texture, unsigned int destWidth, unsigned int destHeight, unsigned int srcLeft, unsigned int srcTop, unsigned int srcWidth, unsigned int srcHeight);
void gl_TargetStartup();
#endif // _GLTARGET_H_
