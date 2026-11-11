#!/usr/bin/env python3
"""Actual229 retained378/keypad13, diagnostic-only next boot; shared378/200 emitter."""
import argparse,copy,importlib.util,json,re
from pathlib import Path
sp=importlib.util.spec_from_file_location('retained378',Path(__file__).with_name('prepare-gpu378-keypad13-inputs.py'));m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m)
require,read,sha,rooted=m.require,m.read,m.sha,m.rooted
PROFILE='combined-features-fresh229-gpu378-keypad13-diagnostic-only-v1'
SERVICE=DIRECTORY='MT8167CombinedFollowup229'
FRESH_PATH='port/xnu/out/private-windows-rt-prebackup-run229/manifest.json'
FRESH_SHA='fb899d1725f3aaafe2ea692175a4f729c25a523cfb3ece4cf166b9d291ffb2f3'
RECOVERY='53000fc0-94ad-4ac4-8cea-61c9d71dfc6f'
ASSETS_PATH='port/xnu/out/private-windows-rt-prebackup-run229-gpu-assets/manifest.json'
ASSETS_SHA='e3a73d3c34dc3f547fc960d62b2a4aa63ac296970fd070456fba2b579084a492'
MUTABLE_SHA='0e0821ec198ba0a859b8bcdd0bd38f61feea5035ec50dcd9c9927c4f4d13031b'
NATIVE_SHA='0d23a6e34bd5dcfceacdb2a31be36fb5485e9515df69a4312465c721bc786659'

def configure():
 s=m.configure(m.accepted());s.CAPTURE=229;s.PROFILE=s.base.PROFILE=PROFILE;s.SERVICE=s.DIRECTORY=s.base.SERVICE=s.base.DIRECTORY=SERVICE;s.INSTALL=s.base.INSTALL=[];return s

class Diagnostic229(m.Fresh227):
 contract_family='GPU378_DIAGNOSTIC'
 template_profile=m.PROFILE
 require_UMD_replacement=False
 @property
 def install_targets(self):return []
 def validate_capture_contract(self,c,pin):require(c['schema']=='GPU378_DIAGNOSTIC_FRESH229_CAPTURE_CONTRACT_V1','actual229 contract only')
 def package_rows(self,rows,pin):
  rows=super().package_rows(rows,pin);lock=m.accepted()
  require(rows['render']['driver_version']=='0.0.3.78' and rows['render']['hashes']['sys']==lock['gpu378']['sys_sha256'] and rows['keypad']['driver_version']=='0.0.1.3' and rows['keypad']['hashes']['sys']==lock['keypad13']['sys_sha256'],'already selected actual378/1.3 only')
  captured={x['source_path']:x for x in pin(self.spec['supplement'])['files']}
  for x in rows['render']['extra_members']:
   path='Windows/System32/'+x['installed'].replace('\\','/');require(captured[path]['sha256']==x['sha256'],'already installed exact UMD/FW only');x['baseline_sha256']=x['sha256']
  return rows
 def boot(self,b,transition,fresh,bootid,pin,pin_file):
  require(b['schema']=='GPU378_DIAG_RETAINED_BOOT229_V1' and b['source_manifest_sha256']==fresh==FRESH_SHA and b['recovery_boot_id']==bootid==RECOVERY,'actual229 retained boot')
  previous=pin(b['previous_witness']);p227=m.Fresh227(self.root,{},{});evidence=p227.boot(previous,{},m.FRESH_SHA,m.RECOVERY,pin,pin_file)
  seal=pin(b['run228_seal']);result=pin(b['run228_result'])
  require(seal['schema']=='GPU378_RUN228_MAIN_SEAL_V1' and seal['status']=='validated-main-seal' and seal['matched_boot_sha256']==m.old.BOOT and seal['boot_write_required'] is False,'actual228 no boot write seal')
  require(result['transaction']=='combined-run228' and result['seal_sha256']==b['run228_seal']['sha256'] and result['boot_written'] is False and result['gpt_written'] is False,'actual228 result retained boot/GPT0')
  require(b['physical_read_this_capture'] is False and b['no_later_boot_or_gpt_write_reported_by_main'] is True and b['continuous_independent_monitoring_performed'] is False,'reported retention scope only')
  for key in ('mode','boot_path','boot_sha256','baseline_boot_sha256','boot_write_required','gpt_written','mixed_old_new_allowed','next_windows_boot_blocked_until_matched_boot','firmware_manifest'):
   require(key in transition,'actual retained transition field '+key)
  return dict(evidence,kind='retained-chain-through-run228',hardware_success_inferred=False)
 def finish(self,p,spec,n,pin,pin_file):
  require(n['capture_number']==229 and n['new_service_name']==SERVICE and n['new_service']['present'] is False and n['full_service_capture_complete'] is True and n['registry_write_requested'] is False,'same-load actual229 new229 absent')
  require(n['captured_named_values'][r'SYSTEM\CurrentControlSet\Services\MT8167CombinedFollowup227\Start']['raw_hex']=='04000000','failed old227 actually disabled; never re-DIF it')
  pin(spec['retained_client_policy'])
  for ref in self.client_references:pin_file(ref)
  for key in ('keypad13_manifest','keypad13_handoff'):pin(spec[key]);p[key]=spec[key]
  p['GPU378_manifest']=p.pop('GPU368_manifest');p['GPU378_handoff']=p.pop('GPU368_handoff')
  p.update(diagnostic_only=True,driver_updates=False,planned_reboots=0)
  p['history_service_baseline_generation_reference_only']=p['service_baseline_generation']
  p['service_baseline_generation']=dict(source=spec['native'],raw_Mt6392Keys_snapshot=n['old_services']['Mt6392Keys'],raw_WDF_timestamp=n['keypad_wdf_runtime']['timestamp'],historical_strict_negative_retained=True,all_retained_full_hashes_strict=True,retained_service_count=len(n['old_services']))
  p['display_transition'].update(DIF_order=[],old_DOD_retirement='none; actual378/keypad13 retained; next normal boot diagnostic only')
  p['diagnostic_clients'][1]['arguments']=['"C:\\Windows\\System32\\'+SERVICE+'\\logs\\global-diag378"'];p['globaldiag_contract']['prefix']=p['diagnostic_clients'][1]['arguments'][0]
  p['development_display_on']=dict(macro='A04_DEVELOPMENT_DISPLAY_ON',value=1,phase=1,before_run_clients=True,after_readback=True,buttons_fix_claimed=False)
  p['main_execution_approved']=spec['main_execution_approved']
  p['gpu378_contract']=dict(schema='GPU378_DIAGNOSTIC_FRESH229_TRANSITION_V1',input=spec['_input_ref'],run=230,fresh=229,post_capture=231,install_mask=0,terminal_windows_hold=True,hardware_success_claimed=False,accepted_lock_sha256=sha(m.ACCEPTED),driver_DIF_allowed=False,old227_disabled=True)

def load_map(path,root):
 spec=read(path);lock=m.accepted()
 require(spec['schema']=='GPU378_DIAGNOSTIC_FRESH229_FINALIZE_INPUTS_V1' and spec['profile']==PROFILE and spec['capture_number']==229 and spec['hardware_owner']=='main' and spec['main_execution_approved'] is True and spec['install_targets']==[],'explicit diagnostic229 only')
 require(spec['fresh']==dict(path=FRESH_PATH,sha256=FRESH_SHA) and spec['native']['sha256']==NATIVE_SHA,'actual229 MF/native only')
 require(spec['supplement']==dict(path=ASSETS_PATH,sha256=ASSETS_SHA) and spec['mutable_baseline']==dict(path=Path(ASSETS_PATH).with_name('mutable-baseline.json').as_posix(),sha256=MUTABLE_SHA),'actual229 assets/mutable only')
 require(spec['GPU378_manifest']==lock['gpu378']['manifest'] and spec['GPU378_handoff']==lock['gpu378']['main_review'] and spec['keypad13_manifest']==lock['keypad13']['manifest'] and spec['keypad13_handoff']==lock['keypad13']['main_review'],'exact retained signed tuples')
 require(not spec.get('wifi_activation') and not spec.get('retained_package_overrides'),'no driver update overrides')
 for key in ('fresh','supplement','native','capture_contract','template_policy','retained_client_policy','actual_package_rows','GPU378_manifest','GPU378_handoff','keypad13_manifest','keypad13_handoff','boot_witness','mutable_baseline','globaldiag_manifest','globaldiag_import_proof'):
  ref=spec[key];require(set(ref)=={'path','sha256'} and re.fullmatch('[0-9a-f]{64}',ref['sha256']) and sha(rooted(root,ref['path']))==ref['sha256'],'actual pinned input '+key)
 f=read(rooted(root,FRESH_PATH));a=read(rooted(root,ASSETS_PATH));require(f['recovery_boot_id']==a['recovery_boot_id']==RECOVERY and len(f['files'])==42 and len(a['files'])==129,'actual229/42+129')
 spec['_input_ref']=dict(path=path.relative_to(root).as_posix(),sha256=sha(path));current=copy.deepcopy(spec);current['GPU368_manifest']=spec['GPU378_manifest'];current['GPU368_handoff']=spec['GPU378_handoff'];s=configure();s.TIMING=Diagnostic229(root,spec,{})
 p=s.build_policy(root,current);pins={x['path']:x['sha256'] for x in p['input_pins']};pins[spec['_input_ref']['path']]=spec['_input_ref']['sha256']
 for ref in read(rooted(root,spec['template_policy']['path']))['input_pins']:require(sha(rooted(root,ref['path']))==ref['sha256'],'retained historical input closure');pins[ref['path']]=ref['sha256']
 p['input_pins']=[dict(path=k,sha256=v) for k,v in sorted(pins.items())];return p

def validate_runtime_policy(root,p):require(p==load_map(rooted(root,p['gpu378_contract']['input']['path']),root),'exact actual229 diagnostic policy')
def generate(p):
 require(p['profile']==PROFILE and p['install_targets']==[] and p['planned_reboots']==0 and p['diagnostic_only'] is True and p['driver_updates'] is False and p['boot_transition']['boot_write_required'] is False,'diagnostic only mask0/restart0/boot0')
 return '#define A04_KEYPAD13_CHECKPOINT 1\n#define A04_DISPLAY_IDLE_BEFORE_CLIENTS 1\n'+configure().generate(p)
def paths():return configure().paths()
def assert_paths(image,capture=False):return configure().assert_paths(image,capture)
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for name in ('workspace','map','out'):ap.add_argument('--'+name,type=Path,required=True)
 ap.add_argument('--finalize',action='store_true');a=ap.parse_args();require(a.finalize,'actual refs required');root=a.workspace.resolve(strict=True);out=(root/a.out).resolve();require(out.is_relative_to(root) and not out.exists(),'new root-local output')
 p=load_map(rooted(root,a.map),root);h=generate(p);out.mkdir(parents=True);(out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(h)
 for name in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/name).write_text(json.dumps(p,indent=2)+'\n')
 print('GPU378_DIAGNOSTIC229_FINALIZED_NOT_BUILT_OR_DEPLOYED')
if __name__=='__main__':main()
