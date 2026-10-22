#!/usr/bin/env python3
"""Read actual C++ exporter output, then reject stale/mismatched attribution."""
import os
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from analyze_packet_profile import analyze

UNIFORM_METADATA = '# strikers-uniform-profile-v1 quantities_only=1'
UNIFORM_FIELDS = ('sample,live_frame,engine_frame,discarded,cache_enabled,'
    'vs_pack_calls,vs_pack_bytes,vs_reservations,vs_reserved_bytes,vs_cache_copies,vs_cache_copy_bytes,vs_reused_values,vs_reused_bytes,'
    'fs_pack_calls,fs_pack_bytes,fs_reservations,fs_reserved_bytes,fs_cache_copies,fs_cache_copy_bytes,fs_reused_values,fs_reused_bytes,'
    'fallback_uploads,errors,cache_bytes,cache_peak_bytes').split(',')


class ProfileTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        subprocess.run([os.environ['STRIKERS_PACKET_PROFILE_PROBE'], str(self.root)], check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.capture = self.root / 'run.csv'
        self.capture.write_text('# play_only=1 counter=live_play\n'
            'sample,match,match_frame,frame_us,tasks_us,present_us,sleep_us\n'
            '0,1,600,12000,10000,2000,0\n1,1,601,12000,10000,2000,0\n')

    def tearDown(self):
        self.tmp.cleanup()

    def select_frames(self, matches, counters):
        self.capture.write_text('# play_only=0 counter=renderer_scene\n'
            'sample,match,match_frame,frame_us,tasks_us,present_us,sleep_us\n' +
            ''.join(f'{i},{match},{counter},12000,10000,2000,0\n'
                    for i, (match, counter) in enumerate(zip(matches, counters))))
        self.mutate_packets(lambda rows: [row.update(live_frame=str(counters[int(row['sample'])]))
                                         for row in rows])

    def mutate_packets(self, change):
        path = Path(str(self.capture) + '.packets.csv')
        with path.open(newline='') as file:
            metadata = file.readline()
            reader = csv.DictReader(file)
            fields, rows = reader.fieldnames, list(reader)
        change(rows)
        with path.open('w', newline='') as file:
            file.write(metadata)
            writer = csv.DictWriter(file, fieldnames=fields)
            writer.writeheader()
            writer.writerows(rows)

    def uniform_rows(self, enabled=True):
        with Path(str(self.capture) + '.packets.csv').open(newline='') as file:
            file.readline()
            packets = list(csv.DictReader(file))
        rows = []
        for i, packet in enumerate(packets):
            row = dict.fromkeys(UNIFORM_FIELDS, 0)
            row.update({key: int(packet[key]) for key in ('sample', 'live_frame', 'engine_frame', 'discarded')})
            scale = i + 1
            row.update(cache_enabled=int(enabled), vs_pack_calls=2 * scale, vs_pack_bytes=24 * scale,
                       vs_reservations=3 * scale, vs_reserved_bytes=96 * scale,
                       fs_pack_calls=scale, fs_pack_bytes=8 * scale,
                       fs_reservations=scale, fs_reserved_bytes=32 * scale, errors=i)
            if enabled:
                row.update(vs_cache_copies=2 * scale, vs_cache_copy_bytes=64 * scale,
                           vs_reused_values=4 * scale, vs_reused_bytes=16 * scale,
                           fs_cache_copies=scale, fs_cache_copy_bytes=32 * scale,
                           fs_reused_values=2 * scale, fs_reused_bytes=8 * scale,
                           fallback_uploads=i, cache_bytes=32 * scale, cache_peak_bytes=64 * scale)
            rows.append(row)
        return rows

    def write_uniforms(self, rows=None, metadata=UNIFORM_METADATA, fields=UNIFORM_FIELDS):
        path = Path(str(self.capture) + '.uniforms.csv')
        if rows is None:
            rows = self.uniform_rows()
        with path.open('w', newline='') as file:
            file.write(metadata + '\n')
            writer = csv.DictWriter(file, fieldnames=fields, extrasaction='ignore')
            writer.writeheader()
            writer.writerows(rows)
        return path

    def test_actual_exporter_and_subset_accounting(self):
        result = analyze(self.capture, 2)
        self.assertTrue(result['attribution_only'])
        self.assertEqual(result['first_live_frame'], 600)
        self.assertEqual(result['counts']['attempts'], 4)
        self.assertEqual(result['counts']['accepted'], 3)
        self.assertEqual(result['counts']['rejected'], 1)
        self.assertAlmostEqual(result['packet_ms_per_frame'], .00099)
        self.assertAlmostEqual(sum(result['phase_ms_per_frame'].values()), result['packet_ms_per_frame'])
        self.assertEqual(result['views'][0]['name'], 'GLV_Characters')
        self.assertEqual(result['counts']['vertex_bytes'], 432)
        self.assertTrue(result['capture_scope']['require_live_play'])
        self.assertTrue(result['capture_scope']['live_play_established'])

    def test_actual_uniform_exporter(self):
        capture = self.root / 'uniform.csv'
        capture.write_text(self.capture.read_text())
        result = analyze(capture, 2)
        self.assertTrue(result['uniforms']['quantities_only'])
        self.assertTrue(result['uniforms']['cache_enabled'])
        self.assertEqual(result['uniforms']['samples'], 2)
        self.assertEqual(result['uniforms']['totals']['vs_pack_calls'], 6)
        self.assertEqual(result['uniforms']['totals']['vs_cache_copy_bytes'], 512)
        self.assertEqual(result['uniforms']['totals']['fallback_uploads'], 2)
        self.assertEqual(result['uniforms']['cache_bytes_last'], 4096)
        self.assertEqual(result['uniforms']['cache_peak_bytes_max'], 8192)
        self.assertIn('uniform.csv.uniforms.csv', result['sha256'])

    def test_non_match_capture_requires_explicit_opt_in(self):
        self.select_frames((0, 0), (0, 0))
        with self.assertRaisesRegex(ValueError, 'live-play'):
            analyze(self.capture, 2)
        result = analyze(self.capture, 2, require_live_play=False)
        scope = result['capture_scope']
        self.assertEqual(scope['counter'], 'renderer_scene')
        self.assertEqual((scope['match_samples'], scope['non_match_samples']), (0, 2))
        self.assertFalse(scope['require_live_play'])
        self.assertFalse(scope['live_play_established'])
        self.assertFalse(result['instrumented_cadence']['all_gameplay'])
        self.assertEqual((result['first_live_frame'], result['last_live_frame']), (0, 0))
        self.assertEqual(result['counts']['attempts'], 4)

    def test_opt_in_allows_mixed_and_match_scenes_without_claiming_live_play(self):
        for matches, counters, counts in (((0, 1), (0, 600), (1, 1)),
                                          ((1, 1), (600, 601), (2, 0))):
            with self.subTest(matches=matches):
                self.select_frames(matches, counters)
                with self.assertRaisesRegex(ValueError, 'live-play'):
                    analyze(self.capture, 2)
                scope = analyze(self.capture, 2, require_live_play=False)['capture_scope']
                self.assertEqual((scope['match_samples'], scope['non_match_samples']), counts)
                self.assertFalse(scope['live_play_established'])

    def test_opt_in_retains_identity_census_and_timing_validation(self):
        self.select_frames((0, 0), (0, 0))
        path = Path(str(self.capture) + '.packets.csv')
        original = path.read_bytes()
        for change, message in (
            (lambda rows: rows[0].update(live_frame='7'), 'identities'),
            (lambda rows: rows[0].update(accepted='99'), 'census'),
            (lambda rows: rows[0].update(packet_ns='999999'), 'partition'),
        ):
            with self.subTest(message=message):
                path.write_bytes(original)
                self.mutate_packets(change)
                with self.assertRaisesRegex(ValueError, message):
                    analyze(self.capture, 2, require_live_play=False)

    def test_opt_in_requires_unambiguous_counter_metadata(self):
        self.select_frames((0, 0), (0, 0))
        original = self.capture.read_text()
        marker = '# play_only=0 counter=renderer_scene\n'
        for replacement in ('', marker + marker, '# play_only=0 counter=unknown\n'):
            with self.subTest(marker=replacement):
                self.capture.write_text(original.replace(marker, replacement))
                with self.assertRaisesRegex(ValueError, 'frame-counter selection'):
                    analyze(self.capture, 2, require_live_play=False)
        self.capture.write_text(original.replace(marker, '# play_only=1 counter=live_play\n'))
        with self.assertRaisesRegex(ValueError, 'outside gameplay'):
            analyze(self.capture, 2, require_live_play=False)

    def test_cli_non_live_flag_is_explicit(self):
        self.select_frames((0, 0), (0, 0))
        command = [sys.executable, str(Path(__file__).with_name('analyze_packet_profile.py')),
                   str(self.capture), '--expected', '2']
        strict = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(strict.returncode, 0)
        allowed = subprocess.run([*command, '--allow-non-live'], capture_output=True, text=True)
        self.assertEqual(allowed.returncode, 0, allowed.stderr)
        self.assertFalse(json.loads(allowed.stdout)['capture_scope']['live_play_established'])

    def test_mismatched_live_frame(self):
        self.capture.write_text(self.capture.read_text().replace('600,', '500,').replace('601,', '501,'))
        with self.assertRaisesRegex(ValueError, 'identities'):
            analyze(self.capture)

    def test_incomplete_capture(self):
        with self.assertRaisesRegex(ValueError, 'Incomplete'):
            analyze(self.capture, 3)

    def test_view_totals_must_match_frames(self):
        path = Path(str(self.capture) + '.views.csv')
        lines = path.read_text().splitlines()
        path.write_text('\n'.join(lines[:-1]) + '\n')
        with self.assertRaisesRegex(ValueError, 'aggregates differ'):
            analyze(self.capture)

    def test_legacy_capture_has_no_uniform_report(self):
        result = analyze(self.capture, 2)
        self.assertNotIn('uniforms', result)
        self.assertEqual(set(result['sha256']), {'run.csv', 'run.csv.packets.csv', 'run.csv.views.csv'})

    def test_uniform_quantities_and_gauges_do_not_change_timing(self):
        legacy = analyze(self.capture, 2)
        path = self.write_uniforms()
        result = analyze(self.capture, 2)
        uniforms = result.pop('uniforms')
        digest = result['sha256'].pop(path.name)
        self.assertEqual(digest, hashlib.sha256(path.read_bytes()).hexdigest())
        self.assertEqual(result, legacy)
        self.assertTrue(uniforms['quantities_only'])
        self.assertTrue(uniforms['cache_enabled'])
        self.assertEqual(uniforms['samples'], 2)
        self.assertEqual(len(uniforms['totals']), 18)
        self.assertEqual(uniforms['totals']['vs_pack_calls'], 6)
        self.assertEqual(uniforms['totals']['vs_pack_bytes'], 72)
        self.assertEqual(uniforms['totals']['vs_cache_copy_bytes'], 192)
        self.assertEqual(uniforms['totals']['fs_reused_bytes'], 24)
        self.assertEqual(uniforms['totals']['fallback_uploads'], 1)
        self.assertEqual(uniforms['totals']['errors'], 1)
        self.assertEqual(uniforms['mean_per_frame']['vs_pack_calls'], 3)
        self.assertEqual(uniforms['mean_per_frame']['fs_pack_calls'], 1.5)
        self.assertEqual(uniforms['cache_bytes_last'], 64)
        self.assertEqual(uniforms['cache_peak_bytes_max'], 128)
        self.assertNotIn('cache_bytes', uniforms['totals'])
        self.assertNotIn('cache_peak_bytes', uniforms['totals'])
        self.assertIn('neither GPU time nor a speedup', uniforms['limits'])

    def test_uniform_off_still_counts_sdk_work_and_errors(self):
        self.write_uniforms(self.uniform_rows(enabled=False))
        result = analyze(self.capture, 2)['uniforms']
        self.assertFalse(result['cache_enabled'])
        self.assertEqual(result['totals']['vs_pack_calls'], 6)
        self.assertEqual(result['totals']['fs_reserved_bytes'], 96)
        self.assertEqual(result['totals']['errors'], 1)
        self.assertEqual(result['cache_peak_bytes_max'], 0)
        self.assertEqual(result['totals']['fallback_uploads'], 0)

    def test_uniform_missing_extra_duplicate_or_reordered_samples(self):
        original = self.uniform_rows()
        for rows in ([], original[:1], original + [original[1]], [original[0], original[0]], original[::-1]):
            with self.subTest(rows=rows):
                self.write_uniforms(rows)
                with self.assertRaisesRegex(ValueError, 'Uniform/packet sample'):
                    analyze(self.capture, 2)

    def test_uniform_each_identity_field_matches_packets(self):
        for key in ('sample', 'live_frame', 'engine_frame', 'discarded'):
            with self.subTest(key=key):
                rows = self.uniform_rows()
                rows[0][key] += 1
                self.write_uniforms(rows)
                with self.assertRaisesRegex(ValueError, 'identities'):
                    analyze(self.capture, 2)

    def test_uniform_requires_exact_metadata_and_columns(self):
        for metadata in ('', '# strikers-uniform-profile-v1 quantities_only=0', '# strikers-uniform-profile-v2 quantities_only=1'):
            with self.subTest(metadata=metadata):
                self.write_uniforms(metadata=metadata)
                with self.assertRaisesRegex(ValueError, 'metadata'):
                    analyze(self.capture, 2)
        for fields in (UNIFORM_FIELDS[:-1], UNIFORM_FIELDS + ['elapsed_ns'],
                       ['unknown'] + UNIFORM_FIELDS[1:], UNIFORM_FIELDS[::-1],
                       UNIFORM_FIELDS[:-1] + [UNIFORM_FIELDS[-2]]):
            with self.subTest(fields=fields):
                self.write_uniforms(fields=fields)
                with self.assertRaisesRegex(ValueError, 'columns'):
                    analyze(self.capture, 2)

    def test_uniform_counters_are_bounded_uint64(self):
        for key in UNIFORM_FIELDS:
            for bad in (-1, 1 << 64, '1.5', '', 'nan', '1_000'):
                with self.subTest(key=key, value=bad):
                    rows = self.uniform_rows()
                    rows[0][key] = bad
                    self.write_uniforms(rows)
                    with self.assertRaisesRegex(ValueError, 'uint64'):
                        analyze(self.capture, 2)

    def test_uniform_uint64_max_and_exact_aggregate(self):
        rows = self.uniform_rows()
        maximum = (1 << 64) - 1
        for row in rows:
            row['vs_pack_bytes'] = maximum
            row['cache_peak_bytes'] = maximum
        self.write_uniforms(rows)
        result = analyze(self.capture, 2)['uniforms']
        self.assertEqual(result['totals']['vs_pack_bytes'], 2 * maximum)
        self.assertEqual(result['cache_peak_bytes_max'], maximum)

    def test_uniform_cache_mode_is_boolean_and_constant(self):
        for modes in ((2, 2), (0, 1), (1, 0)):
            with self.subTest(modes=modes):
                rows = self.uniform_rows(enabled=False)
                for row, mode in zip(rows, modes):
                    row['cache_enabled'] = mode
                self.write_uniforms(rows)
                with self.assertRaisesRegex(ValueError, 'cache mode'):
                    analyze(self.capture, 2)

    def test_uniform_off_forbids_cache_quantities_and_residency(self):
        keys = ('vs_cache_copies', 'vs_cache_copy_bytes', 'vs_reused_values', 'vs_reused_bytes',
                'fs_cache_copies', 'fs_cache_copy_bytes', 'fs_reused_values', 'fs_reused_bytes',
                'fallback_uploads', 'cache_bytes', 'cache_peak_bytes')
        for key in keys:
            with self.subTest(key=key):
                rows = self.uniform_rows(enabled=False)
                rows[0][key] = 1
                self.write_uniforms(rows)
                with self.assertRaises(ValueError):
                    analyze(self.capture, 2)

    def test_uniform_cache_gauges_and_copy_reservations(self):
        for change, message in (
            (lambda rows: rows[0].update(cache_bytes=65), 'gauges'),
            (lambda rows: rows[1].update(cache_peak_bytes=63, cache_bytes=32), 'gauges'),
            (lambda rows: rows[0].update(vs_cache_copies=4), 'reservations'),
            (lambda rows: rows[0].update(vs_cache_copy_bytes=97), 'reservations'),
            (lambda rows: rows[0].update(fs_cache_copies=2), 'reservations'),
            (lambda rows: rows[0].update(fs_cache_copy_bytes=33), 'reservations'),
        ):
            with self.subTest(message=message):
                rows = self.uniform_rows()
                change(rows)
                self.write_uniforms(rows)
                with self.assertRaisesRegex(ValueError, message):
                    analyze(self.capture, 2)

    def test_uniform_presence_never_relaxes_frame_or_packet_validation(self):
        self.select_frames((0, 1), (0, 600))
        self.write_uniforms()
        with self.assertRaisesRegex(ValueError, 'live-play'):
            analyze(self.capture, 2)
        result = analyze(self.capture, 2, require_live_play=False)
        self.assertFalse(result['capture_scope']['live_play_established'])
        self.assertTrue(result['uniforms']['quantities_only'])
        self.mutate_packets(lambda rows: rows[0].update(packet_ns='999999'))
        with self.assertRaisesRegex(ValueError, 'partition'):
            analyze(self.capture, 2, require_live_play=False)

    def test_uniform_truncated_or_extra_cells_fail(self):
        for delta in (-1, 1):
            path = self.write_uniforms()
            lines = path.read_text().splitlines()
            cells = lines[2].split(',')
            lines[2] = ','.join(cells[:-1] if delta < 0 else cells + ['0'])
            path.write_text('\n'.join(lines) + '\n')
            with self.assertRaisesRegex(ValueError, 'Malformed uniform'):
                analyze(self.capture, 2)


if __name__ == '__main__':
    unittest.main()
