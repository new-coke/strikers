#!/usr/bin/env python3
"""Isolated before/after regression for Vita audio-option preservation.

Production option, Revert and serialization bodies are compiled unchanged.
A second binary uses the original SRAM getters/setters to check that the
non-Vita reconciliation retains its console semantics. No device or save I/O.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import sys
import time

from test_fe_audio_lifecycle import extract, run, sha, write_result

FUNCTIONS = (
    ("src/Game/DB/UserOptions.cpp", "AudioSettings::AudioSettings()"),
    ("src/Game/DB/UserOptions.cpp", "void AudioSettings::InitializeDefaults()"),
    ("src/Game/DB/UserOptions.cpp", "void AudioSettings::ForceApplySettings(bool bUpdateMode)"),
    ("src/Game/DB/UserOptions.cpp", "void AudioSettings::ApplySettings(bool bApplyMode, bool bUpdateMode)"),
    ("src/Game/FE/feOptionsSubMenus.cpp", "void OptionsAudioMenuV2::Revert()"),
    ("src/Game/GameInfo.cpp", "unsigned long GameInfoManager::GetMemoryCardDataSize() const"),
    ("src/Game/GameInfo.cpp", "void GameInfoManager::GetMemoryCardData(void* pData)"),
    ("src/Game/GameInfo.cpp", "void GameInfoManager::SetMemoryCardData(void* pData)"),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phase", choices=("before", "after"), required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--compiler", default="clang++")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    repo = root.parent
    output, scratch = args.out.resolve(), args.work_dir.resolve()
    if not output.is_relative_to(repo) or not scratch.is_relative_to(repo):
        raise ValueError("Keep this gate and its scratch products in the selected checkout")
    output.mkdir(parents=True, exist_ok=True)
    scratch.mkdir(parents=True, exist_ok=True)
    attempt = 1
    while (output / f"{args.phase}-{attempt}-gate.json").exists():
        attempt += 1
    prefix = f"{args.phase}-{attempt}"
    hashes, records = {}, []

    def read(relative):
        path = root / relative
        data = path.read_bytes()
        label = str(path.relative_to(repo))
        hashes[label] = sha(data)
        return data.decode(), label

    def definition(relative, signature, semicolon=False):
        source, label = read(relative)
        body, line = extract(source, signature)
        records.append(dict(path=label, signature=signature, line=line,
                            line_count=body.count("\n") + 1,
                            body_sha256=sha(body.encode())))
        return f'#line {line} "{label}"\n{body}' + (";" if semicolon else "") + "\n"

    declarations = definition("include/Game/DB/UserOptions.h", "enum eAudioMode", True)
    declarations += definition("include/Game/DB/UserOptions.h", "class AudioSettings", True)
    os_header, _ = read("include/dolphin/os/OSRtc.h")
    for name in ("OS_SOUND_MODE_MONO", "OS_SOUND_MODE_STEREO"):
        constants = re.findall(r"^#define " + name + r" [01]$", os_header, re.M)
        if len(constants) != 1:
            raise ValueError(f"Expected one original sound-mode constant: {name}")
        declarations += constants[0] + "\n"
    production = "\n".join(definition(path, signature) for path, signature in FUNCTIONS)
    os_source, os_label = read("src/platform/os.c")
    language, _ = extract(os_source, "u8 OSGetLanguage(void)")
    start = os_source.index("u8 OSGetLanguage(void)") + len(language)
    end = os_source.index("u32 OSGetProgressiveMode(void)", start)
    os_block = os_source[start:end]
    # Retain the complete declaration/conditional section, including any state.
    if os_block.count("OSGetSoundMode(void)") not in (1, 2) or os_block.count("OSSetSoundMode(u32 mode)") not in (1, 2):
        raise ValueError("Unexpected sound-mode section")
    os_line = os_source[:start].count("\n") + 1
    native_os = f'#line {os_line} "{os_label}"\n{os_block}\n'
    records.append(dict(path=os_label, signature="complete sound-mode section",
                        line=os_line, line_count=os_block.count("\n") + 1,
                        body_sha256=sha(os_block.encode())))
    retail_os = definition("src/Dolphin/os/OSRtc.c", "u32 OSGetSoundMode(void)")
    retail_os += definition("src/Dolphin/os/OSRtc.c", "void OSSetSoundMode(u32 mode)")
    os_bodies = "#if defined(TEST_RETAIL_SRAM)\n" + retail_os + "#else\n" + native_os + "#endif\n"

    # The audio-bearing prefix is represented at the documented Vita extent;
    # fail if its declaration/offset contract changes instead of testing a stale model.
    user_info, _ = read("include/Game/GameInfo.h")
    required = (
        r"/\* 0x0000 \*/ unsigned long mSaveID;",
        r"/\* 0x0004 \*/ AudioSettings mAudioOptions;",
        r"}; // total size: 0x113C",
    )
    if not all(re.search(item, user_info) for item in required):
        raise ValueError("Vita UserInfo boundary layout changed")
    read("src/Game/DB/SaveLoad.cpp")  # inspected size/dataCRC/iconCRC boundary
    read("src/Game/SH/SHSaveLoad.cpp")
    read("src/Game/SH/SHCrossFader.cpp")
    template, fixture_label = read("tests/audio_options_vita_test.cpp")
    read("tools/test_audio_options_vita.py")
    read("tools/test_fe_audio_lifecycle.py")
    fixture = template
    for marker, content in (
        ("// INSERT_AUDIO_DECLARATIONS", declarations),
        ("// INSERT_OS_BODIES", os_bodies),
        ("// INSERT_PRODUCTION_BODIES", production),
    ):
        if template.count(marker) != 1:
            raise ValueError(f"Expected one marker: {marker}")
        following_line = template[:template.index(marker)].count("\n") + 2
        fixture = fixture.replace(marker + "\n",
                                  content + f'#line {following_line} "{fixture_label}"\n')
    generated = scratch / (prefix + "-driver.cpp")
    generated.write_text(fixture)
    bounded = output / (prefix + "-production-bodies.cpp")
    bounded.write_text(declarations + os_bodies + production)
    compiler = shutil.which(args.compiler)
    if not compiler:
        raise ValueError(f"Compiler unavailable: {args.compiler}")
    version = run([compiler, "--version"], cwd=repo)
    gate = {
        "schema": "strikers-audio-options-v1", "phase": args.phase,
        "attempt": attempt, "created_unix": time.time(), "status": "failed",
        "compiler": version["stdout"].splitlines()[0],
        "source_sha256": hashes, "functions": records,
        "production_bodies": {"path": str(bounded), "sha256": sha(bounded.read_bytes())},
        "generated_source_sha256": sha(generated.read_bytes()),
        "sanitizers": ["address", "undefined"],
        "sanitizer_environment": {"ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
                                  "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"},
        "compiles": [], "cases": [],
        "limits": [
            "Production AudioSettings, Revert and GameInfo serialization bodies are extracted unchanged.",
            "AudioSettings declarations and enum are extracted unchanged; other UserInfo fields are opaque at the documented Vita extent.",
            "The eleven tournament serializers, audio output/track manager, menu constructor and configuration reader are host boundaries.",
            "Save filesystem, CRC callback execution, actual tournament serialization and audible output are not exercised by this gate.",
            "The loaded Mode copy boundary is exercised after the separately inspected unchanged size/dataCRC/iconCRC checks.",
            "Original console SRAM getters/setters execute against an isolated lock/store to qualify the non-Vita branch.",
            "Reboot is modeled by resetting the in-process OS preference before deserializing an existing payload.",
            "No new persistence file or save format is introduced; this is not device audio or whole-game acceptance.",
        ],
    }
    binaries = {}
    for variant, defines in (("vita", ["-DPORT_VITA=1", "-DSTRIKERS_VITA=1"]),
                              ("retail", ["-DTEST_RETAIL_SRAM=1"])):
        binary = scratch / (prefix + "-" + variant)
        argv = [compiler, "-std=c++20", "-O1", "-g", "-UNDEBUG", "-fshort-wchar",
                "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                *defines, str(generated), "-o", str(binary)]
        result = run(argv, cwd=repo, timeout=60)
        gate["compiles"].append({"variant": variant, **write_result(output, prefix + "-" + variant + "-compile", result)})
        if result["returncode"] == 0:
            binaries[variant] = binary
        else:
            print(result["stderr"], file=sys.stderr)
    if len(binaries) == 2:
        if args.phase == "before":
            cases = [("vita", "stereo-control", "pass"),
                     ("retail", "suite", "pass"),
                     ("vita", "reboot-mono", "regression"),
                     ("vita", "revert-mono", "regression"),
                     ("vita", "system-readback", "regression")]
        else:
            cases = [("vita", "suite", "pass"), ("retail", "suite", "pass")]
        env = dict(os.environ, **gate["sanitizer_environment"])
        for variant, selection, expected in cases:
            result = run([str(binaries[variant]), selection], cwd=repo, env=env, timeout=15)
            combined = result["stdout"] + result["stderr"]
            if expected == "pass":
                accepted = result["returncode"] == 0 and "Audio options PASS " in combined
            else:
                accepted = result["returncode"] == 86 and f"AUDIO_OPTIONS_FAIL {selection} " in result["stderr"]
            accepted = accepted and not result["timeout"]
            counts = re.search(r"scenarios=(\d+) checks=(\d+)", result["stdout"])
            gate["cases"].append({
                "variant": variant, "selection": selection, "expectation": expected,
                "accepted": accepted, "scenarios": int(counts[1]) if counts else None,
                "checks": int(counts[2]) if counts else None,
                **write_result(output, f"{prefix}-{variant}-{selection}", result),
            })
        gate["status"] = "passed" if all(case["accepted"] for case in gate["cases"]) else "failed"
    gate["case_count"] = len(gate["cases"])
    gate["accepted_count"] = sum(case["accepted"] for case in gate["cases"])
    path = output / (prefix + "-gate.json")
    path.write_text(json.dumps(gate, indent=2) + "\n")
    print(json.dumps({"gate": str(path), "sha256": sha(path.read_bytes()),
                      "status": gate["status"], "accepted": gate["accepted_count"],
                      "cases": gate["case_count"], "compiles": len(binaries)}))
    for case in gate["cases"]:
        if not case["accepted"]:
            print("Unexpected result: " + case["path"], file=sys.stderr)
    return 0 if gate["status"] == "passed" else 1


if __name__ == "__main__":
    sys.exit(main())
