#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "port/endian.h"
using u16 = std::uint16_t;
using u32 = std::uint32_t;
#include "dsp_header.inc"
#include "convert_header.inc"

static_assert(sizeof(sDSPADPCM) == 0x60);
static_assert(offsetof(sDSPADPCM, num_samples) == 0x00);
static_assert(offsetof(sDSPADPCM, num_adpcm_nibbles) == 0x04);
static_assert(offsetof(sDSPADPCM, sample_rate) == 0x08);
static_assert(offsetof(sDSPADPCM, loop_flag) == 0x0c);
static_assert(offsetof(sDSPADPCM, format) == 0x0e);
static_assert(offsetof(sDSPADPCM, sa) == 0x10);
static_assert(offsetof(sDSPADPCM, ea) == 0x14);
static_assert(offsetof(sDSPADPCM, ca) == 0x18);
static_assert(offsetof(sDSPADPCM, coef) == 0x1c);
static_assert(offsetof(sDSPADPCM, gain) == 0x3c);
static_assert(offsetof(sDSPADPCM, ps) == 0x3e);
static_assert(offsetof(sDSPADPCM, yn1) == 0x40);
static_assert(offsetof(sDSPADPCM, yn2) == 0x42);
static_assert(offsetof(sDSPADPCM, lps) == 0x44);
static_assert(offsetof(sDSPADPCM, lyn1) == 0x46);
static_assert(offsetof(sDSPADPCM, lyn2) == 0x48);
static_assert(offsetof(sDSPADPCM, pad) == 0x4a);

struct GuardedHeader {
    unsigned char prefix[16];
    sDSPADPCM header;
    unsigned char suffix[16];
};

static void put32(unsigned char* out, uint32_t value)
{
    out[0] = static_cast<unsigned char>(value >> 24);
    out[1] = static_cast<unsigned char>(value >> 16);
    out[2] = static_cast<unsigned char>(value >> 8);
    out[3] = static_cast<unsigned char>(value);
}

static void put16(unsigned char* out, uint16_t value)
{
    out[0] = static_cast<unsigned char>(value >> 8);
    out[1] = static_cast<unsigned char>(value);
}

static void check_wire(const unsigned char (&wire)[96], bool missingRate)
{
    GuardedHeader guarded;
    std::memset(&guarded, 0xa5, sizeof guarded);
    std::memcpy(&guarded.header, wire, sizeof wire);
    unsigned char expected[96];
    // Independent serialized-layout oracle. No production decode helper is used.
    for (size_t offset = 0; offset < sizeof wire;)
    {
        const size_t word = offset < 12 || (offset >= 16 && offset < 28) ? 4 : 2;
        for (size_t j = 0; j < word; ++j)
            expected[offset + j] = wire[offset + word - j - 1];
        offset += word;
    }
    void* end = reinterpret_cast<unsigned char*>(&guarded.header) + sizeof guarded.header;
    if (missingRate) convert_missing_rate(end, sizeof guarded.header);
    else convert_after(end, sizeof guarded.header);
    assert(std::memcmp(&guarded.header, expected, sizeof expected) == 0);
    for (unsigned char byte : guarded.prefix) assert(byte == 0xa5);
    for (unsigned char byte : guarded.suffix) assert(byte == 0xa5);
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    const bool missingRate = std::strcmp(argv[1], "missing-rate") == 0;
    assert(missingRate || std::strcmp(argv[1], "after") == 0);
    const uint16_t endianProbe = 1;
    assert(*reinterpret_cast<const unsigned char*>(&endianProbe) == 1);
    unsigned char wire[96] = {};
    put32(wire + 0x00, 0x01020304);
    put32(wire + 0x04, 0x11223344);
    put32(wire + 0x08, 48000);
    put16(wire + 0x0c, 1);
    put16(wire + 0x0e, 0);
    put32(wire + 0x10, 0x010203a0);
    put32(wire + 0x14, 0x0e0f1011);
    put32(wire + 0x18, 0xc001c0de);
    for (size_t i = 0; i < 34; ++i)
        put16(wire + 0x1c + i * 2, static_cast<uint16_t>(0x8000 + i));
    check_wire(wire, missingRate);
    unsigned cases = 1;
    for (unsigned pattern = 0; pattern < 5; ++pattern)
    {
        for (size_t i = 0; i < sizeof wire; ++i)
            wire[i] = static_cast<unsigned char>(
                pattern == 0 ? 0 : pattern == 1 ? 255 : pattern == 2 ? i :
                pattern == 3 ? ((i & 1) ? 0xaa : 0x55) : (i * 37 + 11));
        check_wire(wire, missingRate);
        ++cases;
    }
    std::printf("{\"event\":\"header-pass\",\"cases\":%u,\"bytes_per_header\":96}\n",
                cases);
    return 0;
}
