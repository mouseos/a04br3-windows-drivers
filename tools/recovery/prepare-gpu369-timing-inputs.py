#!/usr/bin/env python3
"""GPU369/boot98deのhost入力準備だけ。fresh/媒体/SCM/native/端末を生成しない。"""
import argparse,copy,hashlib,importlib.util,json,re,sys
from pathlib import Path
REPO=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(REPO/'tools/winpe'))
from peripheral_batch_policy import rooted,sha,read,require
spec=importlib.util.spec_from_file_location('timing_existing_profile',Path(__file__).with_name('prepare-peripheral-followup202.py'))
base202=importlib.util.module_from_spec(spec);spec.loader.exec_module(base202)
base=base202.m.base
MANIFEST='cdfb0064c25f49dceac669aaba0b7050c2d82f8d10b43c417f8ce6d2180cd08e'
BOOT='98de46d30349379669c4a7300d54d7652b40883f01febb10c5b65992a987e322'
OLD_BOOT='0943ab8a6008942d9701140542da6ecaa902f750bf34b864bb6a66808c08df42'
FD='c731f8d68ed39c7a812b427d181468123c58b040fe07fbc666bd8b23e6f0b326'
SIGN_SOURCE='2ffc193078da9badbfff67cba2868f34a47b99f8'
COMPILE_SOURCE='15c64bf3a77bd0d27a49db55f80dc0a12f15d3f5'
NATIVE_SOURCE='a04e22489709db79d7c5a4355419bc7aaf95b057'

def prepare(root,inputs,out):
    pins={}
    def pin(ref,json_file=True):
        require(set(ref)=={'path','sha256'} and re.fullmatch('[0-9a-f]{64}',ref['sha256']),'固定path/SHAが必要')
        path=rooted(root,ref['path']);require(sha(path)==ref['sha256'],'入力SHA不一致: '+ref['path'])
        pins[path.relative_to(root).as_posix()]=ref['sha256']
        return read(path) if json_file else path
    require(inputs['schema']=='GPU369_TIMING_INPUTS_PREPARATION_V1','準備専用schema')
    require(inputs['fresh'] is None and inputs['native'] is None and inputs['capture_contract'] is None and inputs['new_service'] is None,'未取得fresh/new serviceを合成しない')
    require(inputs['main_execution_approved'] is False,'この入口は承認/実行を扱わない')
    prior_doc=pin(inputs['template_media_manifest']);prior=prior_doc['policy']
    require(prior_doc['profile']==prior['profile']=='combined-features-fresh202-gpu368-v1' and prior['install_targets']==['render'],'既202はtemplateのみ')
    require(prior['boot_transition']['boot_sha256']==OLD_BOOT and prior['boot_transition']['boot_write_required'] is False,'既203 mediaの保持boot')
    gm=pin(inputs['signed_manifest']);review=pin(inputs['main_package_boot_review']);ind=pin(inputs['independent_review'])
    require(inputs['signed_manifest']['sha256']==MANIFEST and gm['schema']=='panel-timing369-sign-only-v1' and gm['success'] is True and gm['signed'] is True and gm['hardware_verified'] is False,'GPU369実署名manifest')
    require(gm['recipe_source_commit']==SIGN_SOURCE and gm['new_display_source_commit']==COMPILE_SOURCE and gm['native_owner_source_commit']==gm['umd_source_commit']==NATIVE_SOURCE,'実compile/native/UMD出自を保持')
    require(review['status']=='main-hashes-verified-not-hardware' and review['signed_manifest_sha256']==MANIFEST and review['boot_sha256']==BOOT and review['hardware_verified'] is False and review['device_operations']==0,'main実package/boot review')
    hashes={x['name']:x['sha256'] for x in gm['outputs']};require(len(hashes)==15 and set(review['package_members'])==set(hashes) and all(v is True for v in review['package_members'].values()),'実15成果物review')
    require(ind['success'] is True and ind['signed_manifest_sha256']==MANIFEST and ind['native_UMD_PCO_SDK_rebuild']==0 and ind['hardware_verified'] is False and ind['outputs']==hashes,'独立reviewと全15SHA')
    package_dir=Path(inputs['signed_manifest']['path']).parent
    for name,h in hashes.items():pin(dict(path=(package_dir/name).as_posix(),sha256=h),False)
    boot=pin(inputs['boot_manifest']);composition=pin(inputs['firmware_composition']);pin(inputs['boot_image'],False);pin(inputs['FD'],False)
    require(inputs['boot_image']['sha256']==boot['boot_img_sha256']==review['boot_sha256']==BOOT,'実新boot98de')
    require(inputs['FD']['sha256']==boot['fd_sha256']==composition['fd_sha256']==FD,'実FDの結合')
    require(boot['composition_manifest_sha256']==inputs['firmware_composition']['sha256']==boot['firmware_manifest_sha256'],'実boot/firmware manifest結合')
    require(boot['firmware_sha256']==composition['shimmed_fd_sha256'] and boot['template_boot_sha256']==OLD_BOOT,'既shim/boot0943出自を保持')
    require(boot['uefi_source_commit']==composition['source_commit']=='a82e2713b95e4d272a22683851f876c170dcb3da' and boot['firmware_build_defines']['A04BR3_PANEL_TIMING_HANDOFF']=='1','DTM0 firmware source/flag')
    require(composition['mu_uncommitted_diff_sha256']==hashlib.sha256(b'').hexdigest(),'clean Mu manifest')
    require(out.is_relative_to(root) and not out.exists(),'新規root内outputのみ')
    # Immutable view for the existing strict base.package validator. This is
    # packaging provenance, not a claim that retained UMD/native were rebuilt.
    view=dict(schema='GPU369_SIGNED_MANIFEST_ADAPTER_V1',package_source_commit=SIGN_SOURCE,source_commit=SIGN_SOURCE,manifest_adapter_only=True,original_signed_manifest=inputs['signed_manifest'],new_display_compiled_source_commit=COMPILE_SOURCE,native_owner_source_commit=NATIVE_SOURCE,umd_source_commit=NATIVE_SOURCE,outputs=gm['outputs'],signed_payloads_modified=False)
    view_bytes=(json.dumps(view,indent=2)+'\n').encode();view_ref=dict(path=(out.relative_to(root)/'candidate-package-view.json').as_posix(),sha256=hashlib.sha256(view_bytes).hexdigest())
    candidate_rows={}
    for role in ('display','render'):
        row=copy.deepcopy(prior['packages'][role]);row.update(directory=(package_dir/('display-transport' if role=='display' else '')).as_posix(),manifest=view_ref['path'],manifest_sha256=view_ref['sha256'],source_commit=SIGN_SOURCE,driver_version='0.0.3.69',driver_date='11/10/2026')
        for member in row.get('extra_members',[]):
            member.pop('baseline_sha256',None);member['sha256']=hashes[member['name']]
        row.pop('hashes',None);row.pop('version_u64',None);row.pop('date_filetime',None)
        candidate_rows[role]=row
    # Keep old capture/services/counts out of the candidate policy: there is
    # deliberately no executable policy/header/native map produced here.
    delta=dict(schema='GPU369_TIMING_PROFILE_DELTA_PREPARED_V1',template_profile=prior['profile'],install_targets=['display','render'],install_mask=1056,DIF_order=['display','render'],DIF_contract=prior['display_transition']['DIF_contract'],live_devnode_restart=False,planned_reboots_after_install=1,matched_boot_required=True,boot_transition=dict(mode='matched-panel-timing-DTM0-v1',boot_path=inputs['boot_image']['path'],boot_sha256=BOOT,baseline_boot_sha256=None,boot_write_required=True,gpt_written=False,mixed_old_new_allowed=False,next_windows_boot_blocked_until_matched_boot=True),diagnostic_clients=copy.deepcopy(prior['diagnostic_clients']),retained_console_pair='launcher363/present356',globaldiag_same_postinstall_boot=True,globaldiag_maximum_invocations=1,canonical_and_runtime_policy_tokens_distinct=True,UMD_FW_clients_rebuild=False,all_other_roles='preserve from latest actual fresh inventory, including current WiFi/USB/audio',actual_service_count=None,actual_named_value_count=None,new_service=None,pending_fresh_manifest=None,pending_native_receipt=None,normal_install_allowed=False,media_generation_allowed=False,hardware_verified=False,main_approval_required=True)
    pending=dict(schema='GPU369_TIMING_PENDING_INPUTS_V1',status='source-and-inputs-prepared-not-finalized',fresh=None,native=None,capture_contract=None,supplement=None,mutable_baseline=None,actual_package_rows=None,boot_witness=None,new_service=None,main_execution_approved=False,candidate_rows=candidate_rows,candidate_signed_manifest=inputs['signed_manifest'],candidate_manifest_view=view_ref,matched_boot_manifest=inputs['boot_manifest'],matched_boot_image=inputs['boot_image'],baseline_boot_sha256=None,draft=True,normal_install_allowed=False,media_generation_allowed=False)
    # The helper accepts a new absolute prefix; do not carry old service202 argv.
    delta['diagnostic_clients'][1]['arguments']=None
    delta['diagnostic_clients'][1]['arguments_template']='C:\\Windows\\System32\\${new_service}\\logs\\global-diag369'
    out.mkdir(parents=True)
    try:
        (out/'candidate-package-view.json').write_bytes(view_bytes)
        for role,row in candidate_rows.items():base.package(root,row,role)
    except BaseException:
        # Preserve the exact rejected metadata and exception for review.
        (out/'preparation-incomplete.json').write_text(json.dumps(dict(validated=False,media_generation_allowed=False),indent=2)+'\n');raise
    for name,obj in [('profile-delta.json',delta),('pending-map.json',pending),('input-lock.json',dict(schema='GPU369_TIMING_INPUT_LOCK_V1',validated=True,original_input_pins=[dict(path=k,sha256=v) for k,v in sorted(pins.items())],profile_finalized=False,device_operations=0,VM_operations=0))]:
        (out/name).write_text(json.dumps(obj,indent=2)+'\n')
    return dict(prepared=True,profile_finalized=False,media_generated=False,VM_operations=0,device_operations=0,output=out.relative_to(root).as_posix(),candidate_rows=list(candidate_rows),pending=['latest fresh/native/capture-contract/supplement/package rows/log baseline/boot witness/new service','main approval'])

def main():
    p=argparse.ArgumentParser();p.add_argument('--prepare-only',action='store_true');p.add_argument('--workspace',type=Path,required=True);p.add_argument('--inputs',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    require(a.prepare_only,'準備だけを明示すること');root=a.workspace.resolve(strict=True)
    output=(a.out if a.out.is_absolute() else root/a.out).resolve()
    print(json.dumps(prepare(root,read(rooted(root,a.inputs)),output),ensure_ascii=False))
if __name__=='__main__':main()
