#!/usr/bin/env python3
"""Join both real C++ exporters, preserving CSV names and rejecting mismatches."""
import csv
import ast
from contextlib import redirect_stdout
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from analyze_task_profile import analyze, METADATA, PHASE_METADATA
from analyze_packet_profile import analyze as analyze_packets
from analyze_vita_frames import analyze as analyze_frames


class TaskProfileTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        # Both probes exercise a missing-directory failure followed by retry.
        # Give them independent fixtures, then join their unchanged CSV bytes.
        for variable, directory in (('STRIKERS_TASK_PROFILE_PROBE', 'tasks'),
                                    ('STRIKERS_PACKET_PROFILE_PROBE', 'packets')):
            result = subprocess.run([os.environ[variable], str(self.root / directory)],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
        self.capture = self.root / 'packets' / 'run.csv'
        self.tasks = Path(str(self.capture) + '.tasks.csv')
        (self.root / 'tasks' / 'run.csv.tasks.csv').replace(self.tasks)
        self.phases = Path(str(self.capture) + '.phases.csv')
        (self.root / 'tasks' / 'run.csv.phases.csv').replace(self.phases)
        self.capture.write_text('# play_only=1 counter=live_play\n'
            'sample,match,match_frame,frame_us,tasks_us,present_us,sleep_us\n'
            '0,1,600,12000,10000,2000,0\n1,1,601,12000,10000,2000,0\n')

    def tearDown(self):
        self.tmp.cleanup()

    def mutate(self, change):
        self.mutate_file(self.tasks, METADATA, change)

    def mutate_phases(self, change):
        self.mutate_file(self.phases, PHASE_METADATA, change)

    def mutate_file(self, path, metadata, change):
        with path.open(newline='') as file:
            self.assertEqual(file.readline().rstrip('\r\n'), metadata)
            reader = csv.DictReader(file)
            fields, rows = reader.fieldnames, list(reader)
        change(rows)
        with path.open('w', newline='') as file:
            file.write(metadata + '\n')
            writer = csv.DictWriter(file, fieldnames=fields)
            writer.writeheader()
            writer.writerows(rows)

    def select_frames(self, matches, counters, play_only=False):
        counter_name = 'live_play' if play_only else 'renderer_scene'
        self.capture.write_text(f'# play_only={int(play_only)} counter={counter_name}\n'
            'sample,match,match_frame,frame_us,tasks_us,present_us,sleep_us\n' +
            ''.join(f'{i},{match},{counter},12000,10000,2000,0\n'
                    for i, (match, counter) in enumerate(zip(matches, counters))))
        update = lambda rows: [row.update(live_frame=str(counters[int(row['sample'])])) for row in rows]
        self.mutate(update)
        self.mutate_phases(update)
        packet_metadata = '# strikers-packet-profile-v1 attribution_only=1 waits_are_subsets=1'
        self.mutate_file(Path(str(self.capture) + '.packets.csv'), packet_metadata, update)

    def test_real_exporters_owned_names_and_timing(self):
        result = analyze(self.capture, 2)
        self.assertTrue(result['attribution_only'])
        self.assertEqual((result['first_live_frame'], result['last_live_frame']), (600, 601))
        self.assertEqual((result['first_engine_frame'], result['last_engine_frame']), (20, 21))
        self.assertEqual(result['tasks'][0]['name'], 'Game,"main"\n#phase')
        self.assertEqual(result['tasks'][0]['run_calls'], 2)
        self.assertEqual(result['tasks'][0]['transition_calls'], 1)
        self.assertAlmostEqual(result['tasks'][0]['run_ms_per_frame'], .0045)
        self.assertAlmostEqual(result['manager']['mean_ms'], .006825)
        self.assertAlmostEqual(result['manager_outside_task_scopes_ms_per_frame'], .000075)
        self.assertAlmostEqual(result['manager_outside_packet_scopes_ms_per_frame'], .005835)
        self.assertNotIn('intro-only', [task['name'] for task in result['tasks']])
        phases = result['phase_profile']
        self.assertEqual(phases['simulation']['steps_total'], 18)
        self.assertEqual(phases['simulation']['steps_histogram'], {'2': 1, '16': 1})
        ai = next(row for row in phases['phases'] if row['name'] == 'Game AI')
        self.assertEqual((ai['parent_task_id'], ai['calls'], ai['elapsed_ns']), (0, 18, 180))
        self.assertEqual(ai['parent_name'], 'Game,"main"\n#phase')
        world = next(row for row in phases['phases'] if row['name'] == 'World render')
        self.assertEqual((world['parent_task_id'], world['calls']), (1, 1))
        self.assertTrue(all(row['unattributed_ms_per_frame'] == 0 for row in phases['parents']))
        self.assertIn(self.phases.name, result['sha256'])
        self.assertTrue(result['capture_scope']['require_live_play'])
        self.assertTrue(result['capture_scope']['live_play_established'])

    def test_non_live_scope_is_explicit_and_preserved_through_task_join(self):
        for matches, counters, counts in (((0, 0), (0, 0), (0, 2)),
                                          ((0, 1), (0, 600), (1, 1)),
                                          ((1, 1), (600, 601), (2, 0))):
            with self.subTest(matches=matches):
                self.select_frames(matches, counters)
                with self.assertRaisesRegex(ValueError, 'live-play'):
                    analyze(self.capture, 2)
                result = analyze(self.capture, 2, require_live_play=False)
                scope = result['capture_scope']
                self.assertEqual((scope['match_samples'], scope['non_match_samples']), counts)
                self.assertEqual(scope['counter'], 'renderer_scene')
                self.assertFalse(scope['live_play_established'])
                self.assertFalse(scope['require_live_play'])
                self.assertEqual(result['phase_profile']['simulation']['steps_total'], 18)

    def test_non_live_opt_in_still_rejects_task_and_phase_corruption(self):
        self.select_frames((0, 0), (0, 0))
        tasks, phases = self.tasks.read_bytes(), self.phases.read_bytes()
        for mutate, change, message in (
            (self.mutate, lambda rows: rows[0].update(engine_frame='99'), 'identities'),
            (self.mutate_phases, lambda rows: rows[0].update(live_frame='1'), 'identities'),
            (self.mutate, lambda rows: rows[1].update(run_ns='99999999'), 'exceed manager'),
            (self.mutate_phases, lambda rows: rows[1].update(elapsed_ns='9999999'), 'exceed parent Run'),
        ):
            with self.subTest(message=message):
                self.tasks.write_bytes(tasks)
                self.phases.write_bytes(phases)
                mutate(change)
                with self.assertRaisesRegex(ValueError, message):
                    analyze(self.capture, 2, require_live_play=False)

    def test_task_cli_non_live_flag_is_explicit(self):
        self.select_frames((0, 0), (0, 0))
        command = [sys.executable, str(Path(__file__).with_name('analyze_task_profile.py')),
                   str(self.capture), '--expected', '2']
        strict = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(strict.returncode, 0)
        allowed = subprocess.run([*command, '--allow-non-live'], capture_output=True, text=True)
        self.assertEqual(allowed.returncode, 0, allowed.stderr)
        self.assertEqual(json.loads(allowed.stdout)['capture_scope']['non_match_samples'], 2)

    def test_old_v1_capture_without_phases(self):
        self.phases.unlink()
        result = analyze(self.capture, 2)
        self.assertNotIn('phase_profile', result)
        self.assertAlmostEqual(result['manager']['mean_ms'], .006825)
        self.assertEqual(result['tasks'][0]['run_calls'], 2)

    def test_phase_frame_identity(self):
        self.mutate_phases(lambda rows: rows[0].update(engine_frame='19'))
        with self.assertRaisesRegex(ValueError, 'identities'):
            analyze(self.capture)

    def test_phase_parent_must_exist_in_same_run(self):
        self.mutate_phases(lambda rows: rows[1].update(parent_task_id='31'))
        with self.assertRaisesRegex(ValueError, 'parent must be a Run'):
            analyze(self.capture)

    def test_incomplete_phase_frames(self):
        self.mutate_phases(lambda rows: rows.__setitem__(slice(None), [row for row in rows if row['sample'] == '0']))
        with self.assertRaisesRegex(ValueError, 'sample counts differ'):
            analyze(self.capture)

    def test_missing_phase_row(self):
        self.mutate_phases(lambda rows: rows.pop(1))
        with self.assertRaisesRegex(ValueError, 'Incomplete phase'):
            analyze(self.capture)

    def test_duplicate_phase_row(self):
        self.mutate_phases(lambda rows: rows.append(next(row.copy() for row in reversed(rows) if row['record'] == 'phase')))
        with self.assertRaisesRegex(ValueError, 'duplicate phase'):
            analyze(self.capture)

    def test_phase_scopes_are_subsets_of_parent_run(self):
        self.mutate_phases(lambda rows: rows[1].update(elapsed_ns='9999999'))
        with self.assertRaisesRegex(ValueError, 'exceed parent Run'):
            analyze(self.capture)

    def test_phase_dictionary_is_stable(self):
        self.mutate_phases(lambda rows: rows[1].update(phase_name='another phase'))
        with self.assertRaisesRegex(ValueError, 'Invalid or duplicate phase'):
            analyze(self.capture)

    def test_fixed_calls_match_actual_steps(self):
        self.mutate_phases(lambda rows: next(row for row in rows if row['record'] == 'phase' and row['phase_id'] == '2').update(calls='3'))
        with self.assertRaisesRegex(ValueError, 'recorded simulation steps'):
            analyze(self.capture)

    def test_simulation_count_and_step_bounds(self):
        original = self.phases.read_bytes()
        for change, message in (
            (lambda rows: rows.__setitem__(slice(None), [row for row in rows if not (row['sample'] == '0' and row['record'] == 'simulation')]), 'Incomplete phase'),
            (lambda rows: next(row for row in rows if row['record'] == 'simulation').update(steps='17'), 'bounded counters'),
            (lambda rows: next(row for row in rows if row['record'] == 'simulation').update(lockstep='1'), 'Invalid or duplicate simulation'),
            (lambda rows: next(row for row in rows if row['record'] == 'simulation').update(parent_task_id='1'), 'recorded simulation steps'),
        ):
            with self.subTest(message=message):
                self.phases.write_bytes(original)
                self.mutate_phases(change)
                with self.assertRaisesRegex(ValueError, message):
                    analyze(self.capture)

    def test_simulation_units_and_debt(self):
        original = self.phases.read_bytes()
        for field, value, message in (
            ('debt_after_seconds', '.4', 'debt is inconsistent'),
            ('scaled_dt_seconds', 'nan', 'finite time values'),
            ('tick_seconds', '0', 'Invalid or duplicate simulation'),
        ):
            with self.subTest(field=field):
                self.phases.write_bytes(original)
                self.mutate_phases(lambda rows: next(row for row in rows if row['record'] == 'simulation').update({field: value}))
                with self.assertRaisesRegex(ValueError, message):
                    analyze(self.capture)

    def test_engine_frame_mismatch(self):
        self.mutate(lambda rows: rows[0].update(engine_frame='99'))
        with self.assertRaisesRegex(ValueError, 'identities'):
            analyze(self.capture)

    def test_live_frame_mismatch(self):
        self.mutate(lambda rows: [row.update(live_frame='500') for row in rows if row['sample'] == '0'])
        with self.assertRaisesRegex(ValueError, 'identities'):
            analyze(self.capture)

    def test_incomplete_sample(self):
        self.mutate(lambda rows: rows.__setitem__(slice(None), [row for row in rows if row['sample'] == '0']))
        with self.assertRaisesRegex(ValueError, 'counts differ'):
            analyze(self.capture)

    def test_missing_task_row(self):
        self.mutate(lambda rows: rows.pop())
        with self.assertRaisesRegex(ValueError, 'Incomplete task records'):
            analyze(self.capture)

    def test_duplicate_task(self):
        self.mutate(lambda rows: rows.append(rows[-1].copy()))
        with self.assertRaisesRegex(ValueError, 'duplicate task'):
            analyze(self.capture)

    def test_dictionary_identity(self):
        self.mutate(lambda rows: rows[-1].update(task_name='different'))
        with self.assertRaisesRegex(ValueError, 'dictionary identity'):
            analyze(self.capture)

    def test_task_scopes_are_subsets(self):
        self.mutate(lambda rows: rows[1].update(run_ns='99999999'))
        with self.assertRaisesRegex(ValueError, 'scopes exceed manager'):
            analyze(self.capture)

    def test_malformed_and_counter_overflow(self):
        self.mutate(lambda rows: rows[-1].update(run_calls=str(1 << 32)))
        with self.assertRaisesRegex(ValueError, 'bounded counters'):
            analyze(self.capture)


if __name__ == '__main__':
    unittest.main()
