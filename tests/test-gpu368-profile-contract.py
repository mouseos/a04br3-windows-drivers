#!/usr/bin/env python3
"""保存済み実署名manifestとproductionガードだけを検証。fresh200/profileは生成しない。"""
import argparse,ast,copy,importlib.util,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--signed-manifest',type=Path,required=True);p.add_argument('--globaldiag-directory',type=Path,required=True);a=p.parse_args();root=Path(__file__).resolve().parents[1]
sp=importlib.util.spec_from_file_location('recipe200',root/'tools/recovery/prepare-peripheral-followup200.py');m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m)
gm=json.loads(a.signed_manifest.read_text(encoding='utf-8-sig'));m.validate_gpu368_manifest(gm)
results=['actual signed GPU368 schema/source/runtime identity accepted']
import hashlib
h=a.globaldiag_directory
assert hashlib.sha256((h/'dxg-global-diag198.exe').read_bytes()).hexdigest()=='9be80b8176696302a0e4408e6871d41792c780a37bcf6caca817c817b97061ac'
assert hashlib.sha256((h/'signed-manifest.json').read_bytes()).hexdigest()=='1dcacdd1c70dacdc69a2e08e1294188d7805a2fbab77f9a24a73bcd28b1e28e6'
assert hashlib.sha256((h/'main-rt81-import-proof.json').read_bytes()).hexdigest()=='896137838e17694cc3cedc506ae31aa9b25e1fffd228f4094e2ca6145f7d3828'
helper=json.loads((h/'signed-manifest.json').read_text(encoding='utf-8-sig'));proof=json.loads((h/'main-rt81-import-proof.json').read_text(encoding='utf-8-sig'))
assert helper['executed'] is False and helper['RT81_exe_imports_verified'] is False and proof['missing']==[] and len(proof['imports'])==14 and all(x['validation']['status']=='present' for x in proof['imports'])
assert (1<<m.ROLES.index('render'))==1024
results+=['actual signed globaldiag SHA/source proof and all14 imports checked; original limits retained','render-only mask1024']
for name,change in [('old schema',dict(schema='GPU367_SIGN_ONLY_FROM_LINKED197_V1')),('mixed UMD source',dict(umd_source_commit='aa6613149e09b7b7be718c90103a34323c362917')),('hardware claim',dict(hardware_verified=True)),('wrong INF',dict(inf_driverver='11/10/2026,0.0.3.67'))]:
 bad=copy.deepcopy(gm);bad.update(change)
 try:m.validate_gpu368_manifest(bad)
 except ValueError:results.append(name+' rejected')
 else:raise AssertionError(name)
for path in ['tools/recovery/prepare-peripheral-followup200.py','tools/winpe/combined200_policy.py','tools/winpe/private_hardware_probe_input.py','tools/winpe/build-private-peripheral-batch-media.py']:ast.parse((root/path).read_text())
# 旧prepare196とfacade/service196はcanonical2bからbyte不変。
for path in ['tools/recovery/prepare-peripheral-followup196.py','tools/winpe/combined196_policy.py','tools/ci/build-combined196-service.ps1']:
 original=subprocess.check_output(['git','-C',str(root),'show','2b695765:'+path]);assert original==(root/path).read_bytes(),path
source=(root/'tools/winpe/mt8167-peripheral-update.c').read_text();assert '#if A04_GPU368_DUAL_DIAGNOSTICS' in source and '#define A04_GPU368_DUAL_DIAGNOSTICS 0' in source and 'L"globaldiag-result.bin":L"gpu-result.bin"' in source
assert 'i==0&&!combined_gpu_node_ready' in source and 'if(i==0){boundary=persist_before_gpu(first)' in source
assert 'i+1!=A04_DIAGNOSTIC_CLIENT_COUNT' in source # 旧profile guardも保持
results+=['Python syntax passed','canonical196 preparation/facade/build bytes unchanged','dual-phase1 boundary/result separation and Present readiness guards inspected']
print(json.dumps(dict(cases=results,fresh200_available=False,fresh200_profile_generated=False,Windows_compiled=False,hardware_verified=False)))
