#!/usr/bin/env python3
"""Prepare an opt-in, GX-free GXM scene from AVNR v2 static game geometry.

This is an *asset stress scene*, not an actual match: no live GX material,
skinning, stadium placement or texture identity is inferred from AVNG names.
All vertex data is already in the exact GXM 20-byte vertex layout. No packed
geometry conversion occurs in the Vita draw loop.

The input PSARC and generated scene contain game assets. Keep them private.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

from asset_pipeline.psarc import iter_entry, parse_psarc

MAGIC = b"SMGXM001"
MAX_MESHES = 16
STRIDE = 20
HEADER = struct.Struct("<7I")
MESH_HEADER = struct.Struct("<II6f")
GPU_COLOR_LAYOUT = ((0, 4, 0, 0, STRIDE, 0), (1, 4, 2, 1, STRIDE, 16))
MASK64 = (1 << 64) - 1
P1 = 11400714785074694791
P2 = 14029467366897019727
P3 = 1609587929392839161
P4 = 9650029242287828579
P5 = 2870177450012600261


def _rol(x: int, bits: int) -> int:
    return ((x << bits) | (x >> (64 - bits))) & MASK64


def _round(acc: int, value: int) -> int:
    return (_rol((acc + value * P2) & MASK64, 31) * P1) & MASK64


def _merge(acc: int, val: int) -> int:
    return ((acc ^ _round(0, val)) * P1 + P4) & MASK64


def xxh64(data: bytes) -> int:
    """Portable XXH64(seed=0), for checked AVNR v2 payloads."""
    size = len(data)
    index = 0
    if size >= 32:
        v1 = (P1 + P2) & MASK64
        v2 = P2
        v3 = 0
        v4 = (-P1) & MASK64
        while index + 32 <= size:
            v1 = _round(v1, int.from_bytes(data[index:index + 8], "little"))
            v2 = _round(v2, int.from_bytes(data[index + 8:index + 16], "little"))
            v3 = _round(v3, int.from_bytes(data[index + 16:index + 24], "little"))
            v4 = _round(v4, int.from_bytes(data[index + 24:index + 32], "little"))
            index += 32
        h = (_rol(v1, 1) + _rol(v2, 7) + _rol(v3, 12) + _rol(v4, 18)) & MASK64
        for value in (v1, v2, v3, v4):
            h = _merge(h, value)
    else:
        h = P5
    h = (h + size) & MASK64
    while index + 8 <= size:
        k = _round(0, int.from_bytes(data[index:index + 8], "little"))
        h = (_rol(h ^ k, 27) * P1 + P4) & MASK64
        index += 8
    if index + 4 <= size:
        h = (_rol(h ^ (int.from_bytes(data[index:index + 4], "little") * P1 & MASK64), 23) * P2 + P3) & MASK64
        index += 4
    while index < size:
        h = (_rol(h ^ (data[index] * P5 & MASK64), 11) * P1) & MASK64
        index += 1
    h ^= h >> 33
    h = h * P2 & MASK64
    h ^= h >> 29
    h = h * P3 & MASK64
    h ^= h >> 32
    return h


def scene_hash(data: bytes) -> int:
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & MASK64
    return h


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb", buffering=0) as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def extract_gpu_color(data: bytes, *, max_vertices=3000, max_indices=12000):
    if len(data) < HEADER.size:
        return None
    magic, version, kind, request_bytes, payload_bytes, checksum_low, checksum_high = HEADER.unpack_from(data)
    if (magic, version, kind) != (0x524E5641, 2, 3):
        return None
    if not request_bytes or not payload_bytes or len(data) != HEADER.size + request_bytes + payload_bytes:
        return None
    payload = memoryview(data)[HEADER.size + request_bytes:]
    checksum = checksum_low | (checksum_high << 32)
    if xxh64(payload.tobytes()) != checksum:
        raise ValueError("AVNR v2 record checksum mismatch")
    if len(payload) < 12:
        return None
    vertices, indices, variants = struct.unpack_from("<III", payload)
    if not 3 <= vertices <= max_vertices or not 3 <= indices <= max_indices or indices % 3 or not 1 <= variants <= 64:
        return None
    offset = 12
    end = offset + indices * 2
    if end > len(payload):
        return None
    index_blob = payload[offset:end].tobytes()
    if any(index >= vertices for (index,) in struct.iter_unpack("<H", index_blob)):
        return None
    offset = end
    selected = None
    for _ in range(variants):
        if offset + 4 > len(payload):
            return None
        (count,) = struct.unpack_from("<I", payload, offset)
        offset += 4
        if not 1 <= count <= 16 or offset + 24 * count + 4 > len(payload):
            return None
        layout = tuple(struct.unpack_from("<6I", payload, offset + 24 * a) for a in range(count))
        offset += 24 * count
        (bytes_length,) = struct.unpack_from("<I", payload, offset)
        offset += 4
        if offset + bytes_length > len(payload):
            return None
        if layout == GPU_COLOR_LAYOUT and bytes_length == vertices * STRIDE:
            selected = payload[offset:offset + bytes_length].tobytes()
        offset += bytes_length
    if offset != len(payload) or selected is None:
        return None

    bounds_min = [float("inf")] * 3
    bounds_max = [float("-inf")] * 3
    import math
    for v in range(vertices):
        x, y, z, w = struct.unpack_from("<4f", selected, v * STRIDE)
        if not all(math.isfinite(c) for c in (x, y, z, w)):
            return None
        for axis, val in enumerate((x, y, z)):
            bounds_min[axis] = min(bounds_min[axis], val)
            bounds_max[axis] = max(bounds_max[axis], val)
    extent = [hi - lo for lo, hi in zip(bounds_min, bounds_max)]
    if max(extent) <= 0.0001:
        return None
    return {
        "vertices": vertices, "indices": indices, "vertex_data": selected,
        "index_data": index_blob, "bounds_min": bounds_min,
        "bounds_max": bounds_max, "extent": extent,
    }


def build(archive_path: Path, output: Path, limit: int) -> dict:
    archive = parse_psarc(archive_path)
    geometry = [(i, name) for i, name in enumerate(archive.paths, start=1)
                if name.startswith("native/v2/geometry/") and name.endswith(".avng")]
    scene_records = []
    sources = []
    with archive_path.open("rb", buffering=0) as handle:
        # Distribute a bounded sample across the sorted archive to avoid
        # selecting consecutive fragments of just one geometry family.
        candidates = geometry[::max(1, len(geometry) // (limit * 24))]
        for index, name in candidates:
            if len(scene_records) == limit:
                break
            data = b"".join(iter_entry(handle, archive, index))
            decoded = extract_gpu_color(data)
            # The stress scene deliberately samples substantial meshes. This
            # selection policy must not filter the live match's asset decoder.
            if not decoded or decoded["vertices"] < 20 or decoded["indices"] < 30:
                continue
            body = MESH_HEADER.pack(decoded["vertices"], decoded["indices"],
                                    *decoded["bounds_min"], *decoded["bounds_max"])
            body += decoded["vertex_data"] + decoded["index_data"]
            scene_records.append(body)
            sources.append({key: decoded[key] for key in ("vertices", "indices", "bounds_min", "bounds_max")} |
                           {"record": name, "record_sha256": hashlib.sha256(data).hexdigest()})

    if not scene_records:
        raise ValueError("no checked GPU position+RGBA8 static geometries found")
    payload = b"".join(scene_records)
    wire = struct.pack("<8sIIQ", MAGIC, 1, len(scene_records), scene_hash(payload)) + payload
    if len(wire) > 4 * 1024 * 1024:
        raise ValueError("native scene exceeds 4 MiB cap")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(wire)
    info = {
        "kind": "gpu_static_geometry_replay_NOT_a_live_match",
        "archive": str(archive_path.resolve()),
        "archive_sha256": sha256_file(archive_path),
        "file": str(output.resolve()), "file_sha256": hashlib.sha256(wire).hexdigest(),
        "mesh_count": len(sources), "bytes": len(wire), "index_sum": sum(x["indices"] for x in sources),
        "vertex_sum": sum(x["vertices"] for x in sources),
        "records": sources,
    }
    output.with_suffix(output.suffix + ".json").write_text(json.dumps(info, indent=2) + "\n")
    return info


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--meshes", type=int, default=12)
    args = parser.parse_args()
    if not 1 <= args.meshes <= MAX_MESHES:
        parser.error(f"--meshes must be 1..{MAX_MESHES}")
    info = build(args.archive, args.output, args.meshes)
    print(f"native GXM scene (NOT live match): {info['mesh_count']} meshes, "
          f"{info['vertex_sum']} vertices, {info['index_sum']} indices, "
          f"{info['bytes']} bytes, SHA256 {info['file_sha256']}")


if __name__ == "__main__":
    assert xxh64(b"") == 0xef46db3751d8e999
    assert xxh64(b"abc") == 0x44bc2cf5ad770999
    main()
