import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from analyze_cutscenes import analyze

DRIVER = None


class CutsceneTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "frames.csv"
        self.path.write_text("# strikers-frame-capture-v1 diagnostics=0 fps_overlay=0\n"
            "sample,match,match_frame,frame_us,tasks_us,present_us,sleep_us\n"
            "0,1,0,30000,20000,10000,0\n1,1,1,40000,20000,20000,0\n")
        self.scenes = Path(str(self.path) + ".scenes.csv")
        self.scenes.write_text("# strikers-scenes-v1 state_sampled=before_tasks\n"
            "sample,match_frame,scene,task_state,previous_state,scene_serial,scene_frame,presentation\n"
            "0,0,goal,256,2,1,0,GoalCaptainCelebration\n1,1,goal,256,2,1,1,GoalCaptainCelebration\n")

    def test_distribution_exposes_deadline_misses(self):
        result = analyze(self.path)
        self.assertEqual(result["overall"]["within_30fps_percent"], 50)
        self.assertEqual(result["overall"]["p95_us"], 40000)
        self.assertAlmostEqual(result["overall"]["mean_fps"], 28.5714285714)

    def test_reject_instrumented_or_misaligned_capture(self):
        original = self.path.read_text()
        self.path.write_text(original.replace("diagnostics=0", "diagnostics=1"))
        with self.assertRaises(ValueError): analyze(self.path)
        self.path.write_text(original)
        self.scenes.write_text(self.scenes.read_text().replace("1,1,goal", "1,7,goal"))
        with self.assertRaises(ValueError): analyze(self.path)

    def test_reject_scene_gaps_or_mixed_identity(self):
        original = self.scenes.read_text()
        for damaged in (original.replace("256,2,1,1", "256,2,1,5"),
                        original.replace("1,1,goal", "1,1,replay")):
            self.scenes.write_text(damaged)
            with self.assertRaises(ValueError): analyze(self.path)

    def test_real_capture_driver(self):
        if DRIVER is None: self.skipTest("capture driver argument required")
        subprocess.run([str(DRIVER), str(self.path)], check=True)
        result = analyze(self.path)
        self.assertEqual(result["overall"]["frames"], 8)
        self.assertEqual([r["scene"] for r in result["segments"]], ["intro", "goal", "replay", "trophy"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--driver", type=Path)
    args, rest = parser.parse_known_args()
    DRIVER = args.driver.resolve() if args.driver else None
    unittest.main(argv=[__file__, *rest])
