#!/usr/bin/env python3
"""Fresh188 console-only followup: existing GPU362, one corrected session bridge."""
from pathlib import Path
import argparse,copy,importlib.util,json,sys,struct
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'winpe'))
from peripheral_batch_policy import rooted,sha,read,require,cstr
_s=importlib.util.spec_from_file_location('_console188_base',Path(__file__).with_name('prepare-peripheral-followup184.py'));_b=importlib.util.module_from_spec(_s);_s.loader.exec_module(_b)
PROFILE='combined-features-fresh188-console363-v1';SERVICE=DIRECTORY='MT8167CombinedFollowup188';INSTALL=[];ROLES=_b.ROLES;TARGETS=_b.TARGETS
_b.PROFILE=PROFILE;_b.SERVICE=_b.DIRECTORY=SERVICE;_b.INSTALL=INSTALL
FRESH_SHA='52887e6dcb4e7708c532e5296c340490f30dfd6435c3153f67f21a60f33a359f';BOOT_SHA='0943ab8a6008942d9701140542da6ecaa902f750bf34b864bb6a66808c08df42';GPU_SHA='5d96d2db8743afeb6b413443575b4588ef8c4265312d1148afc8c3c08afbcfbb'
def paths():return _b.paths()
def build_policy(root,spec):
 pins={}
 def pin(ref):
  q=rooted(root,ref['path']);require(sha(q)==ref['sha256'],'Input hash '+ref['path']);pins[ref['path']]=ref['sha256'];return read(q)
 require(spec['profile']==PROFILE and spec['install_targets']==[] and spec['hardware_owner']=='main','Diagnostic-only scope')
 fm=pin(spec['fresh']);require(spec['fresh']['sha256']==FRESH_SHA and fm['status']=='captured-unmounted' and fm['read_only'] and fm['unmounted'] and not fm['errors'] and len(fm['files'])==42 and fm['recovery_boot_id']=='f1f9f981-b3c5-410b-a7e8-10fa207dee93','Exact fresh188')
 originals={f['source_path']:f for f in fm['files']}
 for f in fm['files']:
  q=rooted(root,Path(spec['fresh']['path']).parent/f['backup_path']);require(sha(q)==f['sha256'] and q.stat().st_size==f['bytes'],'Fresh original');pins[q.relative_to(root).as_posix()]=sha(q)
 n=pin(spec['native']);prior=pin(spec['prior187_policy']);launcher=pin(spec['launcher_manifest'])
 require(n['source_manifest_sha256']==FRESH_SHA and not n['errors'] and n['originals_unchanged'] and n['full_service_preservation_equal'] and all(n[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Clean actual188 receipt')
 require(prior['profile']=='combined-features-fresh186-v1' and n['old_services']==prior['service_preservation'] and len(n['old_services'])==18,'18 full services')
 d=n['combined']['Diagnostic']['values'];require(d['Complete']['value']==1 and d['ObservedResult']['value']==1 and d['RunId']['value']==58952 and d['ServicePid']['value']==1520,'Actual187 failed bridge run, not GPU failure')
 node=struct.unpack('<104I',bytes.fromhex(d['Nodes']['value']))[80:88];require(node[:3]==(1,0,0) and node[3]&8 and node[4:7]==(0,0,0),'Actual GPU started/problem0')
 require(originals['Windows/System32/Drivers/ge8300-render.sys']['sha256']==GPU_SHA and n['unified_display_bindings']['ACPI\\MOU0006']['matches'],'Actual GPU362')
 require(n['post_ownership_policy360']['raw']=='00000000','POST0 retained')
 for name,h in n['source_files'].items():require(originals['Windows/System32/config/'+name]['sha256']==h,'SYSTEM trio')
 values=copy.deepcopy(prior['captured_registry_values']);require(len(values)==100,'Prior100 values')
 for v in values:
  got=n['captured_named_values'][v['key']+'\\'+v['name']];require(got['type']==v['type'] and got['raw_hex']==v['raw_hex'],'Prior named value')
 previous=n['new_service'];require(previous['present'] and previous['values']['Start']['value']==4,'Old186 disabled');values+=_b.value_contract('SYSTEM\\CurrentControlSet\\Services\\MT8167CombinedFollowup186',previous['values']);require(len(values)==105,'Exactly105 named')
 require(launcher['signature_status']=='Valid' and launcher['signer']=='BCBF6C456C6F0DD0818434763B189488FBF13570' and launcher['compiled_TUs']==1 and not launcher['hardware_verified'],'Signed fixed bridge')
 ld=rooted(root,spec['launcher_directory']);lp=ld/'mt8167-dxgi-console-launch363.exe';require(sha(lp)==launcher['sha256'],'Bridge bytes');pins[lp.relative_to(root).as_posix()]=sha(lp)
 evaluator='mt8167-dxgi-present356.exe';eh='eaa30760ebc38a1955ccb3bbd834ee5c3972ce8842b90e6a58bc5084109ef8d5';require(sha(ld/evaluator)==eh,'Fixed retained evaluator');pins[(ld/evaluator).relative_to(root).as_posix()]=eh
 p=copy.deepcopy(prior);p.update(profile=PROFILE,service=SERVICE,data_directory=SERVICE,backup_manifest=spec['fresh']['path'],backup_manifest_sha256=FRESH_SHA,recovery_boot_id=fm['recovery_boot_id'],capture_selection={'schema':1,'profile':PROFILE,'capture_number':188,'manifest_sha256':FRESH_SHA,'recovery_boot_id':fm['recovery_boot_id'],'gpu_policy_sha256':n['policy_sha256'],'unchanged_since_capture':True},source_files={name:{k:originals['Windows/System32/config/'+name][k] for k in ('bytes','sha256')} for name in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},service_preservation=n['old_services'],captured_registry_values=values,previous_combined186_service=previous,install_targets=[],planned_reboots=0,diagnostic_only=True,driver_updates=False,baseline188_module_identity_reference_only=n['module_identity'],baseline188_init_reference_only=n['gpu_init'])
 p.pop('previous_combined184_service',None);p.pop('baseline186_module_identity_reference_only',None);p.pop('baseline186_init_reference_only',None)
 p['baselines']={};preserved={};sources={}
 for name,f in originals.items():
  if name.startswith('Windows/System32/Drivers/') or (name.startswith('Windows/System32/') and name.endswith('.exe')):preserved[name]=f['sha256'];sources[name]=(Path(spec['fresh']['path']).parent/f['backup_path']).as_posix()
 for role in ROLES:
  pkg=p['packages'][role];row=n['nodes'][TARGETS[role][0]];require(row['present'] and len(row['instances'])==1,'One role node '+role);selected=n['bt_start']['selection'] if role=='bt' else row['instances'][0]['selected'];require(selected['DriverVersion']['value']==pkg['driver_version'] and originals['Windows/System32/Drivers/'+TARGETS[role][3]]['sha256']==pkg['hashes']['sys'],'Retain actual '+role)
  p['baselines'][role]={'mode':'retained','version_u64':pkg['version_u64'],'date_filetime':pkg['date_filetime'],'sys_sha256':pkg['hashes']['sys'],'inf':selected['InfPath']['value']}
  for name in list(pkg['names'].values())+[x['name'] for x in pkg.get('extra_members',[])]:
   q=rooted(root,Path(pkg['directory'])/name);pins[q.relative_to(root).as_posix()]=sha(q)
  q=rooted(root,pkg['manifest']);require(sha(q)==pkg['manifest_sha256'],'Retained package manifest');pins[q.relative_to(root).as_posix()]=sha(q)
 for x in p['packages']['render']['extra_members']:
  name='Windows/System32/'+x['installed'].replace('\\','/');q=rooted(root,Path(spec['diagnostics'])/'fixed'/name);require(sha(q)==x['sha256'],'Actual retained GPU extra');x['baseline_sha256']=x['sha256'];preserved[name]=sha(q);sources[name]=q.relative_to(root).as_posix();pins[q.relative_to(root).as_posix()]=sha(q)
 for name in ['MT8167CombinedFollowup186.exe','mt8167-dxgi-console-launch356.exe',evaluator]:
  rel='Windows/System32/'+name;q=rooted(root,Path(spec['diagnostics'])/'fixed'/rel);preserved[rel]=sha(q);sources[rel]=q.relative_to(root).as_posix();pins[q.relative_to(root).as_posix()]=sha(q)
 p.update(preserved_files=preserved,preserved_sources=sources,baseline_witnesses={})
 client=copy.deepcopy(prior['diagnostic_clients'][0]);client.update(directory=spec['launcher_directory'],exe='mt8167-dxgi-console-launch363.exe',sha256=launcher['sha256'],manifest=spec['launcher_manifest']['path']);require(client['phase']==1 and client['arguments']==[],'One console client');p['diagnostic_clients']=[client]
 p['observer_contract'].update(fresh_basis=188,planned_reboots=0,GPU_client_index=0,diagnostic_clients=1)
 p['service_baseline_generation']['source']=spec['native'];p['standard_present_contract'].update(launcher_source_commit=launcher['source_commit'],diagnostic_clients=1,execution_index=0)
 p['display_transition'].update(DIF_order=[],old_DOD_retirement='No DIF; retained healthy GPU362/DIS0')
 require(p['boot_transition']['boot_sha256']==BOOT_SHA and not p['boot_transition']['boot_write_required'] and not p['boot_transition']['gpt_written'],'Firmware and GPT retained')
 # Historical clear/UMD files remain observations, not install admission or new success.
 p['gpu_client_log_baseline']=spec['mutable_baseline'];baseline=pin(spec['mutable_baseline']);require(baseline['source_transaction']=='run187','Latest log prefix');mutable=[]
 for name,h in baseline['files'].items():
  q=rooted(root,Path(spec['diagnostics'])/name);require(sha(q)==h,'Saved historical log');pins[q.relative_to(root).as_posix()]=h;mutable.append({'relative_path':'MT8167D3DClear/'+name,'source':q.relative_to(root).as_posix(),'sha256':h,'prefix_bytes':q.stat().st_size})
 bt=p['bt_first_install']
 for f in bt['files']:
  q=rooted(root,Path(bt['directory'])/f['name']);require(sha(q)==f['sha256'],'Retained BT catalog input');pins[q.relative_to(root).as_posix()]=sha(q)
 for ref in [dict(path=bt['manifest'],sha256=bt['manifest_sha256']),bt['native_baseline'],bt['main_seal']]:pin(ref)
 p['mutable_diagnostic_logs'].update(files=mutable,baseline=spec['mutable_baseline']);p['input_pins']=[{'path':k,'sha256':v} for k,v in sorted(pins.items())];return p

def load_map(path,root):return build_policy(root,read(path))
def generate(p):
 text=_b.generate(p);require(text.count('#define A04_DIAGNOSTIC_ONLY 0')==1 and text.count('#define A04_DIAGNOSTIC_CLIENT_COUNT 2u')==1,'Known source generator');return text.replace('#define A04_DIAGNOSTIC_ONLY 0','#define A04_DIAGNOSTIC_ONLY 1').replace('#define A04_DIAGNOSTIC_CLIENT_COUNT 2u','#define A04_DIAGNOSTIC_CLIENT_COUNT 1u')
def assert_paths(image,capture=False):
 b=Path(image).read_bytes()
 for s in [SERVICE,SERVICE+'.exe','mt8167-dxgi-console-launch363.exe','mt8167-msdc.sys']:
  require(s.encode('utf-16le')+b'\0\0' in b,'Fixed runtime path '+s)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--map',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--finalize',action='store_true');a=ap.parse_args();require(a.finalize,'Explicit finalize');root=a.workspace.resolve(strict=True);p=load_map(rooted(root,a.map),root);out=a.out.resolve();require(out.is_relative_to(root) and not out.exists(),'New root-local output');out.mkdir(parents=True)
 (out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(generate(p))
 for name in ['peripheral-policy.json','peripheral-preservation-plan.json']:(out/name).write_text(json.dumps(p,indent=2)+'\n')
 print('CONSOLE_ONLY_FRESH188_FINALIZED_DRIVER_UPDATES0_REBOOTS0')
if __name__=='__main__':main()
