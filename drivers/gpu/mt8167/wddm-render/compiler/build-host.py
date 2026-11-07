#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""既存Eの固定PCO buildを再利用し、D3D bytecode変換を実compilerで一括確認する。"""
import argparse, hashlib, json, os, shlex, subprocess
from pathlib import Path

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--reference-manifest', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    here = Path(__file__).resolve().parent
    root = next(p for p in here.parents if (p / 'windows-rt/AGENTS.md').is_file())
    out = a.output.resolve()
    if not out.is_relative_to(root):
        raise SystemExit('output must be inside project root')
    ref = json.loads(a.reference_manifest.read_text())
    for name, digest in ref['linked_object_hashes'].items():
        if sha(Path(name)) != digest:
            raise SystemExit('fixed PCO reference input changed: ' + name)
    build = Path(ref['build'])
    row = next(x for x in json.loads((build/'compile_commands.json').read_text()) if x['file'].endswith('/pco_nir.c'))
    original = shlex.split(row['command'])
    if 'ccache' in original[0]:
        original = original[1:]
    base, i = [], 0
    while i < len(original):
        if original[i] in ('-o', '-MF', '-MT', '-MQ'):
            i += 2
            continue
        if original[i] in ('-MD', '-MMD', '-c') or original[i] == row['file']:
            i += 1
            continue
        base.append(original[i])
        i += 1
    out.mkdir(parents=True, exist_ok=False)
    env = os.environ.copy()
    env.update(TMPDIR=str(out), MESA_SHADER_CACHE_DISABLE='true')
    # Reuse link identity/dependency set from E; add no compiler/library download.
    libs = [x for x in ref['link'] if x.endswith(('.a', '.so'))]
    env['LD_LIBRARY_PATH'] = str(root/'unixnu/UniXNU-a04br3/.cache/pvr-host-env/lib')
    commands, objects = [], []
    with (out/'build.log').open('w') as log:
        for unit in ('sm2', 'pco_sm2', 'offline'):
            obj = out/(unit+'.o')
            command = base + ['-UNDEBUG', '-Werror', '-c', str(here/(unit+'.c')), '-o', str(obj)]
            commands.append(command)
            subprocess.run(command, cwd=build, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
            objects.append(str(obj))
        command = ['c++', '-o', str(out/'sm2-pco'), *objects, '-Wl,--start-group', *libs, '-Wl,--end-group', '-pthread', '-lm']
        commands.append(command)
        subprocess.run(command, cwd=build, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    with (out/'compiler-output.txt').open('w') as log:
        subprocess.run([str(out/'sm2-pco')], env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    (out/'manifest.json').write_text(json.dumps({
        'source_commit': subprocess.check_output(['git','-C',str(here),'rev-parse','HEAD'], text=True).strip(),
        'source_dirty': bool(subprocess.check_output(['git','-C',str(here),'status','--porcelain'], text=True)),
        'sources': {p.name:sha(p) for p in sorted(here.iterdir()) if p.suffix in ('.c','.h','.py')},
        'reference_manifest_sha256':sha(a.reference_manifest), 'linked_object_hashes':ref['linked_object_hashes'],
        'commands':commands, 'output_sha256':sha(out/'compiler-output.txt'), 'executable_sha256':sha(out/'sm2-pco'),
        'windows_arm_build':False, 'gpu_executed':False, 'feature_level_advertised':False,
    }, indent=2)+'\n')
    print('PASS: actual D3D9 SM2 -> NIR -> PCO USC; no Windows/GPU execution')
if __name__ == '__main__':
    main()
