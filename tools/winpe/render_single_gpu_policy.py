#!/usr/bin/env python3
"""run119: RT8.1 shader署名修正3.23。最新の周辺更新後captureを結合するまで発火不可。"""
import argparse,hashlib,json,re,struct,subprocess
from pathlib import Path
from render_profile_policy import require,rooted as checked_rooted,jsonfile,hexpin,sha,cbytes,SIGNER
from codec_msdc_transaction import TARGETS,RETAINED,RETAINED_FILES,version,datevalue
from codec_returned_baseline import validate_selection,pinned_file
from engine_io_package import require_armnt
PROFILE='render-single-gpu325-run121-v1'
DATA_DIRECTORY='MT8167RenderSingleGpuRun121'
FRESH='68b2d5946a61715a1604c25c554c67a7a4d9b4d1718fd33b627125631b853216'
SOURCE='2ceaabe0a54a651a30ae13c9b956ebb6716809af'
PACKAGE='8e595170ddec23335e9fb2f7031f45e9eba5f393adae32df01c10b93cc896ca0'
NATIVE='6bc00603aea90762a04b125667645979c1887d40e4632e0105d44a51e449a9a3'
UMD='716398511b3e1ed737c96a621c9083ab54d1274f6263b700335af167ac3d3815'
SYS='45aa212862cc4e60812350a1aedcbd6d6f4d6a1415138292fa768f850c071920';BASE_RENDER_SYS='b23ef1dfb4336bf6c3599285f59b4a8c20f69b6801f4873b3c39462ad42e68b9';TRACE_UMD='b4085342c899c24a095780d8b34bbb1b76b2926ef13836e0b4d292d19b5dfbbe';BASE_UMD='aa11e6336d843d6e6f03ff5ab1731264ffa190a3fd7ce246db46fc7f04a923de';FW=RETAINED_FILES['ge8300-mips.fw']
BASE_SYS={'parent':'398fd007079ab4710404d67a60dc7db46b84dd149ce3ae69e3bbc730264e823c','audio':'4facb36eed0b45edf32ae234ec3afa325ea1a9bb58884caa7042139a72121699','emmc':'b3ce17a8a36b1afba25ca53503735f57aba4b47723357a960169dd47b36f9bda','render':BASE_RENDER_SYS}

# Exact post-run115 USB, without mutating legacy codec policy modules.
RETAINED=dict(RETAINED);RETAINED['usb']=('MT8167\\USB20','mshidkmdf','2026.11.8.1009','mt8167-usb20.sys','9f8d3e49d13efe6fa0b84d71c061c3829482c7352ed8f409ac8914d586320a7a')
RETAINED_FILES=dict(RETAINED_FILES);RETAINED_FILES['mt8167-usb20.sys']=RETAINED['usb'][4]
RETAINED_FILES.update({'mma8652.sys':'998329ec0541a7738c893c3185b591439e433ec55489aee657670a5b89f52388','mt6392-rtc.sys':'3da6c7dd7cc5279fccc45581ce2b20405e08291b42ebf1fbf45e2a7a2e49d7eb','mt8167-thermal.sys':'ca211e366704380966ef639c24a1d49b71425b39cd7272ebdeeb03c3b412408b'})
def rooted(root,path):
 p=Path(path);return checked_rooted(root,p if p.is_absolute() else root/p)

def outputs(directory,m,wanted):
 rows=m['outputs'];require(len(rows)==len(wanted) and {x['name'] for x in rows}==set(wanted),'固定outputs schema不一致')
 for x in rows:require(sha(directory/x['name'])==hexpin(x['sha256']),'署名output byte不一致')
 return {n:sha(directory/n) for n in wanted}
def load_boot(entry,root):
 # run115 installed this exact eight-range firmware with parent1101.
 # The GPU update preserves it and must never return to the six-range boot.
 boot=pinned_file(entry,root);bm=jsonfile(boot)
 image=rooted(root,boot.parent/'boot.img')
 require(image.parent==boot.parent and bm.get('uefi_source_commit')=='6c96c6f60149f2b01d068909c8d0c308e5b4f3ee'
  and bm.get('boot_img_sha256')=='ece47d799e04e794bffcf902d2e4ef32c7e898f60079eff98134467afda693d8'
  and sha(image)==bm['boot_img_sha256'],'保持8range POST boot source/image不一致')
 return boot,bm
def load_map(path,root):
 require(re.fullmatch('[0-9a-f]{64}',FRESH),'run115後の最新capture未結合: draft発火不可')
 root=root.resolve(strict=True);s=jsonfile(rooted(root,path))
 require(set(s)=={'schema','profile','backup','backup_sha256','bindings','packages','native','boot','comparator','previous_engine_media','previous_service'},'singleGPU専用input fields必須')
 require(s['schema']==1 and s['profile']==PROFILE and s['backup_sha256']==FRESH,'固定run119/返却後fresh必須')
 backup=rooted(root,s['backup']);require(sha(backup)==FRESH,'返却後fresh実hash不一致');b=jsonfile(backup)
 require(b.get('status')=='captured-unmounted' and b.get('unmounted') is True and b.get('errors')==[],'返却後fresh完了原本必須')
 files={}
 for e in b['files']:
  f=rooted(root,backup.parent/e['backup_path']);require(f.is_relative_to(backup.parent) and sha(f)==e['sha256'] and f.stat().st_size==e['bytes'],'返却後fresh実file不一致');files[e['source_path'].replace('\\','/').lower()]=e
 receipt=rooted(root,s['bindings']['path']);require(sha(receipt)==hexpin(s['bindings']['sha256']),'native seven-node pin不一致')
 binding=jsonfile(receipt)
 current_targets=dict(TARGETS);current_targets['render']=(*TARGETS['render'][:4],'0.0.3.23')
 current_targets['parent']=(*TARGETS['parent'][:4],'2026.11.8.1101')
 current_targets['emmc']=(*TARGETS['emmc'][:4],'0.0.1.9')
 selection=validate_selection(binding,b,FRESH,current_targets,RETAINED,observe_only=True)
 for role,expected in BASE_SYS.items():require(files['windows/system32/drivers/'+TARGETS[role][2]+'.sys']['sha256']==expected,'現在SYS不一致')
 for role,t in RETAINED.items():require(files['windows/system32/drivers/'+t[3]]['sha256']==t[4],'保持SYS不一致')
 for name in ('mma8652.sys','mt6392-rtc.sys','mt8167-thermal.sys'):
  require(files['windows/system32/drivers/'+name]['sha256']==RETAINED_FILES[name],'run115後の追加周辺SYS保持pin不一致')
 require(set(s['packages'])=={'render'},'更新packageはrender1件だけ')
 require(re.fullmatch('[0-9a-f]{40}',SOURCE) and all(re.fullmatch('[0-9a-f]{64}',v) for v in (PACKAGE,SYS)),'GPU323 candidate pins未確定: draftはbuild不可')
 pkg=rooted(root,s['packages']['render']['directory']);mf=pkg/'signed-manifest.json';m=jsonfile(mf)
 require(s['packages']['render']['manifest_sha256']==PACKAGE and sha(mf)==PACKAGE,'正式325 manifest pin必須')
 require(m.get('schema')=='blacktriangle325-independent-sources-v1' and m.get('package_source_commit')==SOURCE and m.get('kernel_binary_source_commit')=='f73da1e31755e08727958ac6f9160a0d5ccdff6b' and m.get('native_owner_source_commit')==SOURCE and m.get('native_owner_baseline_source_commit')=='f96948533601f9fdcdcc22b9700aacd96dbf7204' and m.get('umd_source_commit')==SOURCE and m.get('client_source_commit')=='f4f1b4f94ade6f59efef042f680c2c3e363442f8','325 independent KMD/native/UMD/client provenance required')
 require(m.get('kernel_changed') is True and m.get('kmd_objects_changed') is False and m.get('native_owner_changed') is True and m.get('umd_changed') is True and m.get('client_changed') is False and m.get('firmware_changed') is False and m.get('compiler_changed') is False and m.get('permanent_sysmem_changed') is False and m.get('changed_caps') is False and m.get('changed_wire_abi') is False,'325 native1TU/kernel relink + UMD1TU, no KMD/PCO/FW/client rebuild')
 require(m.get('signature_status')=='Valid' and m.get('signer_thumbprint')==SIGNER and m.get('inf_driverver')=='11/07/2026,0.0.3.25' and m.get('limited_development_profile') is True and m.get('standard_profile_qualified') is False and m.get('development_negotiation_pipeline')=='10_0','325 signed limited FL10 contract required')
 require(m.get('native_compiled_units')==['native_d3d_viewport'] and m.get('umd_compiled_units')==['umd/typed_draw'] and m.get('viewport_ppp_order')=='Xoffset,Xscale,Yoffset,Yscale,Zoffset,Zscale' and m.get('baseline_archive_reconstructed') is True and m.get('historical_baseline_archive_byte_equal') is False,'325 root fix/archive provenance mismatch')
 require(m.get('allocation_list_capacity')==3 and m.get('patch_location_capacity')==2 and m.get('private_dma_expected_used_bytes')==16 and m.get('dma_buffer_size_changed') is False and m.get('render_before_residency') is True and m.get('patch_passive_prepare') is True and m.get('owned_payload_relocation') is True and m.get('selected_mdl_spans') is True and m.get('software_cache_coherent_maps') is True and m.get('private_pointer_input_preserved') is False and m.get('private_dma_cursor_consumed') is True,'retained GPU/DMA contract mismatch')
 require(m.get('baseline_manifest_sha256')=='fd1c90d35fde04663a17285cbb69e91b3545ddf54af1e030a77aacd3813626bf' and sha(pkg/'baseline323-signed-manifest.json')==m['baseline_manifest_sha256'],'Actual323 baseline required; no324')
 buildfile=pkg/'viewport325-build-manifest.json';buildpin='1da373f2106d04bb95d1bbe19603b9372572f97251f8b4b8cd691254fa1651b5'
 require(m.get('build_manifest_file')==buildfile.name and all(m.get(k)==buildpin for k in ('build_manifest_sha256','kernel_build_manifest_sha256','umd_build_manifest_sha256')) and sha(buildfile)==buildpin,'325 two-TU build manifest mismatch');build=jsonfile(buildfile)
 require(build.get('schema')=='blacktriangle325-two-tu-build-v1' and build.get('source_commit')==SOURCE and build.get('baseline323_manifest_sha256')==m['baseline_manifest_sha256'] and build.get('baseline_native_source_commit')=='f96948533601f9fdcdcc22b9700aacd96dbf7204' and build.get('kernel_binary_source_commit')=='f73da1e31755e08727958ac6f9160a0d5ccdff6b' and build.get('native_compiled_units')==['native_d3d_viewport'] and build.get('umd_compiled_units')==['umd/typed_draw'],'325 source/build unit mismatch')
 require(all(build.get(k) is False for k in ('changed_wire_abi','changed_caps','compiler_rebuilt','client_rebuilt','sdk_installed')) and build.get('baseline_native_library_sha256')=='0ebf455c975bd80b838dca3199b06f010a0d282d043ec6db68ec077546d82ce4' and build.get('baseline_kernel_library_sha256')=='3c818dba1d5b567a21ce1e1f594ebfd9fe03b0f26acf22f83978fe92cf29e4ce','325 baseline/retained compiler/KMD contract mismatch')
 for key,count,changed_name in [('native_archive_members',54,'vendor_m1_11-native_d3d_viewport.obj'),('umd_archive_members',17,'umt_typed_draw.obj')]:
  rows=build[key];require(len(rows)==count and len({x['name'] for x in rows})==count,'Archive actual member count/name mismatch')
  changed=[x for x in rows if x['changed']];require(len(changed)==1,'Exactly one changed archive member required')
  if key=='native_archive_members':require(changed[0]['name']==changed_name,'Native viewport TU mismatch')
  else:require('typed_draw' in changed[0]['name'],'UMD typed draw TU mismatch')
  require(all(re.fullmatch('[0-9a-f]{64}',x['before_sha256']) and re.fullmatch('[0-9a-f]{64}',x['after_sha256']) and ((x['before_sha256']!=x['after_sha256'])==x['changed']) for x in rows),'Archive byte preservation mismatch')
 closurepin='f5245d5af584770af7eb92ac12ac37fa6a5aab39876086771acfd4bfd9d28526';require(m.get('native_dependency_closure_sha256')==closurepin and sha(pkg/'native-dependency-closure.json')==closurepin,'325 native closure pin mismatch');closure=jsonfile(pkg/'native-dependency-closure.json')
 require(closure.get('schema')=='viewport325-native-closure-v1' and closure.get('source_commit')==SOURCE and closure.get('baseline_native_owner_source_commit')=='f96948533601f9fdcdcc22b9700aacd96dbf7204' and closure.get('native_build_manifest_sha256')==buildpin and closure.get('changed_wire_abi') is False and closure.get('changed_members')==[x for x in build['native_archive_members'] if x['changed']] and closure.get('retained_members')==[x for x in build['native_archive_members'] if not x['changed']],'Native closure must match all54 actual members')
 require(sha(pkg/closure['baseline_dependency_closure_file'])==closure['baseline_dependency_closure_sha256']=='5f9cd4b3588d8c90503c25edfd624995a1ef0a8b11c3958f5d57f848b6db22cf','Retained322 native closure mismatch')
 for x in m['retained_provenance']:require(sha(pkg/x['name'])==hexpin(x['sha256']),'Retained provenance file hash mismatch')
 require(m.get('trace_manifest_sha256')==NATIVE and sha(pkg/'trace-signed-manifest.json')==NATIVE and sha(pkg/'kernel-signed-manifest.json')==m['kernel_manifest_sha256'],'Retained trace/KMD evidence mismatch')
 names=['ge8300-render.inf','ge8300-render.sys','ge8300-render.cat','ge8300-mips.fw','ge8300-umd.dll']
 hashes=outputs(pkg,m,names+['mt8167-development.cer','ge8300-d3d-clear.exe']);require(hashes[names[1]]==SYS and hashes[names[3]]==FW and hashes[names[4]]==UMD,'SYS/FW保持・UMD正式pin不一致')
 require(hashlib.sha1((pkg/'mt8167-development.cer').read_bytes()).hexdigest().upper()==SIGNER,'cert不一致')
 inf=(pkg/names[0]).read_text(encoding='utf-16');require('DriverVer=11/07/2026,0.0.3.25' in inf and 'ACPI\\MOU0006' in inf and 'HKR,,UserModeDriverName,0x00010000,ge8300-umd.dll,ge8300-umd.dll,ge8300-umd.dll' in inf,'3.23 INF/service/third slot不一致')
 nd=rooted(root,s['native']['directory']);nm=nd/'signed-manifest.json';n=jsonfile(nm)
 require(s['native']['manifest_sha256']==NATIVE and sha(nm)==NATIVE and sha(pkg/'trace-signed-manifest.json')==NATIVE,'native manifest chain不一致')
 require(n.get('source_commit')=='3f8291d094c01206dfaa0a22d1fdccd6eecc5823' and n.get('client_source_commit')=='f4f1b4f94ade6f59efef042f680c2c3e363442f8' and n.get('signature_status')=='Valid' and n.get('signer_thumbprint')==SIGNER and n.get('pipeline')=='10_0' and n.get('limited_development_profile') is True and n.get('standard_profile_qualified') is False and n.get('kernel_changed') is False and n.get('fixed_arguments')=='--hardware=PowerVR' and n.get('retry_count')==0 and n.get('external_timeout_ms')==120000,'FL10限定client契約不一致')
 nh=outputs(nd,n,['ge8300-umd.dll','ge8300-d3d-clear.exe','build-manifest.json']);require(nh['ge8300-umd.dll']==TRACE_UMD,'UMD nativechain不一致');require_armnt(nd/'ge8300-d3d-clear.exe');require_armnt(pkg/'ge8300-umd.dll')
 boot,bm=load_boot(s['boot'],root)
 compdir=rooted(root,s['comparator']['directory']);cm=compdir/'render-service-manifest.json';require(sha(cm)==hexpin(s['comparator']['manifest_sha256']),'既存comparator manifest不一致');cp=jsonfile(cm)['comparator']
 require(cp.get('signature_status')=='Valid' and cp.get('signer_thumbprint')==SIGNER and sha(compdir/'compare-files.exe')==cp['exe_sha256'],'署名済comparator不一致')
 previous=rooted(root,s['previous_engine_media']['path']);require(sha(previous)==hexpin(s['previous_engine_media']['sha256']),'ObserveOnly元媒体pin不一致');old=jsonfile(previous)
 require(old.get('source_commit')=='66996e88297ed6f7883d036c78eecff9530788b4' and old.get('transaction')=='codec-render-observe-fresh92-v1','run93元媒体のみ許可')
 prev=s['previous_service'];require(set(prev)=={'path','sha256','manifest','manifest_sha256','source_commit'},'Previous actual SCM EXE/manifest pins required')
 previous_exe=rooted(root,prev['path']);previous_manifest=rooted(root,prev['manifest']);pm=jsonfile(previous_manifest)
 require(prev['sha256']=='c1de160bcba3cc199d1cfcd6f266ad8793150c7b76f4e441b0ac3c7074f42a4c' and sha(previous_exe)==prev['sha256'] and sha(previous_manifest)==hexpin(prev['manifest_sha256']),'Actual installed119 SCM bytes/manifest mismatch')
 require(prev['source_commit']==pm.get('source_commit')=='a73032808ec741e6d078b0d04e22ad0b98400a8e' and pm.get('exe_sha256')==prev['sha256'] and pm.get('signature_status')=='Valid' and pm.get('signing_thumbprint')==SIGNER and pm.get('profile')=='render-single-gpu323-run119-v1' and pm.get('policy',{}).get('data_directory')=='MT8167RenderSingleGpuRun119' and pm.get('data_directory')=='MT8167RenderSingleGpuRun119' and pm.get('runtime_utf16_path_verified') is True,'Previous SCM source/profile provenance mismatch')
 previous_service={'path':previous_exe.relative_to(root).as_posix(),'sha256':prev['sha256'],'manifest_sha256':sha(previous_manifest),'source_commit':prev['source_commit'],'declared_data_directory':pm['policy']['data_directory'],'runtime_utf16_path_verified':True}
 history=list(old['engine_rearm_plan']['diagnostic_policies']);token=hashlib.sha256(json.dumps(old['probe']['deferred_update'],sort_keys=True,separators=(',',':')).encode()).hexdigest()
 if token not in history:history.append(token)
 require(len(history)<=16 and all(re.fullmatch('[0-9a-f]{64}',x) for x in history),'Engine履歴allowlist不一致')
 preservation={'schema':1,'profile':PROFILE,'backup_manifest_sha256':FRESH,'diagnostic_policies':history,'source_files':{n:{k:files['windows/system32/config/'+n.lower()][k] for k in ('sha256','bytes')} for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},'engine_rearm':False}
 preservation['single_gpu_rearm']={'schema':1,'service':'MT8167RenderSingleGpu','maximum_rearms':1,'diagnostic_policies':['d64c9165297bb602ac04be1d9053998c2f6f8da3581545b6be8b005d237fcdb5','6b00fa9f57696f19ad10ecdace497d0a3b1b4bcb861e98d7a8571e0ce23f19c3','10f8ec77e44a42c25a01c17434adb9c4bc80f180c142173725846d7735015c64','b6273b09f165e3e5fcc61d49bc8b874008f9350d30592737c71e31a2999e905f','bf84654ab50707ca2d660f685663cf4dd8b1c216921e862a7289340c5056ebe0','9947ede87028cd2d8100e443006cf516ed2c5e17e06229bb688943e2746dbc4b','a66ac828fe1c6838f4de2a8ce766313b53a9d48682463a456727166c5bb39bea','71440bc09ce1caced543d8bf1a67a05db0f6f5b45363f60aa58fc9fdfbe0c6d2','4f585721d5ab2e7bedbf61b75e4d76d88ba1f0f549281de6b418a50dfe120643'],'data_directory':DATA_DIRECTORY,'partial_diagnostic_policies':['10f8ec77e44a42c25a01c17434adb9c4bc80f180c142173725846d7735015c64','bf84654ab50707ca2d660f685663cf4dd8b1c216921e862a7289340c5056ebe0','9947ede87028cd2d8100e443006cf516ed2c5e17e06229bb688943e2746dbc4b','a66ac828fe1c6838f4de2a8ce766313b53a9d48682463a456727166c5bb39bea','71440bc09ce1caced543d8bf1a67a05db0f6f5b45363f60aa58fc9fdfbe0c6d2']}
 preservation['single_gpu_rearm']['diagnostic_policies'].append('6bf9d947ba3c09b11e7d330086be64d3368b4cda15a6dda2d6749faf3a723e85')
 preservation['single_gpu_rearm'].update(previous_service_start=4,previous_service_fingerprint='534322f90c44b5b7dc4b091fa6f1a309bd8415bd7786db39bdee4e85df2fb4d4',previous_ledger_fingerprint='0518d9c1220d6877595bd0769b74854b83b404c723fae415e667d4821c20faee',previous_policy='6bf9d947ba3c09b11e7d330086be64d3368b4cda15a6dda2d6749faf3a723e85')

 base=selection['render'];base.update(driver_version='0.0.3.23',sys_name='ge8300-render.sys',sys_sha256=BASE_RENDER_SYS,inf=base['selected']['InfPath'])
 retained={k:v for k,v in selection.items() if k!='render'}
 retainedfiles={**RETAINED_FILES,**{TARGETS[k][2]+'.sys':v for k,v in BASE_SYS.items() if k!='render'}};retainedfiles.pop('ge8300-umd.dll');retainedfiles.pop('ge8300-render.sys')
 return {'schema':1,'profile':PROFILE,'default_off':True,'data_directory':DATA_DIRECTORY,'software_trust_policy':'preserve-existing; require WinVerifyTrust/CAT membership before single-GPU install; no SOFTWARE writes','backup_path':backup.relative_to(root).as_posix(),'backup_manifest_sha256':FRESH,'binding_receipt_sha256':sha(receipt),'baselines':{'render':base},'packages':{'render':{'directory':pkg.relative_to(root).as_posix(),'files':{n:hashes[n] for n in names},'version_u64':version('0.0.3.25'),'date_filetime':datevalue('11/07/2026'),'source_commit':SOURCE,'signed_manifest_sha256':PACKAGE}},'client':{'directory':nd.relative_to(root).as_posix(),'source_commit':n['client_source_commit'],'sha256':nh['ge8300-d3d-clear.exe'],'manifest_sha256':NATIVE,'arguments':'--hardware=PowerVR','timeout_ms':120000,'retry_count':0,'pipeline':'10_0','limited_development_profile':True,'standard_profile_qualified':False},'comparator':{'directory':compdir.relative_to(root).as_posix(),'manifest_sha256':sha(cm),**cp},'previous_service':previous_service,'retained_selection':retained,'retained_files':retainedfiles,'baseline_umd_sha256':BASE_UMD,'engine_preservation':preservation,'boot':{'source_commit':bm['uefi_source_commit'],'sha256':bm['boot_img_sha256'],'manifest_sha256':sha(boot),'action':'preserve; no boot or GPT writes'},'pnp_install_count':1,'maximum_planned_reboot_count':1,'client_count':1,'audio_retest':False,'normal_install_allowed':True,'sequence':['stage exact render candidate and dedicated single-GPU service','one render DIF_INSTALLDEVICE; no live device restart','maximum one planned reboot','exact render/retained verification and one GPU client; stop']}
def generate_paths(p):
 require(p.get('profile')==PROFILE and p.get('data_directory')==DATA_DIRECTORY,'Exact generated service profile/directory required')
 require(p.get('engine_preservation',{}).get('single_gpu_rearm',{}).get('data_directory')==DATA_DIRECTORY,'Native rearm directory differs from runtime')
 return '#ifndef A04_RENDER_SERVICE_PATH_POLICY_H\n#define A04_RENDER_SERVICE_PATH_POLICY_H\n#define A04_RENDER_DATA_DIRECTORY L"'+DATA_DIRECTORY+'"\n#define A04_RENDER_POLICY_PROFILE "'+PROFILE+'"\n#endif\n'
def assert_service_paths(image,p):
 generate_paths(p);require_armnt(Path(image));b=Path(image).read_bytes();want=p['data_directory'].encode('utf-16le')+b'\0\0'
 token=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest().encode()+b'\0'
 require(token in b,'SCM compiled policy token mismatch')
 require(want in b,'SCM EXE does not contain exact generated UTF16 data directory')
 old=set(re.findall(rb'M\x00T\x008\x001\x006\x007\x00R\x00e\x00n\x00d\x00e\x00r\x00S\x00i\x00n\x00g\x00l\x00e\x00G\x00p\x00u\x00R\x00u\x00n\x00(?:[0-9]\x00)+',b))
 require(old=={p['data_directory'].encode('utf-16le')},'SCM contains an old generation directory')
 return {'data_directory':p['data_directory'],'profile':p['profile'],'utf16_directory_verified':True,'old_generation_directory_absent':True}

def generate(p):
 require(re.fullmatch('[0-9a-f]{64}',FRESH),'run115後の最新capture未結合: draft header発行不可')
 require(p.get('profile')==PROFILE and set(p['packages'])=={'render'},'single GPU profile only')
 generate_paths(p)
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
 (out/'render-service-path-policy.h').write_text(generate_paths(p));(out/'render-deferred-policy.h').write_text(generate(p));(out/'render-policy-manifest.json').write_text(json.dumps(p,indent=2)+'\n');(out/'engine-preservation-plan.json').write_text(json.dumps(p['engine_preservation'],indent=2)+'\n')
if __name__=='__main__':main()
