#!/usr/bin/env python3
"""Run unchanged production task bodies with deterministic host service stubs.

The stubs supply time/cost and game state. The real Run/dispatch bodies decide
which ticks and phase boundaries execute, including early return and catch-up.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

from analyze_task_profile import read_phases, read_tasks


def function(source, signature):
    if source.count(signature) != 1:
        raise ValueError(f'Expected one production function: {signature}')
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 0
    # Ignore braces inside comments and literals while preserving every byte.
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
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    fixed = (root / 'src/Game/FixedUpdateTask.cpp').read_text()
    render = (root / 'src/Game/GameRenderTask.cpp').read_text()
    bodies = '\n\n'.join((function(fixed, 'void FixedUpdateTask::Run(float dt)'),
                          function(fixed, 'void FixedUpdateTask::CallFixedUpdateTasks()'),
                          function(render, 'void GameRenderTask::Run(float fDeltaT)')))
    fixture = (root / 'tests/game_phase_hooks_test.cpp').read_text()
    marker = '// INSERT_UNCHANGED_PRODUCTION_TASK_BODIES'
    if fixture.count(marker) != 1:
        raise ValueError('Expected one production insertion point')
    with tempfile.TemporaryDirectory(prefix='strikers-phase-hooks-') as directory:
        scratch = Path(directory)
        source, binary = scratch / 'hooks.cpp', scratch / 'hooks-test'
        capture = scratch / 'run.csv'
        source.write_text(fixture.replace(marker, bodies))
        subprocess.run([args.compiler, '-std=c++20', '-O2', '-ffp-contract=off',
                        '-fsanitize=undefined', '-fno-sanitize-recover=undefined',
                        '-DPORT_VITA=1', '-DSTRIKERS_VITA_NATIVE_RUNTIME=1',
                        '-I', str(root / 'include'), str(source),
                        str(root / 'src/platform/task_profile.cpp'), '-o', str(binary)],
                       check=True, timeout=60)
        subprocess.run([str(binary), str(capture)], check=True, timeout=30)
        tasks, names = read_tasks(Path(str(capture) + '.tasks.csv'))
        phases = read_phases(Path(str(capture) + '.phases.csv'), tasks)
        assert names == {0: 'Game Fixed Update', 1: 'Game Render'}
        assert [row['simulation']['steps'] for row in phases] == [2, 0, 16, 16, 1, 0]
        assert phases[0]['live_frame'] == 600 and phases[-1]['live_frame'] == 605
        assert phases[0]['engine_frame'] == 100 and phases[-1]['engine_frame'] == 105
        assert phases[2]['simulation']['debt_after_seconds'] == phases[3]['simulation']['debt_before_seconds']
        assert set(phases[1]['phases']) == {0, 1, 10}  # Zero ticks + render early return.
        print('Production phase hooks: actual ticks, carry debt, lockstep, early return and CSV hierarchy passed')


if __name__ == '__main__':
    main()
