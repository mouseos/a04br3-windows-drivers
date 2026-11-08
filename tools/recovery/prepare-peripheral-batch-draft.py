#!/usr/bin/env python3
"""既存署名成果物だけを結合したdraftを作る。fresh・旧service・boot handoffは発火前にmainが結合。"""
import argparse,hashlib,json,sys,zipfile
from pathlib import Path,PurePosixPath
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'winpe'))
from peripheral_batch_policy import PROFILE,TARGETS,PRESERVED_SERVICES,BOOT_SOURCE,BOOT_SHA,FD_SHA,packages,require,sha

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--workspace',required=True,type=Path);ap.add_argument('--out',required=True,type=Path);a=ap.parse_args();root=a.workspace.resolve(strict=True);out=a.out.resolve();require(out.is_relative_to(root) and not out.exists(),'新しいroot内出力だけを許可');out.mkdir(parents=True)
 archive=root/'port/xnu/out/windows-rt-parent1101-rtc-thermal-tuple/parent1101-rtc5-thermal17-post.zip';require(sha(archive)=='65b0e38c4d2748b635ab6b39116780656428d5178c782148a69764d4ca10dc27','Carson tuple ZIP pin不一致')
 carson=out/'carson'
 with zipfile.ZipFile(archive) as z:
  members=json.loads(z.read('members.json'));require(set(z.namelist())==set(members)|{'members.json'},'tuple ZIP member schema不一致')
  for n in z.namelist():
   p=PurePosixPath(n);require(not p.is_absolute() and '..' not in p.parts,'ZIP path不正');data=z.read(n)
   if n in members:require(hashlib.sha256(data).hexdigest()==members[n]['sha256'] and len(data)==members[n]['bytes'],'tuple member byte不一致')
   dest=carson/str(p);dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
 cert=root/'port/xnu/out/windows-rt-mma-panel180-110/package/mt8167-development.cer'
 spec={}
 for role,t in TARGETS.items():
  if role in ('rtc','thermal','parent'):
   d=carson/'package'/role;mf=carson/'windows-evidence/signed-manifest.json';member='package/'+role+'/';inf='shared-parent-rtc-thermal1101.inf' if role=='parent' else t[3]
  else:
   d=root/('port/xnu/out/windows-rt-mma-panel180-110/package' if role=='mma' else ('port/xnu/out/windows-rt-usb-host-convergence/package' if role=='usb' else 'port/xnu/out/windows-rt-msdc-ajtd4r-request-mdl/package'));mf=d/'signed-manifest.json';member='';inf=t[3]
  m=json.loads(mf.read_text(encoding='utf-8-sig'));files={k:{'name':n,'sha256':sha(d/n),'manifest_member':member+n} for k,n in (('inf',inf),('sys',t[4]),('cat',t[5]))}
  spec[role]={'directory':str(d.relative_to(root)),'manifest':str(mf.relative_to(root)),'manifest_sha256':sha(mf),'source_commit':m['source_commit'],'files':files,'certificate':{'path':str(cert.relative_to(root)),'sha256':sha(cert)}}
 validated=packages(spec,root)
 draft={'schema':1,'profile':PROFILE,'draft':True,'fresh':None,'packages':spec,'parent_flags':{'baseline':None,'candidate':validated['parent']['parent_flags'],'added':{'MT6392_RTC_CALENDAR':'1','A04_SHARED_THERMAL_DIAGNOSTIC':'1','A04_SHARED_THERMAL_NATIVE_EXPERIMENT':'1','A04_SHARED_GPU_THERMAL_PROFILE':'1'}},'preserved_files':[],'service_preservation':{n:None for n in PRESERVED_SERVICES},'boot_transition':{'source_commit':BOOT_SOURCE,'boot_path':str((carson/'firmware/boot/boot.img').relative_to(root)),'boot_sha256':BOOT_SHA,'fd_path':str((carson/'firmware/SILICIUM_UEFI.fd').relative_to(root)),'fd_sha256':FD_SHA,'mode':'main-controlled-fastboot-handoff','next_windows_boot_blocked_until_matched_boot':False,'handoff_receipt':None,'handoff_receipt_sha256':None}}
 (out/'package-map-DRAFT-UNPINNED.json').write_text(json.dumps(draft,indent=2)+'\n');(out/'candidate-validation.json').write_text(json.dumps({'candidate_packages':validated,'normal_install_allowed':False,'fresh_bound':False,'media_built':False,'device_access':False},indent=2)+'\n')
if __name__=='__main__':main()
