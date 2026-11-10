#!/usr/bin/env python3
"""署名候補からout専用fragmentを作成。媒体/worktree/実機は変更しない。"""
import argparse,datetime,hashlib,json,re
from pathlib import Path

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def prepare(root,out):
 root=root.resolve(strict=True);out=out.resolve();assert out.is_relative_to(root/'port/xnu/out') and not out.exists()
 base=root/'port/xnu/out/windows-rt-clock-snapshot1120-20261110';um=root/'port/xnu/out/windows-rt-clock-snapshot1120-um-20261110'
 m=read(base/'candidate/signed-manifest.json');u=read(um/'candidate/signed-manifest.json');pins={}
 def pin(p):pins[str(p.relative_to(root))]=sha(p)
 for p in [base/'main-independent-seal.json',um/'main-independent-seal.json',base/'candidate/signed-manifest.json',um/'candidate/signed-manifest.json'] :pin(p)
 assert read(base/'main-independent-seal.json')['signed_manifest_sha256']==sha(base/'candidate/signed-manifest.json')
 assert read(um/'main-independent-seal.json')['exe_sha256']==u['exe_sha256']
 packages={}
 for role in ['parent','thermal']:
  d=base/'candidate'/role;names={k:next(d.glob('*.'+k)).name for k in ['inf','sys','cat']};text=(d/names['inf']).read_text();match=re.search(r'^DriverVer=([^,]+),([^\r\n]+)',text,re.M);date,ver=match.groups();v=[int(x) for x in ver.split('.')];version=sum(x<<(16*(3-i)) for i,x in enumerate(v));day=datetime.datetime.strptime(date,'%m/%d/%Y');ft=(day-datetime.datetime(1601,1,1)).days*86400*10000000
  hashes={k:sha(d/n) for k,n in names.items()}
  for k,n in names.items():assert any(f['path']==role+'/'+n and f['sha256']==hashes[k] for f in m['files']);pin(d/n)
  packages[role]={'directory':str(d.relative_to(root)),'manifest':str((base/'candidate/signed-manifest.json').relative_to(root)),'manifest_sha256':sha(base/'candidate/signed-manifest.json'),'source_commit':m['source_commit'],'names':names,'driver_date':date,'driver_version':ver,'version_u64':version,'date_filetime':ft,'hashes':hashes,'main_seal':{'path':str((base/'main-independent-seal.json').relative_to(root)),'sha256':sha(base/'main-independent-seal.json')}}
 exe=um/'candidate/mt8167-clock-snapshot.exe';assert sha(exe)==u['exe_sha256'];pin(exe)
 client={'name':'clock_snapshot1120','directory':str(exe.parent.relative_to(root)),'exe':exe.name,'sha256':u['exe_sha256'],'arguments':['--snapshot'],'timeout_ms':10000,'phase':0,'relative_directory':'.','manifest':str((um/'candidate/signed-manifest.json').relative_to(root)),'dependencies':[]}
 fragment={'schema':'MT8167_CLOCK1120_BATCH_FRAGMENT_V1','packages':packages,'diagnostic_clients_prepend':[client],'install_targets_add':['parent','thermal'],'install_mask_add_hex':'0x42','role_indices':{'parent':6,'thermal':1},'diagnostic_client_count_add':1,'GPU_client_index_shift':1,'GPU_phase1_must_be_last':True,'hardware_operations_allowed':False,'raw_relative_to_base_directory':'mt8167-clock-snapshot-v1.bin','stdout_relative_to_base_directory':'logs/clock_snapshot1120.stdout.txt','stderr_relative_to_base_directory':'logs/clock_snapshot1120.stderr.txt','raw_bytes':128,'actual_capture':False,'effective_CPU_Hz':None,'DDR_Hz':None,'input_pins':[{'path':p,'sha256':h} for p,h in sorted(pins.items())]}
 out.mkdir(parents=True)
 collection={'schema':'MT8167_CLOCK1120_COLLECTION_FRAGMENT_V1','service_binding':'Final185 policy service/data_directory, never historical182','raw_path':'Windows/System32/{data_directory}/mt8167-clock-snapshot-v1.bin','raw_exact_bytes':128,'stdout_path':'Windows/System32/{data_directory}/logs/clock_snapshot1120.stdout.txt','stderr_path':'Windows/System32/{data_directory}/logs/clock_snapshot1120.stderr.txt','service_log_path':'Windows/System32/{data_directory}/logs/service.log','max_service_log_bytes':1048576,'max_stdout_bytes':4096,'max_stderr_bytes':4096,'actual_UM_path':'Windows/System32/mt8167-clock-snapshot.exe','actual_UM_sha256':u['exe_sha256'],'actual_SYS_paths':{r:'Windows/System32/Drivers/'+v['names']['sys'] for r,v in packages.items()},'main_only':True,'collect_missing_partial_failures':True,'delete_overwrite_retry':False,'sameboot_join_pending_main':True}
 (out/'collection-contract.json').write_text(json.dumps(collection,indent=2)+'\n')
 values=[client['exe'],'--snapshot',client['name'],'.'];row='{'+','.join('L'+json.dumps(x) for x in values)+',{' + ','.join('0x'+client['sha256'][i:i+2] for i in range(0,64,2))+'},10000u,0u},\n'
 (out/'diagnostic-client-row.inc').write_text('/* Insert as diagnostic_clients[0]; keep original GPU phase1 last. */\n'+row)
 (out/'package-map-rows.json').write_text(json.dumps(packages,indent=2)+'\n');(out/'diagnostic-client.json').write_text(json.dumps(client,indent=2)+'\n');(out/'fragment.json').write_text(json.dumps(fragment,indent=2)+'\n');return fragment
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();prepare(a.root,a.out)
