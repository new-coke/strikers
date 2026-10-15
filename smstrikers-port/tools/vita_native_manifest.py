#!/usr/bin/env python3
"""Create the optional native Vita archive verifier's SHA-256 manifest."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

CHUNK_BYTES = 1024 * 1024


def manifest(source: Path) -> tuple[bytes, dict]:
    before = source.stat()
    if not 0 < before.st_size <= 16384 * CHUNK_BYTES:
        raise ValueError("archive must contain between 1 byte and 16 GiB")
    complete = hashlib.sha256()
    blocks = bytearray()
    size = 0
    with source.open("rb") as stream:
        while data := stream.read(CHUNK_BYTES):
            complete.update(data)
            blocks.extend(hashlib.sha256(data).digest())
            size += len(data)
    after = source.stat()
    if (size != before.st_size or after.st_size != before.st_size
            or after.st_mtime_ns != before.st_mtime_ns
            or after.st_ino != before.st_ino):
        raise ValueError("archive changed while preparing its manifest")
    count = len(blocks) // 32
    result = struct.pack("<8sIIQI32sI", b"NSVERIFY", 1, CHUNK_BYTES,
                         size, count, complete.digest(), 0) + blocks
    return result, {
        "archive_sha256": complete.hexdigest(), "archive_bytes": size,
        "chunk_bytes": CHUNK_BYTES, "chunks": count,
        "manifest_sha256": hashlib.sha256(result).hexdigest(),
        "manifest_bytes": len(result),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.source.resolve() == args.output.resolve():
        parser.error("source and output must differ")
    if args.output.exists():
        parser.error("output already exists; choose a new manifest filename")
    data, report = manifest(args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("xb") as stream:
        stream.write(data)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
