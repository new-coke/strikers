#!/usr/bin/env python3
import csv
from pathlib import Path
import tempfile
import unittest

from analyze_vita_asset_io import FIELDS, analyze


class ArchiveIoTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name) / "io.csv"
        self.rows = [[i, 100 + i, 10000 + 10 * i, 20 + i, 1000 + 5 * i,
                      500 + 200 * i, 0, 2 + i, 65536 * (2 + i), 100 + 300 * i, 0]
                     for i in range(4)]

    def tearDown(self):
        self.temp.cleanup()

    def write(self, header="# strikers-asset-io-v1 cumulative=1 live_play=1 warmup=0 samples=4;"):
        with self.path.open("w") as file:
            file.write(header + "\n")
            writer = csv.writer(file)
            writer.writerow(FIELDS)
            writer.writerows(self.rows)

    def test_cumulative_baseline_is_not_per_frame_work(self):
        self.write()
        result = analyze(self.path, 4, 1, 3)
        self.assertTrue(result["observations_active"])
        self.assertEqual(result["steady_play"]["samples"], 3)
        self.assertEqual(result["steady_play"]["intervals"], 2)
        self.assertEqual(result["steady_play"]["delta"]["physical_reads"], 2)
        self.assertEqual(result["steady_play"]["read_api_ms_per_interval"], .2)
        self.assertEqual(result["steady_play"]["inflate_ms_per_interval"], .3)
        self.assertEqual(result["entire_live_play"]["delta"]["physical_bytes"], 15)

    def test_zero_live_io_with_nonzero_startup(self):
        self.rows = [[i] + self.rows[0][1:] for i in range(4)]
        self.write()
        result = analyze(self.path, 4, 1, 3)
        self.assertTrue(result["observations_active"])
        self.assertEqual(result["steady_play"]["intervals_with_physical_reads"], 0)
        self.assertEqual(result["steady_play"]["delta"]["physical_reads"], 0)

    def test_rejects_truncation(self):
        self.rows.pop()
        self.write()
        with self.assertRaisesRegex(ValueError, "Incomplete"):
            analyze(self.path, 4, 1, 3)

    def test_rejects_counter_reset(self):
        self.rows[2][3] = 0
        self.write()
        with self.assertRaisesRegex(ValueError, "decreased"):
            analyze(self.path, 4, 1, 3)

    def test_rejects_frame_gap(self):
        self.rows[2][0] = 3
        self.write()
        with self.assertRaisesRegex(ValueError, "sequence"):
            analyze(self.path, 4, 1, 3)

    def test_rejects_missing_live_play_identity(self):
        self.write("# missing identity")
        with self.assertRaisesRegex(ValueError, "identity"):
            analyze(self.path, 4, 1, 3)

    def test_rejects_uncaptured_window(self):
        self.write()
        with self.assertRaisesRegex(ValueError, "absent"):
            analyze(self.path, 4, 2, 4)


if __name__ == "__main__":
    unittest.main()
