#!/usr/bin/env python3
"""既存captureを変更せず、同Recovery bootで固定GPU保持3fileだけを補足する。"""
import argparse,importlib.util,json,shlex,re
from pathlib import Path
spec=importlib.util.spec_from_file_location('prebackup',Path(__file__).with_name('capture-windows-prebackup.py'))
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
EXTRA=('Windows/System32/MT8167RenderSingleGpu.exe','Windows/System32/ge8300-umd.dll','Windows/System32/Drivers/ge8300-mips.fw')
DXGI='Windows/System32/dxgi.dll'
def gpu_extra_specs(runtime_assets=False,include_system_dxgi=False):
 if include_system_dxgi and not runtime_assets:raise ValueError('dxgi logical copy requires --gpu-runtime-assets')
 result=[(n,True) for n in EXTRA]
 if runtime_assets:result += [(n,True) for n in ('Windows/System32/mt8167-dxgi-console-launch363.exe','Windows/System32/mt8167-dxgi-present356.exe')]
 if include_system_dxgi:result.append((DXGI,False))
 return result

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--manifest',type=Path,required=True);ap.add_argument('--manifest-sha256',required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--ntfs3g-remote',default='/tmp/rt-ntfs3g');ap.add_argument('--gpu-runtime-assets',action='store_true',help='既Present363/356と固定GPUログdirも同Recoveryで補足');ap.add_argument('--include-system-dxgi',action='store_true',help='--gpu-runtime-assets時のみ固定System32/dxgi.dllをoptional logical copy/hash。欠落は明示し成功捏造しない');a=ap.parse_args();extras=gpu_extra_specs(a.gpu_runtime_assets,a.include_system_dxgi)
 mf=c.inside_workspace(a.manifest,True);out=c.inside_workspace(a.out);assert not out.exists(),'新規root内出力のみ'
 assert c.SHA256.fullmatch(a.manifest_sha256) and c.sha(mf)==a.manifest_sha256,'原capture pin不一致'
 m=json.loads(mf.read_text());assert m['status']=='captured-unmounted' and m['read_only'] is True and m['unmounted'] is True and m['errors']==[],'完了captureだけ'
 r=c.Recovery(m['serial']);identity=r.identity();assert all(identity[k]==m[k] for k in identity),'同boot/capture出自必須';r.require_unmounted(identity)
 original_names={e['source_path'] for e in m['files']}
 extras=[(n,required) for n,required in extras if n not in original_names]
 extra=[n for n,_ in extras]
 out.mkdir(mode=0o700);mount='/tmp/rt-peripheral-'+__import__('uuid').uuid4().hex;made=False;mounted=False
 result={'schema':1,'source_manifest_sha256':a.manifest_sha256,'recovery_boot_id':identity['recovery_boot_id'],'status':'failed','read_only':True,'unmounted':False,'errors':[],'files':[]}
 try:
  r.shell('mkdir '+mount);made=True;r.shell('test -x '+shlex.quote(a.ntfs3g_remote));mounted=True
  r.shell(shlex.quote(a.ntfs3g_remote)+' -o ro,norecover,nosuid,nodev,noexec '+c.BY_NAME+' '+mount);r.require_own_ro_mount(mount,identity)
  logs=[]
  if a.gpu_runtime_assets:
   base=mount+'/MT8167D3DClear'
   for path in r.shell('find '+base+' -maxdepth 1 -type f -name "*.log"').splitlines():
    assert path.startswith(base+'/') and re.fullmatch(r'(?:clear|umd)-[A-Za-z0-9_.-]+\.log',path[len(base)+1:]),'固定GPUログ名以外は拒否'
    logs.append(path[len(mount)+1:])
   assert len(logs)==len(set(logs)) and len(logs)<=128,'GPUログ件数の上限/重複'
   extra+=sorted(logs);extras += [(n,True) for n in sorted(logs)]
  specs=[(e['source_path'],True) for e in m['files']]+extras;assert len(specs)==len({x[0].lower() for x in specs}),'補足の重複禁止'
  before=r.snapshot(mount,specs)
  missing=[n for n in extra if before[n] is None]
  if missing:result['missing_files']=[{'source_path':n,'reason':'absent','optional':True} for n in missing]
  present_extra=[n for n in extra if before[n] is not None]
  if a.include_system_dxgi:result['dxgi_logical_copy']={'source_path':DXGI,'present':before[DXGI] is not None,'physical_read_performed':False,'loaded_image_proven':False}
  assert sum(before[n]['bytes'] for n in present_extra)<=64*1024*1024,'補足総量の上限'
  for e in m['files']:assert before[e['source_path']]=={k:e[k] for k in ('sha256','bytes')},'原capture byte差分'
  for n in present_extra:
   local=out/'files'/n;local.parent.mkdir(parents=True,exist_ok=True);r.adb('pull',mount+'/'+n,str(local));assert not local.is_symlink() and (local.stat().st_size>0 or n in logs) and local.stat().st_size<=16*1024*1024 and c.sha(local)==before[n]['sha256'] and local.stat().st_size==before[n]['bytes'],'補足byte不一致'
   local.chmod(0o600);result['files'].append({'source_path':n,'backup_path':str(local.relative_to(out)),**before[n]})
  assert r.snapshot(mount,specs)==before and r.identity()==identity and c.sha(mf)==a.manifest_sha256,'同boot/原capture不変必須'
 except Exception as error:result['errors'].append(str(error))
 finally:
  if mounted:
   try:r.shell('umount '+mount)
   except Exception as error:result['errors'].append(str(error))
  try:r.require_unmounted(identity);result['unmounted']=True
  except Exception as error:result['errors'].append(str(error))
  if made and result['unmounted']:
   try:r.shell('rmdir '+mount)
   except Exception as error:result['errors'].append(str(error))
 if not result['errors'] and result['unmounted'] and len(result['files'])==len(present_extra):result['status']='captured-unmounted'
 p=out/'manifest.json';p.write_text(json.dumps(result,indent=2)+'\n');p.chmod(0o600)
 print(json.dumps({'status':result['status'],'manifest_sha256':c.sha(p)}))
 if result['status']!='captured-unmounted':raise SystemExit(1)
 if a.gpu_runtime_assets:
  baseline=dict(source_manifest_sha256=a.manifest_sha256,recovery_boot_id=identity['recovery_boot_id'],files={Path(n).name:dict(path=(out/'files'/n).relative_to(c.WORKSPACE).as_posix(),sha256=before[n]['sha256']) for n in logs})
  (out/'mutable-baseline.json').write_text(json.dumps(baseline,indent=2)+'\n');(out/'mutable-baseline.json').chmod(0o600)
if __name__=='__main__':main()
