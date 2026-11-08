#!/usr/bin/env python3
"""署名済WiFi1020/SD8を検査・束ねる。媒体生成/署名/実機/registry操作なし。"""
import argparse,hashlib,json,re,shutil,subprocess
from pathlib import Path
SIGNER='BCBF6C456C6F0DD0818434763B189488FBF13570'
CANDIDATES={
 'wifi':{'directory':'port/xnu/out/windows-rt-wifi-connect-qos1020-9f1c03b0/package','source_commit':'9f1c03b0e12093d38642f18fa2ccd71e36a5aa0c','manifest_sha256':'027a4eee780141dbe6481d6b2dab461b119ce3c09cb47f5f2137d943b24670ac','sys_sha256':'8137670089c6e69cd3dc1e1e05415269e013573f0af8c640c4d358b22aae8b5c','stem':'mt6630-runtime','driver_version':'2026.11.8.1020','driver_date':'11/08/2026','hardware_id':'A04BR3\\MT6630SDIO','service':'Mt6630Runtime','class_guid':'{4D36E972-E325-11CE-BFC1-08002BE10318}','checkpoint_build':1020},
 'sd':{'directory':'port/xnu/out/windows-rt-sd-detect-epoch/package','source_commit':'4bb60761edf75c79581c879b4b61f1badaaefbd7','manifest_sha256':'511c390a4309617260d1ca30106d3d721ad8f525510f4a409339deb71f0d7f55','sys_sha256':'ab152989bb6752a3a80d267a1f1775b8e333c2299c3fea247a692aa675cb65d9','stem':'mt8167-sd','driver_version':'2026.11.8.8','driver_date':'11/08/2026','hardware_id':'ACPI\\MOU0013','service':'Mt8167Sd','class_guid':'{4D36E97B-E325-11CE-BFC1-08002BE10318}','checkpoint_build':8}}
RECEIPT='port/xnu/out/private-windows-rt-peripheral-run115-registry-post/batch-receipt.json'
FRESH116='port/xnu/out/private-windows-rt-prebackup-run116/manifest.json'
MAP115='port/xnu/out/windows-rt-peripheral-batch114/package-map.json'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def require(ok,msg):
 if not ok:raise ValueError(msg)
def rooted(root,path):
 p=(root/path).resolve();require(p.is_relative_to(root),'workspace外path禁止');return p

def prepare(root,out):
 repo=Path(__file__).resolve().parents[2]
 require(root==repo.parent.parent,'現在の作業rootのみ')
 require(out.is_relative_to(root) and not out.exists(),'新規root内出力のみ')
 head=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()
 require(not subprocess.check_output(['git','-C',str(repo),'status','--porcelain']), 'clean固定source必須')
 receipt=read(root/RECEIPT);fresh=read(root/FRESH116);mapping=read(root/MAP115)
 require(sha(root/FRESH116)==receipt['fresh_manifest_sha256']=='23bd8c5a2b2db021b24b7ef81c48f2ff6478b8ee21955db23dd329839c21e51d','run116 reference pin不一致')
 require(fresh['status']=='captured-unmounted' and fresh['unmounted'] and fresh['read_only'] and not fresh['errors'],'保存capture完了条件不一致')
 files={}
 for e in fresh['files']:
  p=rooted(root,Path(FRESH116).parent/e['backup_path']);require(p.stat().st_size==e['bytes'] and sha(p)==e['sha256'],'reference capture file不一致')
  require(e['source_path'] not in files,'capture重複path');files[e['source_path']]={k:e[k] for k in ('sha256','bytes')}
 for role,node in receipt['nodes'].items():
  require(files[node['sys_capture_path'].split('/files/',1)[1]]['sha256']==node['sys_sha256'],'適用済SYS/reference不一致')
 parent=receipt['nodes']['parent'];require(parent['selected']['DriverVersion']=='2026.11.8.1101' and parent['sys_sha256']=='398fd007079ab4710404d67a60dc7db46b84dd149ce3ae69e3bbc730264e823c','親1101を保持する基準不一致')
 bindings_path=rooted(root,receipt['bindings_seven']);require(sha(bindings_path)==receipt['bindings_seven_sha256'],'run115 native bindings pin不一致')
 bindings=read(bindings_path);require(bindings.get('original_unchanged') is True and not bindings['errors'] and all(bindings[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'run115 native reader終了条件不一致')
 wifi_rows=bindings['nodes'].get('A04BR3\\MT6630SDIO',[]);require(len(wifi_rows)==1 and wifi_rows[0]['selected']['DriverVersion']=='2026.11.6.1012' and wifi_rows[0]['Service']=='Mt6630Runtime','run115 WiFi実選択版不一致')
 require('ACPI\\MOU0013' not in bindings['nodes'],'reference SD読取り範囲が変化したため再確認が必要')
 flags=mapping['parent_flags']['candidate'];require(flags['A04_SHARED_SD_POWER']=='1' and flags['A04_SHARED_WIFI_RUNTIME']=='1','SD/WiFi provider有効sourceが必要')
 # Both consumers use the exact unchanged SD v1 ABI/provider already in 1101.
 for name in ('power_interface.h','power_parent.inc','power_owner.c'):
  rel='drivers/storage/mt8167-sd/'+name
  require(subprocess.check_output(['git','-C',str(repo),'show','9e52cc1c:'+rel])==subprocess.check_output(['git','-C',str(repo),'show','1d248a71:'+rel]),'親1101 SD契約source差異')
 boot=receipt['boot_applied'];require(boot['source_commit']=='6c96c6f60149f2b01d068909c8d0c308e5b4f3ee' and boot['sha256']=='ece47d799e04e794bffcf902d2e4ef32c7e898f60079eff98134467afda693d8','8range基準boot出自不一致')
 for k in ('boot','fd'):
  p=rooted(root,mapping['boot_transition'][k+'_path']);require(sha(p)==boot['sha256' if k=='boot' else 'fd_sha256'],'8range保存artifact不一致')
 packages={}
 for role,spec in CANDIDATES.items():
  d=root/spec['directory'];mf=d/'signed-manifest.json';m=read(mf)
  require(m['source_commit']==spec['source_commit'] and m.get('signed') is True and m.get('signing_thumbprint')==SIGNER,'署名source/identity不一致')
  if 'manifest_sha256' in spec:require(sha(mf)==spec['manifest_sha256'],'manifest pin不一致')
  for e in m['files']:
   require(Path(e['path']).name==e['path'],'manifest path不正');require(sha(d/e['path'])==e['sha256'].lower(),'署名package member hash不一致')
  names={k:spec['stem']+'.'+k for k in ('inf','sys','cat')};require(sha(d/names['sys'])==spec['sys_sha256'],'選択SYS不一致')
  inf=(d/names['inf']).read_text(encoding='utf-8-sig')
  require(re.search(r'DriverVer\s*=\s*'+re.escape(spec['driver_date']+','+spec['driver_version']),inf,re.I),'INF日付/版不一致')
  require(spec['hardware_id'].lower() in inf.lower() and spec['service'].lower() in inf.lower(),'INF HWID/service不一致')
  packages[role]={**spec,'signed_manifest_sha256':sha(mf),'names':names,'hashes':{k:sha(d/v) for k,v in names.items()},'signature_previously_verified':True,'hardware_verified':False}
 require(files['Windows/System32/Drivers/mt6630-runtime.sys']['sha256']=='5044f8b47b5e89383923c36af8c5a29e21bf1224885b42ede55155a59fce4351','reference WiFi1012不一致')
 require(files['Windows/System32/Drivers/mt8167-sd.sys']['sha256']=='f52a2b07f7c89f2cf8014bf64d797cc28e8aeb0d3a5aba63c4c2c64777c5bf3a','reference SD7不一致')
 replaced=['Windows/System32/Drivers/mt6630-runtime.sys','Windows/System32/Drivers/mt8167-sd.sys']
 required_preserve={n:e for n,e in files.items() if n not in replaced and ('/Drivers/' in n or n.endswith(('.exe','.dll','.fw')))}
 result={'schema':1,'profile':'wifi1020-sd8-candidates-for-next-peripheral','source_commit':head,'candidate_packet_only':True,'draft':True,'normal_install_allowed':False,'media_created':False,'service_created':False,'rebuild_required':{'wifi':False,'sd':False,'parent':False,'uefi':False},'packages':packages,
  'run115_reference':{'receipt':RECEIPT,'receipt_sha256':sha(root/RECEIPT),'fresh_manifest':FRESH116,'fresh_sha256':sha(root/FRESH116),'current_gpu_trial_baseline':False,'binding_coverage_missing':['ACPI\\MOU0013'],'observed_wifi_version':'2026.11.6.1012','observed_sd_sys_matches_signed_sd7':True,'diagnostic_result':receipt['diagnostic_result'],'diagnostic_guard_limit_retained':True},
  'preservation_reference':{'parent_version':parent['selected']['DriverVersion'],'parent_sys_sha256':parent['sys_sha256'],'eight_range_boot_reference':boot,'boot_reflash_requested':False,'gpt_change_requested':False,'kernel_boot_changes_requested':False,'files':required_preserve,'parent_flags':flags},
  'policy_delta':{'new_install_targets':['wifi','sd'],'new_stage_order':['wifi','sd'],'remove_only_from_preserved_files':replaced,'keep_existing_six_roles':['rtc','thermal','usb','mma','emmc','parent'],'planned_restart_limit':1,'old_fixed_six_policy_reusable':False,'main_fresh_after_gpu_required':True,'native_wifi_sd_selected_binding_required':True,'preserve_current_gpu_and_current_matched_boot':True,'live_cm_status_before_identity_guards':True,'separate_selected_and_sys_errors':True,'strict_identity_pins_must_remain':True,'eeprom_nvram_mac_cid_firmware_repack_requested':False},
  'pending':['main-selected post-GPU fresh capture and current GPU policy','native SD and WiFi binding snapshot from that same capture','next peripheral service/header/map consumes both roles and preserves latest GPU/boot','single final service build/sign after all peripheral changes are selected']}
 out.mkdir(parents=True)
 for role,spec in CANDIDATES.items():shutil.copytree(root/spec['directory'],out/'packages'/role)
 (out/'candidate-overlay.json').write_text(json.dumps(result,indent=2)+'\n')
 pins={'schema':1,'source_commit':head,'overlay_sha256':sha(out/'candidate-overlay.json'),'files':{p.relative_to(out).as_posix():sha(p) for p in sorted((out/'packages').rglob('*')) if p.is_file()},'hardware_verified':False}
 (out/'pins.json').write_text(json.dumps(pins,indent=2)+'\n')
 print(json.dumps({'source':head,'wifi_sys':packages['wifi']['sys_sha256'],'sd_sys':packages['sd']['sys_sha256'],'rebuild':False,'normal_install_allowed':False}))

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();prepare(a.workspace.resolve(strict=True),a.out.resolve())
if __name__=='__main__':main()
