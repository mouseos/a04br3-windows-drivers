#!/usr/bin/env python3
"""実fresh200/native読取りreceiptから名前集合と件数だけを固定。snapshotは作らない。"""
import argparse,json,hashlib
from pathlib import Path

def main():
 p=argparse.ArgumentParser();p.add_argument('--capture-number',type=int,default=200);p.add_argument('--gpu369-timing',action='store_true')
 for n in ['workspace','fresh','native','out']:p.add_argument('--'+n,type=Path,required=True)
 for n in ['fresh-sha256','native-sha256']:p.add_argument('--'+n,required=True)
 p.add_argument('--supplement',type=Path);p.add_argument('--supplement-sha256');a=p.parse_args();root=a.workspace.resolve(strict=True)
 assert (204<=a.capture_number<=999) if a.gpu369_timing else a.capture_number in (200,202)
 def load(path,h):
  q=path.resolve(strict=True);assert q.is_relative_to(root);b=q.read_bytes();assert hashlib.sha256(b).hexdigest()==h;return json.loads(b.decode('utf-8-sig'))
 f=load(a.fresh,a.fresh_sha256);n=load(a.native,a.native_sha256)
 assert a.fresh.as_posix().endswith(f'private-windows-rt-prebackup-run{a.capture_number}/manifest.json')
 assert f['status']=='captured-unmounted' and f['read_only'] is True and f['unmounted'] is True and f['errors']==[]
 assert n['source_manifest_sha256']==a.fresh_sha256 and n['errors']==[] and n['originals_unchanged'] is True and all(n[k]==0 for k in ['load_rc','unload_rc','privilege_restore_error'])
 assert n['new_service']['present'] is False and n['new_service_name']==f'MT8167CombinedFollowup{a.capture_number}' and n['capture_number']==a.capture_number
 files={x['source_path']:x for x in f['files']};assert len(files)==len(f['files'])
 if a.supplement:
  x=load(a.supplement,a.supplement_sha256);assert x['source_manifest_sha256']==a.fresh_sha256 and x['recovery_boot_id']==f['recovery_boot_id'] and x['status']=='captured-unmounted' and x['errors']==[] and x['read_only'] is True and x['unmounted'] is True
  for row in x['files']:
   if row['source_path'] in files:assert row['sha256']==files[row['source_path']]['sha256'] and row['bytes']==files[row['source_path']]['bytes']
   files[row['source_path']]=row
 result=dict(schema=f'GPU{369 if a.gpu369_timing else 368}_FRESH{a.capture_number}_CAPTURE_CONTRACT_V1',capture_number=a.capture_number,fresh_manifest_sha256=a.fresh_sha256,native_receipt_sha256=a.native_sha256,recovery_boot_id=f['recovery_boot_id'],file_count=len(files),service_names=sorted(n['old_services']),named_value_ids=sorted(n['captured_named_values']),snapshot_synthesized=False)
 out=a.out.resolve();assert out.is_relative_to(root) and not out.exists();out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,indent=2)+'\n')
 print(f'ACTUAL_FRESH{a.capture_number}_INVENTORY_SEALED_NO_SNAPSHOT_CREATED')
if __name__=='__main__':main()
