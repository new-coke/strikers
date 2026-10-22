"""MP4 conversion contracts; synthetic THP audio checked against the game decoder."""
import ctypes
from fractions import Fraction
from pathlib import Path
import random
import struct
import subprocess
import tempfile
import unittest

from asset_pipeline.mp4_video import decode_thp_audio, validate_probe, DESCRIPTOR, MAGIC


class Mp4Tests(unittest.TestCase):
    def probe(self):
        return {"streams": [
            {"codec_type": "video", "codec_name": "h264", "pix_fmt": "yuvj420p",
             "profile": "Constrained Baseline", "width": 64, "height": 64,
             "nb_read_frames": "30", "avg_frame_rate": "30/1", "color_range": "pc",
             "has_b_frames": 0, "duration": "1.0"},
            {"codec_type": "audio", "codec_name": "aac", "profile": "LC",
             "sample_rate": "48000", "channels": 2, "duration": "1.0"}]}

    def test_descriptor_is_fixed_little_endian(self):
        self.assertEqual(DESCRIPTOR.size, 32)
        self.assertEqual(DESCRIPTOR.pack(MAGIC, 512, 416, 1949, 2997, 100, 1)[8:12], b"\0\2\0\0")

    def test_codec_count_rate_range_and_duration(self):
        info = dict(count=2, width=64, height=64, frames=30)
        validate_probe(self.probe(), info, Fraction(30))
        for key, value in (("width", 128), ("nb_read_frames", "29"), ("avg_frame_rate", "2997/100"),
                           ("profile", "High"), ("color_range", "tv"), ("has_b_frames", 1), ("duration", "2.0")):
            probe = self.probe()
            probe["streams"][0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                validate_probe(probe, info, Fraction(30))
        for key, value in (("sample_rate", "32000"), ("channels", 1), ("duration", "1.2")):
            probe = self.probe()
            probe["streams"][1][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                validate_probe(probe, info, Fraction(30))

    def test_no_audio_clip(self):
        probe = self.probe()
        probe["streams"].pop()
        validate_probe(probe, dict(count=1, width=64, height=64, frames=30), Fraction(30))

    def test_audio_rejects_unbounded_components(self):
        for record in (bytes(79), bytes(80), struct.pack(">II", 0, 100000) + bytes(80),
                       struct.pack(">II", 4, 14) + bytes(88)):
            with self.assertRaises(ValueError):
                decode_thp_audio(record)

    def test_audio_matches_port_64bit_decoder(self):
        root = Path(__file__).resolve().parents[1]
        source = (root / "src/platform/thp.c").read_text()
        # Compile the unchanged production audio decoder as an independent oracle.
        source = source[source.index("static u32 rd_be32"):]
        prelude = "#include <stdint.h>\n#include <stddef.h>\ntypedef uint8_t u8; typedef uint32_t u32; typedef int16_t s16; typedef int32_t s32;\n"
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            (base / "audio.c").write_text(prelude + source)
            library = base / "audio.so"
            subprocess.run(["clang", "-shared", "-fPIC", "-O2", str(base / "audio.c"), "-o", str(library)], check=True)
            decode = ctypes.CDLL(str(library)).THPAudioDecode
            decode.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
            decode.restype = ctypes.c_uint
            rng = random.Random(124)
            for mono in (False, True):
                for count in (1, 14, 15, 1068):
                    length = ((count + 13) // 14) * 8
                    record = bytearray(80 + length * (1 if mono else 2))
                    struct.pack_into(">II", record, 0, 0 if mono else length, count)
                    struct.pack_into(">32h", record, 8, *(rng.randrange(-32768, 32768) for _ in range(32)))
                    struct.pack_into(">4h", record, 72, -32768, 32767, 12345, -30000)
                    record[80:] = bytes(rng.randrange(256) for _ in record[80:])
                    expected = (ctypes.c_int16 * (count * 2))()
                    raw = ctypes.create_string_buffer(bytes(record))
                    self.assertEqual(decode(expected, raw, 0), count)
                    pcm, samples = decode_thp_audio(record)
                    self.assertEqual(samples, count)
                    self.assertEqual(struct.unpack("<" + "h" * count * 2, pcm), tuple(expected))


if __name__ == "__main__":
    unittest.main()
