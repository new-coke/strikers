"""Offline audio-capsule gates using the exact preserved core/ELF as oracle.

No build, registry or original evidence is changed. Full binaries remain in
memory; only small text/JSON capsule fixtures are written to a temporary tree.
"""
import argparse
import copy
import gzip
import importlib.util
import json
import os
from pathlib import Path
import struct
import tempfile
import time
import unittest

import build_vita_audio_crash_capsule as builder
from vita_crash_audio import (AudioEvidenceError, core_snapshot, direct_bl_target,
                              memory, program_headers, sha, take, unwind_frame,
                              unwind_stream, validate_extract)
from vita_crash_retention import (AUDIO_SCOPE, CAPSULE_SCHEMA_V2, CrashRetentionError,
                                  verify_reviewed_crash)


ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / builder.EVIDENCE / builder.RUN / 'core-retention-gate/vita_crash_retention.before.py'
OBSERVED_NEGATIVES = []


class AudioRetentionTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prepared = builder.collect(ROOT, Path('/usr/local/vitasdk/bin'))
        cls.extract = json.loads(cls.prepared['outputs']['core-retention-elf.json'])

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.evidence = self.root / 'evidence'
        self.run = self.evidence / builder.RUN
        self.build = self.root / 'build'
        self.run.mkdir(parents=True)
        self.build.mkdir()
        for name, raw in self.prepared['original_inputs'].items():
            (self.run / name).write_bytes(raw)
        builder.publish(self.evidence, builder.RUN, self.prepared['outputs'])
        self.manifest = json.loads(self.prepared['outputs']['core-retention-manifest.json'])
        self.record = copy.deepcopy(self.prepared['record'])

    def verify(self):
        return verify_reviewed_crash(builder.NAME, self.prepared['compressed'], self.record,
                                     evidence_root=self.evidence, build_root=self.build)

    def refresh(self):
        raw = builder.json_bytes(self.manifest)
        (self.run / 'core-retention-manifest.json').write_bytes(raw)
        self.record['capsule_sha256'] = sha(raw)

    def replace_role(self, role, value):
        item = next(row for row in self.manifest['files'] if row['role'] == role)
        raw = value if isinstance(value, bytes) else builder.json_bytes(value)
        (self.evidence / item['path']).write_bytes(raw)
        item.update(size=len(raw), sha256=sha(raw))
        self.refresh()

    def rejected(self, expression, label):
        with self.assertRaises((CrashRetentionError, AudioEvidenceError)) as result:
            expression()
        OBSERVED_NEGATIVES.append({'case': label, 'error': str(result.exception)})

    def test_valid_audio_capsule_needs_no_full_binary_copy(self):
        result = self.verify()
        self.assertEqual(result['mode'], 'analysis_capsule')
        self.assertEqual(result['hardware_status'], 'unresolved')
        self.assertEqual(list(self.build.iterdir()), [])
        self.assertEqual(self.prepared['checks']['functions'], 15)
        self.assertEqual(self.prepared['checks']['code_bytes'], 10484)
        self.assertEqual(self.prepared['checks']['verified_frames'], 7)
        self.assertEqual(self.prepared['checks']['direct_calls'], 5)
        self.assertFalse(self.manifest['parser_executed'])

    def test_previous_validator_rejects_the_new_schema(self):
        spec = importlib.util.spec_from_file_location('retention_before_audio_v2', BASELINE)
        old = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(old)
        with self.assertRaisesRegex(old.CrashRetentionError, 'schema') as caught:
            old.verify_reviewed_crash(builder.NAME, self.prepared['compressed'], self.record,
                                      evidence_root=self.evidence, build_root=self.build)
        OBSERVED_NEGATIVES.append({'case': 'unchanged_v1_validator_rejects_audio_v2', 'error': str(caught.exception)})

    def test_unknown_schema_or_profile_is_rejected(self):
        for field, value in (('schema', 'unreviewed-v3'), ('profile', 'allocator'), ('profile', None)):
            with self.subTest(field=field, value=value):
                before = copy.deepcopy(self.manifest)
                self.manifest[field] = value
                self.refresh()
                self.rejected(self.verify, 'unknown_' + field + '_' + str(value))
                self.manifest = before

    def test_each_required_audio_role_cannot_be_omitted(self):
        from vita_crash_retention import AUDIO_REQUIRED_ROLES
        original = copy.deepcopy(self.manifest['files'])
        for role in sorted(AUDIO_REQUIRED_ROLES):
            with self.subTest(role=role):
                self.manifest['files'] = [row for row in original if row['role'] != role]
                self.refresh()
                self.rejected(self.verify, 'missing_role_' + role)

    def test_fake_graphics_role_cannot_replace_audio_state(self):
        item = next(row for row in self.manifest['files'] if row['role'] == 'track_state')
        item['role'] = 'allocator_state'
        self.refresh()
        self.rejected(self.verify, 'fake_allocator_role')

    def test_scope_types_and_hardware_claims_are_strict(self):
        for key, value in (('immediate_fault_identified', 1), ('verified_stack_frames', True),
                           ('hardware_regression_passed', True), ('live_manager_global_captured', True),
                           ('new_addresses_require_matching_elf', False), ('lifecycle_basis', 'complete_runtime_trace')):
            with self.subTest(key=key):
                self.manifest['analysis_scope'] = dict(AUDIO_SCOPE)
                self.manifest['analysis_scope'][key] = value
                self.refresh()
                self.rejected(self.verify, 'scope_' + key)

    def test_partial_or_extra_function_is_rejected(self):
        for change in ('missing', 'extra', 'partial', 'symbol_size', 'duplicate', 'odd_address'):
            value = copy.deepcopy(self.extract)
            functions = value['functions']
            if change == 'missing':
                functions.pop()
            elif change == 'extra':
                functions.append(copy.deepcopy(functions[0]))
            elif change == 'partial':
                functions[0]['bytes_hex'] = functions[0]['bytes_hex'][:-4]
                functions[0]['sha256'] = sha(bytes.fromhex(functions[0]['bytes_hex']))
                functions[0]['size'] -= 2
            elif change == 'symbol_size':
                functions[0]['symbol']['size'] -= 2
            elif change == 'duplicate':
                functions[-1] = copy.deepcopy(functions[0])
            else:
                functions[0]['start'] += 1
            self.replace_role('elf_extract', value)
            self.rejected(self.verify, 'function_' + change)

    def test_function_payload_hash_and_segment_bounds_are_checked(self):
        for change in ('bytes', 'file_offset', 'overflow', 'oversized'):
            value = copy.deepcopy(self.extract)
            function = value['functions'][0]
            if change == 'bytes':
                function['bytes_hex'] = '00' + function['bytes_hex'][2:]
            elif change == 'file_offset':
                function['file_offset'] += 2
            elif change == 'overflow':
                function['start'] = 0xfffffffe
            else:
                function['size'] = 65536
            self.replace_role('elf_extract', value)
            self.rejected(self.verify, 'function_bound_' + change)

    def test_all_fifteen_ehabi_intervals_are_retained(self):
        records = self.extract['ehabi']
        self.assertEqual(len(records), 15)
        self.assertEqual(sum(row['extab']['limit'] - row['extab']['address'] for row in records if row['extab']), 200)
        cant = next(row for row in records if row['function_start'] == 0x810613ac)
        self.assertIsNone(cant['decoded'])
        self.assertIsNone(cant['opcodes_hex'])
        self.assertIsNone(cant['extab'])
        task = next(row for row in records if row['function_start'] == 0x811f0e50)
        self.assertEqual(len(bytes.fromhex(task['extab']['bytes_hex'])), 76)

    def test_truncated_76_byte_extab_is_rejected_after_rehash(self):
        value = copy.deepcopy(self.extract)
        row = next(row for row in value['ehabi'] if row['function_start'] == 0x811f0e50)
        extab = row['extab']
        extab['bytes_hex'] = extab['bytes_hex'][:128]
        extab['sha256'] = sha(bytes.fromhex(extab['bytes_hex']))
        self.replace_role('elf_extract', value)
        self.rejected(self.verify, 'RunAllTasks_extab_76_to_64')
        extab['limit'] = extab['address'] + 64
        self.replace_role('elf_extract', value)
        self.rejected(self.verify, 'forged_extab_boundary_64')

    def test_ehabi_start_coverage_and_opcode_claim_are_checked(self):
        for change in ('missing', 'start', 'successor', 'decoded', 'opcode'):
            value = copy.deepcopy(self.extract)
            row = value['ehabi'][0]
            if change == 'missing':
                value['ehabi'].pop()
            elif change == 'start':
                row['entry_address'] += 8
            elif change == 'successor':
                row['next_entry_hex'] = row['entry_hex']
            elif change == 'decoded':
                row['decoded']['frame_bytes'] += 4
            else:
                row['opcodes_hex'] = 'b0'
            self.replace_role('elf_extract', value)
            self.rejected(self.verify, 'EHABI_' + change)

    def test_independent_unwind_arithmetic_matches_seven_prologues(self):
        cases = [('0fc980aeb0', 104, 100), ('16c980a9b0', 112, 108), ('02a9b0', 24, 20),
                 ('02afb0', 48, 44), ('aeb0b0', 32, 28), ('c980aa', 24, 20), ('02c980adb0', 48, 44)]
        for code, size, lr in cases:
            with self.subTest(code=code):
                self.assertEqual(unwind_frame(bytes.fromhex(code)), {'frame_bytes': size, 'lr_sp_offset': lr})
        self.assertIsNone(unwind_stream(1))
        self.rejected(lambda: unwind_frame(bytes.fromhex('c982')), 'unsupported_unwind_opcode')

    def test_saved_lr_and_every_reviewed_frame_are_checked_against_core(self):
        for index in range(7):
            value = copy.deepcopy(self.extract)
            value['proof']['chain'][index]['saved_lr_runtime'] ^= 4
            self.replace_role('elf_extract', value)
            self.rejected(self.verify, 'raw_saved_LR_' + str(index))
        value = copy.deepcopy(self.extract)
        value['proof']['chain'][0]['saved_lr_slot'] += 4
        self.replace_role('elf_extract', value)
        self.rejected(self.verify, 'LR_slot_disagrees_with_EHABI')

    def test_fault_and_all_callsite_bytes_are_checked(self):
        points = [0x81060a84] + [row['callsite_elf'] for row in self.extract['proof']['chain']]
        for point in points:
            value = copy.deepcopy(self.extract)
            function = next(row for row in value['functions'] if row['start'] <= point < row['start'] + row['size'])
            code = bytearray.fromhex(function['bytes_hex'])
            code[point - function['start']] ^= 1
            function.update(bytes_hex=code.hex(), sha256=sha(code))
            self.replace_role('elf_extract', value)
            self.rejected(self.verify, 'instruction_' + hex(point))

    def test_direct_bl_is_decoded_from_instruction_bits(self):
        self.assertEqual(direct_bl_target(0x8104af60, bytes.fromhex('15f062fd')), 0x81060a28)
        self.assertEqual(direct_bl_target(0x811a5d26, bytes.fromhex('a5f661f8')), 0x8104adec)
        self.rejected(lambda: direct_bl_target(0x8104af60, b'\0\0\0\0'), 'non_BL_opcode')

    def test_runtime_relocation_and_registers_are_checked_against_raw_notes(self):
        for change in ('relocation', 'register', 'pc'):
            value = copy.deepcopy(self.extract)
            if change == 'relocation':
                value['relocations'][1]['delta'] += 0x1000
            elif change == 'register':
                value['proof']['registers'][5] = 1
            else:
                value['proof']['pc_elf'] += 2
            self.replace_role('elf_extract', value)
            self.rejected(self.verify, 'raw_notes_' + change)

    def test_raw_core_lr_mutation_is_detected_independently_of_inventory(self):
        data, segments, _, _ = core_snapshot(self.prepared['compressed'])
        changed = bytearray(data)
        slot = self.extract['proof']['chain'][0]['saved_lr_slot']
        segment = next(p for p in segments if p['type'] == 1 and p['vaddr'] <= slot < p['vaddr'] + p['filesz'])
        changed[segment['offset'] + slot - segment['vaddr']] ^= 4
        compressed = gzip.compress(changed, mtime=0)
        value = copy.deepcopy(self.extract)
        value['core']['sha256'] = sha(compressed)
        self.rejected(lambda: validate_extract(value, compressed), 'raw_core_LR_byte_changed')

    def test_memory_windows_and_uncaptured_global_limit_are_checked(self):
        for change in ('window', 'manager', 'global_claim', 'vtable'):
            value = copy.deepcopy(self.extract)
            if change == 'window':
                row = value['core_windows'][0]
                raw = bytearray.fromhex(row['bytes_hex']); raw[-1] ^= 1
                row.update(bytes_hex=raw.hex(), sha256=sha(raw))
            elif change == 'manager':
                value['proof']['manager']['table_pointer'] += 4
            elif change == 'global_claim':
                value['proof']['manager']['global_pointer_captured'] = True
            else:
                value['data_fragments'][0]['size'] -= 4
            self.replace_role('elf_extract', value)
            self.rejected(self.verify, 'memory_' + change)

    def test_track_state_and_diagnosis_are_bound_to_the_proof(self):
        track = json.loads(self.prepared['outputs']['core-retention-track-state.json'])
        track['manager']['address'] += 4
        self.replace_role('track_state', track)
        self.rejected(self.verify, 'track_state_disagrees')
        self.replace_role('track_state', self.prepared['outputs']['core-retention-track-state.json'])
        diagnosis = json.loads(self.prepared['original_inputs']['core-diagnosis.json'])
        diagnosis['fault']['pc_elf'] = '0x00000000'
        self.replace_role('diagnosis', diagnosis)
        self.rejected(self.verify, 'diagnosis_disagrees')

    def test_new_dump_bytes_are_not_acknowledged(self):
        self.rejected(lambda: verify_reviewed_crash(builder.NAME, self.prepared['compressed'] + b'new', self.record,
                                                   evidence_root=self.evidence, build_root=self.build), 'new_raw_core_sha')

    def test_builder_refuses_wrong_elf_before_any_extraction(self):
        self.rejected(lambda: builder.make_extract(b'wrong ELF', self.prepared['compressed'], {}, {}), 'builder_wrong_ELF')

    def test_parser_and_reviewed_inputs_remain_byte_exact(self):
        for name, expected in builder.INPUTS.items():
            self.assertEqual(sha((ROOT / builder.EVIDENCE / builder.RUN / name).read_bytes()), expected)
        self.assertEqual(self.manifest['full_binary_copies_created'], 0)
        self.assertFalse(self.manifest['registry_modified'])
        self.assertFalse(self.manifest['rotation_performed'])

    def test_exclusive_outputs_refuse_overwrite_before_any_new_file(self):
        old = (self.run / 'core-retention-manifest.json').read_bytes()
        with self.assertRaises(FileExistsError):
            builder.publish(self.evidence, builder.RUN, {'new-file.txt': b'new', 'core-retention-manifest.json': b'wrong'})
        self.assertFalse((self.run / 'new-file.txt').exists())
        self.assertEqual((self.run / 'core-retention-manifest.json').read_bytes(), old)

    def test_exclusive_outputs_reject_symlink_file_and_parent(self):
        link = self.run / 'alias.txt'
        target = self.root / 'target.txt'; target.write_bytes(b'keep')
        link.symlink_to(target)
        with self.assertRaises(FileExistsError):
            builder.publish(self.evidence, builder.RUN, {'alias.txt': b'wrong'})
        parent = self.evidence / 'alias'; parent.symlink_to(self.run, target_is_directory=True)
        with self.assertRaises(OSError):
            builder.publish(self.evidence, 'alias', {'new.txt': b'wrong'})
        self.assertEqual(target.read_bytes(), b'keep')

    def test_exclusive_outputs_reject_fifo_and_traversal(self):
        fifo = self.run / 'blocked.txt'; os.mkfifo(fifo)
        with self.assertRaises(FileExistsError):
            builder.publish(self.evidence, builder.RUN, {'blocked.txt': b'wrong'})
        self.rejected(lambda: builder.publish(self.evidence, '../outside', {'new.txt': b'wrong'}), 'output_traversal')
        self.rejected(lambda: builder.publish(self.evidence, builder.RUN, {'../new.txt': b'wrong'}), 'filename_traversal')

    def test_invalid_binary_bounds_fail_closed(self):
        self.rejected(lambda: take(b'1234', 3, 2), 'out_of_range_slice')
        self.rejected(lambda: take(b'1234', True, 1), 'bool_as_offset')
        self.rejected(lambda: program_headers(b'not an ELF'), 'invalid_ELF_header')
        self.rejected(lambda: core_snapshot(b'not gzip'), 'invalid_core_stream')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    start = time.monotonic()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(AudioRetentionTest)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    report = {'schema': 'strikers-audio-capsule-host-gate-v1', 'status': 'PASS' if result.wasSuccessful() else 'FAIL',
              'tests': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors),
              'seconds': time.monotonic() - start, 'expected_negatives': OBSERVED_NEGATIVES,
              'full_binary_copies_created': 0, 'original_parser_executed': False,
              'device_or_build_operations': False,
              'source_files': [{'path': str(path.relative_to(ROOT)), 'size': path.stat().st_size, 'sha256': sha(path.read_bytes())}
                               for path in (Path(__file__), Path(builder.__file__),
                                            Path(__file__).with_name('vita_crash_audio.py'),
                                            Path(__file__).with_name('vita_crash_retention.py'))]}
    if hasattr(AudioRetentionTest, 'prepared'):
        report['real_proof_checks'] = AudioRetentionTest.prepared['checks']
    if args.json:
        with args.json.open('xb') as stream:
            stream.write(builder.json_bytes(report))
    raise SystemExit(0 if result.wasSuccessful() else 1)


if __name__ == '__main__':
    main()
