#ifndef _GLMODEL_H_
#define _GLMODEL_H_

#include "NL/gl/glUserData.h"

struct glModel
{
    /* 0x00 */ u32 numPackets;
    /* 0x04 */ u32 id;
    /* 0x08 */ u32 pad;
    /* 0x0C */ glModelPacket* packets;
}; // total size: 0x10

glModel* glModelDupArrayNoStreams(const glModel* pModelArray, unsigned long nModels, bool bUserDup, bool bPermanent);
glModel* glModelDupNoStreams(const glModel* pModel, bool bUserDup, bool bPermanent);
glModel* glModelDup(const glModel* pModel, bool bUserDup);
glModelPacket* glModelPacketDup(const glModelPacket* pPacket, bool bUserDup);

#endif // _GLMODEL_H_
