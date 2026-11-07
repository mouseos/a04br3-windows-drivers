#!/usr/bin/env python3
"""次回eMMC更新用の表示名だけを準備する。CAT/署名/媒体/実機は変更しない。"""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
BASE_MANIFEST='9678bc72f8d99c733fac9bef391d75cbb31d8afe8733e3f504493fd1412be8c1'
EVIDENCE='cadb1622ff6bff471037342f77b73080881752516e653473c9361032df3248ee'
SYS='f92ee2d4e058e345539567153e0b4a20b13669f34f84de017dece2e45707dadd'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser()
 for k in ('workspace','package','evidence','output'):ap.add_argument('--'+k,type=Path,required=True)
 a=ap.parse_args();root=a.workspace.resolve(strict=True);repo=Path(__file__).resolve().parents[2]
 if subprocess.check_output(['git','-C',str(repo),'status','--porcelain']):raise ValueError('Committed clean metadata source required')
 pkg=a.package.resolve(strict=True);evidence=a.evidence.resolve(strict=True);out=a.output.resolve()
 if any(not p.is_relative_to(root) or p==root for p in (pkg,evidence,out)) or out.exists():raise ValueError('root内入力と新出力が必要')
 if sha(pkg/'signed-manifest.json')!=BASE_MANIFEST or sha(evidence)!=EVIDENCE:raise ValueError('固定署名quiet1.8/実採取根拠pin不一致')
 m=json.loads((pkg/'signed-manifest.json').read_text(encoding='utf-8-sig'))
 if m['source_commit']!='e2db926f1ddd12b4cae341a7df50f832e20c16e5' or m['source_dirty'] or not m['logs_only_change']:raise ValueError('quiet1.8出自不一致')
 for f in m['files']:
  p=(pkg/f['path']).resolve(strict=True)
  if not p.is_relative_to(pkg) or sha(p)!=f['sha256'].lower():raise ValueError('署名package file不一致')
 if sha(pkg/'mt8167-msdc.sys')!=SYS:raise ValueError('現eMMC SYS byte不一致')
 values=evidence.read_text().splitlines()
 if values!=['/sys/class/mmc_host/mmc0/mmc0:0001/name','AJTD4R','/sys/class/mmc_host/mmc0/mmc0:0001/manfid','0x000015','/sys/class/mmc_host/mmc0/mmc0:0001/oemid','0x0100']:raise ValueError('実eMMC name/manfid/oemid不一致')
 linux=root/'CT3_GPL_LGPL_Src/kernel-4.4/drivers/mmc/card/block.c'
 if not re.search(r'^#define\s+CID_MANFID_SAMSUNG\s+0x15\s*$',linux.read_text(),re.M):raise ValueError('既存Linuxのmanufacturer対応不一致')
 raw=(pkg/'mt8167-msdc.inf').read_bytes();following=raw
 edits={'Provider':('TAB-A04-BR3 development','mouseos'),'DeviceDescription':('MediaTek MT8167 MSDC eMMC Host Controller','MediaTek MT8167 MSDC0 / Samsung AJTD4R eMMC'),'ServiceDescription':('MediaTek MT8167 MSDC eMMC Host Controller','MediaTek MT8167 MSDC0 / Samsung AJTD4R eMMC'),'DiskName':('MediaTek MT8167 MSDC eMMC Driver','mouseos MT8167 MSDC0 eMMC Driver')}
 for k,(before,after) in edits.items():
  old=(k+' = "'+before+'"').encode();new=(k+' = "'+after+'"').encode()
  if following.count(old)!=1:raise ValueError('固定Strings field不一致: '+k)
  following=following.replace(old,new)
 restored=following
 for k,(before,after) in edits.items():restored=restored.replace((k+' = "'+after+'"').encode(),(k+' = "'+before+'"').encode())
 if restored!=raw:raise ValueError('命名以外のINF差分を拒否')
 out.mkdir(mode=0o700);template=out/'mt8167-msdc.inf.in';template.write_bytes(following)
 result={'status':'metadata-template-only-not-installable','source_commit':subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD']).decode().strip(),'baseline_manifest_sha256':BASE_MANIFEST,'baseline_sys_sha256':SYS,'evidence_path':str(evidence),'evidence_sha256':EVIDENCE,'observed_product_name':'AJTD4R','observed_manfid':'0x15','observed_oemid':'0x0100','manufacturer_mapping_source':str(linux),'manufacturer_mapping_source_sha256':sha(linux),'manufacturer':'Samsung','marketing_part_number':None,'template_sha256':sha(template),'changed_string_fields':edits,'inf_non_string_bytes_preserved':True,'service_hardwareid_start_group_preserved':True,'baseline_driverver_retained_in_template':'11/07/2026,0.0.1.8','future_install_requires_version_rollforward':True,'future_install_requires_new_cat_signature_membership':True,'signed_sys_rebuild_or_resign_required':False,'signed_package_modified':False,'media_generated':False,'hardware_operations':False}
 (out/'metadata-plan.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
 print('AJTD4R_METADATA_TEMPLATE_PREPARED; NO_PACKAGE_OR_MEDIA_CHANGED')
if __name__=='__main__':main()
