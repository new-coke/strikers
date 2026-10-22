#!/usr/bin/env python3
"""Join finite frame and packet attribution; require live play unless explicitly disabled."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
from analyze_vita_frames import analyze as analyze_frames

PHASES = ('invalidation_ns', 'arrays_ns', 'skin_ns', 'material_ns', 'geometry_ns', 'submit_ns', 'cleanup_ns')
COUNTERS = ('attempts', 'accepted', 'rejected', 'resident_hits', 'resident_builds', 'dynamic_draws',
            'input_references', 'decoded_vertices', 'decoded_indices', 'vertex_bytes', 'index_bytes', 'packet_wait_ns')
OWNER = ('owner_draw_us', 'owner_present_us', 'frontend_wait_us', 'command_bytes', 'command_copy_ns',
         'command_vertex_bytes', 'command_index_bytes', 'slot_waits', 'renderer_draws')
UNIFORM_METADATA = '# strikers-uniform-profile-v1 quantities_only=1'
UNIFORM_IDENTITY = ('sample', 'live_frame', 'engine_frame', 'discarded')
UNIFORM_STAGE_COUNTERS = ('pack_calls', 'pack_bytes', 'reservations', 'reserved_bytes',
                          'cache_copies', 'cache_copy_bytes', 'reused_values', 'reused_bytes')
UNIFORM_COUNTERS = tuple(f'{stage}_{key}' for stage in ('vs', 'fs') for key in UNIFORM_STAGE_COUNTERS) + ('fallback_uploads', 'errors')
UNIFORM_FIELDS = (*UNIFORM_IDENTITY, 'cache_enabled', *UNIFORM_COUNTERS, 'cache_bytes', 'cache_peak_bytes')


def read(path, allow_empty=False, fields=None):
    lines = path.read_text().splitlines()
    reader = csv.DictReader(line for line in lines if line and not line.startswith('#'))
    if fields is not None and reader.fieldnames != list(fields):
        raise ValueError(f'Unexpected columns: {path}')
    rows = list(reader)
    try:
        converted = [{key: int(value) for key, value in row.items()} for row in rows]
    except (ValueError, TypeError) as exc:
        raise ValueError(f'Malformed integer CSV: {path}') from exc
    if (not converted and not allow_empty) or any(value < 0 for row in converted for value in row.values()):
        raise ValueError(f'Empty or negative CSV: {path}')
    return converted


def analyze_uniforms(path, packets):
    """Validate optional provider quantities against the exact packet samples."""
    rows = []
    try:
        with path.open(newline='') as file:
            if file.readline().rstrip('\r\n') != UNIFORM_METADATA:
                raise ValueError('Missing uniform profile metadata')
            reader = csv.DictReader(file, strict=True)
            if reader.fieldnames != list(UNIFORM_FIELDS):
                raise ValueError('Unexpected uniform profile columns')
            for raw in reader:
                if set(raw) != set(UNIFORM_FIELDS) or any(value is None for value in raw.values()):
                    raise ValueError('Malformed uniform profile row')
                row = {}
                for key in UNIFORM_FIELDS:
                    value = raw[key]
                    if not re.fullmatch(r'[0-9]+', value):
                        raise ValueError('Uniform profile value is not uint64')
                    row[key] = int(value)
                    if row[key] > 0xffffffffffffffff:
                        raise ValueError('Uniform profile exceeds uint64')
                rows.append(row)
    except csv.Error as exc:
        raise ValueError('Malformed uniform profile CSV') from exc
    if not rows or len(rows) != len(packets):
        raise ValueError('Uniform/packet sample counts differ')
    mode = rows[0]['cache_enabled']
    peak = 0
    cache_only = tuple(f'{stage}_{key}' for stage in ('vs', 'fs')
                       for key in ('cache_copies', 'cache_copy_bytes', 'reused_values', 'reused_bytes')) + (
                           'fallback_uploads', 'cache_bytes', 'cache_peak_bytes')
    for row, packet in zip(rows, packets):
        if any(row[key] != packet[key] for key in UNIFORM_IDENTITY):
            raise ValueError('Uniform/packet sample identities differ')
        if row['cache_enabled'] not in (0, 1) or row['cache_enabled'] != mode:
            raise ValueError('Uniform cache mode is invalid or changes within capture')
        if row['cache_bytes'] > row['cache_peak_bytes'] or row['cache_peak_bytes'] < peak:
            raise ValueError('Uniform cache gauges are inconsistent')
        peak = row['cache_peak_bytes']
        if not mode and any(row[key] for key in cache_only):
            raise ValueError('Disabled uniform cache has active cache counters or gauges')
        for stage in ('vs', 'fs'):
            if (row[f'{stage}_cache_copies'] > row[f'{stage}_reservations'] or
                    row[f'{stage}_cache_copy_bytes'] > row[f'{stage}_reserved_bytes']):
                raise ValueError('Uniform cache copies exceed reservations')
    totals = {key: sum(row[key] for row in rows) for key in UNIFORM_COUNTERS}
    return {'quantities_only': True, 'samples': len(rows), 'cache_enabled': bool(mode),
            'totals': totals, 'mean_per_frame': {key: value / len(rows) for key, value in totals.items()},
            'cache_bytes_last': rows[-1]['cache_bytes'], 'cache_peak_bytes_max': peak,
            'limits': 'Provider counts and bytes only, with cache residency gauges reported separately. '
                      'Pack bytes are input float components times four; reservation/copy bytes use SDK sizes. '
                      'Fallbacks and errors are counted, not converted to time. These quantities establish '
                      'neither GPU time nor a speedup.'}


def analyze(capture, expected=None, *, require_live_play=True):
    capture = Path(capture)
    cadence = analyze_frames(capture, expected, require_live_play=require_live_play)
    if (require_live_play or cadence['live_play_only']) and not cadence['all_gameplay']:
        raise ValueError('Capture includes frames outside gameplay')
    frames = read(capture)
    counter_metadata = [line for line in cadence['metadata'] if line.startswith('# play_only=')]
    if not require_live_play and counter_metadata not in (
            ['# play_only=0 counter=renderer_scene'], ['# play_only=1 counter=live_play']):
        raise ValueError('Capture does not establish one frame-counter selection')
    match_samples = sum(row['match'] for row in frames)
    scope = {'require_live_play': require_live_play,
             'counter': 'live_play' if cadence['live_play_only'] else 'renderer_scene',
             'match_samples': match_samples, 'non_match_samples': len(frames) - match_samples,
             'live_play_established': cadence['live_play_only'] and cadence['all_gameplay'],
             'limits': 'The match flag identifies renderer match activity, not a live-play state. '
                       'With renderer_scene, match samples may include NIS/replay; non-match samples '
                       'do not identify a particular frontend or movie. first/last_live_frame retain '
                       'the exporter column name and use the counter identified here.'}
    packets_path = Path(str(capture) + '.packets.csv')
    views_path = Path(str(capture) + '.views.csv')
    if '# strikers-packet-profile-v1 attribution_only=1 waits_are_subsets=1' not in packets_path.read_text().splitlines():
        raise ValueError('Missing packet profile metadata')
    stats = ('packet_ns', *PHASES, *COUNTERS)
    packets = read(packets_path, fields=('sample', 'live_frame', 'engine_frame', 'discarded', *stats, *OWNER))
    views = read(views_path, allow_empty=True, fields=('view', *stats))
    if len(packets) != len(frames):
        raise ValueError('Packet/cadence sample counts differ')
    for frame, packet in zip(frames, packets):
        if (packet['sample'] != frame['sample'] or packet['live_frame'] != frame['match_frame']
                or packet['discarded'] not in (0, 1)):
            raise ValueError('Packet/cadence sample identities differ')
    for row in [*packets, *views]:
        if row['packet_ns'] != sum(row[key] for key in PHASES):
            raise ValueError('Packet phases do not partition packet wall time')
        if (row['attempts'] != row['accepted'] + row['rejected'] or
                row['accepted'] != row['resident_hits'] + row['resident_builds'] + row['dynamic_draws']):
            raise ValueError('Packet census is inconsistent')
    if len({row['view'] for row in views}) != len(views):
        raise ValueError('Duplicate view aggregation')
    for key in stats:
        if sum(row[key] for row in packets) != sum(row[key] for row in views):
            raise ValueError(f'Frame/view aggregates differ: {key}')
    count = len(packets)
    totals = {key: sum(row[key] for row in packets) for key in (*stats, *OWNER)}
    header = Path(__file__).resolve().parents[1] / 'include/NL/gl/gl.h'
    names = {int(number): name for name, number in re.findall(r'(GLV_\w+)\s*=\s*(\d+)', header.read_text()) if name != 'GLV_Num'}
    ranked = []
    for row in sorted(views, key=lambda row: row['packet_ns'], reverse=True):
        ranked.append({'view': row['view'], 'name': names.get(row['view'], 'unknown'),
                       'packet_ms_per_frame': row['packet_ns'] / count / 1e6,
                       'draws_per_frame': row['accepted'] / count,
                       'dynamic_per_frame': row['dynamic_draws'] / count,
                       'phase_ms_per_frame': {key[:-3]: row[key] / count / 1e6 for key in PHASES},
                       'counts': {key: row[key] for key in COUNTERS}})
    result = {'attribution_only': True,
            'capture_scope': scope,
            'limits': 'Instrumented wall time. Packet waits and command copy are subsets; owner CPU work overlaps producer work. No GPU timestamps or additive frame decomposition.',
            'samples': count, 'first_live_frame': packets[0]['live_frame'], 'last_live_frame': packets[-1]['live_frame'],
            'sha256': {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in (capture, packets_path, views_path)},
            'instrumented_cadence': cadence,
            'tasks_ms_per_frame': sum(row['tasks_us'] for row in frames) / count / 1000,
            'tasks_outside_packet_scopes_ms_per_frame': (sum(row['tasks_us'] for row in frames) * 1000 - totals['packet_ns']) / count / 1e6,
            'packet_ms_per_frame': totals['packet_ns'] / count / 1e6,
            'phase_ms_per_frame': {key[:-3]: totals[key] / count / 1e6 for key in PHASES},
            'owner_draw_ms_per_frame': totals['owner_draw_us'] / count / 1000,
            'owner_present_ms_per_frame': totals['owner_present_us'] / count / 1000,
            'frontend_wait_ms_per_frame': totals['frontend_wait_us'] / count / 1000,
            'packet_wait_ms_per_frame': totals['packet_wait_ns'] / count / 1e6,
            'command_copy_ms_per_frame': totals['command_copy_ns'] / count / 1e6,
            'command_bytes_per_frame': totals['command_bytes'] / count,
            'command_vertex_bytes_per_frame': totals['command_vertex_bytes'] / count,
            'command_index_bytes_per_frame': totals['command_index_bytes'] / count,
            'draws_per_frame': totals['accepted'] / count,
            'counts': {key: totals[key] for key in COUNTERS}, 'views': ranked}
    uniforms_path = Path(str(capture) + '.uniforms.csv')
    if uniforms_path.exists():
        result['uniforms'] = analyze_uniforms(uniforms_path, packets)
        result['sha256'][uniforms_path.name] = hashlib.sha256(uniforms_path.read_bytes()).hexdigest()
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
        parser.exit(1, f'Invalid packet attribution: {exc}\n')
