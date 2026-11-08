#!/usr/bin/env python3
"""fresh152/151実結果待ちのGPU342契約模板。媒体・native armを作らない。"""
import argparse,hashlib,json
from pathlib import Path

PACKAGE_SOURCE='e395c3a6af64c82fdc56eec07a74592e5aa51504'
RUNTIME_SOURCE='59ded879bab1c4c54abac73bd2b673adada33d4c'
PACKAGE=Path('port/xnu/out/windows-rt-tq-clear342/build/package342')

def require(value,message):
 if not value:raise ValueError(message)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def rooted(root,relative):
 p=(root/relative).resolve(strict=True);require(p.is_relative_to(root),'Root-local existing input required');return p

def inspect_candidate(root):
 manifest=root/PACKAGE/'signed-manifest.json'
 if not manifest.exists():return {'status':'awaiting-final-signed-package','manifest':None,'main_seal':None,'files':None}
 p=rooted(root,PACKAGE/'signed-manifest.json');m=read(p);files=[]
 for entry in m.get('outputs',[])+m.get('retained_provenance',[]):
  name=entry['name'];require(Path(name).name==name,'Fixed package leaf required')
  f=rooted(root,PACKAGE/name);require(sha(f)==entry['sha256'],'Candidate member byte mismatch')
  files.append({'name':name,'sha256':entry['sha256'],'bytes':f.stat().st_size})
 require(files,'Completed manifest must list real output bytes')
 return {'status':'member-bytes-verified-not-adopted-not-signature-verification',
         'manifest':{'path':(PACKAGE/'signed-manifest.json').as_posix(),'sha256':sha(p)},
         'main_seal':None,'files':files,
         'reported_provenance':{k:m.get(k) for k in ('schema','package_source_commit','kernel_binary_source_commit','native_owner_source_commit','inf_driverver','build_manifest_file','build_manifest_sha256','hardware_verified','root_reset_fix_proven')},
         'build_manifest':None if not m.get('build_manifest_file') else {'path':(PACKAGE/m['build_manifest_file']).as_posix(),'sha256':sha(rooted(root,PACKAGE/m['build_manifest_file']))}}

def make_draft(root):
 prior=Path('port/xnu/out/windows-rt-combined151-fresh150/main-deployment-contract.json');p=rooted(root,prior);c=read(p)
 require(c['transaction']=='combined-run151' and c['gpu_client_execute'] is False,'Exact frozen151 reference required')
 return {'schema':1,'profile':'combined-features-fresh152-DRAFT','transaction':'combined-run153',
 'status':'awaiting-151-result-fresh152-native-actual-final342-seal-and-main-scope',
 'draft':True,'normal_install_allowed':False,'arm_allowed':False,'media_generation_allowed':False,
 'hardware_owner':'main','hardware_operations_allowed':False,
 'source_basis':'frozen151 source; not a claim of installed152 actual',
 'prior151_reference_only':{'contract':{'path':prior.as_posix(),'sha256':sha(p)},'source_commit':c['source_commit'],'fresh_capture_number':150,'actual_state_must_not_be_inferred_from_planned_tuple':True},
 'capture_selection':{'required_capture_number':152,'fresh':None,'recovery_boot_id':None,'same_recovery_unchanged_proof':None,'native_actual':None,'actual_selected_versions':None,'original_files':None},
 'prior151_result':None,'approved_scope':None,
 'service_template':{'service':'MT8167CombinedFollowup152','data_directory':'MT8167CombinedFollowup152','profile':'combined-features-fresh152-v1','targets':12,'render_target':{'role':'render','hardware_id':'ACPI\\MOU0006','sys':'ge8300-render.sys','index':10},'native_arm':None,'signed_service':None},
 'candidate_GPU342':{'selected':False,'directory':PACKAGE.as_posix(),'requested_package_recipe_source':PACKAGE_SOURCE,'reported_runtime_source':RUNTIME_SOURCE,'sign_only_date_fix_must_not_be_described_as_new_runtime_compile':True,**inspect_candidate(root)},
 'execution_template':{'armed_GPU_client_execute':False,'armed_GPU_client_count':0,'planned_GPU_client_count_after_approval':1,'GPU_client_must_be_last':True,'GPU_phase':1,'client':None,'non_GPU_clients':None,'temperature_retest':None,'audio_retest':None,'WLAN_retest':None,'BT_retest':None,'rotation_retest':False,'benchmark_execute':False,'BT_first_install_execute':False,'successful_unchanged_functions_should_not_be_repeated':True,'planned_reboots':1,'timeouts':{'install_seconds':None,'boot_wait_seconds':None,'service_phase_seconds':None,'GPU_client_seconds':None,'deadline_start_events':None}},
 'runtime_opt_in_template':{'A04_GPU_PHASE1_ENABLED':1,'A04_RUN_THERMAL_READ':None,'A04_RUN_WLAN_READ':None,'A04_RUN_BT_READ':None,'values_require_final_main_scope':True,'legacy_and_frozen151_defaults_unchanged':True},
 'mutable_diagnostics':{'fresh152_baseline':None,'count':None,'source_correlation':None,'live_log_full_equality_is_install_gate':False,'archived_prefixes_hash_pinned':True,'live_overwrite_or_delete':False,'comparison':'saved exact prefix/append/truncation/replacement/missing observation only','tool':'tools/recovery/compare-mutable-diagnostic-logs.py'},
 'preservation_template':{'boot_required_sha256':'1fef20aa28be71ae0bd826ec91619df3b492abd23631632d5b0d679e38ca560a','boot_write_required':False,'gpt_written':False,'current_UEFI8range_retained':True,'service_fingerprints':None,'named_values':None,'prior_combined150_stopped_values':None,'failed149_service148_absence_requires_actual_evidence':True,'BT_existing_ROOT_retained':True,'driver_versions_and_SYS_require_actual152':True,'SystemRequired_existing_implementation_retained':True},
 'post_capture_number':154,
 'required_adoption_checks':['main confirms151 result and actual152 source; never relabel150 as152',
 'fresh152 manifest, all actual files, RecoveryID, clean native observations, stopped previous service and any hidden BT ROOT joined',
 '342 completed manifest/member hashes, UTC INF/CAT date consistency, BCBF signatures and CAT membership independently sealed',
 'package recipe/date source and actual linked runtime/object source identified separately; exactly8 compiled TUs is build evidence, not hardware success',
 'latest mutable diagnostic snapshot archived/prefix-compared separately from immutable installation inputs',
 'formal main-selected client artifact/arguments/timeout/phase fixed; one final GPU client only',
 'skip unchanged successful function tests only after151 outcome; no guessed success or blanket reuse of planned1116/1.10/110',
 'final152 validator/native preparation/postreader and service/header are finalized together before any arm'],
 'limitations':['4 Clear/Copy factory cycles and64 Copy regression are owner-reported mock evidence; no real342 result yet',
 '147 first Clear/Copy/Map/Unmap HRESULT0/PASS1 then second Clear did not return; do not relabel as Unmap failure',
 'this draft produces no native hive, signed service or media']}

def reject_arm(draft):
 require(draft.get('draft') is True and draft.get('normal_install_allowed') is False and draft.get('arm_allowed') is False and draft.get('media_generation_allowed') is False,'Draft state altered')
 raise ValueError('fresh152/151 actual/signed342/main scope未結合:153模板はarm・媒体生成不可')

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--workspace',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();root=a.workspace.resolve(strict=True);out=a.out.resolve();require(out.is_relative_to(root) and not out.exists(),'New root-local draft output required')
 d=make_draft(root);out.mkdir(parents=True);(out/'contract-DRAFT.json').write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n');print(json.dumps({'status':d['status'],'arm':False,'media':False,'GPU342':d['candidate_GPU342']['status']},ensure_ascii=False))
if __name__=='__main__':main()
