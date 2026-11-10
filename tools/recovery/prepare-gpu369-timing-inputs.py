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
WIFI_SOURCE='c11981553cc10e9730565390f5d9f9e09a919992'
WIFI_MANIFEST='d974239fbcf96d7baad96b84fccaa7462a7481058a7c2d80c0d19d4745ef1599'
WIFI_BASE_MANIFEST='fbe1a4dc4101726d21120212ce371740421d1491c3d8a94e33369aec9a3277a2'
WIFI_BASE_SYS='97dcede955d813a7d72ba8d6e950ceae1215c94351187cf18c73cc15d3f47334'

def wifi_activation(spec):
    value=spec.get('wifi_activation',False)
    require(type(value) is bool,'wifi_activationは明示boolのみ')
    require(value == (spec.get('wifi_update_package') is not None),'wifi_activationとcandidate refは両方指定')
    return value

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
    template_path=out/'template-policy-reference.json'
    template_path.write_text(json.dumps(prior,indent=2)+'\n')
    def ref_file(path):return dict(path=path.relative_to(root).as_posix(),sha256=sha(path))
    future=dict(schema='GPU369_TIMING_FINALIZE_INPUTS_V1',capture_number=None,profile=None,hardware_owner='main',install_targets=['display','render'],fresh=None,native=None,capture_contract=None,supplement=None,mutable_baseline=None,actual_package_rows=None,boot_witness=None,template_policy=ref_file(template_path),prepared_inputs=ref_file(out/'pending-map.json'),prepared_input_lock=ref_file(out/'input-lock.json'),globaldiag_manifest=prior['globaldiag_contract']['helper_manifest'],globaldiag_import_proof=prior['globaldiag_contract']['helper_import_proof'],main_execution_approved=False,retained_package_overrides={})
    (out/'finalize-map.pending.json').write_text(json.dumps(future,indent=2)+'\n')
    return dict(prepared=True,profile_finalized=False,media_generated=False,VM_operations=0,device_operations=0,output=out.relative_to(root).as_posix(),candidate_rows=list(candidate_rows),pending=['latest fresh/native/capture-contract/supplement/package rows/log baseline/boot witness/new service','main approval'])

def main():
    p=argparse.ArgumentParser();p.add_argument('--prepare-only',action='store_true');p.add_argument('--finalize',action='store_true');p.add_argument('--workspace',type=Path,required=True);p.add_argument('--inputs','--map',dest='inputs',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    require(a.prepare_only != a.finalize,'prepare-only/finalizeの一方だけ');root=a.workspace.resolve(strict=True)
    output=(a.out if a.out.is_absolute() else root/a.out).resolve()
    if a.prepare_only:print(json.dumps(prepare(root,read(rooted(root,a.inputs)),output),ensure_ascii=False))
    else:
        policy=load_map(rooted(root,a.inputs),root)
        require(output.is_relative_to(root) and not output.exists(),'新規root内output')
        output.mkdir(parents=True)
        (output/'peripheral-service-path-policy.h').write_text(generate_paths())
        (output/'peripheral-batch-policy.h').write_text(generate(policy))
        for name in ('peripheral-policy.json','peripheral-preservation-plan.json'):(output/name).write_text(json.dumps(policy,indent=2)+'\n')
        print('GPU369_ACTUAL_FRESH_FINALIZED_NOT_BUILT_OR_DEPLOYED')

# Existing200 generator is shared; only candidate/capture-dependent admissions
# differ. No old policy is relabelled and no snapshot is reconstructed.
class TimingInputs:
    contract_family="GPU369"
    def validate_capture_contract(self,contract,pin):pass
    @property
    def install_targets(self):return ["display","render"]+(["wifi"] if self.wifi_activation else [])
    def validate_output_review(self,review,hashes):require(review["outputs"]==hashes,"実369全出力とreviewの一致")
    def __init__(self,root,spec,pending):
        self.root,self.spec,self.pending=root,spec,pending
        self.wifi_activation=wifi_activation(spec);self.wifi_candidate=None
    def validate_manifest(self,gm,review,spec):
        require(spec['GPU368_manifest']['sha256']==MANIFEST and gm['schema']=='panel-timing369-sign-only-v1' and gm['success'] is True and gm['signed'] is True,'実369署名成功pin')
        require(gm['new_display_source_commit']==COMPILE_SOURCE and gm['native_owner_source_commit']==gm['umd_source_commit']==NATIVE_SOURCE,'実混合compile出自保持')
        require(review['success'] is True and review['signed_manifest_sha256']==MANIFEST and review['hardware_verified'] is False,'実独立369 review')
    def package_rows(self,rows,pin):
        # Explicit candidate choice, never a claim of actual selection. The
        # shared generator checks captured SYS + selected version/date next.
        overrides=self.spec.get('retained_package_overrides',{})
        require(set(overrides)<= {'wifi'},'明示retained overrideはWiFiのみ')
        rows=copy.deepcopy(rows)
        if self.wifi_activation:
            require('wifi' not in overrides,'1029をretained baselineへ混ぜずwifi_update_packageを使用')
            row=pin(self.spec['wifi_update_package'])
            require(row['source_commit']==WIFI_SOURCE and row['manifest_sha256']==WIFI_MANIFEST and row['driver_version']=='2026.11.11.1029','固定署名1029 candidate row必須')
            manifest=pin(dict(path=row['manifest'],sha256=row['manifest_sha256']))
            require(manifest['source_commit']==WIFI_SOURCE and manifest['version']=='2026.11.11.1029' and manifest['signature_status']=='Valid' and manifest['signer_thumbprint']=='BCBF6C456C6F0DD0818434763B189488FBF13570','実native1029署名manifest必須')
            self.wifi_candidate=base.package(self.root,row,'wifi')
            # Baseline rows stay actual1027; candidate is adopted after selection checks.
        if 'wifi' in overrides:
            row=pin(overrides['wifi'])
            require(row['source_commit']=='c11981553cc10e9730565390f5d9f9e09a919992' and row['manifest_sha256']=='d974239fbcf96d7baad96b84fccaa7462a7481058a7c2d80c0d19d4745ef1599' and row['driver_version']=='2026.11.11.1029','最終署名1029の明示retained rowのみ')
            rows['wifi']=row
        return rows
    def adopt(self,role,old,originals,hashes):
        if role=='wifi':
            require(self.wifi_activation and self.wifi_candidate is not None,'WiFi candidate未検証')
            require(old['driver_version']=='2026.11.9.1027' and old['manifest_sha256']==WIFI_BASE_MANIFEST and old['hashes']['sys']==WIFI_BASE_SYS,'actual選択1027/SYS97dceのbaselineのみ')
            require(originals['Windows/System32/Drivers/mt6630-combo-runtime.sys']['sha256']==WIFI_BASE_SYS,'actual freshの1027 SYS必須')
            return copy.deepcopy(self.wifi_candidate)
        row=copy.deepcopy(self.pending['candidate_rows'][role])
        if role=='render':
            for member in row.get('extra_members',[]):
                name='Windows/System32/'+member['installed'].replace('\\','/')
                require(name in originals and originals[name]['sha256']==hashes[member['name']],'実freshのUMD/FW保持byte不一致')
                member['baseline_sha256']=originals[name]['sha256']
        return row
    def validate_transport(self,packages,hashes):
        for role in ('display','render'):
            require(packages[role]['driver_version']=='0.0.3.69','両369 package必須')
            for key in ('inf','sys','cat'):
                name=('display-transport/' if role=='display' else '')+packages[role]['names'][key]
                require(packages[role]['hashes'][key]==hashes[name],'両369 trio pin')
    def boot(self,boot,transition,fresh,bootid,pin,pin_file):
        # Validate the ACTUAL baseline retained chain using the existing proof.
        # This is not proof that the new candidate boot has already been written.
        evidence=shared.validate_boot_witness(boot,transition,fresh,bootid,pin,pin_file)
        newboot=pin(self.pending['matched_boot_manifest']);pin_file(self.pending['matched_boot_image'])
        require(newboot['boot_img_sha256']==self.pending['matched_boot_image']['sha256']==BOOT,'新boot98deの実byte')
        baseline=transition['boot_sha256']
        transition.update(mode='matched-panel-timing-DTM0-v1',boot_path=self.pending['matched_boot_image']['path'],boot_sha256=BOOT,baseline_boot_sha256=baseline,boot_write_required=True,gpt_written=False,mixed_old_new_allowed=False,next_windows_boot_blocked_until_matched_boot=True,firmware_manifest=self.pending['matched_boot_manifest'])
        evidence.update(candidate_boot_already_written=False,candidate_boot_sha256=BOOT)
        return evidence
    def finish(self,p,spec,n,pin,pin_file):
        require(n['full_service_capture_complete'] is True and n['registry_write_requested'] is False,'実完全read-only inventory必須')
        require(n['new_service_name']==p['service'] and n['capture_number']==shared.CAPTURE and n['new_service']['present'] is False,'実freshの新service不在と番号')
        lock=pin(self.spec['prepared_input_lock']);pin(self.spec['prepared_inputs'])
        require(lock['validated'] is True and lock['profile_finalized'] is False,'実準備入力lock')
        for ref in lock['original_input_pins']:pin_file(ref)
        p['GPU369_manifest']=p.pop('GPU368_manifest');p['GPU369_handoff']=p.pop('GPU368_handoff')
        p['candidate_manifest_view']=self.pending['candidate_manifest_view'];pin(p['candidate_manifest_view'])
        p['display_transition'].update(DIF_order=list(shared.INSTALL),old_DOD_retirement='historical177; display369 and render369 deferred update',firmware_dependency='new matched98de DTM0 boot required before candidate start')
        p['diagnostic_clients'][1]['arguments']=['"C:\\Windows\\System32\\'+p['service']+'\\logs\\global-diag369"']
        p['globaldiag_contract']['prefix']=p['diagnostic_clients'][1]['arguments'][0]
        p['retained_package_overrides']=copy.deepcopy(self.spec.get('retained_package_overrides',{}))
        if self.wifi_activation:
            require(p['baselines']['wifi']['mode']=='selected' and p['baselines']['wifi']['sys_sha256']==WIFI_BASE_SYS,'WiFi actual selected baseline保持')
            p['wifi_activation']=True;p['wifi_update_package']=copy.deepcopy(self.spec['wifi_update_package'])
        p['main_execution_approved']=self.spec.get('main_execution_approved',False)
        p['gpu369_timing_contract']=dict(schema=1,record_method='DTM0',record_bytes=64,transport_QI_version=2,matched_boot_sha256=BOOT,manifest_sha256=MANIFEST,compile_source_commit=COMPILE_SOURCE,baseline_native_UMD_source_commit=NATIVE_SOURCE)

shared=None
PROFILE=SERVICE=DIRECTORY=None

def configure_capture(capture,activate_wifi=False):
    global shared,PROFILE,SERVICE,DIRECTORY
    require(type(capture) is int and 204<=capture<=999,'実capture番号204..999のみ')
    module_spec=importlib.util.spec_from_file_location('gpu369_shared200',Path(__file__).with_name('prepare-peripheral-followup200.py'))
    shared=importlib.util.module_from_spec(module_spec);module_spec.loader.exec_module(shared)
    PROFILE=f'combined-features-fresh{capture}-gpu369-timing-v1';SERVICE=DIRECTORY=f'MT8167CombinedFollowup{capture}'
    shared.CAPTURE=capture;shared.PROFILE=PROFILE;shared.SERVICE=shared.DIRECTORY=SERVICE;shared.INSTALL=['display','render']+(['wifi'] if activate_wifi else [])
    shared.SOURCE=COMPILE_SOURCE;shared.MANIFEST=MANIFEST
    shared.base.PROFILE=PROFILE;shared.base.SERVICE=shared.base.DIRECTORY=SERVICE;shared.base.INSTALL=shared.INSTALL

def load_map(path,root):
    spec=read(path);require(spec['schema']=='GPU369_TIMING_FINALIZE_INPUTS_V1','369 finalize schema')
    contract_ref=spec['capture_contract'];require(isinstance(contract_ref,dict),'実capture contract未取得')
    contract_path=rooted(root,contract_ref['path']);require(sha(contract_path)==contract_ref['sha256'],'実capture contract SHA')
    contract=read(contract_path);capture=contract['capture_number']
    require(spec.get('capture_number') in (None,capture),'指定captureと実contract不一致');configure_capture(capture,wifi_activation(spec))
    require(spec.get('profile') in (None,PROFILE),'実captureと369 profile結合');spec['profile']=PROFILE;spec['capture_number']=capture
    def pinned(ref):
        q=rooted(root,ref['path']);require(sha(q)==ref['sha256'],'準備済みref SHA不一致');return read(q)
    pending=pinned(spec['prepared_inputs']);lock=pinned(spec['prepared_input_lock'])
    require(pending['schema']=='GPU369_TIMING_PENDING_INPUTS_V1' and lock['validated'] is True,'準備済み369入力のみ')
    for ref in lock['original_input_pins']:require(sha(rooted(root,ref['path']))==ref['sha256'],'原本入力変化')
    require(pending['candidate_signed_manifest']['sha256']==MANIFEST,'準備manifest pin')
    current=copy.deepcopy(spec)
    require(spec['install_targets']==shared.INSTALL and spec['hardware_owner']=='main','369 display/renderと明示WiFiだけ')
    current['GPU368_manifest']=pending['candidate_signed_manifest'];current['GPU368_handoff']=next(r for r in lock['original_input_pins'] if r['path'].endswith('windows-rt-panel-timing369-complete/independent-review.json'))
    shared.TIMING=TimingInputs(root,spec,pending)
    return shared.build_policy(root,current)

def generate(policy):
    configure_capture(policy['capture_selection']['capture_number'],policy.get('wifi_activation',False))
    require(policy['install_targets']==shared.INSTALL,'生成時も同じWiFi選択を保持')
    return shared.generate(policy)

def generate_paths():
    require(shared is not None,'先に実captureをconfigureすること')
    return shared.paths()

def assert_paths(image,capture=False):
    require(shared is not None,'先に実captureをconfigureすること')
    return shared.assert_paths(image,capture)

if __name__=='__main__':main()
