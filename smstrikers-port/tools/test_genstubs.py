#!/usr/bin/env python3
"""Which symbols genstubs.py will and will not define a stub for.

Both answers are silent when they are wrong. Stubbing something the host library
provides puts an abort-on-call definition in front of a real function, and the
game dies at runtime the first time it reaches it. Not stubbing something the
port owes is invisible until the link, and only on the platform whose mangling
was missed.

    python3 tools/test_genstubs.py
"""
import importlib.util
import pathlib
import sys
import unittest.mock

TOOLS = pathlib.Path(__file__).resolve().parent


def load(platform):
    """genstubs with its host pinned. UNDERSCORE is decided at import time from sys.platform, and it changes what the SDK pattern matches, so the module is loaded once per host to test both."""
    with unittest.mock.patch.object(sys, "platform", platform):
        spec = importlib.util.spec_from_file_location(
            f"genstubs_{platform}", TOOLS / "genstubs.py")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
    return mod


# (symbol, should be stubbed, why it is in the list)
MACH_O = [
    ("_GXBegin",            True,  "SDK: Aurora supplies it, we owe a stub without"),
    ("_PADRead",            True,  "SDK"),
    ("_sndSetVolume",       True,  "SDK: MusyX"),
    ("GXBegin",             False, "no underscore is not a Mach-O symbol"),
    ("_getenv",             False, "libc, and stubbing it would shadow the real one"),
    ("__ZN14cFuzzyDebugger4stepEv",     True,  "Itanium: a debug class we owe"),
    ("__ZNSt3__16vectorIiE9push_backERKi", False, "Itanium: libc++"),
    ("__ZNKSt3__14pathE",   False, "Itanium: const member of a libc++ type, "
                                   "the case that shadowed __root_directory"),
    ("__ZNSt10__cxxabiv117__class_type_infoE", False, "Itanium: libc++abi"),
    ("__Znwm",              False, "Itanium: operator new"),
    ("_ZSTD_decompress",    False, "C, not Itanium: zstd, and the leading "
                                   "underscore is Mach-O's. Stubbing it beat "
                                   "the real dylib to the link and aborted "
                                   "Aurora's shader cache at startup"),
    ("_ZSTD_isError",       False, "C, not Itanium: zstd"),
    ("_ZLIB_version",       False, "C, not Itanium: an L after _Z is still not "
                                   "a mangled name when a capital follows"),
]

ELF_COFF = [
    ("GXBegin",             True,  "SDK, no leading underscore off Mach-O"),
    ("PADRead",             True,  "SDK"),
    ("_GXBegin",            False, "a leading underscore is not an ELF/COFF C symbol"),
    ("getenv",              False, "libc"),
    ("_ZN14cFuzzyDebugger4stepEv",      True,  "Itanium: ours"),
    ("_ZNSt3__16vectorIiE9push_backERKi", False, "Itanium: libc++"),
    ("_Znwm",               False, "Itanium: operator new"),
    # The libstdc++ substitution abbreviations.
    ("_ZNSolsEi",           False, "Itanium: So = std::basic_ostream<char>, "
                                   "so this is std::ostream::operator<<(int)"),
    ("_ZNSo5writeEPKcl",    False, "Itanium: std::ostream::write"),
    ("_ZNSolsEPFRSt8ios_baseS0_E", False,
                                   "Itanium: operator<<(ios_base&(*)(ios_base&))"
                                   ", St appears, but not where the old "
                                   "pattern looked"),
    ("_ZNSirsERi",          False, "Itanium: Si = std::basic_istream<char>"),
    ("_ZNSsC1EPKcRKSaIcE",  False, "Itanium: Ss = std::string, Sa = allocator"),
    ("_ZNSdD2Ev",           False, "Itanium: Sd = std::basic_iostream<char>"),
    ("_ZN13StatsGatherer3RunEf", True,
                                   "Itanium: ours, and it starts with a capital"
                                   " S, an identifier carries its length in "
                                   "digits, an abbreviation does not"),
    ("_ZN5Sound4PlayEv",    True,  "Itanium: ours, same shape as above"),
    # MSVC decoration. clang-cl emits these because the C++ ABI comes with the windows-msvc target,
    # not with the driver.
    ("?method@Missing@@QEAAXXZ",        True,  "MSVC: a class we owe"),
    ("?step@cFuzzyDebugger@@QEAAXXZ",   True,  "MSVC: ours, nested name"),
    ("?_Xlength_error@std@@YAXPEBD@Z",  False, "MSVC: namespace std"),
    ("??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@XZ",
                                        False, "MSVC: std::string ctor, std "
                                               "buried mid-name, the reason "
                                               "this is a search and not a match"),
    ("??2@YAPEAX_K@Z",      False, "MSVC: operator new"),
    ("??3@YAXPEAX@Z",       False, "MSVC: operator delete"),
    ("??_U@YAPEAX_K@Z",     False, "MSVC: operator new[]"),
    ("??_V@YAXPEAX@Z",      False, "MSVC: operator delete[]"),
    ("??_7type_info@@6B@",   False, "MSVC: type_info vtable, vcruntime's"),
    ("?_com_issue_error@@YAXJ@Z", False, "MSVC: comsupp's, via comdef.h"),
]


def check_nm_batches(mod):
    """The Windows command-line split must preserve symbols and fail closed."""
    failures = 0
    checked = 0

    calls = []

    def successful_nm(command, **_kwargs):
        calls.append(command)
        symbol = "first_batch" if len(calls) == 1 else "second_batch"
        return mod.subprocess.CompletedProcess(
            command, 0, stdout=symbol + "\n", stderr="")

    # Each fake path is under the per-command limit and the pair is over it.
    files = ["a" * 13000, "b" * 13000]
    with unittest.mock.patch.object(mod.subprocess, "run", successful_nm):
        symbols = mod._nm_symbols("nm", ["-j"], "-u", files)
    checked += 1
    if len(calls) != 2 or symbols != {"first_batch", "second_batch"}:
        failures += 1
        print("    FAIL nm batching did not union both command outputs")

    failed = mod.subprocess.CompletedProcess(
        ["nm"], 1, stdout="", stderr="bad object")
    with unittest.mock.patch.object(mod.subprocess, "run", return_value=failed):
        try:
            mod._nm_symbols("nm", ["-j"], "-u", ["object.o"])
        except SystemExit as exc:
            failed_closed = "bad object" in str(exc)
        else:
            failed_closed = False
    checked += 1
    if not failed_closed:
        failures += 1
        print("    FAIL a nonzero nm result was accepted as an empty symbol set")

    return checked, failures


def main():
    failures = 0
    checked = 0
    for platform, cases in (("darwin", MACH_O), ("linux", ELF_COFF)):
        mod = load(platform)
        print(f"  {platform}: symbol prefix {mod.UNDERSCORE!r}")
        for sym, want, why in cases:
            checked += 1
            got = mod.wanted(sym)
            if got != want:
                failures += 1
                print(f"    FAIL {sym}\n         stub={got} want={want}  ({why})")
    # A stub for a missing variable has to be a variable, and only the MSVC decoration says which a
    # symbol is.
    for sym, want, why in (
        ("?gSebringLoadPackageToVirtualMemory@@3EA", True,
         "global bool; TransitionTask writes it one statement into a match"),
        ("?g_e3_Build@@3EA", True, "global bool, declared extern and never defined"),
        ("?g_pCurrentlyUpdatingCharacter@@3PEAVcCharacter@@EA", True,
         "pointer global; its type contains an @@, so read the first one"),
        ("?CONTROLLER_TEXT@@3PAIA", True, "array global"),
        ("?__float_min@@3PAMA", True, "MSL float limit, not defined here"),
        ("??_7type_info@@6B@", True, "vftable: storage class 7, and ends in @"),
        ("?Update@FlickDetection@@SAXXZ", False, "static member function"),
        ("?dSolveLCP@@YAXHPEAM000H00PEAH@Z", False, "free function"),
        ("?CancelPendingReads@StereoAudioStream@GCAudioStreaming@@UEAAXXZ", False,
         "virtual member function, nested class"),
        ("??$Format@V?$BasicString@DVTempStringAllocator@Detail@@@@H@@YA?AV?$Basic"
         "String@DVTempStringAllocator@Detail@@@@AEBV0@AEBH@Z", False,
         "template function whose FIRST @@ falls inside a template argument"),
        ("_ZN15FlickDetection6UpdateEv", False,
         "Itanium says nothing about data vs function; never claim it does"),
    ):
        checked += 1
        got = mod.is_msvc_data_symbol(sym)
        if got != want:
            failures += 1
            print(f"    FAIL {sym}\n         data={got} want={want}  ({why})")

    helper_checked, helper_failures = check_nm_batches(mod)
    checked += helper_checked
    failures += helper_failures

    # The allowlist gate: every stub the port writes today, in each object format's spelling, has
    # a line; a GX symbol Aurora supplies has none, and so does a listed method with a changed
    # signature, which is the gap an unanchored class name left open.
    for platform, allowed, refused in (
        ("darwin",
         ["_GXPeekARGB", "_GXPokeColorUpdate", "__Z9dSolveLCPiPfS_S_S_iS_S_Pi",
          "__ZN13StatsGatherer3RunEf", "__ZThn72_N13StatsGatherer7GetNameEv",
          "__ZN16TMAnimControllerC1EPKcP5World",
          "__ZN21SkinnedAnimController16CreateGLSkinMeshEP7glModel",
          "__ZN14cFuzzyDebugger8WriteXMLEv"],
         ["_GXSetTevOp", "__ZN7cPlayer6UpdateEv", "_sndSetVolume",
          "__ZN13StatsGatherer3RunEv",              # Run() instead of Run(float)
          "__ZN13StatsGatherer4StopEv",             # another method of a listed class
          "__ZN21SkinnedAnimController16CreateGLSkinMeshEv",
          "__ZN13StatsGatherer3RunEf_extra"]),
        ("linux",
         ["GXPeekARGB", "_Z9dSolveLCPiPfS_S_S_iS_S_Pi", "_ZN13StatsGatherer3RunEf"],
         ["GXSetTevOp", "_ZN7cPlayer6UpdateEv", "_ZN13StatsGatherer3RunEv"]),
        ("win32",
         ["GXPokeARGB", "?dSolveLCP@@YAXHPEAM000H00PEAH@Z",
          "?Run@StatsGatherer@@UEAAXM@Z", "?GetName@StatsGatherer@@UEAAPEBDXZ",
          "??0TMAnimController@@QEAA@PEBDPEAVWorld@@@Z",
          "?CreateGLSkinMesh@SkinnedAnimController@@QEAAXPEAUglModel@@@Z"],
         ["GXSetTevOp", "?Update@cPlayer@@QEAAXXZ",
          "?Run@StatsGatherer@@UEAAXXZ",              # Run() instead of Run(float)
          "?Stop@StatsGatherer@@QEAAXXZ"]),
    ):
        mod = load(platform)
        patterns = mod.load_allowlist()
        checked += 1
        got = mod.unlisted(allowed, patterns)
        if got:
            failures += 1
            print(f"    FAIL {platform}: allowed stubs reported as unlisted: {got}")
        checked += 1
        got = mod.unlisted(refused, patterns)
        if got != refused:
            failures += 1
            print(f"    FAIL {platform}: stubs with no allowlist line were accepted: "
                  f"{[s for s in refused if s not in got]}")
    if failures:
        print(f"\n{failures} of {checked} cases failed")
        return 1
    print(f"\nok: {checked} cases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
