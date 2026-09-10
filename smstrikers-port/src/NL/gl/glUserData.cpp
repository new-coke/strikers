#include "NL/gl/glUserData.h"

// PORT: one pointer per eGLUserData slot. Was a literal 0x48, which is 18 slots at the console's pointer width.
#define GLUD_TABLE_BYTES ((unsigned long)GLUD_Num * sizeof(void*))
#include "NL/gl/glMemory.h"
#include "NL/nlString.h"
#include <string.h>

/**
 * Offset/Address/Size: 0x0 | 0x801DEB78 | size: 0x2C
 */
bool glUserHasType(eGLUserData type, const glModelPacket* pPacket)
{
    void** userdata = (void**)(pPacket->userData);
    if (userdata == NULL)
    {
        return false;
    }

    void* val = userdata[(int)type];
    return (val != NULL) ? true : false;
}

/**
 * Offset/Address/Size: 0x2C | 0x801DEBA4 | size: 0x14
 */
void glUserDetach(eGLUserData type, glModelPacket* pPacket)
{
    void** userdata = (void**)(pPacket->userData);
    userdata[(int)type] = NULL;
}

/**
 * Offset/Address/Size: 0x40 | 0x801DEBB8 | size: 0x88
 */
void glUserDup(glModelPacket* pDest, const glModelPacket* pSrc, bool bPerm)
{
    if (pSrc->userData != 0)
    {
        void* copyBlock;
        if (bPerm)
        {
            copyBlock = glResourceAlloc(GLUD_TABLE_BYTES, GLM_Header);
        }
        else
        {
            copyBlock = glFrameAlloc(GLUD_TABLE_BYTES, GLM_Header);
        }

        memcpy(copyBlock, (void**)pSrc->userData, GLUD_TABLE_BYTES);
        pDest->userData = (uintptr_t)copyBlock;
    }
}

/**
 * Offset/Address/Size: 0xC8 | 0x801DEC40 | size: 0x94
 */
void glUserAttach(const void* pUserData, glModelPacket* pPacket, bool bPerm)
{
    if (pPacket->userData == 0)
    {
        void* block;
        if (bPerm)
        {
            block = glResourceAlloc(GLUD_TABLE_BYTES, GLM_Header);
        }
        else
        {
            block = glFrameAlloc(GLUD_TABLE_BYTES, GLM_Header);
        }

        nlZeroMemory(block, GLUD_TABLE_BYTES);
        pPacket->userData = (uintptr_t)block;
    }

    eGLUserData type = *(eGLUserData*)pUserData;
    ((void**)(pPacket->userData))[(int)type] = (void*)pUserData;
}

/**
 * Offset/Address/Size: 0x15C | 0x801DECD4 | size: 0x8
 */
void* glUserGetData(const void* pUserData)
{
    return (u8*)pUserData + 4;
}

/**
 * Offset/Address/Size: 0x164 | 0x801DECDC | size: 0x58
 */
void* glUserAlloc(eGLUserData type, unsigned long size, bool bPerm)
{
    unsigned long actualSize = size + 4;
    void* mem;

    if (bPerm)
    {
        mem = glResourceAlloc(actualSize, GLM_Header);
    }
    else
    {
        mem = glFrameAlloc(actualSize, GLM_Header);
    }

    if (mem == 0)
    {
        return mem;
    }

    *(u32*)mem = (u32)type;
    return mem;
}
