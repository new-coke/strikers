#!/usr/bin/env python3
"""Isolated SDK-mock integration of native multiplayer PAD and game input bodies.

Compiles pad.c as C, with the public Vita SDK header. The C++ fixture executes
the unmodified platpad body, FE input methods and FrontEnd::UpdateForGame.
Only SDK I/O, time and the surrounding world/menu objects are fixtures.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile
import time

STAGE = Path(__file__).resolve().parents[1]


def body(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    token = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]')
    depth = 0
    for match in token.finditer(text, opening):
        if match.group() == '{':
            depth += 1
        elif match.group() == '}':
            depth -= 1
            if depth == 0:
                return text[start:match.end()]
    raise ValueError('unterminated production body: '+signature)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo-root', type=Path, required=True)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--driver', type=Path, default=STAGE/'tests/native_pad_multi_driver_test.cpp')
    parser.add_argument('--runtime', type=Path, default=STAGE/'src/platform/native_runtime.cpp')
    parser.add_argument('--baseline', action='store_true')
    parser.add_argument('--sdk', type=Path, default=Path('/usr/local/vitasdk/arm-vita-eabi/include'))
    parser.add_argument('--cases', default='all')
    parser.add_argument('--expect-fail', default='')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    ROOT = args.repo_root.resolve()
    cases = args.cases.split(',')
    expected_failures = set(filter(None, args.expect_fail.split(',')))
    if len(cases) != len(set(cases)) or not expected_failures.issubset(cases):
        parser.error('cases must be unique; expected failures must be selected cases')
    if args.output.exists():
        parser.error('refusing to overwrite an existing result')
    source = (args.source or STAGE/'src/platform/pad.c').resolve()
    sources = {}
    extracted = []

    def read(path):
        path = path.resolve() if isinstance(path, Path) else (ROOT/path).resolve()
        raw = path.read_bytes()
        sources[str(path)] = hashlib.sha256(raw).hexdigest()
        return raw.decode()

    def select(path, signatures):
        text = read(path)
        selected = []
        for signature in signatures:
            chosen = body(text, signature)
            extracted.append({'source': str(path), 'signature': signature,
                              'sha256': hashlib.sha256(chosen.encode()).hexdigest()})
            selected.append(chosen)
        return '\n\n'.join(selected)+'\n'

    native = read(source)
    for signature in ['u32 PADRead(', 'void PADClampCircle(', 'static s8 vita_axis_x(',
                      'static s8 vita_axis_y(', 'BOOL PADInit(', 'int PADReset(',
                      'void PADControlMotor(', 'PADSamplingCallback PADSetSamplingCallback(',
                      'void PortInvokePadSamplingCallback(']:
        chosen = body(native, signature)
        extracted.append({'source': str(source), 'signature': signature,
                          'sha256': hashlib.sha256(chosen.encode()).hexdigest()})
    # Retain the entire declaration/function section, including private category
    # storage and VBlank's actual transfer-error early return. No body rewrite.
    plat = read('src/NL/plat/platpad.cpp')
    plat = plat[plat.index('PADStatus PadStatus::s_A[4];'):]
    extracted.append({'source': 'src/NL/plat/platpad.cpp',
                      'signature': 'complete declarations and functions after includes',
                      'sha256': hashlib.sha256(plat.encode()).hexdigest()})
    global_pad = read('src/NL/plat/globalpad.cpp')
    global_pad = global_pad[global_pad.index('cGlobalPad* cPadManager::m_aPads'):]
    extracted.append({'source': 'src/NL/plat/globalpad.cpp',
                      'signature': 'complete declarations and functions after includes',
                      'sha256': hashlib.sha256(global_pad.encode()).hexdigest()})
    fe_source = read('src/Game/FE/feInput.cpp')
    fe = body(fe_source, 'struct FEPadData')+';\nFEPadData g_aFEPadData[4];\n'
    fe += 'FEInput* g_pFEInput = nullptr;\n'
    fe += select('src/Game/FE/feInput.cpp', [
        'FEInput::FEInput()', 'void FEInput::Reset()',
        'void FEInput::EnableAnalogToDPadMapping(', 'bool FEInput::IsConnected(',
        'bool FEInput::JustPressed(', 'bool FEInput::JustReleased('])
    frontend = select('src/Game/FE/feManager.cpp', ['void FrontEnd::UpdateForGame('])
    choose_header = read('include/Game/FE/feChooseSideComponent.h')
    choose_declarations = body(read('include/Game/FE/feCaptainComponent.h'), 'enum UpdateResult')+';\n'
    for signature in ['enum Context', 'enum ControllerPos', 'class IChooseSide']:
        chosen = body(choose_header, signature)
        choose_declarations += chosen+';\n'
        extracted.append({'source': 'include/Game/FE/feChooseSideComponent.h',
                          'signature': signature, 'sha256': hashlib.sha256(chosen.encode()).hexdigest()})
    choose_source = read('src/Game/FE/feChooseSideComponent.cpp')
    choose = choose_source[choose_source.index('IChooseSide::IChooseSide()'):
                           choose_source.index('void IChooseSide::SetArrowVisible(')]
    extracted.append({'source': 'src/Game/FE/feChooseSideComponent.cpp',
                      'signature': 'complete controller/ready/team functions before arrow-render helpers',
                      'sha256': hashlib.sha256(choose.encode()).hexdigest()})
    remap = body(read('src/Game/PadActions.cpp'), 's32 g_pPadRemapArray[38]')+';\n'
    shutdown = select(args.runtime, ['void shutdown() noexcept'])
    driver = args.driver.resolve()
    read(driver)
    read(Path(__file__).resolve())
    read(STAGE/'include/port/native_pad.h')
    for relative in ['include/dolphin/pad.h', 'include/dolphin/types.h', 'include/types.h',
                     'include/NL/platpad.h', 'include/NL/globalpad.h', 'include/NL/nlTicker.h',
                     'include/Game/FE/feInput.h', 'include/port/overlay.h',
                     'include/port/prelude.h', 'include/Game/FE/feLibObject.h']:
        read(relative)
    for relative in ['psp2/ctrl.h', 'psp2common/ctrl.h', 'psp2/types.h',
                     'psp2common/types.h', 'vitasdk/build_utils.h', 'psp2/kernel/error.h']:
        read(args.sdk/relative)
    before = dict(sources)
    report = {'schema': 'strikers-native-pad-multiplayer-integration-v1', 'status': 'FAIL',
              'source': str(source), 'sources': sources, 'extracted_bodies': extracted,
              'baseline': args.baseline,
              'sanitizers': ['address', 'undefined'], 'cases': [],
              'limits': [
                  'SDK calls are deterministic mocks using the actual public SDK declarations; physical disconnect/recovery still requires device verification.',
                  'Zero is outside the documented positive PeekBuffer success range; it is handled as an unusable transfer, not identified as a timestamp event.',
                  'The entire production FrontEnd::UpdateForGame runs with world/menu action fixtures. It verifies the real controller-removal and pause predicates, not the rendered prompt or full SHPause scene.',
                  'Controller topology, SDK errors and actuator acknowledgements are mocks. No pairing, two-controller hardware or physical motor effect has been observed by this gate.',
                  'IChooseSide controller/ready/team code and its declarations are production. TLInstance, tween scheduling and audio/world endpoints are fixtures, not a rendered menu oracle.',
                  'The actual shutdown body runs with jobs/startup endpoints that verify ordering. A negative actuator stop remains explicitly unresolved; the SDK cannot guarantee motor effect after an error.',
                  'Port identity is the public port/type plus an observed detach generation. A replacement with identical metadata and no observable missing-device event cannot be distinguished by this SDK interface.',
                  'Host clock and polar math providers are fixtures. Existing PAD axis conversion/clamping run as production C and their exhaustive normal-result digest is recorded.'
              ]}
    with tempfile.TemporaryDirectory(prefix='strikers-native-pad-') as tmp:
        tmp = Path(tmp)
        for name, text in [('native_pad_plat.inc', plat), ('native_pad_global.inc', global_pad),
                           ('native_pad_fe.inc', fe), ('native_pad_frontend.inc', frontend)]:
            (tmp/name).write_text(text)
        for name, text in [('native_pad_choose_declarations.inc', choose_declarations),
                           ('native_pad_choose.inc', choose), ('native_pad_remap.inc', remap),
                           ('native_pad_shutdown.inc', shutdown)]:
            (tmp/name).write_text(text)
        cc, cxx = shutil.which('clang'), shutil.which('clang++')
        if not cc or not cxx:
            raise RuntimeError('clang and clang++ are required')
        common = ['-DTARGET_PC=1', '-DSTRIKERS_VITA=1', '-O2', '-g', '-fsigned-char',
                  '-fshort-wchar', '-fno-omit-frame-pointer', '-fsanitize=address,undefined',
                  '-fno-sanitize-recover=all', '-I'+str(STAGE/'include'),
                  '-I'+str(ROOT/'include'), '-I'+str(tmp),
                  '-idirafter', str(args.sdk)]
        if args.baseline:
            common.append('-DNATIVE_PAD_BASELINE=1')
        commands = [[cc, '-std=c11', *common, '-x', 'c', '-c', str(source), '-o', str(tmp/'pad.o')],
                    [cxx, '-std=c++20', *common, '-fdeclspec', '-Wno-unknown-pragmas',
                     '-Wno-macro-redefined', '-include', str(ROOT/'include/port/prelude.h'),
                     str(driver), str(tmp/'pad.o'), '-o', str(tmp/'native-pad-test')]]
        report['builds'] = []
        for command in commands:
            start = time.monotonic()
            result = subprocess.run(command, text=True, capture_output=True, timeout=60)
            report['builds'].append({'command': command, 'seconds': time.monotonic()-start,
                                     'returncode': result.returncode,
                                     'stdout': result.stdout, 'stderr': result.stderr})
            if result.returncode:
                break
        if all(item['returncode'] == 0 for item in report['builds']) and len(report['builds']) == 2:
            env = os.environ.copy()
            env.pop('STRIKERS_PROBE_PAD', None)
            env['ASAN_OPTIONS'] = 'detect_leaks='+('0' if platform.system() == 'Darwin' else '1')+':halt_on_error=1'
            env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
            for case in cases:
                start = time.monotonic()
                result = subprocess.run([str(tmp/'native-pad-test'), case], text=True,
                                        capture_output=True, env=env, timeout=15)
                expected_failure = case in expected_failures
                check_failed = (result.returncode == 1 and '[pad-check-failed]' in result.stderr)
                accepted = check_failed if expected_failure else result.returncode == 0
                report['cases'].append({'name': case, 'expected_failure': expected_failure,
                                        'expectation_met': accepted, 'returncode': result.returncode,
                                        'seconds': time.monotonic()-start,
                                        'stdout': result.stdout, 'stderr': result.stderr})
            if all(item['expectation_met'] for item in report['cases']):
                report['status'] = 'PASS'
    report['sources_unchanged_during_gate'] = all(
        hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest for path, digest in before.items())
    if not report['sources_unchanged_during_gate']:
        report['status'] = 'FAIL'
    with args.output.open('x') as output:
        json.dump(report, output, indent=2)
        output.write('\n')
    print(json.dumps({'status': report['status'], 'output': str(args.output),
                      'cases': [{'name': c['name'], 'expectation_met': c['expectation_met'],
                                 'returncode': c['returncode']} for c in report['cases']]}))
    if report['status'] != 'PASS':
        for result in report['builds']+report['cases']:
            if result['stderr']:
                print(result['stderr'], end='')
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
