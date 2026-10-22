#!/usr/bin/env python3
"""Synthetic archives/discs only; validate user-facing safety and identity checks."""
import copy
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
from asset_pipeline.mp4_video import DESCRIPTOR, MAGIC
from asset_pipeline.psarc import create_psarc, parse_psarc
from prepare_strikers_vita import main, verify, media_summary


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
        with patch("prepare_strikers_vita.media_summary", return_value={"video_format": "mp4"}):
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


class SetupTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.native = self.root / "native"
        (self.native / "v1").mkdir(parents=True)
        (self.native / "v1/audio-index.bin").write_bytes(b"test audio index")
        movie = self.native / "v2/video/intro.mp4"
        movie.parent.mkdir(parents=True)
        movie.write_bytes(b"test MP4")
        self.meta = movie.with_suffix(".mp4.meta")
        self.meta.write_bytes(DESCRIPTOR.pack(MAGIC, 512, 416, 60, 2997, 100, 1))
        self.report = {"native_media": {"audio": {"mode": "all", "records": 7},
            "video": {"format": "MP4 H.264 Baseline/AAC-LC + STMP4001 descriptor", "videos": [
                {"mp4_path": "native/v2/video/intro.mp4", "width": 512, "height": 416,
                 "frames": 60, "fps": 29.969999313354492, "count": 2}]}}}
        self.archive = self.root / "sms.psarc"

    def check(self, report=None):
        create_psarc(self.archive, [self.native])
        return media_summary(self.archive, parse_psarc(self.archive), report or self.report)

    def test_complete_mp4_pack_selects_correct_consumer(self):
        self.assertEqual(self.check(), {"audio_mode": "all", "audio_records": 7,
            "video_format": "mp4", "movies": 1, "video_player": "avplayer"})

    def test_reject_missing_descriptor_and_incorrect_geometry(self):
        self.meta.unlink()
        with self.assertRaisesRegex(ValueError, "Missing MP4 or descriptor"): self.check()
        self.meta.write_bytes(DESCRIPTOR.pack(MAGIC, 640, 416, 60, 2997, 100, 1))
        with self.assertRaisesRegex(ValueError, "descriptor differs"): self.check()

    def test_reject_incomplete_audio_and_missing_movies(self):
        report = copy.deepcopy(self.report)
        report["native_media"]["audio"]["mode"] = "bank"
        with self.assertRaisesRegex(ValueError, "Incomplete Vita audio"): self.check(report)
        report = copy.deepcopy(self.report)
        report["native_media"]["video"]["videos"] = []
        with self.assertRaisesRegex(ValueError, "Missing native movies"): self.check(report)
        (self.native / "v1/audio-index.bin").unlink()
        with self.assertRaisesRegex(ValueError, "Incomplete Vita audio"): self.check()

    def test_reject_thp_left_in_mp4_pack(self):
        (self.native / "unconverted.thp").write_bytes(b"test THP")
        with self.assertRaisesRegex(ValueError, "original THP"): self.check()


if __name__ == "__main__":
    unittest.main()
