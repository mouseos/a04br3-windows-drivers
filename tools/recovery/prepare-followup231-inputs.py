#!/usr/bin/env python3
"""Actual231 baseline preparation; unresolved candidate schema always rejects finalization."""
import argparse,copy,importlib.util,json,re
from pathlib import Path
sp=importlib.util.spec_from_file_location('retained229',Path(__file__).with_name('prepare-gpu378-diagnostic229-inputs.py'));old=importlib.util.module_from_spec(sp);sp.loader.exec_module(old)
require,read,sha,rooted=old.require,old.read,old.sha,old.rooted
PROFILE='combined-features-fresh231-reviewed-followup-v1'
BASELINE_PROFILE='combined-features-fresh231-retained-baseline-draft-v1'
SERVICE=DIRECTORY='MT8167CombinedFollowup231'
FRESH_PATH='port/xnu/out/private-windows-rt-prebackup-run231/manifest.json'
FRESH_SHA='2fc91c1bc63177947ec861a5a848b27d6a910f6f497782f540205616c56ba21c'
RECOVERY='e4a25046-934f-4402-84e2-e9d5429f0352'
ASSETS_PATH='port/xnu/out/private-windows-rt-prebackup-run231-gpu-assets/manifest.json'
ASSETS_SHA='395c4e3a4296227d392956fcc68e4d03dc56449e87ff3ef45dfe8c9c01b72e04'
MUTABLE_SHA='909f0046ee93f2aac1fa6f6fa5049d51e75d0d54de6d4cc9d4dd6ec610a2de37'
NATIVE_SHA='b7f3c0e8dbf51ec2140c33ab91b308ef6e329bb0fd54b972802230ef567c5167'
ACCEPTED=Path(__file__).with_name('followup231-accepted.json')
# Register only a source-reviewed adapter for the real Hegel receipt schema.
# This intentionally stays empty until actual signed GPU/client evidence exists.
ADAPTERS={}

def accepted(root):
 lock=read(ACCEPTED)
 require(lock['schema']=='FOLLOWUP231_ACCEPTED_CANDIDATES_V1' and lock['status']=='actual-signed-candidates-main-reviewed','Hegel actual signed GPU/client and main review pending; no build/header/native admission')
 require(lock['mode'] in ('client-only','render-update') and lock['adapter_id'] in ADAPTERS,'exact native schema adapter not yet source-reviewed')
 for key in ('client_pair','main_review')+(() if lock['mode']=='client-only' else ('GPU_candidate',)):
  ref=lock[key];require(isinstance(ref,dict) and set(ref)=={'path','sha256'} and re.fullmatch('[0-9a-f]{64}',ref['sha256']) and ref['sha256']!='0'*64 and sha(rooted(root,ref['path']))==ref['sha256'],'missing actual candidate ref '+key)
 require(lock['mode']!='client-only' or lock['GPU_candidate'] is None,'client-only cannot smuggle a driver')
 return lock

def configure_baseline():
 s=old.configure();s.CAPTURE=231;s.PROFILE=s.base.PROFILE=BASELINE_PROFILE;s.SERVICE=s.DIRECTORY=s.base.SERVICE=s.base.DIRECTORY=SERVICE;s.INSTALL=s.base.INSTALL=[];return s

class Baseline231(old.Diagnostic229):
 contract_family='FOLLOWUP231_BASELINE'
 template_profile=old.PROFILE
 def validate_capture_contract(self,c,pin):require(c['schema']=='FOLLOWUP231_BASELINE_FRESH231_CAPTURE_CONTRACT_V1','actual231 baseline contract only')
 def boot(self,b,transition,fresh,bootid,pin,pin_file):
  require(b['schema']=='FOLLOWUP231_RETAINED_BOOT_V1' and b['source_manifest_sha256']==fresh==FRESH_SHA and b['recovery_boot_id']==bootid==RECOVERY,'actual231 boot chain')
  previous=pin(b['previous_witness']);prior=old.Diagnostic229(self.root,{},{});e=prior.boot(previous,copy.deepcopy(transition),old.FRESH_SHA,old.RECOVERY,pin,pin_file)
  seal=pin(b['run230_seal']);result=pin(b['run230_result'])
  require(seal['schema']=='GPU378_DIAGNOSTIC_RUN230_MAIN_SEAL_V1' and seal['status']=='validated-main-seal' and seal['boot_write_required'] is False and seal['matched_boot_sha256']==old.m.old.BOOT,'actual230 retained boot seal')
  require(result['transaction']=='combined-run230' and result['seal_sha256']==b['run230_seal']['sha256'] and result['boot_written'] is False and result['gpt_written'] is False and result['planned_runtime_restarts']==0,'actual230 boot/GPT/restart0')
  require(b['physical_read_this_capture'] is False and b['no_later_boot_or_gpt_write_reported_by_main'] is True and b['continuous_independent_monitoring_performed'] is False,'main retention report, not physical read')
  return dict(e,kind='retained-chain-through-run230',hardware_success_inferred=False)
 def finish(self,p,spec,n,pin,pin_file):
  require(n['capture_number']==231 and n['new_service_name']==SERVICE and n['new_service']['present'] is False and n['full_service_capture_complete'] is True and n['registry_write_requested'] is False,'actual231 complete same-load native/new231 absent')
  cp=n['keypad_checkpoint'];require(cp['present'] and cp['complete'] and not cp['errors'] and all(x['schema_valid'] and x['epoch_binding']=='current-load' for x in cp['records']),'actual bounded CP schema/epoch maintained')
  pin(spec['retained_client_policy'])
  for ref in self.client_references:pin_file(ref)
  for key in ('keypad13_manifest','keypad13_handoff'):pin(spec[key]);p[key]=spec[key]
  p['GPU378_manifest']=p.pop('GPU368_manifest');p['GPU378_handoff']=p.pop('GPU368_handoff')
  p.update(draft=True,normal_install_allowed=False,main_execution_approved=False,diagnostic_only=True,driver_updates=False,planned_reboots=0,baseline_only=True)
  p['display_transition'].update(DIF_order=[],old_DOD_retirement='baseline only; future targets unresolved')
  p['service_baseline_generation']=dict(source=spec['native'],raw_Mt6392Keys_snapshot=n['old_services']['Mt6392Keys'],raw_WDF_timestamp=n['keypad_wdf_runtime']['timestamp'],all_retained_full_hashes_strict=True,retained_service_count=len(n['old_services']))
  p['followup231_pending']=dict(schema='FOLLOWUP231_PENDING_V1',run=232,post_capture=233,new_service=SERVICE,GPU_candidate=None,client_pair=None,next_install_targets=None,next_planned_reboots=None,build_allowed=False,native_patch_allowed=False,device_operations_allowed=False)

def baseline(path,root):
 spec=read(path)
 require(spec['schema']=='FOLLOWUP231_BASELINE_INPUTS_V1' and spec['capture_number']==231 and spec['hardware_owner']=='main' and spec['install_targets']==[],'baseline preparation only')
 require(spec['fresh']==dict(path=FRESH_PATH,sha256=FRESH_SHA) and spec['native']['sha256']==NATIVE_SHA and spec['supplement']==dict(path=ASSETS_PATH,sha256=ASSETS_SHA) and spec['mutable_baseline']==dict(path=Path(ASSETS_PATH).with_name('mutable-baseline.json').as_posix(),sha256=MUTABLE_SHA),'actual231 pins only')
 for key in ('fresh','supplement','native','capture_contract','template_policy','retained_client_policy','actual_package_rows','GPU378_manifest','GPU378_handoff','keypad13_manifest','keypad13_handoff','boot_witness','mutable_baseline','globaldiag_manifest','globaldiag_import_proof'):
  ref=spec[key];require(set(ref)=={'path','sha256'} and re.fullmatch('[0-9a-f]{64}',ref['sha256']) and sha(rooted(root,ref['path']))==ref['sha256'],'actual baseline ref '+key)
 f=read(rooted(root,FRESH_PATH));a=read(rooted(root,ASSETS_PATH));require(f['recovery_boot_id']==a['recovery_boot_id']==RECOVERY and len(f['files'])==42 and len(a['files'])==131,'actual23142+131')
 require(not spec.get('filter_root_transitions') and not spec.get('retained_package_overrides') and not spec.get('wifi_activation'),'no transition exemption or driver override in retained baseline')
 current=copy.deepcopy(spec);current['profile']=BASELINE_PROFILE;current['GPU368_manifest']=spec['GPU378_manifest'];current['GPU368_handoff']=spec['GPU378_handoff'];s=configure_baseline();s.TIMING=Baseline231(root,spec,{})
 p=s.build_policy(root,current);pins={x['path']:x['sha256'] for x in p['input_pins']}
 for ref in read(rooted(root,spec['template_policy']['path']))['input_pins']:require(sha(rooted(root,ref['path']))==ref['sha256'],'historical retained closure');pins[ref['path']]=ref['sha256']
 pins[path.relative_to(root).as_posix()]=sha(path);p['input_pins']=[dict(path=k,sha256=v) for k,v in sorted(pins.items())];return p

def load_map(path,root):
 lock=accepted(root);return ADAPTERS[lock['adapter_id']].load_map(path,root,lock)
def generate(p):
 lock=read(ACCEPTED);require(lock['status']=='actual-signed-candidates-main-reviewed' and lock['adapter_id'] in ADAPTERS,'actual candidate adapter pending')
 require(p['profile']==PROFILE and p['draft'] is False and p['main_execution_approved'] is True,'reviewed final policy only')
 require(p['install_targets']==([] if lock['mode']=='client-only' else ['render']) and p['planned_reboots']==(0 if lock['mode']=='client-only' else 1),'minimal restart and affected role only')
 h=ADAPTERS[lock['adapter_id']].generate(p,lock)
 require('#define A04_VIDPN_CALLBACK_TRACE 1' in h,'strict trace guard must be enabled by reviewed adapter')
 return h
def validate_runtime_policy(root,p):
 lock=accepted(root);return ADAPTERS[lock['adapter_id']].validate_runtime_policy(root,p,lock)
def paths():return configure_baseline().paths()
def assert_paths(image,capture=False):return configure_baseline().assert_paths(image,capture)
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for name in ('workspace','map','out'):ap.add_argument('--'+name,type=Path,required=True)
 g=ap.add_mutually_exclusive_group(required=True);g.add_argument('--prepare-baseline',action='store_true');g.add_argument('--finalize',action='store_true');a=ap.parse_args();root=a.workspace.resolve(strict=True);out=(root/a.out).resolve();require(out.is_relative_to(root) and not out.exists(),'new root-local output only')
 if a.finalize:
  p=load_map(rooted(root,a.map),root);h=generate(p);out.mkdir(parents=True);(out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(h)
  for name in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/name).write_text(json.dumps(p,indent=2)+'\n')
  return
 p=baseline(rooted(root,a.map),root);out.mkdir(parents=True);(out/'retained-baseline-draft.json').write_text(json.dumps(p,indent=2)+'\n')
 pending=dict(schema='FOLLOWUP231_FINALIZE_INPUTS_V1',profile=PROFILE,capture_number=231,hardware_owner='main',main_execution_approved=False,baseline_inputs=dict(path=rooted(root,a.map).relative_to(root).as_posix(),sha256=sha(rooted(root,a.map))),mode=None,GPU_candidate=None,client_pair=None,filter_root_transitions=[],install_targets=None,planned_reboots=None)
 (out/'finalize-map.pending.json').write_text(json.dumps(pending,indent=2)+'\n');print('ACTUAL231_BASELINE_VERIFIED_CANDIDATES_PENDING_NO_HEADER_BUILD_NATIVE_OR_DEVICE')
if __name__=='__main__':main()
