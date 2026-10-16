#!/usr/bin/env python3
"""Validate and summarize lossless, opt-in view/draw diagnostics (NOT GPU time).

The JSONL format is documented in docs/report_performance/.../IMPLEMENTATION.md.
It intentionally cannot turn a partial trace or a diagnostic finish wait into
an FPS or per-fragment cost claim.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import re

SCHEMA = "aurora-vita-view-draw-v2"
HEX64 = re.compile(r"^[0-9a-fA-F]{16}$")
SHA256 = re.compile(r"^[0-9a-fA-F]{64}$")
VIEWS = {3: "Shadowed", 11: "Characters"}
KIND = {"view", "draw", "frame_complete"}


def _int(obj: dict, key: str, *, minimum: int = 0) -> int:
    value = obj.get(key)
    if type(value) is not int or value < minimum:
        raise ValueError(f"Invalid {key}: expected integer >= {minimum}")
    return value


def _hash(obj: dict, key: str, expr: re.Pattern[str]) -> str:
    value = obj.get(key)
    if not isinstance(value, str) or not expr.fullmatch(value):
        raise ValueError(f"Invalid or missing {key}")
    return value.lower()


def _object(text: str, line: int) -> dict:
    try:
        value = json.loads(text)
    except (ValueError, UnicodeError) as exc:
        raise ValueError(f"Malformed JSON on line {line}") from exc
    if not isinstance(value, dict):
        raise ValueError(f"Expected object on line {line}")
    return value


def analyze(path: Path) -> dict:
    path = Path(path)
    data = path.read_bytes()
    if not data or data[-1:] != b"\n":
        raise ValueError("Truncated capture: missing terminal newline")
    try:
        lines = data.decode("utf-8", errors="strict").splitlines()
    except UnicodeError as exc:
        raise ValueError("Capture is not UTF-8") from exc
    if len(lines) < 3:
        raise ValueError("Missing header, events or footer")
    if any(not line.strip() for line in lines):
        raise ValueError("Empty capture record")

    header = _object(lines[0], 1)
    if header.get("type") != "header" or header.get("schema") != SCHEMA:
        raise ValueError("Unknown diagnostic view/draw schema")
    ident = {name: _hash(header, name, SHA256)
             for name in ("self_sha256", "ini_sha256", "shader_cache_sha256")}
    capture_id = header.get("capture_id")
    if not isinstance(capture_id, str) or not (1 <= len(capture_id) <= 128):
        raise ValueError("Missing capture_id")
    if _int(header, "capacity", minimum=1) > 1_000_000:
        raise ValueError("Unbounded capture capacity")

    footer = _object(lines[-1], len(lines))
    if footer.get("type") != "footer":
        raise ValueError("Missing complete footer")
    if _int(footer, "dropped") or footer.get("truncated") is not False:
        raise ValueError("Partial capture: records lost or truncated")
    expected = _int(footer, "records")
    if expected != len(lines) - 2 or expected > header["capacity"]:
        raise ValueError("Capture event count mismatch")
    if _int(footer, "last_sequence") != expected:
        raise ValueError("Capture sequence did not reach footer")

    active_view = None
    active_frame = None
    active_consumer_frame = None
    next_logical_draw = 1
    last_completed_consumer_frame = 0
    completed = set()
    draws = defaultdict(lambda: {"draws": 0, "vertices": 0, "indices": 0, "submit_cpu_us": 0})
    shader_usage = defaultdict(lambda: {"draws": 0, "vertices": 0, "submit_cpu_us": 0})
    outside = 0
    payload_mode = None
    for n, line in enumerate(lines[1:-1], start=1):
        event = _object(line, n + 1)
        if event.get("type") not in KIND:
            raise ValueError(f"Invalid event type at sequence {n}")
        if _int(event, "sequence", minimum=1) != n:
            raise ValueError("Noncontiguous event sequence")
        frame = _int(event, "producer_frame", minimum=1)
        if active_frame is not None and frame < active_frame:
            raise ValueError("Producer frame went backwards")
        if frame in completed:
            raise ValueError("Records after completed frame")
        if frame != active_frame:
            if active_frame is not None and active_frame not in completed:
                raise ValueError("Previous frame not completed")
            active_frame, active_view = frame, None
            active_consumer_frame = None
            next_logical_draw = 1

        if event["type"] == "view":
            view = _int(event, "view")
            if view > 33 and view != 255:
                raise ValueError("View index out of range")
            active_view = view
        elif event["type"] == "frame_complete":
            consumer = _int(event, "consumer_frame", minimum=1)
            if active_consumer_frame is not None and consumer != active_consumer_frame:
                raise ValueError("Consumer frame changed during producer frame")
            if consumer <= last_completed_consumer_frame:
                raise ValueError("Consumer completion did not advance")
            last_completed_consumer_frame = consumer
            completed.add(frame)
            active_view = None
        else:
            if active_view is None or _int(event, "view") != active_view:
                raise ValueError("Draw has no matching ordered view marker")
            consumer = _int(event, "consumer_frame", minimum=1)
            if active_consumer_frame is None:
                active_consumer_frame = consumer
            elif consumer != active_consumer_frame:
                raise ValueError("Inconsistent consumer frame in draw sequence")
            if _int(event, "logical_draw", minimum=1) != next_logical_draw:
                raise ValueError("Noncontiguous logical draw number")
            next_logical_draw += 1
            for key in ("target", "tev_stages", "texture_mask", "texgen_mask", "vertex_count",
                        "index_count", "submit_cpu_us", "uniform_bytes", "program_native",
                        "indexed_pn", "blend", "depth", "alpha", "alpha_ref0", "alpha_ref1",
                        "alpha_op", "scissor"):
                _int(event, key)
            if event["tev_stages"] > 16 or event["texture_mask"] > 255:
                raise ValueError("Out of bounds shader configuration")
            if event["program_native"] > 1 or event["indexed_pn"] > 1:
                raise ValueError("Invalid program path")
            if event["alpha_ref0"] > 255 or event["alpha_ref1"] > 255 or event["alpha_op"] > 3:
                raise ValueError("Invalid alpha compare")
            for key in ("pipeline_requested", "pipeline_active", "vertex_hash", "fragment_hash"):
                _hash(event, key, HEX64)
            present = _int(event, "payload_hashes_present")
            if present not in (0, 1):
                raise ValueError("Invalid payload hash mode")
            if payload_mode is None:
                payload_mode = present
            elif payload_mode != present:
                raise ValueError("Inconsistent payload hash coverage")
            for key in ("vertex_payload_hash", "index_payload_hash",
                        "uniform_payload_hash", "draw_state_hash"):
                actual = _hash(event, key, HEX64)
                if not present and actual != "0000000000000000":
                    raise ValueError("Payload hash present in disabled capture")
                if present and actual == "0000000000000000":
                    raise ValueError("Missing enabled payload hash")
            key = VIEWS.get(active_view, "Outside" if active_view == 255 else f"View{active_view}")
            if active_view == 255:
                outside += 1
            record = draws[key]
            record["draws"] += 1
            record["vertices"] += event["vertex_count"]
            record["indices"] += event["index_count"]
            record["submit_cpu_us"] += event["submit_cpu_us"]
            shader = shader_usage[(key, event["fragment_hash"].lower())]
            shader["draws"] += 1
            shader["vertices"] += event["vertex_count"]
            shader["submit_cpu_us"] += event["submit_cpu_us"]
    if active_frame not in completed:
        raise ValueError("Last frame is incomplete")
    if not shader_usage:
        raise ValueError("Capture contains no GXM draw submissions")

    top = sorted(({"view": view, "fragment_hash": fragment, **counts}
                  for (view, fragment), counts in shader_usage.items()),
                 key=lambda item: (-item["draws"], -item["vertices"], item["view"], item["fragment_hash"]))
    return {
        "schema": SCHEMA,
        "capture_id": capture_id,
        "sha256": hashlib.sha256(data).hexdigest(),
        "identities": ident,
        "complete": True,
        "event_records": expected,
        "completed_frames": len(completed),
        "draws_outside_named_views": outside,
        "payload_hashes_complete": bool(payload_mode),
        "views": dict(sorted(draws.items())),
        "top_fragment_programs": top,
        "limits": "CPU submit scopes are not GPU execution times; sums across async threads are invalid. Diagnostic capture is not quiet FPS.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    report = json.dumps(analyze(args.path), indent=2, sort_keys=True) + "\n"
    if args.out:
        args.out.write_text(report)
    print(report)
