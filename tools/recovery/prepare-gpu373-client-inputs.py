#!/usr/bin/env python3
"""実fresh215の371/369保持、署名client pair更新＋任意WiFi1030だけ。"""
import argparse,copy,importlib.util,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import gpu373_client_admission as admission
sp=importlib.util.spec_from_file_location('retained371',Path(__file__).with_name('prepare-gpu371-output-inputs.py'))
m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m)
PROFILE=admission.PROFILE;SERVICE=DIRECTORY=admission.SERVICE
require,sha,read,rooted=m.require,m.sha,m.read,m.rooted
MANIFEST=m.MANIFEST

def configure(wifi=False):
 m.m.configure_capture(215);s=m.m.shared;s.CAPTURE=215;s.PROFILE=s.base.PROFILE=PROFILE;s.SERVICE=s.DIRECTORY=s.base.SERVICE=s.base.DIRECTORY=SERVICE
 s.INSTALL=s.base.INSTALL=['wifi'] if wifi else [];s.SOURCE=m.RECIPE;s.MANIFEST=MANIFEST
 return s

class Clients373(m.Output371):
 contract_family='GPU373'
 def __init__(self,root,spec,transition):
  self.root=root;self.spec=spec;self.transition=transition
 def validate_capture_contract(self,c,pin):
  require(c['schema']=='GPU373_FRESH215_CAPTURE_CONTRACT_V1','実215 producer contractのみ')
 @property
 def install_targets(self):return self.transition['runtime']['install_targets']
 def package_rows(self,rows,pin):
  require(not self.spec.get('retained_package_overrides') and not self.spec.get('wifi_activation',False),'任意override/旧1029 activation不可')
  return copy.deepcopy(rows)
 def adopt(self,role,old,originals,hashes):
  require(role=='wifi' and old['driver_version']=='2026.11.11.1029' and old['hashes']['sys']=='70253172537b5600e23a7af761ebced1155db4ae2515d2701ba2665d70b231bc','実1029から1030のみ')
  return admission.wifi_package(self.root,self.spec['wifi_update'],old)
 def validate_transport(self,packages,hashes):
  require(packages['render']['hashes']['sys']==admission.RENDER371 and packages['render']['manifest_sha256']==MANIFEST and packages['display']['hashes']['sys']==admission.DISPLAY369,'371/369保持')
  if not self.install_targets:require(packages['wifi']['driver_version']=='2026.11.11.1029','client-onlyは1029保持')
 def client(self,prior,originals,origin_sources,preserved,sources,witnesses,**unused):
  c=copy.deepcopy(prior['diagnostic_clients'][0]);pair=self.spec['client_pair']
  directory=Path(pair['launcher']['file']['path']).parent.as_posix()
  # Dependency table has one directory. Both actual signed leaves must be co-located.
  require(Path(pair['evaluator']['file']['path']).parent.as_posix()==directory,'同一正式pair directory必須')
  c.update(directory=directory,sha256=pair['launcher']['file']['sha256'],phase=1,dependencies=[dict(exe=admission.EVALUATOR,sha256=pair['evaluator']['file']['sha256'])])
  for name,row in self.transition['client_replacements'].items():
   witnesses[name]=row['baseline'];preserved[name]=row['candidate']['sha256'];sources[name]=row['candidate']['path']
  return c
 def boot(self,boot,transition,fresh,bootid,pin,pin_file):
  require(boot['schema']=='GPU373_RETAINED_BOOT215_V1' and boot['source_manifest_sha256']==fresh and boot['recovery_boot_id']==bootid and boot['physical_read_this_capture'] is False and boot['boot_sha256']==admission.BOOT,'実215保持chainのみ。physical bootreadを主張しない')
  previous=pin(boot['previous_witness'])
  evidence=m.validate_boot_retention(previous,previous['source_manifest_sha256'],previous['recovery_boot_id'],pin,pin_file)
  seal=pin(boot['run213_seal']);result=pin(boot['run213_result']);collected=pin(boot['run213_collection']);cap214=pin(boot['capture214'])
  require(seal['schema']=='GPU371_RUN213_MAIN_SEAL_V1' and seal['status']=='validated-main-seal' and seal['matched_boot_sha256']==admission.BOOT and seal['boot_write_required'] is False,'実213 retained98de seal')
  for row in (result,collected):require(row['transaction']=='combined-run213' and row['seal_sha256']==boot['run213_seal']['sha256'] and row['boot_written'] is False and row['gpt_written'] is False,'実213 no boot/GPT write result')
  require(collected['collection_only'] is True and collected['capture_manifest_sha256']==boot['capture214']['sha256'] and cap214['status']=='captured-unmounted' and cap214['read_only'] and cap214['unmounted'] and cap214['errors']==[],'実214 collection結合')
  require(boot['no_later_boot_or_gpt_write_reported_by_main'] is True and boot['continuous_independent_monitoring_performed'] is False,'main保持申告を独立監視へ昇格しない')
  require(boot['host_artifact']==previous['host_artifact'] and boot['firmware_composition_manifest']==previous['firmware_composition_manifest'],'既209 boot98de出自保持')
  transition.update(mode='retained-panel-timing-DTM0-209-v1',boot_path=boot['host_artifact']['path'],boot_sha256=admission.BOOT,baseline_boot_sha256=admission.BOOT,boot_write_required=False,gpt_written=False,mixed_old_new_allowed=False,next_windows_boot_blocked_until_matched_boot=False,firmware_manifest=boot['firmware_composition_manifest'])
  return dict(evidence,kind='retained-chain-209-through-213',physical_read_this_capture=False,continuous_independent_monitoring_performed=False)
 def finish(self,p,spec,n,pin,pin_file):
  p['GPU371_manifest']=p.pop('GPU368_manifest');p['GPU371_handoff']=p.pop('GPU368_handoff')
  for key in ('gpu369_timing_contract','gpu371_output_contract','wifi_activation','wifi_update_package','candidate_manifest_view','retained_package_overrides'):p.pop(key,None)
  x=self.transition['runtime']
  for key in ('diagnostic_only','driver_updates','planned_reboots'):p[key]=x[key]
  p['main_execution_approved']=True
  p['display_transition'].update(DIF_order=x['install_targets'],old_DOD_retirement='render371/transport369 retained; client373 pair; optional wifi1030 only')
  p['observer_contract']['planned_reboots']=x['planned_reboots']
  p['diagnostic_clients'][1]['arguments']=['"C:\\Windows\\System32\\'+SERVICE+'\\logs\\global-diag373"'];p['globaldiag_contract']['prefix']=p['diagnostic_clients'][1]['arguments'][0]
  p['gpu373_client_contract']=self.transition
  admission.validate_runtime_policy(p,self.transition)

def load_map(path,root):
 spec=read(path);require(spec['hardware_owner']=='main' and spec['profile']==PROFILE,'実215 main mapのみ')
 transition=admission.admit(root,spec)
 require(spec['GPU371_manifest']['sha256']==MANIFEST and spec['GPU371_handoff']['sha256']==m.REVIEW,'371はbaseline manifest/reviewを保持')
 s=configure(bool(transition['runtime']['install_targets']));s.TIMING=Clients373(root,spec,transition)
 current=copy.deepcopy(spec);current['GPU368_manifest']=spec['GPU371_manifest'];current['GPU368_handoff']=spec['GPU371_handoff']
 return s.build_policy(root,current)

def generate(p):
 admission.validate_runtime_policy(p,p['gpu373_client_contract']);return configure(bool(p['install_targets'])).generate(p)
def paths():return configure().paths()
def assert_paths(image,capture=False):return configure().assert_paths(image,capture)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--map',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--finalize',action='store_true');a=ap.parse_args()
 require(a.finalize,'実署名pair確定後のみ');root=a.workspace.resolve(strict=True);out=a.out.resolve();require(out.is_relative_to(root) and not out.exists(),'root内新規出力のみ')
 p=load_map(rooted(root,a.map),root);header=generate(p);out.mkdir(parents=True)
 (out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(header)
 for name in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/name).write_text(json.dumps(p,indent=2)+'\n')
 print('GPU373_FRESH215_FINALIZED_NOT_BUILT_OR_DEPLOYED')
if __name__=='__main__':main()
