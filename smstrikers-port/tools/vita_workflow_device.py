#!/usr/bin/env python3
"""Bounded Strikers-only delivery/capture, with readback and PC-only backups.

Does not touch saves, disc data, plugins or other applications. Logs installed
content hashes; hardware measurements come only from downloaded runtime CSV.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import socket
import time
from ftplib import FTP, error_perm

APP = "ux0:/app/SMSVITA01"
DATA = "ux0:/data/strikersVita"

def sha(data):
    return hashlib.sha256(data).hexdigest()

def command(host, value):
    with socket.create_connection((host, 1338), timeout=10) as sock:
        sock.settimeout(3)
        sock.sendall((value + "\n").encode())
        result = bytearray()
        try:
            while len(result) < 4096:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                result.extend(chunk)
        except TimeoutError:
            pass
    return result.decode(errors="replace").strip()

def connect(host):
    ftp = FTP()
    ftp.connect(host, 1337, timeout=15)
    ftp.login()
    return ftp

def read(ftp, directory, name):
    ftp.cwd(directory)
    out = io.BytesIO()
    ftp.retrbinary("RETR " + name, out.write)
    return out.getvalue()

def promote(ftp, directory, name, content, out):
    # Retain rollback history on the PC. Previous remote backup/stage files
    # accumulated in the Vita app/data directories on every test run.
    old = read(ftp, directory, name)
    backup = out / (name + ".before")
    if backup.exists() and backup.read_bytes() != old:
        raise RuntimeError("Refusing to overwrite a different local backup: " + str(backup))
    backup.write_bytes(old)
    if read(ftp, directory, name) != old:
        raise RuntimeError("Remote source changed: " + name)
    try:
        if content != old:
            ftp.cwd(directory)
            ftp.storbinary("STOR " + name, io.BytesIO(content))
        actual = read(ftp, directory, name)
        if actual != content:
            raise RuntimeError("Updated readback differs: " + name)
    except Exception as update_error:
        try:
            if read(ftp, directory, name) != old:
                ftp.cwd(directory)
                ftp.storbinary("STOR " + name, io.BytesIO(old))
                if read(ftp, directory, name) != old:
                    raise RuntimeError("Original readback differs")
        except Exception as rollback_error:
            raise RuntimeError("Update failed and remote rollback could not be verified; "
                               "original is saved at " + str(backup)) from rollback_error
        raise
    return {"path": directory + "/" + name, "sha256": sha(actual),
            "previous_sha256": sha(old), "remote_backup": None,
            "local_backup": str(backup.resolve())}

def ini(original, overrides):
    # The runtime uses first-value-wins and preserves custom lines before the
    # managed block. Every test starts from the byte-verified original INI.
    prefix = "# temporary native-workflow hardware test\n"
    prefix += "".join(f"{key}={value}\n" for key, value in overrides.items())
    return prefix.encode() + original

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("action", choices=["backup", "install", "run", "fetch", "restore", "restore-config"])
    p.add_argument("--host", required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--baseline", type=Path)
    p.add_argument("--self", type=Path)
    p.add_argument("--label", default="candidate")
    p.add_argument("--mask", default="0x8")
    p.add_argument("--asset-archive", help="optional ux0:data/strikersVita/*.psarc for this run")
    p.add_argument("--streamed-vertex-gpu", choices=["0", "1"],
                   help="override only dynamic fixed-vertex GPU preparation for this run")
    p.add_argument("--frames", type=int, default=1200)
    p.add_argument("--skip", type=int, default=600)
    p.add_argument("--visual", action="store_true")
    p.add_argument("--snapshot-frame", type=int, default=120,
                   help="match frame for a separate visual run")
    p.add_argument("--play-snapshot", action="store_true",
                   help="snapshot at live-play frame, excluding the introduction")
    p.add_argument("--diagnostics", action="store_true")
    p.add_argument("--launch-confirm", choices=["twice", "once", "none", "startup-log"],
                   help="defaults to startup-log with diagnostics, otherwise none; once/twice are explicit LiveArea confirmation")
    p.add_argument("--set", dest="ini_settings", action="append", default=[], metavar="KEY=VALUE",
                   help="temporary INI override for one isolated performance experiment")
    args = p.parse_args()
    if args.launch_confirm is None:
        args.launch_confirm = "startup-log" if args.diagnostics else "none"
    if args.action == "run" and args.launch_confirm == "startup-log" and not args.diagnostics:
        p.error("startup-log requires --diagnostics")
    if not 1 <= args.snapshot_frame <= 100000:
        p.error("snapshot-frame must be in [1,100000]")
    if not args.label.replace("-", "").replace("_", "").isalnum():
        p.error("label must contain only letters, digits, - and _")
    args.out.mkdir(parents=True, exist_ok=True)
    metadata = {"host": args.host, "action": args.action, "time_unix": time.time(), "label": args.label}
    snapshot_name=f"debug_frame_{'play' if args.play_snapshot else '3d'}_{args.snapshot_frame}.ppm"
    ftp = connect(args.host)
    if args.action == "backup":
        for directory, name in [(APP, "eboot.bin"), (DATA, "strikers.ini")]:
            content = read(ftp, directory, name)
            target = args.out / name
            if target.exists() and target.read_bytes() != content:
                raise RuntimeError("Refusing to overwrite a different original backup")
            target.write_bytes(content)
            metadata[name] = sha(content)
    elif args.action in ("install", "restore", "restore-config"):
        if not args.baseline:
            p.error("--baseline is required")
        metadata["kill"] = command(args.host, "kill SMSVITA01")
        if args.action == "install":
            if not args.self:
                p.error("--self is required")
            metadata["installed"] = promote(ftp, APP, "eboot.bin", args.self.read_bytes(), args.out)
        else:
            if args.action == "restore":
                metadata["binary"] = promote(ftp, APP, "eboot.bin", (args.baseline / "eboot.bin").read_bytes(), args.out)
            metadata["ini"] = promote(ftp, DATA, "strikers.ini", (args.baseline / "strikers.ini").read_bytes(), args.out)
    elif args.action == "run":
        if not args.baseline or not 1 <= args.frames <= 8192 or not 0 <= args.skip <= 100000:
            p.error("--baseline, frames in [1,8192] and skip in [0,100000] required")
        metadata["kill"] = command(args.host, "kill SMSVITA01")
        overrides = {"gxm_disable": args.mask, "gxm_shader_profile": "WARM",
                     "diagnostics": "1" if args.diagnostics else "0", "fps_overlay": "0",
                     "vita_test_match": "1", "vita_frameskip": "0",
                     "seed": "0x53545249", "fixed_dt": "16.666666667",
                     "frame_capture": DATA.replace(":/", ":") + "/" + args.label + ".csv",
                     "frame_capture_frames": str(args.frames), "frame_capture_match_only": "1",
                     "frame_capture_play_only": "1", "frame_capture_skip": str(args.skip),
                     "vita_snapshot_match_frame": str(args.snapshot_frame) if args.visual and not args.play_snapshot else "0",
                     "vita_snapshot_play_frame": str(args.snapshot_frame) if args.visual and args.play_snapshot else "0"}
        if args.diagnostics:
            overrides["log"] = DATA.replace(":/", ":") + "/" + args.label + ".log"
        if args.asset_archive is not None:
            overrides["asset_archive"] = args.asset_archive
        if args.streamed_vertex_gpu is not None:
            overrides["gxm_streamed_vertex_gpu"] = args.streamed_vertex_gpu
        original = (args.baseline / "strikers.ini").read_bytes()
        for setting in args.ini_settings:
            key, separator, value = setting.partition("=")
            if not separator or not key.replace("_", "").isalnum() or any(c in value for c in "\r\n\x00"):
                p.error("--set requires one valid INI KEY=VALUE")
            overrides[key] = value
        if args.launch_confirm == "startup-log" and (overrides["diagnostics"] != "1"
                or overrides.get("log") != DATA.replace(":/", ":") + "/" + args.label + ".log"):
            p.error("startup-log requires diagnostics=1 and the run's unique log path")
        metadata["config"] = overrides
        metadata["ini"] = promote(ftp, DATA, "strikers.ini", ini(original, overrides), args.out)
        metadata["eboot_sha256"] = sha(read(ftp, APP, "eboot.bin"))
        metadata["launch_confirm"] = args.launch_confirm
        if args.launch_confirm == "startup-log":
            ftp.cwd(DATA)
            try:
                ftp.delete(args.label + ".log")
            except error_perm as e:
                if not str(e).startswith("550"):
                    raise
        if args.visual:
            ftp.cwd(DATA)
            try:
                ftp.delete(snapshot_name)
            except error_perm as e:
                if not str(e).startswith("550"):
                    raise
        metadata["launch"] = command(args.host, "launch SMSVITA01")
        if args.launch_confirm == "startup-log":
            def started():
                ftp.cwd(DATA)
                ftp.voidcmd("TYPE I")
                try:
                    return ftp.size(args.label + ".log") is not None
                except error_perm as e:
                    if not str(e).startswith("550"):
                        raise
                    return False
            time.sleep(3)
            metadata["started_before_input"] = started()
            if not metadata["started_before_input"]:
                metadata["press"] = command(args.host, "press cross")
                time.sleep(.2)
                metadata["release"] = command(args.host, "release cross")
            deadline = time.monotonic() + 30
            while not started():
                if time.monotonic() >= deadline:
                    (args.out / "run-identity.json").write_text(json.dumps(metadata, indent=2) + "\n")
                    raise RuntimeError("No startup log after one confirmation; refusing further game input")
                time.sleep(2)
            metadata["startup_log_confirmed"] = True
        # Launch may first show the app's own LiveArea start button.
        elif args.launch_confirm in ("twice", "once"):
            time.sleep(2)
            metadata["press"] = command(args.host, "press cross")
            time.sleep(.2)
            metadata["release"] = command(args.host, "release cross")
        # Relaunching after kill can leave the LiveArea start animation active
        # at the first input. A second bounded confirmation avoids idle tests.
        if args.launch_confirm == "twice":
            time.sleep(3)
            metadata["press_retry"] = command(args.host, "press cross")
            time.sleep(.2)
            metadata["release_retry"] = command(args.host, "release cross")
    else:
        for name in [args.label + ".csv", args.label + ".log", "strikers.ini"] + ([snapshot_name] if args.visual else []):
            try:
                data = read(ftp, DATA, name)
            except error_perm as e:
                metadata[name] = {"missing": str(e)}
                continue
            (args.out / name).write_bytes(data)
            metadata[name] = {"sha256": sha(data), "bytes": len(data)}
        metadata["eboot_sha256"] = sha(read(ftp, APP, "eboot.bin"))
    ftp.quit()
    (args.out / (args.action + "-identity.json")).write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps(metadata, indent=2))

if __name__ == "__main__":
    main()
