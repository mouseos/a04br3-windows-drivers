#!/usr/bin/env python3
"""run95: fresh94のrender3.13だけを3.14へ。親/Audio/eMMC/USB/WiFi/DODは保持。"""
import argparse,hashlib,json,re,struct,subprocess
from pathlib import Path
from render_profile_policy import require,rooted,jsonfile,hexpin,sha,cbytes,SIGNER
from codec_msdc_transaction import TARGETS,RETAINED,RETAINED_FILES,version,datevalue
from codec_returned_baseline import validate_selection,load_boot
from engine_io_package import require_armnt
PROFILE='render-single-gpu314-run97-v1'
FRESH='f0fab046a19c5cd5cacf16e3dbad324f0cfadcbb24b5acf48839a3fee591e0df'
SOURCE='f4f1b4f94ade6f59efef042f680c2c3e363442f8'
PACKAGE='9ef5d9bfe307f977902b2233bea83c28f92a2971110b4efed1fa1492a55eaab7'
NATIVE='f19d1d1d4f483791eaec494abcecf1964594093c82eca999a35b7a3a249adf95'
UMD='4c0e3ca3564033b2cf11b10f8f8e50cca6b5c79ce6a76b7768a22ab7623c5304'
SYS=RETAINED_FILES['ge8300-render.sys'];FW=RETAINED_FILES['ge8300-mips.fw']
BASE_SYS={'parent':'f176836aafb34447b423398d301ffa01d9787f3bc197c8c29f371145a6e756e1','audio':'4facb36eed0b45edf32ae234ec3afa325ea1a9bb58884caa7042139a72121699','emmc':'f92ee2d4e058e345539567153e0b4a20b13669f34f84de017dece2e45707dadd','render':SYS}
def outputs(directory,m,wanted):
 rows=m['outputs'];require(len(rows)==len(wanted) and {x['name'] for x in rows}==set(wanted),'固定outputs schema不一致')
 for x in rows:require(sha(directory/x['name'])==hexpin(x['sha256']),'署名output byte不一致')
 return {n:sha(directory/n) for n in wanted}
def load_map(path,root):
 root=root.resolve(strict=True);s=jsonfile(rooted(root,path))
 require(set(s)=={'schema','profile','backup','backup_sha256','bindings','packages','native','boot','comparator','previous_engine_media','native_reuse'},'singleGPU専用input fields必須')
 require(s['schema']==1 and s['profile']==PROFILE and s['backup_sha256']==FRESH,'固定run95/fresh94必須')
 backup=rooted(root,s['backup']);require(sha(backup)==FRESH,'fresh94実hash不一致');b=jsonfile(backup)
 require(b.get('status')=='captured-unmounted' and b.get('unmounted') is True and b.get('errors')==[],'fresh94完了原本必須')
 files={}
 for e in b['files']:
  f=rooted(root,backup.parent/e['backup_path']);require(f.is_relative_to(backup.parent) and sha(f)==e['sha256'] and f.stat().st_size==e['bytes'],'fresh94実file不一致');files[e['source_path'].replace('\\','/').lower()]=e
 receipt=rooted(root,s['bindings']['path']);require(sha(receipt)==hexpin(s['bindings']['sha256']),'native seven-node pin不一致')
 binding=jsonfile(receipt);derivation=binding.get('binding_derivation',{})
 require(derivation.get('actual_native_read_manifest_sha256')=='4ff327d3fba781c377dc98615424daa1d90d0df305fac375ae639342589d817c' and derivation.get('identical_system_trio') is True and derivation.get('native_load_repeated') is False,'派生native receiptの由来必須')
 from single_gpu_native_reuse import pinned as reuse_pinned,read as reuse_read
 original_binding=reuse_read(reuse_pinned(root,derivation['original_receipt']))
 require(original_binding['nodes']==binding['nodes'] and original_binding['source_files']==binding['source_files'] and all(original_binding.get(k)==binding.get(k) for k in ('load_rc','unload_rc','privilege_restore_error','errors')),'元native観測値の変更を拒否')
 selection=validate_selection(binding,b,FRESH,TARGETS,RETAINED,observe_only=True)
 for role,expected in BASE_SYS.items():require(files['windows/system32/drivers/'+TARGETS[role][2]+'.sys']['sha256']==expected,'現在SYS不一致')
 for role,t in RETAINED.items():require(files['windows/system32/drivers/'+t[3]]['sha256']==t[4],'保持SYS不一致')
 require(set(s['packages'])=={'render'},'更新packageはrender1件だけ')
 pkg=rooted(root,s['packages']['render']['directory']);mf=pkg/'signed-manifest.json';m=jsonfile(mf)
 require(s['packages']['render']['manifest_sha256']==PACKAGE and sha(mf)==PACKAGE,'正式3.14 manifest pin必須')
 require(m.get('package_source_commit')==SOURCE and m.get('binary_source_commit')==SOURCE and m.get('kernel_binary_source_commit')=='9f31ddcf89fbe6d5c3a7b5abcceadb223212d378' and m.get('baseline_manifest_sha256')=='9ef6d89b900c089e754fa57f4a8ceaa557f4c7d22b69e4e0bbea9666b3c0cb11' and m.get('native_manifest_sha256')==NATIVE,'new explicit 3.14 source schema不一致')
 require(m.get('signature_status')=='Valid' and m.get('signer_thumbprint')==SIGNER and m.get('kernel_changed') is False and m.get('firmware_changed') is False and m.get('limited_development_profile') is True and m.get('standard_profile_qualified') is False and m.get('development_negotiation_pipeline')=='10_0' and m.get('inf_driverver')=='11/07/2026,0.0.3.14','3.14保持/limited契約不一致')
 names=['ge8300-render.inf','ge8300-render.sys','ge8300-render.cat','ge8300-mips.fw','ge8300-umd.dll']
 hashes=outputs(pkg,m,names+['mt8167-development.cer']);require(hashes[names[1]]==SYS and hashes[names[3]]==FW and hashes[names[4]]==UMD,'SYS/FW保持・UMD正式pin不一致')
 require(hashlib.sha1((pkg/'mt8167-development.cer').read_bytes()).hexdigest().upper()==SIGNER,'cert不一致')
 inf=(pkg/names[0]).read_text(encoding='utf-16');require('DriverVer=11/07/2026,0.0.3.14' in inf and 'ACPI\\MOU0006' in inf and 'HKR,,UserModeDriverName,0x00010000,ge8300-umd.dll,ge8300-umd.dll,ge8300-umd.dll' in inf,'3.14 INF/service/third slot不一致')
 nd=rooted(root,s['native']['directory']);nm=nd/'signed-manifest.json';n=jsonfile(nm)
 require(s['native']['manifest_sha256']==NATIVE and sha(nm)==NATIVE and sha(pkg/'native-signed-manifest.json')==NATIVE,'native manifest chain不一致')
 require(n.get('source_commit')==SOURCE and n.get('signature_status')=='Valid' and n.get('signer_thumbprint')==SIGNER and n.get('pipeline')=='10_0' and n.get('limited_development_profile') is True and n.get('standard_profile_qualified') is False and n.get('kernel_changed') is False and n.get('fixed_arguments')=='--hardware=PowerVR' and n.get('retry_count')==0 and n.get('external_timeout_ms')==120000,'FL10限定client契約不一致')
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
 from single_gpu_native_reuse import verify
 reuse=verify(root,s['native_reuse'],preservation)
 base=selection['render'];base.update(driver_version='0.0.3.13',sys_name='ge8300-render.sys',sys_sha256=SYS,inf=base['selected']['InfPath'])
 retained={k:v for k,v in selection.items() if k!='render'}
 retainedfiles={**RETAINED_FILES,**{TARGETS[k][2]+'.sys':v for k,v in BASE_SYS.items() if k!='render'}};retainedfiles.pop('ge8300-umd.dll')
 return {'schema':1,'profile':PROFILE,'default_off':True,'native_reuse':reuse,'software_trust_policy':'preserve-existing; require WinVerifyTrust/CAT membership before single-GPU install; no SOFTWARE writes','backup_path':backup.relative_to(root).as_posix(),'backup_manifest_sha256':FRESH,'binding_receipt_sha256':sha(receipt),'baselines':{'render':base},'packages':{'render':{'directory':pkg.relative_to(root).as_posix(),'files':{n:hashes[n] for n in names},'version_u64':version('0.0.3.14'),'date_filetime':datevalue('11/07/2026'),'source_commit':SOURCE,'signed_manifest_sha256':PACKAGE}},'client':{'directory':nd.relative_to(root).as_posix(),'source_commit':SOURCE,'sha256':nh['ge8300-d3d-clear.exe'],'manifest_sha256':NATIVE,'arguments':'--hardware=PowerVR','timeout_ms':120000,'retry_count':0,'pipeline':'10_0','limited_development_profile':True,'standard_profile_qualified':False},'comparator':{'directory':compdir.relative_to(root).as_posix(),'manifest_sha256':sha(cm),**cp},'retained_selection':retained,'retained_files':retainedfiles,'baseline_umd_sha256':RETAINED_FILES['ge8300-umd.dll'],'engine_preservation':preservation,'boot':{'source_commit':bm['uefi_source_commit'],'sha256':bm['boot_img_sha256'],'manifest_sha256':sha(boot),'action':'preserve; no boot or GPT writes'},'pnp_install_count':1,'maximum_planned_reboot_count':1,'client_count':1,'audio_retest':False,'normal_install_allowed':True,'sequence':['stage exact render candidate and dedicated single-GPU service','one render DIF_INSTALLDEVICE; no live device restart','maximum one planned reboot','exact render/retained verification and one GPU client; stop']}
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
