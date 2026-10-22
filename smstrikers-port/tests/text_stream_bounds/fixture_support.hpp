#ifndef TEXT_STREAM_FIXTURE_SUPPORT_HPP
#define TEXT_STREAM_FIXTURE_SUPPORT_HPP

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <functional>
#include <vector>
#include "NL/nlFormat.h"

static unsigned completedCases;
static bool openCase;
static char currentCase[160];

static void case_begin(const char* prefix, const char* suffix)
{
    assert(!openCase);
    const int n = std::snprintf(currentCase, sizeof(currentCase), "%s.%s", prefix, suffix);
    assert(n > 0 && static_cast<size_t>(n) < sizeof(currentCase));
    openCase = true;
    std::printf("{\"event\":\"case-begin\",\"id\":\"%s\"}\n", currentCase);
    std::fflush(stdout);
}

static void case_pass()
{
    assert(openCase);
    std::printf("{\"event\":\"case-pass\",\"id\":\"%s\"}\n", currentCase);
    std::fflush(stdout);
    openCase = false;
    ++completedCases;
}

// Exact requested allocations let ASan observe the provider's real source extent.
// The array header/construction/destruction are the unchanged port/prelude.h code.
struct LiveAllocation { void* pointer; size_t bytes; };
static LiveAllocation liveAllocations[2048];
static size_t liveCount;

void* nlMalloc(size_t bytes, unsigned int alignment, bool)
{
    assert(bytes > 0 && liveCount < ARRAY_SIZE(liveAllocations));
    assert(alignment != 0 && (alignment & (alignment - 1)) == 0 && alignment <= 16);
    void* p = std::malloc(bytes);
    assert(p != nullptr);
    assert((reinterpret_cast<uintptr_t>(p) & (alignment - 1)) == 0);
    liveAllocations[liveCount++] = {p, bytes};
    return p;
}

void* nlMalloc(size_t bytes) { return nlMalloc(bytes, 8, false); }

extern "C" void nlFree(void* p)
{
    if (!p) return;
    size_t i = 0;
    while (i < liveCount && liveAllocations[i].pointer != p) ++i;
    assert(i != liveCount);
    liveAllocations[i] = liveAllocations[--liveCount];
    std::free(p);
}

// No numeric or debug formatting service is part of these string cases.
void nlBreak() { std::abort(); }
int nlSNPrintf(char*, unsigned long, const char*, ...) { std::abort(); }

class FontCharString;
class TLTextInstance
{
public:
#include "extracted/text_fields.inc"
    void SetString(const unsigned short*);
};
#include "extracted/set_string.inc"

static_assert(sizeof(unsigned short) == 2 && sizeof(u32) == 4 && sizeof(u64) == 8);
static_assert(sizeof(float) == 4 && sizeof(nlVector3) == 12);
static_assert(sizeof(void*) == 8, "This is a host LP64 gate, not a Vita ABI gate.");

#endif
