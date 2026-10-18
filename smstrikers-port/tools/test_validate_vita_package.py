#!/usr/bin/env python3
"""Exercise packaging failures that otherwise produce a stale/broken LiveArea."""
from pathlib import Path
import shutil
import struct
import tempfile
import unittest
import zipfile
import zlib

from validate_vita_package import IMAGES, TEMPLATE, validate


class PackageTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.base = Path(self.tmp.name)
        self.source = self.base / "sce_sys"
        shutil.copytree(Path(__file__).resolve().parents[1] / "assets/vita/sce_sys", self.source)
        self.vpk = self.base / "test.vpk"
        self.executable = self.base / "test.self"
        self.executable.write_bytes(b"synthetic SELF")

    def tearDown(self):
        self.tmp.cleanup()

    def package(self, override=None, omit=None):
        with zipfile.ZipFile(self.vpk, "w") as archive:
            for name in [*IMAGES, TEMPLATE]:
                if name != omit:
                    archive.writestr("sce_sys/" + name, (override or {}).get(name, (self.source / name).read_bytes()))
            archive.writestr("eboot.bin", self.executable.read_bytes())
            archive.writestr("sce_sys/param.sfo", b"synthetic SFO")

    def test_real_source_and_exact_package(self):
        self.package()
        result = validate(self.source, self.vpk, self.executable)
        self.assertEqual(result["assets"]["livearea/contents/startup.png"]["colors"], 128)
        self.assertEqual(result["content_rev"], "3")

    def test_missing_or_stale_template_and_missing_image(self):
        for name in (TEMPLATE, "icon0.png"):
            with self.subTest(omit=name):
                self.package(omit=name)
                with self.assertRaises((KeyError, ValueError)):
                    validate(self.source, self.vpk)
        self.package({TEMPLATE: b"<livearea/>"})
        with self.assertRaisesRegex(ValueError, "stale"):
            validate(self.source, self.vpk)

    def test_wrong_self(self):
        self.package()
        self.executable.write_bytes(b"different build")
        with self.assertRaisesRegex(ValueError, "SELF"):
            validate(self.source, self.vpk, self.executable)

    def test_xml_encoding_and_role_references(self):
        template = self.source / TEMPLATE
        good = template.read_bytes()
        for invalid in (good.replace(b"\r\n", b"\n"), good.replace(b"startup.png", b"missing.png")):
            template.write_bytes(invalid)
            with self.assertRaises(ValueError):
                validate(self.source)

    def test_corrupt_png_rejected(self):
        image = self.source / "icon0.png"
        data = bytearray(image.read_bytes())
        data[30] ^= 1
        image.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "CRC"):
            validate(self.source)

    def test_rgba_png_header_rejected_even_with_correct_crc(self):
        image = self.source / "livearea/contents/startup.png"
        data = bytearray(image.read_bytes())
        data[25] = 6  # RGBA is a valid PNG type, but not a shell indexed PNG.
        struct.pack_into(">I", data, 29, zlib.crc32(data[12:29]))
        image.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "indexed PNG-8"):
            validate(self.source)


if __name__ == "__main__":
    unittest.main()
