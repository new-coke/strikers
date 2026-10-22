#!/usr/bin/env python3
"""Bind a local-only tester VPK to its clean Git source and quiet configuration."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    sha = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            sha.update(block)
    return {"path": path.name, "bytes": path.stat().st_size, "sha256": sha.hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    if build != ROOT / "build-vita-gxm-latest":
        parser.error("Use only the shared build-vita-gxm-latest directory")
    status = subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True)
    if status.strip():
        parser.error("Commit source changes before creating a tester release manifest")
    cache = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if "=" in line and ":" in line.split("=", 1)[0]:
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    required = {"CMAKE_BUILD_TYPE": "Release", "CMAKE_HOME_DIRECTORY": str(ROOT),
        "STRIKERS_AURORA": "OFF", "STRIKERS_VITA_VITAGL": "ON",
        "STRIKERS_VITA_NO_LOGS": "ON", "STRIKERS_VITA_PROFILER": "OFF",
        "STRIKERS_VITA_PROFILE_AUTOSTART": "OFF", "STRIKERS_VITA_AVC_CONTINUITY_PROBE": "OFF"}
    for key, value in required.items():
        if cache.get(key) != value:
            parser.error(f"Release cache mismatch: {key}={cache.get(key)!r}, expected {value!r}")
    ini = {}
    for line in (build / "strikers.ini").read_text().splitlines():
        if line.strip() and not line.lstrip().startswith("#"):
            key, value = (word.strip() for word in line.split("=", 1))
            if key in ini:
                parser.error(f"Duplicate release setting: {key}")
            ini[key] = value
    normal = {"diagnostics": "0", "fps_overlay": "0", "benchmark": "0",
        "vita_test_match": "0", "vita_frameskip": "0", "vita_watchdog": "0",
        "task_profile": "0", "asset_io_profile": "0", "vitagl_timing": "0",
        "vitagl_packet_profile": "0", "vitagl_uniform_profile": "0", "vitagl_uniform_verify": "0",
        "frame_capture_frames": "0", "frame_capture": "", "vita_control_trace": "",
        "vita_snapshot_request": "", "vita_snapshot_match_frame": "0", "vita_snapshot_play_frame": "0",
        "vita_avc_continuity_probe": "0", "vita_view_draw_capture": "0", "log": "",
        "vita_playstation_glyphs": "1", "vita_video_player": "avplayer", "vita_video_archive": ""}
    for key, value in normal.items():
        if ini.get(key) != value:
            parser.error(f"Release INI mismatch: {key}={ini.get(key)!r}")
    names = ("strikers", "strikers.map", "strikers.velf", "strikers_vita.self", "strikers_vita.vpk", "strikers.ini")
    report = {"schema": "strikers-local-release-candidate-v1",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "source_clean": True, "version": cache.get("STRIKERS_VERSION"),
        "build_settings": required, "runtime_settings": ini,
        "files": {name: digest(build / name) for name in names},
        "publication": "local handoff only; VPK is not uploaded to GitHub",
        "hardware_acceptance": "pending for this exact release artifact; earlier C9 measurements are separate"}
    target = build / "release-candidate.json"
    target.write_text(json.dumps(report, indent=2) + "\n")
    (build / "TESTING.md").write_bytes((ROOT.parent / "TESTING_RC.md").read_bytes())
    print(json.dumps({"manifest": str(target), "commit": report["commit"],
                      "vpk": report["files"]["strikers_vita.vpk"]}, indent=2))


if __name__ == "__main__":
    main()
