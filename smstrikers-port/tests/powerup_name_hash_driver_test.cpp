#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

// Compile the real DrawablePowerup methods with narrow world/render fixtures.
// The hash oracle is the real NL/nlString.cpp, compiled separately under a
// renamed export; no production hash algorithm is reproduced in this driver.
using u32 = std::uint32_t;
using s16 = std::int16_t;
using s8 = std::int8_t;
using u16 = std::uint16_t;
u32 powerup_test_original_lower_hash(const char* text);

static unsigned hashCalls;
u32 nlStringLowerHash(const char* text)
{
    ++hashCalls;
    return powerup_test_original_lower_hash(text);
}

extern "C" void DCZeroRange(void* data, u32 bytes)
{
    std::memset(data, 0, bytes);
}

#define _DRAWABLEPOWERUP_H_
#define _DRAWABLEMODEL_H_
#define _WORLDMANAGER_H_
#define _POWERUPS_H_
#define _GLDRAW3_H_
#define _GLSTATE_H_
#define _NLSTRING_H_

struct nlVector3
{
    float x, y, z;
};
struct nlVector2
{
    float x, y;
};
struct nlQuaternion
{
    float x, y, z, w;
};
struct SaveFrame {};
struct LoadFrame {};
class PowerupBase;
class DrawablePowerup
{
public:
    template <typename T> void Replay(T&) {}
    void Blend(const float*, const DrawablePowerup&, const DrawablePowerup&);
    void Render(int) const;
    void Grab(int);
    PowerupBase* GetPowerup(int) const;

    s8 mType;
    float mScale, mRadius;
    bool mVisible;
    nlVector3 mPosition;
    u16 mOrientation;
};

static unsigned radiusCalls, quaternionCalls, quadShadows;
class PowerupBase
{
public:
    s8 m_eType;
    float radius;
    u16 m_aOrientation;
    nlVector3 m_v3Position;
    float m_scale;
    float GetRadius() { ++radiusCalls; return radius; }
};
static PowerupBase* powerupResult;
static unsigned long powerupKey, lastPowerupKey;
static unsigned powerupLookups;
PowerupBase* FindPowerUp(unsigned long key)
{
    ++powerupLookups;
    lastPowerupKey = key;
    return key == powerupKey ? powerupResult : nullptr;
}

class DrawableModel
{
public:
    unsigned shadows;
    DrawableModel() : shadows(0) {}
    void DrawPlanarShadow() { ++shadows; }
};
class DrawableObject
{
public:
    unsigned long m_uObjectFlags;
    nlQuaternion m_orientation;
    bool m_worldMatrixUpToDate;
    nlVector3 m_translation;
    float m_scale;
    DrawableModel* model;
    unsigned draws;
    DrawableObject()
        : m_uObjectFlags(0), m_orientation{0,0,0,0},
          m_worldMatrixUpToDate(true), m_translation{0,0,0}, m_scale(0),
          model(nullptr), draws(0) {}
    void Draw() { ++draws; }
    DrawableModel* AsDrawableModel() { return model; }
};
class World
{
public:
    unsigned long key, lastKey;
    DrawableObject* result;
    unsigned lookups;
    World() : key(0), lastKey(0), result(nullptr), lookups(0) {}
    DrawableObject* FindDrawableObject(unsigned long query)
    {
        ++lookups;
        lastKey = query;
        return key == query ? result : nullptr;
    }
};
struct WorldManager { static World* s_World; };
World* WorldManager::s_World;

static void nlMakeQuat(nlQuaternion& out, const nlVector3&, float angle)
{
    ++quaternionCalls;
    out = nlQuaternion{0,0,angle,1};
}
static void nlVecLerp(nlVector3& out, const nlVector3& lhs,
                      const nlVector3& rhs, float factor)
{
    out = nlVector3{(1-factor)*lhs.x+factor*rhs.x,
                    (1-factor)*lhs.y+factor*rhs.y,
                    (1-factor)*lhs.z+factor*rhs.z};
}
enum eGLState { GLS_AlphaBlend, GLS_Culling, GLS_DepthWrite };
enum eGLTextureType { GLTT_Diffuse };
enum eGLTextureState { GLTS_DiffuseWrap };
enum eGLView { GLV_Unshadowed };
struct MockColour { unsigned char c[4]; };
struct glQuad3
{
    nlVector3 m_pos[4];
    nlVector2 m_uv[4];
    MockColour m_colour[4];
    void Attach(eGLView, int, bool) { ++quadShadows; }
};
static void glSetDefaultState(bool) {}
static void glSetRasterState(eGLState, unsigned long) {}
static void glSetCurrentRasterState(unsigned long) {}
static unsigned long glHandleizeRasterState() { return 0; }
static unsigned long glGetTexture(const char*) { return 1; }
static void glSetCurrentTexture(unsigned long, eGLTextureType) {}
static void glSetTextureState(eGLTextureState, unsigned long) {}
static void glSetCurrentTextureState(unsigned long) {}
static unsigned long glHandleizeTextureState() { return 0; }

#ifndef STRIKERS_POWERUP_SOURCE
#define STRIKERS_POWERUP_SOURCE "../src/Game/Drawable/DrawablePowerup.cpp"
#endif
#include STRIKERS_POWERUP_SOURCE

namespace
{
unsigned checks, failures, exercised;
bool expectOld;

void expect(bool result, const char* operation, int index)
{
    ++checks;
    if (!result && failures++ < 12)
        std::fprintf(stderr, "%s index=%d\n", operation, index);
}

bool same(nlVector3 lhs, nlVector3 rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

void hash_delta(unsigned before, int index, const char* operation)
{
    const bool cached = static_cast<unsigned>(index) < 150u && !expectOld;
    expect(hashCalls - before == (cached ? 0u : 1u), operation, index);
}

void exercise(int index)
{
    ++exercised;
    // GetName is deliberately the unchanged original routine, including its
    // historical behavior for negative and unusually large indices.
    const u32 expected = powerup_test_original_lower_hash(GetName(index));
    if (static_cast<unsigned>(index) < 150u)
    {
        char canonical[32];
        std::snprintf(canonical, sizeof canonical, "powerup_generated_%d", index);
        expect(expected == powerup_test_original_lower_hash(canonical),
               "original-name vs independently formatted name", index);
    }
    const unsigned long widened = static_cast<unsigned long>(expected);
    powerupKey = widened;
    PowerupBase first{3,1.25f,1234,nlVector3{1,2,3},0.75f};
    PowerupBase replacement{5,2.5f,65000,nlVector3{-4,5,6},1.5f};
    DrawablePowerup snapshot{};
    powerupResult = &first;
    unsigned beforeHash = hashCalls, beforeLookup = powerupLookups;
    expect(snapshot.GetPowerup(index) == &first, "initial live powerup", index);
    expect(powerupLookups == beforeLookup+1 && lastPowerupKey == widened,
           "GetPowerup live lookup and unsigned widening", index);
    hash_delta(beforeHash, index, "GetPowerup hash count");

    beforeHash = hashCalls;
    beforeLookup = powerupLookups;
    const unsigned beforeRadius = radiusCalls;
    snapshot.Grab(index);
    expect(powerupLookups == beforeLookup+1 && lastPowerupKey == widened,
           "Grab live lookup", index);
    expect(radiusCalls == beforeRadius+1 && snapshot.mVisible &&
           snapshot.mType == first.m_eType && snapshot.mRadius == first.radius &&
           snapshot.mScale == first.m_scale &&
           snapshot.mOrientation == first.m_aOrientation &&
           same(snapshot.mPosition, first.m_v3Position), "Grab values", index);
    hash_delta(beforeHash, index, "Grab hash count");

    DrawableModel firstModel;
    DrawableObject firstObject;
    firstObject.model = &firstModel;
    const unsigned long preserved = (1ul << (sizeof(unsigned long)*CHAR_BIT-1)) | 0x80ul;
    firstObject.m_uObjectFlags = preserved;
    World firstWorld;
    firstWorld.key = widened;
    firstWorld.result = &firstObject;
    WorldManager::s_World = &firstWorld;
    beforeHash = hashCalls;
    unsigned beforeQuat = quaternionCalls;
    snapshot.Render(index);
    expect(firstWorld.lookups == 1 && firstWorld.lastKey == widened,
           "Render live world lookup and unsigned widening", index);
    expect(firstObject.draws == 1 && firstModel.shadows == 1 &&
           firstObject.m_uObjectFlags == (preserved|1ul) &&
           !firstObject.m_worldMatrixUpToDate &&
           same(firstObject.m_translation, first.m_v3Position) &&
           firstObject.m_scale == first.m_scale &&
           quaternionCalls == beforeQuat+1, "Render visible effects", index);
    hash_delta(beforeHash, index, "Render hash count");

    // Reuse the same logical index while both registries and the world change.
    powerupResult = &replacement;
    beforeHash = hashCalls;
    beforeLookup = powerupLookups;
    expect(snapshot.GetPowerup(index) == &replacement, "replaced powerup", index);
    snapshot.Grab(index);
    expect(powerupLookups == beforeLookup+2 && snapshot.mVisible &&
           snapshot.mType == replacement.m_eType &&
           snapshot.mRadius == replacement.radius &&
           snapshot.mScale == replacement.m_scale &&
           same(snapshot.mPosition, replacement.m_v3Position),
           "replacement Grab values", index);
    const unsigned perHash = static_cast<unsigned>(index) < 150u && !expectOld ? 0u : 1u;
    expect(hashCalls-beforeHash == 2u*perHash, "replacement hash count", index);

    DrawableObject secondObject;
    secondObject.m_uObjectFlags = preserved;
    World secondWorld;
    secondWorld.key = widened;
    secondWorld.result = &secondObject;
    WorldManager::s_World = &secondWorld;
    beforeHash = hashCalls;
    const unsigned beforeShadow = quadShadows;
    snapshot.Render(index);
    expect(firstWorld.lookups == 1 && secondWorld.lookups == 1 &&
           secondWorld.lastKey == widened && secondObject.draws == 1 &&
           firstObject.draws == 1 && quadShadows == beforeShadow+1 &&
           same(secondObject.m_translation, replacement.m_v3Position),
           "new world/object and generic shadow", index);
    hash_delta(beforeHash, index, "replacement Render hash count");

    powerupResult = nullptr;
    beforeHash = hashCalls;
    beforeLookup = powerupLookups;
    expect(snapshot.GetPowerup(index) == nullptr, "removed powerup", index);
    snapshot.Grab(index);
    expect(powerupLookups == beforeLookup+2 && !snapshot.mVisible,
           "removed powerup Grab", index);
    expect(hashCalls-beforeHash == 2u*perHash, "removed powerup hash count", index);
    beforeHash = hashCalls;
    beforeQuat = quaternionCalls;
    snapshot.Render(index);
    expect(secondWorld.lookups == 2 && secondObject.draws == 1 &&
           secondObject.m_uObjectFlags == preserved &&
           quaternionCalls == beforeQuat+1 &&
           same(secondObject.m_translation, replacement.m_v3Position),
           "invisible object still resolves and hides", index);
    hash_delta(beforeHash, index, "invisible Render hash count");

    // An absent drawable must still be queried and then return immediately.
    secondWorld.result = nullptr;
    beforeHash = hashCalls;
    beforeQuat = quaternionCalls;
    snapshot.Render(index);
    expect(secondWorld.lookups == 3 && quaternionCalls == beforeQuat &&
           secondObject.draws == 1, "missing drawable", index);
    hash_delta(beforeHash, index, "missing Render hash count");
}
}

int main(int argc, char** argv)
{
    expectOld = argc == 2 && std::strcmp(argv[1], "--expect-old") == 0;
    static_assert(sizeof(u32) == 4, "Original hashes are exactly 32 bits");
    static_assert(std::is_unsigned<u32>::value, "Hash arithmetic is unsigned");
    expect(hashCalls == (expectOld ? 0u : 150u), "one-time table initialization", 0);

    // Ascending, descending and a permutation exercise both decimal boundaries
    // and reuse after all three characters of the old static buffer changed.
    for (int i=0; i<150; ++i) exercise(i);
    for (int i=149; i>=0; --i) exercise(i);
    for (int i=0; i<150; ++i) exercise((i*47)%150);
    const int outside[] = {INT_MIN,-10000,-256,-255,-200,-176,-175,-129,-128,
                           -100,-49,-48,-1,150,151,199,200,255,256,999,1000,
                           1023,12799,12800,25599,25600,INT_MAX};
    for (int index : outside) exercise(index);
    for (int index : {9,10,99,100,149,0,148}) exercise(index);

    std::printf("powerup hash driver: %u checks, %u cases, %u failures; "
                "char=%s unsigned_long=%u bytes old=%d hash_calls=%u\n",
                checks, exercised, failures, CHAR_MIN < 0 ? "signed" : "unsigned",
                unsigned(sizeof(unsigned long)), int(expectOld), hashCalls);
    return failures ? 1 : 0;
}
