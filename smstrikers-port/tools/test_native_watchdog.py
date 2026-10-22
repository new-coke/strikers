#!/usr/bin/env python3
"""Validate the real SDK-mock driver's records and critical integration order."""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--driver", required=True)
    args = parser.parse_args()
    run = subprocess.run([args.driver], capture_output=True, text=True, timeout=15)
    if run.returncode:
        raise AssertionError(f"driver failed ({run.returncode})\n{run.stdout}\n{run.stderr}")
    records = []
    for line in run.stdout.splitlines():
        if not line:
            continue
        record = json.loads(line)
        if "watchdog_test" in record:
            assert record == {"watchdog_test": "PASS", "cases": 17}
            continue
        # Include both framing newlines in the 2 KiB bounded record contract.
        assert len(line.encode()) + 2 <= 2048
        assert record["v"] == 1 and record["pid"] == 1234
        assert isinstance(record["boot_us"], int) and record["boot_us"] > 0
        assert isinstance(record["instance"], int) and record["instance"] > 0
        if record["kind"] in ("run", "exit"):
            assert len(record["threads"]) == 5
            assert [row["role"] for row in record["threads"]] == [
                "frontend", "renderer", "jobs", "audio_output", "audio_loader"]
            for row in record["threads"]:
                kernel = row["kernel"]
                if kernel is not None:
                    assert isinstance(kernel, list) and all(isinstance(x, int) for x in kernel)
                    assert len(kernel) == (1 if kernel[0] < 0 else 9)
                if row.get("changing"):
                    assert kernel is None
        records.append(record)
    assert len(records) >= 20
    assert any(row["stage"] == "native_audio_init" and row["live"] == 1
               for record in records for row in record.get("threads", []))
    assert any(row["kernel"] and len(row["kernel"]) == 9 and row["kernel"][2] == 8
               for record in records for row in record.get("threads", []))
    assert any(row["kernel"] and len(row["kernel"]) == 9 and row["kernel"][2] == 256
               for record in records for row in record.get("threads", []))
    assert any(row["kernel"] == [-88] for record in records for row in record.get("threads", []))
    assert any(row["kernel"] and len(row["kernel"]) == 9 and row["kernel"][5] == (1 << 64) - 1
               for record in records for row in record.get("threads", []))
    assert any(row.get("changing") for record in records for row in record.get("threads", []))
    same_label = [r for r in records if r["kind"] == "boot" and r["label"] == "boot-a"]
    assert len({(r["boot_us"], r["instance"]) for r in same_label}) == 2

    root = Path(__file__).resolve().parents[1]
    source = (root / "src/Game/main.cpp").read_text()
    source = source[source.index("int main(int argc,"):]
    positions = [source.index(token) for token in [
        "(void)PortConfigLoad();", "port::watchdog::Session watchdogSession",
        "port::watchdog::Stage::VerifyArchive", "port::assets::verify_archive_if_requested",
        "port::watchdog::Stage::NativeAudioInit", "PortNativeAudioInitialize();",
        "port::watchdog::Stage::NativeAudioReady"]]
    assert positions == sorted(positions)
    assert source.index("watchdogSession.finish()") < source.index("sceKernelExitProcess(0)")
    module = (root / "src/platform/native_watchdog.cpp").read_text()
    for forbidden in ("fprintf(", "printf(", "fopen(", "fwrite(", "malloc(", "getenv(",
                      "pthread_mutex_", "PortAudioMixStats", "sceKernelPowerTick"):
        assert forbidden not in module, forbidden
    print(json.dumps({"status": "PASS", "records": len(records),
                      "max_record_bytes": max(len(json.dumps(r, separators=(",", ":")).encode()) + 2
                                              for r in records),
                      "sdk_mock_cases": 17, "boot_order": "PASS", "record_schema": "PASS"}))


if __name__ == "__main__":
    main()
