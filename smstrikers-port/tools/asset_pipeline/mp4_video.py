"""Sequential H.264/AAC MP4 movies for the Vita SceAvPlayer consumer.

The small versioned descriptor is independent of FFmpeg's container layout.
Movie names remain relative to files/; the game still requests its THP name.
"""
from __future__ import annotations

from fractions import Fraction
import json
import mmap
from pathlib import Path
import struct
import subprocess
import wave

from .video import sha, thp_info, thp_frames

DESCRIPTOR = struct.Struct("<8s6I")
MAGIC = b"STMP4001"


def decode_thp_audio(record: bytes) -> tuple[bytes, int]:
    """Match the port's bounded 64-bit DSP accumulator, before AAC encoding."""
    if len(record) < 80:
        raise ValueError("truncated THP audio header")
    offset, samples = struct.unpack_from(">II", record)
    channel_bytes = ((samples + 13) // 14) * 8
    if not samples or samples > 32768 or 80 + channel_bytes > len(record):
        raise ValueError("invalid THP audio sample extent")
    if offset and (offset < channel_bytes or 80 + offset + channel_bytes > len(record)):
        raise ValueError("invalid THP stereo extent")
    channels = []
    for channel in range(2 if offset else 1):
        coef = struct.unpack_from(">16h", record, 8 + channel * 32)
        y1, y2 = struct.unpack_from(">hh", record, 72 + channel * 4)
        raw = record[80 + channel * offset:80 + channel * offset + channel_bytes]
        values = []
        for at in range(0, channel_bytes, 8):
            header = raw[at]
            c1, c2 = coef[((header >> 4) & 7) * 2:((header >> 4) & 7) * 2 + 2]
            scale = 1 << (header & 15)
            for packed in raw[at + 1:at + 8]:
                for nibble in (packed >> 4, packed & 15):
                    if len(values) == samples:
                        break
                    nibble = nibble - 16 if nibble >= 8 else nibble
                    value = (nibble * scale * 2048 + 1024 + y1 * c1 + y2 * c2) >> 11
                    value = max(-32768, min(32767, value))
                    values.append(value)
                    y2, y1 = y1, value
        channels.append(values)
    if not offset:
        channels *= 2
    values = [value for pair in zip(*channels) for value in pair]
    return struct.pack("<" + "h" * len(values), *values), samples


def write_thp_audio(data, info, output: Path) -> dict:
    audio_at = struct.unpack_from(">I", data, 32)[0] + 20
    audio_at += 12 if struct.unpack_from(">I", data, 4)[0] == 0x11000 else 8
    channels, rate, declared = struct.unpack_from(">III", data, audio_at)
    if channels not in (1, 2) or not 8000 <= rate <= 48000:
        raise ValueError("unsupported THP audio metadata")
    if audio_at + 16 <= info["first"] and struct.unpack_from(">I", data, 4)[0] == 0x11000:
        if struct.unpack_from(">I", data, audio_at + 12)[0] != 1:
            raise ValueError("multiple THP audio tracks are not supported")
    import hashlib
    digest = hashlib.sha256()
    samples = 0
    with wave.open(str(output), "wb") as target:
        target.setparams((2, 2, rate, 0, "NONE", "not compressed"))
        for _, _, parts in thp_frames(data, info):
            at, length = parts[1]
            pcm, count = decode_thp_audio(data[at:at + length])
            target.writeframesraw(pcm)
            digest.update(pcm)
            samples += count
    if samples != declared:
        raise ValueError("THP audio sample count differs from its header")
    return {"source_audio_samples": samples, "source_audio_rate": rate,
            "source_pcm_sha256": digest.hexdigest()}


def validate_probe(probe: dict, info: dict, rate: Fraction) -> None:
    streams = probe["streams"]
    video = [s for s in streams if s["codec_type"] == "video"]
    audio = [s for s in streams if s["codec_type"] == "audio"]
    if len(video) != 1 or len(audio) != info["count"] - 1:
        raise ValueError("MP4 stream count changed")
    v = video[0]
    if (v["codec_name"] != "h264" or v["pix_fmt"] not in ("yuv420p", "yuvj420p")
            or v["profile"] not in ("Baseline", "Constrained Baseline")
            or int(v["width"]) != info["width"] or int(v["height"]) != info["height"]
            or int(v["nb_read_frames"]) != info["frames"]
            or Fraction(v["avg_frame_rate"]) != rate
            or v.get("color_range") != "pc" or int(v["has_b_frames"]) != 0):
        raise ValueError("MP4 video geometry, count, rate, range or codec changed")
    if audio and (audio[0]["codec_name"] != "aac" or audio[0]["profile"] != "LC"
                  or int(audio[0]["sample_rate"]) != 48000 or int(audio[0]["channels"]) != 2):
        raise ValueError("expected AAC-LC stereo at 48 kHz")
    expected = info["frames"] / float(rate)
    if abs(float(v["duration"]) - expected) > 1 / float(rate):
        raise ValueError("MP4 video duration changed")
    if audio and abs(float(audio[0]["duration"]) - expected) > max(.1, 2 / float(rate)):
        raise ValueError("MP4 audio/video durations differ")
    if audio and "source_audio_samples" in info:
        duration = info["source_audio_samples"] / info["source_audio_rate"]
        if abs(float(audio[0]["duration"]) - duration) > 1024 / 48000:
            raise ValueError("MP4 audio duration differs from the decoded THP samples")


def convert_mp4(source: Path, output: Path, *, ffmpeg="ffmpeg", ffprobe="ffprobe",
                crf=18, threads=4) -> dict:
    if not 0 <= crf <= 30 or not 1 <= threads <= 16:
        raise ValueError("invalid H.264 encoder options")
    output.parent.mkdir(parents=True, exist_ok=True)
    audio_wave = output.with_suffix(".pending.wav")
    with source.open("rb") as handle, mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ) as data:
        info = thp_info(data)
        # Validate the complete source before launching a potentially long encode.
        for _ in thp_frames(data, info):
            pass
        if info["count"] == 2:
            try:
                info.update(write_thp_audio(data, info, audio_wave))
            except BaseException:
                audio_wave.unlink(missing_ok=True)
                raise
    rate = Fraction(str(round(info["fps"], 3)))
    output.parent.mkdir(parents=True, exist_ok=True)
    pending = output.with_suffix(".pending.mp4")
    descriptor = output.with_suffix(".mp4.meta")
    command = [ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
               "-xerror", "-r", str(rate), "-i", str(source)]
    if info["count"] == 2:
        command += ["-i", str(audio_wave)]
    command += ["-map", "0:v:0",
               "-vf", "scale=in_range=full:out_range=full,format=yuv420p",
               "-fps_mode", "passthrough", "-c:v", "libx264", "-preset", "fast",
               "-crf", str(crf), "-threads", str(threads), "-profile:v", "baseline",
               "-level:v", "3.2", "-color_range", "pc", "-maxrate", "8M", "-bufsize", "8M",
               "-x264-params", "bframes=0:ref=1:keyint=60:scenecut=0:fullrange=on"]
    if info["count"] == 2:
        command += ["-map", "1:a:0", "-c:a", "aac", "-profile:a", "aac_low",
                    "-b:a", "192k", "-ar", "48000", "-ac", "2"]
    else:
        command += ["-an"]
    command += ["-movflags", "+faststart", str(pending)]
    try:
        subprocess.run(command, check=True)
        probe = json.loads(subprocess.check_output(
            [ffprobe, "-v", "error", "-count_frames", "-show_streams", "-of", "json", str(pending)],
            text=True))
        validate_probe(probe, info, rate)
        # A complete decode catches truncated packets beyond the MP4 metadata.
        subprocess.run([ffmpeg, "-v", "error", "-xerror", "-nostdin", "-i", str(pending),
                        "-map", "0:v:0", "-map", "0:a?", "-f", "null", "-"], check=True)
        descriptor.write_bytes(DESCRIPTOR.pack(MAGIC, info["width"], info["height"],
            info["frames"], rate.numerator, rate.denominator, int(info["count"] == 2)))
        pending.replace(output)
    finally:
        pending.unlink(missing_ok=True)
        audio_wave.unlink(missing_ok=True)
    return {**info, "source_bytes": source.stat().st_size, "converted_bytes": output.stat().st_size,
            "source_sha256": sha(source), "converted_sha256": sha(output),
            "descriptor_sha256": sha(descriptor), "encoder_command": command,
            "verified_full_decode": True, "hardware_validated": False}


def prepare_mp4(roots, workspace, *, ffmpeg="ffmpeg", ffprobe="ffprobe", crf=18, threads=4):
    dest = workspace / "native/v2/video"
    rows = []
    for root in roots:
        if root.name != "files":
            continue
        for source in sorted(root.rglob("*")):
            if not source.is_file() or source.suffix.lower() != ".thp":
                continue
            name = source.relative_to(root)
            output = dest / name.with_suffix(".mp4")
            row = convert_mp4(source, output, ffmpeg=ffmpeg, ffprobe=ffprobe, crf=crf, threads=threads)
            rows.append({**row, "path": name.as_posix(), "mp4_path": output.relative_to(workspace).as_posix()})
    if not rows:
        raise ValueError("no THP movies found for MP4 conversion")
    return {"format": "MP4 H.264 Baseline/AAC-LC + STMP4001 descriptor", "videos": rows,
            "hardware_validated": False, "source_bytes": sum(r["source_bytes"] for r in rows),
            "converted_bytes": sum(r["converted_bytes"] for r in rows)}
