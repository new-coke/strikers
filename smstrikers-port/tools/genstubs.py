#!/usr/bin/env python3
"""
Generate stubs for the few symbols the port does not supply.

The point is to reach a *link*. Compiling every translation unit proves the
sources are portable; linking proves the symbol graph closes, which is a
different and stricter claim, and it is the gate before anything can run.

What is left to stub is small and known: Aurora's missing EFB peek and poke,
ODE's LCP solver, and a handful of debug classes whose methods live in files
the decomp deliberately excludes. Each missing symbol gets a zero-argument
function carrying an explicit asm label, so the definition satisfies the
linker under the exact (already-mangled) name the reference uses. Calling one
aborts with its name, so anything the game turns out to need announces itself
instead of crashing anonymously; the ones passed with --noop return 0 silently.

Every stub has to match a line of tools/genstubs.allow, or this exits 1 after
writing the file. A new missing symbol is a translation unit that did not
compile, a declaration two files disagree about, or a function the port owes,
and each of those is a decision to make in the open rather than a stub to find
at runtime. STRIKERS_ACCEPT_NEW_STUBS=1, or --accept-new-stubs, links anyway.

Symbols the host libraries provide, libc, libm, libc++, libc++abi; are NOT
stubbed; they resolve at link time and stubbing them would shadow the real ones.

    tools/genstubs.py            # write src/platform/stubs_generated.c
"""

import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys

PORT = pathlib.Path(__file__).resolve().parent.parent
BUILD = PORT / (os.environ.get("STRIKERS_BUILD_DIR") or "build")
OUT = PORT / "src" / "platform" / "stubs_generated.c"
ALLOW = PORT / "tools" / "genstubs.allow"

# Mach-O prefixes every C symbol with an underscore.
UNDERSCORE = "_" if sys.platform == "darwin" else ""

# Static archives, by the host's spelling.
ARCHIVE_GLOBS = ("*.a", "*.lib")

# Object files, likewise. clang-cl writes .obj, and scanning only for .o meant the Windows build
# reached this script and exited "no objects", with 585 perfectly good objects sitting in the build
# tree.
OBJECT_GLOBS = ("*.o", "*.obj")

# Stub only what the port is KNOWN to owe, never "everything unrecognised".
SDK = re.compile(
    rf"^{UNDERSCORE}(?:"
    r"GX[A-Z]|PAD[A-Z]|DVD[A-Z]|CARD[A-Z]|"          # Aurora
    r"snd[A-Z]|SND[A-Z]|mus[A-Z]|sal[A-Z]|"          # MusyX
    r"THP[A-Z]|"                                     # FFmpeg-backed
    r"DSP[A-Z]|AX[A-Z]|MIX[A-Z]|SYN[A-Z]|SEQ[A-Z]|"  # audio hardware
    r"EXI[A-Z]|SI[A-Z]|GD[A-Z]|DB[A-Z]"              # misc console hardware
    r")")

# C++-mangled, but belonging to the host's standard library rather than to us.
HOST_CXX = re.compile(
    r"^__?Z(?:"
    r"[A-Za-z]{0,6}S(?:t\d*|[abdios])"   # _ZSt, _ZNSt, _ZNKSt3__1..., _ZNSolsEi
    r"|[A-Za-z]*N?10__cxxabiv"      # __cxxabiv1 vtables and helpers
    r"|nw|na|dl|da"                 # operator new/delete
    r")")


# The same judgement as HOST_CXX, in the other C++ ABI. clang-cl targets the MSVC ABI; it has to, to
# link against the Windows CRT; so on Windows the mangled names are MSVC-decorated
# (`?method@Class@@QEAAXXZ`) and not an Itanium `_Z ` in sight.
# type_info's vtable is vcruntime's and _com_issue_error is comsupp's, both linked through the CRT
# headers' default-library directives rather than from anything in the build tree.
HOST_CXX_MSVC = re.compile(r"@std@@|^\?\?(?:2|3|_U|_V)@|type_info@@|^\?_com_issue_error@@")


# Silent no-ops rather than aborts. Set via --noop.
NOOP = re.compile(r"(?!)")   # matches nothing until --noop is passed


# Where a Windows LLVM lands. clang is found through CMake, which searches for it; nothing puts the
# rest of the toolchain on PATH, and llvm-nm is the piece this script cannot do without.
WINDOWS_LLVM_BINS = (
    r"C:\Program Files\LLVM\bin",
    r"C:\Program Files (x86)\LLVM\bin",
)

_NM = None


def _nm():
    """The nm to use, and the flags it understands."""
    global _NM
    if _NM is not None:
        return _NM
    tool = shutil.which("llvm-nm") or shutil.which("nm")
    if tool is None:
        for d in WINDOWS_LLVM_BINS:
            cand = os.path.join(d, "llvm-nm.exe")
            if os.path.exists(cand):
                tool = cand
                break
    if tool is None:
        sys.exit("genstubs: no nm found (need llvm-nm or nm on PATH; on "
                 "Windows the LLVM package supplies llvm-nm.exe)")
    help_text = subprocess.run([tool, "--help"], capture_output=True,
                               text=True).stdout
    if "just-symbols" in help_text:
        # A prebuilt Rust archive may hold LLVM bitcode from a newer LLVM. llvm-nm reads the
        # archive's symbol table, but decoding each bitcode member fails the whole scan after a
        # partial result.
        common = ["--format=just-symbols"]
        if "--no-llvm-bc" in help_text:
            common.append("--no-llvm-bc")
        _NM = (tool, common, "--undefined-only", "--defined-only")
    else:
        _NM = (tool, ["-j"], "-u", "-U")
    return _NM


def itanium_mangled(sym: str) -> bool:
    """Is this actually an Itanium C++ name, or a C symbol that starts with Z?"""
    body = sym[3:] if sym.startswith("__Z") else sym[2:]
    if not body:
        return False
    if body[0] == "S":
        return len(body) > 1 and (body[1] == "t" or body[1] == "_" or body[1].isdigit())
    if body[0] == "L":
        return len(body) > 1 and body[1].isdigit()
    return body[0].isdigit() or body[0].islower() or body[0] in "NTGIZ"


def is_msvc_data_symbol(sym: str) -> bool:
    """Does this MSVC-decorated name refer to a variable rather than a function?"""
    if not sym.startswith("?"):
        return False
    at = sym.find("@@")
    if at < 0 or at + 2 >= len(sym):
        return False
    return sym[at + 2] in "01234567" and not sym.endswith("Z")


def wanted(sym: str) -> bool:
    if SDK.match(sym):
        return True
    if sym.startswith(("_Z", "__Z")) and itanium_mangled(sym) and not HOST_CXX.match(sym):
        return True
    # MSVC decoration always begins with '?'. `search`, not `match`: the namespace component sits in
    # the middle of the name, not at the front.
    if sym.startswith("?") and not HOST_CXX_MSVC.search(sym):
        return True
    return False


# What nm says about a file that was never an object file in the first place.
_NOT_AN_OBJECT = re.compile(
    r"file format not recognized"
    r"|not recognized as a valid object file"
    r"|No such file or directory",
    re.I)

# Files nm rejected during the current _nm_symbols call; see the batch failure branch below for why
# these specific ones are skipped rather than fatal.
_UNREADABLE = []


def _nm_symbols(nm_tool, nm_args, select, files):
    """nm over `files`, in batches that fit a command line."""
    LIMIT = 24000
    out = set()
    batch, size = [], 0
    def run(group):
        if not group:
            return set()
        command = [nm_tool, *nm_args, select, *group]
        try:
            result = subprocess.run(command, capture_output=True, text=True)
        except OSError as exc:
            sys.exit(f"genstubs: could not run {nm_tool} over a batch of "
                     f"{len(group)} file(s): {exc}")
        if result.returncode != 0:
            # One file nm cannot read must not cost the whole batch.
            if len(group) > 1:
                found = set()
                for one in group:
                    found |= run([one])
                return found
            detail = result.stderr.strip() or "no diagnostic"
            # Fail closed on anything but a plain "not an object file": an empty symbol set means
            # "defines nothing", which is how a stub gets written over a definition that is really
            # there.
            if all(_NOT_AN_OBJECT.search(line)
                   for line in detail.splitlines() if line.strip()):
                _UNREADABLE.append((group[0], detail.splitlines()[0]))
                return set()
            sys.exit(f"genstubs: {nm_tool} {select} failed over a batch of "
                     f"{len(group)} file(s): {detail}")
        return {l.split()[-1] for l in result.stdout.splitlines() if l.split()}
    for f in files:
        if batch and size + len(f) + 1 > LIMIT:
            out |= run(batch)
            batch, size = [], 0
        batch.append(f)
        size += len(f) + 1
    out |= run(batch)
    if _UNREADABLE:
        print(f"  {len(_UNREADABLE)} file(s) {nm_tool} could not read, skipped:")
        for name, why in _UNREADABLE[:5]:
            print(f"    {name}: {why}")
        if len(_UNREADABLE) > 5:
            print(f"    ... and {len(_UNREADABLE) - 5} more")
        _UNREADABLE.clear()
    return out


def provided_by_libs():
    """Symbols already defined by static libraries we link (Aurora, mainly)."""
    # Every static archive in the build tree: Dawn, SDL3, fmt and the rest come in through Aurora
    # and define symbols our objects reference.
    libs = sorted(l for g in ARCHIVE_GLOBS for l in BUILD.rglob(g))
    if not libs:
        return set()
    nm_tool, nm_args, _nm_undef, nm_def = _nm()
    syms = _nm_symbols(nm_tool, nm_args, nm_def, [str(l) for l in libs])
    print(f"  {len(syms)} symbols provided by {len(libs)} linked librar"
          f"{'y' if len(libs) == 1 else 'ies'}")
    return syms


def undefined():
    # Exclude our own previous output: its stubs would otherwise count as definitions and each run
    # would find only the handful of new symbols.
    objs = [str(p) for g in OBJECT_GLOBS for p in BUILD.rglob(g)
            if not p.name.startswith("stubs_generated.")]
    if not objs:
        sys.exit("no objects; run: cmake --build build")
    nm_tool, nm_args, nm_undef, nm_def = _nm()
    defined = _nm_symbols(nm_tool, nm_args, nm_def, objs)
    return sorted(_nm_symbols(nm_tool, nm_args, nm_undef, objs) - defined), defined


def msvc_scope(sym: str):
    """The scope-and-name prefix of an MSVC symbol: everything to the first `@@`."""
    if not sym.startswith("?"):
        return None
    # Templates are excluded, because for them the first `@@` is not the end of the name:
    # `??$Format@V?$BasicString@DVTempStringAllocator@Detail@@@@ ` closes a template *argument*
    # there.
    if sym.startswith("??$"):
        return None
    at = sym.find("@@")
    return sym[:at + 2] if at >= 0 else None


def load_allowlist(path=ALLOW):
    """tools/genstubs.allow: one regular expression per line, # comments."""
    patterns = []
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if line and not line.startswith("#"):
            patterns.append(re.compile(line))
    return patterns


def bare(sym):
    """The symbol without the object format's C prefix, so one allowlist serves every format."""
    if UNDERSCORE and sym.startswith(UNDERSCORE):
        return sym[len(UNDERSCORE):]
    return sym


def unlisted(syms, patterns):
    """The stubs no line of the allowlist accounts for."""
    return [s for s in syms if not any(p.search(bare(s)) for p in patterns)]


def report_declaration_mismatches(syms, defined):
    """Name every stub whose function is already defined under another decoration."""
    by_scope = {}
    for d in defined:
        scope = msvc_scope(d)
        if scope:
            by_scope.setdefault(scope, []).append(d)

    clashes = []
    for s in syms:
        scope = msvc_scope(s)
        if scope and scope in by_scope:
            clashes.append((s, by_scope[scope]))
    if not clashes:
        return 0
    print(f"\n  !! {len(clashes)} of these are DECLARATION MISMATCHES, not "
          f"missing code, each is defined in this build under a different\n"
          f"     decoration. A stub over one of these aborts at runtime on a "
          f"function the binary already has. Fix the declaration:")
    for s, defs in clashes:
        print(f"    undefined: {s}")
        for d in defs:
            print(f"      defined: {d}")
    return len(clashes)


def main():
    global NOOP
    ap = argparse.ArgumentParser()
    ap.add_argument("--noop", action="append", default=[],
                    help="regex for symbols that should return 0 silently "
                         "instead of aborting (e.g. --noop '^${U}GX')")
    ap.add_argument("--print-symbol-prefix", action="store_true",
                    help="print the host's C symbol prefix ('_' on Mach-O, "
                         "empty elsewhere) and exit, so callers building "
                         "--noop patterns do not have to know it")
    ap.add_argument("--accept-new-stubs", action="store_true",
                    default=os.environ.get("STRIKERS_ACCEPT_NEW_STUBS") == "1",
                    help="write stubs that tools/genstubs.allow does not list "
                         "and exit 0 anyway (also STRIKERS_ACCEPT_NEW_STUBS=1)")
    ap.add_argument("--out", default=str(OUT),
                    help="where to write the stubs (default: the source tree)")
    args = ap.parse_args()
    if args.print_symbol_prefix:
        print(UNDERSCORE)
        return
    if args.noop:
        NOOP = re.compile("|".join(args.noop))

    have = provided_by_libs()
    undef, defined_in_objects = undefined()
    syms = [s for s in undef if wanted(s) and s not in have]

    # Symbols that return instead of aborting.
    noop = [s for s in syms if NOOP.match(s)]
    noop_set = set(noop)

    lines = [
        "// GENERATED by tools/genstubs.py, do not edit.",
        # (see is_msvc_data_symbol below for why some entries are objects)
        "",
        "// One abort-on-call definition per symbol the port cannot yet supply, each carrying an asm",
        "// label so it defines the already-mangled name without needing the signature.",
        "",
        "#include <stdio.h>",
        "#include <stdlib.h>",
        "",
        "static void port_missing(const char* name)",
        "{",
        '    fprintf(stderr, "\\n[port] not implemented yet: %s\\n", name);',
        "    abort();",
        "}",
        "",
    ]
    for i, s in enumerate(syms):
        # `s` is spelled exactly as the object format spells it, with the leading underscore on
        # Mach-O, without it on ELF; which is exactly what the asm label needs in either case.
        if s in noop_set:
            lines.append(f'long port_stub_{i}(void) __asm__("{s}");')
            lines.append(f'long port_stub_{i}(void) {{ return 0; }}')
        elif is_msvc_data_symbol(s):
            # A missing *variable*, defined as a variable.
            lines.append(f'char port_stub_{i}[16] __asm__("{s}") = {{ 0 }};')
        else:
            lines.append(f'void port_stub_{i}(void) __asm__("{s}");')
            lines.append(f'void port_stub_{i}(void) {{ port_missing("{s}"); }}')

    out = pathlib.Path(args.out)
    out.write_text("\n".join(lines) + "\n")
    shown = out.relative_to(PORT) if out.is_relative_to(PORT) else out
    print(f"wrote {shown}: {len(syms)} stubs "
          f"({len(noop)} silent no-ops, {len(syms) - len(noop)} abort-on-call)")

    # Name the C++ ones, because a stub over a function the source already has is indistinguishable
    # from a stub over one it does not, until the game aborts on it.
    cxx = [s for s in syms if s not in noop_set
           and (s.startswith(("_Z", "__Z")) or s.startswith("?"))]
    if cxx:
        print(f"  {len(cxx)} of them C++-mangled, each should be a definition "
              f"the decomp genuinely excludes:")
        for s in cxx:
            print(f"    {s}")

    # And separate the ones that are not missing at all.
    mismatches = report_declaration_mismatches(
        [s for s in syms if s not in noop_set], defined_in_objects)

    # The gate. Written first so the file is there to read; exit 1 so rebuild.sh stops before the link.
    new = unlisted(syms, load_allowlist())
    if new:
        print(f"\n  !! {len(new)} stub(s) that tools/genstubs.allow does not "
              f"list. Decide what each is (a file that did not compile, a "
              f"declaration\n     two files disagree about, or a definition "
              f"the port owes) before adding a line for it:")
        for s in new:
            print(f"    {s}")
    if (new or mismatches) and not args.accept_new_stubs:
        sys.exit("genstubs: refusing to link over "
                 f"{len(new)} unlisted stub(s) and {mismatches} declaration "
                 "mismatch(es); STRIKERS_ACCEPT_NEW_STUBS=1 overrides")


if __name__ == "__main__":
    main()
