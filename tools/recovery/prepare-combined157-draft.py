#!/usr/bin/env python3
"""実fresh156基点GPU344-only模板。未完成artifactでarm/媒体を生成しない。"""
import argparse, hashlib, json, struct
from pathlib import Path
FRESH=Path('port/xnu/out/private-windows-rt-prebackup-run156/manifest.json')
FRESH_SHA='9f41dccf1052fb0401b76acb94d14cb70c05c9c94d9c25736c3960f5106ddb0a'
RECOVERY='1fc923b6-71b1-46e4-862a-b729086e2f4f'
PRIOR=Path('port/xnu/out/windows-rt-combined155-fresh154/main-deployment-contract.json')
NATIVE=Path('port/xnu/out/private-windows-rt-combined-run155-registry-post/receipt.json')
BASELINE=Path('port/gsi/logs/ios-feasibility/windows-rt-fresh156-baseline-client-files.json')

def require(ok,why):
 if not ok:raise ValueError(why)
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def ref(root,relative,expected=None):
 p=(root/str(relative).replace('\\','/')).resolve(strict=True)
 require(p.is_relative_to(root) and p.is_file(),'Root-local existing input required')
 h=hashlib.sha256(p.read_bytes()).hexdigest();require(expected is None or h==expected,'Input pin mismatch: '+str(relative))
 return {'path':p.relative_to(root).as_posix(),'sha256':h,'bytes':p.stat().st_size}
def values154(native):
 v=native['new_service'];require(v['present'] and v['values']['Start']=={'kind':'DWord','value':4,'present':True},'Actual service154 Start4 required')
 require(v['values']['ImagePath']['value']=='"C:\\Windows\\System32\\MT8167CombinedFollowup154.exe"','Exact prior service image required')
 result=[]
 for name,x in v['values'].items():
  require(x['present'] and name in ('Start','ImagePath','Type','ObjectName','DependOnService'),'Exact prior154 values required')
  k=x['kind'];data=x['value']
  if k=='DWord':kind=4;raw=struct.pack('<I',data)
  elif k in ('String','ExpandString'):kind=1 if k=='String' else 2;raw=(data+'\0').encode('utf-16le')
  elif k=='MultiString':kind=7;raw=('\0'.join(data)+'\0\0').encode('utf-16le')
  else:raise ValueError('Unknown actual named value type')
  result.append({'key':'SYSTEM\\CurrentControlSet\\Services\\MT8167CombinedFollowup154','name':name,'type':kind,'raw_hex':raw.hex()})
 require(len(result)==5,'Five actual154 values required');return result

def make_draft(root):
 f=ref(root,FRESH,FRESH_SHA);m=read(root/f['path'])
 require(m['status']=='captured-unmounted' and m['read_only'] and m['unmounted'] and not m['errors'] and m['recovery_boot_id']==RECOVERY and len(m['files'])==41,'Exact fresh156 required')
 originals=[]
 for x in m['files']:
  r=ref(root,FRESH.parent/x['backup_path'],x['sha256']);require(r['bytes']==x['bytes'],'Original156 size mismatch');originals.append({'source_path':x['source_path'],**r})
 bypath={x['source_path']:x for x in originals}
 require(bypath['Windows/System32/Drivers/ge8300-render.sys']['sha256']=='e884f33652078618ead6e02faaeccf606e15e9d3074e0cffabe681c8eb1db898','Actual343 required')
 require(bypath['Windows/System32/Drivers/mt8167-shared.sys']['sha256']=='bbced952c499ba0588c77b861b20141043929773d703cdd327ffa92e61990979','Actual1117 required')
 nr=ref(root,NATIVE);n=read(root/nr['path'])
 require(n['source_manifest_sha256']==FRESH_SHA and not n['errors'] and n['originals_unchanged'] and all(n[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Clean actual156 native receipt required')
 for name,h in n['source_files'].items():require(h==bypath['Windows/System32/config/'+name]['sha256'],'Same captured trio required')
 init=n['gpu_init'];require(init['reached_mask']==0x7ff,'Actual343 ReachedMask7ff required')
 for name in ('DriverEntry','Contract','DxgkInitialize','AddDevice','DeviceInformation','NativeOwnerCreate','ConfigureAperture','StartOwner','DrawOpsV1','DrawOpsV2','StartDevice'):
  require(init['values'][name]['present'] and init['values'][name]['value']==0,'Actual reached init status required')
 priorref=ref(root,PRIOR);prior=read(root/priorref['path']);require(prior['transaction']=='combined-run155' and prior['boot_write_required'] and prior['matched_boot_sha256']=='2e6cba6ab96cd5b586cefbd4bd004c6cee0484866ca6710ea454144da568c451','Prior matched155 reference required')
 policyref=ref(root,Path(prior['policy_file']),prior['policy_file_sha256']);policy=read(root/policyref['path'])
 require(n['old_services']==policy['service_preservation'] and len(n['old_services'])==18,'18 actual preserved service fingerprints required')
 for v in policy['captured_registry_values']:
  actual=n['captured_named_values'][v['key']+'\\'+v['name']]
  require(actual['type']==v['type'] and actual['bytes']==len(bytes.fromhex(v['raw_hex'])) and actual['raw_hex']==v['raw_hex'],'Actual prior named value mismatch')
 retained_values=policy['captured_registry_values']+values154(n);require(len(retained_values)==25,'18 full plus25 actual raw values required')
 for role in ('parent','thermal','audio','wifi','msdc'):
  e=policy['packages'][role];require(e['hashes']['sys']==bypath['Windows/System32/Drivers/'+e['names']['sys']]['sha256'],'Retained driver mismatch: '+role)
 require(policy['packages']['thermal']['driver_version']=='0.0.1.10','Actual thermal1.10 required')
 br=ref(root,BASELINE);baseline=read(root/br['path']);require(baseline['capture']==156 and baseline['previous_logs_preserved']==67 and len(baseline['files'])==69,'Exact156 log baseline required')
 old=read(root/'port/xnu/out/windows-rt-combined155-fresh154/mutable-baseline154.json')['files']
 require(all(baseline['files'].get(k)==h for k,h in old.items()) and len(set(baseline['files'])-set(old))==2,'Old67 unchanged plus2 real new logs required')
 logs=[]
 for name,h in baseline['files'].items():
  require(Path(name).name==name and name.startswith(('clear-','umd-')) and name.endswith('.log'),'Fixed diagnostic leaf required')
  logs.append(ref(root,Path('port/xnu/out/private-windows-rt-combined-run155-diagnostics')/name,h))
 client=policy['diagnostic_clients'][1].copy();require(client['name']=='gpu_suite' and client['sha256']=='9934ba6bf9741630a544bf096ea3261c717d45873f91b899cc7e3eae62466add' and client['phase']==1 and client['timeout_ms']==900000,'One-shot9934 required');ref(root,Path(client['directory'])/client['exe'],client['sha256']);client['relative_directory']='MT8167CombinedFollowup156\\clients'
 return {'schema':1,'transaction':'combined-run157','profile':'combined-features-fresh156-DRAFT','draft':True,'arm_allowed':False,'normal_install_allowed':False,'media_generation_allowed':False,'hardware_owner':'main','hardware_operations_allowed':False,
 'capture_selection':{'required_capture_number':156,'fresh':f,'recovery_boot_id':RECOVERY,'same_recovery_unchanged_proof':None,'original_files':originals,'native_actual':nr,'actual_module_identity':n['module_identity'],'actual_init_reference_only':init},
 'prior155_reference':{'contract':priorref,'policy':policyref,'main_reported_result':{'thermal_first_sample_C':[44.9,41.1,41.7],'thermal_phase':7,'thermal_status_all_zero':True,'GPU_PID':2536,'GPU_firstCopy_unexpected_reset':True,'BOOT_REASON':9,'GPU_complete_success':False}},
 'GPU344_candidate':{'requested_recipe_source':'0feb67ff','APC_guard_source':'17af96fe','directory':'port/xnu/out/windows-rt-durable-progress344-guarded','registry_IO_requires_PASSIVE_and_special_APCs_enabled':True,'first81b49ee9_artifacts_adopted':False,'branch':'work/ge8300-durable-progress344','artifact':None,'main_seal':None,'runtime_buildid':None,'selected_for_deployment':False,'reported_changed_TUs':4,'purpose':'durable existing PASSIVE registry progress only; payload unchanged; not a GPU root fix'},
 'service_template':{'service':'MT8167CombinedFollowup156','data_directory':'MT8167CombinedFollowup156','targets':12,'install_targets':['render'],'install_mask':0x400,'first_install_mask':0,'planned_restarts':1,'signed_service':None,'native_arm':None,'changed_service_TUs_after_adoption':1},
 'execution_template':{'GPU_client':client,'GPU_client_index':0,'GPU_client_count':1,'GPU_client_last':True,'temperature_count':0,'thermal_status_count':0,'audio_count':0,'WLAN_count':0,'BT_count':0,'BT_registration_count':0,'benchmark_count':0,'other_function_clients':0,'timeouts':{'install_seconds':900,'boot_wait_seconds':180,'pre_GPU_seconds':180,'GPU_seconds':900,'service_start_event':'phase1 PB target0/event0x286; once','GPU_start_event':'phase1 PB target0/event0x220; index0; once','repeated_events_extend':False,'diagnostic_total_seconds':1080}},
 'preservation_template':{'boot_write_required':False,'boot_path':prior['boot_path'],'boot_sha256':prior['matched_boot_sha256'],'boot_evidence':'prior matched155 package/main deployment record; no new physical readback','GPT_written':False,'retain_parent1117':policy['packages']['parent'],'retain_thermal110':policy['packages']['thermal'],'other_selected_actual':n['nodes'],'full_service_fingerprints':n['old_services'],'captured_registry_values':retained_values,'service154_actual_Start':4,'unknown_service_hashes_synthesized':False,'BT2014_mixed':False,'SystemRequired_existing_implementation_retained':True},
 'mutable_logs':{'baseline':br,'capture_number':156,'verified_files':logs,'old67_unchanged':True,'new_files':2,'immutable_admission_gate':False,'overwrite_or_delete':False},
 'post_capture_number':158,'post_reader_requirements':['NativeCheckpoint14 fixed fields plusReachedMask; same-capture only','ModuleIdentity144B plus actual SYS/selectedINF; runtime BuildID from final344 manifest, never guessed','collect durable progress fields declared by final344 source; no oldboot PC/current ASLR comparison','retain errored receipt archive/fetch before classification failure'],
 'adoption_requirements':['completed344 artifact manifest/member hashes and main independent seal','bind exact344 runtime identity/provenance; progress persistence is not a rendering root fix','finalize156 validator/native clone/read156-post/policy/header/client index0 together','actual service154 Start4 plus25 named values and18 fingerprints remain guarded','same Recovery unchanged check by main before arm','service1TU only on existing cache; parent/FW/thermal not rebuilt'],
 'limitations':['draft only; no signed service/native/media generated','GPU344 artifact/main seal incomplete: arm forbidden','thermal success from main report is retained; no thermal retest','GPU343 init succeeded butfirstCopy reset; no GPU complete success claim']}

def reject_arm(d):
 require(d['draft'] and not any(d[k] for k in ('arm_allowed','normal_install_allowed','media_generation_allowed')),'Draft state altered')
 raise ValueError('GPU344 artifact/main seal未結合:157 draftはarm・媒体生成不可')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--workspace',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();root=a.workspace.resolve(strict=True);out=a.out.resolve();require(out.is_relative_to(root) and not out.exists(),'New root-local output required');d=make_draft(root);out.mkdir(parents=True);(out/'contract-DRAFT.json').write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n');print('RUN157_DRAFT_ACTUAL156_VERIFIED_41FILES_69LOGS_18SERVICES_25VALUES_NO_ARM')
if __name__=='__main__':main()
