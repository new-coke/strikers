#!/usr/bin/env python3
"""Join quiet finite frame samples with original game presentation identifiers."""
from __future__ import annotations
import argparse
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import statistics


def read(path):
    data = Path(path).read_bytes()
    text = data.decode("utf-8")
    metadata = [line for line in text.splitlines() if line.startswith("#")]
    rows = list(csv.DictReader(io.StringIO("\n".join(
        line for line in text.splitlines() if line and not line.startswith("#")))))
    if not rows or any(None in row or None in row.values() for row in rows):
        raise ValueError("empty or malformed capture")
    return rows, metadata, hashlib.sha256(data).hexdigest()


def distribution(rows):
    values = sorted(int(r["frame_us"]) for r in rows)
    return {"frames": len(values), "mean_fps": 1e6 / statistics.mean(values),
            "median_us": statistics.median(values),
            "p95_us": values[math.ceil(.95 * len(values)) - 1],
            "p99_us": values[math.ceil(.99 * len(values)) - 1], "worst_us": values[-1],
            "within_30fps_percent": 100 * sum(v <= 1e6 / 30 for v in values) / len(values),
            "within_60fps_percent": 100 * sum(v <= 1e6 / 60 for v in values) / len(values)}


def analyze(path):
    path = Path(path)
    frames, metadata, digest = read(path)
    scenes, scene_metadata, scene_digest = read(str(path) + ".scenes.csv")
    if "# strikers-frame-capture-v1 diagnostics=0 fps_overlay=0" not in metadata:
        raise ValueError("quiet capture required: diagnostics=0 fps_overlay=0")
    if "# strikers-scenes-v1 state_sampled=before_tasks" not in scene_metadata:
        raise ValueError("missing original-state scene capture metadata")
    if len(frames) != len(scenes):
        raise ValueError("frame and scene row counts differ")
    required = {"sample", "match_frame", "scene", "task_state", "previous_state",
                "scene_serial", "scene_frame", "presentation"}
    groups, segments = {}, {}
    prior_serial, prior_scene_frame = -1, -1
    for index, (frame, scene) in enumerate(zip(frames, scenes)):
        if set(scene) != required:
            raise ValueError("unexpected scene columns")
        if int(frame["sample"]) != index or int(scene["sample"]) != index or frame["match_frame"] != scene["match_frame"]:
            raise ValueError("frame and scene sample identities differ")
        if scene["scene"] not in ("unknown", "intro", "goal", "replay", "trophy", "presentation", "play", "menu"):
            raise ValueError("unknown scene type")
        if int(frame["frame_us"]) <= 0:
            raise ValueError("invalid frame duration")
        serial, number = int(scene["scene_serial"]), int(scene["scene_frame"])
        if serial < prior_serial or (serial == prior_serial and number != prior_scene_frame + 1):
            raise ValueError("scene frame sequence is not contiguous")
        prior_serial, prior_scene_frame = serial, number
        key = scene["scene"] + "/" + scene["presentation"]
        groups.setdefault(key, []).append(frame)
        segment = segments.setdefault(serial, {"scene": scene["scene"], "presentation": scene["presentation"],
                                               "task_state": int(scene["task_state"]), "rows": []})
        if (segment["scene"], segment["presentation"], segment["task_state"]) != (
                scene["scene"], scene["presentation"], int(scene["task_state"])):
            raise ValueError("scene identity changed within a segment")
        segment["rows"].append(frame)
    return {"path": str(path.resolve()), "sha256": digest, "scenes_sha256": scene_digest,
            "metadata": metadata, "overall": distribution(frames),
            "groups": {key: distribution(rows) for key, rows in groups.items()},
            "segments": [{"serial": serial, **{k: v for k, v in segment.items() if k != "rows"},
                          **distribution(segment["rows"])} for serial, segment in segments.items()],
            "limits": "Finite captures disable render skipping. Logical-loop timings require visual confirmation of newly presented images; short or incomplete scenes and aggregate means do not prove stable 30 FPS."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    text = json.dumps(analyze(args.path), indent=2) + "\n"
    if args.out:
        args.out.write_text(text)
    print(text, end="")
