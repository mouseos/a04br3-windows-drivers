#!/usr/bin/env python3
"""One provider TU DIRQL correction. Preserve all356r1 binaries except provider.
The original candidate is withdrawn without any hardware run.
"""
from pathlib import Path
import argparse,json,hashlib,subprocess,shutil,zipfile
p=argparse.ArgumentParser();p.add_argument('--source-commit',required=True);a=p.parse_args();t=Path('C:/mt8167-present356');r=t/'source';old=t/'build';o=t/'build-r2';pkg=o/'package356'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def run(args,n):
 q=subprocess.run([str(x)for x in args],capture_output=True,text=True);(o/'logs'/n).write_text(q.stdout+q.stderr)
 if q.returncode:raise RuntimeError(n+' '+q.stdout[-1200:]+q.stderr[-1200:])
 return q.stdout
assert subprocess.check_output(['git','-C',str(r),'rev-parse','HEAD'],text=True).strip()==a.source_commit
assert not subprocess.check_output(['git','-C',str(r),'status','--porcelain'],text=True).strip()
subprocess.run(['python',str(r/'tools/ci/verify-ge8300-fullsource.py')],check=True)
assert sha(old/'package356/signed-manifest.json')=='7abd38eaf75466faf2a970ed3e51662a6d8beb719d4afe18fc5dfaa907755f21'
m=load(old/'package356/signed-manifest.json')
for x in m['outputs']:assert sha(old/'package356'/x['name'])==x['sha256']
assert not o.exists();shutil.copytree(old,o);new=load(pkg/'signed-manifest.json');u=next(x for x in new['compiled_units']if x['source'].endswith('/transport356.c'));src=r/u['source'];obj=o/'transport356.obj';llvm=Path('C:/Program Files/LLVM/bin')
run([llvm/'clang.exe',*u['arguments'],'-c',src,'-o',obj],'r2-provider-compile.log')
u.update(source_sha256=sha(src),object_sha256=sha(obj),recovered_identical_compile_snapshot=False,recovered_stage_source_commit=None,actual_compile_source_commit=a.source_commit)
kit=Path('C:/mt8167-public-fullsource323/wdk81/Lib/winv6.3/km/arm');link=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm/link.exe')
assert sha(link)=='f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938'
d=pkg/'display-transport'
run([link,'/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/largeaddressaware','/tsaware:no','/base:0x400000',obj,o/'scanout_vidmm356.obj',*[kit/(n+'.lib')for n in ['displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk']],'/out:'+str(d/'mt8167-display-transport.sys'),'/map:'+str(o/'transport.map')],'r2-provider-link.log')
sign=Path('C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe');thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
run([sign,'sign','/fd','SHA256','/sha1',thumb,d/'mt8167-display-transport.sys'],'r2-provider-sign.log');run([sign,'verify','/pa',d/'mt8167-display-transport.sys'],'r2-provider-sign-verify.log')
run(['C:/mt8167-hp1033/inputs/signing/bin/Inf2Cat.exe','/driver:'+str(d),'/os:6_3_ARM','/uselocaltime'],'r2-provider-cat.log');run([sign,'sign','/fd','SHA256','/sha1',thumb,d/'mt8167-display-transport.cat'],'r2-provider-cat-sign.log')
for n in ['mt8167-display-transport.sys','mt8167-display-transport.inf']:run([sign,'verify','/pa','/c',d/'mt8167-display-transport.cat',d/n],'r2-cat-member-'+n+'.log')
for x in m['outputs']:
 if x['name']not in ['display-transport/mt8167-display-transport.sys','display-transport/mt8167-display-transport.cat']:assert sha(pkg/x['name'])==x['sha256'],x['name']
for arc in new['archives']:assert sha(o/arc['file'])==arc['sha256']
new.update(package_source_commit=a.source_commit,link_recipe_source_commit=a.source_commit,display_transport_source_commit=a.source_commit,client_source_commit=m['package_source_commit'],r2_compiled_units=[u],r2_reused_raw80=80,r2_reused_UMD22=22,r2_reused_PCO36=36,r2_baseline_manifest_sha256=sha(old/'package356/signed-manifest.json'),withdrawn_r1_hardware_runs=0,IRQ_event_signalling='actual serial only at DIRQL; bounded PASSIVE poll; no KeSetEvent from ISR')
new['outputs']=[dict(name=x['name'],sha256=sha(pkg/x['name']))for x in m['outputs']]
new.pop('build_manifest_sha256',None);(o/'build-manifest.json').write_text(json.dumps(new,indent=2)+'\n');shutil.copyfile(o/'build-manifest.json',pkg/'present356-build-manifest.json');new['build_manifest_sha256']=sha(pkg/'present356-build-manifest.json');(pkg/'signed-manifest.json').write_text(json.dumps(new,indent=2)+'\n')
with zipfile.ZipFile(t/'outputs356-r2.zip','w',zipfile.ZIP_DEFLATED)as z:
 for f in o.rglob('*'):
  if f.is_file():z.write(f,f.relative_to(o).as_posix())
print('SIGNED356_R2_ONE_PROVIDER_TU_HARDWARE_UNVERIFIED',sha(pkg/'signed-manifest.json'))
