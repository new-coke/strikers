#!/usr/bin/env python3
"""Recompute the audit's small CSV captures; no device or build mutations."""
import csv
import hashlib
import json
import math
import statistics
from pathlib import Path

BASE = Path(__file__).resolve().parent


def summarize(path):
    raw = path.read_bytes()
    lines = raw.decode().splitlines()
    rows = list(csv.DictReader(line for line in lines if line and not line.startswith('#')))
    times = [int(row['frame_us']) for row in rows]
    ordered = sorted(times)
    result = {
        'file': path.name, 'sha256': hashlib.sha256(raw).hexdigest(),
        'metadata': [line for line in lines if line.startswith('#')],
        'samples': len(rows), 'columns': list(rows[0]),
        'fps': len(times) * 1e6 / sum(times),
        'mean_ms': statistics.mean(times) / 1000,
        'median_ms': statistics.median(times) / 1000,
        'p95_ms': ordered[math.ceil(len(times) * .95) - 1] / 1000,
        'p99_ms': ordered[math.ceil(len(times) * .99) - 1] / 1000,
        'min_ms': min(times) / 1000, 'max_ms': max(times) / 1000,
        'deadline_60_pct': 100 * sum(t <= 1e6 / 60 for t in times) / len(times),
        'deadline_30_pct': 100 * sum(t <= 1e6 / 30 for t in times) / len(times),
        'required_time_reduction_pct': 100 * (1 - (1e6 / 60) / statistics.mean(times)),
        'required_speedup': statistics.mean(times) / (1e6 / 60),
    }
    for key in ('match_frame', 'frame'):
        if key in rows[0]:
            result[key + '_first'] = int(rows[0][key])
            result[key + '_last'] = int(rows[-1][key])
    result['numeric_medians'] = {
        key: statistics.median(float(row[key]) for row in rows)
        for key in rows[0] if all(row.get(key, '').replace('.', '', 1).isdigit() for row in rows)
    }
    return result


if __name__ == '__main__':
    summaries = [summarize(path) for path in sorted(BASE.glob('run-*.csv'))]
    output = {'percentiles': 'nearest rank, ceil(n*p)-1', 'runs': summaries}
    (BASE / 'quiet-analysis.json').write_text(json.dumps(output, indent=2) + '\n')
    for run in summaries:
        print(json.dumps({k: v for k, v in run.items() if k not in ('numeric_medians', 'columns')}, indent=2))
