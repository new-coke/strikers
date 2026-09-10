// See include/port/morphwatch.h. Finds the frame on which a cSAnim morph field is overwritten: the
// address differs every run, so lldb cannot be pointed at it until the game reports where and when.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if !defined(_WIN32)
#include <execinfo.h>
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#else
// unistd.h does not exist here. Everything this file wants from it (sysconf, write) is inside the
// POSIX half above and never reaches the Windows compiler.
#include <windows.h>
#endif

#include "port/morphwatch.h"

void OSReport(const char* fmt, ...);

static void prot_note_registration(const void* fieldAddr);
static void prot_tick(unsigned long frame);

// There was a second route in here, STRIKERS_WATCH_MORPH_TRAP: park a registration in a spin loop
// so lldb could attach and plant a hardware watchpoint on the byte the corruption zeroed.
typedef struct
{
    const void* obj;
    const void* addr;
    const char* tag;
    unsigned long long snap;
} WatchEntry;

// A match loads a few hundred animations; three fields each.
enum { kMaxEntries = 8192 };
static WatchEntry s_entries[kMaxEntries];
static int s_count;

static int s_enabled = -1; // -1 unread, 0 off, 1 on

static int enabled(void)
{
    if (s_enabled < 0)
        s_enabled = getenv("STRIKERS_WATCH_MORPH") != NULL;
    return s_enabled;
}

void PortMorphWatchRegister(const void* obj, const void* fieldAddr,
                            const char* tag)
{
    if (!enabled() || fieldAddr == NULL)
        return;

    unsigned long long value;
    memcpy(&value, fieldAddr, sizeof value);

    for (int i = 0; i < s_count; i++)
    {
        if (s_entries[i].addr == fieldAddr)
        {
            s_entries[i].obj = obj;
            s_entries[i].tag = tag;
            s_entries[i].snap = value;
            return;
        }
    }

    if (s_count >= kMaxEntries)
    {
        static int warned;
        if (!warned)
        {
            warned = 1;
            OSReport("[morphwatch] table full at %d entries; later loads "
                     "unwatched\n", kMaxEntries);
        }
        return;
    }

    WatchEntry* e = &s_entries[s_count++];
    e->obj = obj;
    e->addr = fieldAddr;
    e->tag = tag;
    e->snap = value;

    prot_note_registration(fieldAddr);
}

void PortMorphWatchPoll(unsigned long frame)
{
    if (s_enabled != 1)
        return;

    prot_tick(frame);

    // Enough reports to show a pattern, capped so a scene transition cannot flood the log.
    static int s_reports;
    enum { kMaxReports = 64 };

    for (int i = 0; i < s_count; i++)
    {
        WatchEntry* e = &s_entries[i];
        unsigned long long now;
        memcpy(&now, e->addr, sizeof now);
        if (now == e->snap)
            continue;

        if (s_reports < kMaxReports)
        {
            s_reports++;
            unsigned long long delta = now ^ e->snap;
            int firstByte = -1, lastByte = -1;
            for (int b = 0; b < 8; b++)
            {
                if ((delta >> (8 * b)) & 0xFF)
                {
                    if (firstByte < 0)
                        firstByte = b;
                    lastByte = b;
                }
            }
            OSReport("[morphwatch] frame=%lu idx=%d %s of obj=%p at %p: "
                     "0x%llx -> 0x%llx (bytes %d..%d)%s\n",
                     frame, i, e->tag, e->obj, e->addr,
                     e->snap, now, firstByte, lastByte,
                     s_reports == kMaxReports ? " (last report)" : "");
        }
        e->snap = now;
    }
}

// Page-protection writer trap: STRIKERS_WATCH_MORPH_PROT=1 (with STRIKERS_WATCH_MORPH=1).

#if !defined(_WIN32)

enum { kMaxPages = 4096 };
typedef struct
{
    uintptr_t base;
    volatile int armed;
    volatile unsigned innocents;
} WatchPage;

static WatchPage s_pages[kMaxPages];
static volatile int s_pageCount;
static long s_pageSize;
static int s_protEnabled = -1; // -1 unread, 0 off, 1 on
static unsigned long s_protFrom = 3000;
static unsigned long s_protTo = 12000;
static volatile int s_protArmed;
static int s_protInstalled;
static volatile unsigned long s_lastFrame;
static struct sigaction s_prevBus, s_prevSegv;
static volatile unsigned s_fieldHits, s_objHits, s_innocentTotal;
static intptr_t s_slide;
static volatile int s_summaryPrinted;

static int prot_enabled(void)
{
    if (s_protEnabled < 0)
    {
        s_protEnabled = getenv("STRIKERS_WATCH_MORPH_PROT") != NULL;
        if (s_protEnabled)
        {
            const char* f = getenv("STRIKERS_WATCH_MORPH_PROT_FROM");
            const char* t = getenv("STRIKERS_WATCH_MORPH_PROT_TO");
            if (f != NULL)
                s_protFrom = strtoul(f, NULL, 0);
            if (t != NULL)
                s_protTo = strtoul(t, NULL, 0);
            s_pageSize = sysconf(_SC_PAGESIZE);
        }
    }
    return s_protEnabled;
}

static void prot_note_registration(const void* fieldAddr)
{
    if (!prot_enabled())
        return;
    // The field is 8 bytes; cover a page-straddling object too.
    uintptr_t lo = (uintptr_t)fieldAddr & ~((uintptr_t)s_pageSize - 1);
    uintptr_t hi = ((uintptr_t)fieldAddr + 7) & ~((uintptr_t)s_pageSize - 1);
    for (uintptr_t page = lo; page <= hi; page += (uintptr_t)s_pageSize)
    {
        int i;
        for (i = 0; i < s_pageCount; i++)
            if (s_pages[i].base == page)
                break;
        if (i < s_pageCount)
            continue;
        if (s_pageCount >= kMaxPages)
        {
            static int warned;
            if (!warned)
            {
                warned = 1;
                OSReport("[morphprot] page table full at %d; later pages "
                         "unwatched\n", kMaxPages);
            }
            return;
        }
        s_pages[s_pageCount].base = page;
        s_pages[s_pageCount].armed = 0;
        s_pages[s_pageCount].innocents = 0;
        s_pageCount++; // count incremented after the entry is complete
    }
}

// async-signal-safe logging helpers (write(2) only)

static void prot_puts(const char* s)
{
    write(2, s, strlen(s));
}

static void prot_puthex(unsigned long long v)
{
    char buf[19];
    int i = 18;
    buf[--i] = 0;
    if (v == 0)
        buf[--i] = '0';
    while (v && i > 2)
    {
        unsigned d = (unsigned)(v & 0xF);
        buf[--i] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v >>= 4;
    }
    buf[--i] = 'x';
    buf[--i] = '0';
    prot_puts(&buf[i]);
}

static void prot_putdec(unsigned long long v)
{
    char buf[24];
    int i = 23;
    buf[i] = 0;
    if (v == 0)
        buf[--i] = '0';
    while (v && i > 0)
    {
        buf[--i] = (char)('0' + (v % 10));
        v /= 10;
    }
    prot_puts(&buf[i]);
}

// Frame-pointer walk of the faulting thread from its saved register state: backtrace() inside a
// handler walks the handler's frames and does not reliably cross the signal frame on arm64.
static int prot_walk(void** out, int max, uintptr_t pc, uintptr_t lr,
                     uintptr_t fp)
{
    int n = 0;
    if (n < max)
        out[n++] = (void*)pc;
    if (lr != 0 && n < max)
        out[n++] = (void*)lr; // leaf caller; duplicates frame 1 harmlessly otherwise
    int steps = 0;
    while (fp != 0 && (fp & 7) == 0 && n < max && steps++ < 64)
    {
        uintptr_t prev = ((uintptr_t*)fp)[0];
        uintptr_t ret = ((uintptr_t*)fp)[1];
        if (ret == 0)
            break;
        out[n++] = (void*)ret;
        if (prev <= fp || prev - fp > (1u << 20))
            break;
        fp = prev;
    }
    return n;
}

static void prot_forward(int sig, siginfo_t* si, void* uc)
{
    struct sigaction* prev = (sig == SIGBUS) ? &s_prevBus : &s_prevSegv;
    if ((prev->sa_flags & SA_SIGINFO) && prev->sa_sigaction != NULL)
    {
        prev->sa_sigaction(sig, si, uc);
        return;
    }
    if (prev->sa_handler == SIG_IGN)
        return;
    if (prev->sa_handler != SIG_DFL && prev->sa_handler != NULL)
    {
        prev->sa_handler(sig);
        return;
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

static void prot_handler(int sig, siginfo_t* si, void* ucv)
{
    uintptr_t addr = (uintptr_t)si->si_addr;
    WatchPage* pg = NULL;
    if (s_pageSize != 0)
    {
        uintptr_t base = addr & ~((uintptr_t)s_pageSize - 1);
        for (int i = 0; i < s_pageCount; i++)
        {
            if (s_pages[i].base == base)
            {
                pg = &s_pages[i];
                break;
            }
        }
    }
    if (pg == NULL)
    {
        prot_forward(sig, si, ucv);
        return;
    }

    // The page is in the table, so the fault is ours to absorb even when the armed flag reads 0
    // (the arming write races the fault by nature).
    {
        static volatile uintptr_t s_lastAddr, s_lastPc;
        static volatile int s_repeats;
        uintptr_t pcNow = 0;
#if defined(__APPLE__) && defined(__arm64__)
        pcNow = (uintptr_t)((ucontext_t*)ucv)->uc_mcontext->__ss.__pc;
#elif defined(__APPLE__) && defined(__x86_64__)
        pcNow = (uintptr_t)((ucontext_t*)ucv)->uc_mcontext->__ss.__rip;
#endif
        if (addr == s_lastAddr && pcNow == s_lastPc)
        {
            if (++s_repeats > 8)
            {
                prot_forward(sig, si, ucv);
                return;
            }
        }
        else
        {
            s_lastAddr = addr;
            s_lastPc = pcNow;
            s_repeats = 0;
        }
    }

    // Classify: 2 = inside a watched 8-byte field (the jackpot), 1 = inside a watched object, 0 =
    // innocent cohabitant of the page.
    int cls = 0;
    const WatchEntry* hit = NULL;
    for (int i = 0; i < s_count; i++)
    {
        uintptr_t f = (uintptr_t)s_entries[i].addr;
        if (addr >= f && addr < f + 8)
        {
            cls = 2;
            hit = &s_entries[i];
            break;
        }
    }
    if (cls == 0)
    {
        // Host cSAnim is 0x88 bytes; 0x90 gives slack without spanning a neighbouring allocation's
        // whole body.
        for (int i = 0; i < s_count; i++)
        {
            uintptr_t o = (uintptr_t)s_entries[i].obj;
            if (o != 0 && addr >= o && addr < o + 0x90)
            {
                cls = 1;
                hit = &s_entries[i];
                break;
            }
        }
    }

    unsigned logged;
    int wantBt = 0;
    if (cls == 2)
    {
        logged = ++s_fieldHits;
        wantBt = (logged <= 16);
    }
    else if (cls == 1)
    {
        logged = ++s_objHits;
        wantBt = (logged <= 8);
    }
    else
    {
        logged = ++s_innocentTotal;
        pg->innocents++;
        wantBt = (logged <= 4);
    }

    if (wantBt)
    {
        uintptr_t pc = 0, lr = 0, fp = 0;
#if defined(__APPLE__) && defined(__arm64__)
        ucontext_t* uc = (ucontext_t*)ucv;
        pc = (uintptr_t)uc->uc_mcontext->__ss.__pc;
        lr = (uintptr_t)uc->uc_mcontext->__ss.__lr;
        fp = (uintptr_t)uc->uc_mcontext->__ss.__fp;
#elif defined(__APPLE__) && defined(__x86_64__)
        ucontext_t* uc = (ucontext_t*)ucv;
        pc = (uintptr_t)uc->uc_mcontext->__ss.__rip;
        fp = (uintptr_t)uc->uc_mcontext->__ss.__rbp;
#else
        (void)ucv;
#endif
        prot_puts("\n[morphprot] ");
        prot_puts(cls == 2 ? "FIELD-HIT" : cls == 1 ? "OBJ-HIT" : "innocent");
        prot_puts(" frame=");
        prot_putdec(s_lastFrame);
        prot_puts(" addr=");
        prot_puthex(addr);
        if (hit != NULL)
        {
            prot_puts(" obj=");
            prot_puthex((uintptr_t)hit->obj);
            prot_puts(" ");
            prot_puts(hit->tag != NULL ? hit->tag : "?");
            prot_puts(" +");
            prot_puthex(addr - (uintptr_t)hit->obj);
        }
        prot_puts(" pc=");
        prot_puthex(pc);
        prot_puts(" lr=");
        prot_puthex(lr);
        prot_puts(" slide=");
        prot_puthex((uintptr_t)s_slide);
        prot_puts("\n");

        void* frames[32];
        int n = prot_walk(frames, 32, pc, lr, fp);
        backtrace_symbols_fd(frames, n, 2);
        prot_puts("[morphprot] end of report\n");
    }

    mprotect((void*)pg->base, (size_t)s_pageSize, PROT_READ | PROT_WRITE);
    pg->armed = 0;
    // Return: the faulting store retries against the now-writable page.
}

static void* prot_reprotect_thread(void* unused)
{
    (void)unused;
    for (;;)
    {
        struct timespec ts = { 0, 250 * 1000 }; // 250us
        nanosleep(&ts, NULL);
        if (!s_protArmed)
            continue;
        for (int i = 0; i < s_pageCount; i++)
        {
            if (!s_pages[i].armed)
            {
                // armed is set BEFORE the mprotect: a write faulting in the gap after protection
                // but before the flag would otherwise be forwarded as "not ours" and kill the run
                // (it did).
                s_pages[i].armed = 1;
                if (mprotect((void*)s_pages[i].base, (size_t)s_pageSize,
                             PROT_READ) != 0)
                    s_pages[i].armed = 0;
            }
        }
    }
    return NULL;
}

static void prot_summary(const char* why)
{
    if (s_summaryPrinted)
        return;
    s_summaryPrinted = 1;
    OSReport("[morphprot] summary (%s): pages=%d field-hits=%u obj-hits=%u "
             "innocent=%u\n", why, s_pageCount, s_fieldHits, s_objHits,
             s_innocentTotal);
}

static void prot_atexit(void)
{
    if (s_protInstalled)
        prot_summary("exit");
}

static void prot_install(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = prot_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGBUS, &sa, &s_prevBus);
    sigaction(SIGSEGV, &sa, &s_prevSegv);
#if defined(__APPLE__)
    s_slide = _dyld_get_image_vmaddr_slide(0);
#endif
    pthread_t tid;
    pthread_create(&tid, NULL, prot_reprotect_thread, NULL);
    atexit(prot_atexit);
}

static void prot_tick(unsigned long frame)
{
    if (!prot_enabled())
        return;
    s_lastFrame = frame;

    int inWindow = (frame >= s_protFrom && frame <= s_protTo);
    if (inWindow)
    {
        if (!s_protInstalled)
        {
            s_protInstalled = 1;
            prot_install();
            OSReport("[morphprot] arming %d pages, frames %lu..%lu "
                     "(page=%ldB)\n", s_pageCount, s_protFrom, s_protTo,
                     s_pageSize);
        }
        s_protArmed = 1;
        for (int i = 0; i < s_pageCount; i++)
        {
            if (!s_pages[i].armed)
            {
                // armed is set BEFORE the mprotect: a write faulting in the gap after protection
                // but before the flag would otherwise be forwarded as "not ours" and kill the run
                // (it did).
                s_pages[i].armed = 1;
                if (mprotect((void*)s_pages[i].base, (size_t)s_pageSize,
                             PROT_READ) != 0)
                    s_pages[i].armed = 0;
            }
        }
    }
    else if (s_protArmed)
    {
        s_protArmed = 0;
        for (int i = 0; i < s_pageCount; i++)
        {
            mprotect((void*)s_pages[i].base, (size_t)s_pageSize,
                     PROT_READ | PROT_WRITE);
            s_pages[i].armed = 0;
        }
        prot_summary("window closed");
    }
}

#else // _WIN32

static void prot_note_registration(const void* fieldAddr) { (void)fieldAddr; }
static void prot_tick(unsigned long frame) { (void)frame; }

#endif
