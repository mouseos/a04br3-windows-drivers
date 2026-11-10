#!/usr/bin/env python3
"""Three caller/native observer TUs over GPU365; retain SDK/UMD/FW/DIS0."""
import argparse, copy, hashlib, json, re, shutil, subprocess, zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True)
t=p.parse_args().task_root;r=t/'source';o=t/'build'
b362=Path('C:/mt8167-unified362/build');b365=Path('C:/mt8167-retirement365/build')
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
load=lambda p:json.loads(Path(p).read_text(encoding='utf-8-sig'))
i=load(t/'inputs.json');old=load(b362/'package362/signed-manifest.json');current=load(b365/'package365/signed-manifest.json')
def pin(p,h):
 if sha(p)!=h:raise ValueError('Input hash mismatch: '+str(p))
pin(b362/'package362/signed-manifest.json',i['baseline362_manifest_sha256'])
pin(b365/'package365/signed-manifest.json',i['baseline365_manifest_sha256'])
for x in current['outputs']:pin(b365/'package365'/x['name'],x['sha256'])
for f,h in i['source_files'].items():pin(r/f,h)
# Local paths in the frozen manifest were exported with doubled slashes.
def path(v):return Path(v.replace('\\','/'))
for x in old['reused_kernel_inputs']:pin(path(x['path']),x['sha256'])
base_kernel=b365/'ge8300-pair-kmd.lib';pin(base_kernel,current['new_kernel_archive_sha256'])
palette=b362/old['new_display_compile']['object'];pin(palette,old['new_display_compile']['object_sha256'])
if o.exists():raise ValueError('Fresh task-local output required')
o.mkdir();(o/'logs').mkdir();pkg=o/'package366';shutil.copytree(b365/'package365',pkg)
def run(args,name,binary=False):
 q=subprocess.run([str(x) for x in args],capture_output=True,text=not binary)
 if not binary:(o/'logs'/name).write_text(q.stdout+q.stderr)
 if q.returncode:raise RuntimeError(name)
 return q.stdout
changed=['drivers/gpu/mt8167/vendor_m1_11/render_owner/native_owner_factory.c','drivers/gpu/mt8167/wddm-render/miniport.c','drivers/gpu/mt8167/wddm-display/unified_display.cxx']
compiles=[];objects={}
cl=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm/cl.exe')
for source in changed:
 u=old['new_display_compile'] if source==changed[2] else next(x for x in old['inherited_compiled_units'] if x['source']==source)
 flags=[re.sub(r'C:/mt8167-[^/]+/source/',r.as_posix()+'/',x.replace('\\','/')) for x in u['arguments']]
 if source==changed[2]:
  obj=o/'display_unified366.obj';run([cl,*flags,r/source,'/Fo'+str(obj)],obj.stem+'-compile.log');compiler=cl.as_posix();compiler_sha256=sha(cl)
 else:
  obj=o/u['object'];run(['clang',*flags,'-H','-c',r/source,'-o',obj],obj.stem+'-compile.log');compiler='clang';compiler_sha256=None
 compiles.append({'source':source,'source_sha256':sha(r/source),'object':obj.name,'object_sha256':sha(obj),'arguments':flags,'compiler':compiler,'compiler_sha256':compiler_sha256})
 objects[source]=obj
palette=objects[changed[2]]
new_inputs=[];archive_receipts=[]
for x in old['reused_kernel_inputs']:
 original=path(x['path']);actual=original
 if original.name in ('ge8300-pair-kmd.lib','ge8300-native-owner-core.lib'):
  prior=base_kernel if original.name=='ge8300-pair-kmd.lib' else b365/original.name
  if original.name=='ge8300-native-owner-core.lib':pin(prior,next(x['sha256'] for x in current['observation365_archives'] if x['name']==original.name))
  target=o/original.name;shutil.copyfile(prior,target)
  obj=objects[changed[1] if original.name=='ge8300-pair-kmd.lib' else changed[0]]
  members=run(['llvm-ar','t',prior],target.stem+'-before.log').splitlines()
  if members.count(obj.name)!=1:raise ValueError('Replacement member must occur once: '+obj.name)
  run(['llvm-ar','d',target,obj.name],target.stem+'-delete.log')
  run(['llvm-ar','rcs',target,obj],target.stem+'-replace.log')
  after=run(['llvm-ar','t',target],target.stem+'-after.log').splitlines()
  if sorted(members)!=sorted(after):raise ValueError('Archive member set changed')
  retained=[]
  for name in members:
   if name==obj.name:continue
   a=run(['llvm-ar','p',prior,name],'unused',True);b=run(['llvm-ar','p',target,name],'unused',True)
   if a!=b:raise ValueError('Unrelated member changed: '+name)
   retained.append({'member':name,'sha256':hashlib.sha256(a).hexdigest()})
  archive_receipts.append({'name':target.name,'baseline_sha256':sha(prior),'sha256':sha(target),'replaced_member':obj.name,'retained':retained});actual=target
 elif original.name=='display_adapter356.obj':
  actual=b365/'display_adapter356.obj';pin(actual,next(x['object_sha256'] for x in current['observation365_compiles'] if x['source'].endswith('/display_adapter356.c')))
 new_inputs.append(actual)
link=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm/link.exe')
pin(link,'f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938')
args=['/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/largeaddressaware','/tsaware:no','/base:0x400000']
run([link,*args,*new_inputs,palette,'/out:'+str(pkg/'ge8300-render.sys'),'/map:'+str(o/'ge8300-render.map')],'link-kmd.log')
inf=pkg/'ge8300-render.inf';raw=inf.read_bytes();enc='utf-16' if raw[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig';text=raw.decode(enc)
if text.count('0.0.3.65')!=1:raise ValueError('Expected baseline INF3.65')
inf.write_text(text.replace('0.0.3.65','0.0.3.66'),encoding=enc)
sign=Path('C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe');thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.sys'],'sign-sys.log')
transport=pkg/'display-transport';saved=t/'preserved-display-transport';transport.rename(saved)
try:run(['C:/mt8167-hp1033/inputs/signing/bin/Inf2Cat.exe','/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime'],'catalog.log')
finally:saved.rename(transport)
run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.cat'],'sign-cat.log')
for name in ['ge8300-render.sys','ge8300-umd.dll','ge8300-render.cat']:run([sign,'verify','/pa','/v',pkg/name],name+'.verify.log')
for name in ['ge8300-render.sys','ge8300-render.inf','ge8300-umd.dll','ge8300-mips.fw']:run([sign,'verify','/pa','/c',pkg/'ge8300-render.cat',pkg/name],name+'.catalog-verify.log')
m=copy.deepcopy(current);m.update(schema='GPU-post-release366-source-overlay-v1',package_source_commit=i['source_commit'],kernel_overlay_source_commit=i['source_commit'],inf_driverver='11/10/2026,0.0.3.66',hardware_verified=False,normal_Present_verified=False,DWM_complete=False,controls_changed=False,SDK_rebuilt=False,PCO_rebuilt=False)
m['baseline365_manifest_sha256']=i['baseline365_manifest_sha256'];m['post_release366_compiles']=compiles;m['post_release366_archives']=archive_receipts
m['post_release366_headers']={f:h for f,h in i['source_files'].items() if f.endswith(('native_factory_checkpoint.h','post_release_stack366.h','unified_display.h'))}
m['compiled_observation366_units']=3;m['compiled_miniport_units']=1
m['baseline_miniport_reference_only']=current['new_miniport_compile'];m['new_miniport_compile']=next(x for x in compiles if x['source']==changed[1]);m['new_display_compile']=next(x for x in compiles if x['source']==changed[2])
m['new_kernel_archive_sha256']=sha(o/'ge8300-pair-kmd.lib')
m['outputs']=[{'name':x['name'],'sha256':sha(pkg/x['name'])} for x in current['outputs']]
shutil.copyfile(b365/'package365/signed-manifest.json',pkg/'baseline365-signed-manifest.json')
(pkg/'signed-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
with zipfile.ZipFile(t/'outputs366.zip','w',zipfile.ZIP_DEFLATED) as z:
 for f in sorted(o.rglob('*')):
  if f.is_file():z.write(f,f.relative_to(o).as_posix())
print('GPU366_SIGNED_NOT_HARDWARE_VERIFIED',sha(pkg/'signed-manifest.json'),sha(pkg/'ge8300-render.sys'))
