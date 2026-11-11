#!/usr/bin/env python3
"""Source/host rejection and legacy-default contracts; no future capture fixture."""
import argparse
import ast
import hashlib
import importlib.util
import json
import subprocess
from pathlib import Path

ap=argparse.ArgumentParser();ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args()
root=a.workspace.resolve(strict=True);out=(root/a.out).resolve();assert out.is_relative_to(root) and not out.exists()
here=Path(__file__).resolve().parent;repo=here.parents[1]
def load(name,path):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
m=load('render_contract',here/'render-signed-capture.py');ctl=load('render_engine',here/'run-gpu368-fresh200.py')
checks=[]
def reject(label,fn):
 try:fn()
 except (ValueError,KeyError,TypeError):checks.append(label);return
 raise AssertionError('Unexpected admission: '+label)
reject('pending ref',lambda:m.pin(root,None))
reject('zero digest',lambda:m.pin(root,dict(path='unacquired235',sha256='0'*64)))
reject('actual233 cannot become235',lambda:m.validate_baseline(root,dict(capture_number=233)))
reject('boolean capture rejected',lambda:m.validate_baseline(root,dict(capture_number=True)))
for name in ('render-update-233','render-update-234','render-update-1000'):
 reject('controller range '+name,lambda name=name:ctl.contract(name))
# Design-only path arithmetic is not an acquired capture or generated policy.
k=ctl.contract('render-update-235');assert k['fresh']==235 and k['run']==236 and k['capture']==237 and k['signed_render'] is True and k['schema']=='SIGNED_RENDER_RUN236_MAIN_SEAL_V1';checks.append('new controller arithmetic only')
actual=root/'port/gsi/logs/ios-feasibility/windows-rt-post233-native/main-recovered/out233/receipt.json'
ref=dict(path=actual.relative_to(root).as_posix(),sha256=m.sha(actual));assert m.pin(root,ref)['capture_number']==233
reject('actual233 SHA drift',lambda:m.pin(root,dict(ref,sha256='1'*64)))
oldmanifest=root/'port/xnu/out/windows-rt-vidpn-callback-candidate/kernel379/build/package379/signed-manifest.json'
oldref=dict(path=oldmanifest.relative_to(root).as_posix(),sha256=m.sha(oldmanifest))
reject('unreviewed old379 not signed380',lambda:m.candidate(root,dict(GPU_candidate=oldref,main_review=oldref,candidate_package_row=oldref)))
lock=m.read(m.LOCK)
row,leaves,manifest,review=m.candidate(root,lock)
assert row['driver_version']=='0.0.3.80' and len(leaves)==15 and leaves['ge8300-render.sys']==lock['SYS_sha256']
assert {x['name']:x['sha256'] for x in row['extra_members']}=={n:leaves[n] for n in ('ge8300-umd.dll','ge8300-mips.fw')}
assert {x['name']:x['installed'] for x in row['extra_members']}=={'ge8300-umd.dll':'ge8300-umd.dll','ge8300-mips.fw':'Drivers\\ge8300-mips.fw'}
captured=m.read(root/'port/xnu/out/private-windows-rt-prebackup-run233-gpu-assets/manifest.json')
primary=m.read(root/'port/xnu/out/private-windows-rt-prebackup-run233/manifest.json')
old={x['source_path']:x['sha256'] for x in captured['files']+primary['files']}
assert old['Windows/System32/Drivers/ge8300-mips.fw']==leaves['ge8300-mips.fw']
checks.append('firmware destination matches INF dir12 and actual captured runtime file')
checks.append('actual380 candidate fifteen leaves/INF/six saved signatures/main review/UMD-FW extras')
reject('changed candidate review ref',lambda:m.candidate(root,dict(lock,main_review=dict(lock['main_review'],sha256='1'*64))))
# The old controller functions are run, not reimplemented. No main()/device path.
old=subprocess.check_output(['git','show','7767f9fa:tools/recovery/run-gpu368-fresh200.py'],cwd=repo,text=True)
ns={'__name__':'legacy_contract','__file__':str(here/'run-gpu368-fresh200.py')};exec(compile(old,'legacy_controller','exec'),ns)
for name in list(ns['KNOWN_PROFILES'])+['peripheral-updates-233','peripheral-updates-235']:
 assert json.dumps(ns['contract'](name),sort_keys=True)==json.dumps(ctl.contract(name),sort_keys=True)
checks.append('all legacy controller contracts unchanged')
for f in ('prepare-peripheral-followup200.py','prepare-peripheral-followup184.py','peripheral-signed-updates.py','read-keypad13-checkpoint-loaded.ps1'):
 assert (here/f).read_bytes()==subprocess.check_output(['git','show','7767f9fa:tools/recovery/'+f],cwd=repo)
checks.append('old generator/peripheral adapter/reader bytes unchanged')
expected=subprocess.check_output(['git','show','a13bf474:tools/recovery/read-keypad13-checkpoint-loaded.ps1'],cwd=repo,text=True).replace('function Read-Keypad13Checkpoint(','function Read-Keypad14Checkpoint(')
assert m.READER.read_text()==expected;checks.append('keypad14 strict reader equals reviewed a13 except function name')
# Emission implementation is byte-identical; do not synthesize a future policy
# merely to reach the emitter's positive path without real235/signed380 inputs.
for p in [here/'render-signed-capture.py',here/'run-gpu368-fresh200.py',here.parent/'winpe/build-private-peripheral-batch-media.py',here.parent/'winpe/private_hardware_probe_input.py']:ast.parse(p.read_text())
checks.append('python source parse')
assert 'A04_KEYPAD14_CHECKPOINT 1' in (here/'render-signed-capture.py').read_text()
assert m.shared.ROLES.index('render')==10;checks.append('render-only mask1 maps to shared13 mask1024, not rtc')
out.mkdir(parents=True);(out/'result.json').write_text(json.dumps(dict(checks=checks,passed=len(checks),future_capture_created=False,final_header_generated=False,native_loads=0,device_operations=0,VM_operations=0,compiled=0,positive_finalization_unverified=True),indent=2)+'\n')
print('RENDER_SIGNED_CAPTURE_HOST_CONTRACT_PASS',len(checks))
