#!/usr/bin/env python3
"""Fixed file-scoped Engine phase evidence; no device access or marker changes."""
import argparse,hashlib,json,re
from pathlib import Path
STATE='Windows/System32/MT8167EngineProbe/'
MARKERS=('attempted.bin','deferred-pending.bin','diagnostic-attempted.bin','reboot-attempted.bin')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--backup',type=Path,required=True);ap.add_argument('--backup-sha256',required=True);ap.add_argument('--policy-hash',required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args()
 root=Path(__file__).resolve().parents[4];p=a.backup.resolve(strict=True);out=a.out.resolve()
 if not p.is_relative_to(root) or not out.is_relative_to(root) or out.exists() or sha(p)!=a.backup_sha256 or not re.fullmatch('[0-9a-f]{64}',a.policy_hash):raise ValueError('New root output and exact pins required')
 b=json.loads(p.read_text());assert b['status']=='captured-unmounted' and b['unmounted'] and b['read_only'] and not b['errors']
 entries={x['source_path'].replace('\\','/'):x for x in b['files']};missing={x['source_path'].replace('\\','/'):x for x in b.get('missing_files',[])}
 def read(name):
  item=entries.get(STATE+name)
  if not item:return None,{'available':False,'state':'captured-absent' if STATE+name in missing else 'not-captured'}
  f=(p.parent/item['backup_path']).resolve(strict=True)
  if not f.is_relative_to(p.parent.resolve()) or f.stat().st_size!=item['bytes'] or sha(f)!=item['sha256']:raise ValueError('Fixed state file hash/size mismatch')
  return f.read_bytes(),{'available':True,'bytes':item['bytes'],'sha256':item['sha256']}
 r={'scope':'fixed captured files only, no missing-state inference','backup_manifest_sha256':a.backup_sha256,'policy_hash':a.policy_hash,'markers':{},'runs':{}}
 for name in MARKERS:
  data,meta=read(name)
  if data is not None:meta['matches_expected_content']=data==(b'MT8167EngineProbe attempted\n' if name=='attempted.bin' else a.policy_hash.encode()+b'\0')
  r['markers'][name]=meta
 data,meta=read('logs/service.log');r['service_log']=meta
 if data is not None:
  for match in re.finditer(rb'^A04HW RUN=([0-9A-F]{8}) ID=([0-9A-F]{8}) EVT=([0-9A-F]{8}) CODE=([0-9A-F]{8})\r?$',data,re.M):
   run,idx,ev,code=[int(v,16) for v in match.groups()];entry=r['runs'].setdefault(f'{run:08X}',{'pnp_results':[],'target_install_results':[],'service_end':[],'flags':[]})
   row={'id':idx,'code':code,'code_hex':f'0x{code:08x}'}
   if ev==0x22:entry['pnp_results'].append(row)
   elif ev==0x105:entry['target_install_results'].append(row)
   elif ev==0x104:entry['flags'].append(row)
   elif ev==0x0a:entry['service_end'].append(row)
  for entry in r['runs'].values():entry['partial_failure_then_reboot_required']=any(x['code'] for x in entry['pnp_results']) and any(x['code']==3010 for x in entry['service_end'])
 out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(r,indent=2)+'\n');out.chmod(0o600)
 print(json.dumps({'status':'fixed-disk-state-decoded','sha256':sha(out)}))
if __name__=='__main__':main()
