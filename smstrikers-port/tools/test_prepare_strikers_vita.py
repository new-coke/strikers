#!/usr/bin/env python3
"""Synthetic archives/discs only; validate user-facing safety and identity checks."""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from asset_pipeline.native_graphics import PACK_MARKER
from asset_pipeline.psarc import create_psarc
from prepare_strikers_vita import main, verify


class SetupTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.base = Path(self.tmp.name)
        self.iso = self.base / "game.iso"
        header = bytearray(0x440)
        header[:6] = b"G4QP01"
        struct.pack_into(">I", header, 0x1c, 0xc2339f3d)
        self.iso.write_bytes(header)
        self.output = self.base / "sms.psarc"

    def tearDown(self):
        self.tmp.cleanup()

    def pack(self, marker=PACK_MARKER):
        source = self.base / "sys"
        source.mkdir(exist_ok=True)
        (source / "strikers-native.bin").write_bytes(marker)
        create_psarc(self.output, [source])
        self.output.with_suffix(".psarc.json").write_text(json.dumps({
            "archive_sha256": hashlib.sha256(self.output.read_bytes()).hexdigest()}))

    def test_archive_identity_and_marker(self):
        self.pack()
        self.assertEqual(verify(self.output)["verification"]["entries"], 1)
        self.pack(b"SNPACK00")
        with self.assertRaisesRegex(ValueError, "marker"):
            verify(self.output)
        self.pack()
        self.output.with_suffix(".psarc.json").write_text('{"archive_sha256":"stale"}')
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            verify(self.output)

    def test_existing_output_and_input_cannot_be_overwritten(self):
        self.output.write_bytes(b"keep")
        with patch("prepare_strikers_vita.build_tools") as build, contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(main(["--iso", str(self.iso), "--output", str(self.output)]), 1)
            self.assertEqual(main(["--iso", str(self.iso), "--output", str(self.iso), "--replace"]), 1)
            build.assert_not_called()
        self.assertEqual(self.output.read_bytes(), b"keep")
        self.assertEqual(self.iso.stat().st_size, 0x440)

    def test_symlink_output_cannot_overwrite_another_file(self):
        self.output.symlink_to(self.iso)
        with patch("prepare_strikers_vita.build_tools") as build, contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(main(["--iso", str(self.iso), "--output", str(self.output), "--replace"]), 1)
            build.assert_not_called()

    def test_wrong_disc_is_rejected_before_build(self):
        self.iso.write_bytes(b"compressed/another game")
        with patch("prepare_strikers_vita.build_tools") as build, contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(main(["--iso", str(self.iso), "--output", str(self.output)]), 1)
            build.assert_not_called()


if __name__ == "__main__":
    unittest.main()
