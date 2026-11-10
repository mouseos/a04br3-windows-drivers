#!/usr/bin/env python3
"""既200/369 generatorの既知371/render-only variant。実fresh212入力だけ。"""
import argparse,copy,importlib.util,json,re
from pathlib import Path
sp=importlib.util.spec_from_file_location('vidpn_shared369',Path(__file__).with_name('prepare-gpu369-timing-inputs.py'))
m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m)
require,sha,read,rooted=m.require,m.sha,m.read,m.rooted
PROFILE='combined-features-fresh212-gpu371-output-v1';SERVICE=DIRECTORY='MT8167CombinedFollowup212'
RECIPE='9d941f9873c1e7d8c166aa04e743a31eb99218dd';RUNTIME='b63827d7c45e099d5844e2c694804a1df047ed10';COMPILED='91f14b4c6c04e43e947822cc2b8fef9dc4be8082'
BASE_MANIFEST='cdfb0064c25f49dceac669aaba0b7050c2d82f8d10b43c417f8ce6d2180cd08e'
BDD_RECEIPT='bed4f463cf75bfda21a4c999f542c40094dcb678c850ab90a5c31c180fe5adc8'
MANIFEST='649612e1b5facd4b0992fea6b2c7c2a37cd2ea47d0570bcbf97b28c871b8655b'
REVIEW='140608b346efcdaba6be0428610f3f8628a647cf26f76bd7fb8ac1f11977e1bc'
RENDER_SYS='202f0f3fd7d4325df20dd8570d88299f008fae421bff16cccf99589282a19ded'
LINK_LOCK='922294e3a8d5c67f22eec295335a566ef3294650a09378f1e9900cf12dc0543c'
BASE_RENDER_SYS='ce0c8aeb8f1df43b74aa9716e9f43e4dee00be449d4ea776850d82be81970741'

def validate_manifest(g):
 require(g['schema']=='GE8300_VIDPN371_RELINK_SIGN_V1' and g['recipe_source_commit']==RECIPE and g['runtime_source_commit']==RUNTIME and g['compiled_source_commit']==COMPILED,'既知371の実source出自必須')
 require(g['success'] is True and g['signed'] is True and g['signer_thumbprint']=='BCBF6C456C6F0DD0818434763B189488FBF13570','実371署名成功のみ')
 require(g['driver_version']=='0.0.3.71' and g['retained_transport_version']=='0.0.3.69' and g['baseline_package_manifest_sha256']==BASE_MANIFEST and g['BDD_compile_receipt_sha256']==BDD_RECEIPT,'実369+承認BDD object由来のみ')
 require(g['compile_count']==0 and g['link_count']==1 and all(g[k] is False for k in ('SDK_rebuilt','PCO_rebuilt','UMD_rebuilt')) and g['retained_UMD_FW_clients_transport'] is True and g['kernel_imports_equal_actual369'] is True,'371 relink1/保持contract')
 require(g['kernel_input_lock_sha256']==LINK_LOCK and g['historical_other_four_library_equality_verified'] is False,'新link lock9222とhistorical未確認を保持')
 require(next(x['sha256'] for x in g['outputs'] if x['name']=='ge8300-render.sys')==RENDER_SYS,'実371 SYS202fのみ')
 require(g['hardware_verified'] is False and g['deployment_performed'] is False,'署名receiptを実機成功へ読み替えない')

def validate_boot_retention(boot,fresh,bootid,pin,pin_file):
 require(boot['schema']=='GPU369_RETAINED_BOOT98DE_CHAIN_V1' and boot['kind']=='retained-chain' and boot['source_manifest_sha256']==fresh and boot['recovery_boot_id']==bootid and boot['read_only'] is True and boot['physical_read_this_capture'] is False and boot['physical_boot_read_operations']==0,'実212の既98de chain schemaのみ。新witnessへ改名しない')
 require(boot['boot_sha256']==m.BOOT and boot['no_intervening_boot_or_gpt_write_reported'] is True and boot['hardware_success_inferred'] is False,'実209後保持報告とscope')
 report=boot['no_additional_write_report'];require(report['boot_written_after_run209'] is False and report['gpt_written_after_run209'] is False and report['continuous_independent_monitoring_performed'] is False,'main申告を連続独立監視に置換しない')
 seal=pin(boot['last_write_seal']);host_seal=pin(boot['matched_host_seal']);result=pin(boot['last_write_result']);manifest=pin(boot['firmware_composition_manifest']);pin_file(boot['host_artifact'])
 require(seal['schema']=='GPU369_RUN209_MAIN_SEAL_V1' and seal['status']=='validated-main-seal' and seal['transaction']=='combined-run209' and seal['boot_write_required'] is True and seal['gpt_written'] is False,'実209 last-write seal必須')
 require(result['transaction']=='combined-run209' and result['seal_sha256']==boot['last_write_seal']['sha256'] and result['boot_written'] is True and result['gpt_written'] is False and result['matched_boot_applied_before_normal'] is True,'209実write結果必須。計画だけでは不可')
 require(result['boot_sha256']==seal['matched_boot_sha256']==host_seal['matched_boot_sha256']==boot['host_artifact']['sha256']==manifest['boot_img_sha256']==m.BOOT and seal['matched_boot']==host_seal['matched_boot']==boot['host_artifact']['path'],'209実writeとhost boot一致')
 require(manifest['fd_sha256']==m.FD and manifest['uefi_source_commit']=='a82e2713b95e4d272a22683851f876c170dcb3da' and manifest['firmware_build_defines']['A04BR3_PANEL_TIMING_HANDOFF']=='1','実98de/FD/DTM0出自')
 require(any(x['path']==boot['host_artifact']['path'] and x['sha256']==m.BOOT for x in seal['pinned_files']),'209 sealの実host pin')
 collection=pin(boot['run209_collection_result'])
 require(boot['seal_boot_written_is_pre_execution_state'] is True and boot['collection_boot_written_is_cumulative_run209_state'] is True and collection['collection_only'] is True and collection['transaction']=='combined-run209' and collection['seal_sha256']==result['seal_sha256'] and collection['boot_written'] is True and collection['boot_sha256']==m.BOOT and collection['gpt_written'] is False,'209collection累積状態を追加writeと誤認しない')
 captures={}
 for ref in boot['readonly_capture_refs']:
  cap=pin(ref);require(cap['status']=='captured-unmounted' and cap['read_only'] is True and cap['unmounted'] is True and cap['errors']==[],'実read-only captureだけ');captures[ref['sha256']]=cap
 require(fresh in captures and captures[fresh]['recovery_boot_id']==bootid and collection['capture_manifest_sha256'] in captures,'209collection/実212captureの保持chain結合')
 return dict(kind='retained-chain-209',physical_read_this_capture=False,boot_sha256=m.BOOT,last_write_controller_verified=True,no_intervening_write_reported_by_main=True,continuous_independent_monitoring_performed=False,hardware_success_inferred=False,evidence_scope=boot['evidence_scope'])

class Output371(m.TimingInputs):
 contract_family='GPU369' # Actual212 capture producer schema is retained verbatim.
 @property
 def install_targets(self):return ['render']
 def validate_manifest(self,gm,review,spec):
  validate_manifest(gm)
  require(review['success'] is True and review['signed_manifest_sha256']==MANIFEST and review['runtime_source_commit']==RUNTIME and review['recipe_source_commit']==RECIPE and review['historical_other_four_library_equality_verified'] is False and review['newly_locked_WDK81_library_inputs']==LINK_LOCK and review['hardware_verified'] is False and review['device_operations']==0,'実main371 reviewと出自/未確認の保持')
 def validate_output_review(self,review,hashes):require(review['output_hashes_verified']==len(hashes)==15 and hashes['ge8300-render.sys']==RENDER_SYS,'実main review15出力/371 SYS一致')
 def package_rows(self,rows,pin):
  require(not self.spec.get('retained_package_overrides') and not self.spec.get('wifi_activation',False) and not self.spec.get('wifi_update_package'),'371はrender-only、任意override不可')
  return copy.deepcopy(rows)
 def adopt(self,role,old,originals,hashes):
  require(role=='render' and old['driver_version']=='0.0.3.69' and old['hashes']['sys']==BASE_RENDER_SYS and old['source_commit']=='2ffc193078da9badbfff67cba2868f34a47b99f8','実選択render369/ce0から371だけ')
  row=copy.deepcopy(old);ref=self.spec['GPU371_manifest'];row.update(directory=Path(ref['path']).parent.as_posix(),manifest=ref['path'],manifest_sha256=ref['sha256'],source_commit=RECIPE,driver_version='0.0.3.71',driver_date='11/10/2026')
  for x in row.get('extra_members',[]):
   name='Windows/System32/'+x['installed'].replace('\\','/');require(originals[name]['sha256']==hashes[x['name']]==x['sha256'],'UMD/FWは実freshと371 package同一保持');x['baseline_sha256']=originals[name]['sha256']
  return row
 def validate_transport(self,packages,hashes):
  display=packages['display'];wifi=packages['wifi']
  require(display['driver_version']=='0.0.3.69' and display['source_commit']=='2ffc193078da9badbfff67cba2868f34a47b99f8','display369を実選択/実byte保持')
  for kind,n in display['names'].items():require(display['hashes'][kind]==hashes['display-transport/'+n],'transport369保持')
  require(wifi['driver_version']=='2026.11.11.1029' and wifi['source_commit']==m.WIFI_SOURCE and wifi['manifest_sha256']==m.WIFI_MANIFEST and wifi['hashes']['sys']=='70253172537b5600e23a7af761ebced1155db4ae2515d2701ba2665d70b231bc','実選択WiFi1029を保持')
 def boot(self,boot,transition,fresh,bootid,pin,pin_file):
  evidence=validate_boot_retention(boot,fresh,bootid,pin,pin_file)
  transition.update(mode='retained-panel-timing-DTM0-209-v1',boot_path=boot['host_artifact']['path'],boot_sha256=m.BOOT,baseline_boot_sha256=m.BOOT,boot_write_required=False,gpt_written=False,mixed_old_new_allowed=False,next_windows_boot_blocked_until_matched_boot=False,firmware_manifest=boot['firmware_composition_manifest'])
  return evidence
 def finish(self,p,spec,n,pin,pin_file):
  require(n['full_service_capture_complete'] is True and n['registry_write_requested'] is False and n['capture_number']==212 and n['new_service_name']==SERVICE and n['new_service']['present'] is False,'実fresh212 inventoryと新service不在')
  p['GPU371_manifest']=p.pop('GPU368_manifest');p['GPU371_handoff']=p.pop('GPU368_handoff')
  p.pop('gpu369_timing_contract',None);p.pop('wifi_activation',None);p.pop('wifi_update_package',None);p.pop('candidate_manifest_view',None);p.pop('retained_package_overrides',None)
  p['display_transition'].update(DIF_order=['render'],old_DOD_retirement='display369 retained; render371 only; actual209 boot98de retained')
  p['diagnostic_clients'][1]['arguments']=['"C:\\Windows\\System32\\'+SERVICE+'\\logs\\global-diag371"'];p['globaldiag_contract']['prefix']=p['diagnostic_clients'][1]['arguments'][0]
  p['main_execution_approved']=self.spec['main_execution_approved']
  p['gpu371_output_contract']=dict(schema=1,run=213,fresh=212,post_capture=214,install_mask=1024,runtime_source_commit=RUNTIME,compiled_BDD_source_commit=COMPILED,signing_recipe_commit=RECIPE,retained_boot_sha256=m.BOOT,retained_display_version='0.0.3.69',retained_wifi_version='2026.11.11.1029',terminal_windows_hold=True,kernel_input_lock_sha256=LINK_LOCK,historical_other_four_library_equality_verified=False)

def configure(digest):
 require(digest==MANIFEST,'固定実署名371 manifestのみ')
 m.configure_capture(212);s=m.shared;s.PROFILE=s.base.PROFILE=PROFILE;s.SERVICE=s.DIRECTORY=s.base.SERVICE=s.base.DIRECTORY=SERVICE;s.INSTALL=s.base.INSTALL=['render'];s.SOURCE=RECIPE;s.MANIFEST=digest
 return s

def load_map(path,root):
 spec=read(path);require(spec['schema']=='GPU371_OUTPUT_FINALIZE_INPUTS_V1' and spec['capture_number']==212 and spec['profile']==PROFILE and spec['install_targets']==['render'] and spec['hardware_owner']=='main','既知213/fresh212/render371だけ')
 require(spec['main_execution_approved'] is True,'main承認未確定。自動付与しない')
 for name in ('fresh','native','capture_contract','template_policy','actual_package_rows','GPU371_manifest','GPU371_handoff','boot_witness','mutable_baseline','globaldiag_manifest','globaldiag_import_proof'):
  ref=spec.get(name);require(isinstance(ref,dict) and re.fullmatch('[0-9a-f]{64}',ref.get('sha256','')) and ref['sha256']!='0'*64,'未取得/未確定入力: '+name);require(sha(rooted(root,ref['path']))==ref['sha256'],'実入力SHA: '+name)
 require(spec['GPU371_manifest']['sha256']==MANIFEST and spec['GPU371_handoff']['sha256']==REVIEW,'実371 manifest/main review pinのみ')
 configure(spec['GPU371_manifest']['sha256']);current=copy.deepcopy(spec);current['GPU368_manifest']=spec['GPU371_manifest'];current['GPU368_handoff']=spec['GPU371_handoff']
 m.shared.TIMING=Output371(root,spec,{})
 return m.shared.build_policy(root,current)

def generate(policy):
 require(policy['profile']==PROFILE and policy['install_targets']==['render'] and policy['capture_selection']['capture_number']==212,'371固定profileのみ')
 configure(policy['GPU371_manifest']['sha256']);return m.shared.generate(policy)
def paths():return m.shared.paths()
def assert_paths(image,capture=False):return m.shared.assert_paths(image,capture)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--map',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--finalize',action='store_true');a=ap.parse_args();require(a.finalize,'実fresh212/371 pin確定後だけ')
 root=a.workspace.resolve(strict=True);out=a.out.resolve();require(out.is_relative_to(root) and not out.exists(),'root内新規出力だけ');p=load_map(rooted(root,a.map),root);out.mkdir(parents=True)
 (out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(generate(p))
 for n in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/n).write_text(json.dumps(p,indent=2)+'\n')
 print('GPU371_FRESH212_FINALIZED_NOT_BUILT_OR_DEPLOYED')
if __name__=='__main__':main()
