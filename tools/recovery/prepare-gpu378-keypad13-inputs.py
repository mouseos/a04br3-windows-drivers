#!/usr/bin/env python3
"""Actual227 -> run228/post229, render378 + keypad1.3. Shared200/377 emitter."""
import argparse,copy,importlib.util,json,re
from pathlib import Path
sp=importlib.util.spec_from_file_location('retained377',Path(__file__).with_name('prepare-gpu377-fresh225-inputs.py'));old=importlib.util.module_from_spec(sp);sp.loader.exec_module(old)
require,read,sha,rooted=old.require,old.read,old.sha,old.rooted
PROFILE='combined-features-fresh227-gpu378-keypad13-client375-v1'
SERVICE=DIRECTORY='MT8167CombinedFollowup227'
FRESH_PATH='port/xnu/out/private-windows-rt-prebackup-run227/manifest.json'
FRESH_SHA='43050ae697167e6d17bb472429270d58eab8210c85ba53389731fb2d8a9205e9'
RECOVERY='0478d105-54cf-4e6b-a04d-b2c48cc2f9c4'
ASSETS_PATH='port/xnu/out/private-windows-rt-prebackup-run227-gpu-assets/manifest.json'
ASSETS_SHA='d27c0b5bd01219bf8865cbcd087046d869619041aaa867309e9bb6fb178032d9'
MUTABLE_SHA='deacfdaf8abe8f64b655140019c76fd32113d00e5410485126f36333cdeeaff1'
NATIVE_SHA='960e31638399550bc70b52931ba3378a82379ce234615b626b4498436ac88417'
BASE_SYS=old.SYS
KEYPAD_BASE='a3041d0f2e386171a8211b7338fc9aaadb0455e524b66e8082fc3776eceeb13f'
ACCEPTED=Path(__file__).with_name('gpu378-keypad13-accepted.json')

def accepted():
 lock=read(ACCEPTED)
 require(lock['schema']=='GPU378_KEYPAD13_ACCEPTED_V1' and lock['status']=='actual-signed-manifests-main-reviewed' and isinstance(lock.get('gpu378'),dict),'GPU378 actual signed manifest/main review pending; no placeholder finalization')
 g=lock['gpu378']
 require(set(g)=={'manifest','main_review','source_commit','sys_sha256','umd_sha256'},'exact378 pin shape')
 require(re.fullmatch('[0-9a-f]{40}',g['source_commit']) and all(re.fullmatch('[0-9a-f]{64}',g[k]) and g[k]!='0'*64 for k in ('sys_sha256','umd_sha256')),'actual378 source/SYS/UMD pins')
 return lock

def configure(lock):
 old.m.m.configure_capture(227);s=old.m.m.shared
 s.PROFILE=s.base.PROFILE=PROFILE;s.SERVICE=s.DIRECTORY=s.base.SERVICE=s.base.DIRECTORY=SERVICE;s.INSTALL=s.base.INSTALL=['render','keypad'];s.SOURCE=lock['gpu378']['source_commit'];s.MANIFEST=lock['gpu378']['manifest']['sha256']
 return s

class Fresh227(old.Fresh225):
 contract_family='GPU378'
 @property
 def install_targets(self):return ['render','keypad']
 def validate_capture_contract(self,c,pin):require(c['schema']=='GPU378_FRESH227_CAPTURE_CONTRACT_V1','actual227 capture contract')
 def validate_manifest(self,g,r,spec):
  lock=accepted()['gpu378'];require(spec['GPU378_manifest']==lock['manifest'] and spec['GPU378_handoff']==lock['main_review'],'exact reviewed378 refs')
  require(g['schema']=='ge8300-wire378-three-tu-v1' and g['source_commit']==g['runtime_source_commit']==g['recipe_source_commit']==lock['source_commit'],'actual wire378 recipe/source')
  require(g['signed'] is True and g['success'] is True and g['signer_thumbprint']==old.m.m.shared.THUMB and g['driver_version']=='0.0.3.78' and g['driver_date']=='11/11/2026','actual378 signed tuple')
  require(g['SDK_rebuild']==g['PCO_rebuild']==g['native_owner_rebuild']==0 and g['hardware_verified'] is False,'retained native/SDK; no hardware success')
  require(r['status']=='main-independent-artifact-verification-pass' and r['signed_manifest_sha256']==lock['manifest']['sha256'] and r['source_commit']==lock['source_commit'] and r['SYS_sha256']==lock['sys_sha256'] and r['UMD_sha256']==lock['umd_sha256'] and r['all_output_hashes_verified']==15 and r['other_package_leaves_equal377'] is True and r['five_BCBF_signatures_verified'] is True and r['signed_payloads_unchanged'] is True and r['all_recorded_steps_return0'] is True and r['hardware_verified'] is False,'exact main378 review scope')
 def validate_output_review(self,r,h):
  lock=accepted()['gpu378'];require(len(h)==15 and h['ge8300-render.sys']==lock['sys_sha256'] and h['ge8300-umd.dll']==lock['umd_sha256'],'378 full15/SYS/UMD actual')
 def adopt(self,role,oldrow,originals,hashes):
  row=copy.deepcopy(oldrow);lock=accepted()
  if role=='render':
   require(oldrow['driver_version']=='0.0.3.77' and oldrow['hashes']['sys']==BASE_SYS,'actual selected377 only')
   ref=self.spec['GPU378_manifest'];row.update(directory=Path(ref['path']).parent.as_posix(),manifest=ref['path'],manifest_sha256=ref['sha256'],source_commit=lock['gpu378']['source_commit'],binary_source_commit=lock['gpu378']['source_commit'],driver_version='0.0.3.78',driver_date='11/11/2026')
   for x in row['extra_members']:
    name='Windows/System32/'+x['installed'].replace('\\','/');baseline=originals[name]['sha256'];x.update(baseline_sha256=baseline,sha256=hashes[x['name']])
    if x['name']=='ge8300-umd.dll':require(x['sha256']==lock['gpu378']['umd_sha256'] and x['sha256']!=baseline,'explicit actual wire378 UMD replacement')
    else:require(x['sha256']==baseline,'FW byte retained')
  else:
   require(role=='keypad' and oldrow['driver_version']=='0.0.1.2' and oldrow['hashes']['sys']==KEYPAD_BASE,'actual keypad1.2 only')
   k=lock['keypad13'];require(self.spec['keypad13_manifest']==k['manifest'] and self.spec['keypad13_handoff']==k['main_review'],'exact reviewed keypad1.3')
   row.update(directory=Path(k['manifest']['path']).parent.joinpath('package').as_posix(),manifest=k['manifest']['path'],manifest_sha256=k['manifest']['sha256'],source_commit=k['source_commit'],driver_version='0.0.1.3',driver_date='11/10/2026')
  return row
 def validate_transport(self,p,h):
  super().validate_transport(p,h)
  require(p['keypad']['hashes']['sys']==accepted()['keypad13']['sys_sha256'],'actual keypad1.3 SYS')
 def client(self,prior,originals,origin_sources,preserved,sources,witnesses,**unused):
  require(prior['profile']==old.PROFILE,'actual run226 policy template only')
  ref=self.spec['retained_client_policy'];q=rooted(self.root,ref['path']);require(sha(q)==ref['sha256'],'original signed375 policy ref');ancestral=read(q)
  client=super().client(ancestral,originals,origin_sources,preserved,sources,witnesses)
  require(client==prior['diagnostic_clients'][0],'same actual375 client retained')
  # The shared builder creates UMD pre-copy witness before preserving files.
  require('Windows/System32/ge8300-umd.dll' in witnesses,'explicit UMD old-byte witness')
  return client
 def boot(self,b,transition,fresh,bootid,pin,pin_file):
  require(b['schema']=='GPU378_RETAINED_BOOT227_V1' and b['source_manifest_sha256']==fresh==FRESH_SHA and b['recovery_boot_id']==bootid==RECOVERY,'actual227 retained boot chain')
  previous=pin(b['previous_witness']);p225=old.Fresh225(self.root,dict(fresh=dict(path=old.FRESH_PATH,sha256=old.FRESH_SHA)),{})
  evidence=p225.boot(previous,{},old.FRESH_SHA,old.RECOVERY,pin,pin_file)
  seal=pin(b['run226_seal']);result=pin(b['run226_result'])
  require(seal['schema']=='GPU377_RUN226_MAIN_SEAL_V1' and seal['status']=='validated-main-seal' and seal['boot_write_required'] is False and seal['matched_boot_sha256']==old.BOOT,'actual226 retained seal')
  require(result['transaction']=='combined-run226' and result['seal_sha256']==b['run226_seal']['sha256'] and result['boot_written'] is False and result['gpt_written'] is False,'actual226 boot/GPT0 result')
  require(b['physical_read_this_capture'] is False and b['no_later_boot_or_gpt_write_reported_by_main'] is True and b['continuous_independent_monitoring_performed'] is False,'retention report is not physical read or continuous monitoring')
  transition.update(mode='retained-panel-timing-DTM0-209-v1',boot_path=previous['host_artifact']['path'],boot_sha256=old.BOOT,baseline_boot_sha256=old.BOOT,boot_write_required=False,gpt_written=False,mixed_old_new_allowed=False,next_windows_boot_blocked_until_matched_boot=False,firmware_manifest=previous['firmware_composition_manifest'])
  return dict(evidence,kind='retained-chain-209-through-run226',hardware_success_inferred=False)
 def finish(self,p,spec,n,pin,pin_file):
  require(n['capture_number']==227 and n['new_service_name']==SERVICE and n['new_service']['present'] is False and n['registry_write_requested'] is False and n['full_service_capture_complete'] is True,'actual same-load227 / new227 absent')
  pin(spec['retained_client_policy'])
  k=accepted()['keypad13'];km=pin(spec['keypad13_manifest']);kr=pin(spec['keypad13_handoff'])
  require(km['schema']=='mt8167-keypad13-incremental-v1' and km['source_commit']==k['source_commit'] and km['version']=='0.0.1.3' and kr['manifest_sha256']==k['manifest']['sha256'] and kr['SYS_sha256']==k['sys_sha256'] and kr['hardware_verified'] is False,'actual keypad1.3 raw receipt and main seal')
  for row in km['outputs']:pin_file(dict(path=(Path(k['manifest']['path']).parent/row['path']).as_posix(),sha256=row['sha256']))
  p['GPU378_manifest']=p.pop('GPU368_manifest');p['GPU378_handoff']=p.pop('GPU368_handoff');p['keypad13_manifest']=spec['keypad13_manifest'];p['keypad13_handoff']=spec['keypad13_handoff']
  p['display_transition'].update(DIF_order=['render','keypad'],old_DOD_retirement='377 to wire378; keypad1.2 to1.3; transport372/WiFi1031/battery1.8/FW/client375 retained; boot/GPT0')
  p['development_display_on']=dict(macro='A04_DEVELOPMENT_DISPLAY_ON',value=1,phase=1,before_run_clients=True,after_readback=True,buttons_fix_claimed=False)
  p['diagnostic_clients'][1]['arguments']=['"C:\\Windows\\System32\\'+SERVICE+'\\logs\\global-diag378"'];p['globaldiag_contract']['prefix']=p['diagnostic_clients'][1]['arguments'][0]
  p['main_execution_approved']=spec['main_execution_approved']
  p['gpu378_contract']=dict(schema='GPU378_FRESH227_TRANSITION_V1',input=spec['_input_ref'],run=228,fresh=227,post_capture=229,install_mask=1152,terminal_windows_hold=True,hardware_success_claimed=False,accepted_lock_sha256=sha(ACCEPTED))

def load_map(path,root):
 lock=accepted();spec=read(path)
 require(spec['schema']=='GPU378_FRESH227_FINALIZE_INPUTS_V1' and spec['profile']==PROFILE and spec['capture_number']==227 and spec['hardware_owner']=='main' and spec['main_execution_approved'] is True and spec['install_targets']==['render','keypad'],'explicit227/378+keypad only')
 require(spec['fresh']==dict(path=FRESH_PATH,sha256=FRESH_SHA) and spec['native']['sha256']==NATIVE_SHA,'actual227 MF/native only')
 require(spec['GPU378_manifest']==lock['gpu378']['manifest'] and spec['GPU378_handoff']==lock['gpu378']['main_review'],'actual accepted378 refs')
 require(spec['supplement']==dict(path=ASSETS_PATH,sha256=ASSETS_SHA) and spec['mutable_baseline']==dict(path=Path(ASSETS_PATH).with_name('mutable-baseline.json').as_posix(),sha256=MUTABLE_SHA),'actual227 assets/mutable only')
 require(not spec.get('wifi_activation') and not spec.get('retained_package_overrides'),'no unrelated override')
 for key in ('fresh','supplement','native','capture_contract','template_policy','retained_client_policy','actual_package_rows','GPU378_manifest','GPU378_handoff','keypad13_manifest','keypad13_handoff','boot_witness','mutable_baseline','globaldiag_manifest','globaldiag_import_proof'):
  ref=spec[key];require(set(ref)=={'path','sha256'} and re.fullmatch('[0-9a-f]{64}',ref['sha256']) and sha(rooted(root,ref['path']))==ref['sha256'],'actual input binding '+key)
 fresh=read(rooted(root,FRESH_PATH));assets=read(rooted(root,ASSETS_PATH));require(fresh['recovery_boot_id']==assets['recovery_boot_id']==RECOVERY and len(fresh['files'])==42 and len(assets['files'])==129,'actual227/42+129')
 spec['_input_ref']=dict(path=path.relative_to(root).as_posix(),sha256=sha(path));current=copy.deepcopy(spec);current['GPU368_manifest']=spec['GPU378_manifest'];current['GPU368_handoff']=spec['GPU378_handoff']
 s=configure(lock);s.TIMING=Fresh227(root,spec,{})
 p=s.build_policy(root,current)
 pins={x['path']:x['sha256'] for x in p['input_pins']};pins[spec['_input_ref']['path']]=spec['_input_ref']['sha256']
 prior=read(rooted(root,spec['template_policy']['path']))
 for ref in prior['input_pins']:require(sha(rooted(root,ref['path']))==ref['sha256'],'historical375 signed closure');pins[ref['path']]=ref['sha256']
 p['input_pins']=[dict(path=k,sha256=v) for k,v in sorted(pins.items())];return p

def validate_runtime_policy(root,p):require(p==load_map(rooted(root,p['gpu378_contract']['input']['path']),root),'full actual-map policy equality')
def generate(p):
 require(p['profile']==PROFILE and p['install_targets']==['render','keypad'] and p['planned_reboots']==1 and p['boot_transition']['boot_write_required'] is False,'378 mask1152/restart1/boot0')
 return '#define A04_KEYPAD13_CHECKPOINT 1\n#define A04_DISPLAY_IDLE_BEFORE_CLIENTS 1\n'+configure(accepted()).generate(p)
def paths():return configure(accepted()).paths()
def assert_paths(image,capture=False):return configure(accepted()).assert_paths(image,capture)
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for name in ('workspace','map','out'):ap.add_argument('--'+name,type=Path,required=True)
 ap.add_argument('--finalize',action='store_true');args=ap.parse_args();require(args.finalize,'actual refs only');root=args.workspace.resolve(strict=True);out=(root/args.out).resolve();require(out.is_relative_to(root) and not out.exists(),'new root-local output only')
 p=load_map(rooted(root,args.map),root);header=generate(p);out.mkdir(parents=True);(out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(header)
 for name in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/name).write_text(json.dumps(p,indent=2)+'\n')
 print('GPU378_FRESH227_FINALIZED_NOT_BUILT_OR_DEPLOYED')
if __name__=='__main__':main()
