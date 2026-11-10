#!/usr/bin/env python3
"""One miniport TU over the sealed GPU362 package; no SDK/native/UMD rebuild."""
import argparse, copy, hashlib, json, re, shutil, subprocess, zipfile
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True)
t=p.parse_args().task_root;r=t/'source';o=t/'build';baseline=Path('C:/mt8167-unified362/build')
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
load=lambda p:json.loads(Path(p).read_text(encoding='utf-8-sig'))
i=load(t/'inputs.json');old=load(baseline/'package362/signed-manifest.json')
def pin(p,h):
 if sha(p)!=h:raise ValueError('Input hash mismatch: '+str(p))
pin(baseline/'package362/signed-manifest.json',i['baseline_manifest_sha256'])
for row in old['outputs']:pin(baseline/'package362'/row['name'],row['sha256'])
for f,h in i['source_files'].items():pin(r/f,h)
changed='drivers/gpu/mt8167/wddm-render/miniport.c'
palette='drivers/gpu/mt8167/wddm-display/unified_display.cxx'
# Retain the palette overlay from362, not the historical359 display object.
for row in old['actual_include_closure']:
 if row.get('role') not in ('kmd','native','display') or row['source'] in (changed,palette):continue
 for f,h in row['local_closure'].items():pin(r/f,h)
for f,h in old['new_display_compile']['local_closure'].items():pin(r/f,h)
for row in old['reused_kernel_inputs']:pin(row['path'],row['sha256'])
palette_obj=baseline/old['new_display_compile']['object'];pin(palette_obj,old['new_display_compile']['object_sha256'])
if o.exists():raise ValueError('Fresh task-local output required')
o.mkdir();(o/'logs').mkdir();pkg=o/'package364';shutil.copytree(baseline/'package362',pkg)
def run(args,name):
 q=subprocess.run([str(x) for x in args],capture_output=True,text=True)
 (o/'logs'/name).write_text(q.stdout+q.stderr)
 if q.returncode:raise RuntimeError(name+' '+q.stdout[-1500:]+q.stderr[-1500:])
 return q.stdout+q.stderr
unit=next(x for x in old['inherited_compiled_units'] if x['source']==changed)
flags=[re.sub(r'C:/mt8167-[^/]+/source/',r.as_posix()+'/',x.replace('\\','/')) for x in unit['arguments']]
obj=o/'miniport.obj';run(['clang',*flags,'-H','-c',r/changed,'-o',obj],'miniport-compile.log')
kernel=next(Path(x['path']) for x in old['reused_kernel_inputs'] if Path(x['path']).name=='ge8300-pair-kmd.lib')
newlib=o/kernel.name;shutil.copyfile(kernel,newlib)
members=run(['llvm-ar','t',newlib],'archive-before.log').splitlines()
if members.count('miniport.obj')!=1:raise ValueError('Exactly one old miniport member required')
run(['llvm-ar','d',newlib,'miniport.obj'],'archive-delete.log')
run(['llvm-ar','rcs',newlib,obj],'archive-replace.log')
after=run(['llvm-ar','t',newlib],'archive-after.log').splitlines()
if sorted(members)!=sorted(after):raise ValueError('Kernel archive member set changed')
link=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm/link.exe')
pin(link,'f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938')
inputs=[newlib if Path(x['path'])==kernel else Path(x['path']) for x in old['reused_kernel_inputs']]
args=['/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/largeaddressaware','/tsaware:no','/base:0x400000']
run([link,*args,*inputs,palette_obj,'/out:'+str(pkg/'ge8300-render.sys'),'/map:'+str(o/'ge8300-render.map')],'link-kmd.log')
inf=pkg/'ge8300-render.inf';raw=inf.read_bytes();enc='utf-16' if raw[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig';text=raw.decode(enc)
if text.count('0.0.3.62')!=1:raise ValueError('Expected baseline INF3.62')
inf.write_text(text.replace('0.0.3.62','0.0.3.64'),encoding=enc)
sign=Path('C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe');thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.sys'],'sign-sys.log')
transport=pkg/'display-transport';saved=t/'preserved-display-transport';transport.rename(saved)
try:run(['C:/mt8167-hp1033/inputs/signing/bin/Inf2Cat.exe','/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime'],'catalog.log')
finally:saved.rename(transport)
run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.cat'],'sign-cat.log')
for name in ['ge8300-render.sys','ge8300-umd.dll','ge8300-render.cat']:run([sign,'verify','/pa','/v',pkg/name],name+'.verify.log')
for name in ['ge8300-render.sys','ge8300-render.inf','ge8300-umd.dll','ge8300-mips.fw']:run([sign,'verify','/pa','/c',pkg/'ge8300-render.cat',pkg/name],name+'.catalog-verify.log')
# Every archive member except miniport must remain byte-identical.
delta=[]
for name in members:
 if name=='miniport.obj':continue
 a=subprocess.run(['llvm-ar','p',str(kernel),name],capture_output=True,check=True).stdout
 b=subprocess.run(['llvm-ar','p',str(newlib),name],capture_output=True,check=True).stdout
 if a!=b:raise ValueError('Unrelated archive member changed: '+name)
 delta.append({'member':name,'sha256':hashlib.sha256(a).hexdigest()})
m=copy.deepcopy(old);m.update(schema='GPU-lifecycle364-source-overlay-v1',package_source_commit=i['source_commit'],kernel_overlay_source_commit=i['source_commit'],inf_driverver='11/10/2026,0.0.3.64',compiled_miniport_units=1,baseline362_manifest_sha256=i['baseline_manifest_sha256'],hardware_verified=False,normal_Present_verified=False,DWM_complete=False,controls_changed=False,SDK_rebuilt=False,PCO_rebuilt=False)
m['new_miniport_compile']={'source':changed,'source_sha256':sha(r/changed),'header_sha256':sha(r/'drivers/gpu/mt8167/wddm-render/display_lifecycle364.h'),'arguments':flags,'object_sha256':sha(obj)}
m['unchanged_kernel_members']=delta;m['new_kernel_archive_sha256']=sha(newlib)
m['outputs']=[{'name':x['name'],'sha256':sha(pkg/x['name'])} for x in old['outputs']]
shutil.copyfile(baseline/'package362/signed-manifest.json',pkg/'baseline362-signed-manifest.json')
(pkg/'signed-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
with zipfile.ZipFile(t/'outputs364.zip','w',zipfile.ZIP_DEFLATED) as z:
 for f in sorted(o.rglob('*')):
  if f.is_file():z.write(f,f.relative_to(o).as_posix())
print('GPU364_SIGNED_NOT_HARDWARE_VERIFIED',sha(pkg/'signed-manifest.json'),sha(pkg/'ge8300-render.sys'))
