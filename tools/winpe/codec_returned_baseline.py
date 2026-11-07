"""返却済み最新captureを呼出側が明示する。探索・旧freshへのfallback・実機操作はしない。"""
import json,re
from pathlib import Path
from private_hardware_probe_input import sha
BOOT_SOURCE='e3b18c0ff1e35dfae54f00c54b6ae2203313c4d3'
BOOT_IMAGE='9dc9f7f104280e8e9b9b51082de596035c173451c0b39229479a0196340882eb'
# Actual pre-peripheral returns, never eligible as post-run87 inputs.
STALE_MANIFESTS={'4330f1c4532b80f86dae0b8516d99555bec4f40c0aa1efe4963e722c4629a325','3d4f1421201ce65940d64659024721168437b2b5e10568fb1373092e0765f622','b7427b65c58beebfdae768de00518f0dd46dec769f9d03ebc0f3e06bbfae9cf2'}
CLASSES={'parent':'4d36e97d','audio':'4d36e96c','emmc':'4d36e97b','usb':'745a17a0','wifi':'4d36e972','display':'4d36e968','render':'4d36e968'}
def pinned_file(entry,root):
 if set(entry)!={'path','sha256'} or not re.fullmatch('[0-9a-f]{64}',entry['sha256'] or ''):raise ValueError('明示path/full SHA256の対が必要です')
 p=Path(entry['path']).resolve(strict=True)
 if not p.is_relative_to(root.resolve()) or not p.is_file() or sha(p)!=entry['sha256']:raise ValueError('返却input path/hash不一致')
 return p

def validate_selection(receipt,record,manifest_sha,targets,retained):
 from codec_msdc_transaction import version
 if (receipt.get('source_manifest_sha256')!=manifest_sha or receipt.get('errors')!=[]
     or any(type(receipt.get(k)) is not int or receipt[k]!=0 for k in ('load_rc','unload_rc','privilege_restore_error'))):raise ValueError('native選択metadataのmanifest/load/unload/privilege不一致')
 inputs={x['source_path'].replace('\\','/'):x for x in record['files']}
 pins=receipt.get('source_files',{})
 for name in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2'):
  e=inputs['Windows/System32/config/'+name]
  if pins.get(name)!={'sha256':e['sha256'].lower(),'bytes':e['bytes']}:raise ValueError('native選択metadataのSYSTEM/LOG由来不一致')
 rows=[row for group in receipt['nodes'].values() for row in group];selected={}
 for role in (*targets,*retained):
  hwid=targets[role][0] if role in targets else retained[role][0]
  service=targets[role][3] if role in targets else retained[role][1]
  matches=[x for x in rows if hwid.casefold() in [h.casefold() for h in x.get('HardwareID',[])]]
  if len(matches)!=1:raise ValueError('選択metadataで一意の実hardware IDが必要です: '+hwid)
  row=matches[0];p=row['selected'];v=version(p['DriverVersion'])
  if row.get('Service','').casefold()!=service.casefold():raise ValueError('実service不一致: '+hwid)
  guid='{745a17a0-74d3-11d0-b6fe-00a0c90f57da}' if role=='usb' else '{'+CLASSES[role]+'-e325-11ce-bfc1-08002be10318}'
  if row.get('ClassGUID','').casefold()!=guid:raise ValueError('実device class不一致: '+hwid)
  if not re.fullmatch('[A-Za-z0-9_.-]+[.]inf',p.get('InfPath',''),re.I):raise ValueError('実選択INF basenameが必要です')
  raw=p.get('DriverDateData','')
  if not re.fullmatch('[0-9a-fA-F]{16}',raw):raise ValueError('native FILETIMEの8byte実値が必要です')
  date=int.from_bytes(bytes.fromhex(raw),'little')
  if not date:raise ValueError('選択dateが不明です')
  if role in targets:
   if v>=version(targets[role][4]):raise ValueError('更新対象は実選択より高版が必要です')
  elif v!=version(retained[role][2]):raise ValueError('保持版不一致: '+hwid)
  selected[role]={'hardware_id':hwid,'service':service,'version_u64':v,'date_filetime':date,'selected':p,'class_guid':row['ClassGUID']}
 return selected

def load_returned(spec,root,targets,retained):
 from codec_msdc_transaction import BASELINES,RETAINED_FILES
 if not spec['backup_manifest'] or not re.fullmatch('[0-9a-f]{64}',spec['backup_manifest_sha256'] or ''):raise ValueError('返却最新manifestを明示path/hashで指定してください')
 backup=pinned_file({'path':spec['backup_manifest'],'sha256':spec['backup_manifest_sha256']},root)
 if sha(backup) in STALE_MANIFESTS or backup.parent.name in {'private-windows-rt-prebackup-run82','private-windows-rt-prebackup-run84','private-windows-rt-prebackup-run86'}:raise ValueError('旧fresh82/84/86は再利用禁止です')
 import importlib.util
 s=importlib.util.spec_from_file_location('codec_backup_validator',Path(__file__).with_name('build-mt8167-shared-io-update-media.py'));module=importlib.util.module_from_spec(s);s.loader.exec_module(module)
 record=module.backup_check(backup,root)
 if record.get('unmounted') is not True or record.get('read_only') is not True or record.get('errors')!=[]:raise ValueError('完了したread-only/unmounted captureだけ結合できます')
 bindings=pinned_file(spec['bindings'],root);receipt=json.loads(bindings.read_text(encoding='utf-8-sig'))
 selection=validate_selection(receipt,record,sha(backup),targets,retained)
 boot,bm=load_boot(spec['boot'],root)
 return backup,record,selection,{'manifest_sha256':sha(boot),'source_commit':BOOT_SOURCE,'image_sha256':BOOT_IMAGE,'action':'preserve; never flashed by codec transaction'},sha(bindings)

def load_boot(entry,root):
 boot=pinned_file(entry,root);bm=json.loads(boot.read_text(encoding='utf-8-sig'))
 image=(boot.parent/'boot.img').resolve(strict=True)
 if not image.is_relative_to(boot.parent):raise ValueError('boot image path逸脱')
 if bm.get('uefi_source_commit')!=BOOT_SOURCE or bm.get('boot_img_sha256')!=BOOT_IMAGE or sha(image)!=BOOT_IMAGE:raise ValueError('保持POST boot source/image出自不一致')
 return boot,bm
