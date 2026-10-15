#!/usr/bin/env python3
"""Prepare verified, deduplicated PSARC assets for Strikers or another port.

Uses only the Python standard library. Original stored blocks avoid decompression work on Vita. Optional native
sidecars alone use zlib blocks and are read on cache admission. Game adapters validate/extract the source; packaging preserves all bytes.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path
import tempfile
import time

from asset_pipeline.adapters import prepare_gamecube
from asset_pipeline.psarc import collect_files, create_psarc, iter_entry, parse_psarc, verify_psarc


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(4 * 1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def verify_sources(archive_path: Path, roots: list[Path]) -> list[dict]:
    """Compare every archived byte against its prepared source, with bounded RAM."""
    parsed = parse_psarc(archive_path)
    sources = {item.archive_path: item for item in collect_files(roots)}
    if set(parsed.paths) != set(sources):
        raise ValueError("archive manifest differs from the prepared input tree")
    result = []
    with archive_path.open("rb", buffering=0) as handle:
        for i, name in enumerate(parsed.paths, start=1):
            h = hashlib.sha256()
            for block in iter_entry(handle, parsed, i):
                h.update(block)
            source = sources[name]
            digest = h.hexdigest()
            if parsed.entries[i].size != source.size or digest != sha256_file(source.source):
                raise ValueError(f"archive/source mismatch: {name}")
            result.append({"path": name, "bytes": source.size, "sha256": digest})
    return result


def prepare(profile: str, iso: Path | None, roots: list[Path], output: Path,
            *, replace: bool = False, deduplicate: bool = True, native_compiler: Path | None = None,
            native_only: bool = False, source_archive: Path | None = None,
            native_textures: str = 'compact', native_audio: str = 'off', audio_compiler: Path | None = None,
            native_video: bool = False, ffmpeg: str = 'ffmpeg', video_crf: int = 18,
            video_threads: int = 4, shader_cache: Path | None = None,
            native_gpu_static: bool = False, native_graphics_compiler: Path | None = None, native_media_archive: Path | None = None) -> dict:
    has_native = bool(native_compiler or native_graphics_compiler or native_media_archive or native_audio != 'off' or native_video or shader_cache)
    if native_media_archive and (native_audio!='off' or native_video or shader_cache):
        raise ValueError('select either prepared native media or media compilation')
    if native_graphics_compiler and (native_only or native_compiler or native_gpu_static):
        raise ValueError('native graphics requires a complete archive and cannot use legacy geometry sidecars')
    if native_textures not in ('compact','all') or native_audio not in ('off','bank','all'):
        raise ValueError('invalid native preparation mode')
    if native_only and not has_native:
        raise ValueError("--native-only requires at least one native asset category")
    if native_audio != 'off' and not audio_compiler:
        raise ValueError('--native-audio requires --audio-compiler')
    if native_gpu_static and not native_compiler:
        raise ValueError('--native-gxm-static requires --native-compiler')
    if has_native and profile != 'strikers':
        raise ValueError('native game adapters require the strikers profile')
    if output.is_symlink():
        raise ValueError("refusing a symlink output")
    output = output.absolute()
    if output.exists() and not replace:
        raise FileExistsError(f"{output} exists; use --replace to replace it")
    if profile == "generic":
        if iso or source_archive or not roots:
            raise ValueError("generic profile requires --root and no --iso")
    elif bool(iso) == bool(source_archive) or roots:
        raise ValueError("gamecube/strikers profile requires exactly one --iso/--source-archive and no --root")
    source_path = iso or source_archive
    if source_path and source_path.resolve() in (output.resolve(),output.with_suffix(output.suffix+'.json').resolve()):
        raise ValueError("output cannot replace the source")
    for root in roots:
        if output.resolve().is_relative_to(root.resolve()):
            raise ValueError("output must be outside the source tree")
    report_path = output.with_suffix(output.suffix + ".json")
    if report_path.is_symlink() or (report_path.exists() and not replace):
        raise FileExistsError(f"report path already exists: {report_path}")
    output.parent.mkdir(parents=True, exist_ok=True)
    started = time.perf_counter()
    # Only this invocation's private temporary workspace is removed.
    with tempfile.TemporaryDirectory(prefix="vita-assets-", dir=output.parent) as tmp:
        workspace = Path(tmp)
        source = None
        if source_archive:
            source_archive=source_archive.resolve();source_hash=sha256_file(source_archive)
            source={'path':str(source_archive),'bytes':source_archive.stat().st_size,'sha256':source_hash,'kind':'psarc'}
            parsed=parse_psarc(source_archive);extract=workspace/'input';extract.mkdir()
            with source_archive.open('rb') as handle:
                for i,name in enumerate(parsed.paths,1):
                    # parse_psarc validates relative paths. Check again at the write boundary.
                    target=extract/name
                    if not target.resolve().is_relative_to(extract.resolve()):raise ValueError('unsafe source archive path')
                    target.parent.mkdir(parents=True,exist_ok=True)
                    with target.open('wb') as out:
                        for block in iter_entry(handle,parsed,i):out.write(block)
            roots=sorted(extract.iterdir())
            if any(not p.is_dir() for p in roots) or not (extract/'files').is_dir():
                raise ValueError('expected a game PSARC with files/ and directory roots')
            if profile=='strikers':
                boot=extract/'sys/boot.bin'
                identity=boot.read_bytes()[:6] if boot.is_file() else b''
                if identity not in (b'G4QE01',b'G4QP01',b'G4QJ01') or not (extract/'files/common.ini').is_file():
                    raise ValueError('source PSARC is not a supported Strikers disc')
            if (extract/'native').exists():raise ValueError('derive new sidecars from the original game PSARC, without native/')
            if sha256_file(source_archive)!=source_hash:raise ValueError('source archive changed during extraction')
            adapter={'payload_policy':'original bytes','input':'verified PSARC','files':len(parsed.paths)}
        elif profile != "generic":
            iso = iso.resolve()
            source_hash = sha256_file(iso)
            source = {"path": str(iso), "bytes": iso.stat().st_size, "sha256": source_hash}
            roots, adapter = prepare_gamecube(iso, workspace, strikers=profile == "strikers")
            if sha256_file(iso) != source_hash:
                raise ValueError("source disc changed during extraction")
        else:
            adapter = {"payload_policy": "original bytes", "roots": [str(p.resolve()) for p in roots]}
        graphics_report = None
        if native_graphics_compiler:
            from asset_pipeline.native_graphics import prepare_graphics
            graphics_report = prepare_graphics(roots,native_graphics_compiler,workspace)
        native_report = None
        native_root=workspace/'native'
        original_roots=list(roots)
        if native_compiler:
            if profile != "strikers":
                raise ValueError("native GLT/GLG adapter requires the strikers profile")
            from asset_pipeline.native import prepare_native
            native_root, native_report = prepare_native(roots, native_compiler, workspace,
                                                        include_rgba=native_textures=='all',
                                                        gpu_static=native_gpu_static)
        media_report={}
        if native_media_archive:
            media_source=native_media_archive.resolve();before=sha256_file(media_source)
            parsed_media=parse_psarc(media_source);copied=[]
            with media_source.open('rb') as handle:
                for i,name in enumerate(parsed_media.paths,1):
                    if not (name.startswith('native/v1/audio/') or name=='native/v1/audio-index.bin' or name.startswith('native/v1/video/')):continue
                    target=workspace/name;target.parent.mkdir(parents=True,exist_ok=True)
                    with target.open('wb') as f:
                        for block in iter_entry(handle,parsed_media,i):f.write(block)
                    copied.append(name)
            if sha256_file(media_source)!=before:raise ValueError('native media source changed')
            media_report['prepared']={'source_sha256':before,'entries':len(copied),'policy':'audio and video only; no geometry, texture or shader sidecars imported'}
        if native_audio!='off':
            from asset_pipeline.media import prepare_audio
            media_report['audio']=prepare_audio(original_roots,audio_compiler,workspace,mode=native_audio)
        if native_video:
            from asset_pipeline.video import prepare_video
            media_report['video']=prepare_video(original_roots,workspace,ffmpeg=ffmpeg,crf=video_crf,threads=video_threads)
        if shader_cache:
            from asset_pipeline.media import prepare_shader_cache
            media_report['shaders']=prepare_shader_cache(shader_cache,workspace)
        if native_graphics_compiler:
            files_root=next(r for r in original_roots if r.name=='files')
            video_root=workspace/'native/v1/video';replaced_video=[]
            if video_root.exists():
                for native_movie in sorted(video_root.rglob('*.thp')):
                    relative=native_movie.relative_to(video_root);target=files_root/relative
                    if not target.is_file():raise ValueError('native movie has no original logical path')
                    os.replace(native_movie,target);replaced_video.append(relative.as_posix())
            media_report['native_movie_paths']=replaced_video
            removed=[]
            # These are GameCube executables/debug outputs, never Vita runtime data.
            for root in original_roots:
                for path in sorted(root.iterdir()):
                    if path.is_file() and ((root.name=='sys' and path.name!='boot.bin' and path.name!='strikers-native.bin') or
                            (root.name=='files' and path.suffix.lower() in ('.elf','.dol','.exe','.idb','.map'))):
                        removed.append(path.relative_to(root.parent).as_posix());path.unlink()
            graphics_report['removed_non_runtime_files']=removed
        if has_native:
            native_root.mkdir(exist_ok=True)
            roots=[native_root] if native_only else [*original_roots,native_root]
        candidate = workspace / "candidate.psarc"
        packed = create_psarc(candidate, roots, merge_duplicates=deduplicate,
                              compress_prefixes=("native/v1/textures/","native/v1/geometry/","native/v2/geometry/","native/v1/audio/",
                                                 "native/v1/shaders/","native/v1/audio-index.bin") if has_native else ())
        checked = verify_psarc(candidate)
        files = verify_sources(candidate, roots)
        archive_hash = sha256_file(candidate)
        packed["archive"] = checked["archive"] = str(output)
        report = {
            "format_version": 1, "profile": profile, "source_disc": source,
            "adapter": adapter, "native_assets": native_report, "native_only": native_only,
            "native_media":media_report, "native_graphics":graphics_report,
            "create": packed, "verify": checked,
            "archive_sha256": archive_hash, "verified_files": files,
            "elapsed_seconds": time.perf_counter() - started,
            "container": {"format": "PSARC 1.4", "license": "CC0-1.0"},
        }
        report_tmp = workspace / "report.json"
        report_tmp.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        # Publish only after container validation and full source round-trip.
        os.replace(candidate, output)
        os.replace(report_tmp, report_path)
    return report


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--profile", choices=("strikers", "gamecube", "generic"), default="strikers")
    p.add_argument("--iso", type=Path)
    p.add_argument('--source-archive',type=Path,help='derive a new pack from an existing verified game PSARC')
    p.add_argument("--root", type=Path, action="append", default=[])
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--replace", action="store_true")
    p.add_argument("--no-dedup", action="store_true")
    p.add_argument("--native-media-archive",type=Path,help="reuse validated native audio/video records; graphics are regenerated from the original source")
    p.add_argument("--native-graphics-compiler",type=Path,help="Strikers compiler: replace GLG/GLT and embedded resource textures with native graphics; no original payload fallback")
    p.add_argument("--native-compiler", type=Path,
                   help="legacy graphics sidecars for older runtimes; current Vita requires --native-graphics-compiler")
    p.add_argument("--native-only", action="store_true",
                   help="legacy sidecar-only pack for older runtimes; unsupported by the current Vita loaders")
    p.add_argument('--native-textures',choices=('compact','all'),default='compact',help='all includes exact RGBA/TPL texture outputs')
    p.add_argument('--native-gxm-static',action='store_true',
                   help='include AVNR v2 source-verified static geometry packed for common exact GXM layouts')
    p.add_argument('--native-audio',choices=('off','bank','all'),default='off',help='bank covers SFX; all also prepares DSP/IDSP music and streams')
    p.add_argument('--audio-compiler',type=Path,help='host strikers_compile_native_audio executable')
    p.add_argument('--native-video',action='store_true',help='prepare independent Baseline H.264 movies; native graphics builds replace original movie payloads')
    p.add_argument('--ffmpeg',default='ffmpeg')
    p.add_argument('--video-crf',type=int,default=18)
    p.add_argument('--video-threads',type=int,default=4)
    p.add_argument('--shader-cache',type=Path,help='directory of programs compiled on Vita; checked AVGX records')
    a = p.parse_args()
    try:
        report = prepare(a.profile, a.iso, a.root, a.output,
                         replace=a.replace, deduplicate=not a.no_dedup,
                         native_compiler=a.native_compiler, native_only=a.native_only,source_archive=a.source_archive,
                         native_textures=a.native_textures,native_audio=a.native_audio,audio_compiler=a.audio_compiler,
                         native_video=a.native_video,ffmpeg=a.ffmpeg,video_crf=a.video_crf,
                         video_threads=a.video_threads,shader_cache=a.shader_cache,
                         native_gpu_static=a.native_gxm_static,native_graphics_compiler=a.native_graphics_compiler,native_media_archive=a.native_media_archive)
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
        p.exit(1, f"Asset preparation failed: {exc}\n")
    print(json.dumps({"archive": str(a.output), "sha256": report["archive_sha256"],
                      "create": report["create"], "seconds": report["elapsed_seconds"]}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
