#!/usr/bin/env python3
"""Opt-in render-only GPU380 adapter over shared200; no capture/build/device I/O."""
import argparse
import copy
import importlib.util
import json
import re
from pathlib import Path

HERE=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('render_shared200',HERE/'prepare-peripheral-followup200.py')
shared=importlib.util.module_from_spec(sp);sp.loader.exec_module(shared)
require,read,sha,rooted=shared.require,shared.read,shared.sha,shared.rooted
SIGNER=shared.THUMB
FAMILY='SIGNED_RENDER_CAPTURE'
READER=HERE/'read-keypad14-checkpoint-loaded.ps1'
LOCK=HERE/'render-signed-capture-accepted.json'

def pin(root,ref):
 require(isinstance(ref,dict) and set(ref)=={'path','sha256'} and isinstance(ref['path'],str) and isinstance(ref['sha256'],str) and re.fullmatch('[0-9a-f]{64}',ref['sha256'] or '') and ref['sha256']!='0'*64,'Actual path/hash required; pending input rejected')
 p=rooted(root,ref['path']);require(sha(p)==ref['sha256'],'Input drift: '+ref['path']);return read(p)

def pin_file(root,ref):
 require(isinstance(ref,dict) and set(ref)=={'path','sha256'} and isinstance(ref['sha256'],str) and re.fullmatch('[0-9a-f]{64}',ref['sha256']) and ref['sha256']!='0'*64,'Actual file ref required')
 p=rooted(root,ref['path']);require(sha(p)==ref['sha256'],'Evidence file drift');return p

def profile_name(capture):return 'combined-features-fresh'+str(capture)+'-signed-render-v1'

def candidate(root,spec):
 lock=read(LOCK)
 require(lock['schema']=='SIGNED_RENDER_CAPTURE_ACCEPTED_V1' and lock['status']=='actual-signed-package-main-reviewed','Actual signed candidate lock required')
 require(spec['GPU_candidate']==lock['GPU_candidate'] and spec['main_review']==lock['main_review'],'Exact actual380 manifest/main review refs')
 ref=spec['GPU_candidate'];m=pin(root,ref);review=pin(root,spec['main_review']);certs=pin(root,lock['certificates'])
 require(review['signed_manifest_sha256']==ref['sha256'] and review['source_commit']==lock['source_commit'] and review['package_version']==lock['driver_version'] and review['verified_recovered_files']==152 and review['verified_package_outputs']==15,'Actual main native review shape/source/output scope')
 require(review['all_build_steps_exit0'] is True and review['CAT_membership_steps_exit0'] is True and review['actual_Windows_signature_reports_bound_to_same_bytes']==6 and review['certificate_report_sha256']==lock['certificates']['sha256'] and review['hardware_verified'] is False and review['GPU_drawing_success'] is False and review['private_key_exported'] is False,'Actual main signature/member evidence, no device success')
 row=dict(directory=Path(ref['path']).parent.as_posix(),manifest=ref['path'],manifest_sha256=ref['sha256'],source_commit=lock['source_commit'],driver_version=lock['driver_version'],driver_date=lock['driver_date'],names=dict(inf='ge8300-render.inf',sys='ge8300-render.sys',cat='ge8300-render.cat'))
 require(m['schema']=='GE8300_RUNTIME380_PLAN_V1' and m['version']=='0.0.3.80' and m['candidate_version']==380 and all(m.get(k) is True for k in ('executed','success','signed')) and m['hardware_verified'] is False and m['signer_thumbprint']==SIGNER,'Completed native signed380 schema only')
 require(re.fullmatch('[0-9a-f]{40}',m['source_commit']) and m['source_commit']==review['source_commit']==row['source_commit']==lock['source_commit'] and row['driver_version']==m['version'],'Actual source/version tuple; no relabel')
 require(row['manifest']==ref['path'] and row['manifest_sha256']==ref['sha256'] and Path(row['directory'])==Path(ref['path']).parent,'Candidate directory/manifest exact')
 require(isinstance(m['steps'],list) and m['steps'] and all(x['exit_code']==0 and re.fullmatch('[0-9a-f]{64}',x['log_sha256']) for x in m['steps']),'All signed package stages complete')
 stages=[x['stage'] for x in m['steps']]
 required={'sign-SYS','catalog','sign-CAT','signer-identities'}|{'member-'+n for n in ('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw')}
 require(required<=set(stages) and len(stages)==len(set(stages)),'Unique completed signature/CAT member stages')
 files={}
 require(len(m['outputs'])==15,'All fifteen actual leaves reviewed')
 for f in m['outputs']:
  name=f['name'];require(re.fullmatch(r'(?:display-transport/)?[A-Za-z0-9_.-]+',name) and name not in files and re.fullmatch('[0-9a-f]{64}',f['sha256']) and type(f['bytes']) is int and f['bytes']>0,'Bounded exact leaf inventory')
  p=rooted(root,Path(row['directory'])/name);require(p.stat().st_size==f['bytes'] and sha(p)==f['sha256'],'Actual candidate leaf hash/size: '+name);files[name]=f['sha256']
 require(certs['schema']=='DWM30CB_CERTIFICATE_VERIFICATION_V1' and certs['hardware_verified'] is False and certs['private_key_exported'] is False and len(certs['files'])==6,'Actual six-file Windows certificate report')
 expected={'ge8300-render.sys','ge8300-render.cat','ge8300-umd.dll','mt8167-dxgi-present356.exe','display-transport/mt8167-display-transport.sys','display-transport/mt8167-display-transport.cat'};seen=set()
 for x in certs['files']:
  name=x['path'].replace('\\','/').rsplit('/package380/',1)[-1]
  require(name in expected and name not in seen and x['sha256']==files[name] and x['status']=='Valid' and x['signer_thumbprint']==SIGNER,'Certificate result bound to exact final bytes');seen.add(name)
 require(seen==expected and files['ge8300-render.sys']==review['SYS_sha256']==lock['SYS_sha256'] and files['ge8300-umd.dll']==review['UMD_sha256']==lock['UMD_sha256'],'Actual SYS/UMD identity')
 inf=shared.base.inftext(rooted(root,Path(row['directory'])/row['names']['inf']))
 require(re.search(r'^\s*UserFiles\s*=\s*11\s*$',inf,re.M|re.I) and re.search(r'^\s*FirmwareFiles\s*=\s*12\s*$',inf,re.M|re.I),'Actual INF UMD/System32 and firmware/Drivers destinations')
 row['extra_members']=[dict(name=name,installed=installed,sha256=files[name]) for name,installed in (('ge8300-umd.dll','ge8300-umd.dll'),('ge8300-mips.fw','Drivers\\ge8300-mips.fw'))]
 return shared.base.package(root,row,'render'),files,m,review

def validate_baseline(root,spec):
 capture=spec['capture_number'];require(type(capture) is int and 235<=capture<=999,'Actual capture235..999; never relabel233')
 require(spec['hardware_owner']=='main' and spec['main_execution_approved'] is True and spec['install_targets']==['render'] and spec['planned_reboots']==1 and spec['filter_root_transitions']==[],'Explicit render-only restart1; no broad exemptions')
 fm=pin(root,spec['fresh']);n=pin(root,spec['native']);c=pin(root,spec['capture_contract']);prior=pin(root,spec['template_policy'])
 require(fm['status']=='captured-unmounted' and fm['read_only'] is True and fm['unmounted'] is True and fm['errors']==[],'Completed actual fresh required')
 require(n['capture_number']==capture and n['new_service_name']=='MT8167CombinedFollowup'+str(capture) and n['new_service']['present'] is False and n['source_manifest_sha256']==spec['fresh']['sha256'] and n['recovery_boot_id']==fm['recovery_boot_id'],'Exact capture/new service/fresh/boot inventory; no renamed receipt')
 require(n['originals_unchanged'] is True and n['registry_write_requested'] is False and n['full_service_capture_complete'] is True and n['errors']==[] and all(n[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Complete same-load read-only inventory')
 require(c['fresh_manifest_sha256']==spec['fresh']['sha256'] and c['native_receipt_sha256']==spec['native']['sha256'] and c['recovery_boot_id']==fm['recovery_boot_id'],'Same-boot capture contract')
 require(n['source_recipe_files'].get(READER.name)==sha(READER),'Keypad14 strict reader source required, not old24-stage reader')
 cp=n['keypad_checkpoint'];require(cp['complete'] is True and cp['errors']==[] and all(x['schema_valid'] is True and x['epoch_binding'] in ('current-load','stale-prior-load') for x in cp['records']),'Bounded CP records; stale is never successful current input')
 for name in spec['additional_service_names']:require(name in n['old_services'],'No old controller service omitted')
 rows=pin(root,spec['actual_package_rows']);require(set(rows)==set(shared.ROLES),'All thirteen actual package rows required')
 require(rows['wifi']['driver_version']=='2026.11.11.1033' and rows['keypad']['driver_version']=='0.0.1.4','Retain actually installed WiFi1033/keypad14')
 # The shared builder checks every captured file byte, selected node INF tuple,
 # full service/named inventory, supplement and mutable same-boot provenance.
 return fm,n,c,prior

def configure(spec,root):
 global PROFILE,SERVICE,DIRECTORY
 capture=spec['capture_number'];PROFILE=profile_name(capture);SERVICE=DIRECTORY='MT8167CombinedFollowup'+str(capture)
 shared.CAPTURE=capture;shared.PROFILE=shared.base.PROFILE=PROFILE;shared.SERVICE=shared.DIRECTORY=shared.base.SERVICE=shared.base.DIRECTORY=SERVICE
 shared.INSTALL=shared.base.INSTALL=['render']
 class RenderUpdate:
  contract_family=FAMILY
  install_targets=['render']
  def validate_capture_contract(self,c,pin):require(c['schema']==FAMILY+'_FRESH'+str(capture)+'_CAPTURE_CONTRACT_V1','Capture-bound schema only')
  def validate_manifest(self,m,review,current):
   self.row,self.hashes,self.manifest,self.review=candidate(root,spec)
   require(m==self.manifest and review==self.review,'Shared manifest/review binding')
  def validate_output_review(self,review,hashes):require(hashes==self.hashes,'Complete final output set')
  def package_rows(self,rows,pin):return rows
  def adopt(self,role,prior,originals,hashes):
   require(role=='render' and prior['hashes']['sys']!=self.row['hashes']['sys'],'Only a changed render SYS; do not reinstall same candidate')
   old=read(rooted(root,prior['manifest']));oldfiles={x['name']:x['sha256'] for x in old['outputs']}
   require(set(oldfiles)==set(hashes),'Candidate full leaf set equals actual retained package')
   for x in prior['extra_members']:
    name='Windows/System32/'+x['installed'].replace('\\','/')
    require(originals[name]['sha256']==x['sha256']==oldfiles[x['name']],'Actual old UMD/FW equals retained package, not a historical declaration')
   for name,h in hashes.items():
    if name not in {'ge8300-render.inf','ge8300-render.sys','ge8300-render.cat','ge8300-umd.dll'}:require(oldfiles[name]==h,'Unrelated leaf retained: '+name)
   row=copy.deepcopy(self.row)
   for x in row['extra_members']:
    installed='Windows/System32/'+x['installed'].replace('\\','/');require(installed in originals,'Actual old extra required')
    x['baseline_sha256']=originals[installed]['sha256'];require(x['sha256']==hashes[x['name']],'Final signed extra')
   return row
  def validate_transport(self,packages,hashes):
   rows=pin(root,spec['actual_package_rows'])
   for role in shared.ROLES:
    if role!='render':require(packages[role]==shared.base.package(root,rows[role],role),'Other twelve roles retained exactly: '+role)
   for kind,name in packages['display']['names'].items():require(packages['display']['hashes'][kind]==hashes['display-transport/'+name],'Actual transport trio retained in final package')
  def client(self,prior,originals,origin_sources,preserved,sources,witnesses,**unused):
   sp=importlib.util.spec_from_file_location('retained379_schema',HERE/'followup231-vidpn379-schema.py');pairmod=importlib.util.module_from_spec(sp);sp.loader.exec_module(pairmod)
   pair=pairmod.inspect_signed_pair(root,rooted(root,spec['client_pair']['path']));pin(root,spec['client_pair']);self.pair=pair;self.evaluator_source=pairmod.CLIENT
   self.client_refs=[spec['client_pair']]+pair['references']
   for role in ('launcher','evaluator'):
    leaf=pair[role];self.client_refs.extend(leaf[k] for k in ('file','manifest','import_proof'))
    installed='Windows/System32/'+Path(leaf['file']['path']).name
    require(originals[installed]['sha256']==leaf['file']['sha256'],'Keep actual already installed signed379 pair, no client replacements')
    preserved[installed]=leaf['file']['sha256'];sources[installed]=origin_sources[installed]
   launcher=pair['launcher']['file'];evaluator=pair['evaluator']['file']
   self.client_policy=pin(root,spec['retained_client_policy']);client=copy.deepcopy(self.client_policy['diagnostic_clients'][0])
   require(client['name']=='gpu_present' and client['sha256']==launcher['sha256'] and client['exe']==Path(launcher['path']).name and client['dependencies']==[dict(exe=Path(evaluator['path']).name,sha256=evaluator['sha256'])] and client['phase']==1 and client['timeout_ms']==900000,'Existing signed379 diag2 client contract')
   client['directory']=Path(launcher['path']).parent.as_posix();return client
  def seed_present_contract(self,p):
   # The current peripheral-only baseline has no GPU diagnostic contract.
   # Restore the already verified retained client contract before shared update.
   p['standard_present_contract']=copy.deepcopy(self.client_policy['standard_present_contract'])
  def boot(self,b,transition,fresh,bootid,pin,pin_file):
   seal=pin(spec['last_boot_seal']);result=pin(spec['last_run_result'])
   require(b['schema']=='SIGNED_RENDER_CAPTURE_RETAINED_BOOT_V1' and b['source_manifest_sha256']==fresh and b['recovery_boot_id']==bootid and b['last_boot_seal']==spec['last_boot_seal'] and b['last_run_result']==spec['last_run_result'],'Current capture retained boot witness')
   require(seal['status']=='validated-main-seal' and seal['matched_boot_sha256']==transition['boot_sha256'] and result['seal_sha256']==spec['last_boot_seal']['sha256'] and result['transaction']==seal['transaction'] and b['boot_sha256']==transition['boot_sha256'],'Last actual main seal/result')
   require(b['physical_read_this_capture'] is False and b['no_intervening_boot_or_gpt_write_reported_by_main'] is True and seal['boot_write_required'] is False and seal['gpt_written'] is False and result['boot_written'] is False and result['gpt_written'] is False and transition['boot_write_required'] is False and transition['gpt_written'] is False,'Retained boot/GPT0 report; no invented physical read')
   pin_file(dict(path=transition['boot_path'],sha256=transition['boot_sha256']))
   for key in ('firmware_manifest','firmware_invariants','firmware_reset_invariants'):pin(transition[key])
   return dict(kind='last-main-seal-retained-no-write',physical_read_this_capture=False,boot_sha256=transition['boot_sha256'],hardware_success_inferred=False)
  def finish(self,p,current,n,pin,pin_file):
   for ref in self.client_refs:pin_file(ref)
   for ref in (spec['retained_client_policy'],spec['GPU_candidate'],spec['main_review'],read(LOCK)['certificates'],spec['last_boot_seal'],spec['last_run_result']):pin(ref)
   for r in pin(spec['template_policy'])['input_pins']:pin_file(r)
   for q in (Path(__file__),LOCK,READER,HERE.parent/'winpe/keypad13-checkpoint-policy.h',HERE.parent/'winpe/vidpn-callback-trace-policy.h',HERE.parent/'winpe/display-idle-policy.h'):
    pin_file(dict(path=q.resolve().relative_to(root).as_posix(),sha256=sha(q)))
   for x in p['packages']['render']['extra_members']:
    name='Windows/System32/'+x['installed'].replace('\\','/');q=rooted(root,Path(p['packages']['render']['directory'])/x['name'])
    pin_file(dict(path=q.relative_to(root).as_posix(),sha256=x['sha256']))
    # Pre-copy baseline stays in witnesses; preserved() must test NEW bytes
    # AFTER installation even for extras which changed from the capture.
    p['preserved_files'][name]=x['sha256'];p['preserved_sources'][name]=q.relative_to(root).as_posix()
   for key in list(p):
    if key.endswith('_contract') and (key.startswith('followup') or key.startswith('gpu') or key.startswith('signed_peripheral')):p['history_'+key+'_reference_only']=p.pop(key)
   p.pop('peripheral_status_only',None);p.pop('retained_GPU_reference_only',None)
   for k in ('collection_reboot','vidpn_uart_snapshot'):
    if k in p:p['history_'+k+'_reference_only']=p.pop(k)
   p['observer_contract']=copy.deepcopy(self.client_policy['observer_contract'])
   p['observer_contract'].update(fresh_basis=capture,planned_reboots=1,GPU_client_last=False,GPU_client_index=0,globaldiag_client_index=1,diagnostic_clients=2,GPU_timeout_ms=900000,absolute_batch_timeout_seconds=1140)
   p.update(gpu_install=True,gpu_client_execute=True,gpu_client_count=2,diagnostic_only=False,driver_updates=True,planned_reboots=1,main_execution_approved=True)
   p['display_transition'].update(DIF_order=['render'],old_DOD_retirement='None; actual transport/other12 roles retained')
   p['service_baseline_generation'].update(source=spec['native'],raw_Mt6392Keys_snapshot=copy.deepcopy(n['old_services']['Mt6392Keys']),raw_WDF_timestamp=copy.deepcopy(n['keypad_wdf_runtime']['timestamp']),all_retained_full_hashes_strict=True,retained_service_count=len(n['old_services']))
   p['normal_function_expectations'].update(GPU_execute=True,WLAN_readonly_execute=False)
   p['standard_present_contract']=copy.deepcopy(self.client_policy['standard_present_contract'])
   p['standard_present_contract'].update(diagnostic_clients=2,execution_index=0,driver_package_source_commit=self.manifest['source_commit'],driver_signed_manifest_sha256=spec['GPU_candidate']['sha256'],launcher_compile_source_commit=self.pair['launcher_source_commit'],launcher_source_commit=self.pair['launcher_source_commit'],evaluator_source_commit=self.evaluator_source,launch_image='System32/mt8167-dxgi-console-launch363.exe',required_evaluator='System32/mt8167-dxgi-present356.exe',evaluator_sha256=self.pair['evaluator']['file']['sha256'],copy_assets=['mt8167-dxgi-console-launch363.exe','mt8167-dxgi-present356.exe'],GPU_success='Standard Present and completed GPU/frame/pixel evidence joined to the current main-sealed SYS/UMD/FW tuple; D3D create or launcher exit alone is insufficient',hardware_success_inferred=False)
   if 'present_collect' in self.client_policy:
    p['present_collect']=copy.deepcopy(self.client_policy['present_collect']);p['present_collect'].pop('mount_root',None)
    p['present_collect']['mount_root_policy']='current main-sealed workspace-local read-only mount; historical mount path is not reused'
   p['development_display_on']=dict(macro='A04_DEVELOPMENT_DISPLAY_ON',value=1,phase=1,before_run_clients=True,after_readback=True,buttons_fix_claimed=False)
   p['signed_render_contract']=dict(schema='SIGNED_RENDER_CAPTURE_TRANSITION_V1',accepted_lock_sha256=sha(LOCK),input=spec['_input_ref'],capture=capture,run=capture+1,post_capture=capture+2,render_update_mask=1,install_mask=1<<shared.ROLES.index('render'),planned_reboots=1,boot_write_required=False,gpt_written=False,terminal_windows_hold=not spec.get('unattended_collection_reboot',False),GPU_candidate=spec['GPU_candidate'],main_review=spec['main_review'],retained_packages={r:p['packages'][r] for r in shared.ROLES if r!='render'},keypad14_reader_sha256=sha(READER),hardware_success_claimed=False)
   if spec.get('unattended_collection_reboot'):
    from collection_reboot import CONFIG,source_closure,policy_config
    p['unattended_collection_reboot']=dict(schema='SIGNED_RENDER_UNATTENDED_COLLECTION_V1',capture=capture,bounded_winpe_required=True,require_atomic_publication=True)
    p['collection_reboot']=dict(CONFIG)
    for ref in source_closure(HERE.parents[1]):
     q=HERE.parents[1]/ref['path'];pin_file(dict(path=q.resolve().relative_to(root).as_posix(),sha256=ref['sha256']))
    policy_config(p)
 shared.TIMING=RenderUpdate();return shared

def load_map(path,root):
 spec=read(path);require(spec.get('schema')=='SIGNED_RENDER_CAPTURE_FINALIZE_V1','Explicit finalized map, never a preparation template')
 required={'schema','capture_number','hardware_owner','main_execution_approved','fresh','native','supplement','capture_contract','template_policy','actual_package_rows','mutable_baseline','boot_witness','last_boot_seal','last_run_result','additional_service_names','GPU_candidate','main_review','retained_client_policy','client_pair','globaldiag_manifest','globaldiag_import_proof','install_targets','planned_reboots','filter_root_transitions'}
 require(set(spec) in (required,required|{'unattended_collection_reboot'}),'Exact final map fields; no pending or extra components')
 require(type(spec.get('unattended_collection_reboot',False)) is bool,'Explicit boolean unattended opt-in required')
 require(not spec.get('unattended_collection_reboot') or spec['capture_number']>=237,'Only a new actual237+ capture may enable unattended collection')
 validate_baseline(root,spec)
 require(not any(spec.get(k) for k in ('wifi_activation','retained_package_overrides','collection_reboot','USB')),'No other components or collection reboot')
 spec['_input_ref']=dict(path=path.relative_to(root).as_posix(),sha256=sha(path));spec['profile']=profile_name(spec['capture_number']);spec['GPU368_manifest']=spec['GPU_candidate'];spec['GPU368_handoff']=spec['main_review']
 p=configure(spec,root).build_policy(root,spec)
 pins={x['path']:x['sha256'] for x in p['input_pins']};pins[spec['_input_ref']['path']]=spec['_input_ref']['sha256'];p['input_pins']=[dict(path=k,sha256=v) for k,v in sorted(pins.items())];return p

def generate(p):
 capture=p['capture_selection']['capture_number'];require(p['profile']==profile_name(capture) and p['install_targets']==['render'] and p['planned_reboots']==1 and p['boot_transition']['boot_write_required'] is False and p['boot_transition']['gpt_written'] is False,'Render-only/restart1/bootGPT0')
 shared.PROFILE=shared.base.PROFILE=p['profile'];shared.SERVICE=shared.DIRECTORY=shared.base.SERVICE=shared.base.DIRECTORY=p['service'];shared.INSTALL=shared.base.INSTALL=['render']
 return '#define A04_KEYPAD13_CHECKPOINT 1\n#define A04_KEYPAD14_CHECKPOINT 1\n#define A04_DISPLAY_IDLE_BEFORE_CLIENTS 1\n#define A04_VIDPN_CALLBACK_TRACE 1\n'+shared.generate(p)

def paths():return shared.paths()
def validate_runtime_policy(root,p):require(p==load_map(rooted(root,p['signed_render_contract']['input']['path']),root),'Exact regenerated actual capture/render policy')
def assert_paths_from_policy(b,p):
 require(p['profile']==profile_name(p['capture_selection']['capture_number']),'Capture-bound path profile')
 for name in (p['service'],p['service']+'.exe','mt8167-dxgi-console-launch363.exe','dxg-global-diag198.exe'):require(name.encode('utf-16le') in b,'Signed service/client path identity')
 require('bt2013-install.bin'.encode('utf-16le') not in b,'No BT installation payload')
def assert_paths(image,capture=False):return assert_paths_from_policy(Path(image).read_bytes(),read(Path(image).with_name('peripheral-policy.json')))

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for k in ('workspace','map','out'):ap.add_argument('--'+k,type=Path,required=True)
 ap.add_argument('--finalize',action='store_true',required=True);a=ap.parse_args();root=a.workspace.resolve(strict=True);out=(root/a.out).resolve();require(out.is_relative_to(root) and not out.exists(),'New root-local output')
 p=load_map(rooted(root,a.map),root);h=generate(p);out.mkdir(parents=True)
 for name,text in [('peripheral-service-path-policy.h',paths()),('peripheral-batch-policy.h',h),('peripheral-policy.json',json.dumps(p,indent=2)+'\n'),('peripheral-preservation-plan.json',json.dumps(p,indent=2)+'\n')]: (out/name).write_text(text,newline="\n")
if __name__=='__main__':main()
