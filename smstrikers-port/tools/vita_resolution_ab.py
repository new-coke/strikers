#!/usr/bin/env python3
"""Prepare/capture/analyze isolated resolution or geometry A/B. No device writes/builds."""
import argparse
import hashlib
import json
import shlex
from pathlib import Path

from analyze_vita_frames import analyze
import vita_workflow_device as device

COMMON = {
    "gxm_disable": "0x8", "gxm_shader_profile": "WARM",
    "vita_test_match": "1", "vita_frameskip": "0", "seed": "0x53545249",
    "gxm_native_model_draw": "0", "gxm_native_model_cache": "0",
    "gxm_native_model_uniform_build": "0", "gxm_native_model_prepared": "0",
    "gxm_native_model_early": "0", "gxm_native_model_census": "0",
    "gxm_native_model_metrics": "0", "gxm_native_draw_replay": "0",
    "gxm_streamed_vertex_gpu": "0", "gxm_local_draw_batching": "0",
    "vita_view_draw_capture": "0", "vita_view_draw_payloads": "0",
    "vita_snapshot_3d_frame": "0", "task_profile": "0",
    "vita_packet_profile_period": "0", "performance_capture": "0",
    "performance_capture_frames": "0", "fps_overlay": "0",
    "static_geometry_mb": "8", "gfx_resource_cached": "1", "gfx_resource_mb": "48",
    "aurora_distinct_cpu_cores": "0", "gxm_immediate_draw_view": "0",
    "gxm_vertex_reuse_probe": "0", "gxm_dl_shadow": "0", "gxm_prepared_dl": "0",
    "cpu_mhz": "500", "gpu_mhz": "222",
    "gxm_fixed_min": "16", "vita_draw_limit": "0",
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def effective_ini(path):
    values = {}
    for line in Path(path).read_text().splitlines():
        line = line.strip()
        if line and not line.startswith(("#", ";", "[")) and "=" in line:
            key, value = line.split("=", 1)
            values.setdefault(key.strip().lower(), value.strip())
    return values


def plan(out, host, self_path=None, experiment="resolution"):
    out = out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    helper = Path(__file__).resolve().with_name("vita_workflow_device.py")
    # Isolate pixel cost from resident geometry / fixed-vertex GPU admission.
    variants = [("960x544", "960x544", "8"), ("640x480", "640x480", "8")] if experiment == "resolution" else [
        ("cache-8", "960x544", "8"), ("cache-0", "960x544", "0")]
    prefix = "res" if experiment == "resolution" else "geo"
    runs = []
    # Fixed simulation steps for separate visual inspection; real time for FPS.
    cases = [(f"{prefix}-v-{variant[0]}-{frame}", variant, True, frame)
             for frame in (120, 360) for variant in variants]
    cases += [(f"{prefix}-v-{variants[0][0]}-120-repeat", variants[0], True, 120)]
    cases += [(f"{prefix}-q-{i+1}-{variants[index][0]}", variants[index], False, 0)
              for i, index in enumerate((0, 1, 1, 0, 0, 1))]
    for label, (variant, size, geometry), visual, frame in cases:
        config = dict(COMMON, gxm_internal_resolution=size, fixed_dt="50" if visual else "0",
                      static_geometry_mb=geometry,
                      diagnostics="1" if visual else "0", frame_capture_frames="120" if visual else "300",
                      frame_capture_skip="60" if visual else "600",
                      frame_capture_match_only="1", frame_capture_play_only="1",
                      frame_capture=f"ux0:data/strikersVita/{label}.csv",
                      vita_snapshot_match_frame="0", vita_snapshot_play_frame=str(frame))
        cmd = ["python3", str(helper), "run", "--host", host, "--out", str(out / label),
               "--baseline", str(out / "baseline"), "--label", label,
               "--frames", config["frame_capture_frames"], "--skip", config["frame_capture_skip"]]
        if visual:
            config["log"] = f"ux0:data/strikersVita/{label}.log"
            cmd += ["--diagnostics", "--visual", "--play-snapshot", "--snapshot-frame", str(frame),
                    "--launch-confirm", "startup-log"]
        else:
            cmd += ["--launch-confirm", "none"]
        for key, value in config.items():
            cmd += ["--set", f"{key}={value}"]
        runs.append({"label": label, "variant": variant, "resolution": size, "visual": visual,
                     "config": config, "command": cmd})
    if any((out / run["label"] / "run-identity.json").exists() for run in runs):
        raise ValueError("Existing run evidence: prepare a new output directory")
    manifest = {"schema": "strikers-resolution-ab-v1", "host": host,
                "experiment": experiment, "reference": variants[0][0], "candidate": variants[1][0],
                "self_sha256": sha(self_path) if self_path else None,
                "self_path": str(Path(self_path).resolve()) if self_path else None,
                "status": "requires manual Vita build, sealed SELF and hardware visual review",
                "runs": runs,
                "capture_requirements": "Run each command separately. Fetch complete CSV, final INI, visual PPM/log, "
                    "and capture-identity.json with eboot_sha256 read after that run. Check for new coredumps. "
                    "Verify the startup render config has the requested internal size before FPS runs. "
                    "Restore original SELF/INI and verify their bytes after the test."}
    (out / "plan.json").write_text(json.dumps(manifest, indent=2) + "\n")
    for variant, size, geometry in variants:
        config = dict(COMMON, gxm_internal_resolution=size, static_geometry_mb=geometry,
                      fixed_dt="0", diagnostics="0")
        (out / f"{variant}.overrides.ini").write_text(
            "# Temporary overrides: prepend to a preserved original INI; restart the game.\n" +
            "".join(f"{key}={value}\n" for key, value in config.items()))
    (out / "commands.txt").write_text(
        "Commands are for separate runs after backup/install and visual review; this file does not execute them.\n"
        "Record device-state before installing/testing. Fetch each capture only when complete.\n\n" +
        shlex.join(["python3", str(helper), "backup", "--host", host, "--out", str(out / "baseline")]) + "\n\n" +
        shlex.join(["python3", str(Path(__file__).resolve()), "device-state", str(out / "plan.json")]) + "\n\n" +
        (shlex.join(["python3", str(helper), "install", "--host", host, "--out", str(out / "install"),
                     "--baseline", str(out / "baseline"), "--self", manifest["self_path"]]) + "\n\n"
         if self_path else "Regenerate this plan with --self after the manual build, before installation.\n\n") +
        "\n\n".join(shlex.join(run["command"]) + "\n" + shlex.join(
            ["python3", str(Path(__file__).resolve()), "capture", str(out / "plan.json"), run["label"]])
            for run in runs) + "\n\n" + shlex.join(["python3", str(helper), "restore", "--host", host,
                "--out", str(out / "restore"), "--baseline", str(out / "baseline")]) + "\n")
    return manifest


def device_state(manifest_path):
    manifest = json.loads(Path(manifest_path).read_text())
    ftp = device.connect(manifest["host"])
    try:
        ftp.cwd("ux0:/data")
        dumps = sorted(name for name in ftp.nlst() if "psp2dmp" in name)
        return {"coredumps": dumps, "eboot_sha256": device.sha(device.read(ftp, device.APP, "eboot.bin"))}
    finally:
        ftp.quit()


def required_files(run):
    names = ["strikers.ini", f"{run['label']}.csv"]
    if run["visual"]:
        names += [f"debug_frame_play_{run['config']['vita_snapshot_play_frame']}.ppm", f"{run['label']}.log"]
    return names


def capture(manifest_path, label):
    path = Path(manifest_path)
    manifest = json.loads(path.read_text())
    run = next(run for run in manifest["runs"] if run["label"] == label)
    initial = json.loads((path.parent / "device-initial.json").read_text())
    folder = path.parent / label
    start = json.loads((folder / "run-identity.json").read_text())
    expected = manifest.get("self_sha256")
    if not expected or start.get("eboot_sha256") != expected:
        raise ValueError("Launch SELF differs from sealed plan")
    identity = device_state(path)
    identity["new_coredumps"] = sorted(set(identity["coredumps"]) - set(initial["coredumps"]))
    if identity["new_coredumps"]:
        (folder / "new-coredumps.json").write_text(json.dumps(identity, indent=2) + "\n")
        raise ValueError("New coredump; retain matching binary and stop the comparison")
    if identity["eboot_sha256"] != expected:
        raise ValueError("Installed SELF differs from sealed plan")
    ftp = device.connect(manifest["host"])
    try:
        identity["files"] = {}
        for name in required_files(run):
            data = device.read(ftp, device.DATA, name)
            (folder / name).write_bytes(data)
            identity["files"][name] = {"bytes": len(data), "sha256": device.sha(data)}
        identity["eboot_sha256"] = device.sha(device.read(ftp, device.APP, "eboot.bin"))
    finally:
        ftp.quit()
    analyze(folder / f"{label}.csv", expected=int(run["config"]["frame_capture_frames"]), require_live_play=True)
    if identity["eboot_sha256"] != expected:
        raise ValueError("Installed SELF changed during capture")
    actual = effective_ini(folder / "strikers.ini")
    if any(actual.get(k) != v for k, v in run["config"].items()):
        raise ValueError("Captured INI differs from launch plan")
    final_state = device_state(path)
    if final_state["eboot_sha256"] != expected or set(final_state["coredumps"]) - set(initial["coredumps"]):
        raise ValueError("Device identity or coredumps changed during capture")
    (folder / "capture-identity.json").write_text(json.dumps(identity, indent=2) + "\n")
    return identity


def analyze_plan(manifest_path, visual_reviewed=False):
    path = Path(manifest_path)
    manifest = json.loads(path.read_text())
    expected = manifest.get("self_sha256")
    if not expected:
        raise ValueError("Plan has no sealed SELF; regenerate with --self after the manual build")
    groups = {manifest["reference"]: [], manifest["candidate"]: []}
    for run in manifest["runs"]:
        folder = path.parent / run["label"]
        start = json.loads((folder / "run-identity.json").read_text())
        end = json.loads((folder / "capture-identity.json").read_text())
        if start.get("eboot_sha256") != expected or end.get("eboot_sha256") != expected:
            raise ValueError(f"{run['label']}: SELF changed or unverified")
        if end.get("new_coredumps") != []:
            raise ValueError(f"{run['label']}: coredump audit missing or failed")
        for name in required_files(run):
            record = end.get("files", {}).get(name, {})
            file = folder / name
            if not file.is_file() or record.get("sha256") != sha(file) or record.get("bytes") != file.stat().st_size:
                raise ValueError(f"{run['label']}: capture file missing or hash mismatch: {name}")
        if any(start.get("config", {}).get(k) != v for k, v in run["config"].items()):
            raise ValueError(f"{run['label']}: launch config differs from plan")
        actual = effective_ini(folder / "strikers.ini")
        if any(actual.get(k) != v for k, v in run["config"].items()):
            raise ValueError(f"{run['label']}: effective INI differs from plan")
        if run["visual"]:
            frame = run["config"]["vita_snapshot_play_frame"]
            if not (folder / f"debug_frame_play_{frame}.ppm").is_file():
                raise ValueError(f"{run['label']}: visual capture missing")
            log = (folder / f"{run['label']}.log").read_text(errors="replace")
            if f"render config display=960x544 internal={run['resolution']} " not in log:
                raise ValueError(f"{run['label']}: actual raster extent was not confirmed")
            if manifest["experiment"] == "geometry":
                budget = int(run["config"]["static_geometry_mb"]) * 1024
                if f"static geometry budget={budget} KB gpu_fixed_vertex={int(budget != 0)} " not in log:
                    raise ValueError(f"{run['label']}: actual geometry budget was not confirmed")
            continue
        result = analyze(folder / f"{run['label']}.csv", expected=300, require_live_play=True)
        if "# strikers-frame-capture-v1 diagnostics=0 fps_overlay=0" not in result["metadata"]:
            raise ValueError(f"{run['label']}: quiet capture metadata missing")
        if not result["all_gameplay"] or (result["match_frame_first"], result["match_frame_last"]) != (600, 899):
            raise ValueError(f"{run['label']}: wrong live-play window")
        groups[run["variant"]].append(result)
    aggregate = {}
    for size, runs in groups.items():
        samples = sum(run["samples"] for run in runs)
        duration_ms = sum(run["samples"] * run["mean_ms"] for run in runs)
        aggregate[size] = {"samples": samples, "mean_ms": duration_ms / samples,
                           "fps": samples * 1000 / duration_ms, "runs": runs}
    return {"self_sha256": expected, "groups": aggregate,
            "experiment": manifest["experiment"],
            "time_change_pct": 100 * (aggregate[manifest["candidate"]]["mean_ms"] /
                                      aggregate[manifest["reference"]]["mean_ms"] - 1),
            "visual_reviewed": visual_reviewed,
            "limits": "Real dt does not fix poses or draw count. FPS covers these sampled windows only. "
                      "A lower pixel count does not establish GPU execution time or a 60 FPS guarantee. "
                      "Disabling resident geometry also changes vertex transformation from GPU to CPU.",
            "eligible_for_performance_decision": visual_reviewed}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    prepare = sub.add_parser("plan")
    prepare.add_argument("--out", type=Path, required=True)
    prepare.add_argument("--host", default="192.168.1.79")
    prepare.add_argument("--self", type=Path)
    prepare.add_argument("--experiment", choices=("resolution", "geometry"), default="resolution")
    check = sub.add_parser("analyze")
    check.add_argument("manifest", type=Path)
    check.add_argument("--visual-reviewed", action="store_true")
    state = sub.add_parser("device-state", help="read-only snapshot before installing/testing")
    state.add_argument("manifest", type=Path)
    fetch = sub.add_parser("capture", help="read-only fetch after a run has completed")
    fetch.add_argument("manifest", type=Path)
    fetch.add_argument("label")
    args = parser.parse_args()
    if args.action == "plan":
        result = plan(args.out, args.host, args.self, args.experiment)
        print(json.dumps({"plan": str(args.out / "plan.json"), "self_sha256": result["self_sha256"],
                          "visual_runs": 5, "quiet_runs": 6}, indent=2))
    elif args.action == "device-state":
        result = device_state(args.manifest)
        target = args.manifest.parent / "device-initial.json"
        if target.exists():
            raise ValueError("Device baseline already exists; retain the original coredump snapshot")
        target.write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result, indent=2))
    elif args.action == "capture":
        print(json.dumps(capture(args.manifest, args.label), indent=2))
    else:
        result = analyze_plan(args.manifest, args.visual_reviewed)
        (args.manifest.parent / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps({k: v for k, v in result.items() if k != "groups"}, indent=2))


if __name__ == "__main__":
    main()
