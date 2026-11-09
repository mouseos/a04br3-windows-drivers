#!/usr/bin/env python3
"""Relink the sealed342 objects with the working RT8.1 Microsoft linker contract."""
import argparse,hashlib,json,shutil,struct,subprocess,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True);p.add_argument('--recipe-commit',required=True);a=p.parse_args()
t=a.task_root;base=t/'baseline';old=base/'package342';out=t/'build';pkg=out/'package343'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def pin(p,h):
 if sha(p)!=h:raise ValueError('Input pin: '+str(p))
def run(c):subprocess.run([str(x)for x in c],check=True)
def header(p):
 b=p.read_bytes();at=struct.unpack_from('<I',b,60)[0];assert b[at:at+4]==b'PE\0\0';o=at+24
 machine,ns=struct.unpack_from('<HH',b,at+4);size=struct.unpack_from('<H',b,at+20)[0]
 return {'machine':machine,'characteristics':struct.unpack_from('<H',b,at+22)[0],'magic':struct.unpack_from('<H',b,o)[0],'subsystem':struct.unpack_from('<H',b,o+68)[0],'dll_characteristics':struct.unpack_from('<H',b,o+70)[0],'sections':{b[o+size+i*40:o+size+i*40+8].rstrip(b'\0').decode():struct.unpack_from('<I',b,o+size+i*40+36)[0]for i in range(ns)}}
if out.exists():raise ValueError('New output required')
pin(old/'signed-manifest.json','d167f2e56ae363341c778fa3ddde4a48200d62284132b7eea2caffe30056fb7b')
m=load(old/'signed-manifest.json')
for x in m['outputs']+m['retained_provenance']:pin(old/x['name'],x['sha256'])
libs=[]
for x in m['archives']:
 if x['role'] in ['kmd','native']:pin(base/x['file'],x['sha256']);libs.append(base/x['file'])
link=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm/link.exe')
pin(link,'f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938')
out.mkdir();pkg.mkdir();kit=Path('C:/mt8167-public-fullsource323/wdk81/Lib/winv6.3/km/arm')
kmd=next(base/x['file']for x in m['archives']if x['role']=='kmd');native=next(base/x['file']for x in m['archives']if x['role']=='native')
args=['/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/largeaddressaware','/tsaware:no','/base:0x400000',kmd,native,*[kit/(n+'.lib')for n in ['displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk']],'/out:'+str(out/'ge8300-render.sys'),'/map:'+str(out/'ge8300-render.map')]
run([link,*args]);h=header(out/'ge8300-render.sys')
if (h['machine'],h['magic'],h['characteristics'],h['subsystem'],h['dll_characteristics'])!=(0x1c4,0x10b,0x122,1,0x140):raise ValueError('RT81 baseline PE contract not restored')
for n in ['.text','.rdata','.data','.pdata']:
 if not h['sections'].get(n,0)&0x08000000:raise ValueError('Baseline nonpageable section contract: '+n)
sign=Path('C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe');thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
run([sign,'sign','/fd','SHA256','/sha1',thumb,out/'ge8300-render.sys']);run([sign,'verify','/pa',out/'ge8300-render.sys'])
for f in old.iterdir():
 if f.is_file()and f.name not in ['ge8300-render.sys','ge8300-render.cat','ge8300-render.inf','signed-manifest.json']:shutil.copyfile(f,pkg/f.name)
shutil.copyfile(old/'signed-manifest.json',pkg/'baseline342-signed-manifest.json');shutil.copyfile(out/'ge8300-render.sys',pkg/'ge8300-render.sys')
b=(old/'ge8300-render.inf').read_bytes();enc='utf-16'if b[:2]in [b'\xff\xfe',b'\xfe\xff']else'utf-8-sig';s=b.decode(enc);before='DriverVer=11/08/2026,0.0.3.42';after='DriverVer=11/08/2026,0.0.3.43'
if s.count(before)!=1:raise ValueError('Fixed342 DriverVer required')
(pkg/'ge8300-render.inf').write_text(s.replace(before,after),encoding=enc)
cat=Path('C:/mt8167-hp1033/inputs/signing/bin/Inf2Cat.exe');run([cat,'/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime']);run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.cat'])
for n in ['ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw']:run([sign,'verify','/pa','/c',pkg/'ge8300-render.cat',pkg/n])
for x in m['outputs']:
 if x['name']not in ['ge8300-render.sys','ge8300-render.cat','ge8300-render.inf']:pin(pkg/x['name'],x['sha256'])
new=dict(m);new.update(schema='msvc-relink343-reuse-complete342-objects-v1',package_source_commit=a.recipe_commit,link_recipe_source_commit=a.recipe_commit,kernel_binary_source_commit=m['kernel_binary_source_commit'],compiled_units=[],reused_342_compiled_units=m['compiled_units'],compiler_reexecuted=False,source_changed=False,linker_changed=True,linker_path=str(link),linker_sha256=sha(link),link_arguments=[str(x)for x in args],PE_contract=h,runtime_build_id='GPU342-tq-clear-v1',inf_driverver='11/08/2026,0.0.3.43',baseline_manifest_file='baseline342-signed-manifest.json',baseline_manifest_sha256=sha(pkg/'baseline342-signed-manifest.json'),build_manifest_file='msvc-relink343-build-manifest.json',hardware_verified=False,runtime_verified=False,root_reset_fix_proven=False)
new['outputs']=[{'name':x['name'],'sha256':sha(pkg/x['name'])}for x in m['outputs']]
new['retained_provenance']=[{'name':f.name,'sha256':sha(f)}for f in pkg.iterdir()if f.is_file()and f.name not in {x['name']for x in new['outputs']}]
(out/'build-manifest.json').write_text(json.dumps(new,indent=2)+'\n');shutil.copyfile(out/'build-manifest.json',pkg/'msvc-relink343-build-manifest.json');new['build_manifest_sha256']=sha(pkg/'msvc-relink343-build-manifest.json');(pkg/'signed-manifest.json').write_text(json.dumps(new,indent=2)+'\n')
with zipfile.ZipFile(t/'outputs.zip','w',zipfile.ZIP_DEFLATED)as z:
 for f in out.rglob('*'):
  if f.is_file():z.write(f,f.relative_to(out).as_posix())
print('MSVC_RELINK343_SIGNED_NO_COMPILATION',sha(pkg/'signed-manifest.json'),sha(pkg/'ge8300-render.sys'))
