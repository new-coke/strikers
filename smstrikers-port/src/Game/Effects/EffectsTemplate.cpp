#include "Game/Effects/EffectsTemplate.h"
#include "Game/Effects/EmissionController.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/GL/GLInventory.h"
#include "stdlib.h"
#include "NL/nlAVLTree.h"
#include "NL/nlDLListContainer.h"
#include "NL/nlDLRing.h"
#include "NL/nlMath.h"
#include "NL/nlString.h"
#include "NL/gl/glState.h"
#include "NL/glx/glxTexture.h"
#include "Game/Sys/simpleparser.h"

static nlAVLTree<unsigned long, EffectsTemplate*, DefaultKeyCompare<unsigned long> >* pTemplateMap = nullptr;
static unsigned int uSeed = 0x9184EB0C;

struct ColourKey
{
    int index;
    int value;
};

/**
 * Offset/Address/Size: 0x12D8 | 0x801F1E9C | size: 0x24
 */
float RandomizedValue(float value)
{
    return nlRandomf(value, &uSeed);
}

/**
 * Offset/Address/Size: 0x1260 | 0x801F1E24 | size: 0x78
 */
float RandomizedValue(float base, float range)
{
    float randomOffset = nlRandomf(0.5f * range, &uSeed);
    unsigned int randomSign = nlRandom(0x7FFFFFFF, &uSeed);

    if (randomSign & 1)
    {
        return base + randomOffset;
    }
    else
    {
        return base - randomOffset;
    }
}

static inline unsigned char& ColourComponent(nlColour* pColour, int index, int cindex)
{
    return pColour[index].c[cindex];
}

static void BlendSpan(nlColour* pColour, int cindex, const ColourKey& k0, const ColourKey& k1)
{
    ColourComponent(pColour, k1.index, cindex) = k1.value;
    int index = k0.index;
    int value = k0.value;
    int valueDiff = k1.value - value;
    int indexDiff = k1.index - index;
    float current = (float)value;
    float step = (float)valueDiff / (float)indexDiff;
    nlColour* p = pColour + index;
    while (index < k1.index)
    {
        p->c[cindex] = current + 0.00001f;
        index++;
        current += step;
        p++;
    }
}

/**
 * Offset/Address/Size: 0xFD4 | 0x801F1B98 | size: 0x28C
 */
static void GetColourComponent(SimpleParser* parser, nlColour* pColour, int cindex)
{
    typedef DLListContainerBase<ColourKey, NewAdapter<DLListEntry<ColourKey> > > ColourKeyList;
    char ind[8];
    char val[8];
    ColourKey key;
    ColourKeyList keys(0);
    while (true)
    {
        char* token = parser->NextTokenOnLine(true);
        if (token == NULL)
            break;
        char* cp = token;
        int i = 0;
        while (*cp != ':')
        {
            ind[i++] = *cp++;
        }
        ind[i] = 0;
        cp++;
        i = 0;
        while ((char)*cp != 0)
        {
            val[i++] = *cp++;
        }
        val[i] = 0;

        int index = atoi(ind);
        int value = atoi(val);
        key.index = index;
        key.value = value;

        DLListEntry<ColourKey> localEntry(key);
        DLListEntry<ColourKey>* entry = keys.m_Allocator.Allocate();
        entry = new (entry) DLListEntry<ColourKey>(localEntry);
        nlDLRingAddEnd<DLListEntry<ColourKey> >(&keys.m_Head, entry);
    }
    DLListEntry<ColourKey>* current;
    DLListEntry<ColourKey>* start = nlDLRingGetStart<DLListEntry<ColourKey> >(keys.m_Head);
    DLListEntry<ColourKey>* head = keys.m_Head;
    current = start;
    while (true)
    {
        if (nlDLRingIsEnd<DLListEntry<ColourKey> >(head, current) || current == NULL)
            current = NULL;
        else
            current = current->m_next;
        if (current == NULL)
            break;
        BlendSpan(pColour, cindex, start->entry, current->entry);
        start->entry.index = current->entry.index;
        start->entry.value = current->entry.value;
    }
}

static inline float GetFloat(SimpleParser* parser)
{
    char* token = parser->NextToken(true);
    return atof(token);
}

static inline fxRange GetRange(SimpleParser* parser)
{
    fxRange value;
    value.base = atof(parser->NextTokenOnLine(true));
    value.range = atof(parser->NextTokenOnLine(true));
    return value;
}

static inline void InterpolateColours(EffectsTemplate* dest, const nlColour* source, int numDestColours)
{
    for (int i = 0; i < numDestColours; i++)
    {
        nlColour a = source[i * 2];
        nlColour b = source[(i * 2) + 1];
        nlColour c;

        c.c[0] = (a.c[0] + b.c[0]) / 2;
        c.c[1] = (a.c[1] + b.c[1]) / 2;
        c.c[2] = (a.c[2] + b.c[2]) / 2;
        c.c[3] = (a.c[3] + b.c[3]) / 2;
        dest->m_cColour[i] = c;
    }
}

/**
 * Offset/Address/Size: 0x2A8 | 0x801F0E6C | size: 0xD2C
 */
static EffectsTemplate* parse_template(SimpleParser* parser, bool bQuick)
{
    char name[128];
    EffectsTemplate t;
    char* token;
    nlColour colours[50];
    bool bFountain;
    bool bFountainForever;
    bool bValue;
    char texname[264];
    char modelname[264];

    nlZeroMemory(&t, sizeof(EffectsTemplate));
    t.m_hTexture = (u32)-1;
    t.m_uModelID = (u32)-1;

    if (bQuick)
    {
        t.m_uHashID = nlStringHash("quickeffect");
        nlStrNCpy<char>(name, "quickeffect", 0x80);
    }
    else
    {
        token = parser->NextToken(true);
        if (token == NULL)
        {
            return NULL;
        }

        t.m_uHashID = nlStringHash(token);
        nlStrNCpy<char>(name, token, 0x80);
    }

    bFountain = true;
    bFountainForever = false;

    while (true)
    {
        token = parser->NextToken(true);
        if (token == NULL)
        {
            if (!bQuick)
            {
                return NULL;
            }
            break;
        }

        if (nlStrCmp<char>(token, "end") == 0)
        {
            break;
        }

        if (nlStrCmp<char>(token, "fountainlife") == 0)
        {
            t.m_fFountainLife = GetFloat(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "fountainmode") == 0)
        {
            bValue = (nlStrCmp<char>(parser->NextToken(true), "true") == 0);
            bFountain = bValue;
            continue;
        }

        if (nlStrCmp<char>(token, "fountainforever") == 0)
        {
            bValue = (nlStrCmp<char>(parser->NextToken(true), "true") == 0);
            bFountainForever = bValue;
            continue;
        }

        if (nlStrCmp<char>(token, "number") == 0)
        {
            t.m_rNumber = GetRange(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "bounce") == 0)
        {
            t.m_bBounce = (nlStrCmp<char>(parser->NextToken(true), "true") == 0);
            continue;
        }

        if (nlStrCmp<char>(token, "meshlighting") == 0)
        {
            t.m_bLit = (nlStrCmp<char>(parser->NextToken(true), "true") == 0);
            continue;
        }

        if (nlStrCmp<char>(token, "infront") == 0)
        {
            t.m_bInFront = (nlStrCmp<char>(parser->NextToken(true), "true") == 0);
            continue;
        }

        if (nlStrCmp<char>(token, "sizebegin") == 0)
        {
            t.m_rSizeBegin = GetRange(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "sizeend") == 0)
        {
            t.m_rSizeEnd = GetRange(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "mass") == 0)
        {
            t.m_rMass = GetRange(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "particlelife") == 0)
        {
            t.m_rParticleLife = GetRange(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "rotation") == 0)
        {
            t.m_rRotation = GetRange(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "radius") == 0)
        {
            t.m_rRadius = GetRange(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "inheritvelocity") == 0)
        {
            t.m_rInheritVelocity = GetRange(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "velocity") == 0)
        {
            t.m_rVelocity = GetRange(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "acceleration") == 0)
        {
            t.m_rAcceleration = GetRange(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "particlefps") == 0)
        {
            t.m_rFPS = GetRange(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "angle") == 0)
        {
            t.m_rAngle = GetRange(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "tilt") == 0)
        {
            t.m_rTilt = GetRange(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "texturenumframes") == 0)
        {
            t.m_nFrames = (s32)GetFloat(parser);
            continue;
        }

        if (nlStrCmp<char>(token, "emitter") == 0)
        {
            token = parser->NextToken(true);
            if (nlStrCmp<char>(token, "circle") == 0)
            {
                t.m_eEmitter = Emitter_Circle;
                continue;
            }

            if (nlStrCmp<char>(token, "sphere") == 0)
            {
                t.m_eEmitter = Emitter_Sphere;
                continue;
            }

            if (nlStrCmp<char>(token, "spindle") == 0)
            {
                t.m_eEmitter = Emitter_Spindle;
                continue;
            }

            if (nlStrCmp<char>(token, "hemisphere") == 0)
            {
                t.m_eEmitter = Emitter_Hemisphere;
                continue;
            }

            EmissionManager::AddError("%s : unknown emitter type '%s'\n", name, token);
            continue;
        }

        if (nlStrCmp<char>(token, "blendmode") == 0)
        {
            token = parser->NextToken(true);
            if (nlStrCmp<char>(token, "normal") == 0)
            {
                t.m_eBlend = EfBlend_Normal;
                continue;
            }

            if (nlStrCmp<char>(token, "additive") == 0)
            {
                t.m_eBlend = EfBlend_Additive;
                continue;
            }

            EmissionManager::AddError("%s : unknown blend type '%s'\n", name, token);
            continue;
        }

        if (nlStrCmp<char>(token, "billboardmode") == 0)
        {
            token = parser->NextToken(true);
            if (nlStrCmp<char>(token, "billboard") == 0)
            {
                t.m_eBillboard = EfBill_Billboard;
                continue;
            }

            if (nlStrCmp<char>(token, "groundboard") == 0)
            {
                t.m_eBillboard = EfBill_Groundboard;
                continue;
            }

            if (nlStrCmp<char>(token, "softwarecontrolled") == 0)
            {
                t.m_eBillboard = EfBill_SoftwareControlled;
                continue;
            }

            EmissionManager::AddError("%s : unknown billboard type '%s'\n", name, token);
            continue;
        }

        if (nlStrCmp<char>(token, "texture") == 0)
        {
            token = parser->NextToken(true);
            nlStrNCat<char>(texname, "effects/", token, 0x100);
            t.m_hTexture = glGetTexture(texname);
            t.m_hTexture = (uintptr_t)glx_GetTex(t.m_hTexture, true, true);
            continue;
        }

        if (nlStrCmp<char>(token, "model") == 0)
        {
            token = parser->NextTokenOnLine(true);
            if (token == NULL)
            {
                t.m_uModelID = (u32)-1;
                continue;
            }

            nlStrNCat<char>(modelname, "effects/", token, 0x100);
            t.m_uModelID = nlStringHash(modelname);
            continue;
        }

        if (nlStrCmp<char>(token, "matchmodelfpstoparticlelife") == 0)
        {
            t.m_bMatchLifespan = (nlStrCmp<char>(parser->NextToken(true), "true") == 0);
            continue;
        }

        if (nlStrCmp<char>(token, "follow") == 0)
        {
            t.m_bLocalSpace = (nlStrCmp<char>(parser->NextToken(true), "true") == 0);
            continue;
        }

        if (nlStrCmp<char>(token, "red") == 0)
        {
            GetColourComponent(parser, colours, 0);
            continue;
        }

        if (nlStrCmp<char>(token, "green") == 0)
        {
            GetColourComponent(parser, colours, 1);
            continue;
        }

        if (nlStrCmp<char>(token, "blue") == 0)
        {
            GetColourComponent(parser, colours, 2);
            continue;
        }

        if (nlStrCmp<char>(token, "alpha") == 0)
        {
            GetColourComponent(parser, colours, 3);
            continue;
        }

        if (nlStrCmp<char>(token, "link") == 0)
        {
            while (token != NULL)
            {
                token = parser->NextTokenOnLine(true);
            }
            continue;
        }

        EmissionManager::AddError("%s : unknown token in a template '%s'\n", name, token);
    }

    if (!bFountain)
    {
        t.m_fFountainLife = 0.0f;
    }

    if (bFountain && bFountainForever)
    {
        t.m_fFountainLife = 10000000000.0f;
    }

    t.m_rInheritVelocity.base /= 100.0f;
    t.m_rInheritVelocity.range /= 100.0f;

    if (t.m_nFrames == 0)
    {
        t.m_nFrames = 1;
    }

    if (glInventory.GetModel(t.m_uModelID) == NULL)
    {
        t.m_uModelID = (u32)-1;
    }

    InterpolateColours(&t, colours, 25);

    EffectsTemplate* out = (EffectsTemplate*)nlMalloc(sizeof(EffectsTemplate), 8, false);
    memcpy(out, &t, sizeof(EffectsTemplate));
    return out;
}

static inline void AddTemplateNoCollisions(EffectsTemplate* p)
{
    pTemplateMap->Add(p->m_uHashID, p);
}

/**
 * Offset/Address/Size: 0x27C | 0x801F0E40 | size: 0x2C
 */
bool fxLoadTemplateBundle(const char* filename)
{
    unsigned long fileSize;
    void* data = fxLoadEntireFileHigh(filename, &fileSize);
    return fxLoadTemplateBundle(data, fileSize);
}

/**
 * Offset/Address/Size: 0x13C | 0x801F0D00 | size: 0x140
 */
bool fxLoadTemplateBundle(void* data, unsigned long size)
{
    if (data == nullptr)
    {
        return false;
    }

    pTemplateMap = new (nlMalloc(sizeof(nlAVLTree<unsigned long, EffectsTemplate*, DefaultKeyCompare<unsigned long> >), 8, false)) nlAVLTree<unsigned long, EffectsTemplate*, DefaultKeyCompare<unsigned long> >();

    SimpleParser parser;
    parser.StartParsing((char*)data, size, true);

    for (;;)
    {
        char* token = parser.NextToken(true);
        if (token == nullptr)
        {
            break;
        }

        if (nlStrCmp<char>(token, "begin") == 0)
        {
            EffectsTemplate* pTemplate = parse_template(&parser, false);
            AddTemplateNoCollisions(pTemplate);
        }
        else
        {
            EmissionManager::AddError("EffectsTemplate: unrecognized token '%s'\n", token);
        }
    }

    nlFree(data);
    return true;
}

/**
 * Offset/Address/Size: 0x90 | 0x801F0C54 | size: 0x4C
 */
bool fxUnloadTemplates()
{
    if (pTemplateMap == nullptr)
    {
        return true;
    }

    pTemplateMap->DeleteValues();
    delete pTemplateMap;
    pTemplateMap = nullptr;
    return true;
}

/**
 * Offset/Address/Size: 0x0 | 0x801F0BC4 | size: 0x90
 */
EffectsTemplate* fxGetTemplate(unsigned long hashID)
{
    EffectsTemplate** theTemplate;
    if (!pTemplateMap->FindGet(hashID, &theTemplate))
    {
        return nullptr;
    }
    return *theTemplate;
}
