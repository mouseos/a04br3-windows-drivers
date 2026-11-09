#!/usr/bin/env python3
"""保存済み診断ログのprefix/追記を観測する。実機I/O・導入判定・上書きはしない。"""
import argparse,hashlib,json,re
from pathlib import Path

def require(v,m):
 if not v:raise ValueError(m)
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def compare(root,baseline,baseline_sha256,collected):
 root=root.resolve(strict=True);baseline=baseline.resolve(strict=True);collected=collected.resolve(strict=True)
 require(baseline.is_relative_to(root) and collected.is_relative_to(root),'root-local archived inputs only')
 require(digest(baseline)==baseline_sha256,'Baseline archive manifest mismatch')
 b=json.loads(baseline.read_text(encoding='utf-8-sig'));source=(root/b['source_directory']).resolve(strict=True)
 require(source.is_relative_to(root) and len(b['files'])<=128,'Bounded root-local diagnostic archive required')
 rows=[]
 for name,h in b['files'].items():
  require(re.fullmatch(r'(clear|umd)-[A-Za-z0-9_.-]+\.log',name) is not None,'Fixed diagnostic log basename required')
  before=(source/name).resolve(strict=True);require(before.is_relative_to(source) and digest(before)==h,'Saved prefix archive changed')
  old=before.read_bytes();after=collected/name
  row={'name':name,'before_bytes':len(old),'before_sha256':h,'prefix_preserved':False}
  if not after.is_file():row['observation']='missing'
  else:
   require(not after.is_symlink() and after.resolve().is_relative_to(collected),'Diagnostic collection leaf must not redirect')
   new=after.read_bytes();row.update(after_bytes=len(new),after_sha256=hashlib.sha256(new).hexdigest(),prefix_preserved=new.startswith(old))
   row['observation']='unchanged' if old==new else 'appended' if row['prefix_preserved'] else 'truncated' if len(new)<len(old) else 'replaced-or-modified'
  rows.append(row)
 return {'schema':1,'scope':'mutable-diagnostic-log-observation-not-install-admission','source_manifest_sha256':b['source_fresh_manifest_sha256'],'baseline_sha256':baseline_sha256,'saved_prefixes_preserved':all(r['prefix_preserved'] for r in rows),'files':rows,'overwrite_or_delete':False,'hardware_operations':False}

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--workspace',type=Path,required=True);a.add_argument('--baseline',type=Path,required=True);a.add_argument('--baseline-sha256',required=True);a.add_argument('--collected',type=Path,required=True);a.add_argument('--out',type=Path,required=True);x=a.parse_args()
 root=x.workspace.resolve(strict=True);out=x.out.resolve();require(out.is_relative_to(root) and not out.exists(),'New root-local observation output required')
 result=compare(root,x.baseline,x.baseline_sha256,x.collected);out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'saved_prefixes_preserved':result['saved_prefixes_preserved'],'files':len(result['files']),'install_admission_changed':False}));raise SystemExit(0 if result['saved_prefixes_preserved'] else 2)
if __name__=='__main__':main()
