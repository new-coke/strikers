// Fatal-error path: a line on stderr, and a message box where there is a display.

#include "port/fatal.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(PORT_USE_AURORA)
#include <SDL3/SDL.h>
#endif

static int messagebox_suppressed(void)
{
    const char* e = getenv("STRIKERS_NO_MESSAGEBOX");
    return e != NULL && *e != '\0' && e[0] != '0';
}

void port_fatal(const char* title, const char* text)
{
    if (title == NULL)
        title = "Super Mario Strikers";
    if (text == NULL)
        text = "The game cannot start.";

    fprintf(stderr, "\n=== %s ===\n%s\n", title, text);
    fflush(stderr);

#if defined(PORT_USE_AURORA)
    if (!messagebox_suppressed())
    {
        // SDL turns SIGINT and SIGTERM into a quit event nothing pumps under a box.
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);

        // Asking for the subsystem is how to find out whether there is a display.
        if (SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, text, NULL);
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
        else
        {
            fprintf(stderr, "[port] (no display for a message box: %s)\n",
                    SDL_GetError());
            fflush(stderr);
        }
    }
#else
    (void)messagebox_suppressed;
#endif

    // exit, not abort: the crash handler must not backtrace a refusal to start.
    exit(1);
}
