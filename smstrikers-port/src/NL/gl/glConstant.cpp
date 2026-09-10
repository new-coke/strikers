#include "NL/gl/glConstant.h"
#include "NL/nlAVLTree.h"
#include "NL/nlString.h"

static int level = 0;

typedef nlAVLTree<unsigned long, nlVector4, DefaultKeyCompare<unsigned long> > ConstantTree;
enum
{
    CONSTANT_TREE_SIZE = sizeof(ConstantTree),
};

static ConstantTree* constants[16] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

static nlVector4 vZero = { 0.0f, 0.0f, 0.0f, 0.0f };

/**
 * Offset/Address/Size: 0x3D4 | 0x801DF5F4 | size: 0x98
 */
void gl_ConstantStartup()
{
    for (int i = 0; i < 16; i++)
    {
        constants[i] = new (nlMalloc(CONSTANT_TREE_SIZE, 8, 0)) ConstantTree;
    }
    level = 0;
}

/**
 * Offset/Address/Size: 0x3C4 | 0x801DF5E4 | size: 0x10
 */
void gl_ConstantMarkerAdvance()
{
    level++;
}

/**
 * Offset/Address/Size: 0x364 | 0x801DF584 | size: 0x60
 */
void gl_ConstantMarkerBackup(int arg)
{
    while (level != arg)
    {
        constants[level]->Clear();
        level--;
    }
}

static inline nlVector4* glConstantFind(unsigned long constantHash)
{
    nlVector4* foundValue;

    for (int i = level; i >= 0; i--)
    {
        if (constants[i]->FindGet(constantHash, &foundValue))
        {
            return foundValue;
        }
    }

    return NULL;
}

static void glConstantSet(unsigned long constantHash, const nlVector4& value)
{
    unsigned long key = constantHash;
    nlVector4* result = glConstantFind(key);

    if (result == NULL)
    {
        ConstantTree* tree = constants[level];

        tree->Add(key, value);
    }
    else
    {
        *result = value;
    }
}

/**
 * Offset/Address/Size: 0x218 | 0x801DF438 | size: 0x14C
 */
void glConstantSet(const char* constantName, const nlVector4& value)
{
    glConstantSet(nlStringHash(constantName), value);
}

static bool glConstantGet(unsigned long constantHash, nlVector4& result)
{
    nlVector4* out = glConstantFind(constantHash);

    if (out == NULL)
    {
        return false;
    }

    result = *out;
    return true;
}

/**
 * Offset/Address/Size: 0x10C | 0x801DF32C | size: 0x10C
 */
bool glConstantGet(const char* constantName, nlVector4& result)
{
    return glConstantGet(nlStringHash(constantName), result);
}

static nlVector4 glConstantGet(unsigned long constantHash)
{
    nlVector4* result = glConstantFind(constantHash);
    return *(result == NULL ? &vZero : result);
}

/**
 * Offset/Address/Size: 0x0 | 0x801DF220 | size: 0x10C
 */
nlVector4 glConstantGet(const char* constantName)
{
    return glConstantGet(nlStringHash(constantName));
}
