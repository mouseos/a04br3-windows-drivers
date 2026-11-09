#!/usr/bin/env python3
"""run111: fresh110の実搭載render3.20からnative境界記録3.21へ（機能成功ではない）。親/Audio/eMMC/USB/WiFi/DODは保持。"""
import argparse,hashlib,json,re,struct,subprocess
from pathlib import Path
from render_profile_policy import require,rooted,jsonfile,hexpin,sha,cbytes,SIGNER
from codec_msdc_transaction import TARGETS,RETAINED,RETAINED_FILES,version,datevalue
from codec_returned_baseline import validate_selection,load_boot
from engine_io_package import require_armnt
PROFILE='render-single-gpu321-run111-v1'
FRESH='fc2a8277510f8836ca1d74efd15939e48a6dbe734b37cf31510448ceb6efa9d9'
SOURCE='33853e9c5397614349ecdd1178f1cdd3b0feb4a8'
PACKAGE='9d37f9274882cc49dc7d1f5ed044b94d1b8c3be4c5dd818a84918bbf0cac4c07'
NATIVE='6bc00603aea90762a04b125667645979c1887d40e4632e0105d44a51e449a9a3'
UMD='b4085342c899c24a095780d8b34bbb1b76b2926ef13836e0b4d292d19b5dfbbe'
SYS='d220cc241d4a7966ca90d9be131bd693d60fbd0449217ad8e809e50175abd602';BASE_RENDER_SYS='37624344a8a2ed23d1026db57324c82180c21ee13cf077817241f332a71e1683';BASE_UMD='b4085342c899c24a095780d8b34bbb1b76b2926ef13836e0b4d292d19b5dfbbe';FW=RETAINED_FILES['ge8300-mips.fw']
BASE_SYS={'parent':'f176836aafb34447b423398d301ffa01d9787f3bc197c8c29f371145a6e756e1','audio':'4facb36eed0b45edf32ae234ec3afa325ea1a9bb58884caa7042139a72121699','emmc':'f92ee2d4e058e345539567153e0b4a20b13669f34f84de017dece2e45707dadd','render':BASE_RENDER_SYS}
def outputs(directory,m,wanted):
 rows=m['outputs'];require(len(rows)==len(wanted) and {x['name'] for x in rows}==set(wanted),'固定outputs schema不一致')
 for x in rows:require(sha(directory/x['name'])==hexpin(x['sha256']),'署名output byte不一致')
 return {n:sha(directory/n) for n in wanted}
def load_map(path,root):
 root=root.resolve(strict=True);s=jsonfile(rooted(root,path))
 require(set(s)=={'schema','profile','backup','backup_sha256','bindings','packages','native','boot','comparator','previous_engine_media'},'singleGPU専用input fields必須')
 require(s['schema']==1 and s['profile']==PROFILE and s['backup_sha256']==FRESH,'固定run111/fresh110必須')
 backup=rooted(root,s['backup']);require(sha(backup)==FRESH,'fresh110実hash不一致');b=jsonfile(backup)
 require(b.get('status')=='captured-unmounted' and b.get('unmounted') is True and b.get('errors')==[],'fresh110完了原本必須')
 files={}
 for e in b['files']:
  f=rooted(root,backup.parent/e['backup_path']);require(f.is_relative_to(backup.parent) and sha(f)==e['sha256'] and f.stat().st_size==e['bytes'],'fresh110実file不一致');files[e['source_path'].replace('\\','/').lower()]=e
 receipt=rooted(root,s['bindings']['path']);require(sha(receipt)==hexpin(s['bindings']['sha256']),'native seven-node pin不一致')
 binding=jsonfile(receipt)
 current_targets=dict(TARGETS);current_targets['render']=(*TARGETS['render'][:4],'0.0.3.20')
 selection=validate_selection(binding,b,FRESH,current_targets,RETAINED,observe_only=True)
 for role,expected in BASE_SYS.items():require(files['windows/system32/drivers/'+TARGETS[role][2]+'.sys']['sha256']==expected,'現在SYS不一致')
 for role,t in RETAINED.items():require(files['windows/system32/drivers/'+t[3]]['sha256']==t[4],'保持SYS不一致')
 require(set(s['packages'])=={'render'},'更新packageはrender1件だけ')
 require(re.fullmatch('[0-9a-f]{40}',SOURCE) and all(re.fullmatch('[0-9a-f]{64}',v) for v in (PACKAGE,SYS)),'GPU321 candidate pins未確定: draftはbuild不可')
 pkg=rooted(root,s['packages']['render']['directory']);mf=pkg/'signed-manifest.json';m=jsonfile(mf)
 require(s['packages']['render']['manifest_sha256']==PACKAGE and sha(mf)==PACKAGE,'正式3.21 manifest pin必須')
 require(m.get('schema')=='native-progress321-independent-sources-v1' and m.get('package_source_commit')==SOURCE and m.get('kernel_binary_source_commit')=='f73da1e31755e08727958ac6f9160a0d5ccdff6b' and m.get('native_owner_source_commit')=='e0614136f88262f52d60f6458b5e3811de04fdb6' and m.get('native_owner_changed') is True and m.get('umd_source_commit')=='3f8291d094c01206dfaa0a22d1fdccd6eecc5823' and m.get('baseline_manifest_sha256')=='86557e13fbac04f3d38b3949516f47b14435fa2e6c3c411e5e2319209dee2669' and m.get('trace_manifest_sha256')==NATIVE and m.get('allocation_list_capacity')==3 and m.get('patch_location_capacity')==2 and m.get('dma_buffer_size_changed') is False and m.get('render_before_residency') is True and m.get('patch_passive_prepare') is True and m.get('owned_payload_relocation') is True and m.get('selected_mdl_spans') is True and m.get('software_cache_coherent_maps') is True and m.get('private_pointer_input_preserved') is False and m.get('private_dma_cursor_consumed') is True and m.get('private_dma_expected_used_bytes')==16 and m.get('umd_changed') is False and m.get('client_changed') is False and m.get('client_source_commit')=='f4f1b4f94ade6f59efef042f680c2c3e363442f8','independent KMD/UMD source chain required')
 require(m.get('signature_status')=='Valid' and m.get('signer_thumbprint')==SIGNER and m.get('kernel_changed') is True and m.get('firmware_changed') is False and m.get('permanent_sysmem_changed') is False and m.get('limited_development_profile') is True and m.get('standard_profile_qualified') is False and m.get('development_negotiation_pipeline')=='10_0' and m.get('inf_driverver')=='11/07/2026,0.0.3.21','3.21 contract mismatch')
 for leaf,key in [('kernel-signed-manifest.json','kernel_manifest_sha256'),('kernel-build-manifest.json','kernel_build_manifest_sha256'),('trace-signed-manifest.json','trace_manifest_sha256'),('native-dependency-closure.json','native_dependency_closure_sha256')]:require(sha(pkg/leaf)==m[key],'independent evidence hash mismatch')
 closure=jsonfile(pkg/'native-dependency-closure.json')
 require(closure.get('schema')=='native-progress321-incremental-closure-v1' and closure.get('baseline_native_source_commit')=='fb2173597de9acc12af8f06fd771037e02093360' and closure.get('native_source_commit')=='e0614136f88262f52d60f6458b5e3811de04fdb6' and closure.get('changed_units')==['native_owner_factory','native_owner_factory_runtime'] and closure.get('changed_abi') is False,'native境界記録の固定2TU/ABI保持契約不一致')
 require(sha(pkg/'baseline-native-dependency-closure.json')==hexpin(closure['baseline_dependency_closure_sha256']) and sha(pkg/'native-progress-manifest.json')==hexpin(closure['native_progress_manifest_sha256']),'native境界記録の出自chain不一致')
 combined=jsonfile(pkg/'kernel-build-manifest.json')
 require(combined.get('schema')=='native-progress321-combined-v1' and combined.get('package_source_commit')==SOURCE and combined.get('kmd_source_commit')=='f73da1e31755e08727958ac6f9160a0d5ccdff6b' and combined.get('native_source_commit')=='e0614136f88262f52d60f6458b5e3811de04fdb6' and combined.get('instrumentation_only') is True and len(combined.get('native_units',[]))==2,'3.21はnative境界記録のみ/KMD保持を要求')
 names=['ge8300-render.inf','ge8300-render.sys','ge8300-render.cat','ge8300-mips.fw','ge8300-umd.dll']
 hashes=outputs(pkg,m,names+['mt8167-development.cer','ge8300-d3d-clear.exe']);require(hashes[names[1]]==SYS and hashes[names[3]]==FW and hashes[names[4]]==UMD,'SYS/FW保持・UMD正式pin不一致')
 require(hashlib.sha1((pkg/'mt8167-development.cer').read_bytes()).hexdigest().upper()==SIGNER,'cert不一致')
 inf=(pkg/names[0]).read_text(encoding='utf-16');require('DriverVer=11/07/2026,0.0.3.21' in inf and 'ACPI\\MOU0006' in inf and 'HKR,,UserModeDriverName,0x00010000,ge8300-umd.dll,ge8300-umd.dll,ge8300-umd.dll' in inf,'3.21 INF/service/third slot不一致')
 nd=rooted(root,s['native']['directory']);nm=nd/'signed-manifest.json';n=jsonfile(nm)
 require(s['native']['manifest_sha256']==NATIVE and sha(nm)==NATIVE and sha(pkg/'trace-signed-manifest.json')==NATIVE,'native manifest chain不一致')
 require(n.get('source_commit')=='3f8291d094c01206dfaa0a22d1fdccd6eecc5823' and n.get('client_source_commit')=='f4f1b4f94ade6f59efef042f680c2c3e363442f8' and n.get('signature_status')=='Valid' and n.get('signer_thumbprint')==SIGNER and n.get('pipeline')=='10_0' and n.get('limited_development_profile') is True and n.get('standard_profile_qualified') is False and n.get('kernel_changed') is False and n.get('fixed_arguments')=='--hardware=PowerVR' and n.get('retry_count')==0 and n.get('external_timeout_ms')==120000,'FL10限定client契約不一致')
 nh=outputs(nd,n,['ge8300-umd.dll','ge8300-d3d-clear.exe','build-manifest.json']);require(nh['ge8300-umd.dll']==UMD,'UMD nativechain不一致');require_armnt(nd/'ge8300-d3d-clear.exe');require_armnt(pkg/'ge8300-umd.dll')
 boot,bm=load_boot(s['boot'],root)
 compdir=rooted(root,s['comparator']['directory']);cm=compdir/'render-service-manifest.json';require(sha(cm)==hexpin(s['comparator']['manifest_sha256']),'既存comparator manifest不一致');cp=jsonfile(cm)['comparator']
 require(cp.get('signature_status')=='Valid' and cp.get('signer_thumbprint')==SIGNER and sha(compdir/'compare-files.exe')==cp['exe_sha256'],'署名済comparator不一致')
 previous=rooted(root,s['previous_engine_media']['path']);require(sha(previous)==hexpin(s['previous_engine_media']['sha256']),'ObserveOnly元媒体pin不一致');old=jsonfile(previous)
 require(old.get('source_commit')=='66996e88297ed6f7883d036c78eecff9530788b4' and old.get('transaction')=='codec-render-observe-fresh92-v1','run93元媒体のみ許可')
 history=list(old['engine_rearm_plan']['diagnostic_policies']);token=hashlib.sha256(json.dumps(old['probe']['deferred_update'],sort_keys=True,separators=(',',':')).encode()).hexdigest()
 if token not in history:history.append(token)
 require(len(history)<=16 and all(re.fullmatch('[0-9a-f]{64}',x) for x in history),'Engine履歴allowlist不一致')
 preservation={'schema':1,'profile':PROFILE,'backup_manifest_sha256':FRESH,'diagnostic_policies':history,'source_files':{n:{k:files['windows/system32/config/'+n.lower()][k] for k in ('sha256','bytes')} for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},'engine_rearm':False}
 preservation['single_gpu_rearm']={'schema':1,'service':'MT8167RenderSingleGpu','maximum_rearms':1,'diagnostic_policies':['d64c9165297bb602ac04be1d9053998c2f6f8da3581545b6be8b005d237fcdb5','6b00fa9f57696f19ad10ecdace497d0a3b1b4bcb861e98d7a8571e0ce23f19c3','10f8ec77e44a42c25a01c17434adb9c4bc80f180c142173725846d7735015c64','b6273b09f165e3e5fcc61d49bc8b874008f9350d30592737c71e31a2999e905f','bf84654ab50707ca2d660f685663cf4dd8b1c216921e862a7289340c5056ebe0','9947ede87028cd2d8100e443006cf516ed2c5e17e06229bb688943e2746dbc4b','a66ac828fe1c6838f4de2a8ce766313b53a9d48682463a456727166c5bb39bea'],'data_directory':'MT8167RenderSingleGpuRun111','partial_diagnostic_policies':['10f8ec77e44a42c25a01c17434adb9c4bc80f180c142173725846d7735015c64','bf84654ab50707ca2d660f685663cf4dd8b1c216921e862a7289340c5056ebe0','9947ede87028cd2d8100e443006cf516ed2c5e17e06229bb688943e2746dbc4b','a66ac828fe1c6838f4de2a8ce766313b53a9d48682463a456727166c5bb39bea']}
 base=selection['render'];base.update(driver_version='0.0.3.20',sys_name='ge8300-render.sys',sys_sha256=BASE_RENDER_SYS,inf=base['selected']['InfPath'])
 retained={k:v for k,v in selection.items() if k!='render'}
 retainedfiles={**RETAINED_FILES,**{TARGETS[k][2]+'.sys':v for k,v in BASE_SYS.items() if k!='render'}};retainedfiles.pop('ge8300-umd.dll');retainedfiles.pop('ge8300-render.sys')
 return {'schema':1,'profile':PROFILE,'default_off':True,'data_directory':'MT8167RenderSingleGpuRun111','software_trust_policy':'preserve-existing; require WinVerifyTrust/CAT membership before single-GPU install; no SOFTWARE writes','backup_path':backup.relative_to(root).as_posix(),'backup_manifest_sha256':FRESH,'binding_receipt_sha256':sha(receipt),'baselines':{'render':base},'packages':{'render':{'directory':pkg.relative_to(root).as_posix(),'files':{n:hashes[n] for n in names},'version_u64':version('0.0.3.21'),'date_filetime':datevalue('11/07/2026'),'source_commit':SOURCE,'signed_manifest_sha256':PACKAGE}},'client':{'directory':nd.relative_to(root).as_posix(),'source_commit':n['client_source_commit'],'sha256':nh['ge8300-d3d-clear.exe'],'manifest_sha256':NATIVE,'arguments':'--hardware=PowerVR','timeout_ms':120000,'retry_count':0,'pipeline':'10_0','limited_development_profile':True,'standard_profile_qualified':False},'comparator':{'directory':compdir.relative_to(root).as_posix(),'manifest_sha256':sha(cm),**cp},'retained_selection':retained,'retained_files':retainedfiles,'baseline_umd_sha256':BASE_UMD,'engine_preservation':preservation,'boot':{'source_commit':bm['uefi_source_commit'],'sha256':bm['boot_img_sha256'],'manifest_sha256':sha(boot),'action':'preserve; no boot or GPT writes'},'pnp_install_count':1,'maximum_planned_reboot_count':1,'client_count':1,'audio_retest':False,'normal_install_allowed':True,'sequence':['stage exact render candidate and dedicated single-GPU service','one render DIF_INSTALLDEVICE; no live device restart','maximum one planned reboot','exact render/retained verification and one GPU client; stop']}
def generate(p):
 require(p.get('profile')==PROFILE and set(p['packages'])=={'render'},'single GPU profile only')
 v=p['packages']['render'];base=p['baselines']['render']
 lines=['#define DEFERRED_TARGET_COUNT 1','#define DEFERRED_GPU_UPDATE 0','#define DEFERRED_SD_UPDATE 0','#define DEFERRED_BRIGHTNESS_SCOPED_LOG 0','#define RENDER_OFFLINE_DIAGNOSTIC 0','#if !A04_RENDER_SINGLE_GPU || !A04_RENDER_BIND_UPDATE\n#error Use the explicit single-GPU compile profile\n#endif','static const struct deferred_pin deferred_pins[1]={{'+str(v['version_u64'])+'ULL,'+str(v['date_filetime'])+'ULL,{'+','.join(cbytes(x) for x in v['files'].values())+'}}};','static const struct deferred_pin deferred_baselines[1]={{'+str(base['version_u64'])+'ULL,'+str(base['date_filetime'])+'ULL,{{0},'+cbytes(base['sys_sha256'])+'}}};']
 for field,cname in [('sys_name','sysnames'),('class_guid','classes'),('service','services')]:lines.append('static const WCHAR *const deferred_baseline_'+cname+'[1]={L"'+base[field]+'"};')
 lines+=['static const BYTE single_baseline_umd[32]='+cbytes(p['baseline_umd_sha256'])+';','static const WCHAR *single_baseline_inf=L"'+base['inf']+'";','struct single_retained_pin {const WCHAR *id,*service,*cls,*inf;ULONGLONG version,date;};','static const struct single_retained_pin single_retained[]={']
 for v in p['retained_selection'].values():
  values=[v['hardware_id'].replace('\\','\\\\'),v['service'],v['class_guid'],v['selected']['InfPath']];lines.append('{'+','.join('L"'+x+'"' for x in values)+','+str(v['version_u64'])+'ULL,'+str(v['date_filetime'])+'ULL},')
 lines+=['};','static const WCHAR *single_retained_names[]={'+','.join('L"'+n+'"' for n in p['retained_files'])+'};','static const BYTE single_retained_hashes[][32]={'+','.join(cbytes(h) for h in p['retained_files'].values())+'};','static const struct deferred_pin deferred_render_baseline={0};','static const char deferred_token[]="'+hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()+'";','static const BYTE deferred_client_hashes[1][32]={'+cbytes(p['client']['sha256'])+'};']
 return '\n'.join(lines)+'\n'
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--enable-render-profile',action='store_true')
 for n in ('workspace','package-map','output'):ap.add_argument('--'+n,type=Path,required=True)
 a=ap.parse_args();require(a.enable_render_profile,'明示singleGPU opt-in必須');p=load_map(a.package_map,a.workspace);out=a.output.resolve();require(out.is_relative_to(a.workspace.resolve()) and not out.exists(),'root内新出力必須');out.mkdir(parents=True)
 (out/'render-deferred-policy.h').write_text(generate(p));(out/'render-policy-manifest.json').write_text(json.dumps(p,indent=2)+'\n');(out/'engine-preservation-plan.json').write_text(json.dumps(p['engine_preservation'],indent=2)+'\n')
if __name__=='__main__':main()
