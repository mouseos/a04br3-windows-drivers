#!/usr/bin/env python3
"""One display TU on signed UI texture cache; reuse proven UpdateSubresource UMD."""
from pathlib import Path
import argparse,copy,hashlib,json,re,shutil,subprocess,zipfile
p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True);a=p.parse_args();t=a.task_root;r=t/'source';o=t/'build';pkg=o/'package362'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
load=lambda p:json.loads(Path(p).read_text(encoding='utf-8-sig'))
i=load(t/'inputs.json');b=Path('C:/mt8167-ui-texture359/build');s=Path('C:/mt8167-startgap359/build')
def pin(p,h):
 if sha(p)!=h:raise ValueError('Input hash '+str(p))
pin(b/'package-ui-texture359/signed-manifest.json',i['baseline_manifest_sha256']);old=load(b/'package-ui-texture359/signed-manifest.json')
pin(t/'updated-ge8300-umd.dll',i['UMD_sha256'])
for f,h in i['source_files'].items():pin(r/f,h)
for x in old['outputs']:pin(b/'package-ui-texture359'/x['name'],x['sha256'])
for ar in old['archives']:pin(b/ar['file'],ar['sha256'])
if o.exists():raise ValueError('New output required')
o.mkdir();(o/'logs').mkdir();shutil.copytree(b/'package-ui-texture359',pkg)
def run(args,name):
 q=subprocess.run([str(x) for x in args],capture_output=True,text=True);(o/'logs'/name).write_text(q.stdout+q.stderr)
 if q.returncode:raise RuntimeError(name+' '+q.stdout[-1800:]+q.stderr[-1800:])
 return q.stdout+q.stderr
vc=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133');cl=vc/'bin/Hostx64/arm/cl.exe';link=vc/'bin/Hostx64/arm/link.exe';pin(link,'f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938')
u=next(x for x in old['compiled_units'] if x['source'].endswith('/unified_display.cxx'));flags=[]
for flag in u['arguments']:
 flag=re.sub('/+','/',flag.replace('\\','/'))
 flag=re.sub(r'C:/mt8167-[^/]+/source/',r.as_posix()+'/',flag)
 flags.append(flag)
obj=o/'display_unified_display.obj';src=r/u['source'];log=run([cl,*flags,'/showIncludes',src,'/Fo'+str(obj)],'unified-display-compile.log')
closure={u['source']:sha(src)}
for line in log.splitlines():
 if 'including file:' not in line:continue
 q=Path(line.split('including file:',1)[1].strip())
 try:rel=q.resolve().relative_to(r.resolve()).as_posix()
 except ValueError:continue
 closure[rel]=sha(q)
kit=Path('C:/mt8167-public-fullsource323/wdk81/Lib/winv6.3/km/arm');names=['bdd','bdd_dmm','bdd_util','bltfuncs','blthw','mt8167_brightness','memory','ge8300_trace','ge8300_present'];reuse=[s/'display_adapter356.obj',*[s/('display_'+n+'.obj') for n in names]]
expected={x['object']:x['object_sha256'] for x in old['compiled_units']}
for q in reuse:
 if q.name not in expected:raise ValueError('Unpinned display input '+q.name)
 pin(q,expected[q.name])
libs=[kit/(n+'.lib') for n in ['displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk']]
args=['/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/largeaddressaware','/tsaware:no','/base:0x400000']
run([link,*args,b/'ge8300-pair-kmd.lib',b/'ge8300-native-owner-core.lib',*reuse,obj,*libs,'/out:'+str(pkg/'ge8300-render.sys'),'/map:'+str(o/'ge8300-render.map')],'link-kmd.log')
shutil.copyfile(t/'updated-ge8300-umd.dll',pkg/'ge8300-umd.dll')
inf=pkg/'ge8300-render.inf';raw=inf.read_bytes();enc='utf-16' if raw[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig';text=raw.decode(enc)
if text.count('0.0.3.61')!=1:raise ValueError('Expected prior INF3.61')
text=text.replace('0.0.3.61','0.0.3.62');text=re.sub(r'(?im)^(DriverVer\s*=\s*)[^,\r\n]+,',r'\g<1>11/10/2026,',text);inf.write_text(text,encoding=enc)
sign=Path('C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe');thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.sys'],'sign-sys.log')
transport=pkg/'display-transport';saved=t/'preserved-display-transport';transport.rename(saved)
try:run(['C:/mt8167-hp1033/inputs/signing/bin/Inf2Cat.exe','/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime'],'catalog.log')
finally:saved.rename(transport)
run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.cat'],'sign-cat.log')
for n in ['ge8300-render.sys','ge8300-umd.dll','ge8300-render.cat']:run([sign,'verify','/pa','/v',pkg/n],n+'.verify.log')
for n in ['ge8300-render.sys','ge8300-render.inf','ge8300-umd.dll','ge8300-mips.fw']:run([sign,'verify','/pa','/c',pkg/'ge8300-render.cat',pkg/n],n+'.catalog-verify.log')
m=copy.deepcopy(old);m.update(schema='GPU-unified362-source-overlay-v1',package_source_commit=i['source_commit'],kernel_overlay_source_commit=i['source_commit'],UMD_overlay_source_commit=i['UMD_source_commit'],baseline_ui_manifest_sha256=i['baseline_manifest_sha256'],inf_driverver='11/10/2026,0.0.3.62',compiled_display_units=1,SDK_rebuilt=False,PCO_rebuilt=False,hardware_verified=False,normal_Present_verified=False,DWM_complete=False,required_palette_rejection=True,UMD_reused_signed=True,UMD_sha256=i['UMD_sha256'])
m['new_display_compile']={'source':u['source'],'sha256':sha(src),'object':obj.name,'object_sha256':sha(obj),'arguments':flags,'local_closure':closure}
m['reused_kernel_inputs']=[{'path':str(q),'sha256':sha(q)} for q in [b/'ge8300-pair-kmd.lib',b/'ge8300-native-owner-core.lib',*reuse,*libs]]
m['inherited_compiled_units']=m.pop('compiled_units');m['UMD_source_closure']=i['UMD_source_closure'];m['runtime_build_id_inherited']=True
m['outputs']=[{'name':x['name'],'sha256':sha(pkg/x['name'])} for x in old['outputs']]
# Keep the historical source/object recipe in its original immutable manifest.
shutil.copyfile(b/'package-ui-texture359/signed-manifest.json',pkg/'baseline-ui-signed-manifest.json');m.pop('build_manifest_sha256',None)
(pkg/'signed-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
with zipfile.ZipFile(t/'outputs362.zip','w',zipfile.ZIP_DEFLATED) as z:
 for f in sorted(o.rglob('*')):
  if f.is_file():z.write(f,f.relative_to(o).as_posix())
print('GPU362_PACKAGE_SIGNED',sha(pkg/'signed-manifest.json'),sha(pkg/'ge8300-render.sys'),sha(pkg/'ge8300-umd.dll'))
