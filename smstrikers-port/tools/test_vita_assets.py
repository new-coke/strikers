#!/usr/bin/env python3
"""Synthetic discs/archives only; no retail game data is required."""
from pathlib import Path
import argparse
import hashlib
import os
import struct
import subprocess
import tempfile
import unittest
import zlib
from unittest.mock import patch

from asset_pipeline.psarc import SourceFile, create_psarc, extract_entry, parse_psarc, verify_psarc
from vita_prepare_assets import prepare, verify_sources
from test_extract_disc import write_disc, payload_offset, PAYLOAD

PROBE = None


class AssetsTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.base = Path(self.tmp.name)
        self.tree = self.base / "tree"
        self.files = self.tree / "files"
        self.sys = self.tree / "sys"
        (self.files / "Art").mkdir(parents=True)
        self.sys.mkdir()
        (self.files / "common.ini").write_bytes(b"common config=" * 6000)
        (self.files / "Art/texture.glt").write_bytes(bytes(range(256)) * 513)
        (self.files / "copy.bin").write_bytes((self.files / "Art/texture.glt").read_bytes())
        (self.files / "empty.bin").write_bytes(b"")
        (self.sys / "boot.bin").write_bytes(b"G4QP01" + bytes(0x440 - 6))
        self.archive = self.base / "sms.psarc"

    def tearDown(self):
        self.tmp.cleanup()

    def pack(self):
        return create_psarc(self.archive, [self.files, self.sys])

    def probe(self, archive=None, mode=None):
        if PROBE:
            subprocess.run([str(PROBE), str(archive or self.archive), mode or str(self.tree)],
                           check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def test_reader_profile_on_and_off(self):
        self.pack()
        if PROBE:
            for enabled in ("0", "1"):
                env = dict(os.environ, STRIKERS_ASSET_IO_PROFILE=enabled)
                subprocess.run([str(PROBE), str(self.archive), str(self.tree)],
                               check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)

    def test_native_only_compression_preserves_stored_game_blocks(self):
        native=self.tree/"native";native.mkdir();payload=b"canonical native geometry"*10000
        (native/"mesh.avnr").write_bytes(payload)
        report=create_psarc(self.archive,[self.files,self.sys,native],compress_prefixes=("native/",))
        archive=parse_psarc(self.archive)
        entry=archive.entries[archive.paths.index("native/mesh.avnr")+1]
        self.assertLess(archive.block_sizes[entry.zindex],65536)
        original=archive.entries[archive.paths.index("files/Art/texture.glt")+1]
        self.assertEqual(archive.block_sizes[original.zindex],0)
        self.assertEqual(extract_entry(self.archive,"native/mesh.avnr"),payload)
        self.assertGreater(report["native_compression_saved_bytes"],0)
        self.assertEqual(verify_psarc(self.archive)["entries"],len(archive.paths))
        self.probe()
        before=self.archive.read_bytes()
        create_psarc(self.archive,[self.files,self.sys,native],compress_prefixes=("native/",))
        self.assertEqual(before,self.archive.read_bytes())

    def test_existing_psarc_roundtrip_and_input_protection(self):
        self.pack();before=self.archive.read_bytes();output=self.base/'derived.psarc'
        result=prepare('strikers',None,[],output,source_archive=self.archive)
        self.assertEqual(result['source_disc']['kind'],'psarc')
        self.assertEqual(before,self.archive.read_bytes());self.assertEqual(before,output.read_bytes())
        with self.assertRaises(ValueError):prepare('strikers',None,[],self.archive,source_archive=self.archive,replace=True)

    def test_native_only_package_keeps_game_archive_independent(self):
        with self.assertRaises(ValueError):
            prepare("generic",None,[self.files],self.archive,native_only=True)
        entries=[(True,"",0,2),(False,"common.ini",0,len(PAYLOAD))]
        entries[1]=(False,"common.ini",payload_offset(entries),len(PAYLOAD))
        iso=self.base/"disc.iso";write_disc(iso,entries)
        native=self.base/"sidecars"/"native";native.mkdir(parents=True)
        payload=b"versioned native payload"*10000
        (native/"mesh.avnr").write_bytes(payload)
        with patch("asset_pipeline.native.prepare_native",return_value=(native,{"compiled":1})):
            report=prepare("strikers",iso,[],self.archive,native_compiler=self.base/"compiler",native_only=True)
        self.assertTrue(report["native_only"])
        self.assertEqual(parse_psarc(self.archive).paths,("native/mesh.avnr",))
        self.assertEqual(extract_entry(self.archive,"native/mesh.avnr"),payload)
        if PROBE:
            subprocess.run([str(PROBE),str(self.archive),str(native.parent),"--reader-only"],check=True,
                           stdout=subprocess.PIPE,stderr=subprocess.PIPE)

    def test_roundtrip_dedup_determinism_and_real_reader(self):
        report = self.pack()
        self.assertEqual(report["deduplicated_bytes"], 131328)
        a = parse_psarc(self.archive)
        one = a.entries[a.paths.index("files/Art/texture.glt") + 1]
        two = a.entries[a.paths.index("files/copy.bin") + 1]
        self.assertEqual((one.offset, one.zindex), (two.offset, two.zindex))
        self.assertEqual(extract_entry(self.archive, "FILES/ART/TEXTURE.GLT"),
                         (self.files / "Art/texture.glt").read_bytes())
        verify_psarc(self.archive)
        verify_sources(self.archive, [self.files, self.sys])
        self.probe()
        first = self.archive.read_bytes()
        self.pack()
        self.assertEqual(self.archive.read_bytes(), first)

    def test_hardlinks_and_no_dedup(self):
        os.link(self.files / "copy.bin", self.files / "hardlink.bin")
        self.assertEqual(self.pack()["deduplicated_bytes"], 262656)
        self.assertEqual(create_psarc(self.archive, [self.files, self.sys],
                                     merge_duplicates=False)["deduplicated_bytes"], 0)
        self.probe()

    def test_case_collision_and_symlink_refused_before_publish(self):
        # Represent two case-sensitive input names even on macOS's default
        # case-insensitive filesystem, where creating COMMON.INI replaces common.ini.
        source = self.files / "common.ini"
        collision = [SourceFile("files/common.ini", source, source.stat().st_size),
                     SourceFile("files/COMMON.INI", source, source.stat().st_size)]
        with patch("asset_pipeline.psarc.collect_files", return_value=collision):
            with self.assertRaises(ValueError): self.pack()
        self.assertFalse(self.archive.exists())
        (self.files / "link").symlink_to(self.sys / "boot.bin")
        with self.assertRaises(ValueError): self.pack()
        self.assertFalse(self.archive.exists())

    def test_source_mismatch_detected(self):
        self.pack()
        (self.files / "copy.bin").write_bytes(b"changed")
        with self.assertRaises(ValueError): verify_sources(self.archive, [self.files, self.sys])

    def test_pipeline_and_existing_output(self):
        r = prepare("generic", None, [self.files, self.sys], self.archive)
        self.assertEqual(r["archive_sha256"], hashlib.sha256(self.archive.read_bytes()).hexdigest())
        self.assertEqual(len(r["verified_files"]), 5)
        with self.assertRaises(FileExistsError):
            prepare("generic", None, [self.files], self.archive)
        with self.assertRaises(ValueError):
            prepare("generic", None, [self.files], self.files / "inside.psarc")

    def test_strikers_disc_adapter_and_wrong_disc(self):
        entries = [(True, "", 0, 2), (False, "common.ini", 0, len(PAYLOAD))]
        entries[1] = (False, "common.ini", payload_offset(entries), len(PAYLOAD))
        iso = self.base / "disc.iso"
        write_disc(iso, entries)
        r = prepare("strikers", iso, [], self.archive)
        self.assertEqual(r["adapter"]["game_id"], "G4QE01")
        self.assertEqual(extract_entry(self.archive, "files/common.ini"), PAYLOAD)
        self.assertEqual(extract_entry(self.archive, "sys/boot.bin")[:6], b"G4QE01")
        bad = bytearray(iso.read_bytes()); bad[:6] = b"GSAP01"; iso.write_bytes(bad)
        with self.assertRaises(ValueError): prepare("strikers", iso, [], self.base / "wrong.psarc")
        self.assertFalse((self.base / "wrong.psarc").exists())

    def test_disc_case_collision_before_extraction(self):
        entries = [(True, "", 0, 3), (False, "common.ini", 0, len(PAYLOAD)),
                   (False, "COMMON.INI", 0, len(PAYLOAD))]
        off = payload_offset(entries)
        entries = [(d, name, a if d else off, b) for d, name, a, b in entries]
        iso = self.base / "collision.iso"; write_disc(iso, entries)
        with self.assertRaises(ValueError): prepare("strikers", iso, [], self.archive)
        self.assertFalse(self.archive.exists())

    def test_corrupt_metadata_and_truncated_archive(self):
        self.pack()
        good = self.archive.read_bytes()
        bad_cases = [good[:31], good[:-1]]
        for at, data in ((4, b"\0\0\0\0"), (8, b"lzma"), (12, b"\xff" * 4),
                         (20, b"\xff" * 4), (28, b"\0\0\0\2"), (32, b"\x01")):
            bad = bytearray(good); bad[at:at + len(data)] = data; bad_cases.append(bad)
        for i, bad in enumerate(bad_cases):
            p = self.base / f"bad-{i}.psarc"; p.write_bytes(bad)
            with self.assertRaises((ValueError, struct.error)): verify_psarc(p)
            self.probe(p, "--reject")

    def test_corrupt_path(self):
        self.pack()
        a = parse_psarc(self.archive)
        b = bytearray(self.archive.read_bytes())
        start = a.entries[0].offset
        b[start:start + 5] = b"../xx"
        self.archive.write_bytes(b)
        with self.assertRaises(ValueError): verify_psarc(self.archive)
        self.probe(mode="--reject")

    def test_compressed_fixture_and_corrupt_stream(self):
        # Independent fixture writer: PSARC metadata remains identical; physical
        # payloads and table words are rebuilt from zlib-compressed source blocks.
        self.pack()
        a = parse_psarc(self.archive)
        original = self.archive.read_bytes()
        toc = bytearray(original[:a.toc_length]); payload = bytearray(); words = []
        for i, e in enumerate(a.entries):
            raw = original[e.offset:e.offset + e.size]
            block_index = len(words); offset = a.toc_length + len(payload)
            for at in range(0, len(raw), 65536):
                block = raw[at:at + 65536]; compressed = zlib.compress(block)
                stored = compressed if len(compressed) < len(block) else block
                words.append(0 if len(stored) == 65536 else len(stored)); payload.extend(stored)
            toc[32 + i * 30 + 16:32 + i * 30 + 20] = struct.pack(">I", block_index)
            toc[32 + i * 30 + 25:32 + i * 30 + 30] = offset.to_bytes(5, "big")
        # Recompute TOC size because this fixture deliberately doesn't deduplicate.
        new_toc_size = 32 + 30 * len(a.entries) + 2 * len(words)
        delta = new_toc_size - a.toc_length
        toc = toc[:32 + 30 * len(a.entries)]
        struct.pack_into(">I", toc, 12, new_toc_size)
        for i in range(len(a.entries)):
            pos = 32 + i * 30 + 25
            off = int.from_bytes(toc[pos:pos + 5], "big") + delta
            toc[pos:pos + 5] = off.to_bytes(5, "big")
        toc.extend(b"".join(struct.pack(">H", w) for w in words))
        self.archive.write_bytes(toc + payload)
        verify_sources(self.archive, [self.files, self.sys]); self.probe()
        parsed = parse_psarc(self.archive)
        e = parsed.entries[parsed.paths.index("files/common.ini") + 1]
        bad = bytearray(self.archive.read_bytes()); bad[e.offset] = 0
        self.archive.write_bytes(bad)
        with self.assertRaises((ValueError, zlib.error)): verify_psarc(self.archive)
        self.probe(mode="--read-reject")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(); parser.add_argument("--probe", type=Path)
    args, rest = parser.parse_known_args(); PROBE = args.probe
    unittest.main(argv=[__file__, *rest])
