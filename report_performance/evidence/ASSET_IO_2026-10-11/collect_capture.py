#!/usr/bin/env python3
"""Collect one sealed quiet device run; keep all identity evidence locally."""
import json
from pathlib import Path
import sys
import time
from ftplib import error_perm

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "smstrikers-port/tools"))
import vita_workflow_device as device
from analyze_vita_frames import analyze
from analyze_vita_asset_io import analyze as analyze_io

folder = Path(sys.argv[1]).resolve()
initial = json.loads((Path(__file__).parent / "preflight.json").read_text())
start = json.loads((folder / "run-identity.json").read_text())
name = start["config"]["frame_capture"].rsplit("/", 1)[-1]
deadline = time.monotonic() + 360
attempts = 0
while True:
    attempts += 1
    ftp = device.connect(start["host"])
    try:
        ftp.cwd("ux0:/data")
        dumps = sorted(x for x in ftp.nlst() if "psp2dmp" in x)
        new_dumps = sorted(set(dumps) - set(start["coredumps_before"]))
        if new_dumps:
            (folder / "new-coredumps.json").write_text(json.dumps(new_dumps, indent=2) + "\n")
            raise RuntimeError("New coredump: preserve matching SELF and stop this run")
        try:
            content = device.read(ftp, device.DATA, name)
        except error_perm as e:
            if not str(e).startswith("550"):
                raise
            content = None
        if content is not None:
            (folder / name).write_bytes(content)
            stats = analyze(folder / name, expected=int(start["config"]["frame_capture_frames"]), require_live_play=True)
            ini = device.read(ftp, device.DATA, "strikers.ini")
            binary = device.read(ftp, device.APP, "eboot.bin")
            original = (Path(__file__).parent / "strikers.ini").read_bytes()
            launch_ini = device.ini(original, start["config"])
            marker = b"# --- STRIKERS VALIDATED DEFAULTS BEGIN ---"
            # refresh_vita_default_config adds one blank line before the
            # existing managed block on startup. Accept only that exact
            # known rewrite; every setting and custom line must still match.
            known_refresh = launch_ini.replace(marker, b"\n" + marker, 1)
            known_refresh = known_refresh.replace(b"gxm_tev_decoded_write_gate = 0\n", b"gxm_tev_decoded_write_gate = 0\ngxm_raster_uniform_reuse = 0\ngxm_native_pn_palette = 0\n", 1)
            known_refresh_2 = launch_ini.replace(marker, b"\n" + marker, 1).replace(b"gxm_raster_uniform_reuse = 0\n", b"gxm_raster_uniform_reuse = 0\ngxm_native_pn_palette = 0\n", 1)
            if ini not in (launch_ini, known_refresh, known_refresh_2):
                (folder / "strikers.ini.unexpected").write_bytes(ini)
                raise RuntimeError("INI changed beyond known startup blank line")
            if device.sha(binary) != start["eboot_sha256"]:
                raise RuntimeError("SELF changed during run")
            (folder / "strikers.ini").write_bytes(ini)
            identity = {"time_unix": time.time(), "label": start["label"],
                        "eboot_sha256": device.sha(binary), "ini_sha256": device.sha(ini),
                        "csv_sha256": device.sha(content), "csv_bytes": len(content),
                        "launch_ini_sha256": device.sha(launch_ini),
                        "ini_startup_blank_line": ini == known_refresh,
                        "new_coredumps": [], "attempts": attempts}
            io_name = start["config"]["asset_io_capture"].rsplit("/", 1)[-1]
            io_content = device.read(ftp, device.DATA, io_name)
            (folder / io_name).write_bytes(io_content)
            io_stats = analyze_io(folder / io_name,
                                  expected=int(start["config"]["asset_io_capture_frames"]),
                                  window_first=int(start["config"]["frame_capture_skip"]),
                                  window_last=int(start["config"]["frame_capture_skip"]) +
                                              int(start["config"]["frame_capture_frames"]) - 1)
            if not io_stats["observations_active"]:
                raise RuntimeError("Archive I/O observations inactive: no runtime attribution")
            identity["io_csv_sha256"] = device.sha(io_content)
            identity["io_csv_bytes"] = len(io_content)
            identity["capture_scope"] = "I/O attribution, not a quiet FPS improvement test"
            (folder / "io-analysis.json").write_text(json.dumps(io_stats, indent=2) + "\n")
            (folder / "capture-identity.json").write_text(json.dumps(identity, indent=2) + "\n")
            (folder / "analysis.json").write_text(json.dumps(stats, indent=2) + "\n")
            print(json.dumps({"identity": identity, "analysis": stats, "io_analysis": io_stats}, indent=2), flush=True)
            break
    finally:
        ftp.quit()
    if time.monotonic() >= deadline:
        (folder / "capture-timeout.json").write_text(json.dumps(
            {"time_unix": time.time(), "attempts": attempts,
             "note": "Complete finite CSV absent; no numerical FPS result."}, indent=2) + "\n")
        raise SystemExit("No complete CSV within 360 seconds")
    time.sleep(10)
