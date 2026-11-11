#!/usr/bin/env python3
"""One observation TU + two retained objects, real WDK81/KMDF1.13, unsigned only."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import subprocess
import zipfile


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def require(ok, message):
    if not ok:
        raise ValueError(message)


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def dependencies(text):
    # Same Windows make-escape rules as the existing compiler cache reader.
    text = re.sub(r'\\\r?\n', ' ', text.lstrip('\ufeff'))
    require(text.startswith('keypad14-unit:'), 'Unexpected dependency target')
    text = text[len('keypad14-unit:'):]
    result, token, i = [], [], 0
    while i < len(text):
        c = text[i]
        if c.isspace():
            if token:
                result.append(''.join(token)); token = []
            i += 1; continue
        if c == '\\' and i+1 < len(text) and text[i+1] in ' \t#':
            token.append(text[i+1]); i += 2; continue
        if c == '$' and i+1 < len(text) and text[i+1] == '$':
            token.append('$'); i += 2; continue
        token.append(c); i += 1
    if token:
        result.append(''.join(token))
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for n in ('inputs-zip', 'wdk-root', 'output', 'expected-source-commit'):
        p.add_argument('--'+n, required=True)
    p.add_argument('--compiler', default='C:/Program Files/LLVM/bin/clang.exe')
    p.add_argument('--readobj', default='C:/Program Files/LLVM/bin/llvm-readobj.exe')
    p.add_argument('--linker', default='C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm/link.exe')
    a = p.parse_args()
    repo = Path(__file__).resolve().parents[2]
    lock = read(repo/'tools/ci/mt8167-keypad14-inputs.json')
    out = Path(a.output).resolve(); wdk = Path(a.wdk_root).resolve()
    require(os.name == 'nt', 'Native Windows case-insensitive WDK paths required')
    require(not out.exists() and out.is_relative_to(repo), 'Fresh checkout-local output required')
    require(not subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain']), 'Clean fixed source required')
    commit = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    require(commit == a.expected_source_commit, 'Expected fixed source commit mismatch')
    require(not any(os.environ.get(n) for n in ('CCC_OVERRIDE_OPTIONS', 'CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH')), 'Uncontrolled compiler environment')
    require(sha(a.inputs_zip) == lock['small_inputs_sha256'], 'Small input ZIP hash mismatch')
    out.mkdir(parents=True); (out/'obj').mkdir(); (out/'package').mkdir(); (out/'tmp').mkdir()
    state = {'source_commit':commit, 'stage':'preflight', 'compile_complete':False, 'link_complete':False, 'signed':False}
    commands = []
    def save():
        (out/'state.json').write_text(json.dumps(state, indent=2)+'\n')
    def run(command):
        commands.append(command)
        (out/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
        with (out/'build.log').open('a', encoding='utf-8') as f:
            f.write(json.dumps(command)+'\n'); f.flush()
            proc = subprocess.run(command, env=env, stdout=f, stderr=subprocess.STDOUT)
        state['last_exit_code'] = proc.returncode; save()
        require(proc.returncode == 0, 'Stage '+state['stage']+' failed; preserve successful artifacts')
    env = os.environ.copy(); env['TEMP'] = env['TMP'] = str(out/'tmp')
    save()
    try:
        tools = {'clang':Path(a.compiler), 'llvm-readobj':Path(a.readobj), 'link':Path(a.linker)}
        for name, path in tools.items():
            require(sha(path) == lock['tools'][name], 'Pinned tool mismatch: '+name)
        resource = Path(subprocess.check_output([a.compiler, '--no-default-config', '-print-resource-dir'], text=True).strip())/'include'
        staged = out/'inputs'
        with zipfile.ZipFile(a.inputs_zip) as z:
            require(z.testzip() is None, 'Small input ZIP CRC')
            names = z.namelist()
            require(len(names) == len(set(names)) and len(names) <= 64, 'Small closure file count')
            for name in names:
                q = PurePosixPath(name)
                require(not q.is_absolute() and '..' not in q.parts and '\\' not in name and ':' not in name, 'Unsafe ZIP member')
            index = json.loads(z.read('index.json'))
            require(index['schema'] == 'keypad14-small-inputs-v1' and set(names) == {r['path'] for r in index['files']}|{'index.json'}, 'Small closure index')
            for row in index['files']:
                b = z.read(row['path'])
                require(len(b) == row['bytes'] and hashlib.sha256(b).hexdigest() == row['sha256'], 'Small closure bytes/hash')
            z.extractall(staged)
        baseline = staged/'baseline12/unsigned-manifest.json'
        require(sha(baseline) == lock['baseline12_manifest_sha256'] and sha(staged/'signed13-manifest.json') == lock['signed13_manifest_sha256'], 'Historical baseline manifest binding')
        old = read(baseline); signed = read(staged/'signed13-manifest.json')
        require(signed['source_commit'] == lock['baseline_source'] and signed['version'] == '0.0.1.3', 'Actual13 source/version binding')
        require(any(r['sha256'] == lock['signed13_SYS_sha256'] for r in signed['outputs']), 'Actual signed13 SYS binding')
        for row in lock['repo_inputs']:
            require(sha(repo/row['path']) == row['sha256'], 'Source/header/INF changed: '+row['path'])
        # Both objects are exactly those already retained by the signed 1.3 package.
        reused = []
        for row in old['compiled'][1:]:
            q = staged/'baseline12/obj'/(Path(row['path']).stem+'.obj')
            prior = next(x for x in signed['reused'] if x['path'] == row['path'])
            require(sha(q) == row['object_sha256'] == prior['object_sha256'] and sha(repo/row['path']) == row['source_sha256'] == prior['source_sha256'], 'Retained object/source differs from actual13')
            reused.append(row)
        require(len(reused) == 2, 'Exactly two retained objects required')
        for row in old['inputs']:
            if row['path'].endswith(('button_state.h', 'button_pipeline.h')):
                require(sha(repo/row['path']) == row['sha256'], 'Retained repository header changed')
        headers = read(staged/'header-bindings.json')
        require(sha(staged/'header-bindings.json') == lock['header_index_sha256'], 'Header index binding')
        known = {}; header_rows = []
        def register(path, origin):
            key = str(path.resolve()).lower()
            h = sha(path); known[key] = h
            header_rows.append({'path':str(path), 'sha256':h, 'origin':origin})
            return h
        for row in headers:
            q = (staged/'include'/row['path']) if row['origin'] == 'wdf' else (wdk/'Include'/row['path'])
            require(q.stat().st_size == row['bytes'] and register(q, row['origin']) == row['sha256'], 'Real WDK/WDF header mismatch: '+row['path'])
        for q in sorted(resource.rglob('*')):
            if q.is_file(): register(q, 'current-clang20-resource')
        for row in lock['repo_inputs']:register(repo/row['path'], 'fixed-repository')
        (out/'header-inputs.json').write_text(json.dumps(header_rows, indent=2)+'\n')
        libs = []
        for row in old['libraries']:
            rel = row['path'].split('/compiler-libs/wdk/Lib/')[1]
            q = (staged/'lib'/rel) if rel.startswith('wdf/') else (wdk/'Lib'/rel)
            require(sha(q) == row['sha256'], 'Real WDK/WDF library mismatch')
            libs.append(q)
        require(len(libs) == 6, 'Six actual libraries required')
        flags = []; i = 0
        while i < len(old['codegen']):
            arg = old['codegen'][i]
            if arg == '-ivfsoverlay':i += 2;continue
            if arg == '-isystem':
                rel = old['codegen'][i+1].split('/sdk-headers/wdk/Include/')[1]
                path = staged/'include'/rel if rel.startswith('wdf/') else wdk/'Include'/rel
                flags += [arg, str(path)];i += 2;continue
            flags.append(arg);i += 1
        # Historical argv kept in the baseline; explicitly record this new compile's
        # no-default-config and dependency emission, without claiming old env parity.
        flags += ['--no-default-config']
        src = repo/'drivers/input/mt8167-keypad/wdf_keys.c';obj = out/'obj/wdf_keys.obj'
        state['stage'] = 'compile1';save()
        run([a.compiler, *flags, '-MD', '-MF', str(out/'wdf_keys.d'), '-MT', 'keypad14-unit', '-c', str(src), '-o', str(obj)])
        state.update(compile_complete=True, compiled_object_sha256=sha(obj));save()
        require(struct.unpack_from('<H', obj.read_bytes())[0] == 0x1c4, 'Compiled object must be ARMNT')
        deps = []
        for name in sorted(set(dependencies((out/'wdf_keys.d').read_text()))):
            q = Path(name).resolve();h = sha(q)
            require(str(q).lower() in known and known[str(q).lower()] == h, 'Dependency outside prehashed input closure: '+str(q))
            deps.append({'path':str(q), 'bytes':q.stat().st_size, 'sha256':h})
        (out/'actual-dependencies.json').write_text(json.dumps(deps, indent=2)+'\n')
        objects = [obj]
        for row in reused:
            q = staged/'baseline12/obj'/(Path(row['path']).stem+'.obj');target = out/'obj'/q.name
            shutil.copyfile(q, target);objects.append(target)
        package = out/'package';sysfile = package/'mt8167-keypad.sys'
        compile_receipt = {**state, 'actual_dependencies':deps, 'codegen':flags, 'baseline_codegen_sha256':hashlib.sha256(json.dumps(old['codegen'], separators=(',', ':')).encode()).hexdigest()}
        (out/'compile-manifest.json').write_text(json.dumps(compile_receipt, indent=2)+'\n')
        state['stage'] = 'MSVC1429-link1';save()
        run([a.linker, '/nologo', '/driver', '/subsystem:native,6.03', '/machine:arm', '/entry:FxDriverEntry', '/nodefaultlib', '/incremental:no', *map(str,objects), *map(str,libs), '/out:'+str(sysfile), '/map:'+str(out/'keypad14.map')])
        state.update(link_complete=True, unsigned_SYS_sha256=sha(sysfile));save()
        pe = sysfile.read_bytes();off=struct.unpack_from('<I',pe,60)[0]
        require(pe[off:off+4] == b'PE\0\0' and struct.unpack_from('<H',pe,off+4)[0] == 0x1c4, 'Linked SYS must be ARMNT')
        inf = repo/'drivers/input/mt8167-keypad/mt8167-keypad.inf'
        require(('DriverVer='+lock['driverver']).encode() in inf.read_bytes(), 'Candidate INF version/date')
        (package/inf.name).write_bytes(inf.read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'))
        state['stage'] = 'inspect';save()
        run([a.readobj, '--file-headers', '--coff-imports', str(sysfile)])
        for row,q in zip(old['libraries'],libs):require(sha(q) == row['sha256'], 'Library changed during build')
        for name,q in tools.items():require(sha(q) == lock['tools'][name], 'Tool changed during build')
        for row in deps:require(sha(row['path']) == row['sha256'], 'Dependency changed during build')
        manifest = {'schema':'mt8167-keypad14-incremental-unsigned-v1','source_commit':commit,'source_dirty':False,'observation_source':lock['observation_source'],'baseline_source':lock['baseline_source'],'signed13_manifest_sha256':lock['signed13_manifest_sha256'],'version':'0.0.1.4','target':'MT6392\\Buttons','service':'Mt6392Keys','compiled_TUs':1,'reused_objects':reused,'baseline12_manifest_sha256':sha(baseline),'codegen':flags,'baseline_codegen_sha256':compile_receipt['baseline_codegen_sha256'],'inputs_zip_sha256':sha(a.inputs_zip),'repo_inputs':lock['repo_inputs'],'header_inputs_sha256':sha(out/'header-inputs.json'),'actual_dependencies_sha256':sha(out/'actual-dependencies.json'),'actual_dependencies':deps,'tools':[{'name':k,'path':str(v),'sha256':sha(v)} for k,v in tools.items()],'libraries':[{'path':str(q),'sha256':sha(q)} for q in libs],'objects':[{'path':str(q.relative_to(out)),'sha256':sha(q),'bytes':q.stat().st_size} for q in objects],'commands':commands,'unsigned_outputs':[{'path':str(q.relative_to(out)),'bytes':q.stat().st_size,'sha256':sha(q)} for q in sorted(package.iterdir())],'signed':False,'CAT_created':False,'deployable':False,'device_tested':False,'functional_fix_claimed':False,'SDK_rebuilt':False,'historical_resource_header_hash_recorded':False,'codegen_rebinding':'retained Linux18 object provenance kept; this new TU uses pinned clang20, real Windows case-insensitive include paths, no-default-config, and MD dependency capture; no claim of unrecorded historical resource/env identity'}
        (out/'unsigned-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
        state['stage']='unsigned-package-complete';save()
        print('KEYPAD14_COMPILE1_MSVC_LINK1_UNSIGNED_NOT_DEVICE_TESTED')
    except Exception as e:
        state['error_type']=type(e).__name__;state['error']=str(e);save();raise


if __name__ == '__main__':main()
