#!/usr/bin/env python3
"""既存署名候補を集約する。fresh120未結合ではservice/header/hive/mediaを生成しない。"""
import argparse,hashlib,json,shutil,struct
from pathlib import Path
SIGNER='BCBF6C456C6F0DD0818434763B189488FBF13570'
USB_SOURCE='cb4b5ae7b7dc7ad49951e3e178fd5f658364c01d'
USB_SYS='4d547ed45597395088c89bd52de2ffd83180322b753c9ab30842966cc50fee31'
USB_MANIFEST='3ee0f2a9785607f13873eca836aee66297ef2a4998c140b86860656f0bc91f7e'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def require(ok,msg):
 if not ok:raise ValueError(msg)
def arm(p):
 b=p.read_bytes();require(b[:2]==b'MZ' and struct.unpack_from('<H',b,struct.unpack_from('<I',b,60)[0]+4)[0]==0x1c4,'ARMNT required')
def main():
 a=argparse.ArgumentParser();a.add_argument('--workspace',required=True,type=Path);a.add_argument('--out',required=True,type=Path);o=a.parse_args();r=o.workspace.resolve(strict=True);out=o.out.resolve();require(out.is_relative_to(r) and not out.exists(),'新規root内出力必須')
 wf=r/'port/xnu/out/windows-rt-peripheral-wifi1020-sd8-prep/packet/candidate-overlay.json';w=read(wf)
 require(w['source_commit']=='52e715198b211681906f32acf53b8d0a1587118d' and w['draft'] is True and w['normal_install_allowed'] is False,'Fixed WiFi1020/SD8 draft only')
 packages={}
 for role,source,sys in [('wifi','9f1c03b0e12093d38642f18fa2ccd71e36a5aa0c','8137670089c6e69cd3dc1e1e05415269e013573f0af8c640c4d358b22aae8b5c'),('sd','4bb60761edf75c79581c879b4b61f1badaaefbd7','ab152989bb6752a3a80d267a1f1775b8e333c2299c3fea247a692aa675cb65d9')]:
  p=w['packages'][role];require(p['source_commit']==source and p['sys_sha256']==sys,'Fixed signed candidate identity only');d=r/p['directory'];require(sha(d/'signed-manifest.json')==p['manifest_sha256'],'Signed manifest changed')
  for kind,name in p['names'].items():require(sha(d/name)==p['hashes'][kind],'Candidate byte changed')
  arm(d/p['names']['sys']);packages[role]=p
 d=r/'port/xnu/out/windows-rt-usb-composite1010/package';require(sha(d/'signed-manifest.json')==USB_MANIFEST,'USB1010 fixed signed manifest required');u=read(d/'signed-manifest.json');require(u['source_commit']==USB_SOURCE and u['signature_status']=='Valid' and u['signing_thumbprint']==SIGNER,'USB1010 signer/source mismatch')
 for f in u['files']:require(sha(d/f['path'])==f['sha256'],'USB signed member changed')
 require(sha(d/'mt8167-usb20.sys')==USB_SYS,'USB1010 SYS mismatch');arm(d/'mt8167-usb20.sys')
 packages={'usb':{'directory':str(d.relative_to(r)),'source_commit':USB_SOURCE,'manifest_sha256':USB_MANIFEST,'sys_sha256':USB_SYS,'hardware_id':'MT8167\\USB20','service':'mshidkmdf','class_guid':'{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}','driver_version':'2026.11.8.1010','driver_date':'11/08/2026','names':{k:'mt8167-usb20.'+ext for k,ext in [('inf','inf'),('sys','sys'),('cat','cat')]},'hashes':{k:sha(d/('mt8167-usb20.'+k)) for k in ('inf','sys','cat')},'hardware_verified':False},**packages}
 client=r/'port/xnu/out/windows-rt-capture-error22-20261108/package';cm=client/'signed-client-manifest.json';require(sha(cm)=='25373f54f26098e91b3a5911eaabcc8954d340820c9e123ba12558a50f2c1c26','Fixed buffer-first capture client required');c=read(cm)
 exe=client/'mt8167-capture-probe.exe';require(sha(exe)=='974518d0377995c74421e0b1000eea87dec30e1b2ea354a12cd98182da1db1cf' and c['signer']==SIGNER and c['audio_sys_sha256']=='4facb36eed0b45edf32ae234ec3afa325ea1a9bb58884caa7042139a72121699','Capture client/audio tuple mismatch');arm(exe)
 draft={'schema':1,'profile':'peripheral-usb1010-wifi1020-sd8-next-v1','draft':True,'normal_install_allowed':False,'fresh':None,'required_capture_number':120,'required_main_manifest_hash':None,'required_recovery_boot_id':None,'required_current_gpu_policy':None,'native_snapshot':None,'packages':packages,'planned_reboots':1,'install_targets':['usb','wifi','sd'],'same_candidate_reinstall_allowed':False,'unchanged_targets_reinstall_allowed':False,'stage_before_dif':list(packages),'preserve_from_same_capture':['GPU_SYS','UMD','client','FW','Engine_EXE','RenderSingleGpu_EXE','all_old_service_roots_and_ledgers','parent1101_or_newer_main_selected','MMA1.10','eMMC1.9','RTC','thermal','matched_8range_or_newer_main_selected_boot'],'boot_action':'preserve current matched boot; never downgrade to fixed old image','gpu_install':False,'engine_retest':False,'software_or_userfs_rollback':False,'diagnostic_clients':{'capture':{'path':str(exe.relative_to(r)),'sha256':sha(exe),'manifest_sha256':sha(cm),'source_commit':'61106acf7ae7cfd7831f09dc0a37ead841221d73','audio_sys_sha256':c['audio_sys_sha256'],'state_order':['STOP-buffer','ACQUIRE','PAUSE','RUN','STOP'],'launch_allowed_before_final_profile':False,'real_capture_success':False}},'optional_candidates':{'Carson_DOD_rotation':{'included':False,'reason':'signed candidate pins not supplied yet'},'ACCDET_GPIO19':{'included':False,'source_commit':'dd1928f2','reason':'optional parent integration; no parent replacement without exact current flags/pins'}},'service_created':False,'media_created':False,'hardware_verified':False,'remaining_source_work':['finalize next dedicated service identity and exact baseline service root after fresh120','generalize runtime targets/install mask without reinstalling retained drivers','independent CM status before strict selected metadata guards; preserve API/type/byte failures','bind current native snapshot, original trio, GPU policy and matched boot to main-approved manifest']}
 out.mkdir(parents=True);(out/'draft.json').write_text(json.dumps(draft,indent=2)+'\n')
 for role,p in packages.items():
  dest=out/'packages'/role;dest.mkdir(parents=True)
  for name in set(p['names'].values())|{'signed-manifest.json'}:shutil.copyfile(r/p['directory']/name,dest/name)
 dest=out/'clients';dest.mkdir();shutil.copyfile(exe,dest/exe.name);shutil.copyfile(cm,dest/cm.name)
 (out/'pins.json').write_text(json.dumps({str(p.relative_to(out)):sha(p) for p in sorted(out.rglob('*')) if p.is_file()},indent=2)+'\n')
 print('DRAFT_ONLY_SIGNATURE_BYTES_REUSED_NO_SERVICE_HIVE_MEDIA')
if __name__=='__main__':main()
