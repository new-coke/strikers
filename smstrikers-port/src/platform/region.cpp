// One function, and it exists only because of a language boundary.

#include "port/region.h"

#if defined(PORT_USE_AURORA)

#include <string.h>

namespace aurora
{
extern char g_gameName[4];
}

extern "C" void PortSetDiscGameName(const char* code4)
{
    if (code4 == nullptr)
        return;
    memcpy(aurora::g_gameName, code4, sizeof(aurora::g_gameName));
}

#else

extern "C" void PortSetDiscGameName(const char* code4) { (void)code4; }

#endif
