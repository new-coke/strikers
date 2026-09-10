// The control channel: stdio and nothing else, so this file compiles in the STRIKERS_AURORA=OFF
// scan with the rest of src/platform.

#include "port/control.h"
#include "port/input.h"
#include "port/overlay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Declared rather than included: aurora/aurora.h is only on the include path in the Aurora build.
extern "C" void aurora_capture_frame(const char* path);

namespace
{

// Read once, on the first poll. -1 not yet looked at, 0 off, 1 on.
int s_state = -1;
char s_path[1024];

// Where the last complete line ended; everything before it has run exactly once.
long s_offset;

// The ack counter, counting executed lines from 1.
unsigned long s_seq;

// Next frame the heartbeat is due on.
unsigned long s_nextBeat = 600;

const unsigned long kBeatFrames = 600;

void init(void)
{
    if (s_state >= 0)
        return;
    const char* path = getenv("STRIKERS_CONTROL");
    if (path == NULL || *path == '\0')
    {
        s_state = 0;
        return;
    }
    snprintf(s_path, sizeof s_path, "%s", path);
    s_state = 1;
    fprintf(stderr, "[control] reading '%s'\n", s_path);
}

// Not strtok: PortDebugParseCommand below uses it, and strtok's state is one per process.

int is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// Cut the next whitespace-delimited word out of *p in place, leaving *p on what follows; NULL when
// nothing is left.
char* next_word(char** p)
{
    char* s = *p;
    while (*s != '\0' && is_space(*s))
        s++;
    if (*s == '\0')
    {
        *p = s;
        return NULL;
    }
    char* start = s;
    while (*s != '\0' && !is_space(*s))
        s++;
    if (*s != '\0')
        *s++ = '\0';
    *p = s;
    return start;
}

// The rest of the line, leading spaces dropped; echo and shot take a value that may contain spaces.
char* rest_of(char* p)
{
    while (*p != '\0' && is_space(*p))
        p++;
    return p;
}

int parse_int(const char* s, int fallback)
{
    if (s == NULL || *s == '\0')
        return fallback;
    return (int)strtol(s, NULL, 10);
}

// dump: one line each, key=value, no spaces inside a value; an empty stack prints as an empty
// value, so a parser can split on ',' and drop the empties.

void print_ints(const int* v, int n, int cap)
{
    if (n > cap)
        n = cap;
    for (int i = 0; i < n; i++)
        fprintf(stderr, "%s%d", i != 0 ? "," : "", v[i]);
}

// A name with a space in it would break the key=value split; say so rather than emit an unparseable
// line.
void print_name(const char* s)
{
    if (s == NULL || *s == '\0')
    {
        fputc('-', stderr);
        return;
    }
    for (; *s != '\0'; s++)
        fputc(is_space(*s) ? '_' : *s, stderr);
}

void do_dump(void)
{
    const PortDebugSession* s = PortDebugGetSession();
    const PortDebugMatch* m = PortDebugGetMatch();

    fprintf(stderr, "[dump] session task=%d fe=", s->taskState);
    print_ints(s->feStack, s->feDepth, 8);
    fprintf(stderr, " overlay=");
    print_ints(s->overlayStack, s->overlayDepth, 8);
    fprintf(stderr, " pause=%d\n", s->inPauseMenu ? 1 : 0);

    fprintf(stderr,
            "[dump] match valid=%d state=%d ot=%d clock=%.2f dur=%.2f score=%d-%d "
            "owner=%d mode=%d demo=%d skill=%d pads=",
            m->valid ? 1 : 0, m->gameState, m->inSuddenDeath ? 1 : 0, (double)m->clock,
            (double)m->duration, m->scoreHome, m->scoreAway, m->ballOwner, m->gameMode,
            m->demoMode ? 1 : 0, m->skillLevel);
    print_ints(m->padSide, 4, 4);
    fprintf(stderr, " pres=");
    print_name(m->presentation);
    fprintf(stderr, " pres_t=%.2f\n", (double)m->presentationTime);
}

enum Verb { kUnknown, kPress, kStick, kCmd, kShot, kDump, kQuit, kEcho };

Verb verb_from_name(const char* w)
{
    static const struct { const char* name; Verb verb; } kVerbs[] = {
        { "press", kPress }, { "stick", kStick }, { "cmd", kCmd },  { "shot", kShot },
        { "dump", kDump },   { "quit", kQuit },   { "echo", kEcho },
    };
    for (size_t i = 0; i < sizeof kVerbs / sizeof kVerbs[0]; i++)
        if (strcmp(w, kVerbs[i].name) == 0)
            return kVerbs[i].verb;
    return kUnknown;
}

void execute(char* line, unsigned long frame)
{
    // The ack echoes the line as written, taken before the parsing below cuts it up in place.
    char raw[512];
    {
        const char* s = rest_of(line);
        size_t n = strlen(s);
        while (n != 0 && is_space(s[n - 1]))
            n--;
        if (n >= sizeof raw)
            n = sizeof raw - 1;
        memcpy(raw, s, n);
        raw[n] = '\0';
    }

    char* p = line;
    char* verb = next_word(&p);

    // Blank lines and # comments are not commands and are not acked, so a stray newline cannot move
    // the counter.
    if (verb == NULL || verb[0] == '#')
        return;

    // Decided once, because the ack has to name an unknown line as unknown before it is acted on.
    const Verb v = verb_from_name(verb);

    // Ack before the effect, so a reader that waits for the ack and then reads what follows never
    // has to look backwards.
    s_seq++;
    fprintf(stderr, "[control] #%lu frame %lu: %s%s\n", s_seq, frame,
            v == kUnknown ? "unknown: " : "", raw);

    // press BTN[+BTN...] [frames]
    if (v == kPress)
    {
        char* names = next_word(&p);
        const int frames = parse_int(next_word(&p), 0);
        unsigned int mask = 0;
        for (const char* n = names; n != NULL && *n != '\0';)
        {
            const char* plus = strchr(n, '+');
            const int len = plus != NULL ? (int)(plus - n) : (int)strlen(n);
            mask |= PortInputButtonFromName(n, len);
            if (plus == NULL)
                break;
            n = plus + 1;
        }
        if (mask != 0)
            PortInputPress(mask, frames);
        else
            fprintf(stderr, "[control] no such button: %s\n", names != NULL ? names : "");
        return;
    }

    // stick X Y [frames]
    if (v == kStick)
    {
        const int x = parse_int(next_word(&p), 0);
        const int y = parse_int(next_word(&p), 0);
        PortInputStick(x, y, parse_int(next_word(&p), 0));
        return;
    }

    // cmd OP[,a,b,c,f0,f1,f2,f3][=string]
    if (v == kCmd)
    {
        char* spec = rest_of(p);
        PortDebugCommand k;
        if (!PortDebugParseCommand(spec, &k))
            fprintf(stderr, "[control] not a command: %s\n", spec);
        else if (!PortDebugPushCommand(&k))
            fprintf(stderr, "[control] command queue full, op %d dropped\n", k.op);
        return;
    }

    // shot <path.ppm>
    if (v == kShot)
    {
        char* path = rest_of(p);
        if (*path != '\0')
            aurora_capture_frame(path);
        else
            fprintf(stderr, "[control] shot needs a path\n");
        return;
    }

    if (v == kDump)
    {
        do_dump();
        return;
    }

    if (v == kQuit)
    {
        PortRequestQuit();
        return;
    }

    // echo: the ack already carries the text, but a marker that can be grepped for on its own is
    // worth the second line.
    if (v == kEcho)
        fprintf(stderr, "[control] echo %s\n", rest_of(p));
}

} // namespace

extern "C" int PortDebugStateWanted(void)
{
    init();
    return s_state;
}

extern "C" int PortDebugParseCommand(const char* spec, PortDebugCommand* out)
{
    if (spec == NULL || out == NULL)
        return 0;

    memset(out, 0, sizeof *out);

    // A copy, because the numbers are cut out of it in place; 160 is what STRIKERS_DEBUG_CMD
    // allowed itself per item.
    char item[160];
    snprintf(item, sizeof item, "%s", spec);

    char* str = strchr(item, '=');
    if (str != NULL)
    {
        *str = '\0';
        snprintf(out->s, sizeof out->s, "%s", str + 1);
    }

    float v[8];
    memset(v, 0, sizeof v);
    int n = 0;
    for (char* tok = strtok(item, ","); tok != NULL && n < 8; tok = strtok(NULL, ","))
        v[n++] = (float)atof(tok);
    if (n == 0)
        return 0;

    out->op = (int)v[0];
    out->a = (int)v[1];
    out->b = (int)v[2];
    out->c = (int)v[3];
    for (int i = 0; i < 4; i++)
        out->f[i] = v[4 + i];
    return 1;
}

extern "C" void PortControlPoll(void)
{
    init();
    if (s_state != 1)
        return;

    // Reopened every frame rather than held open: the file need not exist when the game starts, and
    // sixty opens a second of a few hundred bytes is not worth a state machine.
    FILE* f = fopen(s_path, "rb");
    if (f != NULL)
    {
        if (fseek(f, s_offset, SEEK_SET) == 0)
        {
            char buf[4096];
            const size_t got = fread(buf, 1, sizeof buf - 1, f);
            buf[got] = '\0';

            // Only whole lines: a line halfway through being written waits for the next frame,
            // which is why the offset advances by consumed bytes and not by got.
            size_t consumed = 0;
            const unsigned long frame = PortInputFrame();
            for (;;)
            {
                char* nl = strchr(buf + consumed, '\n');
                if (nl == NULL)
                    break;
                *nl = '\0';
                execute(buf + consumed, frame);
                consumed = (size_t)(nl - buf) + 1;
            }
            s_offset += (long)consumed;
        }
        fclose(f);
    }

    // The heartbeat: a frame counter alone is not a liveness check, so it is printed with the
    // scene, and a hang reads as a number that climbs under a label that does not.
    const unsigned long frame = PortInputFrame();
    if (frame >= s_nextBeat)
    {
        fprintf(stderr, "[port] frame %lu scene %s\n", frame, PortOverlaySceneName());
        s_nextBeat = (frame / kBeatFrames + 1) * kBeatFrames;
    }
}
