#include "port/launch.h"

#if defined(PORT_VITA)

#include <cstdlib>

namespace {
float s_renderScale = 544.0f / 448.0f;
bool s_checked;
}

#if !defined(STRIKERS_VITA_NATIVE_RUNTIME)
extern "C" void PortAuroraConfigure(AuroraConfig* cfg)
{
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)

#else

    (void)cfg;

#endif
}

#endif
extern "C" void PortFollowRenderScale(unsigned int windowHeight)
{
    if (!s_checked)
    {
        s_checked = true;
        const char* value = std::getenv("STRIKERS_RES_SCALE");
        if (value != nullptr && *value != '\0')
            s_renderScale = (float)std::atof(value);
    }
    else if (windowHeight != 0 && std::getenv("STRIKERS_RES_SCALE") == nullptr)
    {
        s_renderScale = (float)windowHeight / 448.0f;
    }
}

extern "C" void PortSetRenderScale(float scale)
{
    s_checked = true;
    s_renderScale = scale;
}

extern "C" float PortRenderScale(void)
{
    return s_renderScale;
}

#endif
