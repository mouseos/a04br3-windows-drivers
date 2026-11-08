#!/usr/bin/env python3
"""既存captureを変更せず、同Recovery bootで固定GPU保持3fileだけを補足する。"""
import argparse,importlib.util,json,shlex
from pathlib import Path
spec=importlib.util.spec_from_file_location('prebackup',Path(__file__).with_name('capture-windows-prebackup.py'))
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
EXTRA=('Windows/System32/MT8167RenderSingleGpu.exe','Windows/System32/ge8300-umd.dll','Windows/System32/Drivers/ge8300-mips.fw')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--manifest',type=Path,required=True);ap.add_argument('--manifest-sha256',required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--ntfs3g-remote',default='/tmp/rt-ntfs3g');a=ap.parse_args()
 mf=c.inside_workspace(a.manifest,True);out=c.inside_workspace(a.out);assert not out.exists(),'新規root内出力のみ'
 assert c.SHA256.fullmatch(a.manifest_sha256) and c.sha(mf)==a.manifest_sha256,'原capture pin不一致'
 m=json.loads(mf.read_text());assert m['status']=='captured-unmounted' and m['read_only'] is True and m['unmounted'] is True and m['errors']==[],'完了captureだけ'
 r=c.Recovery(m['serial']);identity=r.identity();assert all(identity[k]==m[k] for k in identity),'同boot/capture出自必須';r.require_unmounted(identity)
 specs=[(e['source_path'],True) for e in m['files']]+[(n,True) for n in EXTRA];assert len(specs)==len({x[0].lower() for x in specs}),'補足の重複禁止'
 out.mkdir(mode=0o700);mount='/tmp/rt-peripheral-'+__import__('uuid').uuid4().hex;made=False;mounted=False
 result={'schema':1,'source_manifest_sha256':a.manifest_sha256,'recovery_boot_id':identity['recovery_boot_id'],'status':'failed','read_only':True,'unmounted':False,'errors':[],'files':[]}
 try:
  r.shell('mkdir '+mount);made=True;r.shell('test -x '+shlex.quote(a.ntfs3g_remote));mounted=True
  r.shell(shlex.quote(a.ntfs3g_remote)+' -o ro,norecover,nosuid,nodev,noexec '+c.BY_NAME+' '+mount);r.require_own_ro_mount(mount,identity)
  before=r.snapshot(mount,specs)
  for e in m['files']:assert before[e['source_path']]=={k:e[k] for k in ('sha256','bytes')},'原capture byte差分'
  for n in EXTRA:
   local=out/'files'/n;local.parent.mkdir(parents=True,exist_ok=True);r.adb('pull',mount+'/'+n,str(local));assert not local.is_symlink() and 0<local.stat().st_size<=16*1024*1024 and c.sha(local)==before[n]['sha256'] and local.stat().st_size==before[n]['bytes'],'補足byte不一致'
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
 if not result['errors'] and result['unmounted'] and len(result['files'])==3:result['status']='captured-unmounted'
 p=out/'manifest.json';p.write_text(json.dumps(result,indent=2)+'\n');p.chmod(0o600)
 print(json.dumps({'status':result['status'],'manifest_sha256':c.sha(p)}))
 if result['status']!='captured-unmounted':raise SystemExit(1)
if __name__=='__main__':main()
