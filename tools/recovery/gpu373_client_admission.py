"""Small admission shared by future generator/SCM/media gates; no profile cloning.
No capture, hive operation, compiler, deployment or guessed signed hashes.
"""
import hashlib,importlib.util,json,re,struct
from pathlib import Path
PROFILE='combined-features-fresh215-gpu373-clients-v1'
SERVICE='MT8167CombinedFollowup215'
CLIENT_SOURCE='22030aed65fc72fd39e6274ec6ea7b0c8c204521'
SIGNER='BCBF6C456C6F0DD0818434763B189488FBF13570'
BOOT='98de46d30349379669c4a7300d54d7652b40883f01febb10c5b65992a987e322'
LAUNCHER='mt8167-dxgi-console-launch363.exe'
EVALUATOR='mt8167-dxgi-present356.exe' # CLIENT_VERSION373 retains its public file name.
RENDER371='202f0f3fd7d4325df20dd8570d88299f008fae421bff16cccf99589282a19ded'
DISPLAY369='f7ec03b7a6ad87b3e042f96818080b1617ed7b68ff5e58d704bdd22158b79b5d'
def require(v,message):
 if not v:raise ValueError(message)
def mode(wifi=False):
 require(type(wifi) is bool,'Explicit WiFi selection')
 return dict(install_targets=['wifi'] if wifi else [],install_mask=8 if wifi else 0,
             diagnostic_only=not wifi,driver_updates=wifi,planned_reboots=int(wifi),
             render_DIF=0,display_DIF=0,boot_written=False,gpt_written=False,
             DWM372_selected=False,SCM_TUs=1)
def pinned(root,ref):
 require(isinstance(ref,dict) and set(('path','sha256'))<=set(ref),'Actual hash-bound input required')
 require(re.fullmatch('[0-9a-f]{64}',ref['sha256'] or '') and ref['sha256']!='0'*64,'Non-placeholder SHA required')
 p=(root/ref['path']).resolve(strict=True);require(p.is_relative_to(root) and p.is_file(),'Root-local input only')
 require(hashlib.sha256(p.read_bytes()).hexdigest()==ref['sha256'],'Actual input hash mismatch: '+ref['path']);return p
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def frozen_kernel(policy):
 require(policy['packages']['render']['driver_version']=='0.0.3.71' and policy['packages']['render']['hashes']['sys']==RENDER371,'Retain actual371 kernel; DWM372 is a different experiment')
 require(policy['packages']['display']['driver_version']=='0.0.3.69' and policy['packages']['display']['hashes']['sys']==DISPLAY369,'Retain transport369')
 require(policy['boot_transition']['boot_sha256']==BOOT and policy['boot_transition']['boot_write_required'] is False and policy['boot_transition']['gpt_written'] is False,'Retain boot98de/GPT')
def _signed_leaf(root,entry):
 path=pinned(root,entry['file']);m=read(pinned(root,entry['manifest']))
 require(m.get('schema')=='ge8300-one-TU-client-pair-v1' and m.get('success') is True and m.get('signature',{}).get('status')=='Valid' and m['signature']['thumbprint']==SIGNER,'Native signing receipt Valid/BCBF required')
 require(m['signed_sha256']==m['signature']['sha256']==entry['file']['sha256'] and m['non_signature_payload_unchanged'] is True and m['driver_package_modified'] is False,'Signing receipt must bind exact leaf')
 b=path.read_bytes();require(len(b)>=64 and b[:2]==b'MZ','Actual PE required');at=struct.unpack_from('<I',b,60)[0]
 require(at+80<=len(b) and b[at:at+4]==b'PE\0\0' and struct.unpack_from('<H',b,at+4)[0]==0x1c4 and struct.unpack_from('<HH',b,at+72)==(6,3),'ARMNT/RT8.1 subsystem6.3')
 proof=read(pinned(root,entry['import_proof']))
 require(proof['candidate_sha256']==entry['file']['sha256'] and proof['machine']=='ARMNT' and proof['subsystem']==[6,3] and proof['missing']==[] and proof['imports'] and all(x['validation']['status']=='present' for x in proof['imports']),'Actual RT8.1 export proof required')
 return m

def wifi_package(root,ref,baseline):
 """正式1030 signed-manifestを直接読み、既package検証器へ渡す行を導出。"""
 import copy
 path=pinned(root,ref);m=read(path)
 require(ref['sha256']=='d8a21808627b6fe75fb4a772a9acf96a30abe08f922fdf24a92e3f14315eccaf' and m['schema']=='mt6630-provider1030-incremental-signed-v1' and m['signature_status']=='Valid' and m['signer_thumbprint']==SIGNER,'正式1030 manifest/Valid署名')
 require(m['source_commit']=='4bcae1b684656eaafd7a45e7f0cf95a7385b74eb' and m['version']=='2026.11.11.1030' and m['driver_date']=='11/10/2026' and m['driver_compiled_source_commit']=='2b2e2b68e3868ae2c6e6abb29efa490ca4e0812e','実1030 source/INF tuple')
 row=copy.deepcopy(baseline);row.update(directory=path.parent.relative_to(root).as_posix(),manifest=ref['path'],manifest_sha256=ref['sha256'],source_commit=m['source_commit'],driver_version=m['version'],driver_date=m['driver_date'])
 loader=importlib.util.spec_from_file_location('client373_existing_packages',Path(__file__).with_name('prepare-peripheral-followup200.py'))
 existing=importlib.util.module_from_spec(loader);loader.loader.exec_module(existing)
 return existing.base.package(root,row,'wifi')

def admit(root,spec):
 """Returns a small transition admission, never a final policy/hive/manifest.
 Optional WiFi row is validated by the existing package validator at integration.
 """
 root=Path(root).resolve(strict=True)
 require(spec.get('schema')=='GPU373_CLIENT_UPDATE_ADMISSION_V1' and spec.get('capture_number')==215,'Explicit fresh215 client admission')
 require(spec.get('main_execution_approved') is True and spec.get('DWM372_selected') is False,'Main selects client373; no implicit DWM372 adoption')
 selected=spec.get('wifi_update') is not None;runtime=mode(selected)
 require(spec.get('install_targets')==runtime['install_targets'],'Only optional WiFi DIF is admitted')
 fresh_path=pinned(root,spec['fresh']);fresh=read(fresh_path);native=read(pinned(root,spec['native']))
 require(spec['fresh']['path'].endswith('/private-windows-rt-prebackup-run215/manifest.json') and fresh['status']=='captured-unmounted' and fresh['read_only'] and fresh['unmounted'] and fresh['errors']==[],'Actual completed fresh215, never relabel another capture')
 require(spec['fresh']['sha256']=='0c1b965c784d1c60aa9d07b2fc1baed81b2f49d8cc1d8e6182d263eafb5f8107' and fresh['recovery_boot_id']=='4548c4fb-a6ca-4785-a681-8b47b5fc0c03','Actual215 fixed capture/Recovery')
 require(native['source_manifest_sha256']==spec['fresh']['sha256'] and native['recovery_boot_id']==fresh['recovery_boot_id'] and native['capture_number']==215 and native['new_service_name']==SERVICE and native['new_service']['present'] is False,'Same fresh/native and new215 absent')
 require(native['originals_unchanged'] and native['registry_write_requested'] is False and native['full_service_capture_complete'] and native['errors']==[] and all(native[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Actual clean read-only inventory receipt')
 baseline=read(pinned(root,spec['baseline_policy']));frozen_kernel(baseline)
 supplement_path=pinned(root,spec['supplement']);supplement=read(supplement_path)
 require(supplement['source_manifest_sha256']==spec['fresh']['sha256'] and supplement['recovery_boot_id']==fresh['recovery_boot_id'] and supplement['status']=='captured-unmounted' and supplement['read_only'] and supplement['unmounted'] and supplement['errors']==[],'Same Recovery actual supplement')
 originals={}
 for mp,cap in ((fresh_path,fresh),(supplement_path,supplement)):
  for row in cap['files']:
   ref={'path':(mp.parent/row['backup_path']).relative_to(root).as_posix(),'sha256':row['sha256']};p=pinned(root,ref);require(p.stat().st_size==row['bytes'],'Actual capture size')
   if row['source_path'] in originals:require(originals[row['source_path']]['sha256']==row['sha256'],'Same Recovery original changed')
   originals[row['source_path']]=dict(row,host_ref=ref)
 for name,h in native['source_files'].items():require(originals['Windows/System32/config/'+name]['sha256']==h,'Same SYSTEM trio')
 dxgi=originals.get('Windows/System32/dxgi.dll');require(dxgi is not None,'Same capture logical dxgi.dll copy required; optional missing is not evidence')
 require(originals['Windows/System32/Drivers/ge8300-render.sys']['sha256']==RENDER371 and originals['Windows/System32/Drivers/mt8167-display-transport.sys']['sha256']==DISPLAY369,'Actual retained kernel bytes')
 pair=spec['client_pair'];require(set(pair)=={'launcher','evaluator'},'Exact client pair only')
 launcher=_signed_leaf(root,pair['launcher']);evaluator=_signed_leaf(root,pair['evaluator'])
 require(Path(pair['launcher']['file']['path']).name==LAUNCHER and Path(pair['evaluator']['file']['path']).name==EVALUATOR,'Retain stable launcher/evaluator file names')
 require(evaluator['phase']=='evaluator' and evaluator.get('runtime_source_commit')==CLIENT_SOURCE,'Approved373 evaluator source; no arbitrary replacement')
 require(launcher['phase']=='launcher' and launcher['evaluator_sha256']==pair['evaluator']['file']['sha256'] and launcher['evaluator_source_commit']==CLIENT_SOURCE and launcher['runtime_source_commit']=='f863841859fc7b1873f7d5bed7d1d1dd6f7187c9','Launcher rebound to exact evaluator; session/token/GPU roles retained')
 replacements={}
 for role,name in (('launcher',LAUNCHER),('evaluator',EVALUATOR)):
  installed='Windows/System32/'+name;old=originals[installed];replacements[installed]={'baseline':old['host_ref'],'candidate':pair[role]['file'],'manifest':pair[role]['manifest'],'import_proof':pair[role]['import_proof']}
 if selected:wifi_package(root,spec['wifi_update'],baseline['packages']['wifi'])
 return dict(schema='GPU373_CLIENT_TRANSITION_V1',profile=PROFILE,service=SERVICE,capture=spec['fresh'],native=spec['native'],supplement=spec['supplement'],runtime=runtime,client_replacements=replacements,
  retained_package_fingerprints={role:hashlib.sha256(json.dumps(row,sort_keys=True,separators=(',',':')).encode()).hexdigest() for role,row in baseline['packages'].items() if role!='wifi' or not selected},retained_kernel_manifest=baseline['GPU371_manifest'],retained_boot_sha256=BOOT,dxgi_logical_copy=dxgi['host_ref'],wifi_update=spec.get('wifi_update'),service_names=sorted(native['old_services']),named_value_ids=sorted(native['captured_named_values']),
  build_performed=False,device_applied=False)

def validate_runtime_policy(policy,admission):
 """Fail-closed hook for the integration, not a broad existing-policy bypass."""
 frozen_kernel(policy);x=admission['runtime']
 require(x==mode(bool(policy['install_targets'])) and admission['schema']=='GPU373_CLIENT_TRANSITION_V1' and admission['profile']==PROFILE and admission['service']==SERVICE,'Exact373 transition/mode only')
 for role,h in admission['retained_package_fingerprints'].items():require(hashlib.sha256(json.dumps(policy['packages'][role],sort_keys=True,separators=(',',':')).encode()).hexdigest()==h,'Retained INF/SYS/CAT/package metadata changed: '+role)
 require(policy['profile']==PROFILE and policy['service']==policy['data_directory']==SERVICE,'Fresh215 service identity')
 for k in ('install_targets','diagnostic_only','driver_updates','planned_reboots'):require(policy[k]==x[k],'Runtime mode mismatch: '+k)
 require(policy['first_install_mask']==policy['deferred_unbound_mask']==0,'No first-install/root PDO experiment')
 require(sorted(policy['service_preservation'])==admission['service_names'] and sorted(v['key']+'\\'+v['name'] for v in policy['captured_registry_values'])==admission['named_value_ids'],'Actual service/value sets, never old212 fingerprints')
 client=policy['diagnostic_clients'][0];require(client['exe']==LAUNCHER and client['phase']==1 and client['relative_directory']=='.' and len(client['dependencies'])==1 and client['dependencies'][0]['exe']==EVALUATOR,'Same Session1 launcher/client route')
 for name,row in admission['client_replacements'].items():
  require(policy['preserved_files'][name]==row['candidate']['sha256'] and policy['baseline_witnesses'][name]==row['baseline'],'Separate captured pre-copy guard from runtime new bytes')
 require(client['sha256']==admission['client_replacements']['Windows/System32/'+LAUNCHER]['candidate']['sha256'] and client['dependencies'][0]['sha256']==admission['client_replacements']['Windows/System32/'+EVALUATOR]['candidate']['sha256'],'Exact signed pair in runtime table')
