#!/usr/bin/env python3
"""実policyを新generatorへ通し、実C preserved/pathをhost API mockで検査する。"""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--policy', type=Path, required=True)
parser.add_argument('--work', type=Path, required=True, help='root内の新しい検査出力先')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(repo/'tools/recovery'))
workspace = repo.parents[1]
work = args.work.resolve()
assert work.is_relative_to(workspace) and not work.exists()
work.mkdir(parents=True)
spec = importlib.util.spec_from_file_location('phase_render', repo/'tools/recovery/render-signed-capture.py')
render = importlib.util.module_from_spec(spec)
spec.loader.exec_module(render)
policy_bytes = args.policy.read_bytes()
original = render.read(args.policy)
assert original['baselines']['render']['version_u64'] == 196687
assert original['packages']['render']['driver_version'] == '0.0.3.80'
key = 'Windows/System32/ge8300-umd.dll'
old = original['baseline_witnesses'][key]['sha256']
new = original['preserved_files'][key]
assert old != new, 'This regression needs actual OLD -> NEW UMD bytes'
old_file = workspace/original['baseline_witnesses'][key]['path']
new_file = workspace/original['preserved_sources'][key]
assert hashlib.sha256(old_file.read_bytes()).hexdigest() == old
assert hashlib.sha256(new_file.read_bytes()).hexdigest() == new
policy = copy.deepcopy(original)
render.bind_preserved_phases(policy)
assert policy['preinstall_preserved_files'][key] == old
assert policy['preserved_files'][key] == new
assert {name for name in policy['preserved_files']
        if policy['preserved_files'][name] != policy['preinstall_preserved_files'][name]} == {key}
header = render.generate(policy)
assert '#define A04_PRESERVED_PHASE_GUARD 1\n' in header
(work/'phase-policy.json').write_text(json.dumps(policy, indent=2)+'\n')
(work/'phase-policy.h').write_text(header)

for mutation in ('new-before-install', 'unrelated-before-change', 'old-witness-drift', 'no-contract'):
    bad = copy.deepcopy(policy)
    if mutation == 'new-before-install': bad['preinstall_preserved_files'][key] = new
    elif mutation == 'unrelated-before-change':
        name = next(name for name in bad['preinstall_preserved_files'] if name != key)
        bad['preinstall_preserved_files'][name] = '0'*64
    elif mutation == 'old-witness-drift': bad['baseline_witnesses'][key]['sha256'] = new
    else: del bad['preserved_phase_guard']
    try: render.generate(bad)
    except ValueError: pass
    else: raise AssertionError('Invalid phase policy admitted: '+mutation)
print('PASS actual OLD/NEW source bytes, explicit policy binding and four malformed-policy rejections')

source = (repo/'tools/winpe/mt8167-peripheral-update.c').read_text()
phase_definitions = source[source.index('#ifndef A04_PRESERVED_PHASE_GUARD'):
                           source.index('#ifndef A04_DIAGNOSTIC_ONLY')]
def function(signature):
    start = source.index(signature)
    begin = source.index('{', start)
    end, depth = begin+1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

transaction = function('static DWORD transaction(')
assert transaction.count('preserved(diagnostic)') == 1
assert transaction.index('preserved(diagnostic)') < transaction.index('installed_gpu_extras(0)')
assert transaction.index('preserved(diagnostic)') < transaction.index('deferred_install(')
assert transaction.rindex('preserved(1)') > transaction.index('deferred_install(')
assert 'preserved()' not in source
print('PASS real transaction selects preinstall at entry, candidate after DIF and on diagnostics')

prelude = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t DWORD; typedef unsigned char BYTE;
typedef wchar_t WCHAR; typedef void *HANDLE;
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define ERROR_INSUFFICIENT_BUFFER 122u
#define ERROR_INVALID_PARAMETER 87u
#define ERROR_INVALID_DATA 13u
#define ERROR_CRC 23u
#define A04_FEATURE148 1
#define A04_KEYPAD13_CHECKPOINT 0
#define A04_VIDPN_CALLBACK_TRACE 0
#define lstrcpyW wcscpy
#define lstrcmpiW wcscmp /* Fixture emits exactly the same names in both tables. */
struct preserved_file {const WCHAR *name;BYTE hash[32];};
static BYTE actual_umd[32];
static unsigned file_index,corrupt_fw,corrupt_retained,service_error,registry_error;
static DWORD GetSystemDirectoryW(WCHAR *out,DWORD n){const WCHAR *s=L"C:\\Windows\\System32";assert(wcslen(s)<n);wcscpy(out,s);return (DWORD)wcslen(s);}
static DWORD GetLastError(void){return 1;}
static int CloseHandle(HANDLE h){(void)h;return 1;}
static DWORD old_services(void){return service_error;}
static DWORD captured_registry_values(void){return registry_error;}
'''
mock = r'''
static DWORD deferred_file(const WCHAR *name,const BYTE expected[32],HANDLE *held){
 const WCHAR *leaf=wcsrchr(name,L'\\');const BYTE *actual;
 assert(file_index<sizeof(preserved_files)/sizeof(preserved_files[0]));
 actual=preserved_files[file_index++].hash; *held=INVALID_HANDLE_VALUE;
 if(leaf)++leaf;else leaf=name;
 if(!wcscmp(leaf,L"ge8300-umd.dll"))actual=actual_umd;
 else if(!wcscmp(leaf,L"ge8300-mips.fw")){if(corrupt_fw)return ERROR_CRC;}
 else if(corrupt_retained)return ERROR_CRC;
 return memcmp(actual,expected,32)?ERROR_CRC:0;
}
static DWORD check(int candidate){file_index=0;return preserved(candidate);}
'''

def table(text, name):
    marker = 'static const struct preserved_file '+name+'[]='
    start = text.index(marker)
    return text[start:text.index('};', start)+2]+'\n'

def digest_array(name, value):
    return 'static const BYTE '+name+'[32]={'+','.join('0x'+value[i:i+2] for i in range(0,64,2))+'};\n'

main = r'''
int main(void){
 memcpy(actual_umd,old_digest,32);
#if A04_PRESERVED_PHASE_GUARD
 assert(check(0)==0 && file_index==sizeof(preserved_files)/sizeof(preserved_files[0]));
 assert(check(1)==ERROR_CRC);
 memcpy(actual_umd,new_digest,32);assert(check(0)==ERROR_CRC);assert(check(1)==0);
 memset(actual_umd,0xff,32);assert(check(0)==ERROR_CRC && check(1)==ERROR_CRC);
 memcpy(actual_umd,old_digest,32);corrupt_fw=1;assert(check(0)==ERROR_CRC);
 memcpy(actual_umd,new_digest,32);assert(check(1)==ERROR_CRC);corrupt_fw=0;
 memcpy(actual_umd,old_digest,32);corrupt_retained=1;assert(check(0)==ERROR_CRC);
 memcpy(actual_umd,new_digest,32);assert(check(1)==ERROR_CRC);corrupt_retained=0;
 memcpy(actual_umd,old_digest,32);service_error=123;assert(check(0)==123);
 memcpy(actual_umd,new_digest,32);assert(check(1)==123);service_error=0;
 registry_error=124;assert(check(1)==124);registry_error=0;
#else
 /* Existing profiles without explicit phase opt-in retain the original table. */
 assert(check(0)==ERROR_CRC && check(1)==ERROR_CRC);
 memcpy(actual_umd,new_digest,32);assert(check(0)==0 && check(1)==0);
#endif
 assert(check(2)==ERROR_INVALID_PARAMETER);
 puts("PASS actual C guard: stage-specific exact hashes; retained files/FW/services/registry remain guarded");return 0;
}
'''

def run_case(label, text, mode, old_hash, new_hash, compile_failure=False):
    # Forward declaration permits the real preserved() to call the host API mock.
    code = prelude + table(text, 'preserved_files')
    if mode: code += table(text, 'preinstall_preserved_files')
    code += phase_definitions
    code += 'static DWORD deferred_file(const WCHAR *,const BYTE[32],HANDLE *);\n'
    code += function('static DWORD path(')+'\n'+function('static DWORD preserved(int candidate)')+'\n'
    code += mock+digest_array('old_digest',old_hash)+digest_array('new_digest',new_hash)+main
    c = work/(label+'.c'); exe = work/label
    c.write_text(code)
    flags = [] if mode is None else ['-DA04_PRESERVED_PHASE_GUARD='+str(mode)]
    build = subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
                            *flags,str(c),'-o',str(exe)],
                           capture_output=True,text=True)
    (work/(label+'-build.log')).write_text(build.stdout+build.stderr)
    if compile_failure:
        assert build.returncode != 0 and 'preserved_phase_counts_must_match' in build.stderr
        print(label+': PASS mismatched table count rejected at compile time')
        return
    assert build.returncode==0,build.stderr
    run = subprocess.run([str(exe)],capture_output=True,text=True)
    (work/(label+'-run.log')).write_text(run.stdout+run.stderr)
    assert run.returncode==0,run.stderr
    print(label+': '+run.stdout.strip())

run_case('actual-379-to-380',header,1,old,new)
run_case('legacy-no-opt-in',header,None,old,new)
# An unchanged extra is still strictly guarded in both phases, not waived.
retained = copy.deepcopy(policy)
retained['preserved_files'][key] = old
next(x for x in retained['packages']['render']['extra_members'] if x['name']=='ge8300-umd.dll')['sha256'] = old
render.bind_preserved_phases(retained)
retained_header = render.generate(retained)
retained_main = main
# Both phases now require old; exercise corruption/FW/service/registry cases too.
main = main.replace('assert(check(1)==ERROR_CRC);','assert(check(1)==0);',1)
main = main.replace('memcpy(actual_umd,new_digest,32);assert(check(0)==ERROR_CRC);assert(check(1)==0);',
                    'memcpy(actual_umd,new_digest,32);assert(check(0)==0);assert(check(1)==0);')
run_case('retained-extra-same-hash',retained_header,1,old,old)
main = retained_main
before_table = table(header, 'preinstall_preserved_files')
bad_name_header = header.replace(before_table, before_table.replace('{L"','{L"metadata-mismatch-',1))
main = '''int main(void){(void)old_digest;(void)new_digest;
assert(check(0)==ERROR_INVALID_DATA && file_index==0);
assert(check(1)==ERROR_INVALID_DATA && file_index==0);
puts("PASS both stages reject mismatched table names before file checks");return 0;}\n'''
run_case('mismatched-table-name',bad_name_header,1,old,new)
main = retained_main
rows = before_table.splitlines()
bad_count_header = header.replace(before_table, '\n'.join([rows[0]]+rows[2:])+'\n')
run_case('mismatched-table-count',bad_count_header,1,old,new,compile_failure=True)
assert args.policy.read_bytes() == policy_bytes
print('PASS input policy bytes unchanged; no VM/device/Windows API executed')
