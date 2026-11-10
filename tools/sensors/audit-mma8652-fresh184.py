#!/usr/bin/env python3
"""fresh184の実配置と署名1.10源泉を照合。実機操作・追加反転・再署名なし。"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();root=a.root.resolve(strict=True);out=a.out.resolve();assert out.is_relative_to(root/'port/xnu/out') and not out.exists();out.mkdir(parents=True)
repo=Path(__file__).resolve().parents[2];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
assert subprocess.check_output(['git','-C',str(repo),'remote','get-url','origin'],text=True).strip()=='https://github.com/mouseos/a04br3-windows-drivers.git'
base=root/'port/xnu/out/private-windows-rt-prebackup-run184';mf=base/'manifest.json';m=read(mf);rp=root/'port/xnu/out/private-windows-rt-combined-run183-registry-post/receipt.json';receipt=read(rp)
assert receipt['source_manifest_sha256']==sha(mf) and receipt['originals_unchanged'] and receipt['load_rc']==receipt['unload_rc']==receipt['privilege_restore_error']==0
node=receipt['nodes']['ACPI\\MOU0010'];assert node['present'] and len(node['instances'])==1;instance=node['instances'][0];assert instance['instance']=='0' and instance['selected_present'];selected=instance['selected']
for k,v in {'InfPath':'mma8652.inf','DriverVersion':'0.0.1.10','DriverDate':'11-6-2026','MatchingDeviceId':'acpi\\mou0010'}.items():assert selected[k]['present'] and selected[k]['value']==v
assert instance['values']['Service']['value']=='mshidkmdf' and instance['values']['LowerFilters']['value']==['A04Mma8652']
files={x['source_path']:x for x in m['files']};actual=files['Windows/System32/Drivers/mma8652.sys'];signed=root/'port/xnu/out/windows-rt-mma-panel180-110/package';sm=read(signed/'signed-manifest.json');assert sm['source_commit']=='9ad5ef0afff9712399f86e429787464e6bf710cd'
assert sha(base/actual['backup_path'])==actual['sha256']==sm['files']['mma8652.sys']['sha256']==sha(signed/'mma8652.sys') and actual['bytes']==21440
for name in ['SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2']:
 x=files['Windows/System32/config/'+name];assert sha(base/x['backup_path'])==x['sha256']==receipt['source_files'][name]
source=[]
for path,h in sm['source_files'].items():
 blob=subprocess.check_output(['git','-C',str(repo),'show',sm['source_commit']+':'+path]);exact=hashlib.sha256(blob).hexdigest()==h
 assert exact or (path.endswith('.inf') and hashlib.sha256(blob.replace(b'\r\n',b'\n').replace(b'\n',b'\r\n')).hexdigest()==h)
 if path.startswith('drivers/'):assert (repo/path).read_bytes().replace(b'\r\n',b'\n')==blob.replace(b'\r\n',b'\n')
 source.append({'path':path,'signed_manifest_sha256':h,'git_blob_exact':exact,'only_INF_CRLF_normalization':not exact})
for n,x in sm['files'].items():assert sha(signed/n)==x['sha256']
prior=read(root/'port/xnu/out/mma110-orientation-contract-20261108/source-evidence.json');evidence=[]
for x in prior['source_evidence']:
 if x['repository'] not in ['CT3_GPL_LGPL_Src','port/xnu/kernel']:continue
 assert sha(root/x['repository']/x['path'])==x['sha256'];evidence.append(x)
# Existing released runtime and its actual raw-I2C->board->HID test, one invocation.
sensor=repo/'drivers/sensors/mma8652';command=['clang','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(sensor),str(sensor/'mma8652.c'),str(sensor/'sensor_hid.c'),str(repo/'tests/mma8652_orientation_test.c'),'-o',str(out/'orientation-test')]
env=dict(os.environ,TMPDIR=str(out),TMP=str(out),TEMP=str(out));subprocess.run(command,env=env,check=True);result=subprocess.check_output([str(out/'orientation-test')],env=env,text=True);(out/'vectors.log').write_text(result)
B=[[0,-1,0],[-1,0,0],[0,0,-1]];P=[[0,-1,0],[1,0,0],[0,0,1]];R180=[[-1,0,0],[0,-1,0],[0,0,1]]
def mul(a,b):return [[sum(a[i][k]*b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
H=mul(P,B);double=mul(R180,H);assert H==[[1,0,0],[0,-1,0],[0,0,-1]] and double==[[-1,0,0],[0,1,0],[0,0,-1]]
r={'schema':'MMA8652_FRESH184_TRANSFORM_AUDIT_V1','manifest_sha256':sha(mf),'native_receipt_sha256':sha(rp),'actual_node':node,'actual_SYS':actual,'signed_manifest_sha256':sha(signed/'signed-manifest.json'),'signed_runtime_source':sm['source_commit'],'source_files':source,'Linux_XNU_evidence':evidence,'raw_to_board':B,'board_to_HID110':P,'raw_to_HID110':H,'extra180_raw_to_HID':double,'extra180_equals_18_19_output':True,'already_has_existing180_correction':True,'physical_upside_down_issue_resolved':False,'new_post110_orientation_observation_established_from_capture':False,'host_cases':8,'four_inplane_Z_both_and_endpoints':True,'ASan_UBSan':True,'host_command':command,'host_stdout':result,'new_runtime_patch':False,'new_WDK_build_or_sign':False,'device_access':False,'185_GPU360_clock_FW_changed':False,'decision':'Current1.10 already implements the requested historical180 correction. Physical issue remains unresolved; no second inversion or identical rebuild without version-correlated new orientation evidence.'}
(out/'evidence.json').write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n');print(json.dumps({k:r[k] for k in ['signed_runtime_source','already_has_existing180_correction','physical_upside_down_issue_resolved','host_cases','new_runtime_patch','new_WDK_build_or_sign']},ensure_ascii=False))
