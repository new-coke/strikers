#!/usr/bin/env python3
"""Check factory type layouts and execute the real TopDown constructor under ASan.

Example: python3 tools/test_camera_layout.py --output ../.tmp/camera-layout-test
Outputs must use a new directory. This never configures or builds the game.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[1])
parser.add_argument('--source', type=Path)
parser.add_argument('--compile-commands', type=Path)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--clang', default='/usr/bin/clang++')
args = parser.parse_args()
project = args.project.resolve()
source = (args.source or project/'src/Game/Camera/CameraMan.cpp').resolve()
commands = (args.compile_commands or project/'build-vita-gxm-latest/compile_commands.json').resolve()
out = args.output.resolve()
assert source.is_file() and commands.is_file()
assert not out.exists(), 'Choose a fresh output directory.'
out.mkdir(parents=True)
inc = project/'include'
text = source.read_text()
expression = re.search(r'^\s*pBaseCamera = .*sizeof\(TopDownCamera\).*;$', text, re.M)
assert expression, 'TopDown factory allocation was not found.'
expression = expression.group(0).strip()
canonical = '#include "Game/Camera/TopDownCamera.h"'
local = re.search(r'class TopDownCamera\n\{.*?\n\};', text, re.S)
assert canonical in text or local, 'Cannot resolve the factory type from its source.'
type_source = canonical if canonical in text else local.group(0)
record = {'source': str(source), 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'allocation_expression': expression, 'commands': []}

def execute(label, argv, cwd=None, env=None, timeout=90):
    run = subprocess.run(argv, cwd=cwd, env=env, capture_output=True, text=True, timeout=timeout)
    (out/(label+'.stdout.txt')).write_text(run.stdout)
    (out/(label+'.stderr.txt')).write_text(run.stderr)
    record['commands'].append({'label': label, 'argv': argv, 'returncode': run.returncode})
    (out/'result.json').write_text(json.dumps(record, indent=2)+'\n')
    return run

entries = json.loads(commands.read_text())
entry = next(e for e in entries if e['file'].endswith('/Game/Camera/CameraMan.cpp'))
base = []
skip = False
for arg in shlex.split(entry['command']):
    if skip:
        skip = False
        continue
    if arg in ('-o', '-MF', '-MT', '-MQ'):
        skip = True
        continue
    if arg in ('-c', '-MD', '-MMD', '-MP', '-ffat-lto-objects', entry['file']) or arg.startswith(('-flto', '-Wl,')):
        continue
    base.append(arg)
probe = project/'tests/camera_layout_types.cpp'
run = execute('vita_type_contract', base+['-fno-lto', '-fsyntax-only', '-DCAMERA_LAYOUT_MANAGER_SOURCE="'+str(source)+'"', str(probe)], cwd=entry['directory'])
assert run.returncode == 0, 'Complete CameraMan TU violates the canonical camera layout contract; see vita_type_contract.stderr.txt.'

prefix = '#include "Game/Camera/BaseCamera.h"\n#include <new>\nvoid* nlMalloc(size_t,unsigned int,bool);\n'
body = '\nextern "C" void* fixture_allocate_top_down() {\n cBaseCamera* pBaseCamera;\n '+expression+'\n return pBaseCamera;\n}\n'
negative_type = (project/'tests/camera_layout_legacy_stub.inc').read_text()
constructor = project/'src/Game/Camera/TopDownCamera.cpp'
record['constructor_sha256'] = hashlib.sha256(constructor.read_bytes()).hexdigest()
flags = ['-std=gnu++20', '-O1', '-g', '-fshort-wchar', '-fsigned-char', '-fno-strict-aliasing', '-fdeclspec', '-Wno-unknown-pragmas', '-Wno-macro-redefined', '-Wno-ignored-attributes', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-DTARGET_PC=1', '-DSTRIKERS_VITA_NATIVE_RUNTIME=1', '-I'+str(inc/'port/native-sdk'), '-I'+str(inc), '-I'+str(inc/'msl'), '-include', str(inc/'port/prelude.h')]
env = os.environ.copy()
env['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1'
env['UBSAN_OPTIONS'] = 'print_stacktrace=1:halt_on_error=0'
for label, declaration in [('candidate', type_source), ('negative_control', negative_type)]:
    factory = out/(label+'_factory.cpp')
    factory.write_text(prefix+declaration+body)
    binary = out/label
    run = execute(label+'_compile', [args.clang]+flags+[str(factory), str(constructor), str(project/'tests/camera_layout_driver.cpp'), '-o', str(binary)])
    assert run.returncode == 0, 'Host witness did not compile: '+label
    run = execute(label, [str(binary)], env=env, timeout=20)
    if label == 'candidate':
        assert run.returncode == 0 and not run.stderr, 'Current factory fails the real-constructor sanitizer witness.'
    else:
        assert run.returncode != 0 and 'AddressSanitizer: heap-buffer-overflow' in run.stderr, 'Historical empty-class negative control was not detected.'
record['passed'] = True
(out/'result.json').write_text(json.dumps(record, indent=2)+'\n')
print('PASS: full Vita camera type contract, real TopDown constructor and historical negative control. No device/FPS claim.')
