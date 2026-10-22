"""Offline crash-retention gates; no device, build cache or real evidence writes."""
import ast
import hashlib
import json
import os
from pathlib import Path
import tempfile
import time
from types import SimpleNamespace
import unittest

from vita_crash_retention import (BUILD_NAMES, CAPSULE_SCHEMA, EXTRACT_SCHEMA,
                                  REQUIRED_ROLES, CrashRetentionError,
                                  parse_json, verify_reviewed_crash)


def sha(data):
    return hashlib.sha256(data).hexdigest()


class RetentionTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.evidence = self.root / 'evidence'
        self.build = self.root / 'build'
        self.run = self.evidence / 'run'
        self.run.mkdir(parents=True)
        self.build.mkdir()
        self.name = 'psp2core-123-0x456-eboot.bin.psp2dmp'
        self.dump = b'reviewed original core'
        self.elf = b'original symbols'
        self.executable = b'original executable'
        self.inventory = {'schema': 'strikers-core-offline-inventory-v1',
                          'core': {'path': '.debug/' + self.name, 'sha256': sha(self.dump)},
                          'elf': {'sha256': sha(self.elf)}}
        self.record = {'sha256': sha(self.dump), 'elf_sha256': sha(self.elf),
                       'self_sha256': sha(self.executable), 'analysis': 'run/inventory.json',
                       'diagnosis': 'run/diagnosis.txt', 'retention_mode': 'analysis_capsule',
                       'capsule': 'run/capsule.json'}
        extract = {'schema': EXTRACT_SCHEMA, 'core': {'sha256': sha(self.dump)},
                   'elf': {'sha256': sha(self.elf)}, 'self': {'sha256': sha(self.executable)}}
        files = []
        for role in sorted(REQUIRED_ROLES):
            if role == 'inventory':
                name, content = 'inventory.json', json.dumps(self.inventory).encode()
            elif role == 'elf_extract':
                name, content = 'elf_extract.json', json.dumps(extract).encode()
            else:
                name, content = role + '.txt', ('retained ' + role).encode()
            (self.run / name).write_bytes(content)
            files.append({'role': role, 'path': 'run/' + name, 'size': len(content), 'sha256': sha(content)})
        products = []
        for name in sorted(BUILD_NAMES):
            content = self.elf if name == 'strikers' else self.executable if name == 'strikers_vita.self' else name.encode()
            products.append({'name': name, 'sha256': sha(content), 'size': len(content)})
        self.capsule = {'schema': CAPSULE_SCHEMA, 'status': 'analyzed_hardware_unresolved',
                        'core': {'name': self.name, 'path': '.debug/' + self.name,
                                 'sha256': sha(self.dump), 'size': len(self.dump)},
                        'elf': {'sha256': sha(self.elf), 'size': len(self.elf)},
                        'self': {'sha256': sha(self.executable), 'size': len(self.executable)},
                        'analysis_scope': {'fault_cause_resolved': False,
                                           'hardware_regression_passed': False,
                                           'new_addresses_require_matching_elf': True},
                        'build_artifacts': products, 'files': files}
        self.publish()

    def publish(self):
        raw = json.dumps(self.capsule).encode()
        (self.run / 'capsule.json').write_bytes(raw)
        self.record['capsule_sha256'] = sha(raw)

    def verify(self, name=None, dump=None, record=None):
        return verify_reviewed_crash(name or self.name, self.dump if dump is None else dump,
                                     self.record if record is None else record,
                                     evidence_root=self.evidence, build_root=self.build)

    def full_build(self, suffix=''):
        (self.build / ('strikers' + suffix)).write_bytes(self.elf)
        (self.build / ('strikers_vita.self' + suffix)).write_bytes(self.executable)

    def test_capsule_accepts_exact_reviewed_core_without_full_build(self):
        result = self.verify()
        self.assertEqual(result['mode'], 'analysis_capsule')
        self.assertEqual(result['hardware_status'], 'unresolved')
        self.assertEqual(result['capsule_sha256'], self.record['capsule_sha256'])
        self.assertEqual(list(self.build.iterdir()), [])

    def test_full_mode_and_legacy_default_require_a_matching_pair(self):
        self.record['retention_mode'] = 'full'
        with self.assertRaisesRegex(CrashRetentionError, 'complete executable'):
            self.verify()
        self.full_build('.previous')
        self.assertEqual(self.verify()['mode'], 'full')
        del self.record['retention_mode']
        self.assertEqual(self.verify()['mode'], 'full')
        (self.build / 'strikers.previous').rename(self.build / 'strikers')
        with self.assertRaises(CrashRetentionError):
            self.verify()

    def test_new_name_or_new_sha_is_never_acknowledged(self):
        with self.assertRaisesRegex(CrashRetentionError, 'New Strikers crash'):
            verify_reviewed_crash('psp2core-new.psp2dmp', self.dump, None,
                                  evidence_root=self.evidence, build_root=self.build)
        with self.assertRaisesRegex(CrashRetentionError, 'different dump'):
            self.verify(name='psp2core-new.psp2dmp')
        with self.assertRaisesRegex(CrashRetentionError, 'New Strikers crash'):
            self.verify(dump=self.dump + b'new data')

    def test_missing_or_altered_capsule_is_rejected(self):
        path = self.run / 'capsule.json'
        path.write_bytes(path.read_bytes() + b' ')
        with self.assertRaisesRegex(CrashRetentionError, 'capsule hash'):
            self.verify()
        path.unlink()
        with self.assertRaises(CrashRetentionError):
            self.verify()

    def test_missing_or_altered_retained_evidence_is_rejected(self):
        path = self.run / 'disassembly.txt'
        path.write_bytes(b'different instructions')
        with self.assertRaisesRegex(CrashRetentionError, 'evidence changed'):
            self.verify()
        path.unlink()
        with self.assertRaises(CrashRetentionError):
            self.verify()

    def test_unsafe_capsule_paths_are_rejected(self):
        for path in ('../capsule.json', '/tmp/capsule.json', 'run/../capsule.json',
                     'run//capsule.json', 'run\\capsule.json', 'C:foo', './run/capsule.json'):
            with self.subTest(path=path):
                self.record['capsule'] = path
                with self.assertRaises(CrashRetentionError):
                    self.verify()

    def test_unsafe_evidence_path_is_rejected_even_with_rehashed_manifest(self):
        self.capsule['files'][0]['path'] = '../outside.txt'
        self.publish()
        with self.assertRaisesRegex(CrashRetentionError, 'Unsafe'):
            self.verify()

    def test_symlink_file_and_parent_are_rejected(self):
        path = self.run / 'disassembly.txt'
        target = self.root / 'instructions.txt'
        path.rename(target)
        path.symlink_to(target)
        with self.assertRaises(CrashRetentionError):
            self.verify()
        path.unlink()
        target.rename(path)
        alias = self.evidence / 'alias'
        alias.symlink_to(self.run, target_is_directory=True)
        self.record['capsule'] = 'alias/capsule.json'
        with self.assertRaises(CrashRetentionError):
            self.verify()

    def test_wrong_identity_or_retention_schema_is_rejected(self):
        for field in ('elf', 'self', 'core'):
            original = self.capsule[field]['sha256']
            self.capsule[field]['sha256'] = 'a' * 64
            self.publish()
            with self.subTest(field=field), self.assertRaises(CrashRetentionError):
                self.verify()
            self.capsule[field]['sha256'] = original
        self.capsule['schema'] = 'future-unsupported'
        self.publish()
        with self.assertRaisesRegex(CrashRetentionError, 'schema'):
            self.verify()

    def test_nonregular_file_is_rejected_without_waiting_for_a_fifo_writer(self):
        path = self.run / 'disassembly.txt'
        path.unlink()
        os.mkfifo(path)
        with self.assertRaisesRegex(CrashRetentionError, 'regular file'):
            self.verify()

    def test_inventory_binding_cannot_be_changed(self):
        self.inventory['elf']['sha256'] = 'b' * 64
        (self.run / 'inventory.json').write_text(json.dumps(self.inventory))
        with self.assertRaisesRegex(CrashRetentionError, 'identity mismatch'):
            self.verify()

    def test_mode_and_unresolved_status_are_explicit(self):
        self.record['retention_mode'] = 'ignore'
        with self.assertRaisesRegex(CrashRetentionError, 'retention mode'):
            self.verify()
        self.record['retention_mode'] = 'analysis_capsule'
        self.capsule['analysis_scope']['hardware_regression_passed'] = True
        self.publish()
        with self.assertRaisesRegex(CrashRetentionError, 'analysis limits'):
            self.verify()

    def test_incomplete_duplicate_or_oversized_file_records_are_rejected(self):
        original = list(self.capsule['files'])
        for change in ('missing', 'duplicate', 'oversized'):
            self.capsule['files'] = json.loads(json.dumps(original))
            if change == 'missing':
                self.capsule['files'].pop()
            elif change == 'duplicate':
                self.capsule['files'][-1] = self.capsule['files'][0]
            else:
                self.capsule['files'][0]['size'] = 1024 * 1024 + 1
            self.publish()
            with self.subTest(change=change), self.assertRaises(CrashRetentionError):
                self.verify()

    def test_duplicate_json_keys_and_nonfinite_values_are_rejected(self):
        for data in (b'{"schema":1,"schema":2}', b'{"value":NaN}'):
            with self.subTest(data=data), self.assertRaises(CrashRetentionError):
                parse_json(data)

    def test_check_dumps_uses_capsule_and_still_stops_for_a_new_core(self):
        # Compile only the production function, avoiding launcher import-time
        # baseline setup and all network-capable command paths.
        source = Path(__file__).resolve().parents[2] / '.tmp/safe-script-native-device-rgba-20261018.py'
        tree = ast.parse(source.read_text())
        function = next(x for x in tree.body if isinstance(x, ast.FunctionDef) and x.name == 'check_dumps')
        baseline = {'initial_dumps': ['psp2core-before.psp2dmp']}
        before = json.dumps(baseline, sort_keys=True)
        registry = self.evidence / 'reviewed-crashes.json'
        registry.write_text(json.dumps({self.name: self.record}))
        listing = [self.name, baseline['initial_dumps'][0]]
        transport = SimpleNamespace(cwd=lambda _: None, nlst=lambda: listing)
        output = self.evidence / 'result'
        output.mkdir()
        namespace = {'Path': Path, 'json': json, 'time': time, 'ROOT': self.root,
                     'OUT': self.evidence, 'BUILD': self.build, 'IDENTITY': baseline,
                     'device': SimpleNamespace(read=lambda *args: self.dump),
                     'classify': lambda _: {'matches_title': True},
                     'save_json': lambda p, v: p.write_text(json.dumps(v)),
                     'verify_reviewed_crash': verify_reviewed_crash}
        exec(compile(ast.Module(body=[function], type_ignores=[]), str(source), 'exec'), namespace)
        self.assertEqual(namespace['check_dumps'](transport, output), [])
        result = json.loads((output / 'dump-check.json').read_text())
        self.assertEqual(result['reviewed_strikers'], [self.name])
        self.assertEqual(result['reviewed_retention'][self.name]['mode'], 'analysis_capsule')
        self.assertEqual((self.root / '.debug' / self.name).read_bytes(), self.dump)
        listing.append('psp2core-new.psp2dmp')
        with self.assertRaisesRegex(CrashRetentionError, 'New Strikers crash'):
            namespace['check_dumps'](transport, output)
        self.assertEqual(json.dumps(baseline, sort_keys=True), before)


if __name__ == '__main__':
    unittest.main()
