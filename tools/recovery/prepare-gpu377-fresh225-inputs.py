#!/usr/bin/env python3
"""実fresh225 → run226/render377。既200 emitter/検証を共有。"""
import argparse, copy, hashlib, importlib.util, json
from pathlib import Path
sp=importlib.util.spec_from_file_location('retained_output',Path(__file__).with_name('prepare-gpu371-output-inputs.py'))
m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m)
require,read,sha,rooted=m.require,m.read,m.sha,m.rooted
PROFILE='combined-features-fresh225-gpu377-output-client375-v1'
SERVICE=DIRECTORY='MT8167CombinedFollowup225'
FRESH_PATH='port/xnu/out/private-windows-rt-prebackup-run225/manifest.json'
FRESH_SHA='9aa07e695ccc308156cdb11f960cda2a1d4b79a9249ef610b812e026bcf577eb'
RECOVERY='e31770a3-ba85-40d2-ab9e-ada6ced949ab'
ASSETS_PATH='port/xnu/out/private-windows-rt-prebackup-run225-gpu-assets/manifest.json'
ASSETS_SHA='f148fa1807c2ebf95a84d59209499be2b598214c2e2ac691e1f123432213585e'
MUTABLE_SHA='9e2052a7a8af2ef470f8d02dfff1a66f9f1f8f94858e3a3824e1a6206a2ab149'
MANIFEST='767c1877268042331a0eee5125e080ccc1b2ec0be125f1b6665a9a171dfeff0e'
SOURCE='956008469031c7d0a8872a821250214b43b905b2'
SYS='b4a2a17f98b377a2767b4daef8d5e4daa9e2110b0b6f9a8a422c04fcf52e29e4'
BASE_SYS='0ae5b536bd87cd9a7a673bae61a4cebd029251fd446c77e5150de2acf21acc22'
BOOT=m.m.BOOT

def configure():
 m.m.configure_capture(225);s=m.m.shared
 s.PROFILE=s.base.PROFILE=PROFILE;s.SERVICE=s.DIRECTORY=s.base.SERVICE=s.base.DIRECTORY=SERVICE
 s.INSTALL=s.base.INSTALL=['render'];s.SOURCE=SOURCE;s.MANIFEST=MANIFEST
 return s

class Fresh225(m.Output371):
 contract_family='GPU377'
 def validate_capture_contract(self,c,pin):
  require(c['schema']=='GPU377_FRESH225_CAPTURE_CONTRACT_V1','実225 contractだけ。旧contract改名不可')
 def validate_manifest(self,g,r,spec):
  require(spec['GPU377_manifest']['sha256']==MANIFEST and g['schema']=='ge8300-gdi377-render-sign-only-v1','正式377のみ')
  require(g['recipe_source_commit']==g['runtime_source_commit']==g['kernel_binary_source_commit']==SOURCE and g['signed'] is True and g['success'] is True and g['signer_thumbprint']==m.m.shared.THUMB,'正式377 source/署名')
  require(g['inf_driverver']=='11/11/2026,0.0.3.77' and g['retained_transport372_UMD_FW_clients'] is True and g['hardware_verified'] is False,'実署名を描画成功へ昇格しない')
  require(r['status']=='main-independent-artifact-verification-pass' and r['signed_manifest_sha256']==MANIFEST and r['source_commit']==SOURCE and r['SYS_sha256']==SYS and r['all_other_package_leaves_equal376'] is True and r['imports_equal376'] is True and r['five_BCBF_signatures_verified'] is True and r['hardware_verified'] is False,'main実377 review')
 def validate_output_review(self,r,h):require(h['ge8300-render.sys']==SYS and len(h)==15,'実377全15leaf')
 def package_rows(self,rows,pin):
  require(not self.spec.get('retained_package_overrides') and not self.spec.get('wifi_activation') and not self.spec.get('wifi_update_package'),'render以外変更不可')
  return copy.deepcopy(rows)
 def adopt(self,role,old,originals,hashes):
  require(role=='render' and old['driver_version']=='0.0.3.76' and old['hashes']['sys']==BASE_SYS,'実選択376からだけ')
  row=copy.deepcopy(old);ref=self.spec['GPU377_manifest']
  row.update(directory=Path(ref['path']).parent.as_posix(),manifest=ref['path'],manifest_sha256=MANIFEST,source_commit=SOURCE,binary_source_commit=SOURCE,driver_version='0.0.3.77',driver_date='11/11/2026',artifact_review=self.spec['GPU377_handoff'])
  for x in row.get('extra_members',[]):
   name='Windows/System32/'+x['installed'].replace('\\','/')
   require(originals[name]['sha256']==hashes[x['name']]==x['sha256'],'実UMD/FW保持')
   x['baseline_sha256']=originals[name]['sha256']
  return row
 def validate_transport(self,packages,hashes):
  require(packages['display']['driver_version']=='0.0.3.72' and packages['wifi']['driver_version']=='2026.11.11.1031' and packages['wifi']['hashes']['sys']=='e6fed86cceb0da99785619906b989c6e65fcf34c8c8c295878bfa2c1512be7b0' and packages['battery']['driver_version']=='0.0.1.8' and packages['battery']['hashes']['sys']=='04af6c3c95aa716b37aa0c1e976cf0520bca16b90e6b49fd43b9b04505261cfd','実372/1031/電池1.8保持')
  for kind,name in packages['display']['names'].items():require(packages['display']['hashes'][kind]==hashes['display-transport/'+name],'transport372 trio保持')
 def client(self,prior,originals,origin_sources,preserved,sources,witnesses,**unused):
  # Reuse the exact signed375 pair from actual combined policy, not package377's old356.
  c=copy.deepcopy(prior['diagnostic_clients'][0])
  require(prior['profile']=='combined-features-fresh221-gpu376-battery18-wifi1031-client375-v1','正式combined375 templateのみ')
  cc=prior['baseline221_client_transition_reference_only']
  import sys
  sys.path.insert(0,str(Path(__file__).resolve().parent))
  import gpu373_client_admission as admission
  require(set(cc['client_replacements'])=={'Windows/System32/'+admission.LAUNCHER,'Windows/System32/'+admission.EVALUATOR},'client375正式pairだけ')
  signed={name:admission._signed_leaf(self.root,dict(file=row['candidate'],manifest=row['manifest'],import_proof=row['import_proof'])) for name,row in cc['client_replacements'].items()}
  launcher=signed['Windows/System32/'+admission.LAUNCHER];evaluator=signed['Windows/System32/'+admission.EVALUATOR]
  require(evaluator['phase']=='evaluator' and evaluator['runtime_source_commit']==admission.CLIENT375_SOURCE and launcher['phase']=='launcher' and launcher['evaluator_source_commit']==admission.CLIENT375_SOURCE and launcher['evaluator_sha256']==cc['client_replacements']['Windows/System32/'+admission.EVALUATOR]['candidate']['sha256'],'実signed375 source/pair結合')
  for name,row in cc['client_replacements'].items():require(originals[name]['sha256']==preserved[name]==row['candidate']['sha256'],'実保持client375 pair')
  require(c['sha256']==cc['client_replacements']['Windows/System32/'+c['exe']]['candidate']['sha256'] and len(c['dependencies'])==1 and c['dependencies'][0]['sha256']==cc['client_replacements']['Windows/System32/'+c['dependencies'][0]['exe']]['candidate']['sha256'],'375 pairの実table結合')
  c['phase']=1
  return c
 def boot(self,boot,transition,fresh,bootid,pin,pin_file):
  require(boot['schema']=='GPU377_RETAINED_BOOT225_V1' and boot['source_manifest_sha256']==fresh==self.spec['fresh']['sha256'] and boot['recovery_boot_id']==bootid and boot['physical_read_this_capture'] is False and boot['no_later_boot_or_gpt_write_reported_by_main'] is True and boot['continuous_independent_monitoring_performed'] is False,'実225保持申告。独立監視とはしない')
  previous=pin(boot['previous_witness'])
  # Previous221 witness already joins the actual209 write through actual219.
  v=importlib.util.spec_from_file_location('client375',Path(__file__).with_name('prepare-gpu375-client-inputs.py'));cm=importlib.util.module_from_spec(v);v.loader.exec_module(cm)
  evidence=cm.m.retained_client_boot(previous,{},previous['source_manifest_sha256'],previous['recovery_boot_id'],pin,pin_file,221)
  seal=pin(boot['run224_seal']);result=pin(boot['run224_result']);cap=pin(boot['capture225'])
  require(boot['capture225']==self.spec['fresh'] and cap['status']=='captured-unmounted' and cap['read_only'] and cap['unmounted'] and not cap['errors'],'実fresh225')
  require(seal['schema']=='GPU375_COMBINED_RUN222_MAIN_SEAL_V1' and seal['status']=='validated-main-seal' and seal['boot_write_required'] is False and seal['matched_boot_sha256']==BOOT,'実224再開seal')
  require(result['transaction']=='combined-run222' and result['seal_sha256']==boot['run224_seal']['sha256'] and result['boot_written'] is False and result['gpt_written'] is False,'実224結果のboot/GPT0。予定だけで不可')
  require(boot['boot_sha256']==previous['boot_sha256']==BOOT and boot['host_artifact']==previous['host_artifact'] and boot['firmware_composition_manifest']==previous['firmware_composition_manifest'],'実209 boot出自保持')
  transition.update(mode='retained-panel-timing-DTM0-209-v1',boot_path=boot['host_artifact']['path'],boot_sha256=BOOT,baseline_boot_sha256=BOOT,boot_write_required=False,gpt_written=False,mixed_old_new_allowed=False,next_windows_boot_blocked_until_matched_boot=False,firmware_manifest=boot['firmware_composition_manifest'])
  return dict(evidence,kind='retained-chain-209-through-resume224',physical_read_this_capture=False,hardware_success_inferred=False)
 def finish(self,p,spec,n,pin,pin_file):
  require(n['capture_number']==225 and n['new_service_name']==SERVICE and n['new_service']['present'] is False and n['registry_write_requested'] is False and n['full_service_capture_complete'] is True and n['recovery_boot_id']==p['recovery_boot_id'],'実225 native結果必須。未着を補わない')
  for key in ('gpu373_client_contract','gpu375_combined_contract','baseline221_client_transition_reference_only'):
   if key in p:p['history_'+key+'_reference_only']=p.pop(key)
  p['GPU377_manifest']=p.pop('GPU368_manifest');p['GPU377_handoff']=p.pop('GPU368_handoff')
  p['main_execution_approved']=True
  p['display_transition'].update(DIF_order=['render'],old_DOD_retirement='render376 to377 only; transport372/WiFi1031/battery1.8/UMD/FW/client375 retained; boot98de retained')
  p['standard_present_contract'].update(driver_package_source_commit=SOURCE,driver_signed_manifest_sha256=MANIFEST)
  p['diagnostic_clients'][1]['arguments']=['"C:\\Windows\\System32\\'+SERVICE+'\\logs\\global-diag377"'];p['globaldiag_contract']['prefix']=p['diagnostic_clients'][1]['arguments'][0]
  p['development_display_on']=dict(macro='A04_DEVELOPMENT_DISPLAY_ON',value=1,phase=1,buttons_fix_claimed=False)
  p['gpu377_contract']=dict(schema='GPU377_FRESH225_TRANSITION_V1',input=spec['_input_ref'],run=226,fresh=225,post_capture=227,install_mask=1024,terminal_windows_hold=True,hardware_success_claimed=False)

def load_map(path,root):
 spec=read(path)
 require(spec.get('schema')=='GPU377_FRESH225_FINALIZE_INPUTS_V1' and spec.get('profile')==PROFILE and spec.get('capture_number')==225 and spec.get('hardware_owner')=='main' and spec.get('main_execution_approved') is True and spec.get('install_targets')==['render'],'明示377/実225 main入力だけ')
 require(spec['fresh']==dict(path=FRESH_PATH,sha256=FRESH_SHA) and spec['GPU377_manifest']['sha256']==MANIFEST,'実suffix/377 manifest固定')
 # Validate the real same-load native before any output is created.
 refs={k:spec[k] for k in ('fresh','supplement','native','capture_contract','template_policy','actual_package_rows','GPU377_manifest','GPU377_handoff','boot_witness','mutable_baseline','globaldiag_manifest','globaldiag_import_proof')}
 for ref in refs.values():
  q=rooted(root,ref['path']);require(sha(q)==ref['sha256'],'実入力SHA')
 require(spec['supplement']==dict(path=ASSETS_PATH,sha256=ASSETS_SHA) and spec['mutable_baseline']==dict(path=str(Path(ASSETS_PATH).with_name('mutable-baseline.json')),sha256=MUTABLE_SHA),'実225 assets/mutableだけ。旧221流用禁止')
 assets=read(rooted(root,ASSETS_PATH));require(len(assets['files'])==127 and assets['recovery_boot_id']==RECOVERY,'実225 assets127集合')
 fresh=read(rooted(root,FRESH_PATH));require(fresh['recovery_boot_id']==RECOVERY and len(fresh['files'])==42,'実225/Recovery/42filesのみ')
 spec['_input_ref']=dict(path=path.relative_to(root).as_posix(),sha256=sha(path))
 current=copy.deepcopy(spec);current['GPU368_manifest']=spec['GPU377_manifest'];current['GPU368_handoff']=spec['GPU377_handoff']
 s=configure();s.TIMING=Fresh225(root,spec,{})
 p=s.build_policy(root,current)
 pins={r['path']:r['sha256'] for r in p['input_pins']};pins[spec['_input_ref']['path']]=spec['_input_ref']['sha256']
 # Preserve the signed375 receipt/proof closure from the immutable template.
 prior=read(rooted(root,spec['template_policy']['path']))
 for r in prior['input_pins']:
  q=rooted(root,r['path']);require(sha(q)==r['sha256'],'歴史375 closure変化');pins[r['path']]=r['sha256']
 p['input_pins']=[dict(path=k,sha256=v) for k,v in sorted(pins.items())]
 return p

def validate_runtime_policy(root,p):
 require(p==load_map(rooted(root,p['gpu377_contract']['input']['path']),root),'実入力からの全policy一致必須')

def generate(p):
 require(p['profile']==PROFILE and p['install_targets']==['render'] and p['planned_reboots']==1 and p['boot_transition']['boot_write_required'] is False and p['development_display_on']['value']==1,'render-only/macro1/boot0')
 return configure().generate(p)
def paths():return configure().paths()
def assert_paths(image,capture=False):return configure().assert_paths(image,capture)
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for name in ('workspace','map','out'):ap.add_argument('--'+name,type=Path,required=True)
 ap.add_argument('--finalize',action='store_true');args=ap.parse_args();require(args.finalize,'実native取得後だけ')
 root=args.workspace.resolve(strict=True);out=(root/args.out).resolve();require(out.is_relative_to(root) and not out.exists(),'root内新規出力のみ')
 p=load_map(rooted(root,args.map),root);header=generate(p);out.mkdir(parents=True)
 (out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(header)
 for name in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/name).write_text(json.dumps(p,indent=2)+'\n')
 print('GPU377_FRESH225_FINALIZED_NOT_BUILT_OR_DEPLOYED')
if __name__=='__main__':main()
