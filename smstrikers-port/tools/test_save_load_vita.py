#!/usr/bin/env python3
"""Exercise production MemCard and save-load callbacks with the Vita CARD backend.

Only GameInfo, platform services and injected I/O failures are host adapters.
The callback bodies are copied unchanged from SaveLoad.cpp for each run.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, signature):
    if source.count(signature) != 1:
        raise ValueError(f'Expected one production function: {signature}')
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 0
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
    for token in tokens.finditer(source, opening):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return source[start:token.end()]
    raise ValueError(f'Unclosed production function: {signature}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='clang++')
    parser.add_argument('--sdk-include', required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = (root / 'src/Game/DB/SaveLoad.cpp').read_text()
    signatures = [
        'inline LoadCallbacks::LoadCallbacks()',
        'inline SaveCallbacks::SaveCallbacks()',
        'static void ConstructIconCfg(',
        'static unsigned long FinishLoadError(',
        'inline unsigned long LoadCallbacks::LoadIconDataDoneCB(',
        'unsigned long LoadCallbacks::ReadDoneCB(',
        'inline unsigned long LoadCallbacks::CardMountCB(',
        'bool SaveLoad::CardBusy()',
        'long SaveLoad::StartLoad(',
        'inline unsigned long FileExistsCallbacks::CardMountCB(',
        'long SaveLoad::StartFileExistsCheck(',
        'int SaveLoad::GetSaveBlockSize(',
        'u8 SaveLoad::HasEnoughFreeSpace(',
        'void SaveLoad::FreeAllCallbackMemory()',
    ]
    bodies = '\n\n'.join(function(source, name) for name in signatures)
    harness = (root / 'tests/card_vita_driver_test.cpp').read_text().split('int main()\n{')[0]
    fixture = (root / 'tests/save_load_vita_test.cpp').read_text()
    assert fixture.count('// INSERT_CARD_HOST_HARNESS') == 1
    assert fixture.count('// INSERT_UNCHANGED_SAVE_LOAD_BODIES') == 1
    fixture = fixture.replace('// INSERT_CARD_HOST_HARNESS', harness)
    fixture = fixture.replace('// INSERT_UNCHANGED_SAVE_LOAD_BODIES', bodies)
    with tempfile.TemporaryDirectory(prefix='strikers-save-load-') as directory:
        scratch = Path(directory)
        cpp, binary = scratch / 'save-load.cpp', scratch / 'save-load-test'
        cpp.write_text(fixture)
        subprocess.run([args.compiler, '-std=c++20', '-O1', '-g', '-UNDEBUG',
                        '-fdeclspec', '-fshort-wchar', '-Wno-register', '-Wno-ignored-attributes',
                        '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                        '-DTARGET_PC=1', '-DSTRIKERS_CARD_TEST=1',
                        '-I', str(root), '-I', str(root / 'include'),
                        '-idirafter', args.sdk_include, str(cpp), '-o', str(binary)],
                       check=True, timeout=60)
        subprocess.run([str(binary), str(scratch / 'cards')], check=True, timeout=30)


if __name__ == '__main__':
    main()
