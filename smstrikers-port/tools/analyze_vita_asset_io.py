#!/usr/bin/env python3
"""Validate a finite archive I/O capture and distinguish startup from live deltas."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re

FIELDS = ["live_frame", "requests", "requested_bytes", "physical_reads", "physical_bytes",
          "read_us", "read_errors", "inflates", "inflated_bytes", "inflate_us", "inflate_errors"]
COUNTERS = FIELDS[1:]


def analyze(path, expected=None, window_first=600, window_last=899):
    path = Path(path)
    content = path.read_bytes()
    lines = content.decode("utf-8").splitlines()
    metadata = [line for line in lines if line.startswith("#")]
    marker = re.compile(r"# strikers-asset-io-v1 cumulative=1 live_play=1 warmup=(\d+) samples=(\d+);")
    headers = [marker.match(line) for line in metadata if marker.match(line)]
    if len(headers) != 1:
        raise ValueError("Missing or ambiguous live-play cumulative capture identity")
    warmup, declared = map(int, headers[0].groups())
    if declared < 2 or declared > 3600 or (expected is not None and declared != expected):
        raise ValueError("Unexpected sample count")
    reader = csv.DictReader(line for line in lines if line and not line.startswith("#"))
    if reader.fieldnames != FIELDS:
        raise ValueError("Unexpected archive I/O columns")
    rows = []
    for raw in reader:
        if set(raw) != set(FIELDS) or any(raw[key] is None for key in FIELDS):
            raise ValueError("Malformed archive I/O row")
        try:
            row = {key: int(raw[key]) for key in FIELDS}
        except ValueError as error:
            raise ValueError("Non-integer archive I/O value") from error
        if any(value < 0 for value in row.values()) or row["live_frame"] != warmup + len(rows):
            raise ValueError("Invalid live-play sequence or counter")
        if rows and any(row[key] < rows[-1][key] for key in COUNTERS):
            raise ValueError("Cumulative archive I/O counter decreased")
        rows.append(row)
    if len(rows) != declared:
        raise ValueError("Incomplete archive I/O capture")
    if not warmup <= window_first < window_last <= rows[-1]["live_frame"]:
        raise ValueError("Requested live-play window is absent")

    def window(first, last):
        selected = rows[first - warmup:last - warmup + 1]
        intervals = len(selected) - 1
        delta = {key: selected[-1][key] - selected[0][key] for key in COUNTERS}
        differences = [{key: right[key] - left[key] for key in COUNTERS}
                       for left, right in zip(selected, selected[1:])]
        return {
            "first_live_frame": first, "last_live_frame": last,
            "samples": len(selected), "intervals": intervals, "delta": delta,
            "intervals_with_physical_reads": sum(row["physical_reads"] > 0 for row in differences),
            "read_api_ms_per_interval": delta["read_us"] / (1000 * intervals),
            "max_read_api_ms_in_interval": max(row["read_us"] for row in differences) / 1000,
            "inflate_ms_per_interval": delta["inflate_us"] / (1000 * intervals),
            "max_inflate_ms_in_interval": max(row["inflate_us"] for row in differences) / 1000,
        }

    return {
        "path": str(path.resolve()), "sha256": hashlib.sha256(content).hexdigest(),
        "metadata": metadata, "samples": declared,
        "observations_active": rows[-1]["physical_reads"] > 0 and rows[-1]["requests"] > 0,
        "first_snapshot_cumulative": rows[0], "last_snapshot_cumulative": rows[-1],
        "entire_live_play": window(warmup, rows[-1]["live_frame"]),
        "steady_play": window(window_first, window_last),
        "scope": "PSARC reads across game/native archive handles; excludes shader filesystem, saves and other file I/O",
        "timing_scope": "CPU-observed API/inflate elapsed time; simultaneous threads overlap; not storage utilization or a summed frame CPU budget",
        "first_snapshot_scope": "Includes archive initialization and all activity before first captured live-play snapshot",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path)
    parser.add_argument("--expected", type=int, default=900)
    parser.add_argument("--window-first", type=int, default=600)
    parser.add_argument("--window-last", type=int, default=899)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    result = analyze(args.path, args.expected, args.window_first, args.window_last)
    output = json.dumps(result, indent=2) + "\n"
    if args.out:
        args.out.write_text(output)
    print(output)
