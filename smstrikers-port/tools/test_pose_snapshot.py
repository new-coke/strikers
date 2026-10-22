#!/usr/bin/env python3
"""Run the real snapshot/PoseAccumulator bodies against the former full copy.

The fixture replaces world objects and Replay transport, not pose, vector,
matrix, head-callback, Grab, Blend, EvaluateFrom, or Replay control flow.
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

ROOT = Path(__file__).resolve().parents[1]

def body(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    # Ignore braces inside comments and string/character literals.
    token = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]')
    depth = 0
    for match in token.finditer(text, opening):
        if match.group() == '{':
            depth += 1
        elif match.group() == '}':
            depth -= 1
            if depth == 0:
                return text[start:match.end()]
    raise ValueError(f'unterminated body: {signature}')

def run(args):
    source = args.source.resolve() if args.source else ROOT/'src/Game/Drawable/DrawableCharacter.cpp'
    touched = {}
    extracted = []
    def read(path):
        p = path if isinstance(path, Path) else ROOT/path
        text = p.read_text()
        touched[str(p)] = hashlib.sha256(text.encode()).hexdigest()
        return text
    def extract(path, signatures):
        text = read(path)
        result = []
        for signature in signatures:
            selected = body(text, signature)
            extracted.append({'source': str(path), 'signature': signature,
                              'sha256': hashlib.sha256(selected.encode()).hexdigest()})
            result.append(selected)
        return '\n\n'.join(result)+'\n'

    drawable = read(source)
    declarations = body(read('include/Game/Drawable/DrawableCharacter.h'), 'class DrawableCharacter\n')+';\n'
    foundation = '#include "dolphin/mtx.h"\n'
    foundation += extract('src/Dolphin/mtx/mtx44.c', [
        'void C_MTX44Identity(', 'void PSMTX44Identity(', 'void C_MTX44Copy(',
        'void C_MTX44Concat(', 'void PSMTX44Concat('])
    foundation += extract('src/Dolphin/mtx/quat.c', [
        'void C_QUATScale(', 'void PSQUATScale(', 'f32 C_QUATDotProduct(', 'f32 PSQUATDotProduct('])
    math = read('src/NL/nlMath.cpp')
    foundation += body(math, 'struct nlAngleConstants')+';\n'
    foundation += re.search(r'const nlAngleConstants nlAngleConstantsData\s*=\s*\{[^}]*\};', math).group()+'\n'
    foundation += extract('src/NL/nlMath.cpp', ['void nlSinCos(', 'float nlRecipSqrt('])
    foundation += extract('src/NL/math.cpp', ['void nlQuatNLerp('])
    foundation += '#define RAD_TO_FIXED16 10430.378f\n'
    foundation += extract('src/NL/plat/platqmath.cpp', ['void nlQuatScale(', 'f32 nlQuatDot(', 'void nlQuatToMatrix('])
    foundation += extract('src/NL/plat/platvmath.cpp', [
        'void nlMatrix4::SetIdentity(', 'void nlMultMatrices(',
        'void nlMakeRotationMatrixX(', 'void nlMakeRotationMatrixZ('])
    foundation += extract('src/Game/SHierarchy.cpp', [
        'void cSHierarchy::BuildPushPopFlags(', 'int cSHierarchy::GetChild(',
        'int cSHierarchy::GetNodeIndexByID(', 'u32 cSHierarchy::GetNodeID(', 'int cSHierarchy::GetMirroredNode(',
        'int cSHierarchy::GetPushPop(', 'nlVector3& cSHierarchy::GetTranslationOffset(',
        'bool cSHierarchy::PreserveBoneLength('])
    foundation += extract('src/Game/PoseNode.cpp', ['cPoseNode::cPoseNode()', 'cPoseNode::~cPoseNode()'])
    foundation += read('src/Game/PoseAccumulator.cpp')+'\n'
    foundation += extract('src/Game/AI/HeadTrack.cpp', ['void CalcHeadTrackMatrix('])

    signatures = [
        'DrawableCharacter::DrawableCharacter()', 'DrawableCharacter::~DrawableCharacter()',
        'DrawableCharacter& DrawableCharacter::operator=(', 'void DrawableCharacter::Free()',
        'void DrawableCharacter::Grab(cCharacter&',
        'static void DrawableCharacterHeadTrackCallback(',
        'void DrawableCharacter::DrawableBowserHeadTrackCallback(',
        'void DrawableCharacter::BuildNodeMatrices()',
        'void DrawableCharacter::Grab(SkinAnimatedMovableNPC&',
        'void DrawableCharacter::Blend(', 'void DrawableCharacter::EvaluateFrom(',
        'template <typename T>\nvoid DrawableCharacter::Replay(']
    helper_signature = 'static void CopySnapshotPose('
    if helper_signature in drawable:
        helper = body(drawable, helper_signature)
    else:
        # Baseline reproduction uses the actual pre-change Grab and former copy.
        helper = 'static void CopySnapshotPose(cPoseAccumulator& d, const cPoseAccumulator& s) { d = s; }'
    selected = [body(drawable, s) for s in signatures]
    for signature, selected_body in zip(signatures, selected):
        extracted.append({'source':str(source), 'signature':signature,
                          'sha256':hashlib.sha256(selected_body.encode()).hexdigest()})
    implementation = helper+'\n\n'+'\n\n'.join(selected)+'\n'
    # Only the tested copy operation differs in the independent old-behaviour path.
    references = []
    for s, arg in [('void DrawableCharacter::Grab(cCharacter&', 'character.m_pPoseAccumulator'),
                   ('void DrawableCharacter::Grab(SkinAnimatedMovableNPC&', 'npc.mpPoseAccumulator')]:
        b = body(drawable, s).replace('DrawableCharacter::Grab(', 'ReferenceDrawableCharacter::Grab(', 1)
        b = b.replace(f'CopySnapshotPose(*mPoseAccumulator, *{arg});', f'*mPoseAccumulator = *{arg};')
        references.append(b)
    for path in ['include/Game/PoseAccumulator.h','include/Game/PoseNode.h','include/Game/SHierarchy.h',
                 'include/NL/nlVector.h','include/NL/nlMath.h','include/port/prelude.h']:
        read(path)
    driver = ROOT/'tests/pose_snapshot_driver_test.cpp'
    read(driver)
    before = dict(touched)
    with tempfile.TemporaryDirectory(prefix='strikers-pose-snapshot-') as temp:
        temp = Path(temp)
        (temp/'pose_snapshot_declarations.inc').write_text(declarations)
        (temp/'pose_snapshot_foundation.inc').write_text(foundation)
        (temp/'pose_snapshot_implementation.inc').write_text(implementation)
        (temp/'pose_snapshot_reference.inc').write_text('\n\n'.join(references)+'\n')
        compiler = shutil.which(args.cxx or 'clang++')
        if not compiler:
            raise RuntimeError('clang++ unavailable')
        command = [compiler,'-std=c++20','-DTARGET_PC=1','-O2','-g','-fno-omit-frame-pointer',
                   '-fsanitize=address,undefined','-fno-sanitize-recover=all',
                   '-fshort-wchar','-fsigned-char','-fdeclspec','-Wno-register',
                   '-Wno-unknown-pragmas','-I'+str(ROOT/'include'),'-I'+str(temp),
                   '-include',str(ROOT/'include/port/prelude.h'),str(driver),'-o',str(temp/'pose_snapshot_test')]
        t = time.monotonic()
        build = subprocess.run(command,text=True,capture_output=True)
        build_seconds = time.monotonic()-t
        if build.returncode:
            print(build.stdout+build.stderr,end='')
            return build.returncode
        env = os.environ.copy()
        # Apple ASan does not provide LeakSanitizer. The driver separately
        # proves every nlMalloc-owned object/array is freed exactly once.
        env['ASAN_OPTIONS'] = 'detect_leaks='+('0' if platform.system()=='Darwin' else '1')+':halt_on_error=1'
        env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
        command_run = [str(temp/'pose_snapshot_test')]+(['--require-skip'] if args.require_skip else [])
        t = time.monotonic()
        result = subprocess.run(command_run,text=True,capture_output=True,env=env)
        seconds = time.monotonic()-t
        unchanged = all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in before.items())
        report = {'schema':'strikers-pose-snapshot-host-gate-v1',
                  'status':'PASS' if result.returncode==0 and unchanged else 'FAIL',
                  'source':str(source),'require_skipped_previous_copy':args.require_skip,
                  'build_seconds':build_seconds,'run_seconds':seconds,'returncode':result.returncode,
                  'stdout':result.stdout,'stderr':result.stderr,'compiler':compiler,
                  'sanitizers':['address','undefined'],'source_unchanged_during_gate':unchanged,
                  'sources':before,'extracted_bodies':extracted,
                  'limits':['Host float arithmetic is compared bit for bit between the new and old copy paths; it is not a separate ARM numerical oracle.',
                            'World fixtures supply legal hierarchies and deterministic animation data. Replay transport is raw fixture transport; the production Replay body and its evaluation/build/delete sequence run unchanged.',
                            'Host callback/pointer sizes differ from Vita; recorded host copy bytes are not a measured Vita runtime or FPS benefit.']}
        if args.output:
            args.output.parent.mkdir(parents=True,exist_ok=True)
            args.output.write_text(json.dumps(report,indent=2)+'\n')
        print(result.stdout,end='')
        print(result.stderr,end='')
        print(json.dumps({'status':report['status'],'build_seconds':build_seconds,'run_seconds':seconds,
                          'source_unchanged_during_gate':unchanged,'extracted_bodies':len(extracted)}))
        return result.returncode if result.returncode else (0 if unchanged else 1)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path)
    parser.add_argument('--cxx')
    parser.add_argument('--require-skip',action='store_true')
    parser.add_argument('--output',type=Path)
    raise SystemExit(run(parser.parse_args()))
