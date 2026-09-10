#ifndef _GLRENDERLIST_H_
#define _GLRENDERLIST_H_

#include "NL/gl/glView.h"

struct glModel;
class GLTexturePacketTree;
class GLDepthPacketTree;
class GLPacketList;

class GLRenderList
{
public:
    bool AttachModel(const glModel* pModel, unsigned long layer);
    void AttachPacket(unsigned long layer, const glModelPacket* pPacket);
    void Iterate(eGLView view, void (*cb)(eGLView, unsigned long, const glModelPacket*));
    bool IsEmpty() const;
    void Compact();
    void Clear();

    GLRenderList();
    ~GLRenderList();

    /* 0x00 */ eGLView view;
    /* 0x04 */ eGLViewSort sortMode;

private:
    /* 0x08 */ unsigned long uDepthInsertNumber;
    /* 0x0C */ GLTexturePacketTree* texPacketTree[7];
    /* 0x28 */ GLDepthPacketTree* depthPacketTree;
    /* 0x2C */ GLPacketList* packetList;
};

#endif // _GLRENDERLIST_H_
