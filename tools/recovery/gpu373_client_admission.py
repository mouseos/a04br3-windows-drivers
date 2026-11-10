"""Small admission shared by future generator/SCM/media gates; no profile cloning.
No capture, hive operation, compiler, deployment or guessed signed hashes.
"""
import hashlib,importlib.util,json,re,struct
from pathlib import Path
PROFILE='combined-features-fresh215-gpu373-clients-v1'
SERVICE='MT8167CombinedFollowup215'
CLIENT_SOURCE='5653962fb84a43ad3e27b4cbf1ca6aeac454cd77'
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
 require(m.get('signature_status')=='Valid' and m.get('signer',m.get('signer_thumbprint'))==SIGNER,'Native signing receipt Valid/BCBF required')
 require(m.get('sha256',m.get('exe_sha256'))==entry['file']['sha256'],'Signing receipt must bind exact leaf')
 b=path.read_bytes();require(len(b)>=64 and b[:2]==b'MZ','Actual PE required');at=struct.unpack_from('<I',b,60)[0]
 require(at+80<=len(b) and b[at:at+4]==b'PE\0\0' and struct.unpack_from('<H',b,at+4)[0]==0x1c4 and struct.unpack_from('<HH',b,at+72)==(6,3),'ARMNT/RT8.1 subsystem6.3')
 proof=read(pinned(root,entry['import_proof']))
 require(proof['candidate_sha256']==entry['file']['sha256'] and proof['machine']=='ARMNT' and proof['subsystem']==[6,3] and proof['missing']==[],'Actual RT8.1 export proof required')
 return m

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
 require(evaluator.get('source_commit')==CLIENT_SOURCE,'Approved373 evaluator source; no arbitrary replacement')
 require(launcher['evaluator_sha256']==pair['evaluator']['file']['sha256'] and launcher['GPU_calls_in_launcher']==0 and launcher['console_system_token_duplicate'] is True and launcher['user_token_substitution'] is False,'Launcher rebound to exact evaluator; session/token/GPU roles retained')
 replacements={}
 for role,name in (('launcher',LAUNCHER),('evaluator',EVALUATOR)):
  installed='Windows/System32/'+name;old=originals[installed];replacements[installed]={'baseline':old['host_ref'],'candidate':pair[role]['file'],'manifest':pair[role]['manifest'],'import_proof':pair[role]['import_proof']}
 wifi=None
 if selected:
  wifi=read(pinned(root,spec['wifi_update']))
  require(wifi['driver_version'].endswith('.1030') and wifi['driver_version']!=baseline['packages']['wifi']['driver_version'],'Only explicit new1030 tuple; version/hash remain signed input facts')
  loader=importlib.util.spec_from_file_location('client373_existing_packages',Path(__file__).with_name('prepare-peripheral-followup200.py'))
  existing=importlib.util.module_from_spec(loader);loader.loader.exec_module(existing)
  existing.base.package(root,wifi,'wifi') # Actual existing verifier; never accept a fabricated completed:true flag.
 return dict(schema='GPU373_CLIENT_TRANSITION_V1',profile=PROFILE,service=SERVICE,capture=spec['fresh'],native=spec['native'],supplement=spec['supplement'],runtime=runtime,client_replacements=replacements,
  retained_package_fingerprints={role:hashlib.sha256(json.dumps(row,sort_keys=True,separators=(',',':')).encode()).hexdigest() for role,row in baseline['packages'].items() if role!='wifi' or not selected},retained_kernel_manifest=baseline['GPU371_manifest'],retained_boot_sha256=BOOT,dxgi_logical_copy=dxgi['host_ref'],wifi_update=spec.get('wifi_update'),service_names=sorted(native['old_services']),named_value_ids=sorted(native['captured_named_values']),
  additional_integration_required=['generator/admission/SCM/native/media/controller must all consume the same transition','runtime preserved-file guards use candidate client hashes; WinPE pre-copy proofs retain captured baseline hashes'],build_performed=False,device_applied=False)

def validate_runtime_policy(policy,admission):
 """Fail-closed hook for the integration, not a broad existing-policy bypass."""
 frozen_kernel(policy);x=admission['runtime']
 for role,h in admission['retained_package_fingerprints'].items():require(hashlib.sha256(json.dumps(policy['packages'][role],sort_keys=True,separators=(',',':')).encode()).hexdigest()==h,'Retained INF/SYS/CAT/package metadata changed: '+role)
 require(policy['profile']==PROFILE and policy['service']==policy['data_directory']==SERVICE,'Fresh215 service identity')
 for k in ('install_targets','diagnostic_only','driver_updates','planned_reboots'):require(policy[k]==x[k],'Runtime mode mismatch: '+k)
 require(policy['first_install_mask']==policy['deferred_unbound_mask']==0,'No first-install/root PDO experiment')
 require(sorted(policy['service_preservation'])==admission['service_names'] and sorted(v['key']+'\\'+v['name'] for v in policy['captured_registry_values'])==admission['named_value_ids'],'Actual service/value sets, never old212 fingerprints')
 client=policy['diagnostic_clients'][0];require(client['exe']==LAUNCHER and client['phase']==1 and client['relative_directory']=='.' and len(client['dependencies'])==1 and client['dependencies'][0]['exe']==EVALUATOR,'Same Session1 launcher/client route')
 for name,row in admission['client_replacements'].items():
  require(policy['preserved_files'][name]==row['candidate']['sha256'] and policy['baseline_witnesses'][name]==row['baseline'],'Separate captured pre-copy guard from runtime new bytes')
 require(client['sha256']==admission['client_replacements']['Windows/System32/'+LAUNCHER]['candidate']['sha256'] and client['dependencies'][0]['sha256']==admission['client_replacements']['Windows/System32/'+EVALUATOR]['candidate']['sha256'],'Exact signed pair in runtime table')
