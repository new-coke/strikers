#!/usr/bin/env python3
"""Test the production frontend audio lifecycle with isolated host boundaries.

The five function bodies and scene enum are extracted unchanged from the current
sources. Each process starts with a fresh function-local gAlreadyBooted and
executes two frontend visits. This is an offline lifecycle regression, not a
device, asset-loading or decoder test.
"""
import argparse
import hashlib
import itertools
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

FUNCTIONS = (
    ("src/Game/Audio/AudioLoader.cpp", "void AudioLoader::LoadFE(bool bLoadSampleFile)"),
    ("src/Game/Audio/AudioLoader.cpp", "void AudioLoader::UnloadInGame()"),
    ("src/Game/Audio/AudioLoader.cpp", "void AudioLoader::PlayFETitleMusicWithFade()"),
    ("src/Game/SH/SHTitleScreen.cpp", "void TitleScene::SceneCreated()"),
    ("src/Game/TransitionTask.cpp", "void TransitionTask::InitializeFEState()"),
)
TOKENS = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def extract(source, signature):
    if source.count(signature) != 1:
        raise ValueError(f"Expected exactly one production definition: {signature}")
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for token in TOKENS.finditer(source, opening):
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return source[start:token.end()], source[:start].count("\n") + 1
    raise ValueError(f"Unclosed production definition: {signature}")


def run(argv, *, cwd, env=None, timeout=60):
    started = time.time()
    try:
        result = subprocess.run(argv, cwd=cwd, env=env, capture_output=True,
                                text=True, timeout=timeout)
        return dict(argv=argv, returncode=result.returncode,
                    elapsed_s=time.time() - started, stdout=result.stdout,
                    stderr=result.stderr, timeout=False)
    except subprocess.TimeoutExpired as error:
        def text(value):
            return value.decode(errors="replace") if isinstance(value, bytes) else value or ""
        return dict(argv=argv, returncode=None, elapsed_s=time.time() - started,
                    stdout=text(error.stdout), stderr=text(error.stderr), timeout=True)


def write_result(directory, name, result):
    path = directory / (name + ".log")
    path.write_text("argv: " + json.dumps(result["argv"]) + "\n"
                    + "returncode: " + str(result["returncode"]) + "\n"
                    + "elapsed_s: " + str(result["elapsed_s"]) + "\n"
                    + "--- stdout ---\n" + result["stdout"]
                    + "--- stderr ---\n" + result["stderr"])
    return {"path": str(path), "sha256": sha(path.read_bytes()),
            "returncode": result["returncode"], "elapsed_s": result["elapsed_s"],
            "timeout": result["timeout"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phase", choices=("before", "after"), required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--compiler", default="clang++")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    repo = root.parent
    output = args.out.resolve()
    scratch = args.work_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    scratch.mkdir(parents=True, exist_ok=True)
    # Keep failed attempts reviewable instead of replacing their compiler logs.
    attempt = 1
    while (output / f"{args.phase}-{attempt}-gate.json").exists():
        attempt += 1
    prefix = f"{args.phase}-{attempt}"
    source_hashes, function_records, body_parts = {}, [], []
    for relative, signature in FUNCTIONS:
        path = root / relative
        source = path.read_text()
        label = str(path.relative_to(repo))
        source_hashes[label] = sha(path.read_bytes())
        body, line = extract(source, signature)
        function_records.append({"path": label, "signature": signature,
                                 "line": line, "line_count": body.count("\n") + 1,
                                 "body_sha256": sha(body.encode())})
        body_parts.append(f'#line {line} "{label}"\n{body}\n')
    bodies = "\n".join(body_parts)
    enum_path = root / "include/Game/BaseGameSceneManager.h"
    enum_source = enum_path.read_text()
    enum, enum_line = extract(enum_source, "enum SceneList")
    source_hashes[str(enum_path.relative_to(repo))] = sha(enum_path.read_bytes())
    enum = f'#line {enum_line} "{enum_path.relative_to(repo)}"\n{enum};\n'
    fixture_path = root / "tests/fe_audio_lifecycle_test.cpp"
    template = fixture_path.read_text()
    fixture = template
    source_hashes[str(fixture_path.relative_to(repo))] = sha(fixture_path.read_bytes())
    source_hashes[str(Path(__file__).resolve().relative_to(repo))] = sha(Path(__file__).read_bytes())
    for marker, content in (
            ("// INSERT_SCENE_ENUM", enum),
            ("// INSERT_PRODUCTION_BODIES", bodies)):
        if fixture.count(marker) != 1:
            raise ValueError(f"Expected one fixture marker: {marker}")
        following_line = template[:template.index(marker)].count("\n") + 2
        restore = f'#line {following_line} "{fixture_path.relative_to(repo)}"\n'
        fixture = fixture.replace(marker + "\n", content + restore)
    generated = scratch / (prefix + "-driver.cpp")
    binary = scratch / (prefix + "-driver")
    generated.write_text(fixture)
    bounded_source = output / (prefix + "-production-bodies.cpp")
    bounded_source.write_text(bodies)
    compiler = shutil.which(args.compiler)
    if not compiler:
        raise ValueError(f"Compiler not found: {args.compiler}")
    compile_argv = [compiler, "-std=c++20", "-O1", "-g", "-UNDEBUG",
                    "-fdeclspec", "-fshort-wchar", "-Wno-register",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    str(generated), "-o", str(binary)]
    version = run([compiler, "--version"], cwd=repo)
    build = run(compile_argv, cwd=repo)
    gate = {
        "schema": "strikers-fe-audio-lifecycle-v1",
        "phase": args.phase, "attempt": attempt, "created_unix": time.time(),
        "status": "failed", "compiler": version["stdout"].splitlines()[0],
        "source_sha256": source_hashes, "functions": function_records,
        "production_bodies": {"path": str(bounded_source),
                              "sha256": sha(bounded_source.read_bytes())},
        "generated_source_sha256": sha(generated.read_bytes()),
        "compile": write_result(output, prefix + "-compile", build),
        "sanitizers": ["address", "undefined"],
        "sanitizer_environment": {
            "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
            "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
        },
        "cases": [], "limitations": [
            "Track storage, platform sound I/O and frontend managers are host boundaries.",
            "The first Save/Legal-to-Title advance is simulated after the real InitializeFEState.",
            "The second real InitializeFEState follows its existing demo-to-Title branch.",
            "ASan and UBSan are enabled; macOS leak detection is disabled, with explicit track balance checks.",
            "This does not attest device playback, all frontend branches, or resolution of the hardware crash.",
        ],
    }
    if build["returncode"] == 0:
        if args.phase == "before":
            cases = [
                ("normal-control", (0, 1, 1, 1, 75, 1), "pass"),
                ("skip-order", (1, 1, 1, 1, 75, 1), "order-failure"),
                ("skip-title-play-null", (1, 1, 1, 1, 75, 0), "null-member"),
                ("skip-title-stop-null", (1, 0, 1, 1, 0, 0), "null-member"),
            ]
        else:
            cases = [
                (f"skip{s}-io{i}-audio{a}-streams{t}-volume{v}",
                 (s, i, a, t, v, 1), "pass")
                for s, i, a, t, v in itertools.product((0, 1), (0, 1),
                                                       (0, 1), (0, 1), (0, 75))
            ]
        env = dict(os.environ, **gate["sanitizer_environment"])
        for name, values, expectation in cases:
            result = run([str(binary), *map(str, values)], cwd=repo, env=env, timeout=15)
            combined = result["stdout"] + result["stderr"]
            if expectation == "pass":
                accepted = result["returncode"] == 0 and "FE audio lifecycle PASS " in combined
            elif expectation == "order-failure":
                accepted = result["returncode"] == 86 and "g_pTrackManager && g_pTrackManager->ready()" in combined
            else:
                accepted = (result["returncode"] not in (None, 0)
                            and "member call on null pointer" in combined
                            and "src/Game/Audio/AudioLoader.cpp:" in combined)
            accepted = accepted and not result["timeout"]
            gate["cases"].append({"name": name, "arguments": values,
                                  "expectation": expectation, "accepted": accepted,
                                  **write_result(output, prefix + "-" + name, result)})
        gate["status"] = "passed" if all(case["accepted"] for case in gate["cases"]) else "failed"
    gate["case_count"] = len(gate["cases"])
    gate["accepted_count"] = sum(case["accepted"] for case in gate["cases"])
    gate_path = output / (prefix + "-gate.json")
    gate_path.write_text(json.dumps(gate, indent=2) + "\n")
    print(json.dumps({"gate": str(gate_path), "sha256": sha(gate_path.read_bytes()),
                      "status": gate["status"], "accepted": gate["accepted_count"],
                      "cases": gate["case_count"], "compile_rc": build["returncode"]}))
    if build["returncode"] != 0:
        print(build["stderr"], file=sys.stderr)
    for case in gate["cases"]:
        if not case["accepted"]:
            print(f"Unexpected result: {case['name']} -> {case['path']}", file=sys.stderr)
    return 0 if gate["status"] == "passed" else 1


if __name__ == "__main__":
    sys.exit(main())
