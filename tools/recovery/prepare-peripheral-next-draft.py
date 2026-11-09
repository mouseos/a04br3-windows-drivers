#!/usr/bin/env python3
"""既存署名7package/ACCDET clientを固定draftに集約。main capture122未結合は発火不可。"""
import argparse,hashlib,json,re,shutil,sys
from pathlib import Path
sys.dont_write_bytecode=True;sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'winpe'))
from peripheral_next_policy import PROFILE,ROLES,PINS,TARGETS,SERVICE
from peripheral_batch_policy import inftext,require,read,sha

def main():
 p=argparse.ArgumentParser();p.add_argument('--workspace',type=Path,required=True);p.add_argument('--out',type=Path,required=True);x=p.parse_args();r=x.workspace.resolve(strict=True);out=x.out.resolve();require(out.is_relative_to(r) and not out.exists(),'New root-local draft output required')
 old=read(r/'port/xnu/out/windows-rt-peripheral-batch114/package-map.json');wf=read(r/'port/xnu/out/windows-rt-peripheral-wifi1020-sd8-prep/packet/candidate-overlay.json');spec={}
 for role in ('rtc','thermal'):
  q=old['packages'][role];spec[role]={'directory':q['directory'],'manifest':q['manifest'],'source_commit':q['source_commit'],'names':{k:v['name'] for k,v in q['files'].items()}}
 for role in ('wifi','sd'):
  q=wf['packages'][role];spec[role]={'directory':q['directory'],'manifest':q['directory']+'/signed-manifest.json','source_commit':q['source_commit'],'names':q['names']}
 d='port/xnu/out/windows-rt-usb-composite1010/package';spec['usb']={'directory':d,'manifest':d+'/signed-manifest.json','source_commit':'cb4b5ae7b7dc7ad49951e3e178fd5f658364c01d','names':{k:'mt8167-usb20.'+k for k in ('inf','sys','cat')}}
 d='port/xnu/out/mt8167-display-rotation4-20261108/package';spec['display']={'directory':d,'manifest':d+'/signed-manifest.json','source_commit':'2d94bcc89b08773624d105b6e5b24e2eea21de64','names':{k:'mt8167-display.'+k for k in ('inf','sys','cat')}}
 d='port/xnu/out/windows-rt-mt8167-accdet1102-signed/package';spec['parent']={'directory':d+'/parent','manifest':d+'/signed-manifest.json','source_commit':'4e253a5c5f53ccdc34c8aaa92296986d53f72d6a','names':{'inf':'mt8167-shared-accdet1102.inf','sys':'mt8167-shared.sys','cat':'mt8167-shared-accdet.cat'}}
 result={}
 for role in ROLES:
  q=spec[role];m=read(r/q['manifest']);q['manifest_sha256']=sha(r/q['manifest']);require(q['manifest_sha256']==PINS[role][0] and sha(r/q['directory']/q['names']['sys'])==PINS[role][1],'Fixed signed tuple required')
  text=inftext(r/q['directory']/q['names']['inf']);v=re.search(r'(?im)^DriverVer\s*=\s*([^\r\n;]+)',text);require(v and TARGETS[role][0].lower() in text.lower(),'Fixed INF HWID/version required');q['driver_date'],q['driver_version']=v[1].split(',');result[role]=q
 manifest=d+'/signed-manifest.json';client=d+'/client/mt8167-accdet-read.exe';require(sha(r/client)=='56650bc0d0339564dfdfd3287153a8b782a92fb127a54e29ffb9654a2921b85f','Signed ACCDET client required')
 draft={'schema':1,'profile':PROFILE,'draft':True,'fresh':None,'native':None,'seven':None,'approved_capture':None,'gpu_seal':None,'gpu_media':None,'packages':result,'capture_client':None,'rotation_client':{'directory':'port/xnu/out/mt8167-display-rotation-probe-20261108/package','manifest':'port/xnu/out/mt8167-display-rotation-probe-20261108/package/signed-manifest.json','manifest_sha256':'30a6d4bafae4ff518a2b192c6922b49a3394dc6695da7e4e12a3e707e95d34dc','exe':'mt8167-display-rotation-probe.exe','sha256':'0455f9b0f026aae287198dd1f5f9c8269cc6167bc44f90c94ad1938c55dfca6b'},'accdet_client':{'directory':d+'/client','manifest':manifest,'manifest_sha256':PINS['parent'][0],'exe':'mt8167-accdet-read.exe','sha256':sha(r/client)},'runtime_retained':None,'boot':{'path':'port/xnu/out/mt8167-thermal-stage1-readback-20261108/firmware/boot/manifest.json','sha256':sha(r/'port/xnu/out/mt8167-thermal-stage1-readback-20261108/firmware/boot/manifest.json')}}
 out.mkdir(parents=True);(out/'package-map-DRAFT.json').write_text(json.dumps(draft,indent=2)+'\n')
 for role,q in result.items():
  target=out/'packages'/role;target.mkdir(parents=True)
  for name in q['names'].values():shutil.copyfile(r/q['directory']/name,target/name)
 (out/'packet-state.json').write_text(json.dumps({'profile':PROFILE,'service':SERVICE,'draft':True,'normal_install_allowed':False,'required_main_capture_number':122,'stage_order':list(ROLES),'install_targets':list(ROLES[2:]),'planned_reboots':1,'source_draft_only':True,'service_created':False,'native_arm_created':False,'media_created':False,'retained_emmc':'AJTD4R1.9; no benchmark','pending':'fresh122 approval/native/seven/currentGPU plus capture-client signature and DOD API client receipt'},indent=2)+'\n')
 (out/'pins.json').write_text(json.dumps({str(p.relative_to(out)):sha(p) for p in sorted(out.rglob('*')) if p.is_file()},indent=2)+'\n');print('SEVEN_SIGNED_PACKAGES_COLLECTED_DRAFT_NO_ARM_MEDIA')
if __name__=='__main__':main()
