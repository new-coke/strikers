// NL assertion handler, declared in NL/nlDebug.h and defined nowhere in the decompilation.

#include <cstdio>

#include "NL/nlDebug.h"

void nlAssertFail(const char* condition, const char* filename, int line, bool bBreak)
{
    std::fprintf(stderr, "\nassertion failed: %s\n  at %s:%d\n",
                 condition != nullptr ? condition : "(no condition)",
                 filename != nullptr ? filename : "(unknown file)", line);
    std::fflush(nullptr);

    if (bBreak)
        nlBreak();
}
