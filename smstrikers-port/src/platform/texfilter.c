// STRIKERS_ANISO. See include/port/texfilter.h for what the two numbers mean and why the port asks
// the GX enum for "full" and never "half".

#include "port/texfilter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The default, and it is 16 rather than the console's 1.
#define PORT_ANISO_DEFAULT 16u

static unsigned int ParseAniso(void)
{
    static const unsigned int kLevels[] = { 1u, 2u, 4u, 8u, 16u };
    const char* v = getenv("STRIKERS_ANISO");
    size_t i;

    if (v == NULL || *v == '\0')
        return PORT_ANISO_DEFAULT;

    for (i = 0; i < sizeof kLevels / sizeof kLevels[0]; i++)
    {
        char want[4];
        snprintf(want, sizeof want, "%u", kLevels[i]);
        if (strcmp(v, want) == 0)
            return kLevels[i];
    }

    // Not one of the five. Warn rather than fall silent, the same way STRIKERS_LANGUAGE does in
    // src/platform/os.c: a dropped config value presents as "the setting does nothing", which reads
    // as a broken feature rather than a typo.
    fprintf(stderr,
            "[port] STRIKERS_ANISO=%s is not a supported level; using %u. "
            "Valid: 1 (console), 2, 4, 8, 16.\n",
            v, PORT_ANISO_DEFAULT);
    return PORT_ANISO_DEFAULT;
}

unsigned int PortTextureAniso(void)
{
    // Cached, and it has to be: this is read from glx_SwitchTextureState, which runs per texture
    // stage per state change per frame.
    static unsigned int level = 0;
    if (level == 0)
        level = ParseAniso();
    return level;
}

int PortTextureAnisoIndex(void)
{
    return PortTextureAniso() > 1u ? 2 : 0;
}
