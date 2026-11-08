#!/usr/bin/env python3
"""Validate fresh134 and emit an explicitly non-armable run135 receipt draft."""
import argparse,hashlib,json
from pathlib import Path
FRESH='9d4b078f989a589166ad7ef53a211251628e6d32a93c8c66fe4858233f29bee2'
BOOT='3d353a0c-b6e9-4697-957d-aa2b8b82cbe7'
BASELINE='aacf10f27fc52ac9b773afba39a32c861dc7873b33c0e2a276c6b6dfb9586008'
RTCBOOT='4b67b67aedc4beb9863bfe1791de8713f925d8eedc6dfc702ea66df82895927b'
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--native-baseline',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();root=a.workspace.resolve(strict=True)
 def path(n):
  p=(root/n).resolve(strict=True)
  if not p.is_relative_to(root):raise ValueError('Root-local input required')
  return p
 def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
 def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
 f=path('port/xnu/out/private-windows-rt-prebackup-run134/manifest.json');m=read(f)
 assert sha(f)==FRESH and m['recovery_boot_id']==BOOT and m['status']=='captured-unmounted' and m['read_only'] and m['unmounted'] and not m['errors'] and len(m['files'])==35
 files={e['source_path']:e for e in m['files']};assert len(files)==35
 for e in files.values():p=path(f.parent/e['backup_path']);assert p.stat().st_size==e['bytes'] and sha(p)==e['sha256']
 b=path('port/gsi/logs/ios-feasibility/windows-rt-fresh134-baseline-client-files.json');bm=read(b);assert sha(b)==BASELINE and bm['fresh_manifest_sha256']==FRESH and len(bm['files'])==bm['baseline_count']==47
 for name,want in bm['files'].items():assert '/' not in name and '\\' not in name and sha(path(Path(bm['source_directory'])/name))==want
 n=path(a.native_baseline);nr=read(n);assert nr['source_manifest_sha256']==FRESH and not nr['errors'] and nr['originals_unchanged'] and all(nr[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error'))
 expected={'MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','Mt6392Keys'}
 assert set(nr['old_services'])==expected and len(nr['nodes'])==13 and nr['old_gpu_service']['Start']['value']==4
 gpu=nr['nodes']['ACPI\\MOU0006']['instances'];assert len(gpu)==1 and gpu[0]['selected']['DriverVersion']['value']=='0.0.3.27'
 boot=path('port/xnu/out/mt6392-gettime-contract-20261108/firmware-ordered/boot/boot.img');assert sha(boot)==RTCBOOT
 out=a.out.resolve();assert out.is_relative_to(root) and not out.exists()
 d={'schema':1,'profile':'combined-gpu332-fresh134-v1','service':'MT8167CombinedFollowup134','data_directory':'MT8167CombinedFollowup134','status':'awaiting-signed-sealed-332-package','draft':True,'normal_install_allowed':False,'hardware_owner':'main','hardware_operations_allowed':False,'device_verified':False,'fresh':{'path':str(f.relative_to(root)),'sha256':FRESH,'recovery_boot_id':BOOT,'verified_original_files':35},'native_baseline':{'path':str(n.relative_to(root)),'sha256':sha(n)},'service_preservation':nr['old_services'],'install_targets':['render'],'install_mask':1024,'retain_no_dif':['rtc','thermal','usb','wifi','sd','display','parent','keypad','battery','audio'],'planned_reboots':1,'boot_transition':{'boot_path':str(boot.relative_to(root)),'boot_sha256':RTCBOOT,'boot_write_required':False,'gpt_written':False},'gpu_client_log_baseline':{'path':str(b.relative_to(root)),'sha256':BASELINE,'file_count':47},'candidate_gpu332':None,'new_gpu_client_count':1,'old_gpu_client_execute':False,'peripheral_clients_first':True,'gpu_suite_last':True,'provisional_phase_bounds':{'install_seconds':900,'diagnostic_seconds':1500,'gpu_client_seconds':900,'must_confirm_with_332_source':True},'blockers':['Exact332 signed manifest and full member pins','Main independent seal for332','332 exact SYS/UMD/private contract/client schema and bound','Generated final policy/header/SCM signature','Fresh134 new-clone native service arm','Final media/WIM/ESP and independent main contract seal']}
 out.write_text(json.dumps(d,indent=2)+'\n');print('RUN135_DRAFT_ONLY_NOT_ARMABLE_ALL134_INPUTS_VERIFIED')
if __name__=='__main__':main()
