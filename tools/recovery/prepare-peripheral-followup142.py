#!/usr/bin/env python3
"""fresh142向け次GPU試行の非arm雛形。未確定capture/署名物を代用しない。"""
import argparse,json
from pathlib import Path

PROFILE='combined-gpu-final-fresh142-draft-v1'
SERVICE='MT8167CombinedFollowup142'
BOOT_SHA256='1fef20aa28be71ae0bd826ec91619df3b492abd23631632d5b0d679e38ca560a'
PARENT_SHA256='a4d86dfcdaa6243619a7d4ca413995e3505b559afb3b9e9bf2a5269ef054046b'
AUDIO_SHA256='1408e60886f6b7ed8a025a288c08b4c6f3282d8864e27e77e4ccb94d43688cf0'

def draft_contract():
 return {
  'schema':1,'profile':PROFILE,'service':SERVICE,'data_directory':SERVICE,
  'draft':True,'normal_install_allowed':False,'execution_enabled':False,
  'hardware_owner':'main','hardware_operations_allowed':False,'device_verified':False,
  'fresh':{'capture_number':142,'manifest_path':None,'manifest_sha256':None,'recovery_boot_id':None,'native_baseline':None},
  'gpu_candidate':{'producer':'final Huygens/Mencius integrated signed candidate','source_commit':None,'manifest_path':None,'manifest_sha256':None,'main_independent_seal':None,'signed_package':None,'signed_client':None},
  'prior_run141_result':None,
  'planned_reboots':1,'install_targets':['render'],
  'gpu_client_execute':False,'gpu_client_count':0,'diagnostic_clients':[],
  'planned_diagnostics':{'gpu_count':1,'gpu_phase':1,'gpu_last':True,'rerun_old335_client':False,'peripheral_phase':0,'peripheral_clients':[],'peripheral_selection':'only justified by run141 per-feature result; currently pending'},
  'retained_expected':{
   'parent':{'version':'2026.11.8.1115','sys_sha256':PARENT_SHA256,'thermal_interface_version':3,'thermal_parent_build':1104,'headphone_interface_version':2,'headphone_parent_build':1107},
   'audio':{'version':'2026.11.8.1033','sys_sha256':AUDIO_SHA256},
   'matched_boot':{'sha256':BOOT_SHA256,'write_required':None,'state':'confirm from fresh142/main proof; do not infer current boot from candidate pins'},
   'hcd1014':'retain exact signed package and actual run141 adoption state',
   'static_config_old_SCM_GPU_ledger':'preserve from new actual native baseline; no historical hive substitution'},
  'baseline_gpu_logs':{'path':None,'sha256':None,'count':None,'source':'fresh142 actual logs; preserve late/same-PID append history'},
  'build_policy':{'source_TUs':'only changed SCM policy/service and final producer-required units','retained_driver_rebuild':False,'SDK_WDK_PCO_setup':False,'large_input_retransfer':False,'reuse_rule':'hash-and-size join host retained closure; transfer only required delta'},
  'blockers':['fresh142 not captured/verified','run141 per-feature result pending','final signed integrated GPU and client pins pending','main independent adoption seal pending'],
  'warnings':['render DIF only; never reinstall parent1115/audio1033','one planned restart; exactly one final GPU diagnostic last','no old run139 retry; run141 frozen artifacts remain unchanged','not an executable policy; no service/header/native hive/media output']
 }

def validate_draft(p):
 if p.get('schema')!=1 or p.get('profile')!=PROFILE or p.get('service')!=SERVICE or p.get('data_directory')!=SERVICE:raise ValueError('Explicit fresh142 draft identity required')
 if p.get('draft') is not True or p.get('normal_install_allowed') is not False or p.get('execution_enabled') is not False:raise ValueError('Draft cannot enable installation or execution')
 if p.get('gpu_client_execute') is not False or p.get('gpu_client_count')!=0 or p.get('diagnostic_clients')!=[]:raise ValueError('Pending clients must not become runnable')
 if p.get('install_targets')!=['render'] or p.get('planned_reboots')!=1:raise ValueError('Only render DIF and one planned restart')
 plan=p['planned_diagnostics']
 if plan['gpu_count']!=1 or plan['gpu_phase']!=1 or plan['gpu_last'] is not True or plan['rerun_old335_client'] is not False or plan['peripheral_phase']!=0 or plan['peripheral_clients']!=[]:raise ValueError('One final GPU client last; peripheral selection pending')
 if p['fresh']['capture_number']!=142 or any(p['fresh'][k] is not None for k in ['manifest_path','manifest_sha256','recovery_boot_id','native_baseline']):raise ValueError('No assumed fresh142 capture')
 if p['retained_expected']!=draft_contract()['retained_expected']:raise ValueError('Retained expected tuple must remain exact, pending actual fresh142 verification')
 if any(p['gpu_candidate'][k] is not None for k in ['source_commit','manifest_path','manifest_sha256','main_independent_seal','signed_package','signed_client']):raise ValueError('Unsigned or unfinished GPU pins cannot be adopted by this scaffold')
 if p['blockers']!=draft_contract()['blockers']:raise ValueError('Unresolved blockers cannot be erased')
 return p

def reject_arm(*args,**kwargs):
 raise ValueError('fresh142 draft is non-armable: implement reviewed final profile only after actual capture, signed GPU/client and main seal arrive')

# Fail closed even if an existing media builder imports this module as a final profile.
load_map=generate=assert_paths=reject_arm

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--finalize',action='store_true');a=ap.parse_args()
 if a.finalize:reject_arm()
 root=a.workspace.resolve(strict=True);out=a.out.resolve();source=Path(__file__).resolve().parents[2]
 if not out.is_relative_to(root) or out==root or out.is_relative_to(source) or out.exists():raise ValueError('New workspace-local artifact directory outside source checkout required')
 p=validate_draft(draft_contract());out.mkdir(mode=0o700)
 for name,data in [('package-map-draft.json',p),('validation.json',{'profile':PROFILE,'status':'draft-not-armable','draft':True,'normal_install_allowed':False,'blockers':p['blockers'],'executable_outputs':[],'device_verified':False})]:
  f=out/name;f.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n');f.chmod(0o600)
 print('FRESH142_TEMPLATE_ONLY_NOT_ARMABLE')
if __name__=='__main__':main()
