#!/usr/bin/env python3
"""Validate task attribution against the same finite frame and packet captures."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics
from analyze_packet_profile import analyze as analyze_packets, read as read_integers

METADATA = '# strikers-task-profile-v1 attribution_only=1 task_scopes_are_subsets=1'
FIELDS = ('sample', 'live_frame', 'engine_frame', 'record', 'task_id', 'task_name', 'task_count',
          'manager_ns', 'run_ns', 'transition_ns', 'run_calls', 'transition_calls')
IDENTITY = ('sample', 'live_frame', 'engine_frame')
STATS = ('run_ns', 'transition_ns', 'run_calls', 'transition_calls')
PHASE_METADATA = '# strikers-task-phases-v1 attribution_only=1 disjoint_children_of_run=1 simulation_units=seconds'
PHASE_FIELDS = ('sample', 'live_frame', 'engine_frame', 'record', 'parent_task_id', 'phase_id',
                'phase_name', 'phase_count', 'simulation_count', 'elapsed_ns', 'calls', 'steps',
                'scaled_dt_seconds', 'debt_before_seconds', 'debt_after_seconds', 'tick_seconds', 'lockstep')
SECONDS = ('scaled_dt_seconds', 'debt_before_seconds', 'debt_after_seconds', 'tick_seconds')
PHASE_NAMES = ('Fixed control', 'Fixed input', 'Game AI', 'NPC AI', 'Pre-physics', 'Physics',
               'Post-physics', 'Animated nets', 'Sideline', 'Replay capture', 'Object lighting',
               'Presentation', 'Render preparation', 'World render', 'Snapshot render',
               'Inventory/indicators', 'Post-effects')
FIXED_PHASES = 10


def read_tasks(path):
    frames, names = [], {}
    try:
        # Skip only the metadata line. CSV task names may contain commas,
        # quotes, CR/LF and a '#' at the start of a quoted continuation line.
        with Path(path).open(newline='') as file:
            if file.readline().rstrip('\r\n') != METADATA:
                raise ValueError('Missing task profile metadata')
            reader = csv.DictReader(file, strict=True)
            if reader.fieldnames != list(FIELDS):
                raise ValueError('Unexpected task profile columns')
            for raw in reader:
                if set(raw) != set(FIELDS) or any(value is None for value in raw.values()):
                    raise ValueError('Malformed task profile row')
                row = {key: raw[key] if key in ('record', 'task_name') else int(raw[key]) for key in FIELDS}
                if any(value < 0 for key, value in row.items() if key not in ('record', 'task_name')):
                    raise ValueError('Negative task profile value')
                if (row['sample'] >= 8192 or row['engine_frame'] > 0xffffffff or
                        row['task_count'] > 32 or row['run_calls'] > 0xffffffff or
                        row['transition_calls'] > 0xffffffff or
                        any(row[key] > 0xffffffffffffffff for key in ('manager_ns', 'run_ns', 'transition_ns'))):
                    raise ValueError('Task profile exceeds bounded counters')
                if row['record'] == 'frame':
                    if (row['sample'] != len(frames) or row['task_id'] or row['task_name'] or
                            any(row[key] for key in STATS)):
                        raise ValueError('Invalid task frame sequence or manager row')
                    row['tasks'] = {}
                    frames.append(row)
                elif row['record'] == 'task':
                    if not frames or any(row[key] != frames[-1][key] for key in IDENTITY):
                        raise ValueError('Task/frame identities differ')
                    task_id, name = row['task_id'], row['task_name']
                    if (task_id >= 32 or row['manager_ns'] or row['task_count'] or
                            task_id in frames[-1]['tasks'] or not (row['run_calls'] or row['transition_calls']) or
                            (not row['run_calls'] and row['run_ns']) or
                            (not row['transition_calls'] and row['transition_ns']) or
                            len(name.encode('utf-8')) >= 96 or '\0' in name):
                        raise ValueError('Invalid or duplicate task record')
                    if task_id not in names:
                        if task_id != len(names):
                            raise ValueError('Nonconsecutive task dictionary')
                        names[task_id] = name
                    elif names[task_id] != name:
                        raise ValueError('Task dictionary identity changed')
                    frames[-1]['tasks'][task_id] = row
                else:
                    raise ValueError('Unknown task record type')
    except (csv.Error, TypeError) as exc:
        raise ValueError('Malformed task profile CSV') from exc
    if not frames:
        raise ValueError('Incomplete task profile')
    for frame in frames:
        if len(frame['tasks']) != frame['task_count']:
            raise ValueError('Incomplete task records for a frame')
        frame['attributed_ns'] = sum(task['run_ns'] + task['transition_ns'] for task in frame['tasks'].values())
        if frame['attributed_ns'] > frame['manager_ns']:
            raise ValueError('Task scopes exceed manager wall time')
    return frames, names


def distribution(values):
    ordered = sorted(values)
    return {'mean_ms': statistics.mean(values) / 1e6, 'median_ms': statistics.median(values) / 1e6,
            'p95_ms': ordered[math.ceil(len(ordered) * .95) - 1] / 1e6, 'max_ms': max(values) / 1e6}


def read_phases(path, tasks):
    frames = []
    try:
        with Path(path).open(newline='') as file:
            if file.readline().rstrip('\r\n') != PHASE_METADATA:
                raise ValueError('Missing phase profile metadata')
            reader = csv.DictReader(file, strict=True)
            if reader.fieldnames != list(PHASE_FIELDS):
                raise ValueError('Unexpected phase profile columns')
            for raw in reader:
                if set(raw) != set(PHASE_FIELDS) or any(value is None for value in raw.values()):
                    raise ValueError('Malformed phase profile row')
                row = {key: raw[key] if key in ('record', 'phase_name') else
                       float(raw[key]) if key in SECONDS else int(raw[key]) for key in PHASE_FIELDS}
                if (any(value < 0 for key, value in row.items() if key not in ('record', 'phase_name', *SECONDS)) or
                        any(not math.isfinite(row[key]) for key in SECONDS) or
                        row['sample'] >= 8192 or row['engine_frame'] > 0xffffffff or
                        row['live_frame'] > 0xffffffffffffffff or row['parent_task_id'] >= 32 or
                        row['phase_id'] >= len(PHASE_NAMES) or row['phase_count'] > len(PHASE_NAMES) or
                        row['simulation_count'] > 1 or row['lockstep'] > 1 or row['steps'] > 16 or
                        row['elapsed_ns'] > 0xffffffffffffffff or row['calls'] > 0xffffffff):
                    raise ValueError('Phase profile exceeds bounded counters or finite time values')
                if row['record'] == 'frame':
                    if (row['sample'] != len(frames) or len(frames) >= len(tasks) or
                            any(row[key] != tasks[len(frames)][key] for key in IDENTITY) or
                            any(row[key] for key in ('parent_task_id', 'phase_id', 'phase_name', 'elapsed_ns',
                                                    'calls', 'steps', 'lockstep', *SECONDS))):
                        raise ValueError('Phase/task identities or frame sequence differ')
                    row['phases'], row['simulation'] = {}, None
                    frames.append(row)
                    continue
                if not frames or any(row[key] != frames[-1][key] for key in IDENTITY):
                    raise ValueError('Phase/frame identities differ')
                parent = tasks[len(frames) - 1]['tasks'].get(row['parent_task_id'])
                if parent is None or not parent['run_calls']:
                    raise ValueError('Phase parent must be a Run in the same frame')
                if row['phase_count'] or row['simulation_count']:
                    raise ValueError('Invalid phase row counts')
                if row['record'] == 'phase':
                    phase_id = row['phase_id']
                    if (phase_id in frames[-1]['phases'] or not row['calls'] or
                            row['phase_name'] != PHASE_NAMES[phase_id] or
                            any(row[key] for key in ('steps', 'lockstep', *SECONDS))):
                        raise ValueError('Invalid or duplicate phase record')
                    frames[-1]['phases'][phase_id] = row
                elif row['record'] == 'simulation':
                    if (frames[-1]['simulation'] is not None or row['phase_id'] or row['phase_name'] or
                            row['elapsed_ns'] or row['calls'] != 1 or parent['run_calls'] != 1 or
                            row['tick_seconds'] <= 0 or (row['lockstep'] and row['steps'] > 1)):
                        raise ValueError('Invalid or duplicate simulation record')
                    frames[-1]['simulation'] = row
                else:
                    raise ValueError('Unknown phase record type')
    except (csv.Error, TypeError) as exc:
        raise ValueError('Malformed phase profile CSV') from exc
    if len(frames) != len(tasks):
        raise ValueError('Phase/task sample counts differ')
    for frame, task_frame in zip(frames, tasks):
        if (len(frame['phases']) != frame['phase_count'] or
                int(frame['simulation'] is not None) != frame['simulation_count']):
            raise ValueError('Incomplete phase or simulation records')
        children = {}
        for row in frame['phases'].values():
            parent = row['parent_task_id']
            children[parent] = children.get(parent, 0) + row['elapsed_ns']
        if any(ns > task_frame['tasks'][parent]['run_ns'] for parent, ns in children.items()):
            raise ValueError('Child phase scopes exceed parent Run wall time')
        frame['children_ns'] = children
        simulation = frame['simulation']
        for phase_id in range(FIXED_PHASES):
            row = frame['phases'].get(phase_id)
            expected = simulation['steps'] + int(phase_id < 2) if simulation else 0
            if ((row['calls'] if row else 0) != expected or
                    (row and (simulation is None or row['parent_task_id'] != simulation['parent_task_id']))):
                raise ValueError('Fixed phase calls disagree with recorded simulation steps')
        if simulation:
            before, scaled, after, tick = (simulation[key] for key in
                ('debt_before_seconds', 'scaled_dt_seconds', 'debt_after_seconds', 'tick_seconds'))
            expected = 0.0 if simulation['lockstep'] and simulation['steps'] else before + scaled - simulation['steps'] * tick
            # Inputs are original float32 values; each accumulator subtraction
            # can round. This tolerance covers those operations and 9-digit CSV.
            tolerance = (simulation['steps'] + 4) * 2 ** -23 * max(1.0, abs(before) + abs(scaled)) + 1e-8
            if abs(expected - after) > tolerance:
                raise ValueError('Recorded simulation debt is inconsistent with steps and scaled dt')
    return frames


def summarize_phases(frames, tasks, names):
    count = len(frames)
    keys = {(row['parent_task_id'], phase) for frame in frames for phase, row in frame['phases'].items()}
    ranked = []
    for parent, phase in sorted(keys):
        rows = [frame['phases'].get(phase) for frame in frames]
        rows = [row if row and row['parent_task_id'] == parent else {} for row in rows]
        ns = [row.get('elapsed_ns', 0) for row in rows]
        calls = sum(row.get('calls', 0) for row in rows)
        ranked.append({'parent_task_id': parent, 'parent_name': names[parent], 'phase_id': phase,
                       'name': PHASE_NAMES[phase], 'elapsed_ns': sum(ns), 'calls': calls,
                       'ms_per_frame': sum(ns) / count / 1e6,
                       'ms_per_call': sum(ns) / calls / 1e6, 'distribution': distribution(ns)})
    ranked.sort(key=lambda row: row['elapsed_ns'], reverse=True)
    parents = []
    for parent in sorted({parent for parent, _ in keys}):
        parent_ns = sum(frame['tasks'].get(parent, {}).get('run_ns', 0) for frame in tasks)
        child_ns = sum(frame['children_ns'].get(parent, 0) for frame in frames)
        parents.append({'task_id': parent, 'name': names[parent], 'run_ms_per_frame': parent_ns / count / 1e6,
                        'children_ms_per_frame': child_ns / count / 1e6,
                        'unattributed_ms_per_frame': (parent_ns - child_ns) / count / 1e6})
    simulation = [frame['simulation'] for frame in frames if frame['simulation'] is not None]
    histogram = {}
    for row in simulation:
        key = str(row['steps'])
        histogram[key] = histogram.get(key, 0) + 1
    return {'limits': 'Disjoint child phases are subsets of parent Run time, not additional task or manager time. Pre-physics includes worker execution and joins. Steps are recorded loop iterations.',
            'phases': ranked, 'parents': parents,
            'simulation': {'runs': len(simulation), 'steps_total': sum(row['steps'] for row in simulation),
                           'steps_histogram': histogram,
                           'scaled_dt_total_seconds': sum(row['scaled_dt_seconds'] for row in simulation),
                           'simulated_total_seconds': sum(row['steps'] * row['tick_seconds'] for row in simulation),
                           'max_debt_after_seconds': max((row['debt_after_seconds'] for row in simulation), default=0)}}


def analyze(capture, expected=None, *, require_live_play=True):
    capture = Path(capture)
    packets_summary = analyze_packets(capture, expected, require_live_play=require_live_play)
    cadence = read_integers(capture)
    packets = read_integers(Path(str(capture) + '.packets.csv'))
    task_path = Path(str(capture) + '.tasks.csv')
    frames, names = read_tasks(task_path)
    if len(frames) != len(cadence):
        raise ValueError('Task/cadence sample counts differ')
    for frame, packet, wall in zip(frames, packets, cadence):
        if (any(frame[key] != packet[key] for key in IDENTITY) or
                frame['sample'] != wall['sample'] or frame['live_frame'] != wall['match_frame']):
            raise ValueError('Task/packet/cadence identities differ')
        if frame['manager_ns'] > wall['frame_us'] * 1000 + 2000:
            raise ValueError('Task manager exceeds whole-frame wall time')
        if packet['packet_ns'] > frame['manager_ns']:
            raise ValueError('Packet scopes exceed task manager wall time')
    count = len(frames)
    ranked = []
    for task_id, name in names.items():
        rows = [frame['tasks'].get(task_id, dict.fromkeys(STATS, 0)) for frame in frames]
        totals = {key: sum(row[key] for row in rows) for key in STATS}
        ranked.append({'task_id': task_id, 'name': name, **totals,
                       'run_ms_per_frame': totals['run_ns'] / count / 1e6,
                       'transition_ms_per_frame': totals['transition_ns'] / count / 1e6,
                       'run_calls_per_frame': totals['run_calls'] / count,
                       'transition_calls_per_frame': totals['transition_calls'] / count,
                       'combined': distribution([row['run_ns'] + row['transition_ns'] for row in rows])})
    ranked.sort(key=lambda task: task['run_ns'] + task['transition_ns'], reverse=True)
    result = {'attribution_only': True,
            'capture_scope': packets_summary['capture_scope'],
            'limits': 'Instrumented producer wall time. Run/transition scopes are subsets of manager time; packet scopes lie inside task work. Owner CPU work overlaps producer work. No GPU time or quiet-build FPS inference.',
            'samples': count, 'first_live_frame': frames[0]['live_frame'], 'last_live_frame': frames[-1]['live_frame'],
            'first_engine_frame': frames[0]['engine_frame'], 'last_engine_frame': frames[-1]['engine_frame'],
            'sha256': {**packets_summary['sha256'], task_path.name: hashlib.sha256(task_path.read_bytes()).hexdigest()},
            'instrumented_cadence': packets_summary['instrumented_cadence'],
            'manager': distribution([frame['manager_ns'] for frame in frames]),
            'manager_outside_task_scopes_ms_per_frame': sum(frame['manager_ns'] - frame['attributed_ns'] for frame in frames) / count / 1e6,
            'manager_outside_packet_scopes_ms_per_frame': sum(frame['manager_ns'] - packet['packet_ns'] for frame, packet in zip(frames, packets)) / count / 1e6,
            'frame_outside_manager_ms_per_frame': sum(wall['frame_us'] * 1000 - frame['manager_ns'] for frame, wall in zip(frames, cadence)) / count / 1e6,
            'tasks': ranked}
    phase_path = Path(str(capture) + '.phases.csv')
    if phase_path.exists():
        phases = read_phases(phase_path, frames)
        result['sha256'][phase_path.name] = hashlib.sha256(phase_path.read_bytes()).hexdigest()
        result['phase_profile'] = summarize_phases(phases, frames, names)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--expected', type=int)
    parser.add_argument('--allow-non-live', action='store_true',
                        help='Explicitly allow match scenes, non-match frames or mixed captures; no live-play inference')
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    try:
        result = json.dumps(analyze(args.capture, args.expected,
                                    require_live_play=not args.allow_non_live), indent=2) + '\n'
        if args.out:
            args.out.write_text(result)
        print(result)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'Invalid task attribution: {exc}\n')
