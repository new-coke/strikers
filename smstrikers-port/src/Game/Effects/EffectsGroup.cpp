#include "Game/Effects/EffectsGroup.h"
#include "Game/Effects/EmissionController.h"
#include "Game/Effects/EmissionManager.h"
#include "NL/nlAVLTree.h"
#include "NL/nlDLListContainer.h"
#include "NL/nlDLRing.h"
#include "NL/nlMain.h"
#include "NL/nlString.h"

static nlAVLTree<unsigned long, EffectsGroup*, DefaultKeyCompare<unsigned long> >* pGroupMap = nullptr;
static nlAVLTree<unsigned long, EffectsTerrainSpec*, DefaultKeyCompare<unsigned long> >* pTerrainSpecMap = nullptr;

extern "C" int atoi(const char*);

static class UserEffectFactory* gUserEffectTypes[3];
static char last_spec_name[0x100];
static s32 gnUserEffectTypes;

class UserEffectFactory
{
public:
    virtual ~UserEffectFactory();
    virtual UserEffectSpec* ParseSpec(SimpleParser* parser);
    virtual const char* GetName();
};

struct EffectsSpecRaw
{
    u32 m_uHashID;
    EffectsTemplate* m_pTemplate;
    eFXBinding m_eAttach;
    u32 m_uJointID;
    f32 m_fDelay;
    u32 m_uLayer;
    eJointBinding m_eJointBinding;
    f32 m_fJointVelocity;
    u8 m_bInFront;
    u8 m_bGround;
    u8 m_bLight;
    u8 _pad23;
    f32 m_fOffset;
    nlVector3 m_vLocalOffset;
    EffectsTerrainSpec* m_pTerrainSpec;
    f32 m_fLingerStart;
    f32 m_fLingerEnd;
};

static void Tweak_AddScriptEffect(const char*)
{
}

EffectsTerrainSpec::EffectsTerrainSpec()
{
    m_pTerrainIDs = nullptr;
    m_uNumTerrains = 0;
}

EffectsTerrainSpec::~EffectsTerrainSpec()
{
    if (m_pTerrainIDs != nullptr)
    {
        delete[] m_pTerrainIDs;
        m_pTerrainIDs = nullptr;
    }
}

unsigned long EffectsTerrainSpec::GetHashID() const
{
    RunningChecksum checksum;
    checksum.ChecksumInt(m_uNumTerrains);
    checksum.ChecksumData(m_pTerrainIDs, m_uNumTerrains * sizeof(unsigned long));
    return ~checksum.m_nChecksum;
}

/**
 * Offset/Address/Size: 0xFC8 | 0x801F3A10 | size: 0x38
 */
bool EffectsTerrainSpec::HasTerrain(unsigned long terrainID) const
{
    for (u32 i = 0; i < m_uNumTerrains; i++)
    {
        if (m_pTerrainIDs[i] == terrainID)
        {
            return true;
        }
    }

    return false;
}

static EffectsTerrainSpec* fxGetTerrainSpec(unsigned long hashID)
{
    EffectsTerrainSpec** terrainSpec;
    if (pTerrainSpecMap->FindGet(hashID, &terrainSpec))
    {
        return *terrainSpec;
    }
    return nullptr;
}

static void AddTerrainSpec(EffectsTerrainSpec* spec)
{
    unsigned long hashID = spec->GetHashID();
    pTerrainSpecMap->Add(hashID, spec);
}

EffectsGroup::EffectsGroup()
{
    m_hashID = 0;
    m_specs = nullptr;
    m_numSpecs = 0;
    m_userSpecsPtr = nullptr;
    m_userSpecs = 0;
    m_isLingering = false;
}

EffectsGroup::~EffectsGroup()
{
    if (m_specs != nullptr)
    {
        if (m_specs != nullptr)
        {
            ::operator delete[]((char*)m_specs - 0x10);
        }
    }
    if (m_userSpecs != 0)
    {
        for (int i = 0; i < m_userSpecs; i++)
        {
            delete m_userSpecsPtr[i];
            m_userSpecsPtr[i] = nullptr;
        }
        delete[] m_userSpecsPtr;
        m_userSpecsPtr = nullptr;
    }
}

bool EffectsGroup::IsPersistent() const
{
    return m_isLingering || m_userSpecsPtr != nullptr;
}

void EffectsGroup::SetUserSpecs(int numSpecs, UserEffectSpec** specs)
{
    if (numSpecs > 0)
    {
        m_userSpecs = numSpecs;
        m_userSpecsPtr = specs;
    }
}

void EffectsGroup::SetSpecs(int numSpecs, EffectsSpec* specs)
{
    m_specs = specs;
    m_numSpecs = numSpecs;

    for (int i = 0; i < m_numSpecs; i++)
    {
        if (m_specs[i].m_fLingerStart >= 0.0f)
        {
            m_isLingering = true;
            break;
        }
    }
}

static unsigned long GetJointID(const char* shortName)
{
    char jointName[128];
    nlStrNCat<char>(jointName, "bip01 ", shortName, 0x80);

    char* cp = jointName;
    while (*cp != '\0')
    {
        if (*cp == '_')
        {
            *cp = ' ';
        }
        cp++;
    }

    return nlStringLowerHash(jointName);
}

/**
 * Offset/Address/Size: 0xF70 | 0x801F39B8 | size: 0x58
 */
EffectsSpec::EffectsSpec()
{
    m_uHashID = 0;
    m_pTemplate = nullptr;
    m_eAttach = FXBind_Emitter;
    m_uJointID = 0;
    m_fDelay = 0.0f;
    m_uLayer = 0;
    m_eJointBinding = JB_Normal;
    m_fJointVelocity = 0.0f;
    m_bInFront = false;
    m_bGround = false;
    m_bLight = false;
    m_fOffset = 0.0f;
    m_pTerrainSpec = nullptr;
    m_fLingerStart = -1.0f;
    m_fLingerEnd = -1.0f;
    m_vLocalOffset.x = 0.0f;
    m_vLocalOffset.y = 0.0f;
    m_vLocalOffset.z = 0.0f;
}

struct EffectsSpecShadow
{
    u32 m_uHashID;
    EffectsTemplate* m_pTemplate;
    eFXBinding m_eAttach;
    u32 m_uJointID;
    f32 m_fDelay;
    u32 m_uLayer;
    eJointBinding m_eJointBinding;
    f32 m_fJointVelocity;
    u8 m_bInFront;
    u8 m_bGround;
    u8 m_bLight;
    u8 _pad23;
    f32 m_fOffset;
    nlVector3 m_vLocalOffset;
    EffectsTerrainSpec* m_pTerrainSpec;
    f32 m_fLingerStart;
    f32 m_fLingerEnd;
};

/**
 * Offset/Address/Size: 0xA30 | 0x801F3478 | size: 0x540
 */
bool parse_spec(SimpleParser* parser, EffectsSpec& spec)
{
    char* token;
    char* nextToken = nullptr;
    EffectsSpecShadow init;
    EffectsSpecShadow* initAlias = &init;

    init.m_vLocalOffset.x = 0.0f;
    init.m_vLocalOffset.y = 0.0f;
    init.m_vLocalOffset.z = 0.0f;

    spec.m_uHashID = 0;
    spec.m_pTemplate = nullptr;
    spec.m_eAttach = FXBind_Emitter;
    spec.m_uJointID = 0;
    spec.m_fDelay = 0.0f;
    spec.m_uLayer = 0;
    spec.m_eJointBinding = JB_Normal;
    spec.m_fJointVelocity = 0.0f;
    spec.m_bInFront = false;
    spec.m_bGround = false;
    spec.m_bLight = false;
    spec.m_fOffset = 0.0f;
    spec.m_vLocalOffset = init.m_vLocalOffset;
    spec.m_pTerrainSpec = nullptr;
    spec.m_fLingerStart = -1.0f;

    init.m_eAttach = FXBind_Emitter;
    init.m_uJointID = 0;
    init.m_fDelay = 0.0f;
    init.m_uLayer = 0;
    init.m_eJointBinding = JB_Normal;
    init.m_fJointVelocity = 0.0f;
    init.m_bInFront = false;
    init.m_bGround = false;
    init.m_bLight = false;
    init.m_fOffset = 0.0f;
    init.m_pTerrainSpec = nullptr;
    init.m_fLingerStart = -1.0f;
    init.m_fLingerEnd = -1.0f;

    spec.m_fLingerEnd = -1.0f;

    token = parser->NextToken(true);
    if (token == nullptr)
    {
        return false;
    }

    nlStrNCpy<char>(last_spec_name, token, 0x100);

    spec.m_pTemplate = fxGetTemplate(nlStringHash(token));
    if (spec.m_pTemplate == nullptr)
    {
        EmissionManager::AddError("parse_group couldn't find template '%s'\n", last_spec_name);
        return false;
    }

    while (true)
    {
        if (nextToken == nullptr)
        {
            token = parser->NextTokenOnLine(true);
        }
        else
        {
            token = nextToken;
        }

        nextToken = nullptr;

        if (token == nullptr)
        {
            break;
        }

        if (nlStrCmp<char>(token, "linger_start") == 0)
        {
            f32 value = atof(parser->NextTokenOnLine(true));
            spec.m_fLingerEnd = value;
            spec.m_fLingerStart = value;
            continue;
        }

        if (nlStrCmp<char>(token, "linger_end") == 0)
        {
            spec.m_fLingerEnd = atof(parser->NextTokenOnLine(true));
            continue;
        }

        if (nlStrCmp<char>(token, "at") == 0 || nlStrCmp<char>(token, "on") == 0)
        {
            token = parser->NextTokenOnLine(true);

            if (nlStrCmp<char>(token, "emitter") == 0)
            {
                spec.m_eAttach = FXBind_Emitter;
                continue;
            }

            if (nlStrCmp<char>(token, "joint") == 0)
            {
                char* jointToken;

                jointToken = parser->NextTokenOnLine(true);
                spec.m_eAttach = FXBind_Joint;
                spec.m_uJointID = GetJointID(jointToken);

                token = parser->NextTokenOnLine(true);
                if (token == nullptr)
                {
                    continue;
                }

                if (nlStrCmp<char>(token, "ascend") == 0)
                {
                    spec.m_fJointVelocity = 1.0f;
                    spec.m_eJointBinding = JB_Ascend;

                    token = parser->NextTokenOnLine(true);
                    if (token != nullptr)
                    {
                        spec.m_fJointVelocity = atof(token);
                    }
                    continue;
                }

                nextToken = token;
                continue;
            }

            if (nlStrCmp<char>(token, "object") == 0 || nlStrCmp<char>(token, "ball") == 0 || nlStrCmp<char>(token, "puck") == 0)
            {
                spec.m_eAttach = FXBind_Object;
                continue;
            }

            EmissionManager::AddError("parse_spec: unknown fx binding '%s'\n", token);
            continue;
        }

        if (nlStrCmp<char>(token, "layer") == 0)
        {
            spec.m_uLayer = atoi(parser->NextTokenOnLine(true));
            continue;
        }

        if (nlStrCmp<char>(token, "infront") == 0)
        {
            spec.m_bInFront = true;
            continue;
        }

        if (nlStrCmp<char>(token, "delay") == 0)
        {
            spec.m_fDelay = atof(parser->NextTokenOnLine(true));
            continue;
        }

        if (nlStrCmp<char>(token, "ground") == 0)
        {
            spec.m_bGround = true;
            continue;
        }

        if (nlStrCmp<char>(token, "offset") == 0)
        {
            spec.m_fOffset = atof(parser->NextTokenOnLine(true));
            continue;
        }

        if (nlStrCmp<char>(token, "light") == 0)
        {
            spec.m_bLight = true;
            continue;
        }

        if (nlStrCmp<char>(token, "offsetx") == 0)
        {
            spec.m_vLocalOffset.x = atof(parser->NextTokenOnLine(true));
            continue;
        }

        if (nlStrCmp<char>(token, "offsety") == 0)
        {
            spec.m_vLocalOffset.y = atof(parser->NextTokenOnLine(true));
            continue;
        }

        if (nlStrCmp<char>(token, "offsetz") == 0)
        {
            spec.m_vLocalOffset.z = atof(parser->NextTokenOnLine(true));
            continue;
        }

        if (nlStrCmp<char>(token, "offsetxyz") == 0)
        {
            for (s32 i = 0; i < 3; i++)
            {
                (&spec.m_vLocalOffset.x)[i] = atof(parser->NextTokenOnLine(true));
            }
            continue;
        }

        EmissionManager::AddError("parse_spec has an unrecognized token '%s'\n", token);
    }

    return true;
}

/**
 * Offset/Address/Size: 0x80C | 0x801F3254 | size: 0x224
 */
EffectsTerrainSpec* parse_terrain_spec(SimpleParser* parser)
{
    unsigned long terrainIDs[256];
    unsigned long numTerrains = 0;
    char* token;

    while (true)
    {
        token = parser->NextTokenOnLine(true);
        if (token == nullptr)
        {
            break;
        }

        terrainIDs[numTerrains++] = nlStringLowerHash(token);
    }

    EffectsTerrainSpec* pSpec = new (nlMalloc(sizeof(EffectsTerrainSpec), 8, false)) EffectsTerrainSpec;

    pSpec->m_uNumTerrains = numTerrains;
    pSpec->m_pTerrainIDs = (unsigned long*)nlMalloc(numTerrains * sizeof(unsigned long), 8, false);
    memcpy(pSpec->m_pTerrainIDs, terrainIDs, numTerrains << 2);

    EffectsTerrainSpec* existingSpec = fxGetTerrainSpec(pSpec->GetHashID());

    if (existingSpec == nullptr)
    {
        AddTerrainSpec(pSpec);
        return pSpec;
    }

    delete pSpec;

    return existingSpec;
}

static inline DLListEntry<UserEffectSpec*>* InitUserSpecWalk(
    DLListEntry<UserEffectSpec*>* pNode,
    nlDLListContainer<UserEffectSpec*>& userSpecs,
    DLListEntry<UserEffectSpec*>*& pHead,
    UserEffectSpec**& pWalk,
    UserEffectSpec** pUserSpecs)
{
    pHead = userSpecs.m_Head;
    pWalk = pUserSpecs;
    return pNode;
}

/**
 * Offset/Address/Size: 0x338 | 0x801F2D80 | size: 0x4D4
 */
static EffectsGroup* parse_group(SimpleParser* parser)
{
    unsigned long hashID;
    EffectsGroup* pGroup;
    EffectsSpec specs[64];
    unsigned long specCount;
    EffectsSpecRaw spec;
    EffectsTerrainSpec* pTerrainSpec;
    int i;
    EffectsSpec* pSpecs;
    unsigned long specOffset;
    char* token;

    spec.m_uHashID = 0;
    spec.m_pTemplate = nullptr;
    spec.m_eAttach = FXBind_Emitter;
    spec.m_uJointID = 0;
    spec.m_fDelay = 0.0f;
    spec.m_uLayer = 0;
    spec.m_eJointBinding = JB_Normal;
    spec.m_fJointVelocity = 0.0f;
    spec.m_bInFront = false;
    spec.m_bGround = false;
    spec.m_bLight = false;
    spec.m_fOffset = 0.0f;
    spec.m_pTerrainSpec = nullptr;
    spec.m_fLingerStart = -1.0f;
    spec.m_fLingerEnd = -1.0f;
    spec.m_vLocalOffset.x = 0.0f;
    spec.m_vLocalOffset.y = 0.0f;
    spec.m_vLocalOffset.z = 0.0f;

    pTerrainSpec = nullptr;
    nlDLListContainer<UserEffectSpec*> userSpecs;

    token = parser->NextToken(true);
    if (token == nullptr)
    {
        return nullptr;
    }

    hashID = nlStringHash(token);
    specCount = 0;
    specOffset = 0;

    while (true)
    {
        token = parser->NextToken(true);
        if (token == nullptr)
        {
            return nullptr;
        }

        if (nlStrCmp<char>(token, "end") == 0)
        {
            if (pTerrainSpec == nullptr)
            {
                break;
            }

            pTerrainSpec = nullptr;
            continue;
        }

        if (nlStrCmp<char>(token, "play") == 0)
        {
            if (!parse_spec(parser, *(EffectsSpec*)&spec))
            {
                continue;
            }

            spec.m_pTerrainSpec = pTerrainSpec;
            // PORT: index by element, `>> 6` is the on-console sizeof.
            specs[specCount] = *(EffectsSpec*)&spec;
            specCount++;
            specOffset += sizeof(EffectsSpec);
            continue;
        }

        if (nlStrCmp<char>(token, "terrain") == 0)
        {
            pTerrainSpec = parse_terrain_spec(parser);
            continue;
        }

        for (i = 0; i < gnUserEffectTypes; i++)
        {
            if (nlStrCmp<char>(gUserEffectTypes[i]->GetName(), token) == 0)
            {
                UserEffectSpec* pUserSpec = gUserEffectTypes[i]->ParseSpec(parser);
                // PORT: 0xC is the console's three 4-byte pointers.
                void* mem = nlMalloc(sizeof(DLListEntry<UserEffectSpec*>), 8, false);
                DLListEntry<UserEffectSpec*>* pSpecEntry = (DLListEntry<UserEffectSpec*>*)mem;
                if (mem != nullptr)
                {
                    ((DLListEntry<UserEffectSpec*>*)mem)->m_next = nullptr;
                    ((DLListEntry<UserEffectSpec*>*)mem)->m_prev = nullptr;
                    ((DLListEntry<UserEffectSpec*>*)mem)->entry = pUserSpec;
                }

                nlDLRingAddEnd(&userSpecs.m_Head, pSpecEntry);
                break;
            }
        }

        if (i == gnUserEffectTypes)
        {
            EmissionManager::AddError("EffectsGroup::parse_group unsupported token '%s'\n", token);
        }
    }

    if (specCount == 0)
    {
        pSpecs = nullptr;
    }
    else
    {
        // PORT: sixteen bytes in, where the console's array cookie put it, so the `- 0x10` in ~EffectsGroup recovers the block.
        pSpecs = new ((char*)nlMalloc(specCount * sizeof(EffectsSpec) + 0x10, 8, false) + 0x10) EffectsSpec[specCount];
        memcpy(pSpecs, specs, specCount * sizeof(EffectsSpec));
    }

    pGroup = new (nlMalloc(sizeof(EffectsGroup), 8, false)) EffectsGroup;

    pGroup->m_hashID = hashID;
    pGroup->SetSpecs(specCount, pSpecs);

    i = nlDLRingCountElements(userSpecs.m_Head);
    if (i > 0)
    {
        UserEffectSpec** pUserSpecs = new (8, false) UserEffectSpec*[i];
        DLListEntry<UserEffectSpec*>* pHead;
        DLListEntry<UserEffectSpec*>* pNode;
        UserEffectSpec** pWalk;
        pNode = InitUserSpecWalk(
            nlDLRingGetStart(userSpecs.m_Head), userSpecs, pHead, pWalk, pUserSpecs);

        while (pNode != nullptr)
        {
            *pWalk = pNode->entry;
            pWalk++;

            if (nlDLRingIsEnd(pHead, pNode) || pNode == nullptr)
            {
                pNode = nullptr;
            }
            else
            {
                pNode = pNode->m_next;
            }
        }

        pGroup->SetUserSpecs(i, pUserSpecs);
    }

    return pGroup;
}

static void AddGroupNoCollisions(EffectsGroup* group)
{
    pGroupMap->Add(group->GetHashID(), group);
}

/**
 * Offset/Address/Size: 0x30C | 0x801F2D54 | size: 0x2C
 */
bool fxLoadGroupBundle(const char* filename)
{
    unsigned long fileSize;
    void* data = fxLoadEntireFileHigh(filename, &fileSize);
    return fxLoadGroupBundle(data, fileSize);
}

/**
 * Offset/Address/Size: 0x178 | 0x801F2BC0 | size: 0x194
 */
bool fxLoadGroupBundle(void* data, unsigned long size)
{
    if (data == nullptr)
    {
        return false;
    }

    pGroupMap = new (nlMalloc(sizeof(nlAVLTree<unsigned long, EffectsGroup*, DefaultKeyCompare<unsigned long> >), 8, false))
        nlAVLTree<unsigned long, EffectsGroup*, DefaultKeyCompare<unsigned long> >();
    pTerrainSpecMap = new (nlMalloc(sizeof(nlAVLTree<unsigned long, EffectsTerrainSpec*, DefaultKeyCompare<unsigned long> >), 8, false))
        nlAVLTree<unsigned long, EffectsTerrainSpec*, DefaultKeyCompare<unsigned long> >();

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
            EffectsGroup* group;
            group = parse_group(&parser);
            AddGroupNoCollisions(group);
        }
        else
        {
            EmissionManager::AddError("EffectsGroup: unrecognized token '%s'\n", token);
        }
    }

    nlFree(data);
    return true;
}

/**
 * Offset/Address/Size: 0xA4 | 0x801F2AEC | size: 0x74
 */
bool fxUnloadGroups()
{
    if ((pGroupMap == NULL) && (pTerrainSpecMap == NULL))
    {
        return true;
    }

    pGroupMap->DeleteValues();
    delete pGroupMap;
    pGroupMap = nullptr;

    pTerrainSpecMap->DeleteValues();
    delete pTerrainSpecMap;
    pTerrainSpecMap = nullptr;

    return true;
}

static EffectsGroup* fxGetGroup(unsigned long hashID)
{
    EffectsGroup** group;
    return pGroupMap->FindGet(hashID, &group) ? *group : nullptr;
}

/**
 * Offset/Address/Size: 0x0 | 0x801F2A48 | size: 0xA4
 */
EffectsGroup* fxGetGroup(const char* groupName)
{
    return fxGetGroup(nlStringHash(groupName));
}

static void fxRegisterUserEffect(UserEffectFactory* factory)
{
    gUserEffectTypes[gnUserEffectTypes++] = factory;
}
