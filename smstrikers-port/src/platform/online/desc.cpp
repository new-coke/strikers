// The match descriptor's wire form and range checks, and the name rules; no state, no network.

#include "port/online.h"

#include <cstring>

static_assert(sizeof(OnlineMatchDesc) == ONLINE_DESC_BYTES, "OnlineMatchDesc has padding");

namespace
{
// The retail menu's lengths, then the online-only ones.
const uint16_t kGameTimes[] = { 120, 180, 240, 300, 600, 900, 1200, 1800, 2700, 3600 };
const int kGameTimeCount = (int)(sizeof kGameTimes / sizeof kGameTimes[0]);

bool is_flag(uint8_t v) { return v <= 1; }

void put16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

void put32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}

uint16_t get16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

uint32_t get32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// The UTF-8 character at s[0..n) and its byte length, or -1 for a malformed or overlong one.
long decode_utf8(const unsigned char* s, size_t n, int* len)
{
    const unsigned char c = s[0];
    *len = 1;
    if (c < 0x80)
        return c;
    int extra;
    long cp;
    long min;
    if ((c & 0xE0) == 0xC0)
    {
        extra = 1;
        cp = c & 0x1F;
        min = 0x80;
    }
    else if ((c & 0xF0) == 0xE0)
    {
        extra = 2;
        cp = c & 0x0F;
        min = 0x800;
    }
    else if ((c & 0xF8) == 0xF0)
    {
        extra = 3;
        cp = c & 0x07;
        min = 0x10000;
    }
    else
        return -1;
    if ((size_t)extra >= n)
        return -1;
    for (int i = 1; i <= extra; i++)
    {
        if ((s[i] & 0xC0) != 0x80)
            return -1;
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *len = 1 + extra;
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return -1;
    return cp;
}
}   // namespace

extern "C" void PortOnlineDescDefault(OnlineMatchDesc* desc)
{
    std::memset(desc, 0, sizeof *desc);
    desc->protocol = ONLINE_PROTOCOL_VERSION;
    desc->gameTime = 180;
    desc->team[0] = 3;       // TEAM_MARIO
    desc->team[1] = 2;       // TEAM_LUIGI
    desc->sidekick[0] = 0;   // SK_TOAD
    desc->sidekick[1] = 1;   // SK_KOOPA
    for (int i = 0; i < ONLINE_MAX_SLOTS; i++)
    {
        desc->padSide[i] = -1;
        desc->slotMachine[i] = -1;
    }
    desc->stadium = 1;   // STAD_PEACH_TOAD_STADIUM
    desc->skill = 2;     // PROFESSIONAL
    desc->powerUps = 1;
    desc->powerupMask = 0xFF;
    desc->shoot2Score = 1;
    desc->bowser = 1;
}

extern "C" uint16_t PortOnlineGameTimeStep(uint16_t gameTime, int step)
{
    int i = 0;
    while (i < kGameTimeCount && kGameTimes[i] != gameTime)
        i++;
    if (i == kGameTimeCount)
        return kGameTimes[0];
    return kGameTimes[((i + step) % kGameTimeCount + kGameTimeCount) % kGameTimeCount];
}

extern "C" const char* PortOnlineDescCheck(const OnlineMatchDesc* d)
{
    if (d->protocol != ONLINE_PROTOCOL_VERSION)
        return "protocol";
    bool time_ok = false;
    for (uint16_t t : kGameTimes)
        time_ok = time_ok || d->gameTime == t;
    if (!time_ok)
        return "gameTime";
    for (int i = 0; i < 2; i++)
    {
        if (d->team[i] < 0 || d->team[i] > 8)
            return "team";
        if (d->sidekick[i] < 0 || d->sidekick[i] > 3)
            return "sidekick";
    }
    int per_machine[ONLINE_MAX_MACHINES] = {};
    for (int i = 0; i < ONLINE_MAX_SLOTS; i++)
    {
        if (d->padSide[i] < -1 || d->padSide[i] > 1)
            return "padSide";
        if (d->slotMachine[i] < -1 || d->slotMachine[i] >= ONLINE_MAX_MACHINES)
            return "slotMachine";
        if (d->padSide[i] >= 0 && d->slotMachine[i] < 0)
            return "slotMachine";
        if (d->slotMachine[i] >= 0 && ++per_machine[d->slotMachine[i]] > ONLINE_MAX_LOCAL_HUMANS)
            return "slotMachine";
    }
    if (d->stadium < 0 || d->stadium >= ONLINE_MAX_STADIUMS)
        return "stadium";
    if (d->skill > 4)
        return "skill";
    if (!is_flag(d->powerUps))
        return "powerUps";
    if (d->customPowerups > 5)
        return "customPowerups";
    if (d->cheats & ~ONLINE_CHEAT_ALL)
        return "cheats";
    if (!is_flag(d->shoot2Score))
        return "shoot2Score";
    if (!is_flag(d->bowser))
        return "bowser";
    if (!is_flag(d->bowserRepeat))
        return "bowserRepeat";
    if (!is_flag(d->pureGame))
        return "pureGame";
    if (!is_flag(d->pauseAllowed))
        return "pauseAllowed";
    if (d->reserved0 != 0)
        return "reserved";
    for (uint8_t b : d->reserved)
        if (b != 0)
            return "reserved";
    return NULL;
}

extern "C" void PortOnlineDescEncode(const OnlineMatchDesc* d, uint8_t out[ONLINE_DESC_BYTES])
{
    std::memset(out, 0, ONLINE_DESC_BYTES);
    put16(out + 0, d->protocol);
    put16(out + 2, d->gameTime);
    put32(out + 4, d->build);
    put32(out + 8, d->dataFamily);
    put32(out + 16, d->seed);
    for (int i = 0; i < 2; i++)
    {
        out[20 + i] = (uint8_t)d->team[i];
        out[22 + i] = (uint8_t)d->sidekick[i];
    }
    for (int i = 0; i < ONLINE_MAX_SLOTS; i++)
    {
        out[24 + i] = (uint8_t)d->padSide[i];
        out[28 + i] = (uint8_t)d->slotMachine[i];
    }
    out[32] = (uint8_t)d->stadium;
    out[33] = d->skill;
    out[34] = d->powerUps;
    out[35] = d->powerupMask;
    out[36] = d->customPowerups;
    out[37] = d->cheats;
    out[38] = d->shoot2Score;
    out[39] = d->bowser;
    out[40] = d->bowserRepeat;
    out[41] = d->pureGame;
    out[42] = d->pauseAllowed;
    std::memcpy(out + 43, d->reserved, sizeof d->reserved);
}

extern "C" const char* PortOnlineDescDecode(const uint8_t in[ONLINE_DESC_BYTES],
                                            OnlineMatchDesc* out)
{
    OnlineMatchDesc d;
    d.protocol = get16(in + 0);
    d.gameTime = get16(in + 2);
    d.build = get32(in + 4);
    d.dataFamily = get32(in + 8);
    d.reserved0 = get32(in + 12);
    d.seed = get32(in + 16);
    for (int i = 0; i < 2; i++)
    {
        d.team[i] = (int8_t)in[20 + i];
        d.sidekick[i] = (int8_t)in[22 + i];
    }
    for (int i = 0; i < ONLINE_MAX_SLOTS; i++)
    {
        d.padSide[i] = (int8_t)in[24 + i];
        d.slotMachine[i] = (int8_t)in[28 + i];
    }
    d.stadium = (int8_t)in[32];
    d.skill = in[33];
    d.powerUps = in[34];
    d.powerupMask = in[35];
    d.customPowerups = in[36];
    d.cheats = in[37];
    d.shoot2Score = in[38];
    d.bowser = in[39];
    d.bowserRepeat = in[40];
    d.pureGame = in[41];
    d.pauseAllowed = in[42];
    std::memcpy(d.reserved, in + 43, sizeof d.reserved);

    const char* bad = PortOnlineDescCheck(&d);
    if (bad == NULL)
        *out = d;
    return bad;
}

extern "C" uint64_t PortOnlineDescHash(const OnlineMatchDesc* desc)
{
    uint8_t bytes[ONLINE_DESC_BYTES];
    PortOnlineDescEncode(desc, bytes);
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint8_t b : bytes)
    {
        h ^= b;
        h *= 0x100000001b3ull;
    }
    return h;
}

extern "C" int PortOnlineCleanName(char out[ONLINE_NAME_MAX + 1], const char* in)
{
    const unsigned char* s = (const unsigned char*)(in != NULL ? in : "");
    size_t n = std::strlen((const char*)s);
    size_t len = 0;

    while (n > 0)
    {
        int step;
        const long cp = decode_utf8(s, n, &step);
        const bool keep = cp >= 0x20 && cp != 0x7F && !(cp >= 0x80 && cp <= 0x9F);
        if (keep && !(cp == ' ' && len == 0))
        {
            if (len + (size_t)step > ONLINE_NAME_MAX)
                break;
            std::memcpy(out + len, s, (size_t)step);
            len += (size_t)step;
        }
        s += step;
        n -= (size_t)step;
    }
    while (len > 0 && out[len - 1] == ' ')
        len--;
    out[len] = '\0';
    return (int)len;
}

extern "C" const char* PortOnlinePlatformName(int platform)
{
    static const char* const kNames[ONLINE_PLATFORM_COUNT] = {
        "Unknown", "Mac", "Windows", "Linux", "Steam Deck", "Switch", "Xbox",
    };
    return platform >= 0 && platform < ONLINE_PLATFORM_COUNT ? kNames[platform] : kNames[0];
}
