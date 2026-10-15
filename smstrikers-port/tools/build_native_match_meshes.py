#!/usr/bin/env python3
"""Build a strict, prepacked GLG->GXM source-indexed mesh sidecar.

The source identity is a content signature of the immutable original GLG
display-list vertex words and Position/Color array bytes, not a GX runtime
state snapshot. The runtime never decodes a GX display list to produce GPU
vertices: the exact native vertex/index buffers are in this file already.
Only static, prepacked position+RGBA8 geometry is admitted. The original
material/shader and skinning are NOT part of this v1 sidecar.
"""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

from asset_pipeline.psarc import iter_entry, parse_psarc
from build_direct_gxm_scene import extract_gpu_color, scene_hash, sha256_file

HEADER = struct.Struct("<8sIIQ")
RECORD = struct.Struct("<7I3Q")
MAX_FILE_BYTES = 16 * 1024 * 1024
MAX_RECORDS = 5000
MAGIC = b"SMNVM001"


def source_identity(record: bytes):
    if len(record) < 28:
        return None
    magic, version, kind, src_size, gpu_size, _, _ = struct.unpack_from("<7I", record)
    if (magic, version, kind) != (0x524E5641, 2, 3) or len(record) != 28 + src_size + gpu_size:
        return None
    req = memoryview(record)[28:28 + src_size]
    if len(req) < 28:
        return None
    version, count, primitive, stride, little, attrs, raw_length = struct.unpack_from("<7I", req)
    if (version != 2 or not 3 <= count <= 64000 or attrs > 24 or
        not 0 <= primitive <= 5 or little not in (0, 1) or
        not 0 < stride <= 256):
        return None
    offset, arrays = 28, {}
    for _ in range(attrs):
        if offset + 40 > len(req):
            return None
        sem, src, typ, comp, frac, streamoff, valoff, arraystride, arrayle, size =             struct.unpack_from("<10I", req, offset)
        offset += 40
        if offset + size > len(req):
            return None
        if sem in (9, 13) and src == 3 and size:
            arrays[sem] = bytes(req[offset:offset + size])
        offset += size
    if offset + raw_length != len(req) or 9 not in arrays or len(arrays[9]) > 1_500_000:
        return None
    raw = bytes(req[offset:])
    pos = arrays[9]
    col = arrays.get(13, b"")
    return (count, primitive, len(raw), len(pos), len(col),
            scene_hash(raw), scene_hash(pos), scene_hash(col) if col else 0)


def build(archive_path: Path, output: Path, *, max_records=MAX_RECORDS,
          max_bytes=MAX_FILE_BYTES) -> dict:
    source = parse_psarc(archive_path)
    entries = [(i, name) for i, name in enumerate(source.paths, start=1)
               if name.startswith("native/v2/geometry/") and name.endswith(".avng")]
    payload = bytearray()
    known = set()
    selected = []
    rejected = {"incompatible": 0, "duplicate": 0, "budget": 0}
    with archive_path.open("rb") as input_file:
        # Stable order; never use program/texture identities inferred from filenames.
        for index, name in entries:
            raw_record = b"".join(iter_entry(input_file, source, index))
            mesh = extract_gpu_color(raw_record, max_vertices=64000, max_indices=192000)
            identity = source_identity(raw_record)
            if mesh is None or identity is None:
                rejected["incompatible"] += 1
                continue
            key = identity
            if key in known:
                rejected["duplicate"] += 1
                continue
            gpu_count = mesh["vertices"]
            idx_count = mesh["indices"]
            candidate = RECORD.pack(
                *identity[:5], gpu_count, idx_count, *identity[5:]) +                 mesh["vertex_data"] + mesh["index_data"]
            if len(candidate) + len(payload) + HEADER.size > max_bytes:
                rejected["budget"] += 1
                continue
            known.add(key)
            payload.extend(candidate)
            selected.append({"record": name, "source_draw_vertices": identity[0],
                             "source_primitive": identity[1], "gpu_vertices": gpu_count,
                             "gpu_indices": idx_count})
            if len(selected) >= max_records:
                break

    if not selected:
        raise ValueError("no immutable prepacked GLG assets could be admitted")
    output.parent.mkdir(parents=True, exist_ok=True)
    result = HEADER.pack(MAGIC, 1, len(selected), scene_hash(payload)) + payload
    output.write_bytes(result)
    report = {
        "kind": "prepacked_native_match_meshes_VERTEX_COLOR_PREVIEW_NOT_FULL_GAME",
        "source_archive": str(archive_path),
        "source_sha256": sha256_file(archive_path),
        "bytes": len(result), "sha256": sha256_file(output),
        "selected": len(selected), "eligible_archive_entries": len(entries),
        "rejections": rejected, "gpu_vertices": sum(x["gpu_vertices"] for x in selected),
        "gpu_indices": sum(x["gpu_indices"] for x in selected),
        "records": selected,
    }
    output.with_suffix(output.suffix + ".json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--archive", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--records", type=int, default=MAX_RECORDS)
    p.add_argument("--max-mib", type=int, default=16)
    a = p.parse_args()
    if not 1 <= a.records <= MAX_RECORDS:
        p.error("--records out of bounds")
    if not 1 <= a.max_mib <= 16:
        p.error("--max-mib must be 1..16")
    stats = build(a.archive, a.output, max_records=a.records,
                  max_bytes=a.max_mib * 1024 * 1024)
    print(f"prepared native GLG source index: {stats['selected']} meshes, "
          f"{stats['gpu_vertices']} vertices, {stats['gpu_indices']} indices, "
          f"{stats['bytes']} bytes (not complete Strikers materials)")


if __name__ == "__main__":
    main()
