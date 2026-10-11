#!/usr/bin/env python3
"""Synthetic host fixtures for comparison integrity; never device FPS evidence."""
import json
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

from vita_resolution_ab import analyze_plan, capture, plan, required_files, sha


class ResolutionComparisonTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        binary = self.root / "fixture.self"
        binary.write_bytes(b"synthetic fixture only")
        self.manifest = plan(self.root, "fixture-host", binary)
        self.path = self.root / "plan.json"
        for run in self.manifest["runs"]:
            folder = self.root / run["label"]
            folder.mkdir()
            (folder / "run-identity.json").write_text(json.dumps({
                "eboot_sha256": self.manifest["self_sha256"], "config": run["config"]}))
            (folder / "strikers.ini").write_text("".join(f"{k}={v}\n" for k, v in run["config"].items()))
            count = int(run["config"]["frame_capture_frames"])
            first = int(run["config"]["frame_capture_skip"])
            duration = 40000 if run["resolution"] == "960x544" else 30000
            (folder / f"{run['label']}.csv").write_text(
                f"# strikers-frame-capture-v1 diagnostics={int(run['visual'])} fps_overlay=0\n"
                "# play_only=1 counter=live_play\n"
                "sample,match,match_frame,frame_us,tasks_us,present_us,sleep_us\n" +
                "".join(f"{i},1,{first+i},{duration},{duration-1000},1000,0\n" for i in range(count)))
            if run["visual"]:
                (folder / f"{run['label']}.log").write_text(
                    f"[aurora-vita] render config display=960x544 internal={run['resolution']} profile=WARM\n")
                (folder / f"debug_frame_play_{run['config']['vita_snapshot_play_frame']}.ppm").write_bytes(
                    b"P6\n1 1\n255\n\x00\x00\x00")
            self.refresh(run)

    def refresh(self, run):
        folder = self.root / run["label"]
        (folder / "capture-identity.json").write_text(json.dumps({
            "eboot_sha256": self.manifest["self_sha256"], "new_coredumps": [],
            "files": {name: {"sha256": sha(folder / name), "bytes": (folder / name).stat().st_size}
                      for name in required_files(run)}}))

    def test_aggregation_and_visual_gate(self):
        result = analyze_plan(self.path)
        self.assertEqual(result["groups"]["640x480"]["samples"], 900)
        self.assertEqual(result["time_change_pct"], -25)
        self.assertFalse(result["eligible_for_performance_decision"])
        self.assertTrue(analyze_plan(self.path, True)["eligible_for_performance_decision"])

    def test_geometry_plan_isolates_only_geometry_budget(self):
        manifest = plan(self.root / "geometry", "fixture-host", experiment="geometry")
        baseline, candidate = manifest["runs"][:2]
        ignored = {"static_geometry_mb", "frame_capture", "log"}
        self.assertEqual({k: v for k, v in baseline["config"].items() if k not in ignored},
                         {k: v for k, v in candidate["config"].items() if k not in ignored})
        self.assertEqual(baseline["config"]["static_geometry_mb"], "8")
        self.assertEqual(candidate["config"]["static_geometry_mb"], "0")
        self.assertEqual(candidate["resolution"], "960x544")

    def test_capture_reads_files_and_rejects_new_dump(self):
        run = self.manifest["runs"][-1]
        folder = self.root / run["label"]
        (self.root / "device-initial.json").write_text(json.dumps({"coredumps": []}))
        state = {"eboot_sha256": self.manifest["self_sha256"], "coredumps": []}
        def read_file(ftp, directory, name):
            return (self.root / "fixture.self" if name == "eboot.bin" else folder / name).read_bytes()
        with patch("vita_resolution_ab.device_state", return_value=state), \
             patch("vita_resolution_ab.device.connect"), \
             patch("vita_resolution_ab.device.read", side_effect=read_file):
            result = capture(self.path, run["label"])
            self.assertEqual(result["new_coredumps"], [])
            self.assertEqual(set(result["files"]), set(required_files(run)))
        with patch("vita_resolution_ab.device_state", return_value=dict(state, coredumps=["new.psp2dmp"])):
            with self.assertRaisesRegex(ValueError, "New coredump"):
                capture(self.path, run["label"])

    def test_unsealed_self_rejected(self):
        self.manifest["self_sha256"] = None
        self.path.write_text(json.dumps(self.manifest))
        with self.assertRaisesRegex(ValueError, "no sealed SELF"):
            analyze_plan(self.path)

    def test_changed_self_and_coredump_rejected(self):
        run = self.manifest["runs"][0]
        file = self.root / run["label"] / "capture-identity.json"
        original = json.loads(file.read_text())
        for field, value, message in (("eboot_sha256", "wrong", "SELF"),
                                      ("new_coredumps", ["new.psp2dmp"], "coredump")):
            file.write_text(json.dumps(dict(original, **{field: value})))
            with self.assertRaisesRegex(ValueError, message):
                analyze_plan(self.path)

    def test_capture_hash_mismatch_rejected(self):
        run = self.manifest["runs"][-1]
        file = self.root / run["label"] / f"{run['label']}.csv"
        file.write_text(file.read_text() + "\n")
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            analyze_plan(self.path)

    def test_wrong_effective_resolution_rejected(self):
        run = self.manifest["runs"][0]
        file = self.root / run["label"] / "strikers.ini"
        file.write_text("gxm_internal_resolution=640x480\n" + file.read_text())
        self.refresh(run)
        with self.assertRaisesRegex(ValueError, "effective INI"):
            analyze_plan(self.path)

    def test_ignored_runtime_resolution_rejected(self):
        run = self.manifest["runs"][1]
        file = self.root / run["label"] / f"{run['label']}.log"
        file.write_text(file.read_text().replace("internal=640x480", "internal=960x544"))
        self.refresh(run)
        with self.assertRaisesRegex(ValueError, "raster extent"):
            analyze_plan(self.path)

    def test_bad_window_incomplete_or_noisy_capture_rejected(self):
        run = self.manifest["runs"][-1]
        file = self.root / run["label"] / f"{run['label']}.csv"
        original = file.read_text()
        bad_window = original.replace(",899,", ",900,")
        for text in (bad_window, "\n".join(original.splitlines()[:-1]) + "\n",
                     original.replace("diagnostics=0", "diagnostics=1")):
            file.write_text(text)
            self.refresh(run)
            with self.assertRaises(ValueError):
                analyze_plan(self.path)


if __name__ == "__main__":
    unittest.main()
