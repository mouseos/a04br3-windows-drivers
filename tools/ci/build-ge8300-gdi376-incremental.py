#!/usr/bin/env python3
"""GDI376: replace eight retained372/370 objects, link render only; no signing."""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import importlib.util

# Reuse the existing archive parser and input/receipt utilities, not its
# DWM372-specific thirteen-unit compile plan or full GPU include probes.
HERE = Path(__file__).resolve().parent
module = importlib.util.spec_from_file_location('dwm372', HERE / 'build-ge8300-dwm372-incremental.py')
reuse = importlib.util.module_from_spec(module)
module.loader.exec_module(reuse)
ROOT = HERE.parents[1]
GPU = ROOT / 'drivers/gpu/mt8167'
require, sha, read, pin, members = reuse.require, reuse.sha, reuse.read, reuse.pin, reuse.members


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--enable-candidate', action='store_true')
    p.add_argument('--inputs', type=Path, required=True)
    p.add_argument('--native-generated-directory', type=Path, required=True)
    p.add_argument('--ucrt-directory', type=Path)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--plan-only', action='store_true')
    p.add_argument('--vc-directory', type=Path, default=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133'))
    p.add_argument('--llvm-directory', type=Path, default=Path('C:/Program Files/LLVM/bin'))
    p.add_argument('--jobs', type=int, choices=range(1, 5), default=2)
    a = p.parse_args()
    require(a.enable_candidate, 'Explicit candidate opt-in required')
    spec = read(HERE / 'ge8300-gdi376-incremental-pins.json')
    commit = reuse.clean_source()
    subprocess.run(['git', '-C', ROOT, 'merge-base', '--is-ancestor', spec['runtime_base'], commit], check=True)
    rows = spec['rows']
    expected = {'drivers/gpu/mt8167/' + r['source'] for r in rows}
    changed = set(subprocess.check_output(['git', '-C', ROOT, 'diff', '--name-only', spec['runtime_base'], '--', 'drivers/gpu/mt8167'], text=True).splitlines())
    require(changed - expected == {'drivers/gpu/mt8167/wddm-render/ge8300-render.inf',
                                  'drivers/gpu/mt8167/wddm-render/tests/gdi_present.py',
                                  'drivers/gpu/mt8167/wddm-render/tests/standard_staging.py',
                                  'drivers/gpu/mt8167/wddm-render/clear_engine.h',
                                  'drivers/gpu/mt8167/wddm-render/kmd_clear.h'},
            'Unexpected GPU changes outside exact eight TU plan')
    require(expected <= changed, 'An expected changed TU is absent')
    inputs, out = a.inputs.resolve(), a.output.resolve()
    for name, digest in spec['inputs'].items():
        pin(inputs / name, digest)
    pin(a.native_generated_directory / 'a04_copy_usc_asset.h', spec['generated_header_sha256'])
    baseline = read(inputs / 'baseline372/incremental-build-manifest.json')
    require(baseline['success'] and baseline['source_commit'] == spec['runtime_base'], 'Wrong retained372 source')
    archives = {'kmd': inputs / 'baseline372/ge8300-kmd-clear.lib',
                'native': inputs / 'native/ge8300-native-owner-core.lib'}
    parsed = {kind: members(path) for kind, path in archives.items()}
    for row in rows:
        require(sum(name == row['object'] for name, _ in parsed[row['archive']]) == 1, 'Missing/ambiguous retained member')
    require('display_unified_display.obj' not in dict(members(inputs / 'baseline372/ge8300-display-transport.lib')),
            'Transport unexpectedly consumes changed display object; revise tuple')
    plan = dict(source_commit=commit, candidate_version=spec['candidate_version'],
                rows=[dict(source=r['source'], archive=r['archive'], object=r['object']) for r in rows],
                native_changed_TU=3, module_changed_TU=5, UMD_compiles=0, SDK_builds=0,
                transport_rebuilt=False, full_native_rebuilt=False, SYS_links=1,
                signed=False, hardware_verified=False, deployment_performed=False)
    if a.plan_only:
        print(json.dumps(dict(plan, executed=False), indent=2))
        return
    require(os.name == 'nt', 'Windows only; host plan verification does not compile')
    require(not out.exists() and out.is_relative_to(ROOT) and not out.is_relative_to(inputs), 'Fresh source-local output required')
    require(a.ucrt_directory and (a.ucrt_directory / 'corecrt.h').is_file(), 'Retained UCRT directory required for unchanged C++ include contract')
    vc = a.vc_directory.resolve()
    require(vc.name == '14.29.30133', 'Retained VC14.29 required')
    cl, link = vc / 'bin/Hostx64/arm/cl.exe', vc / 'bin/Hostx64/arm/link.exe'
    pin(cl, spec['CL_sha256']); pin(link, spec['LINK_sha256'])
    tools = {name: a.llvm_directory / (name + '.exe') for name in ('clang', 'llvm-lib', 'llvm-readobj')}
    for name, tool in tools.items():
        pin(tool, spec['tool_pins'][name])
    for name, digest in spec['link_inputs'].items():
        pin(inputs / 'wdk81/Lib/winv6.3/km/arm' / name, digest)
    resource = Path(subprocess.check_output([tools['clang'], '-print-resource-dir'], text=True).strip())
    out.mkdir(parents=True); (out / 'objects').mkdir(); (out / 'logs').mkdir()
    env = dict(os.environ, TEMP=str(out / 'logs'), TMP=str(out / 'logs'), VSLANG='1033')
    receipt = dict(plan, success=False, new_compiles=[], commands=[], reused_archives={}, historical_include_closure_reconstructed=False)

    def run(args, name):
        q = subprocess.run(list(map(str, args)), cwd=ROOT, env=env, capture_output=True, text=True, errors='replace')
        log = out / 'logs' / name; log.write_text(q.stdout + q.stderr)
        receipt['commands'].append(dict(arguments=list(map(str, args)), returncode=q.returncode, log=name, log_sha256=sha(log)))
        require(q.returncode == 0, 'Command failed: ' + str(log))
        return q.stdout + q.stderr

    def relocate(arg):
        prefix = '/I' if arg.startswith('/I') else '-I' if arg.startswith('-I') else None
        if prefix is None:
            return arg
        path = re.sub('/+', '/', arg[2:].replace('\\', '/'))
        lower = path.lower()
        if '/windows kits/8.1/include/' in lower:
            path = inputs / 'wdk81/Include' / path.split('/Include/', 1)[1]
        elif '/windows kits/10/include/' in lower and lower.endswith('/ucrt'):
            path = a.ucrt_directory.resolve()
        elif '/drivers/gpu/mt8167' in lower:
            path = GPU / path.split('/drivers/gpu/mt8167', 1)[1].lstrip('/')
        elif lower.endswith('/native/generated'):
            path = a.native_generated_directory.resolve()
        elif '/llvm/lib/clang/' in lower and lower.endswith('/include'):
            path = resource / 'include'
        elif '/vc/tools/msvc/' in lower and lower.endswith('/include'):
            path = vc / 'include'
        else:
            raise ValueError('Unmapped original include: ' + path)
        return prefix + str(path)

    def compile_row(row):
        source = GPU / row['source']; obj = out / 'objects' / row['object']
        args = [relocate(x) for x in row['arguments']]
        cpp = source.suffix == '.cxx'
        if cpp:
            output = run([cl, *args, '/showIncludes', source, '/Fo' + str(obj)], obj.stem + '.log')
            headers = [Path(x.split('including file:', 1)[1].strip()) for x in output.splitlines() if 'including file:' in x]
        else:
            dep = obj.with_suffix('.d')
            run([tools['clang'], *args, '-MD', '-MF', dep, '-MT', 'ge8300-unit', '-c', source, '-o', obj], obj.stem + '.log')
            headers = [Path(x) for x in reuse.dependencies(dep)]
        require(headers, 'Actual include closure missing')
        closure = {str(h.resolve(strict=True)): sha(h) for h in [source, *headers]}
        receipt['new_compiles'].append(dict(source=source.relative_to(ROOT).as_posix(), source_sha256=sha(source),
                                            object=obj.name, object_sha256=sha(obj), arguments=args,
                                            tool_sha256=sha(cl if cpp else tools['clang']), actual_include_closure=closure))
        return row['archive'], obj

    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
            replacements = list(pool.map(compile_row, rows))
        libraries = {}
        for kind in ('kmd', 'native'):
            folder = out / ('retained-' + kind); folder.mkdir()
            replacement = {obj.name: obj for archive, obj in replacements if archive == kind}
            objects, hashes = [], {}
            for name, data in parsed[kind]:
                old = folder / name; old.write_bytes(data)
                obj = replacement.get(name, old); objects.append(obj); hashes[name] = sha(obj)
            target = out / archives[kind].name
            run([tools['llvm-lib'], '/machine:arm', '/out:' + str(target), *reversed(objects)], kind + '-archive.log')
            actual = members(target)
            require([name for name, _ in actual] == [name for name, _ in parsed[kind]], 'Archive order drift')
            require({n: hashlib.sha256(b).hexdigest() for n, b in actual} == hashes, 'Archive bytes differ from selected retained/new objects')
            libraries[kind] = target
            receipt['reused_archives'][kind] = dict(baseline_sha256=sha(archives[kind]), output_sha256=sha(target), replacement_members=sorted(replacement), members=hashes)
        km = inputs / 'wdk81/Lib/winv6.3/km/arm'
        # Same command contract as link-kmd-msvc-wdk81.ps1, with explicit
        # relocated libraries. No ProgramFiles/global SDK mutation is needed.
        run([link, '/largeaddressaware', '/tsaware:no', '/nologo', '/driver', '/subsystem:native,6.03', '/machine:arm',
             '/entry:DriverEntry', '/nodefaultlib', '/incremental:no', '/base:0x400000', libraries['kmd'], libraries['native'],
             *[km / name for name in ('displib.lib', 'ntoskrnl.lib', 'hal.lib', 'libcntpr.lib', 'bufferoverflowfastfailk.lib')],
             '/out:' + str(out / 'ge8300-render.sys'), '/map:' + str(out / 'ge8300-render.map')], 'render-link.log')
        run([tools['llvm-readobj'], '--file-headers', '--coff-imports', out / 'ge8300-render.sys'], 'render-pe.log')
        receipt['actual_link_inputs'] = {str(path): sha(path) for path in [link, *libraries.values(), *[km / name for name in spec['link_inputs']]]}
        receipt['retained_transport_sha256'] = sha(inputs / 'package372/display-transport/mt8167-display-transport.sys')
        receipt['retained_package_manifest_sha256'] = sha(inputs / 'package372/signed-manifest.json')
        consumed = dict(reuse.HASH_CACHE); reuse.HASH_CACHE.clear()
        for path, digest in consumed.items():
            pin(Path(path), digest)
        require(reuse.clean_source() == commit, 'Source changed during build')
        receipt['success'] = True
    finally:
        receipt['outputs'] = {str(f.relative_to(out)): sha(f) for f in out.rglob('*') if f.is_file()}
        (out / 'incremental-build-manifest.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('GDI376_EIGHT_TU_RENDER_UNSIGNED; TRANSPORT_UMD_FW_RETAINED; NO_DEVICE_OPERATION')


if __name__ == '__main__':
    main()
