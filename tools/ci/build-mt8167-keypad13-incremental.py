#!/usr/bin/env python3
"""Retained 1.2 objects + one changed TU, recovered WDK81 only; no installation."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
p=argparse.ArgumentParser()
for n in ('baseline','cache','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();r=Path(__file__).resolve().parents[2];out=a.output.resolve();baseline=a.baseline.resolve();cache=a.cache.resolve()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def require(v,s):
 if not v:raise SystemExit(s)
require(not out.exists(),'fresh dedicated output required')
require(not subprocess.check_output(['git','-C',str(r),'status','--porcelain']),'clean committed source required')
commit=subprocess.check_output(['git','-C',str(r),'rev-parse','HEAD'],text=True).strip()
mp=baseline/'unsigned-manifest.json'
require(sha(mp)=='fd8157f5421728acc807c5d76d5d64db3ac07ce3961b6b707363c54735fdf7e9','retained 1.2 manifest mismatch')
m=json.loads(mp.read_text());inc=cache/'sdk-headers/wdk/Include';lib=cache/'compiler-libs/wdk/Lib'
# The two reused C files only include these repository headers and stdint.h.
for f in m['inputs']:
 if f['path'].endswith(('button_state.h','button_pipeline.h')):require(sha(r/f['path'])==f['sha256'],'reused header mismatch')
reused=[]
for row in m['compiled'][1:]:
 obj=baseline/'obj'/(Path(row['path']).stem+'.obj')
 require(sha(obj)==row['object_sha256'] and sha(r/row['path'])==row['source_sha256'],'retained object/source mismatch')
 reused.append({**row,'object_path':str(obj)})
libs=[]
for row in m['libraries']:
 rel=row['path'].split('/compiler-libs/wdk/Lib/')[1];q=lib/rel
 require(sha(q)==row['sha256'],'retained library mismatch');libs.append(q)
inf=r/'drivers/input/mt8167-keypad/mt8167-keypad.inf'
require(b'DriverVer=11/10/2026,0.0.1.3' in inf.read_bytes(),'candidate INF mismatch')
(out/'obj').mkdir(parents=True);(out/'package').mkdir();(out/'tmp').mkdir()
commands=[];env=os.environ.copy();env['TEMP']=env['TMP']=env['TMPDIR']=str(out/'tmp')
def run(cmd):
 commands.append(cmd);(out/'commands.json').write_text(json.dumps(commands,indent=2));subprocess.run(cmd,env=env,check=True)
# Keep the baseline codegen switches; rebind only overlay/include paths.
flags=[];i=0
while i<len(m['codegen']):
 f=m['codegen'][i]
 if f=='-ivfsoverlay':i+=2;continue
 if f=='-isystem':
  flags += [f,str(inc/m['codegen'][i+1].split('/sdk-headers/wdk/Include/')[1])];i+=2;continue
 flags.append(f);i+=1
require(os.name=='nt','this variant uses native Windows case-insensitive WDK paths')
# Record the actual recovered headers, without claiming an unrecorded historical closure.
headers=[{'path':str(q.relative_to(inc)).replace('\\','/'),'bytes':q.stat().st_size,'sha256':sha(q)} for q in sorted(inc.rglob('*')) if q.is_file()]
(out/'wdk-header-closure.json').write_text(json.dumps(headers,indent=2))
src=r/'drivers/input/mt8167-keypad/wdf_keys.c';obj=out/'obj/wdf_keys.obj'
run(['clang',*flags,'-c',str(src),'-o',str(obj)])
objects=[obj]
for row in reused:
 q=Path(row['object_path']);dest=out/'obj'/q.name;shutil.copyfile(q,dest);objects.append(dest)
sys=out/'package/mt8167-keypad.sys'
run(['lld-link','/driver','/subsystem:native','/machine:arm','/entry:FxDriverEntry','/nodefaultlib',*map(str,objects),*map(str,libs),'/out:'+str(sys)])
(out/'package'/inf.name).write_bytes(inf.read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'))
inputs=[{'path':str(q.relative_to(r)).replace('\\','/'),'sha256':sha(q)} for d in ('drivers/input/mt8167-keypad','drivers/input/a04br3-buttons') for q in sorted((r/d).iterdir()) if q.is_file()]
manifest={'schema':'mt8167-keypad13-incremental-v1','status':'unsigned-cross-built-with-recovered-WDK81','source_commit':commit,'source_dirty':False,'target':'MT6392\\Buttons','service':'Mt6392Keys','version':'0.0.1.3','compiled':[{'path':str(src.relative_to(r)).replace('\\','/'),'source_sha256':sha(src),'object_sha256':sha(obj)}],'reused':reused,'baseline_manifest_sha256':sha(mp),'inputs':inputs,'libraries':[{'path':str(q),'sha256':sha(q)} for q in libs],'commands':commands,'codegen':flags,'codegen_rebinding':'native Windows case-insensitive paths replace historical Linux VFS overlay; actual compiler and full header set recorded; historical header hashes unavailable','compiler':subprocess.check_output(['clang','--version'],text=True).splitlines()[0],'linker':subprocess.check_output(['lld-link','--version'],text=True).strip(),'header_closure_sha256':sha(out/'wdk-header-closure.json'),'parent_changed':False,'device_tested':False,'wake_implemented':False,'unsigned_outputs':[{'path':str(q.relative_to(out)).replace('\\','/'),'bytes':q.stat().st_size,'sha256':sha(q)} for q in sorted((out/'package').iterdir())]}
(out/'unsigned-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('KEYPAD13_COMPILE1_LINK1_NOT_DEVICE_TESTED')
