// Backtrace on fatal signals, printed from the handler so a crash in CI or on someone else's
// machine says something without a debugger.

// A stack-protector trip is not caught: __stack_chk_fail skips raise(SIGABRT), so no handler runs
// and macOS writes no .ips report.

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)

#include <execinfo.h>
#include <unistd.h>

static void crash_handler(int sig)
{
    // Async-signal-safe only: backtrace_symbols() mallocs, so raw addresses go out and atos
    // resolves them.
    const char* name = "signal";
    switch (sig)
    {
    case SIGSEGV: name = "SIGSEGV (bad memory access)\n"; break;
    case SIGBUS:  name = "SIGBUS (misaligned or invalid access)\n"; break;
    case SIGABRT: name = "SIGABRT (abort/assert/uncaught exception)\n"; break;
    case SIGILL:  name = "SIGILL (illegal instruction)\n"; break;
    case SIGFPE:  name = "SIGFPE (arithmetic fault)\n"; break;
    default: break;
    }
    write(2, "\n*** strikers: ", 15);
    write(2, name, strlen(name));

    void* frames[64];
    int n = backtrace(frames, 64);
    backtrace_symbols_fd(frames, n, 2);

    signal(sig, SIG_DFL);
    raise(sig);
}

__attribute__((constructor)) static void port_install_crash_handler(void)
{
    if (getenv("STRIKERS_NO_CRASH_HANDLER") != NULL)
        return;

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_ONSTACK;

    // Not SIGSTKSZ, which since glibc 2.34 can expand to a sysconf() call and cannot size a
    // file-scope array. 64 KiB clears every MINSIGSTKSZ.
    static char altstack[65536];
    stack_t ss;
    ss.ss_sp = altstack;
    ss.ss_size = sizeof altstack;
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
}

#else // _WIN32

// Windows: an access violation is an exception, never a signal, so this needs
// SetUnhandledExceptionFilter for what the CPU raises, signal(SIGABRT) for abort() and assert(),
// and _set_invalid_parameter_handler for the CRT's own checks.

// SetErrorMode goes with them, or a fault raises a modal WER dialog and a headless run hangs rather
// than fails.

// Symbolisation needs a PDB, which is why CMakeLists.txt adds /Z7 and /DEBUG; without one this
// still prints module+offset.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <dbghelp.h>

static CRITICAL_SECTION port_sym_lock;
static LONG port_crash_reported = 0;

static void port_print_stack(CONTEXT* ctx)
{
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    STACKFRAME64 frame;
    CONTEXT walk;
    char symbuf[sizeof(SYMBOL_INFO) + 512];
    SYMBOL_INFO* sym = (SYMBOL_INFO*)symbuf;
    int depth;

    walk = *ctx;

    memset(&frame, 0, sizeof frame);
    frame.AddrPC.Offset = walk.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = walk.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = walk.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    memset(sym, 0, sizeof symbuf);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 500;

    for (depth = 0; depth < 64; ++depth)
    {
        DWORD64 displacement = 0;
        DWORD lineDisplacement = 0;
        IMAGEHLP_LINE64 line;
        IMAGEHLP_MODULE64 mod;
        DWORD64 pc;

        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame,
                         &walk, NULL, SymFunctionTableAccess64,
                         SymGetModuleBase64, NULL))
            break;

        pc = frame.AddrPC.Offset;
        if (pc == 0)
            break;

        memset(&mod, 0, sizeof mod);
        mod.SizeOfStruct = sizeof mod;
        if (SymGetModuleInfo64(process, pc, &mod))
            // ASLR moves the absolute address every run; the RVA is stable and is what
            // llvm-symbolizer wants.
            fprintf(stderr, "%2d  %-16s 0x%016llx +0x%08llx", depth,
                    mod.ModuleName, (unsigned long long)pc,
                    (unsigned long long)(pc - mod.BaseOfImage));
        else
            fprintf(stderr, "%2d  %-16s 0x%016llx %11s", depth, "?",
                    (unsigned long long)pc, "");

        if (SymFromAddr(process, pc, &displacement, sym))
            fprintf(stderr, "  %s + 0x%llx", sym->Name,
                    (unsigned long long)displacement);

        memset(&line, 0, sizeof line);
        line.SizeOfStruct = sizeof line;
        if (SymGetLineFromAddr64(process, pc, &lineDisplacement, &line))
            fprintf(stderr, "  (%s:%lu)", line.FileName,
                    (unsigned long)line.LineNumber);

        fprintf(stderr, "\n");

        {
            DWORD inlineCount = SymAddrIncludeInlineTrace(process, pc);
            DWORD ctx = 0, frameIdx = 0;
            if (inlineCount > 0 &&
                SymQueryInlineTrace(process, pc, 0, pc, pc, &ctx, &frameIdx))
            {
                DWORD i;
                for (i = 0; i < inlineCount; ++i, ++ctx)
                {
                    DWORD64 inlDisp = 0;
                    DWORD inlLineDisp = 0;
                    IMAGEHLP_LINE64 inlLine;

                    fprintf(stderr, "      inlined  ");
                    if (SymFromInlineContext(process, pc, ctx, &inlDisp, sym))
                        fprintf(stderr, "%s + 0x%llx", sym->Name,
                                (unsigned long long)inlDisp);

                    memset(&inlLine, 0, sizeof inlLine);
                    inlLine.SizeOfStruct = sizeof inlLine;
                    if (SymGetLineFromInlineContext(process, pc, ctx, 0,
                                                    &inlLineDisp, &inlLine))
                        fprintf(stderr, "  (%s:%lu)", inlLine.FileName,
                                (unsigned long)inlLine.LineNumber);
                    fprintf(stderr, "\n");
                }
            }
        }
    }
    fflush(stderr);
}

static const char* port_exception_name(DWORD code)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:      return "EXCEPTION_ACCESS_VIOLATION (bad memory access)";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_ILLEGAL_INSTRUCTION:   return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case EXCEPTION_IN_PAGE_ERROR:         return "EXCEPTION_IN_PAGE_ERROR";
    case EXCEPTION_PRIV_INSTRUCTION:      return "EXCEPTION_PRIV_INSTRUCTION";
    case EXCEPTION_STACK_OVERFLOW:        return "EXCEPTION_STACK_OVERFLOW";
    // The int3 nlBreak() compiles to: the game noticed something rather than faulting, and the
    // interesting frame is its caller.
    case EXCEPTION_BREAKPOINT:            return "breakpoint: nlBreak()/assert, not a fault";
    case 0xE06D7363:                      return "unhandled C++ exception";
    default:                              return "exception";
    }
}

static LONG WINAPI port_exception_filter(EXCEPTION_POINTERS* info)
{
    DWORD code = info->ExceptionRecord->ExceptionCode;

    if (InterlockedExchange(&port_crash_reported, 1) != 0)
        return EXCEPTION_CONTINUE_SEARCH;

    EnterCriticalSection(&port_sym_lock);

    fprintf(stderr, "\n*** strikers: %s (0x%08lx)\n", port_exception_name(code),
            (unsigned long)code);

    // The record carries what was touched: a null is a missing object, a small offset a field on
    // one, a wild address a 32-bit pointer assumption.
    if (code == EXCEPTION_ACCESS_VIOLATION &&
        info->ExceptionRecord->NumberParameters >= 2)
    {
        static const char* const how[] = { "reading", "writing", "executing" };
        ULONG_PTR op = info->ExceptionRecord->ExceptionInformation[0];
        fprintf(stderr, "    %s address 0x%016llx\n",
                op < 3 ? how[op] : "accessing",
                (unsigned long long)info->ExceptionRecord->ExceptionInformation[1]);
    }

    port_print_stack(info->ContextRecord);

    LeaveCriticalSection(&port_sym_lock);

    return EXCEPTION_EXECUTE_HANDLER;
}

static void port_abort_handler(int sig)
{
    CONTEXT ctx;

    if (InterlockedExchange(&port_crash_reported, 1) != 0)
        _exit(134);

    EnterCriticalSection(&port_sym_lock);
    fprintf(stderr, "\n*** strikers: SIGABRT (abort/assert/uncaught exception)\n");
    RtlCaptureContext(&ctx);
    port_print_stack(&ctx);
    LeaveCriticalSection(&port_sym_lock);

    signal(sig, SIG_DFL);
    raise(sig);
}

static void port_invalid_parameter(const wchar_t* expression, const wchar_t* function,
                                   const wchar_t* file, unsigned int line,
                                   uintptr_t reserved)
{
    (void)expression;
    (void)function;
    (void)file;
    (void)line;
    (void)reserved;
    fprintf(stderr, "\n*** strikers: CRT invalid parameter\n");
    port_abort_handler(SIGABRT);
}

__attribute__((constructor)) static void port_install_crash_handler(void)
{
    if (getenv("STRIKERS_NO_CRASH_HANDLER") != NULL)
        return;

    InitializeCriticalSection(&port_sym_lock);

    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);

    SetUnhandledExceptionFilter(port_exception_filter);
    signal(SIGABRT, port_abort_handler);
    _set_invalid_parameter_handler(port_invalid_parameter);

    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

#endif // !_WIN32
