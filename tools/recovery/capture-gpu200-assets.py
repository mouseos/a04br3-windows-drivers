#!/usr/bin/env python3
"""fresh200を変更せず、同Recoveryで保持assetと旧GPUログを実回収する。"""
from pathlib import Path
import argparse,importlib.util,json,re,shlex,uuid

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--manifest',type=Path,required=True);ap.add_argument('--manifest-sha256',required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args()
 spec=importlib.util.spec_from_file_location('prebackup',Path(__file__).with_name('capture-windows-prebackup.py'));c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
 root=Path.cwd().resolve();mf=c.inside_workspace(a.manifest,True);out=c.inside_workspace(a.out);assert not out.exists() and c.sha(mf)==a.manifest_sha256
 m=json.loads(mf.read_text());assert m['status']=='captured-unmounted' and m['read_only'] and m['unmounted'] and not m['errors']
 r=c.Recovery(m['serial']);identity=r.identity();assert all(identity[k]==m[k] for k in identity);r.require_unmounted(identity)
 extra=['Windows/System32/MT8167RenderSingleGpu.exe','Windows/System32/ge8300-umd.dll','Windows/System32/Drivers/ge8300-mips.fw','Windows/System32/mt8167-dxgi-console-launch363.exe','Windows/System32/mt8167-dxgi-present356.exe']
 assert not {x['source_path'].lower() for x in m['files']} & {x.lower() for x in extra}
 out.mkdir(mode=0o700);mount='/tmp/rt-gpu200-'+uuid.uuid4().hex;made=False;mounted=False
 result=dict(schema=1,source_manifest_sha256=a.manifest_sha256,recovery_boot_id=identity['recovery_boot_id'],status='failed',read_only=True,unmounted=False,errors=[],files=[])
 mutable=dict(source_manifest_sha256=a.manifest_sha256,recovery_boot_id=identity['recovery_boot_id'],files={})
 try:
  r.shell('mkdir '+mount);made=True;mounted=True;r.shell('/tmp/rt-ntfs3g -o ro,norecover,nosuid,nodev,noexec '+c.BY_NAME+' '+mount);r.require_own_ro_mount(mount,identity)
  names=r.shell('find '+mount+'/MT8167D3DClear -maxdepth 1 -type f').splitlines();assert len(names)<=128
  logs=[]
  for n in names:
   name=n.rsplit('/',1)[-1]
   assert n==mount+'/MT8167D3DClear/'+name and re.fullmatch(r'(clear|umd)-[A-Za-z0-9_.-]+\.log',name)
   logs.append('MT8167D3DClear/'+name)
  assert len(logs)==len(set(logs))
  specs=[(x['source_path'],True) for x in m['files']]+[(n,True) for n in extra+logs]
  before=r.snapshot(mount,specs)
  for x in m['files']:assert before[x['source_path']]=={k:x[k] for k in ('sha256','bytes')}
  total=0
  for n in extra+logs:
   e=before[n];total+=e['bytes'];assert 0<e['bytes']<=16*1024*1024 and total<=80*1024*1024
   q=out/'files'/n;q.parent.mkdir(parents=True,exist_ok=True);r.adb('pull',mount+'/'+n,str(q));assert q.stat().st_size==e['bytes'] and c.sha(q)==e['sha256'];q.chmod(0o600)
   result['files'].append(dict(source_path=n,backup_path=q.relative_to(out).as_posix(),**e))
   if n in logs:mutable['files'][n.split('/')[-1]]=dict(path=q.relative_to(root).as_posix(),sha256=e['sha256'])
  assert r.snapshot(mount,specs)==before and r.identity()==identity and c.sha(mf)==a.manifest_sha256
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
 if not result['errors'] and result['unmounted']:
  result['status']='captured-unmounted';(out/'mutable-baseline.json').write_text(json.dumps(mutable,indent=2)+'\n')
 (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(status=result['status'],errors=result['errors'],files=len(result['files']))))
 if result['status']!='captured-unmounted':raise SystemExit(1)

if __name__=='__main__':main()
