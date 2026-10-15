#!/usr/bin/env python3
"""Create/list/verify PSARC 1.4 archives without proprietary host tools.

Distributed under CC0-1.0. See LICENSE-CC0.txt in this directory.

The writer supports the following container features:
- PSARC 1.4, zlib tag, 64 KiB blocks
- original payload blocks stored uncompressed; optional prefix-limited zlib for cold native sidecars
- relative paths with optional case-insensitive hashing
- optional content deduplication

Only Python's standard library is required.
The implementation uses the documented container layout and contains no Sony
SDK source code.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import os
from pathlib import Path
import struct
import sys
import tempfile
import zlib

MAGIC = b"PSAR"
VERSION = 0x00010004
COMPRESSION = b"zlib"
HEADER = struct.Struct(">4sI4sIIIII")
ENTRY_SIZE = 30
BLOCK_SIZE = 64 * 1024
BLOCK_WORD_SIZE = 2
DATA_ALIGNMENT = 8 * 1024
FLAG_IGNORE_CASE = 1
MAX_U40 = (1 << 40) - 1
COPY_CHUNK = 4 * 1024 * 1024


@dataclass(frozen=True)
class SourceFile:
    archive_path: str
    source: Path
    size: int


@dataclass
class Payload:
    source: Path | None
    size: int
    zindex: int = 0
    offset: int = 0


@dataclass(frozen=True)
class TocEntry:
    digest: bytes
    zindex: int
    size: int
    offset: int


@dataclass(frozen=True)
class ParsedArchive:
    version: int
    compression: bytes
    toc_length: int
    entry_size: int
    block_size: int
    flags: int
    entries: tuple[TocEntry, ...]
    block_sizes: tuple[int, ...]
    paths: tuple[str, ...]


def _align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def _u40(value: int) -> bytes:
    if not 0 <= value <= MAX_U40:
        raise ValueError(f"value does not fit PSARC uint40: {value}")
    return value.to_bytes(5, "big")


def _path_digest(path: str, ignore_case: bool) -> bytes:
    encoded = (path.upper() if ignore_case else path).encode("utf-8")
    return hashlib.md5(encoded).digest()


def _block_words(size: int, block_size: int = BLOCK_SIZE) -> list[int]:
    words: list[int] = []
    remaining = size
    while remaining:
        chunk = min(block_size, remaining)
        words.append(0 if chunk == block_size else chunk)
        remaining -= chunk
    return words


def _sha256_file(path: Path) -> bytes:
    digest = hashlib.sha256()
    with path.open("rb", buffering=0) as handle:
        while True:
            block = handle.read(COPY_CHUNK)
            if not block:
                break
            digest.update(block)
    return digest.digest()


def collect_files(roots: list[Path]) -> list[SourceFile]:
    seen_roots: set[str] = set()
    files: list[SourceFile] = []
    for raw_root in roots:
        if raw_root.is_symlink():
            raise ValueError(f"symlink archive root: {raw_root}")
        root = raw_root.resolve()
        if not root.is_dir():
            raise FileNotFoundError(root)
        name = root.name
        folded = name.casefold()
        if folded in seen_roots:
            raise ValueError(f"duplicate archive root name: {name}")
        seen_roots.add(folded)
        parent = root.parent
        for source in root.rglob("*"):
            if source.is_symlink():
                raise ValueError(f"symlink in archive input: {source}")
            if not source.is_file():
                continue
            archive_path = source.relative_to(parent).as_posix()
            _validate_path(archive_path)
            files.append(SourceFile(archive_path, source, source.stat().st_size))
    files.sort(key=lambda item: (item.archive_path.casefold(), item.archive_path))
    return files


def _validate_path(path: str) -> None:
    if (not path or "\\" in path or ":" in path or
            any(ord(c) < 32 or ord(c) == 127 for c in path) or
            any(part in ("", ".", "..") for part in path.split("/"))):
        raise ValueError(f"unsafe archive path: {path!r}")


def _dedup_payloads(files: list[SourceFile], merge_duplicates: bool) -> tuple[list[Payload], list[int]]:
    if not merge_duplicates:
        return [Payload(item.source, item.size) for item in files], list(range(len(files)))

    groups_by_size: dict[int, list[int]] = {}
    for index, item in enumerate(files):
        groups_by_size.setdefault(item.size, []).append(index)

    payloads: list[Payload] = []
    payload_for_file = [-1] * len(files)
    for indices in groups_by_size.values():
        if len(indices) == 1:
            index = indices[0]
            payload_for_file[index] = len(payloads)
            payloads.append(Payload(files[index].source, files[index].size))
            continue

        by_identity: dict[tuple[int, int], int] = {}
        unresolved: list[int] = []
        for index in indices:
            stat = files[index].source.stat()
            identity = (stat.st_dev, stat.st_ino)
            if stat.st_ino and identity in by_identity:
                payload_for_file[index] = by_identity[identity]
            else:
                unresolved.append(index)
                if stat.st_ino:
                    by_identity[identity] = -1

        by_digest: dict[bytes, int] = {}
        for index in unresolved:
            digest = _sha256_file(files[index].source)
            existing = by_digest.get(digest)
            if existing is None:
                payload_index = len(payloads)
                payloads.append(Payload(files[index].source, files[index].size))
                by_digest[digest] = payload_index
            else:
                payload_index = existing
            payload_for_file[index] = payload_index
            stat = files[index].source.stat()
            if stat.st_ino:
                by_identity[(stat.st_dev, stat.st_ino)] = payload_index

        for index in indices:
            if payload_for_file[index] >= 0:
                continue
            stat = files[index].source.stat()
            payload_index = by_identity.get((stat.st_dev, stat.st_ino), -1)
            if payload_index < 0:
                raise AssertionError("failed to resolve deduplicated payload")
            payload_for_file[index] = payload_index

    if any(index < 0 for index in payload_for_file):
        raise AssertionError("incomplete PSARC payload map")
    return payloads, payload_for_file


def create_psarc(
    output: Path,
    roots: list[Path],
    *,
    ignore_case: bool = True,
    merge_duplicates: bool = True,
    compress_prefixes: tuple[str, ...] = (),
) -> dict:
    if output.is_symlink():
        raise ValueError("refusing a symlink output")
    if any(output.resolve().is_relative_to(root.resolve()) for root in roots):
        raise ValueError("archive output must be outside input trees")
    files = collect_files(roots)
    archive_paths = [item.archive_path for item in files]
    keys = [(p.upper() if ignore_case else p) for p in archive_paths]
    if len(set(keys)) != len(keys):
        raise ValueError("archive paths collide under the selected case policy")
    manifest = "\n".join(archive_paths).encode("utf-8")
    payloads, payload_for_file = _dedup_payloads(files, merge_duplicates)

    # Compression is restricted to explicitly selected cold native sidecars.
    # Original game payloads remain stored. Deduplicated aliases outside the
    # selected prefixes protect that shared payload from compression too.
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="psarc-native-",dir=output.parent) as compressed_workspace:
        selected={payload_for_file[i] for i,f in enumerate(files) if any(f.archive_path.startswith(p) for p in compress_prefixes)}
        selected.difference_update(payload_for_file[i] for i,f in enumerate(files) if not any(f.archive_path.startswith(p) for p in compress_prefixes))
        payload_blocks=[];stored_sizes=[]
        for index,payload in enumerate(payloads):
            if index not in selected:
                payload_blocks.append(_block_words(payload.size));stored_sizes.append(payload.size);continue
            encoded_path=Path(compressed_workspace)/str(index)
            blocks=[];stored=0
            with payload.source.open("rb") as source,encoded_path.open("wb") as target:
                remaining=payload.size
                while remaining:
                    block=source.read(min(BLOCK_SIZE,remaining))
                    if not block:raise IOError("truncated native payload while compressing")
                    candidate=zlib.compress(block,9)
                    encoded=candidate if len(candidate)<len(block) else block
                    target.write(encoded);blocks.append(0 if len(encoded)==BLOCK_SIZE else len(encoded));stored+=len(encoded);remaining-=len(block)
            payload.source=encoded_path;payload_blocks.append(blocks);stored_sizes.append(stored)
        entry_count = 1 + len(files)
        block_count = 1 + len(_block_words(len(manifest)))
        for payload in payloads:
            block_count += len(_block_words(payload.size))

        toc_length = HEADER.size + entry_count * ENTRY_SIZE + block_count * BLOCK_WORD_SIZE
        data_start = _align_up(toc_length, DATA_ALIGNMENT)
        padding = data_start - toc_length
        if padding > 0xFFFF:
            raise AssertionError("PSARC TOC padding does not fit block table word")

        block_sizes: list[int] = [padding]
        manifest_payload = Payload(None, len(manifest), zindex=1, offset=data_start)
        block_sizes.extend(_block_words(manifest_payload.size))
        data_offset = data_start + manifest_payload.size

        for index,payload in enumerate(payloads):
            payload.zindex = len(block_sizes)
            payload.offset = data_offset
            block_sizes.extend(payload_blocks[index])
            data_offset += stored_sizes[index]

        if len(block_sizes) != block_count:
            raise AssertionError("PSARC block table size changed during layout")

        flags = FLAG_IGNORE_CASE if ignore_case else 0
        entries = [
            TocEntry(bytes(16), manifest_payload.zindex, manifest_payload.size, manifest_payload.offset)
        ]
        for file_index, item in enumerate(files):
            payload = payloads[payload_for_file[file_index]]
            entries.append(
                TocEntry(
                    _path_digest(item.archive_path, ignore_case),
                    payload.zindex,
                    item.size,
                    payload.offset,
                )
            )

        output = output.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        fd, temporary_name = tempfile.mkstemp(
            prefix=output.name + ".", suffix=".tmp", dir=output.parent
        )
        os.close(fd)
        temporary = Path(temporary_name)
        try:
            with temporary.open("wb", buffering=0) as out:
                out.write(
                    HEADER.pack(
                        MAGIC,
                        VERSION,
                        COMPRESSION,
                        toc_length,
                        ENTRY_SIZE,
                        entry_count,
                        BLOCK_SIZE,
                        flags,
                    )
                )
                for entry in entries:
                    out.write(entry.digest)
                    out.write(struct.pack(">I", entry.zindex))
                    out.write(_u40(entry.size))
                    out.write(_u40(entry.offset))
                for word in block_sizes:
                    out.write(struct.pack(">H", word))
                current = out.tell()
                if current != toc_length:
                    raise AssertionError(f"TOC size mismatch: {current} != {toc_length}")
                out.write(bytes(padding))
                if out.tell() != data_start:
                    raise AssertionError("PSARC data alignment mismatch")
                out.write(manifest)
                for index,payload in enumerate(payloads):
                    if payload.source is None:
                        raise AssertionError("missing PSARC payload source")
                    with payload.source.open("rb", buffering=0) as src:
                        remaining = stored_sizes[index]
                        while remaining:
                            block = src.read(min(COPY_CHUNK, remaining))
                            if not block:
                                raise IOError(f"{payload.source}: truncated while packing")
                            out.write(block)
                            remaining -= len(block)
                if out.tell() != data_offset:
                    raise AssertionError("PSARC final size mismatch")
            os.replace(temporary, output)
        except Exception:
            temporary.unlink(missing_ok=True)
            raise

        logical_bytes = sum(item.size for item in files)
        unique_bytes = sum(payload.size for payload in payloads)
        return {
            "archive": str(output),
            "archive_bytes": output.stat().st_size,
            "entries": len(files),
            "unique_payloads": len(payloads),
            "logical_payload_bytes": logical_bytes,
            "unique_payload_bytes": unique_bytes,
            "deduplicated_bytes": logical_bytes - unique_bytes,
            "manifest_bytes": len(manifest),
            "toc_bytes": toc_length,
            "data_offset": data_start,
            "block_count": len(block_sizes),
            "ignore_case": ignore_case,
            "merge_duplicates": merge_duplicates,
            "compressed_prefixes": list(compress_prefixes),
            "native_compression_saved_bytes": sum(p.size-n for p,n in zip(payloads,stored_sizes)),
        }


def _parse_entry(data: bytes) -> TocEntry:
    if len(data) != ENTRY_SIZE:
        raise ValueError("truncated PSARC entry")
    return TocEntry(
        data[:16],
        struct.unpack(">I", data[16:20])[0],
        int.from_bytes(data[20:25], "big"),
        int.from_bytes(data[25:30], "big"),
    )


def _stored_block_size(word: int, block_size: int) -> int:
    return block_size if word == 0 else word


def _iter_entry(
    handle,
    entry: TocEntry,
    block_sizes: tuple[int, ...],
    block_size: int,
    compression: bytes,
) :
    if entry.size == 0:
        return
    if entry.offset < 0:
        raise ValueError("invalid PSARC entry offset")
    handle.seek(entry.offset)
    remaining = entry.size
    zindex = entry.zindex
    while remaining:
        if zindex >= len(block_sizes):
            raise ValueError("PSARC entry block index exceeds table")
        word = block_sizes[zindex]
        stored = _stored_block_size(word, block_size)
        data = handle.read(stored)
        if len(data) != stored:
            raise ValueError("truncated PSARC payload block")

        expected = min(block_size, remaining)
        decoded = data
        if compression == b"zlib" and word != 0 and stored < expected:
            decoder = zlib.decompressobj()
            decoded = decoder.decompress(data, expected + 1)
            if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
                raise ValueError("invalid compressed PSARC block")
        if len(decoded) != expected:
            raise ValueError("PSARC block has an invalid logical extent")
        yield decoded
        remaining -= expected
        zindex += 1


def _read_entry(handle, entry, block_sizes, block_size, compression) -> bytes:
    return b"".join(_iter_entry(handle, entry, block_sizes, block_size, compression))


def iter_entry(handle, archive: ParsedArchive, index: int):
    """Stream one TOC entry in bounded blocks, including compressed archives."""
    yield from _iter_entry(handle, archive.entries[index], archive.block_sizes,
                           archive.block_size, archive.compression)


def parse_psarc(path: Path, *, read_manifest: bool = True) -> ParsedArchive:
    path = path.resolve()
    file_size = path.stat().st_size
    with path.open("rb", buffering=0) as handle:
        header = handle.read(HEADER.size)
        if len(header) != HEADER.size:
            raise ValueError("truncated PSARC header")
        magic, version, compression, toc_length, entry_size, count, block_size, flags = HEADER.unpack(header)
        if magic != MAGIC:
            raise ValueError("not a PSARC archive")
        if version != VERSION:
            raise ValueError(f"unsupported PSARC version 0x{version:08x}")
        if compression != COMPRESSION or flags & ~FLAG_IGNORE_CASE:
            raise ValueError("unsupported PSARC codec or flags")
        if entry_size != ENTRY_SIZE:
            raise ValueError(f"unsupported PSARC entry size {entry_size}")
        if block_size != BLOCK_SIZE:
            raise ValueError(f"unsupported PSARC block size {block_size}")
        if toc_length < HEADER.size + count * entry_size or toc_length > file_size:
            raise ValueError("invalid PSARC TOC length")
        entries = tuple(_parse_entry(handle.read(entry_size)) for _ in range(count))
        block_bytes = toc_length - HEADER.size - count * entry_size
        if block_bytes % BLOCK_WORD_SIZE:
            raise ValueError("misaligned PSARC block-size table")
        block_sizes = tuple(
            struct.unpack(">H", handle.read(BLOCK_WORD_SIZE))[0]
            for _ in range(block_bytes // BLOCK_WORD_SIZE)
        )
        if not entries:
            raise ValueError("PSARC has no manifest entry")
        if entries[0].digest != bytes(16):
            raise ValueError("PSARC manifest entry hash is not zero")

        paths: tuple[str, ...] = ()
        if read_manifest:
            manifest = _read_entry(handle, entries[0], block_sizes, block_size, compression)
            try:
                decoded = manifest.decode("utf-8")
            except UnicodeDecodeError as exc:
                raise ValueError("PSARC manifest is not UTF-8") from exc
            paths = tuple(decoded.split("\n")) if decoded else ()
            if len(paths) != count - 1:
                raise ValueError(
                    f"PSARC manifest has {len(paths)} paths for {count - 1} entries"
                )

    return ParsedArchive(
        version, compression, toc_length, entry_size, block_size, flags,
        entries, block_sizes, paths,
    )


def verify_psarc(path: Path, *, full: bool = True) -> dict:
    archive = parse_psarc(path)
    file_size = path.resolve().stat().st_size
    ignore_case = bool(archive.flags & FLAG_IGNORE_CASE)
    if archive.flags & ~FLAG_IGNORE_CASE:
        raise ValueError(f"unsupported PSARC flags 0x{archive.flags:x}")

    seen: set[str] = set()
    for index, path_name in enumerate(archive.paths, start=1):
        _validate_path(path_name)
        key = path_name.upper() if ignore_case else path_name
        if key in seen:
            raise ValueError(f"duplicate PSARC manifest path: {path_name}")
        seen.add(key)
        if archive.entries[index].digest != _path_digest(path_name, ignore_case):
            raise ValueError(f"PSARC path hash mismatch: {path_name}")

    for entry in archive.entries:
        if entry.offset < archive.toc_length or entry.offset > file_size or entry.size > MAX_U40:
            raise ValueError("PSARC entry is outside archive")
        remaining = entry.size
        zindex = entry.zindex
        physical = 0
        while remaining:
            if zindex >= len(archive.block_sizes):
                raise ValueError("PSARC block table underrun")
            stored = _stored_block_size(archive.block_sizes[zindex], archive.block_size)
            physical += stored
            remaining -= min(archive.block_size, remaining)
            zindex += 1
        if entry.offset + physical > file_size:
            raise ValueError("PSARC entry payload exceeds archive")

    if full:
        with path.resolve().open("rb", buffering=0) as handle:
            for entry in archive.entries:
                for _ in _iter_entry(
                    handle, entry, archive.block_sizes,
                    archive.block_size, archive.compression,
                ):
                    pass

    return {
        "archive": str(path.resolve()),
        "archive_bytes": file_size,
        "entries": len(archive.paths),
        "toc_entries": len(archive.entries),
        "block_count": len(archive.block_sizes),
        "flags": archive.flags,
        "compression": archive.compression.decode("ascii", errors="replace"),
        "full_payload_check": full,
    }


def extract_entry(path: Path, archive_path: str) -> bytes:
    archive = parse_psarc(path)
    ignore_case = bool(archive.flags & FLAG_IGNORE_CASE)
    needle = archive_path.casefold() if ignore_case else archive_path
    match = -1
    for index, candidate in enumerate(archive.paths, start=1):
        key = candidate.casefold() if ignore_case else candidate
        if key == needle:
            match = index
            break
    if match < 0:
        raise KeyError(archive_path)
    with path.resolve().open("rb", buffering=0) as handle:
        return _read_entry(
            handle, archive.entries[match], archive.block_sizes,
            archive.block_size, archive.compression,
        )


def _cli() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    create = sub.add_parser("create", help="create an uncompressed PSARC")
    create.add_argument("output", type=Path)
    create.add_argument("roots", nargs="+", type=Path)
    create.add_argument("--case-sensitive", action="store_true")
    create.add_argument("--no-merge-dups", action="store_true")

    listing = sub.add_parser("list", help="list archive paths")
    listing.add_argument("archive", type=Path)

    verify = sub.add_parser("verify", help="validate archive structure and payloads")
    verify.add_argument("archive", type=Path)
    verify.add_argument("--metadata-only", action="store_true")

    ns = parser.parse_args()
    if ns.command == "create":
        report = create_psarc(
            ns.output,
            ns.roots,
            ignore_case=not ns.case_sensitive,
            merge_duplicates=not ns.no_merge_dups,
        )
        for key, value in report.items():
            print(f"{key}: {value}")
        return 0
    if ns.command == "list":
        archive = parse_psarc(ns.archive)
        for path_name in archive.paths:
            print(path_name)
        return 0
    if ns.command == "verify":
        report = verify_psarc(ns.archive, full=not ns.metadata_only)
        for key, value in report.items():
            print(f"{key}: {value}")
        return 0
    return 2


if __name__ == "__main__":
    try:
        raise SystemExit(_cli())
    except (OSError, ValueError, KeyError, AssertionError, zlib.error) as exc:
        print(f"PSARC error: {exc}", file=sys.stderr)
        raise SystemExit(1)
