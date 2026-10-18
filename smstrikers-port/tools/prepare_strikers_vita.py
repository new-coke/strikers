#!/usr/bin/env python3
"""Build the host converters and prepare the complete archive for the Vita runtime."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from vita_prepare_assets import prepare, sha256_file
from asset_pipeline.psarc import extract_entry, parse_psarc, verify_psarc

ROOT = Path(__file__).resolve().parents[1]


def executable(name: str) -> str:
    path = shutil.which(name)
    if not path:
        raise ValueError(f"Missing executable: {name}. See ASSET_SETUP.md for host dependencies.")
    return path


def build_tools(build_dir: Path, jobs: int) -> tuple[Path, Path]:
    cmake = executable("cmake")
    subprocess.run([cmake, "-S", str(ROOT / "tools/asset_pipeline"), "-B", str(build_dir),
                    "-DCMAKE_BUILD_TYPE=Release"], check=True)
    subprocess.run([cmake, "--build", str(build_dir), "--config", "Release", "--parallel", str(jobs)], check=True)
    # Stable install location also handles multi-configuration generators.
    prefix = build_dir / "host"
    subprocess.run([cmake, "--install", str(build_dir), "--config", "Release",
                    "--prefix", str(prefix)], check=True)
    return prefix / "bin/strikers_compile_native_graphics", prefix / "bin/strikers_compile_native_audio"


def verify(path: Path) -> dict:
    parsed = parse_psarc(path)
    if "sys/strikers-native.bin" not in parsed.paths:
        raise ValueError("Archive lacks the native graphics marker; a plain/legacy PSARC cannot boot this Vita build")
    # The production reader requires the complete, versioned marker, not its name alone.
    marker = extract_entry(path, "sys/strikers-native.bin")
    from asset_pipeline.native_graphics import PACK_MARKER
    if marker != PACK_MARKER:
        raise ValueError("Archive has an unsupported native graphics marker")
    report_path = path.with_suffix(path.suffix + ".json")
    if not report_path.is_file():
        raise ValueError(f"Missing preparation report: {report_path}; cannot compare archive identity")
    report = json.loads(report_path.read_text())
    digest = sha256_file(path)
    if digest != report["archive_sha256"]:
        raise ValueError("Archive SHA-256 differs from its preparation report")
    checked = verify_psarc(path)
    return {"path": str(path.resolve()), "bytes": path.stat().st_size,
            "sha256": digest, "native_graphics": True, "verification": checked}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--iso", type=Path, help="Your plain Strikers GameCube ISO/GCM")
    source.add_argument("--verify", type=Path, help="Verify an existing sms.psarc against its adjacent .json report")
    parser.add_argument("--output", type=Path, help="Destination sms.psarc (required with --iso)")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build-asset-tools")
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--jobs", type=int, default=min(4, os.cpu_count() or 1))
    parser.add_argument("--video-crf", type=int, default=18)
    parser.add_argument("--replace", action="store_true", help="Replace an existing output archive and report")
    args = parser.parse_args(argv)
    try:
        if args.verify:
            if args.output or args.replace:
                parser.error("--output/--replace apply only to --iso")
            print(json.dumps(verify(args.verify), indent=2))
            return 0
        if not args.output:
            parser.error("--output is required with --iso")
        if not 1 <= args.jobs <= 64 or not 1 <= args.video_crf <= 51:
            parser.error("--jobs must be 1..64 and --video-crf 1..51 (Baseline H.264)")
        if sys.platform == "win32":
            raise ValueError("Use WSL2 for the host converters; native Windows/MSVC is not supported by this wrapper")
        if not args.iso.is_file():
            raise ValueError(f"Disc image not found: {args.iso}")
        report_path = args.output.with_suffix(args.output.suffix + ".json")
        if args.output.is_symlink() or report_path.is_symlink():
            raise ValueError("Refusing a symlink output archive/report")
        if args.iso.resolve() in (args.output.resolve(), report_path.resolve()):
            raise ValueError("Output cannot replace the source disc")
        if not args.replace and (args.output.exists() or args.output.with_suffix(args.output.suffix + ".json").exists()):
            raise ValueError("Output archive/report already exists; select a new path or pass --replace")
        # Validate the disc before building converters or doing a long extraction.
        import struct
        with args.iso.open("rb") as handle:
            header = handle.read(0x440)
        if (len(header) != 0x440 or header[:6] not in (b"G4QE01", b"G4QP01", b"G4QJ01")
                or struct.unpack_from(">I", header, 0x1c)[0] != 0xc2339f3d):
            raise ValueError("Expected a plain Strikers ISO/GCM: G4QE01, G4QP01 or G4QJ01; compressed images must be converted first")
        ffmpeg = executable(args.ffmpeg)
        encoders = subprocess.run([ffmpeg, "-hide_banner", "-encoders"], check=True,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True).stdout
        if not any(len(line.split()) >= 2 and line.split()[1] == "libx264" for line in encoders.splitlines()):
            raise ValueError("FFmpeg must include the libx264 encoder")
        graphics, audio = build_tools(args.build_dir.resolve(), args.jobs)
        print("Preparing native models/textures, exact PCM and H.264 movies; verifying archive bytes.", flush=True)
        report = prepare("strikers", args.iso.resolve(), [], args.output.absolute(),
                         replace=args.replace, native_graphics_compiler=graphics,
                         native_audio="all", audio_compiler=audio, native_video=True,
                         ffmpeg=ffmpeg, video_crf=args.video_crf, video_threads=args.jobs)
        print(json.dumps({"archive": str(args.output.resolve()), "sha256": report["archive_sha256"],
                          "copy_to": "ux0:data/strikersVita/sms.psarc",
                          "ini": "asset_archive = ux0:data/strikersVita/sms.psarc"}, indent=2))
        return 0
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as exc:
        print(f"Asset preparation failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
