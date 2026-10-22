#!/usr/bin/env python3
"""Exercise real formatted-copy statements and GC ADPCM header conversion on a host.

Run from any directory:
  python3 smstrikers-port/tools/test_text_stream_bounds.py --output .tmp/text-stream-host

Requires Clang with ASan/UBSan and a 64-bit little-endian host. The output directory
must not already exist. This is a finite host regression, not a Vita build.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time

TARGETS = [
    ("src/Game/FE/Cup/CupTickerManager.cpp", "include/Game/FE/Cup/CupTickerManager.h", 1),
    ("src/Game/FE/Overlay/OverlayHandlerWinner.cpp", "include/Game/FE/Overlay/OverlayHandlerWinner.h", 2),
    ("src/Game/FE/Overlay/OverlayHandlerInGameText.cpp", "include/Game/OverlayHandlerInGameText.h", 2),
    ("src/Game/FE/Overlay/OverlayHandlerHUD.cpp", "include/Game/OverlayHandlerHUD.h", 1),
    ("src/Game/FE/BraggingRights.cpp", "include/Game/FE/BraggingRights.h", 3),
    ("src/Game/FE/Overlay/OverlayHandlerGoal.cpp", "include/Game/OverlayHandlerGoal.h", 7),
    ("src/Game/FE/Overlay/OverlayHandlerSummary.cpp", "include/Game/FE/Overlay/OverlayHandlerSummary.h", 2),
    ("src/Game/SH/SHMilestoneTrophy.cpp", "include/Game/SH/SHMilestoneTrophy.h", 4),
    ("src/Game/SH/SHLoading.cpp", "include/Game/SH/SHLoading.h", 1),
]
COPY = re.compile(r"^[ \t]*(?:memcpy|nlStrNCpy)\((\w+(?:\[i\])?), (\w+)\.c_str\(\), [^;]+\);", re.M)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def balanced(text, needle, semicolon=False):
    start = text.index(needle)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        depth += (text[pos] == "{") - (text[pos] == "}")
        if depth == 0:
            end = pos + 1
            if semicolon:
                if text[end] != ";":
                    raise RuntimeError("Expected declaration terminator")
                end += 1
            return text[start:end]
    raise RuntimeError("Unclosed source fragment: " + needle)

def run(name, argv, output, environment, timeout):
    row = {"name": name, "command": argv, "timeout_seconds": timeout}
    begin = time.monotonic()
    with (output / (name + ".stdout")).open("xb") as stdout, \
         (output / (name + ".stderr")).open("xb") as stderr:
        child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, cwd=output,
                                 env=environment, start_new_session=True)
        try:
            row["returncode"] = child.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            row["timed_out"] = True
            os.killpg(child.pid, signal.SIGTERM)
            try:
                row["returncode"] = child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                row["returncode"] = child.wait(timeout=3)
    row["seconds"] = round(time.monotonic() - begin, 3)
    row["stdout_sha256"] = sha(output / (name + ".stdout"))
    row["stderr_sha256"] = sha(output / (name + ".stderr"))
    print(json.dumps({"command": name, "returncode": row["returncode"],
                      "seconds": row["seconds"]}), flush=True)
    return row

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--fixture-dir", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", default=shutil.which("clang++"))
    args = parser.parse_args()
    root, output = args.project_root.resolve(), args.output.resolve()
    fixture = (args.fixture_dir or Path(__file__).resolve().parents[1] /
               "tests/text_stream_bounds").resolve()
    if not args.compiler:
        raise RuntimeError("clang++ was not found")
    if output.exists():
        raise RuntimeError("Refuse to overwrite an existing output directory")
    for filename in ("fixture_support.hpp", "text_test.cpp", "header_test.cpp"):
        if not (fixture / filename).is_file():
            raise RuntimeError("Missing fixture asset " + filename)
    if not (root / "include/port/prelude.h").is_file():
        raise RuntimeError("--project-root must name smstrikers-port")
    output.mkdir(parents=True)
    generated = output / "generated"
    for relative in ("extracted", "provider/NL", "provider/dolphin"):
        (generated / relative).mkdir(parents=True, exist_ok=True)
    (output / "tmp").mkdir()
    source_hashes, sites, wrappers = {}, [], []
    def read(relative):
        path = root / relative
        source_hashes[relative] = sha(path)
        return path.read_text()
    def write(relative, text):
        (generated / relative).write_text(text)

    for source, header, expected in TARGETS:
        text, declarations = read(source), read(header)
        matches = list(COPY.finditer(text))
        if len(matches) != expected:
            raise RuntimeError("Update this finite fixture after copy-site changes: " + source)
        for match in matches:
            destination, argument = match.group(1), match.group(2)
            indexed = "[i]" in destination
            name = destination.split("[")[0]
            fields = [line for line in declarations.splitlines()
                      if re.search(r"\b" + re.escape(name) + r"\[", line)
                      and line.count("[") == (2 if indexed else 1)]
            if len(fields) != 1:
                raise RuntimeError("Ambiguous destination declaration: " + name)
            field = fields[0]
            dimensions = [int(n) for n in re.findall(r"\[(\d+)\]", field)]
            if not dimensions or dimensions[-1] <= 0:
                raise RuntimeError("Unbounded destination")
            index = len(sites)
            line = text[:match.start()].count("\n") + 1
            statement = match.group(0).strip()
            terminator = "mDescriptionBuffer[127] = 0;" if re.match(
                r"\s*\n\s*mDescriptionBuffer\[127\] = 0;", text[match.end():]) else ""
            mutation = "memcpy(" + destination + ", " + argument + ".c_str(), sizeof(" + destination + "));"
            wrappers.append("\n".join([
                "struct CopySite" + str(index) + " {", "    uint64_t before;", field,
                "    uint64_t after;",
                "    unsigned short* buffer(int i) { return " + destination + "; }",
                "    size_t capacity(int i) const { return sizeof(" + destination + ") / sizeof(" + destination + "[0]); }",
                "    __attribute__((noinline)) void copy(const WideBasicString& " + argument + ", int i) {",
                "#if defined(USE_UNBOUNDED_MUTATION)", "        " + mutation, "#else",
                '#line ' + str(line) + ' "' + source + '"', "        " + statement, "#endif",
                "        " + terminator, "    }", "};", ""
            ]))
            sites.append({"path": source, "line": line, "capacity": dimensions[-1],
                          "rows": dimensions[0] if indexed else 1, "statement": statement})
    if len(sites) != 23:
        raise RuntimeError("Expected all 23 retained copy sites")
    write("copy_sites.inc", "\n".join(wrappers))
    write("run_sites.inc", "\n".join(
        'run_copy_site<CopySite' + str(i) + '>("' + site["path"].split("/")[-1] +
        ":" + str(site["line"]) + '", ' + str(site["rows"]) + ");"
        for i, site in enumerate(sites)) + "\n")
    write("extracted/nlvector3.inc", balanced(read("include/NL/nlMath.h"), "class nlVector3\n", True) + "\n")
    text_header = read("include/Game/FE/tlTextInstance.h")
    fields = []
    for name in ("m_OverloadFlags", "m_pFontString", "m_wcUserString"):
        matches = [line for line in text_header.splitlines() if re.search(r"\b" + name + r";", line)]
        if len(matches) != 1:
            raise RuntimeError("Ambiguous text field: " + name)
        fields.append(matches[0])
    write("extracted/text_fields.inc", "\n".join(fields) + "\n")
    write("extracted/set_string.inc", balanced(
        read("src/Game/FE/tlTextInstance_runtime.cpp"),
        "void TLTextInstance::SetString(const unsigned short* utf16)") + "\n")
    write("provider/NL/nlMath.h",
          '#ifndef TEXT_STREAM_NLMATH_H\n#define TEXT_STREAM_NLMATH_H\n#include "types.h"\n#include "extracted/nlvector3.inc"\n#endif\n')
    write("provider/dolphin/os.h",
          '#ifndef TEXT_STREAM_OS_H\n#define TEXT_STREAM_OS_H\n#include "types.h"\n#endif\n')
    write("provider/printf.h",
          '#ifndef TEXT_STREAM_PRINTF_H\n#define TEXT_STREAM_PRINTF_H\n#include <stdio.h>\n#include <stdarg.h>\n#endif\n')
    write("dsp_header.inc", balanced(read("include/Game/Sys/GCStream.h"), "struct sDSPADPCM\n", True) + "\n")
    stream = read("src/Game/Sys/GCStream.cpp")
    start = stream.index("\n{", stream.index("void GCAudioStreaming::AudioStream::_HdrReadCB")) + 2
    end = stream.index("\n    READ_CB_INFO* pCBInfo", start)
    block = stream[start:end]
    rate = "        h->sample_rate = port_be32(&h->sample_rate);\n"
    if block.count(rate) != 1:
        raise RuntimeError("Expected the named sample-rate conversion")
    write("convert_header.inc",
          "static void convert_after(void* pData, unsigned int Length)\n{" + block + "\n}\n" +
          "static void convert_missing_rate(void* pData, unsigned int Length)\n{" +
          block.replace(rate, "") + "\n}\n")
    for relative in ("include/port/prelude.h", "include/port/endian.h",
                     "include/NL/nlBasicString.h", "include/NL/nlVector.h",
                     "include/NL/nlstring_tmpl.h", "include/NL/nlFormat.h",
                     "include/NL/nlString.h", "include/NL/nlMemory.h"):
        read(relative)
    result = {"schema": "text-stream-bounds-host-v1", "status": "running",
              "source_hashes": source_hashes, "copy_sites": sites, "steps": [],
              "hardware_status": "not_run",
              "limits": "Host LP64; scene construction, localization fitting, DSP playback and target ABI are outside this fixture."}
    environment = {"PATH": "/usr/bin:/bin:/usr/sbin:/sbin", "LC_ALL": "C",
                   "TMPDIR": str(output / "tmp"),
                   "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1:allocator_may_return_null=1",
                   "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"}
    flags = [args.compiler, "-std=c++20", "-O1", "-g0", "-UNDEBUG",
             "-fshort-wchar", "-fno-omit-frame-pointer", "-fno-strict-aliasing", "-fsigned-char",
             "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-DTARGET_PC=1", "-fno-modules"]
    if sys.platform == "darwin":
        sdk = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True,
                             text=True, timeout=10, check=True).stdout.strip()
        flags += ["-isysroot", sdk]
    includes = ["-I", str(generated), "-I", str(fixture), "-I", str(generated / "provider"),
                "-I", str(root / "include")]
    text_args = flags + ["-include", str(root / "include/port/prelude.h")] + includes + [str(fixture / "text_test.cpp")]
    try:
        for name, argv in [
            ("compile-text", text_args + ["-o", str(output / "text")]),
            ("compile-text-mutation", text_args + ["-DUSE_UNBOUNDED_MUTATION=1", "-o", str(output / "text-mutation")]),
            ("compile-header", flags + includes + [str(fixture / "header_test.cpp"), "-o", str(output / "header")]),
        ]:
            row = run(name, argv, output, environment, 90)
            result["steps"].append(row)
            if row["returncode"] != 0 or row.get("timed_out"):
                raise RuntimeError(name + " failed; inspect its stderr")
        for name, argv, expected in [
            ("text", [str(output / "text"), "all"], "pass"),
            ("header", [str(output / "header"), "after"], "pass"),
            ("text-unbounded", [str(output / "text-mutation"), "short-witness"], "heap-overflow"),
            ("header-missing-rate", [str(output / "header"), "missing-rate"], "assertion"),
        ]:
            row = run(name, argv, output, environment, 15)
            result["steps"].append(row)
            stderr = (output / (name + ".stderr")).read_text(errors="replace")
            passed = row["returncode"] == 0 if expected == "pass" else (
                row["returncode"] != 0 and (
                    "AddressSanitizer: heap-buffer-overflow" in stderr if expected == "heap-overflow"
                    else "Assertion failed" in stderr or "Assertion " in stderr))
            row.update(expected=expected, expectation_met=passed and not row.get("timed_out", False))
            if not row["expectation_met"]:
                raise RuntimeError(name + " did not meet its expected outcome")
        for relative, value in source_hashes.items():
            if sha(root / relative) != value:
                raise RuntimeError("Source changed during the test: " + relative)
        result["status"] = "pass"
    except BaseException as error:
        result.update(status="failed", error=type(error).__name__ + ": " + str(error))
    finally:
        (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"status": result["status"], "result": str(output / "result.json")}), flush=True)
    return 0 if result["status"] == "pass" else 1

if __name__ == "__main__":
    raise SystemExit(main())
