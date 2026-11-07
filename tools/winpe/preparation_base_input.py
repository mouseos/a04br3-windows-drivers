"""既存originalと、明示pinした3f19af59 quiet派生だけを選択する。書込みなし。"""
import hashlib,json
from pathlib import Path
ORIGINAL_WIM='0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'
ORIGINAL_ESP='22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce'
QUIET_MANIFEST='be3bdabed03f50065719d5c99d06e77d2f663b0df44fc2f5ba406ed1bbfb140c'
QUIET_WIM='dee7bf6400f61c291ec3b9f7cb34c7b2ad3fb928da381c5559496956628ccb07'
QUIET_ESP='0e23cdb1ba9bb7650f684f7d96ec26d371cb2496cd9e2de58fbfb7d94e49b4b4'
RECEIPT='f659cdbf86555774f93434dc6983865e6ff137d379153364e80368d59aa47460'
PACKAGE='11c68361ebc405d81a1b32820813964f6471880787fd863a2327a777b6a73fcd'
SIGNER='BCBF6C456C6F0DD0818434763B189488FBF13570'
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def select_preparation_base(base,root,manifest=None,expected_manifest_sha256=None,membership_receipt=None,private=False):
 root=Path(root).resolve(strict=True);base=Path(base).resolve(strict=True)
 if not base.is_relative_to(root) or base==root:raise ValueError('baseは作業root内に限定')
 extras=(manifest,expected_manifest_sha256,membership_receipt)
 if not any(extras):
  if sha(base/'maintenance-staged.wim')!=ORIGINAL_WIM or sha(base/'maintenance-staged-esp.img')!=ORIGINAL_ESP:raise ValueError('original base不一致。quiet派生には明示manifest/member receiptが必要')
  return {'kind':'original-fixed','wim_sha256':ORIGINAL_WIM,'esp_sha256':ORIGINAL_ESP}
 if not private or not all(extras):raise ValueError('quiet派生baseにはprivate媒体と全pin引数が必要')
 manifest=Path(manifest).resolve(strict=True);receipt=Path(membership_receipt).resolve(strict=True)
 if manifest.parent!=base or manifest.name!='manifest.json' or not receipt.is_relative_to(root):raise ValueError('quiet manifest/base/receipt配置不一致')
 if expected_manifest_sha256!=QUIET_MANIFEST or sha(manifest)!=QUIET_MANIFEST or sha(receipt)!=RECEIPT:raise ValueError('固定quiet manifest/member receipt SHA256不一致')
 m=read(manifest);v=read(receipt)
 required={'schema':1,'kind':'fixed-preparation-winpe-msdc-quiet-base','source_commit':'3f19af59514e87521ac27dedd28d0894c5983ff5','source_dirty':False,'parent_wim_sha256':ORIGINAL_WIM,'parent_esp_sha256':ORIGINAL_ESP,'wim_sha256':QUIET_WIM,'esp_sha256':QUIET_ESP,'package_source_commit':'91ee3ce6f773f97d8c6964df1a73e2283ec610b4','package_manifest_sha256':PACKAGE,'membership_receipt_sha256':RECEIPT,'signer':SIGNER,'private_do_not_distribute':True,'existing_certificate_stores_preserved':True,'unrelated_wim_records_preserved':True,'all_other_esp_files_preserved':True,'frozen_inputs_preserved':True,'system_hive_modified':False,'driver_logic_modified':False,'dma_timeout_clock_acpi_modified':False}
 if any(m.get(k)!=x for k,x in required.items()):raise ValueError('quiet派生型/出自/保持契約不一致')
 if (v.get('schema')!=1 or v.get('status')!='verified' or v.get('package_manifest_sha256')!=PACKAGE or v.get('package_source_commit')!=required['package_source_commit'] or v.get('signer')!=SIGNER or set(v.get('catalog_members',[]))!={'mt8167-msdc.sys','a04-msdc-probe.inf'} or set(v.get('signatures',[]))!={'mt8167-msdc.sys','mt8167-msdc.cat','observe-msdc-storage.exe'} or v.get('verification_recipe_sha256')!=sha(Path(__file__).parents[1]/'ci/verify-msdc-winpe-quiet.ps1') or {e['path']:e['sha256'] for e in v['files']}!=m['flat_driver_files']):raise ValueError('WDK署名/member receipt内容不一致')
 if sha(base/'maintenance-staged.wim')!=QUIET_WIM or sha(base/'maintenance-staged-esp.img')!=QUIET_ESP:raise ValueError('quiet派生WIM/ESP byte不一致')
 return {'kind':m['kind'],'source_commit':m['source_commit'],'manifest_sha256':QUIET_MANIFEST,'wim_sha256':QUIET_WIM,'esp_sha256':QUIET_ESP,'package_manifest_sha256':PACKAGE,'membership_receipt_sha256':RECEIPT,'flat_driver_files':m['flat_driver_files'],'signer':SIGNER,'hardware_verified':False,'performance_change_measured':False}
