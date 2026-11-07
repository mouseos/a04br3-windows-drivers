"""既成native SYSTEMのfresh94→96限定再利用。元receipt/実行観測は書き換えない。"""
import hashlib,json
from pathlib import Path
OLD='4ff327d3fba781c377dc98615424daa1d90d0df305fac375ae639342589d817c'
NEW='f0fab046a19c5cd5cacf16e3dbad324f0cfadcbb24b5acf48839a3fee591e0df'
CANDIDATE='6e8b1fa642a130462c2299b5ec08f7703b084f999adc4ff3f2c66288e9b37d22'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def pinned(root,entry):
 p=(root/entry['path']).resolve(strict=True)
 if not p.is_relative_to(root) or sha(p)!=entry['sha256']:raise ValueError('native再利用正本pin不一致')
 return p

def verify(root,proof_entry,new_preservation):
 root=root.resolve(strict=True);pp=pinned(root,proof_entry);p=read(pp)
 if p.get('schema')!=1 or p.get('kind')!='fresh94-96-identical-system-trio-native-reuse':raise ValueError('固定native再利用型だけ許可')
 old=pinned(root,p['old_fresh']);new=pinned(root,p['new_fresh'])
 if sha(old)!=OLD or sha(new)!=NEW:raise ValueError('固定94→96だけ許可')
 before=read(old);after=read(new)
 for m in (before,after):
  if m.get('status')!='captured-unmounted' or m.get('errors')!=[] or not m.get('unmounted'):raise ValueError('完了capture必須')
 a={e['source_path']:e for e in before['files']};b={e['source_path']:e for e in after['files']}
 if set(a)!=set(b):raise ValueError('捕捉file集合の変更を拒否')
 diff=[]
 for name in a:
  paths=[]
  for manifest,entry in ((old,a[name]),(new,b[name])):
   f=(manifest.parent/entry['backup_path']).resolve(strict=True)
   if not f.is_relative_to(manifest.parent) or f.stat().st_size!=entry['bytes'] or sha(f)!=entry['sha256']:raise ValueError('capture実bytes不一致')
   paths.append(f)
  if a[name]['sha256']!=b[name]['sha256'] or a[name]['bytes']!=b[name]['bytes']:
   if name!='Windows/System32/config/SOFTWARE.LOG1' or a[name]['bytes']!=565248 or b[name]['bytes']!=196608 or paths[0].read_bytes()[:196608]!=paths[1].read_bytes():raise ValueError('既知SOFTWARE.LOG1 prefix縮小以外は拒否')
   diff.append(name)
 if diff!=['Windows/System32/config/SOFTWARE.LOG1']:raise ValueError('既知94→96差分だけ許可')
 candidate=pinned(root,p['candidate']);reportpath=pinned(root,p['original_native_result']);report=read(reportpath)
 if sha(candidate)!=CANDIDATE or report['candidate']['sha256']!=CANDIDATE or report.get('status')!='verified-clean-system-services-only' or report.get('services')!=['MT8167RenderSingleGpu']:raise ValueError('既成singleGPU native候補不一致')
 original_plan=pinned(root,p['original_engine_plan']);op=read(original_plan)
 if sha(original_plan)!=report.get('engine_preservation_plan_sha256'):raise ValueError('元native/保持plan chain不一致')
 # Only capture identity/profile changes; SYSTEM source bytes and ledger policy set do not.
 stripped=lambda x:{k:v for k,v in x.items() if k not in ('profile','backup_manifest_sha256')}
 if op['backup_manifest_sha256']!=OLD or new_preservation['backup_manifest_sha256']!=NEW or stripped(op)!=stripped(new_preservation):raise ValueError('SYSTEM/Engine保持契約の相違を拒否')
 for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2'):
  e=b['Windows/System32/config/'+n]
  if new_preservation['source_files'][n]!={k:e[k] for k in ('sha256','bytes')} or report['source_files'][n]!=new_preservation['source_files'][n]:raise ValueError('native入力SYSTEM trio全byte同一必須')
 return {'kind':p['kind'],'proof_sha256':sha(pp),'original_native_result_sha256':sha(reportpath),'candidate_sha256':CANDIDATE,'old_manifest_sha256':OLD,'new_manifest_sha256':NEW,'system_trio_identical':True,'no_native_reload':True,'only_changed_file':diff[0],'software_log1_retained_prefix_bytes':196608,'software_log1_prefix_difference_bytes':0,'existing_engine_ledger_preserved':True}
