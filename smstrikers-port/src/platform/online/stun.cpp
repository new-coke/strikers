// STUN Binding requests and responses, big-endian as RFC 5389 has them.

#include "stun.h"

#include <cstring>

namespace online
{
namespace
{
const uint32_t kCookie = 0x2112A442;
const uint16_t kBindingRequest = 0x0001;
const uint16_t kBindingSuccess = 0x0101;
const uint16_t kMappedAddress = 0x0001;
const uint16_t kXorMappedAddress = 0x0020;

uint16_t get16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }

uint32_t get32(const uint8_t* p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

void put16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

void put32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (24 - 8 * i));
}

// An IPv4 address attribute's value; XOR-MAPPED-ADDRESS masks it with the cookie.
bool read_ipv4(const uint8_t* v, size_t n, bool masked, NetAddr* out)
{
    if (n != 8 || v[1] != 0x01)
        return false;
    uint8_t cookie[4];
    put32(cookie, masked ? kCookie : 0);
    NetAddr a;
    std::memset(&a, 0, sizeof a);
    a.family = 4;
    a.port = (uint16_t)(get16(v + 2) ^ get16(cookie));
    for (int i = 0; i < 4; i++)
        a.ip[i] = (uint8_t)(v[4 + i] ^ cookie[i]);
    if (a.port == 0)
        return false;
    *out = a;
    return true;
}
}   // namespace

size_t EncodeStunRequest(const uint8_t txid[kStunTxidSize], uint8_t out[kStunRequestSize])
{
    put16(out, kBindingRequest);
    put16(out + 2, 0);
    put32(out + 4, kCookie);
    std::memcpy(out + 8, txid, kStunTxidSize);
    return kStunRequestSize;
}

bool DecodeStunResponse(const uint8_t* p, size_t n, const uint8_t txid[kStunTxidSize], NetAddr* out)
{
    if (n < 20 || get16(p) != kBindingSuccess || get32(p + 4) != kCookie ||
        std::memcmp(p + 8, txid, kStunTxidSize) != 0)
        return false;
    const size_t length = get16(p + 2);
    if (length % 4 != 0 || 20 + length != n)
        return false;
    bool haveXor = false;
    bool havePlain = false;
    NetAddr fromXor;
    NetAddr fromPlain;
    for (size_t at = 20; at < n;)
    {
        if (n - at < 4)
            return false;
        const uint16_t type = get16(p + at);
        const size_t size = get16(p + at + 2);
        const size_t padded = (size + 3) & ~(size_t)3;
        if (padded > n - at - 4)
            return false;
        const uint8_t* v = p + at + 4;
        if (type == kXorMappedAddress && !haveXor)
            haveXor = read_ipv4(v, size, true, &fromXor);
        else if (type == kMappedAddress && !havePlain)
            havePlain = read_ipv4(v, size, false, &fromPlain);
        at += 4 + padded;
    }
    if (!haveXor && !havePlain)
        return false;
    *out = haveXor ? fromXor : fromPlain;
    return true;
}
}   // namespace online
