#!/usr/bin/env python3
"""Retain the reviewed candidate7 audio proof; never rotate or edit a registry.

The exact original core and all five original build products are prerequisites.
All generated files are exclusive, bounded text/JSON outputs in the core7
dossier. The original parser is retained as provenance and is never executed.
"""
import argparse
import bisect
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import struct
import subprocess

from vita_crash_audio import (PROFILE, core_snapshot, elf_header, memory, prel31,
                              program_headers, require, sections, sha, symbols,
                              take, u32, unwind_frame, unwind_stream, validate_extract)
from vita_crash_retention import (AUDIO_SCOPE, BUILD_NAMES, CAPSULE_SCHEMA_V2,
                                  EXTRACT_SCHEMA, parse_json, read_regular,
                                  verify_reviewed_crash)


RUN = 'native-candidate7-watchdog-1'
EVIDENCE = 'docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18'
BUILD = 'smstrikers-port/build-vita-gxm-latest'
NAME = 'psp2core-1792351328-0x000240247b-eboot.bin.psp2dmp'
CORE_SHA = 'd0dc77803c5d131d2f4f60d5a4d0f5eea5fbb84a3e074f8fd80d06762f081ca2'
ELF_SHA = '1b6422b3075475042dc447f7cfcb12a9a380df1042113f7a7f35e181237d9446'
SELF_SHA = 'ce82e093e411deadbfc333cf72aa3c8992ca1563ca3aa947dd91b858ea6ae924'
PRODUCTS = (
    ('strikers', 9154072, ELF_SHA),
    ('strikers.velf', 10794062, '37d43f3e666f8a86f9da05f10507238e8453061f23b37f4ee6470c9a515661ff'),
    ('strikers.map', 8471015, 'eab3ff0679377622045cc479f033d6794b5b5f0894333386f5d7e4f4ed40d0a6'),
    ('strikers_vita.self', 2452134, SELF_SHA),
    ('strikers_vita.vpk', 2985981, '9f7a1635c496632a9e6543cb985ab7258be1aaa5dca6d9b799c2cef2b4c62942'),
)
# Exact STT_FUNC bounds from the preserved ELF, not the historical disassembly
# window starts (TitleScene starts at 0x811a5d1c, not four bytes before it).
TARGETS = (
    (0x81060a28, 980, 'PlayStream'),
    (0x8104adec, 624, 'PlayFETitleMusicWithFade'),
    (0x811a5d1c, 212, 'TitleSceneCreated'),
    (0x81102ae0, 624, 'LoadScene'),
    (0x81102d50, 318, 'ProcessPushPopQueue'),
    (0x81102e90, 80, 'SceneManagerUpdate'),
    (0x8110b79c, 534, 'FrontEndTaskRun'),
    (0x811f0e50, 1656, 'RunAllTasks'),
    (0x810613ac, 94, 'GetTrack'),
    (0x811bd388, 838, 'InitializeFEState'),
    (0x8104b92c, 366, 'LoadFE'),
    (0x8104bc78, 924, 'UnloadInGame'),
    (0x811bdb04, 1246, 'StateTransition'),
    (0x811bc9f8, 1524, 'InitializeGameState'),
    (0x810614e8, 464, 'DestroyAllTracks'),
)
INPUTS = {
    'core-offline-inventory.json': 'dd90b80402b95fbeedfb2adbf844aee785d8f8d5c6b884719a5174d376a121d0',
    'core-diagnosis.json': 'f37cd2983e2faef341623c9cea095abf946cd0090ba9d1f0c41b643796ec56e5',
    'core-diagnosis.md': '0019a4a07274994cb57d54d5aaa34d606f6678c35333dfe063cd4977bce494e1',
    'core-verified-call-chain.txt': 'af125ca7a77f93235a5ccfa86dbbd93cfa59e92d0edad66955dd262aba360372',
    'core-track-candidate-memory.json': '5eeb17569034d5c4f143b74d416c96865d701f52f7a8b8327b014c3fb8991283',
    'core-track-memory.json': '86926b1a4473351720ba33e318397f849e4be2115a5a19fe78eb4b8297fb983e',
    'parse-core-evidence.py': '28132a54e545b261b7cfd6c0bca39a4699830fc62b14291de4f436243627b8cc',
    'run-identity.json': '95a462d32335a6347f6a2505e24de1cbbb98d903b0e53937cd7ff0a9b303f21e',
    'ui-review.json': '525dd74e2d2297cd8a8e8199459cc37048e45a8a34515f9365edf02b1069450c',
}
GENERATED = ('core-retention-elf.json', 'core-retention-disassembly.txt',
             'core-retention-ehabi.txt', 'core-retention-track-state.json',
             'core-retention-manifest.json', 'core-retention-proposed-record.json')


def json_bytes(value):
    return (json.dumps(value, indent=2, allow_nan=False) + '\n').encode('ascii')


def run_tool(arguments):
    result = subprocess.run(arguments, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, timeout=30)
    require(result.returncode == 0 and len(result.stdout) <= 4 * 1024 * 1024,
            'Bounded SDK inspection failed: ' + result.stderr[:1024])
    return result.stdout


def make_extract(elf, compressed, inventory, diagnosis):
    require(len(elf) == 9154072 and sha(elf) == ELF_SHA, 'Expected exact candidate7 ELF')
    require(len(compressed) == 245936 and sha(compressed) == CORE_SHA, 'Expected exact candidate7 core')
    ph, sec = program_headers(elf), sections(elf)
    syms = symbols(elf, sec)
    by_name = {row['name']: row for row in sec}
    functions = []
    for address, size, label in TARGETS:
        found = [row for row in syms if row['info'] & 15 == 2 and (row['value'] & ~1) == address]
        require(len(found) == 1 and found[0]['size'] == size and found[0]['value'] & 1,
                'Original full-function bounds changed: ' + label)
        code = memory(elf, ph, address, size)
        segment = next(row for row in ph if row['type'] == 1 and row['vaddr'] <= address < row['vaddr'] + row['filesz'])
        functions.append({'label': label, 'start': address, 'size': size,
                          'file_offset': segment['offset'] + address - segment['vaddr'],
                          'symbol': found[0], 'bytes_hex': code.hex(), 'sha256': sha(code)})
    exidx, extab = by_name['.ARM.exidx'], by_name['.ARM.extab']
    require(exidx['size'] % 8 == 0, 'Invalid exidx extent')
    entries, references = [], {}
    for offset in range(0, exidx['size'], 8):
        address = exidx['addr'] + offset
        raw = memory(elf, ph, address, 8)
        function, word = struct.unpack('<II', raw)
        entries.append({'address': address, 'start': address + prel31(function),
                        'word': word, 'raw_hex': raw.hex()})
        if word != 1 and not word & 0x80000000:
            ea = address + 4 + prel31(word)
            require(extab['addr'] <= ea < extab['addr'] + extab['size'], 'Extab reference outside section')
            references.setdefault(ea, entries[-1])
    starts = [row['start'] for row in entries]
    require(starts == sorted(starts) and len(starts) == len(set(starts)), 'Unordered or duplicate exidx starts')
    extab_starts = sorted(references)
    retained = []
    for function in functions:
        index = bisect.bisect_right(starts, function['start']) - 1
        require(0 <= index < len(entries) - 1, 'Missing next exidx boundary')
        entry, following = entries[index:index + 2]
        require(entry['start'] == function['start'] and function['start'] + function['size'] <= following['start'],
                'EHABI interval does not cover the complete function')
        row = {'function_start': function['start'], 'entry_address': entry['address'],
               'entry_hex': entry['raw_hex'], 'next_entry_hex': following['raw_hex'],
               'covered_until': following['start'], 'extab': None}
        data = None
        if entry['word'] != 1 and not entry['word'] & 0x80000000:
            address = entry['address'] + 4 + prel31(entry['word'])
            next_index = bisect.bisect_right(extab_starts, address)
            require(next_index < len(extab_starts), 'Missing next referenced extab boundary')
            limit = extab_starts[next_index]
            require(0 < limit - address <= 4096, 'Oversized extab span')
            data = memory(elf, ph, address, limit - address)
            next_reference = references[limit]
            row['extab'] = {'address': address, 'limit': limit, 'bytes_hex': data.hex(), 'sha256': sha(data),
                            'next_reference_address': next_reference['address'],
                            'next_reference_hex': next_reference['raw_hex'],
                            'boundary_basis': 'Complete span through the next referenced extab address; may include alignment padding.'}
        stream = unwind_stream(entry['word'], data)
        row['opcodes_hex'] = stream.hex() if stream is not None else None
        row['decoded'] = unwind_frame(stream)
        retained.append(row)
    require(next(row for row in retained if row['function_start'] == 0x811f0e50)['extab']['limit']
            - next(row for row in retained if row['function_start'] == 0x811f0e50)['extab']['address'] == 76,
            'RunAllTasks must retain the complete 76-byte extab span')

    data, core_ph, _, threads = core_snapshot(compressed)
    fault = next(row for row in inventory['threads'] if row['uid'] == '0x40010003')
    registers = [int(fault['registers']['sp' if index == 13 else 'lr' if index == 14 else 'pc' if index == 15 else 'r' + str(index)], 16)
                 for index in range(16)]
    require(registers == threads[0x40010003]['registers'], 'Inventory registers differ from raw core')
    frames = []
    require(len(diagnosis['verified_call_chain']) == 7, 'Reviewed chain length changed')
    for index, original in enumerate(diagnosis['verified_call_chain']):
        address = int(original['callsite_elf'], 16)
        count = len(bytes.fromhex(original['call_bytes_le']))
        instruction = memory(elf, ph, address, count)
        require(instruction.hex() == original['call_bytes_le'], 'Reviewed callsite bytes changed')
        frames.append({'function_start': TARGETS[index][0], 'label': TARGETS[index][2],
                       'sp': int(original['sp'], 16), 'frame_bytes': original['frame_bytes'],
                       'saved_lr_slot': int(original['saved_lr_slot'], 16),
                       'saved_lr_runtime': int(original['saved_lr_runtime'], 16),
                       'callsite_elf': address, 'call_size': count, 'call_bytes_hex': instruction.hex(),
                       'basis': 'Seven manually reviewed frames; five BL targets and two BLX return relationships.'})
    manager = diagnosis['manager_evidence']
    proof_manager = {'address': int(manager['candidate_address'], 16),
                     'caller_slot': int(manager['caller_saved_candidate_slot'], 16),
                     'vptr_runtime': int(manager['vptr_runtime'], 16),
                     'vtable_elf': int(manager['vtable_elf_symbol'], 16),
                     'table_pointer': int(manager['table_pointer'], 16),
                     'global_pointer_runtime': int(manager['global_pointer_runtime'], 16),
                     'global_pointer_captured': manager['global_pointer_captured']}
    windows = []
    for address, size, label in ((0x8cda1a60, 512, 'candidate_manager'), (0x818c0b88, 392, 'seven_frame_stack')):
        raw = memory(data, core_ph, address, size)
        windows.append({'address': address, 'size': size, 'label': label, 'bytes_hex': raw.hex(), 'sha256': sha(raw)})
    selected_symbols = []
    for address in (0x81312750, 0x81378734, 0x812baa90):
        found = [row for row in syms if (row['value'] & ~1) == address and row['size'] > 0]
        require(len(found) == 1, 'Ambiguous retained supporting symbol')
        selected_symbols.append(found[0])
    vtable = memory(elf, ph, 0x81312750, 40)
    extract = {'schema': EXTRACT_SCHEMA, 'profile': PROFILE,
               'core': {'name': NAME, 'path': '.debug/' + NAME, 'size': len(compressed), 'sha256': CORE_SHA},
               'elf': {'size': len(elf), 'sha256': ELF_SHA},
               'self': {'size': 2452134, 'sha256': SELF_SHA},
               'elf_header_hex': elf_header(elf).hex(), 'program_headers': ph,
               'functions': functions, 'code_bytes': sum(row['size'] for row in functions),
               'ehabi_sections': {'exidx': exidx, 'extab': extab}, 'ehabi': retained,
               'supporting_symbols': selected_symbols,
               'data_fragments': [{'address': 0x81312750, 'size': 40, 'label': 'TrackManager3_vtable',
                                   'bytes_hex': vtable.hex(), 'sha256': sha(vtable)}],
               'relocations': [{key: int(row[key], 16) for key in ('runtime_start', 'elf_start', 'runtime_size', 'elf_filesz', 'elf_memsz', 'delta')}
                               for row in inventory['relocations']],
               'proof': {'thread_uid': 0x40010003, 'stop_reason': 0x00030004,
                         'registers': registers, 'pc_elf': 0x81060a84, 'this_move_elf': 0x81060a3c,
                         'null_argument_slot': 0x818c0c00, 'chain': frames, 'manager': proof_manager},
               'core_windows': windows,
               'limits': ['Only the seven declared frames are verified; other inventory stack addresses remain scan candidates.',
                          'Indirect calls preserve the reviewed BLX/return relationship; a complete virtual-call execution trace is unavailable.',
                          'The candidate manager global was not captured; the saved local alone does not prove the live global value.',
                          'The register-note tail is not treated as a confirmed DFAR layout.',
                          'EHABI bytes and referenced personality identity are retained for this bounded decode, not execution of exception handlers.',
                          'New addresses or other builds require their exact matching ELF; retention does not establish hardware fix success.']}
    require(extract['code_bytes'] == 10484, 'Unexpected complete-function byte total')
    return extract, validate_extract(extract, compressed)


def collect(root, sdk):
    """Prepare every byte and check in memory before any evidence write."""
    root, sdk = Path(root), Path(sdk)
    evidence, build = root / EVIDENCE, root / BUILD
    inputs, identities = {}, []
    for name, expected in INPUTS.items():
        raw = read_regular(evidence, RUN + '/' + name, 1024 * 1024)
        require(sha(raw) == expected, 'Reviewed input changed: ' + name)
        inputs[name] = raw
        identities.append({'path': RUN + '/' + name, 'size': len(raw), 'sha256': sha(raw)})
    compressed = read_regular(root, '.debug/' + NAME, 64 * 1024 * 1024)
    require(sha(compressed) == CORE_SHA, 'Raw core identity changed')
    products, elf = [], None
    require({row[0] for row in PRODUCTS} == BUILD_NAMES, 'Incomplete pinned product set')
    for name, size, expected in PRODUCTS:
        raw = read_regular(build, name + '.previous', 64 * 1024 * 1024)
        require(len(raw) == size and sha(raw) == expected, 'Preserved candidate7 artifact changed: ' + name)
        products.append({'name': name, 'size': size, 'sha256': expected})
        if name == 'strikers':
            elf = raw
    registry_bytes = read_regular(evidence, 'reviewed-crashes.json', 256 * 1024)
    registry = parse_json(registry_bytes)
    require(NAME in registry and registry[NAME].get('retention_mode', 'full') == 'full', 'Core7 must still be retained in full mode')
    prior = verify_reviewed_crash(NAME, compressed, registry[NAME], evidence_root=evidence, build_root=build)
    require(prior['self_sha256'] == SELF_SHA and prior['elf_sha256'] == ELF_SHA, 'Reviewed core7 build identity mismatch')
    extract, checks = make_extract(elf, compressed, parse_json(inputs['core-offline-inventory.json']),
                                   parse_json(inputs['core-diagnosis.json']))
    output = {}
    output['core-retention-elf.json'] = json_bytes(extract)
    disassembly = []
    elf_path = build / 'strikers.previous'
    for address, size, label in TARGETS:
        text = run_tool([str(sdk / 'arm-vita-eabi-objdump'), '-d', '-C', '--start-address=' + hex(address),
                         '--stop-address=' + hex(address + size), str(elf_path)])
        instructions = [int(match.group(1), 16) for line in text.splitlines()
                        if (match := re.match(r'\s*([0-9a-fA-F]+):\s', line))]
        require(instructions and instructions[0] == address and all(address <= x < address + size for x in instructions)
                and 'out of bounds' not in text.lower(), 'SDK disassembly escaped the exact function extent')
        disassembly += ['# ' + label + ' / complete original STT_FUNC', text]
    output['core-retention-disassembly.txt'] = '\n'.join(disassembly).encode('ascii')
    unwind_text = run_tool([str(sdk / 'arm-vita-eabi-readelf'), '--unwind', str(elf_path)])
    selected, active, covered = [], False, set()
    for line in unwind_text.splitlines():
        match = re.match(r'0x([0-9a-fA-F]+) ', line)
        if match:
            address = int(match.group(1), 16)
            active = address in {row[0] for row in TARGETS}
            if active:
                covered.add(address)
        if active:
            selected.append(line)
    require(covered == {row[0] for row in TARGETS}, 'SDK EHABI decode omitted a selected function')
    output['core-retention-ehabi.txt'] = ('Selected original ELF7 EHABI entries; CANTUNWIND is preserved.\n\n'
                                            + '\n'.join(selected) + '\n').encode('ascii')
    track = {'schema': 'strikers-audio-track-state-v1', 'profile': PROFILE,
             'core': extract['core'], 'manager': extract['proof']['manager'], 'windows': extract['core_windows'],
             'original_inputs': [row for row in identities if 'track-' in row['path']],
             'limit': 'The captured zero-count object is a candidate; the live manager global is absent from core memory.'}
    output['core-retention-track-state.json'] = json_bytes(track)
    role_names = {'inventory': 'core-offline-inventory.json', 'diagnosis': 'core-diagnosis.json',
                  'call_chain': 'core-verified-call-chain.txt', 'parser': 'parse-core-evidence.py',
                  'diagnosis_notes': 'core-diagnosis.md', 'run_identity': 'run-identity.json', 'ui_review': 'ui-review.json',
                  'elf_extract': 'core-retention-elf.json', 'disassembly': 'core-retention-disassembly.txt',
                  'ehabi_decode': 'core-retention-ehabi.txt', 'track_state': 'core-retention-track-state.json'}
    files = []
    for role, name in role_names.items():
        raw = output[name] if name in output else inputs[name]
        require(0 < len(raw) <= 1024 * 1024, 'Oversized retained evidence file')
        files.append({'role': role, 'path': RUN + '/' + name, 'size': len(raw), 'sha256': sha(raw)})
    require(sum(row['size'] for row in files) <= 2 * 1024 * 1024, 'Capsule evidence exceeds total bound')
    tool_sources = []
    for name in ('build_vita_audio_crash_capsule.py', 'vita_crash_audio.py', 'vita_crash_retention.py'):
        relative = 'smstrikers-port/tools/' + name
        raw = read_regular(root, relative, 1024 * 1024)
        tool_sources.append({'path': relative, 'size': len(raw), 'sha256': sha(raw)})
    manifest = {'schema': CAPSULE_SCHEMA_V2, 'profile': PROFILE, 'status': 'analyzed_hardware_unresolved',
                'created_utc': datetime.now(timezone.utc).isoformat(),
                'core': extract['core'], 'elf': extract['elf'], 'self': extract['self'],
                'analysis_scope': dict(AUDIO_SCOPE), 'build_artifacts': products, 'files': files,
                'creation_checks': checks, 'original_inputs': identities, 'source_tools': tool_sources,
                'sdk': {'objdump': run_tool([str(sdk / 'arm-vita-eabi-objdump'), '--version']).splitlines()[0],
                        'readelf': run_tool([str(sdk / 'arm-vita-eabi-readelf'), '--version']).splitlines()[0]},
                'full_binary_copies_created': 0, 'registry_modified': False, 'rotation_performed': False,
                'parser_executed': False,
                'limits': extract['limits']}
    output['core-retention-manifest.json'] = json_bytes(manifest)
    proposed = dict(registry[NAME])
    proposed.update(retention_mode='analysis_capsule', diagnosis=RUN + '/core-diagnosis.json',
                    capsule=RUN + '/core-retention-manifest.json',
                    capsule_sha256=sha(output['core-retention-manifest.json']))
    output['core-retention-proposed-record.json'] = json_bytes({'name': NAME, 'record': proposed,
                                                               'requires_root_review': True,
                                                               'registry_before_sha256': sha(registry_bytes)})
    require(set(output) == set(GENERATED), 'Unexpected generated output set')
    # Inputs used by external SDK tools and the registry must still be the
    # original bytes; no output is published until this check succeeds.
    require(sha(read_regular(build, 'strikers.previous', 64 * 1024 * 1024)) == ELF_SHA,
            'ELF changed during SDK inspection')
    require(read_regular(evidence, 'reviewed-crashes.json', 256 * 1024) == registry_bytes,
            'Registry changed during capsule preparation')
    return {'outputs': output, 'original_inputs': inputs, 'record': proposed, 'checks': checks,
            'compressed': compressed, 'manifest': manifest, 'registry_bytes': registry_bytes}


def publish(evidence_root, relative_directory, outputs):
    """Exclusive openat writes, rejecting links and pre-existing output sets."""
    parts = relative_directory.split('/')
    require(parts and all(part not in ('', '.', '..') and '\\' not in part and ':' not in part for part in parts),
            'Unsafe output directory')
    require(outputs and all(isinstance(name, str) and '/' not in name and '\\' not in name
                            and name not in ('', '.', '..') and ':' not in name for name in outputs), 'Unsafe output filename')
    require(all(isinstance(data, bytes) and 0 < len(data) <= 1024 * 1024 for data in outputs.values())
            and sum(map(len, outputs.values())) <= 2 * 1024 * 1024, 'Oversized exclusive output set')
    directory = os.open(evidence_root, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        for part in parts:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=directory)
            os.close(directory)
            directory = child
        for name in outputs:
            try:
                os.stat(name, dir_fd=directory, follow_symlinks=False)
            except FileNotFoundError:
                continue
            raise FileExistsError('Refusing to replace existing capsule output: ' + name)
        for name, data in outputs.items():
            fd = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=directory)
            with os.fdopen(fd, 'wb') as stream:
                require(stream.write(data) == len(data), 'Incomplete capsule write')
                stream.flush()
                os.fsync(stream.fileno())
        os.fsync(directory)
    finally:
        os.close(directory)
    for name, data in outputs.items():
        require(read_regular(evidence_root, relative_directory + '/' + name, 1024 * 1024) == data, 'Exclusive output readback mismatch')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--sdk', type=Path, default=Path('/usr/local/vitasdk/bin'))
    args = parser.parse_args()
    evidence = args.root / EVIDENCE
    # Fail before expensive work if any final output already exists.
    for name in GENERATED:
        path = evidence / RUN / name
        require(not path.exists() and not path.is_symlink(), 'Capsule outputs already exist; verify instead of rebuilding')
    prepared = collect(args.root, args.sdk)
    publish(evidence, RUN, prepared['outputs'])
    result = verify_reviewed_crash(NAME, prepared['compressed'], prepared['record'],
                                   evidence_root=evidence, build_root=args.root / BUILD)
    require(read_regular(evidence, 'reviewed-crashes.json', 256 * 1024) == prepared['registry_bytes'], 'Registry changed while publishing')
    print(json.dumps({'status': 'PASS', 'retention': result, 'checks': prepared['checks'],
                      'manifest': RUN + '/core-retention-manifest.json',
                      'source_and_build_modified': False, 'registry_modified': False,
                      'full_binary_copies_created': 0}, indent=2))


if __name__ == '__main__':
    main()
