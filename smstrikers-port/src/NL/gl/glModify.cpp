#include "NL/gl/glModify.h"
#include "NL/gl/glMemory.h"
#include <string.h>

struct GLModifier
{
    /* 0x00 */ eGLModifier type;
    // PORT: texture and program hashes, not signed values.
    /* 0x04 */ u32 was;
    /* 0x08 */ u32 willBe;
};

static GLModifier glModifier[6];
static u32 glNumModifiers = 0;

static const unsigned long GLTT_Diffuse_bit = 1UL << 0;
static const unsigned long GLTT_Gloss_bit = 1UL << 4;

/**
 * Offset/Address/Size: 0x0 | 0x801D903C | size: 0x30
 */
void gl_ModifyAddMapping(eGLModifier modifier, unsigned long willBe)
{
    glModifier[glNumModifiers].type = modifier;
    glModifier[glNumModifiers].was = -1;
    glModifier[glNumModifiers].willBe = willBe;
    glNumModifiers++;
}

/**
 * Offset/Address/Size: 0x30 | 0x801D906C | size: 0x2C
 */
void gl_ModifyAddMapping(eGLModifier modifier, unsigned long was, unsigned long willBe)
{
    glModifier[glNumModifiers].type = modifier;
    glModifier[glNumModifiers].was = was;
    glModifier[glNumModifiers].willBe = willBe;
    glNumModifiers++;
}

/**
 * Offset/Address/Size: 0x5C | 0x801D9098 | size: 0x1C
 */
void gl_ModifyClearLastMapping()
{
    s32 lastIndex;

    lastIndex = glNumModifiers - 1;
    glNumModifiers = lastIndex;
    if (lastIndex < 0)
    {
        glNumModifiers = 0;
    }
}

/**
 * Offset/Address/Size: 0x78 | 0x801D90B4 | size: 0xC
 */
void gl_ModifyClearMappings()
{
    glNumModifiers = 0;
}

static inline glModelPacket* gl_ModifyClonePacket(const glModelPacket* pPacket)
{
    glModelPacket* packet = (glModelPacket*)glFrameAlloc(sizeof(glModelPacket), GLM_Header);
    memcpy(packet, pPacket, sizeof(glModelPacket));
    return packet;
}

/**
 * Offset/Address/Size: 0x84 | 0x801D90C0 | size: 0x1F8
 */
glModelPacket* gl_Modify(const glModelPacket* pPacket)
{
    glModelPacket* newPacket = NULL;
    s32 i;

    for (i = 0; i < (s32)glNumModifiers; i++)
    {
        switch (glModifier[i].type)
        {
        case GLMod_Program:
            if (pPacket->state.program == glModifier[i].was)
            {
                if (newPacket == NULL)
                {
                    newPacket = gl_ModifyClonePacket(pPacket);
                }
                newPacket->state.program = glModifier[i].willBe;
            }
            break;

        case GLMod_DiffuseTex:
            if ((u32)glModifier[i].was == 0xFFFFFFFF)
            {
                if (newPacket == NULL)
                {
                    newPacket = gl_ModifyClonePacket(pPacket);
                }
                newPacket->state.texture[0] = glModifier[i].willBe;
                newPacket->state.texconfig |= GLTT_Diffuse_bit;
            }
            else if (pPacket->state.texture[0] == glModifier[i].was)
            {
                if (newPacket == NULL)
                {
                    newPacket = gl_ModifyClonePacket(pPacket);
                }
                newPacket->state.texture[0] = glModifier[i].willBe;
                newPacket->state.texconfig |= GLTT_Diffuse_bit;
            }
            break;

        case GLMod_GlossTex:
            if (newPacket == NULL)
            {
                newPacket = gl_ModifyClonePacket(pPacket);
            }
            newPacket->state.texture[4] = glModifier[i].willBe;
            newPacket->state.texconfig |= GLTT_Gloss_bit;
            break;

        case GLMod_TextureMask:
            if (newPacket == NULL)
            {
                newPacket = gl_ModifyClonePacket(pPacket);
            }
            newPacket->state.texconfig &= glModifier[i].willBe;
            break;
        }
    }

    return newPacket;
}

/**
 * Offset/Address/Size: 0x27C | 0x801D92B8 | size: 0x8
 */
u32 gl_ModifyGetNum()
{
    return glNumModifiers;
}
