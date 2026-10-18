#!/usr/bin/env python3
"""Validate LiveArea source assets and their exact paths/bytes in a Vita VPK."""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
import zipfile
import zlib

IMAGES = {"icon0.png": (128, 128), "pic0.png": (960, 544),
          "livearea/contents/bg.png": (840, 500), "livearea/contents/startup.png": (280, 158)}
TEMPLATE = "livearea/contents/template.xml"


def image_info(data: bytes, expected: tuple[int, int], *, opaque: bool = False) -> dict:
    if len(data) > 420 * 1024 or data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("PNG signature/size is invalid")
    at = 8
    chunks = []
    payloads = {}
    while at < len(data):
        if at + 12 > len(data):
            raise ValueError("Truncated PNG chunk")
        size, = struct.unpack_from(">I", data, at)
        kind = data[at + 4:at + 8]
        end = at + 12 + size
        if end > len(data) or zlib.crc32(data[at + 4:end - 4]) != struct.unpack_from(">I", data, end - 4)[0]:
            raise ValueError("Invalid PNG chunk extent/CRC")
        chunks.append(kind)
        payloads[kind] = payloads.get(kind, b"") + data[at + 8:end - 4]
        at = end
        if kind == b"IEND":
            break
    if (not chunks or chunks[0] != b"IHDR" or chunks[-1] != b"IEND" or at != len(data)
            or chunks.count(b"IHDR") != 1 or len(payloads[b"IHDR"]) != 13):
        raise ValueError("Invalid PNG structure")
    width, height, bits, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", payloads[b"IHDR"])
    if (width, height) != expected or (bits, color, compression, filtering, interlace) != (8, 3, 0, 0, 0):
        raise ValueError(f"Expected non-interlaced indexed PNG-8 {expected}, got {(width, height, bits, color, interlace)}")
    palette = payloads.get(b"PLTE", b"")
    if (not palette or len(palette) % 3 or len(palette) > 768 or b"IDAT" not in chunks
            or chunks.count(b"PLTE") != 1 or chunks.index(b"PLTE") > chunks.index(b"IDAT")
            or len(payloads.get(b"tRNS", b"")) > len(palette) // 3):
        raise ValueError("Invalid indexed PNG palette/data")
    if opaque and any(alpha != 255 for alpha in payloads.get(b"tRNS", b"")):
        raise ValueError("Bubble icon must be opaque")
    decoder = zlib.decompressobj()
    try:
        raw = decoder.decompress(payloads[b"IDAT"], (width + 1) * height + 1)
    except zlib.error as exc:
        raise ValueError("Invalid PNG compressed data") from exc
    if len(raw) != (width + 1) * height or not decoder.eof or decoder.unused_data:
        raise ValueError("Invalid PNG pixel extent")
    return {"width": width, "height": height, "colors": len(palette) // 3, "bytes": len(data)}


def validate(source: Path, vpk: Path | None = None, executable: Path | None = None) -> dict:
    result = {"assets": {}, "template": TEMPLATE}
    for name, dimensions in IMAGES.items():
        data = (source / name).read_bytes()
        result["assets"][name] = image_info(data, dimensions, opaque=name == "icon0.png")
        result["assets"][name]["sha256"] = hashlib.sha256(data).hexdigest()
    xml = (source / TEMPLATE).read_bytes()
    if len(xml) > 32 * 1024 or b"\n" in xml.replace(b"\r\n", b""):
        raise ValueError("LiveArea XML must be UTF-8 with CRLF line endings, at most 32 KiB")
    root = ET.fromstring(xml.decode("utf-8"))
    if (root.tag != "livearea" or root.get("style") != "a1" or root.get("format-ver") != "01.00"
            or root.findtext("livearea-background/image") != "bg.png"
            or root.findtext("gate/startup-image") != "startup.png"):
        raise ValueError("LiveArea must bind bg.png as background and startup.png as the a1 launch gate")
    result["content_rev"] = root.get("content-rev")
    if vpk:
        with zipfile.ZipFile(vpk) as archive:
            names = archive.namelist()
            if any(count != 1 for count in Counter(names).values()) or archive.testzip() is not None:
                raise ValueError("VPK has duplicate names or corrupt entries")
            for name in [*IMAGES, TEMPLATE]:
                if archive.read("sce_sys/" + name) != (source / name).read_bytes():
                    raise ValueError("VPK missing or stale LiveArea asset: " + name)
            if "sce_sys/param.sfo" not in names or "eboot.bin" not in names:
                raise ValueError("VPK lacks param.sfo or eboot.bin")
            if executable and archive.read("eboot.bin") != executable.read_bytes():
                raise ValueError("Packaged eboot.bin differs from the built SELF")
        result["vpk"] = str(vpk.resolve())
        result["vpk_sha256"] = hashlib.sha256(vpk.read_bytes()).hexdigest()
    elif executable:
        raise ValueError("--self requires --vpk")
    return result


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source-dir", type=Path, default=Path(__file__).resolve().parents[1] / "assets/vita/sce_sys")
    p.add_argument("--vpk", type=Path)
    p.add_argument("--self", dest="executable", type=Path)
    a = p.parse_args()
    try:
        print(json.dumps(validate(a.source_dir, a.vpk, a.executable), indent=2))
        return 0
    except (OSError, ValueError, ET.ParseError, KeyError, zipfile.BadZipFile) as exc:
        p.exit(1, f"Vita package validation failed: {exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
