// Rollback verifier, macOS only: STRIKERS_RBPROBE=every,count,span reruns spans, compares hashes.

#if !defined(__APPLE__)
extern "C" {
int PortRbProbeWantsDouble(unsigned) { return 0; }
void PortRbProbeDoubleSave(void) {}
void PortRbProbeDoubleRestore(void) {}
void PortRbProbeSpanHash(int, int) {}
}
#else

#include "port/rollback.h"
#include "port/simhash.h"
#include "Game/Game.h"
#include "Game/Camera/CameraMan.h"
#include "Game/Character.h"
#include "Game/CharacterTemplate.h"
#include "Game/Player.h"
#include "Game/AI/Fielder.h"
#include "Game/PoseAccumulator.h"

#include <dlfcn.h>
#include <execinfo.h>
#include <mach/mach_time.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
const void* PortSimHeapBase(void);
int PortSimHeapBlockOf(const void* p, const void** start, size_t* size);
void PortRollbackDiff(unsigned int a, unsigned int b, void (*report)(const char*, size_t));
const void* PortRollbackRegion(size_t index, size_t* size);
int PortSimHeapOwns(const void* p);
unsigned int PortSimTick(void);
extern void (*g_rbprobe_draw)(unsigned int seed);
void PortRollbackDataTake(int which);
void PortRollbackDataReport(void);
void PortRollbackDataCensus(void);
}

namespace
{
const unsigned kRollbackSlot = PORT_ROLLBACK_SCRATCH + 3;
const unsigned kMaxSpan = 64;

struct State
{
    unsigned every = 41, count = 0, span = 8;
    unsigned runs, bad;
    unsigned diffs;
    std::string diffText;
    unsigned char c0[2][sizeof(cFielder)];
    uint64_t hash[2][kMaxSpan];
    PortSimHashes full[2][kMaxSpan];
    int drawPass = -1;
    bool inDraw;
    std::vector<std::string> draws[2];
    double saveMs, loadMs;
    unsigned sts,
        stack;   // spans during a captain's Super Strike, and with a camera pushed over play
};
State* P;   // the only probe datum in the data segment

double now_ms(void)
{
    static mach_timebase_info_data_t tb;
    if (tb.denom == 0)
        mach_timebase_info(&tb);
    return (double)mach_absolute_time() * tb.numer / tb.denom / 1e6;
}

void on_draw(unsigned int seed)
{
    if (P->drawPass < 0 || P->inDraw)
        return;
    P->inDraw = true;
    void* frames[8];
    const int n = backtrace(frames, 8);
    char line[1024];
    int at = std::snprintf(line, sizeof line, "%08x", seed);
    for (int i = 2; i < n && i < 6 && at < (int)sizeof line; i++)
    {
        Dl_info info;
        if (dladdr(frames[i], &info) && info.dli_sname)
            at += std::snprintf(line + at, sizeof line - at, " %s+0x%lx", info.dli_sname,
                                (unsigned long)((char*)frames[i] - (char*)info.dli_saddr));
    }
    P->draws[P->drawPass].push_back(line);
    P->inDraw = false;
}

std::string components(const PortSimHashes& a, const PortSimHashes& b)
{
    std::string w;
    if (a.ball != b.ball)
        w += " ball";
    if (a.clk != b.clk)
        w += " clk";
    if (a.score != b.score)
        w += " score";
    if (a.game != b.game)
        w += " game";
    if (a.pwr != b.pwr)
        w += " pwr";
    if (a.npc != b.npc)
        w += " npc";
    if (a.seed != b.seed)
        w += " seed";
    if (a.scr != b.scr)
        w += " scr";
    for (int c = 0; c < 10; c++)
        if (a.perChar[c] != b.perChar[c])
            w += " c" + std::to_string(c);
    return w;
}
}   // namespace

extern "C" {

__attribute__((constructor)) static void rbprobe_ctor(void)
{
    const char* v = std::getenv("STRIKERS_RBPROBE");
    if (v == NULL || *v == '\0')
        return;
    P = new State();
    std::sscanf(v, "%u,%u,%u", &P->every, &P->count, &P->span);
    if (P->span < 1)
        P->span = 1;
    if (P->span > kMaxSpan)
        P->span = kMaxSpan;
    g_rbprobe_draw = on_draw;
    std::fprintf(stderr, "[rbprobe] every %u ticks, %u spans of %u\n", P->every, P->count, P->span);
}

// For the tick loop: the length of the rolled-back span to run from `tick`, or 0.
int PortRbProbeWantsDouble(unsigned tick)
{
    if (P == NULL || P->runs >= P->count || tick < 4 || (tick >= 6 && tick < 20) ||
        (tick >= 20 && (tick - 20) % P->every != 0))
        return 0;
    return (int)P->span;
}

void PortRbProbeDoubleSave(void)
{
    const double t0 = now_ms();
    PortRollbackSave(kRollbackSlot);
    P->saveMs += now_ms() - t0;
    PortRollbackDataTake(0);
    // Online, nothing past the span's first tick is confirmed while it can still roll back.
    PortRollbackSetConfirmed(PortSimTick() > 0 ? PortSimTick() - 1 : 0);
    P->drawPass = 0;
    P->draws[0].clear();
    P->draws[1].clear();
}

void PortRbProbeDoubleRestore(void)
{
    const double t0 = now_ms();
    PortRollbackLoad(kRollbackSlot);
    P->loadMs += now_ms() - t0;
    PortRollbackDataTake(1);
    if (std::getenv("STRIKERS_RB_CENSUS") != NULL)
        PortRollbackDataCensus();
    P->drawPass = 1;
    PortRollbackSetResimulating(1);
}

void PortRbProbeSpanHash(int pass, int j)
{
    PortSimHashes h;
    PortSimHash(&h);
    P->hash[pass][j] = h.all ^ (h.scr * 0x9E3779B97F4A7C15ull);
    P->full[pass][j] = h;
    if (j == 0)
    {
        // Character 0's own bytes after each run's first tick, for a field-level diff.
        if (g_pCharacters[0] != NULL)
            std::memcpy(P->c0[pass], g_pCharacters[0], sizeof P->c0[pass]);
        P->drawPass = -1;   // draws are kept for the first tick of each run only
        // The state after each run's first tick, in two spare ring slots, for a byte diff.
        PortRollbackSave(kRollbackSlot - 2 + (unsigned)pass);
        if (pass == 1)
        {
            P->diffs = 0;
            P->diffText.clear();
            PortRollbackDiff(kRollbackSlot - 2, kRollbackSlot - 1, [](const char* what, size_t o) {
                if (P->diffs++ >= 6)
                    return;
                char b[160];
                if (std::strcmp(what, "static") == 0)
                {
                    size_t n = 0;
                    const void* p = PortRollbackRegion(o, &n);
                    Dl_info info;
                    const char* name = dladdr(p, &info) && info.dli_sname ? info.dli_sname : "?";
                    std::snprintf(b, sizeof b, "\n[rbprobe]     static #%zu %p (%zu B) %s", o, p, n,
                                  name);
                }
                else
                {
                    // Which object of which character the offset falls in, and where in it.
                    const char* a = (const char*)PortSimHeapBase() + o;
                    char who[96] = "";
                    for (int c = 0; c < 10 && who[0] == 0; c++)
                    {
                        cCharacter* ch = g_pCharacters[c];
                        if (ch == NULL)
                            continue;
                        struct
                        {
                            const char* name;
                            const void* p;
                            size_t n;
                        } objs[] = {
                            { "character", ch, sizeof(cFielder) },
                            { "pose", ch->m_pPoseAccumulator, sizeof(cPoseAccumulator) },
                            { "pose.nodes",
                              ch->m_pPoseAccumulator ? ch->m_pPoseAccumulator->m_NodeMatrices.mData
                                                     : NULL,
                              64 * 80 },
                            { "pose.prev",
                              ch->m_pPoseAccumulator
                                  ? ch->m_pPoseAccumulator->m_PrevNodeMatrices.mData
                                  : NULL,
                              64 * 80 },
                            { "pose.rot",
                              ch->m_pPoseAccumulator
                                  ? (const void*)ch->m_pPoseAccumulator->m_rot.mData
                                  : NULL,
                              32 * 80 },
                            { "pose.trans",
                              ch->m_pPoseAccumulator
                                  ? (const void*)ch->m_pPoseAccumulator->m_trans.mData
                                  : NULL,
                              32 * 80 },
                            { "physics", ch->m_pPhysicsCharacter, 0x400 },
                            { "headtrack", ch->m_pHeadTrack, 0x100 },
                            { "play",
                              ch->m_eClassType == FIELDER
                                  ? (const void*)((cFielder*)ch)->m_pCurrentPlay
                                  : NULL,
                              0x200 },
                            { "avoid",
                              ch->m_eClassType == FIELDER
                                  ? (const void*)((cFielder*)ch)->m_pAvoidance
                                  : NULL,
                              0x200 },
                            { "shotmeter",
                              ch->m_eClassType == FIELDER
                                  ? (const void*)((cFielder*)ch)->m_pShotMeter
                                  : NULL,
                              0x100 },
                        };
                        for (auto& ob : objs)
                            if (ob.p != NULL && a >= (const char*)ob.p &&
                                a < (const char*)ob.p + ob.n)
                            {
                                std::snprintf(who, sizeof who, " = character %d %s +0x%zx", c,
                                              ob.name, (size_t)(a - (const char*)ob.p));
                                break;
                            }
                    }
                    const void* bs = NULL;
                    size_t bn = 0;
                    if (who[0] == 0 && PortSimHeapBlockOf(a, &bs, &bn))
                        std::snprintf(who, sizeof who, " = block of %zu bytes, +0x%zx into it", bn,
                                      (size_t)(a - (const char*)bs));
                    std::snprintf(b, sizeof b, "\n[rbprobe]     heap +0x%zx%s", o, who);
                }
                P->diffText += b;
            });
        }
    }
    if (pass != 1 || j + 1 != (int)P->span)
        return;
    PortRollbackSetResimulating(0);
    PortSoundsReplayDone();
    PortRollbackSetConfirmed(0xFFFFFFFFu);

    P->runs++;
    if (g_pGame != NULL && g_pGame->mbCaptainShotToScoreOn)
        P->sts++;
    if (cCameraManager::m_cameraStack != NULL &&
        cCameraManager::PeekCamera()->GetType() != eCameraType_Gameplay)
        P->stack++;
    int first = -1;
    for (unsigned i = 0; i < P->span; i++)
        if (P->hash[0][i] != P->hash[1][i])
        {
            first = (int)i;
            break;
        }
    if (first >= 0)
    {
        P->bad++;
        std::fprintf(stderr, "[rbprobe] span at t=%u diverges at +%d in%s\n",
                     PortSimTick() - P->span + 1, first,
                     components(P->full[0][first], P->full[1][first]).c_str());
        if (first == 0)
        {
            std::fprintf(stderr, "[rbprobe]   %u byte groups differ after the first tick:%s\n",
                         P->diffs, P->diffText.c_str());
            std::fprintf(stderr, "[rbprobe]   binary data that differs between the two starts:\n");
            PortRollbackDataReport();
            for (size_t o = 0; o < sizeof(cFielder); o += 4)
                if (std::memcmp(P->c0[0] + o, P->c0[1] + o, 4) != 0)
                    std::fprintf(stderr, "[rbprobe]   c0 +0x%zx: %08x vs %08x\n", o,
                                 *(const uint32_t*)(P->c0[0] + o),
                                 *(const uint32_t*)(P->c0[1] + o));
            const std::vector<std::string>& a = P->draws[0];
            const std::vector<std::string>& b = P->draws[1];
            size_t k = 0;
            while (k < a.size() && k < b.size() && a[k] == b[k])
                k++;
            std::fprintf(stderr, "[rbprobe]   draws %zu vs %zu, first difference at %zu\n",
                         a.size(), b.size(), k);
            for (size_t d = k > 2 ? k - 2 : 0; d < k + 3; d++)
                std::fprintf(stderr, "[rbprobe]   %zu  A %s\n[rbprobe]   %zu  B %s\n", d,
                             d < a.size() ? a[d].c_str() : "-", d,
                             d < b.size() ? b[d].c_str() : "-");
        }
    }
    if (P->runs == P->count)
        std::fprintf(
            stderr,
            "[rbprobe] spans of %u ticks: %u, diverged in %u; save mean %.3f ms, load mean "
            "%.3f ms, sim heap %zu KB; %u during a Super Strike, %u over a pushed camera\n",
            P->span, P->runs, P->bad, P->saveMs / P->runs, P->loadMs / P->runs,
            PortSimHeapUsed() / 1024, P->sts, P->stack);
}

}   // extern "C"

#endif
