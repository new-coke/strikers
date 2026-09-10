#include "NL/gl/glModel.h"
#include "NL/gl/glMemory.h"
#include "NL/gl/glUserData.h"
#include "types.h"
#include <string.h>

/**
 * Offset/Address/Size: 0x0 | 0x801DED34 | size: 0x160
 */
glModel* glModelDupArrayNoStreams(const glModel* pModelArray, unsigned long nModels, bool bUserDup, bool bPermanent)
{
    glModel* result;
    glModel* src_model;
    glModel* dst_model;
    u32 i;

    result = bPermanent ? (glModel*)glResourceAlloc(nModels * sizeof(glModel), GLM_Header) : (glModel*)glFrameAlloc(nModels * sizeof(glModel), GLM_Header);

    if (result == NULL)
    {
        return NULL;
    }

    memcpy(result, pModelArray, nModels * sizeof(glModel));

    src_model = (glModel*)pModelArray;
    dst_model = result;
    i = 0;

    while (i < nModels)
    {
        glModelPacket* new_packets;

        if (bPermanent != 0)
        {
            new_packets = (glModelPacket*)glResourceAlloc(src_model->numPackets * sizeof(glModelPacket), GLM_Header);
        }
        else
        {
            new_packets = (glModelPacket*)glFrameAlloc(src_model->numPackets * sizeof(glModelPacket), GLM_Header);
        }

        if (new_packets == NULL)
        {
            return NULL;
        }

        memcpy(new_packets, src_model->packets, src_model->numPackets * sizeof(glModelPacket));
        dst_model->packets = new_packets;

        if (bUserDup != 0)
        {
            glModelPacket* pOut = new_packets;
            glModelPacket* pSrc = src_model->packets;
            glModelPacket* pEnd = pSrc + src_model->numPackets;

            while (pSrc < pEnd)
            {
                if (pSrc->userData != 0)
                {
                    pOut->userData = 0;
                    glUserDup(pOut, pSrc, false);
                }
                pSrc++;
                pOut++;
            }
        }

        src_model++;
        dst_model++;
        i++;
    }

    return result;
}

/**
 * Offset/Address/Size: 0x160 | 0x801DEE94 | size: 0x12C
 */
glModel* glModelDupNoStreams(const glModel* pModel, bool bUserDup, bool bPermanent)
{
    glModel* newModel;
    glModelPacket* newPackets;

    if (bPermanent)
    {
        newModel = (glModel*)glResourceAlloc(sizeof(glModel), GLM_Header);
    }
    else
    {
        newModel = (glModel*)glFrameAlloc(sizeof(glModel), GLM_Header);
    }

    if (newModel == NULL)
    {
        return NULL;
    }

    if (bPermanent)
    {
        newPackets = (glModelPacket*)glResourceAlloc(pModel->numPackets * sizeof(glModelPacket), GLM_Header);
    }
    else
    {
        newPackets = (glModelPacket*)glFrameAlloc(pModel->numPackets * sizeof(glModelPacket), GLM_Header);
    }

    if (newPackets == NULL)
    {
        return NULL;
    }

    memcpy(newModel, pModel, sizeof(glModel));
    memcpy(newPackets, pModel->packets, pModel->numPackets * sizeof(glModelPacket));
    newModel->packets = newPackets;

    if (bUserDup)
    {
        glModelPacket* pOut = newPackets;
        glModelPacket* pSrc = pModel->packets;
        glModelPacket* pEnd = (glModelPacket*)((u8*)pSrc + pModel->numPackets * sizeof(glModelPacket));

        while (pSrc < pEnd)
        {
            if (pSrc->userData != 0)
            {
                pOut->userData = 0;
                glUserDup(pOut, pSrc, false);
            }
            pSrc = (glModelPacket*)((u8*)pSrc + sizeof(glModelPacket));
            pOut = (glModelPacket*)((u8*)pOut + sizeof(glModelPacket));
        }
    }

    return newModel;
}

/**
 * Offset/Address/Size: 0x28C | 0x801DEFC0 | size: 0x114
 */
glModel* glModelDup(const glModel* pModel, bool bUserDup)
{
    glModel* newModel;
    glModelPacket* newPackets;
    glModelPacket* pOut;
    glModelPacket* pSrc;

    newModel = (glModel*)glFrameAlloc(sizeof(glModel), GLM_Header);
    if (newModel == NULL)
    {
        return NULL;
    }

    newPackets = (glModelPacket*)glFrameAlloc(pModel->numPackets * sizeof(glModelPacket), GLM_Header);
    if (newPackets == NULL)
    {
        return NULL;
    }

    memcpy(newModel, pModel, sizeof(glModel));
    memcpy(newPackets, pModel->packets, pModel->numPackets * sizeof(glModelPacket));

    pOut = newPackets;
    pSrc = pModel->packets;
    newModel->packets = newPackets;

    u8* pEnd = (u8*)&pSrc[pModel->numPackets];

    while ((u8*)pSrc < pEnd)
    {
        pOut->streams = (glModelStream*)glFrameAlloc(pOut->numStreams * sizeof(glModelStream), GLM_Header);
        memcpy(pOut->streams, pSrc->streams, pOut->numStreams * sizeof(glModelStream));

        if ((bUserDup != 0) && (pSrc->userData != 0))
        {
            pOut->userData = 0;
            glUserDup(pOut, pSrc, false);
        }

        pSrc++;
        pOut++;
    }

    return newModel;
}

/**
 * Offset/Address/Size: 0x3A0 | 0x801DF0D4 | size: 0x8C
 */
glModelPacket* glModelPacketDup(const glModelPacket* pPacket, bool bUserDup)
{
    glModelPacket* newPacket = (glModelPacket*)glFrameAlloc(sizeof(glModelPacket), GLM_Header);
    if (newPacket != NULL)
    {
        memcpy(newPacket, pPacket, sizeof(glModelPacket));
        if (bUserDup && pPacket->userData != 0)
        {
            newPacket->userData = 0;
            glUserDup(newPacket, pPacket, false);
        }
    }
    return newPacket;
}
