"""Opt-in admission of an intact native compile checkpoint; never performs codegen.
Offline audit deliberately cannot produce an admission receipt or reusable outputs.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import struct
import subprocess
import sys
import uuid
import zipfile

SCHEMA = 'ge8300-verified-native-compile-cache-v1'
ROOTS = ('clang-resource20', 'wdk81-km', 'wdk81-shared', 'wdk81-um',
         'msvc1444', 'ucrt22621', 'generated-native')


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(b):
    return hashlib.sha256(b).hexdigest()


def sha(path):
    return digest(Path(path).read_bytes())


def win(path):
    # Windows separators/case only. Do not erase build-root or __FILE__ changes.
    return re.sub('/+', '/', str(path).replace('\\', '/')).rstrip('/').lower()


class Archive:
    def __init__(self, path, expected):
        require(re.fullmatch('[0-9a-fA-F]{64}', expected or ''), 'Explicit archive SHA256 required')
        require(sha(path) == expected.lower(), 'Archive SHA256 mismatch')
        self.path = Path(path)
        self.z = zipfile.ZipFile(path)
        self.names = {}
        total = 0
        for entry in self.z.infolist():
            name = entry.filename.replace('\\', '/')
            p = PurePosixPath(name)
            require(not p.is_absolute() and '..' not in p.parts and ':' not in name,
                    'Unsafe archive path')
            require(name.lower() not in {n.lower() for n in self.names}, 'Duplicate archive path')
            require(not entry.is_dir() and (entry.external_attr >> 16) & 0o170000 != 0o120000,
                    'Unexpected directory/symlink entry')
            total += entry.file_size
            require(total <= 512 * 1024 * 1024 and len(self.names) < 20000, 'Archive size/count bound')
            self.names[name] = entry.filename
        require(self.z.testzip() is None, 'Archive CRC mismatch')

    def read(self, name):
        return self.z.read(self.names[name])

    def json(self, name):
        return json.loads(self.read(name).decode('utf-8-sig'))

    def index(self, name):
        index = self.json(name)
        rows = index['files']
        paths = [r['path'] for r in rows]
        require(len(paths) == len(set(paths)) and set(paths) | {name} == set(self.names),
                'Index file-set mismatch')
        for row in rows:
            b = self.read(row['path'])
            require(len(b) == row['bytes'] and digest(b) == row['sha256'].lower(),
                    'Indexed file hash/size mismatch: ' + row['path'])
        return index


def library_order(data, object_hashes, object_names):
    require(data[:8] == b'!<arch>\n', 'Native library archive signature')
    pos, members, names, strings = 8, [], [], b''
    while pos < len(data):
        h = data[pos:pos+60]
        require(len(h) == 60 and h[58:] == b'`\n', 'Native library member header')
        size = int(h[48:58])
        b = data[pos+60:pos+60+size]
        require(len(b) == size, 'Truncated library member')
        name = h[:16].decode('ascii').strip()
        if name == '//':
            strings = b
        elif name not in ('/', '/SYM64/'):
            if re.fullmatch(r'/[0-9]+', name):
                offset = int(name[1:])
                require(offset < len(strings), 'Library member name offset')
                end = strings.find(b'\0', offset)
                require(end >= offset, 'Unterminated library member name')
                name = strings[offset:end].decode('ascii')
            names.append(name.replace('\\', '/').rsplit('/', 1)[-1].rstrip('/'))
            require(len(b) >= 20 and struct.unpack_from('<H', b)[0] == 0x1c4, 'Library member ARMNT')
            members.append(digest(b))
        pos += 60 + size + size % 2
    # This pinned llvm-lib prepends its inputs; physical order is significant.
    require(pos == len(data) and members == object_hashes[::-1] and names == object_names[::-1],
            'Library member name/content/order mismatch')


def tree(path):
    root = Path(path)
    require(root.is_dir(), 'Missing current include root: ' + str(root))
    result = {}
    for parent, dirs, files in os.walk(root, followlinks=False):
        for name in dirs + files:
            p = Path(parent) / name
            st = p.lstat()
            require(not p.is_symlink() and not getattr(st, 'st_file_attributes', 0) &
                    getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 1024), 'Nested reparse/symlink input')
        for name in files:
            p = Path(parent) / name
            rel = p.relative_to(root).as_posix()
            require(rel.lower() not in {x.lower() for x in result}, 'Case-colliding include path')
            result[rel] = (p.stat().st_size, sha(p))
    return result


def verify_clang_configuration(clang, commands, environ=None, runner=None):
    """No codegen: discover LLVM20's actual system/user/executable cfg search.

    Explicit missing --config plus --no-default-config emits the driver's own
    searched-directory diagnostics without loading existing default cfg contents.
    Both recorded targets are probed. Unrecognized diagnostics fail closed.
    """
    environ = os.environ if environ is None else environ
    runner = subprocess.run if runner is None else runner
    require(not environ.get('CCC_OVERRIDE_OPTIONS'), 'Nonempty CCC_OVERRIDE_OPTIONS changes effective argv')
    require(not any(environ.get(v) for v in ('CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH',
                'OBJC_INCLUDE_PATH')), 'Unrecorded include environment in current process')
    for command in commands:
        require(not any(a.startswith(('--config', '--no-default-config', '--driver-mode', '@'))
                        for a in command['arguments']), 'Config-dir/config/driver/response override in recorded argv')
    targets = sorted({a for c in commands for a in c['arguments'] if a.startswith('--target=')})
    require(targets == ['--target=armv7-w64-windows-gnu', '--target=thumbv7-pc-windows-msvc'],
            'Exact common/codec targets required for config discovery')
    evidence = []
    for target in targets:
        missing = '__a04_native_cache_missing_' + uuid.uuid4().hex + '.cfg'
        command = [str(clang), target, '--no-default-config', '--config='+missing,
                   '-fno-color-diagnostics', '-###', '-x', 'c', '-c', '-']
        result = runner(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE, text=True, timeout=20)
        expected_error = "configuration file '" + missing + "' cannot be found"
        errors = [line for line in result.stderr.splitlines() if 'error:' in line]
        require(result.returncode != 0 and len(errors) == 1 and expected_error in errors[0],
                'LLVM20 config discovery did not report the explicit missing config')
        directories = re.findall(r'^.*note: was searched for in the directory: (.+)$',
                                 result.stderr, re.M)
        require(1 <= len(directories) <= 3 and '\x1b' not in result.stderr,
                'LLVM20 config search directories not discoverable')
        require(win(Path(clang).parent) in {win(p) for p in directories},
                'Executable config search directory missing from LLVM20 discovery')
        checked = []
        for directory in directories:
            path = Path(directory)
            require(path.is_absolute(), 'Relative LLVM20 config search directory')
            try:
                path.stat()
            except FileNotFoundError:
                checked.append({'path':directory, 'present':False})
                continue
            require(path.is_dir(), 'Config search path is not a directory')
            # All direct .cfg names are rejected, covering target, mode and
            # target-mode combinations (including LLVM triple-version fallback).
            with os.scandir(path) as entries:
                count = 0
                for entry in entries:
                    count += 1
                    require(count <= 4096, 'Config directory entry bound')
                    require(not entry.name.lower().endswith('.cfg'),
                            'Clang default .cfg present in search directory: '+str(path))
            checked.append({'path':directory, 'resolved':str(path.resolve()),
                            'present':True, 'default_cfg_absent':True})
        evidence.append({'target':target, 'search_directories':checked,
                         'discovery_no_codegen':True, 'config_dir_overrides':False})
    return {'CCC_OVERRIDE_OPTIONS_empty':True, 'targets':evidence,
            'CLANG_NO_DEFAULT_CONFIG_current_nonempty':bool(environ.get('CLANG_NO_DEFAULT_CONFIG')),
            'default_cfg_policy':'reject-all-direct-cfg-in-LLVM-reported-search-directories',
            'historical_config_or_environment_absence_claimed':False}


def verify(args):
    repo = Path(args.repo).resolve()
    native = Archive(args.native_archive, args.expected_native_sha256)
    snapshot = Archive(args.input_snapshot, args.expected_input_sha256)
    ni = native.index('checkpoint-index.json')
    si = snapshot.index('snapshot-index.json')
    bm = native.json('build-manifest.json')
    cm = native.json('compile-manifest.json')
    compiled = bm['source_commit']
    require(ni['schema'] == 'PUBLIC379_COMPLETED_STAGE_CHECKPOINT_V1' and ni['stage'] == 'native',
            'Native checkpoint schema/stage')
    require(si['schema'] == 'NATIVE379_CURRENT_INPUT_SNAPSHOT_V1' and si['capture_after_compile'] is True
            and si['past_wrapper_environment_captured'] is False
            and si['historical_closure_claimed'] is False, 'Snapshot provenance/limits schema')
    require(all(x['source_commit'] == compiled for x in (cm, ni, si)), 'Compiled source binding')
    require(cm['compile_complete'] is True and bm['source_dirty'] is False
            and bm['target'] == cm['target'] == 'thumbv7-pc-windows-msvc'
            and bm['linkcheck_requested'] is False and cm['linkcheck_requested'] is False
            and bm['reused_objects'] == [], 'Native compile-only stage contract')
    require(digest(native.read('compile-manifest.json')) == ni['source_manifest_sha256'],
            'Compile manifest index binding')
    for a, b in [('native-build-manifest.json', 'build-manifest.json'),
                 ('native-compile-manifest.json', 'compile-manifest.json')]:
        require(snapshot.read(a) == native.read(b), 'Snapshot/native manifest byte binding')
    source_rows = bm['sources']
    require(len(source_rows) == 451 and len({win(x['path']) for x in source_rows}) == 451,
            'Expected 451 native source closure')
    for row in source_rows:
        rel = row['path'].replace('\\', '/')
        require(not PurePosixPath(rel).is_absolute() and '..' not in PurePosixPath(rel).parts,
                'Source path escaped repository')
        require((repo/rel).resolve().is_relative_to(repo) and not (repo/rel).is_symlink(), 'Native source link escape')
        require(sha(repo / rel) == row['sha256'].lower(), 'Current native source changed: ' + rel)
    recorded_gpu = {win(r['path']) for r in source_rows if win(r['path']).startswith('drivers/gpu/')}
    current_gpu = {win(p.relative_to(repo)) for p in (repo/'drivers/gpu/mt8167').rglob('*')
                   if p.is_file() and p.suffix in ('.c', '.h')}
    require(current_gpu == recorded_gpu, 'Current GPU source/header file-set changed')
    metadata = [('source-plan.json', 'third_party/mesa-pco/source-plan.json'),
                ('native-source-map.json', 'tools/ci/ge8300-native-source-map.json'),
                ('asset-source.json', 'tools/ci/ge8300-copy-usc-asset.json')]
    for a, b in metadata:
        require(snapshot.read(a) == (repo/b).read_bytes(), 'Current source metadata changed: ' + b)
    plan = snapshot.json('source-plan.json')
    require(len(plan['files']) == 605 and len({r['path'] for r in plan['files']}) == 605,
            'Expected 605 Mesa source closure')
    for row in plan['files']:
        rel = row['path']
        require(not PurePosixPath(rel).is_absolute() and '..' not in PurePosixPath(rel).parts,
                'Mesa source escaped repository')
        require((repo/rel).resolve().is_relative_to(repo) and not (repo/rel).is_symlink(), 'Mesa source link escape')
        require(sha(repo/rel) == row['sha256'].lower(), 'Current Mesa source/include changed: ' + rel)
    mesa_expected = {r['path'] for r in plan['files'] if r['path'].startswith('third_party/mesa-pco/')}
    mesa_expected |= {'third_party/mesa-pco/'+n for n in ('MESA-LICENSE.rst', 'upstream-local.patch', 'source-plan.json', 'README.md')}
    mesa_actual = {p.relative_to(repo).as_posix() for p in (repo/'third_party/mesa-pco').rglob('*') if p.is_file()}
    require(mesa_actual == mesa_expected, 'Mesa include/source file-set changed')
    roots = {r['name']: r for r in si['roots']}
    require(len(si['roots']) == 7 and set(roots) == set(ROOTS), 'Exact seven snapshot roots required')
    snapshot_metadata = {'asset-source.json', 'compiler-cache-verification.json', 'native-build-manifest.json',
                         'native-compile-manifest.json', 'native-source-map.json', 'sdk-view-inputs.json',
                         'source-plan.json', 'wrapper-command.json', 'snapshot-index.json'}
    require(all(p in snapshot_metadata or p.split('/')[0] in roots for p in snapshot.names),
            'Unexpected snapshot root/file')
    asset = snapshot.json('asset-source.json')
    b = bytes(asset['bytes'])
    require(len(b) == 152 and digest(b) == asset['sha256'] ==
            '5c68917faa051238b23674b64c6b137ef7ff75425dbde9dbc5e1e5a36e02c4a3', 'Generated asset source')
    generated = ('static const unsigned char a04_copy_usc_asset[152]={' +
                 ','.join('0x%02x' % x for x in b) + '};\n').encode('ascii')
    require(snapshot.read('generated-native/a04_copy_usc_asset.h') == generated, 'Generated asset bytes')
    require([x for x in snapshot.names if x.startswith('generated-native/')] ==
            ['generated-native/a04_copy_usc_asset.h'], 'Generated input file set')
    commands = cm['commands']
    require(commands == bm['commands'] and len(commands) == 67 and
            all(c['tool'] == 'clang' for c in commands), '67 actual compiler commands required')
    old_repo = commands[0]['arguments'][-3].replace('\\', '/').rsplit('/drivers/', 1)[0]
    # The recipe itself is one of the 451 immutable inputs. Derive the full enabled
    # unit sequence from its literal declarations, rather than trusting prior units.
    recipe = (repo/'tools/ci/build-ge8300-native-owner-core.ps1').read_text(encoding='utf-8-sig')
    units = [u for group in re.findall(r'\$units(?:\+)?=@\((.*?)\)', recipe, re.S)
             for u in re.findall("'([^']+)'", group)]
    require(len(units) == 67 and units == bm['units'], 'Native unit order/feature selection changed')
    source_map = snapshot.json('native-source-map.json')['native_unit_sources']
    # Exact fixed flags from the hash-bound native recipe; the two codec units use
    # the source plan instead of these common flags, including their different ABI.
    common = ['--target=thumbv7-pc-windows-msvc', '-std=c11', '-O2', '-fms-extensions',
              '-ffreestanding', '-fno-builtin', '-fno-stack-protector', '-fno-vectorize',
              '-fno-slp-vectorize', '-mno-unaligned-access', '-mfpu=none',
              '-Werror=implicit-function-declaration', '-Werror=incompatible-pointer-types',
              '-Werror=return-type', '-D_KERNEL_MODE', '-D_ARM_', '-DARM',
              '-DNTDDI_VERSION=0x06030000', '-D_WIN32_WINNT=0x0603',
              '-DA04_GE8300_VENDOR_NATIVE_CLEAR_JOB=1',
              '-D__ldrexd=__geo_wdk81_ldrexd_declaration']
    common += ['-I'+roots[t]['path'] for t in ('clang-resource20', 'wdk81-km')]
    common += ['-I'+roots['wdk81-km']['path']+'\\crt', '-I'+roots['wdk81-shared']['path']]
    gpu = old_repo+'/drivers/gpu/mt8167'
    common += ['-I'+gpu+s for s in ('', '/fwif', '/compat', '/vendor_m1_11')]
    common += ['-DA04_GE8300_UNIFIED_DISPLAY356=1', '-DA04_GE8300_NATIVE_OWNER_MDL_TARGET=1',
               '-DA04_GE8300_NATIVE_OWNER_DRAW=1', '-I'+roots['generated-native']['path']]
    require([win(x) for x in common] == [win(x) for x in bm['compile_flags']], 'Common compiler flags changed')
    codec = [u for u in plan['units'] if u['source'] == 'drivers/gpu/mt8167/wddm-render/compiler/pco_sm2.c']
    require(len(codec) == 1, 'Unique codec flags required')
    replacements = {'${REPO}': old_repo, '${VC_INCLUDE}': roots['msvc1444']['path'],
                    '${UCRT_INCLUDE}': roots['ucrt22621']['path'],
                    '${SDK_SHARED}': roots['wdk81-shared']['path'], '${SDK_UM}': roots['wdk81-um']['path']}
    codec_flags = []
    for item in codec[0]['arguments']:
        for token, value in replacements.items():
            item = item.replace(token, value.replace('\\', '/'))
        codec_flags.append(item)
    codec_flags += ['-DNDEBUG', '-mfpu=none', '-DA04_GE8300_UNIFIED_DISPLAY356=1']
    output_names = []
    # Only path-bearing operands get Windows path equivalence, never case-fold -D
    # values or arbitrary codegen options. All other bytes/order remain exact.
    def same_args(actual, expected):
        if len(actual) != len(expected):
            return False
        for i, (a, b) in enumerate(zip(actual, expected)):
            path_operand = a.startswith('-I') or (i > 0 and actual[i-1] in ('-I', '-isystem', '-c', '-o'))
            different = win(a) != win(b) if path_operand else a != b
            if different:
                return False
        return True
    for unit, command in zip(units, commands):
        args0 = command['arguments']
        flags = codec_flags if unit in ('wddm-render\\compiler\\witness_codec',
                                        'wddm-render\\compiler\\witness_info') else common
        name = unit.replace('\\', '-')+'.obj'
        expected = flags+['-c', old_repo+'/'+source_map[unit].replace('\\', '/'),
                         '-o', old_repo+'/build/current379/native/'+name]
        require(same_args(args0, expected), 'Per-unit compiler flags/source/output mismatch: ' + unit)
        output_names.append(name)
    require(same_args(bm['compile_flags'], common), 'Actual common flags mismatch')
    output_names.append('ge8300-native-owner-core.lib')
    require(set(native.names) == set(output_names) | {'compile-manifest.json', 'build-manifest.json',
            'checkpoint-index.json', 'build.log', 'objects.txt'}, 'Unexpected native checkpoint file set')
    require([x['path'] for x in cm['outputs']] == output_names and cm['outputs'] == bm['outputs']
            and ni['declared_output_order'] == output_names, 'Native output order/binding')
    hashes = []
    for row in cm['outputs']:
        b = native.read(row['path'])
        require(digest(b) == row['sha256'].lower(), 'Native output hash mismatch')
        if row['path'].endswith('.obj'):
            require(len(b) >= 20 and struct.unpack_from('<H', b)[0] == 0x1c4, 'Native object not ARMNT')
            hashes.append(digest(b))
    library_order(native.read(output_names[-1]), hashes, output_names[:-1])
    tools = {t['name']: t for t in si['tools']}
    require(set(tools) == {'clang', 'llvm-lib', 'llvm-readobj'}, 'Exact three native tools required')
    require(cm['tools'] == bm['tools'] and all(tools[t['name']]['sha256'].lower() == t['sha256'].lower()
            and win(tools[t['name']]['path']) == win(t['path']) for t in cm['tools']), 'Tool snapshot binding')
    libs = {win(t['path']): t['sha256'].lower() for t in si['libraries']}
    require(len(libs) == 4 and libs == {win(t['path']): t['sha256'].lower() for t in cm['wdk_inputs']},
            'Four kernel library bindings required')
    require(len(bm['wdk_inputs']) == 5 and bm['wdk_inputs'][:4] == cm['wdk_inputs'] and
            bm['wdk_inputs'][4]['sha256'].lower() == digest(snapshot.read('wdk81-km/ntddk.h')),
            'Native ntddk/WDK binding')
    pco = snapshot.json('compiler-cache-verification.json')
    require(pco['current_source_commit'] == compiled and pco['source_plan_sha256'] == digest(snapshot.read('source-plan.json'))
            and pco['all_sources_includes_flags_tools_API_identical'] is True, 'Before-native PCO receipt binding')
    current = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    report = {'schema': SCHEMA, 'admitted': False, 'offline_audit': bool(args.audit_only),
              'compiled_source_commit': compiled, 'current_source_commit': current,
              'current_source_closure_identical': True, 'native_archive_sha256': args.expected_native_sha256.lower(),
              'input_snapshot_sha256': args.expected_input_sha256.lower(),
              'native_build_manifest_sha256': digest(native.read('build-manifest.json')),
              'native_compile_manifest_sha256': digest(native.read('compile-manifest.json')),
              'native_library_sha256': digest(native.read('ge8300-native-owner-core.lib')),
              'compile_complete': True, 'linkcheck_requested': False, 'linkcheck_completed': False,
              'after_compile_snapshot': True, 'past_wrapper_environment_captured': False,
              'historical_per_TU_include_closure_claimed': False, 'new_codegen_invocations': 0,
              'clang_driver_queries': 0,
              'verified_native_objects': 67, 'objects_reused': 0, 'hardware_verified': False,
              'verified_source_files': 451, 'verified_mesa_plan_files': 605,
              'per_unit_flags_verified': True, 'library_member_order': 'reverse-manifest',
              'snapshot_capture_utc': si['captured_utc'],
              'evidence_class': 'immutable-task-origin-and-aftercompile-full-root-snapshot',
              'native_source_directory': old_repo}
    if args.audit_only:
        require(not args.configuration, 'Offline audit cannot use a live admission configuration')
        return report
    require(os.name == 'nt', 'Live cache admission requires Windows; offline audit is not admission')
    require(args.configuration and args.output, 'Live configuration/fresh output required')
    cfg = json.loads(Path(args.configuration).read_text(encoding='utf-8-sig'))
    require(cfg['unified_display'] is True and win(repo) == win(old_repo),
            'Unified/native source build root changed; no __FILE__ relocation certification')
    require(win(args.output) == win(roots['generated-native']['path'].rsplit('\\', 1)[0]), 'Native output path changed')
    require(set(cfg['roots']) == set(ROOTS)-{'generated-native'} and set(cfg['tools']) == set(tools),
            'Current root/tool configuration incomplete')
    # This check applies only to the opt-in live cache path. Never erase env or
    # alter historical argv to pretend that the old environment was captured.
    require(not os.environ.get('CCC_OVERRIDE_OPTIONS'), 'Nonempty CCC_OVERRIDE_OPTIONS changes effective argv')
    for tag, path in cfg['roots'].items():
        require(win(path) == win(roots[tag]['path']), 'Include search root relocated: ' + tag)
        targets = roots[tag]['root_target']
        target = targets[0] if targets else roots[tag]['path']
        require(win(Path(path).resolve()) == win(target), 'Current root target changed: ' + tag)
        expected = {p[len(tag)+1:]: (len(snapshot.read(p)), digest(snapshot.read(p)))
                    for p in snapshot.names if p.startswith(tag+'/')}
        require(tree(path) == expected, 'Current include file-set/bytes changed: ' + tag)
    for name, path in cfg['tools'].items():
        require(win(path) == win(tools[name]['path']) and sha(path) == tools[name]['sha256'].lower(),
                'Current tool changed: ' + name)
    for row in si['libraries']:
        require(sha(row['path']) == row['sha256'].lower(), 'Current kernel library changed')
    config_evidence = verify_clang_configuration(cfg['tools']['clang'], commands)
    require(not subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain'], text=True).strip(),
            'Current checkout must be clean')
    dest = Path(args.output)
    require(not dest.exists() and dest.resolve().is_relative_to(repo), 'Fresh checkout-local native output required')
    # Emit only after every live validation. Preserve all old manifest/log bytes.
    dest.mkdir(parents=True)
    for name in native.names:
        if name == 'checkpoint-index.json':
            continue
        p = dest/name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(native.read(name))
    (dest/'generated').mkdir()
    (dest/'generated/a04_copy_usc_asset.h').write_bytes(generated)
    report.update(admitted=True, objects_reused=67,
                  native_output_directory=str(dest.resolve()), current_inputs_verified=True,
                  effective_clang_configuration=config_evidence, clang_driver_queries=2)
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('repo', 'native-archive', 'expected-native-sha256', 'input-snapshot', 'expected-input-sha256', 'receipt'):
        p.add_argument('--'+key, required=True)
    p.add_argument('--configuration')
    p.add_argument('--output')
    p.add_argument('--audit-only', action='store_true')
    args = p.parse_args()
    receipt = Path(args.receipt)
    require(not receipt.exists(), 'Fresh receipt required')
    report = verify(args)
    receipt.parent.mkdir(parents=True, exist_ok=True)
    receipt.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
