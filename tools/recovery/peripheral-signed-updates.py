#!/usr/bin/env python3
"""Shared200 signed peripheral updates; actual capture required for finalization."""
import argparse,copy,hashlib,importlib.util,json,re
from pathlib import Path
HERE=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('updates_shared200',HERE/'prepare-peripheral-followup200.py')
shared=importlib.util.module_from_spec(sp);sp.loader.exec_module(shared)
require,read,sha,rooted=shared.require,shared.read,shared.sha,shared.rooted
LOCK=HERE/'peripheral-signed-updates-accepted.json'
SIGNER='BCBF6C456C6F0DD0818434763B189488FBF13570'
ROLES=('wifi','keypad')

def pinned(root,ref):
 require(isinstance(ref,dict) and set(ref)=={'path','sha256'} and re.fullmatch('[0-9a-f]{64}',ref['sha256']) and ref['sha256']!='0'*64,'Actual path/SHA required; pending rejected')
 p=rooted(root,ref['path']);require(sha(p)==ref['sha256'],'Input drift: '+ref['path']);return read(p)

def candidate_rows(root):
 """Return real manifests/role rows for shared200 TIMING.adopt, no flat receipt."""
 lock=read(LOCK);require(lock['schema']=='SIGNED_PERIPHERAL_UPDATES_ACCEPTED_V1' and set(lock['packages'])==set(ROLES),'Exact reviewed two-role lock')
 rows={}
 for role,x in lock['packages'].items():
  m=pinned(root,x['manifest']);review=pinned(root,x['main_review'])
  require(m['schema']==x['schema'] and m['source_commit']==x['compiled_source_commit'] and m['signed'] is True and m['version']==x['version'],'Actual signed schema/compile source/version')
  if role=='wifi':
   require(m['success'] is True and m['signer_thumbprint']==SIGNER and m['compiled_source_commit']==m['source_commit'] and m['signing_recipe_commit']==x['signing_source_commit'] and m['private_driver_baseline_required'] is False and m['non_signature_payload_unchanged'] is True and m['original_unsigned_manifest_preserved'] is True,'Public cold signed1033, not incremental1033')
   require(review['steps_exit_zero']==9 and review['non_signature_SYS_payload_unchanged'] is True and review['Windows_verify_CAT_membership_certificate_identity_steps_bound'] is True and review['compiled_source_commit']==m['source_commit'] and review['signing_recipe_commit']==m['signing_recipe_commit'],'Existing main cold-sign review')
   files=m['files']
  else:
   require(m['status']=='cross-built-WDK81-signed-catalog-verified-not-device-tested' and m['signer']==SIGNER and m['signing_source_commit']==x['signing_source_commit'] and m['metadata_source_commit']==x['metadata_source_commit'] and m['observation_source']==x['observation_source'] and m['functional_fix_claimed'] is False and m['device_tested'] is False,'Actual signed14 split provenance; raw deployable=false is not rewritten')
   require(review['manifest_archive_trio_hashsize_verified'] is True and review['non_signature_SYS_payload_identical'] is True and review['Windows_verify_and_CAT_membership_receipt_bound'] is True,'Existing main keypad review')
   files=m['outputs']
  require(len(files)==3 and {Path(f['path']).name for f in files}==set(x['names'].values()),'Exact trio only')
  for f in files:
   name=Path(f['path']).name;p=rooted(root,Path(x['directory'])/name)
   require(p.stat().st_size==f['bytes'] and sha(p)==f['sha256']==x['hashes'][p.suffix[1:]],'Actual signed trio hash/size')
  row=dict(directory=x['directory'],manifest=x['manifest']['path'],manifest_sha256=x['manifest']['sha256'],source_commit=m['source_commit'],driver_version=x['version'],driver_date=x['date'],names=x['names'])
  rows[role]=shared.base.package(root,row,role) # existing HWID/class/INF/source/trio guard
 return rows

def adopt(root,role,baseline):
 """Call inside existing shared200 TIMING.adopt; unrelated roles stay strict."""
 require(role in ROLES,'Only WiFi1033/keypad14 adopted by this source adapter')
 row=candidate_rows(root)[role]
 require(baseline['names']['sys']==row['names']['sys'],'No installed-path change')
 return row

def input_refs(root):
 lock=read(LOCK);rows=candidate_rows(root);refs=[]
 for role,x in lock['packages'].items():
  refs.extend([x['manifest'],x['main_review']])
  refs.extend(dict(path=(Path(rows[role]['directory'])/name).as_posix(),sha256=rows[role]['hashes'][kind]) for kind,name in rows[role]['names'].items())
 return refs

def validate_baseline(root,spec):
 """Use shared200's complete inventory contract, not a handpicked service subset."""
 require(spec['schema']=='SIGNED_PERIPHERAL_UPDATES_BASELINE_INPUTS_V1','Explicit baseline input contract')
 capture=spec['capture_number'];require(type(capture) is int and 1<=capture<=9999,'Actual capture number')
 service='MT8167CombinedFollowup'+str(capture)
 fm=pinned(root,spec['fresh']);n=pinned(root,spec['native']);c=pinned(root,spec['capture_contract']);prior=pinned(root,spec['template_policy'])
 require(fm['status']=='captured-unmounted' and fm['read_only'] is True and fm['unmounted'] is True and fm['errors']==[],'Actual completed fresh only')
 require(n['source_manifest_sha256']==spec['fresh']['sha256'] and n['recovery_boot_id']==fm['recovery_boot_id'] and n['capture_number']==capture and n['originals_unchanged'] is True and n['errors']==[] and n['registry_write_requested'] is False and n['full_service_capture_complete'] is True,'Same-fresh complete native inventory')
 require(all(n[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')) and n['new_service_name']==service and n['new_service']['present'] is False,'Original reader cleanup/new service absent')
 require(c['fresh_manifest_sha256']==spec['fresh']['sha256'] and c['native_receipt_sha256']==spec['native']['sha256'] and c['recovery_boot_id']==fm['recovery_boot_id'],'Same-fresh contract')
 for name in spec['additional_service_names']:
  require(name in n['old_services'] and n['old_services'][name]['present'] is True,'Additional old service must not be omitted')
 original={x['source_path']:x for x in fm['files']}
 for name,h in n['source_files'].items():require(original['Windows/System32/config/'+name]['sha256']==h,'Same SYSTEM trio')
 shared.CAPTURE=capture;shared.SERVICE=service
 from types import SimpleNamespace
 shared.TIMING=SimpleNamespace(contract_family='SIGNED_PERIPHERAL_UPDATES')
 values=shared.inventory_contract(c,n,prior)
 return dict(capture_number=capture,service=service,source=spec['native'],service_preservation=copy.deepcopy(n['old_services']),captured_registry_values=values,complete_service_set_preserved=True,new_service_absent=True,build_allowed=False,native_patch_allowed=False,device_operations_allowed=False)

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--baseline-map','--map',dest='baseline_map',type=Path);ap.add_argument('--finalize',action='store_true');a=ap.parse_args()
 root=a.workspace.resolve(strict=True);out=(root/a.out).resolve();require(out.is_relative_to(root) and not out.exists(),'New workspace-local preparation output')
 if a.finalize:
  require(a.baseline_map is not None,'Actual map required');p=load_map(rooted(root,a.baseline_map),root);h=generate(p);out.mkdir(parents=True)
  (out/'peripheral-batch-policy.h').write_text(h);(out/'peripheral-service-path-policy.h').write_text(paths())
  for name in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/name).write_text(json.dumps(p,indent=2)+'\n')
  print('SIGNED_PERIPHERALS_FINALIZED_GPU_RETAINED_NO_GPU_DIAGNOSTICS');return
 result=dict(schema='SIGNED_PERIPHERAL_UPDATES_PREPARATION_V1',packages=candidate_rows(root),input_refs=input_refs(root),install_targets=list(ROLES),install_mask=136,GPU=None,USB=None,final_policy_generated=False,build_allowed=False,native_patch_allowed=False,device_operations_allowed=False,required_macros=['A04_KEYPAD13_CHECKPOINT=1','A04_KEYPAD14_CHECKPOINT=1'])
 if a.baseline_map:result['baseline']=validate_baseline(root,read(rooted(root,a.baseline_map)))
 out.mkdir(parents=True);(out/'candidate-preparation.json').write_text(json.dumps(result,indent=2)+'\n')
 print('CANDIDATES_PREPARED_GPU_USB_FINAL_POLICY_PENDING_NO_HEADER_BUILD_OR_NATIVE')

# Formal peripheral-only lane over the same shared200 builder and C transaction.
def profile_name(capture):return 'combined-features-fresh'+str(capture)+'-signed-peripherals-v1'

def configure(spec,root):
 global PROFILE,SERVICE,DIRECTORY
 capture=spec['capture_number'];require(type(capture) is int and 204<=capture<=999,'Shared service capture204..999');profile=profile_name(capture);service='MT8167CombinedFollowup'+str(capture)
 PROFILE=profile;SERVICE=DIRECTORY=service
 shared.CAPTURE=capture;shared.PROFILE=shared.base.PROFILE=profile;shared.SERVICE=shared.DIRECTORY=shared.base.SERVICE=shared.base.DIRECTORY=service
 shared.INSTALL=shared.base.INSTALL=list(ROLES)
 class PeripheralUpdate:
  contract_family='SIGNED_PERIPHERAL_UPDATES'
  retained_gpu=True;disable_gpu_diagnostics=True;install_targets=list(ROLES)
  def validate_capture_contract(self,c,pin):require(c['schema']=='SIGNED_PERIPHERAL_UPDATES_FRESH'+str(capture)+'_CAPTURE_CONTRACT_V1','Same capture shared contract')
  def package_rows(self,rows,pin):return rows
  def adopt(self,role,baseline,originals,hashes):
   updated=adopt(root,role,baseline)
   require(baseline['hashes']['sys']!=updated['hashes']['sys'],'Do not reinstall already selected candidate')
   return updated
  def validate_transport(self,packages,hashes):
   prior=pinned(root,spec['template_policy'])
   for role in shared.ROLES:
    if role not in ROLES:require(packages[role]==prior['packages'][role],'Unrelated package stays exact: '+role)
  def boot(self,b,transition,fresh,bootid,pin,pin_file):
   result=pin(spec['last_run_result'])
   require(b['status']=='validated-main-seal' and b['boot_write_required'] is False and b['gpt_written'] is False and result['boot_written'] is False and result['gpt_written'] is False and result['transaction']==b['transaction'] and result['seal_sha256']==spec['boot_witness']['sha256'],'Actual last-run no-write seal/result')
   require(b['matched_boot_sha256']==transition['boot_sha256'] and b['matched_boot']==transition['boot_path'] and spec['no_intervening_boot_or_gpt_write_reported_by_main'] is True,'Retained boot report, not physical read')
   pin_file(dict(path=transition['boot_path'],sha256=transition['boot_sha256']))
   for key in ('firmware_manifest','firmware_invariants','firmware_reset_invariants'):pin(transition[key])
   return dict(kind='last-main-seal-retained-no-write',physical_read_this_capture=False,boot_sha256=transition['boot_sha256'],last_run=spec['last_run_result'],hardware_success_inferred=False)
  def finish(self,p,spec,n,pin,pin_file):
   baseline=pin(spec['baseline_inputs'])
   require(baseline['schema']=='SIGNED_PERIPHERAL_UPDATES_BASELINE_INPUTS_V1' and all(spec[k]==v for k,v in baseline.items() if k!='schema'),'Main baseline map exact binding')
   for ref in input_refs(root):pin_file(ref)
   for file in ('peripheral-signed-updates.py','peripheral-signed-updates-accepted.json'):
    q=HERE/file;pin_file(dict(path=q.relative_to(root).as_posix(),sha256=sha(q)))
   q=HERE.parent/'winpe/keypad13-checkpoint-policy.h';pin_file(dict(path=q.relative_to(root).as_posix(),sha256=sha(q)))
   for ref in pinned(root,spec['template_policy'])['input_pins']:pin_file(ref)
   # The real retained render module identity is reference-only, never renamed.
   for item in p['packages']['render']['extra_members']:
    name='Windows/System32/'+item['installed'].replace('\\','/')
    require(p['preserved_files'].get(name)==item['sha256'],'Retained UMD/FW actual bytes')
   for key in list(p):
    if key.endswith('_contract') and (key.startswith('followup') or key.startswith('gpu')):p['history_'+key+'_reference_only']=p.pop(key)
   p.pop('GPU368_manifest',None);p.pop('GPU368_handoff',None)
   p.update(peripheral_status_only=True,diagnostic_clients=[],gpu_client_execute=False,gpu_client_count=0,gpu_install=False,engine_retest=False,diagnostic_only=False,driver_updates=True,planned_reboots=1,main_execution_approved=True)
   p['normal_function_expectations'].update(GPU_execute=False,WLAN_readonly_execute=True)
   p['display_transition'].update(DIF_order=list(ROLES),old_DOD_retirement='None; actual render/transport/UMD/FW retained exactly')
   p['history_standard_present_contract_reference_only']=p.pop('standard_present_contract')
   if 'present_collect' in p:p['history_present_collect_reference_only']=p.pop('present_collect')
   p['globaldiag_contract']=dict(execution_enabled=False,maximum_invocations=0,hardware_success_inferred=False)
   p['observer_contract'].update(diagnostic_clients=0,GPU_client_index=None,globaldiag_client_index=None,GPU_timeout_ms=0,planned_reboots=1,GPU_client_count=0,GPU_client_last=False,GPU_start_event=None,gpu_client_timeout_seconds=0)
   p['signed_peripheral_contract']=dict(schema='SIGNED_PERIPHERAL_UPDATE_TRANSITION_V1',capture=capture,run=capture+1,post_capture=capture+2,install_mask=136,input=spec['_input_ref'],accepted_lock_sha256=sha(LOCK),GPU_updated=False,USB_updated=False,GPU_diagnostics=0,terminal_windows_hold=True)
   p['retained_GPU_reference_only']=dict(packages={r:p['packages'][r] for r in ('render','display')},module_identity=n['module_identity'],new_GPU_candidate=None)
 shared.TIMING=PeripheralUpdate();return shared

def load_map(path,root):
 spec=read(path);require(spec['schema']=='SIGNED_PERIPHERAL_UPDATES_FINALIZE_V1' and spec['hardware_owner']=='main' and spec['main_execution_approved'] is True and spec['install_targets']==list(ROLES),'Explicit two-role finalized inputs')
 validate_baseline(root,dict(spec,schema='SIGNED_PERIPHERAL_UPDATES_BASELINE_INPUTS_V1'))
 spec['_input_ref']=dict(path=path.relative_to(root).as_posix(),sha256=sha(path));spec['profile']=profile_name(spec['capture_number'])
 builder=configure(spec,root);p=builder.build_policy(root,spec)
 pins={x['path']:x['sha256'] for x in p['input_pins']};pins[spec['_input_ref']['path']]=spec['_input_ref']['sha256'];p['input_pins']=[dict(path=k,sha256=v) for k,v in sorted(pins.items())]
 return p

def generate(p):
 require(p['profile']==profile_name(p['capture_selection']['capture_number']) and p['install_targets']==list(ROLES) and p['peripheral_status_only'] is True and p['diagnostic_clients']==[] and p['boot_transition']['boot_write_required'] is False and p['boot_transition']['gpt_written'] is False,'Peripheral only, GPU/boot retained')
 return '#define A04_KEYPAD13_CHECKPOINT 1\n#define A04_KEYPAD14_CHECKPOINT 1\n'+shared.generate(p).replace('#define A04_RUN_WLAN_READ 0','#define A04_RUN_WLAN_READ 1').replace('/* run185:DIS0 System transport then GPU0 unified356; matchedboot required; BT retained. */','/* Signed WiFi/keypad only; actual GPU/boot retained; no diagnostic clients. */')

def validate_runtime_policy(root,p):require(p==load_map(rooted(root,p['signed_peripheral_contract']['input']['path']),root),'Exact shared map regeneration')
def paths():return shared.paths()
def assert_paths(image,capture=False):
 p=read(Path(image).with_name('peripheral-policy.json'));service=p['service']
 require(p['profile']==profile_name(p['capture_selection']['capture_number']) and service=='MT8167CombinedFollowup'+str(p['capture_selection']['capture_number']),'Exact service path identity')
 b=Path(image).read_bytes();require(service.encode('utf-16le') in b,'Actual service paths')
 require(p['diagnostic_clients']==[] and p['gpu_client_count']==0,'No executable diagnostic entries')
 token=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()
 require(token.encode()+b'\0' in b and (service+'.exe').encode('utf-16le') in b,'Exact policy token/service executable paths')
 # GPU filenames remain in preserved_files; their presence is not an invocation.
 require('bt2013-install.bin'.encode('utf-16le') not in b,'No BT installation payload')

if __name__=='__main__':main()
