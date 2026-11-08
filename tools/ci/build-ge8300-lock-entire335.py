#!/usr/bin/env python3
"""Frozen332 -> full-allocation Lock contract; exactly one production ARM TU."""
import argparse,hashlib,json,ntpath,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
for n in ['baseline-build','dependency','output','sign-tool','inf2cat']:p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();base=a.baseline_build.resolve();out=a.output.resolve();dep=a.dependency.resolve()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def run(cmd,**kw):subprocess.run(list(map(str,cmd)),check=True,**kw)
def cap(cmd):return subprocess.check_output(list(map(str,cmd)),text=True).strip()
def pin(p,h):
 if sha(p)!=h:raise ValueError('Pin mismatch '+str(p))
if not out.is_relative_to(root) or out.exists():raise ValueError('Fresh checkout-local output required')
commit=cap(['git','-C',root,'rev-parse','HEAD'])
if cap(['git','-C',root,'status','--porcelain']):raise ValueError('Clean committed source required')
pkg0=base/'package332';pin(pkg0/'signed-manifest.json','55a4d2e6c918275aeccb0364c8af823c1c4f25defd54cf1f3df9753748d753d8')
m=load(pkg0/'signed-manifest.json')
for row in m['outputs']:pin(pkg0/row['name'],row['sha256'])
source='drivers/gpu/mt8167/wddm-render/umd/clear_ddi.c'
changes=cap(['git','-C',root,'diff','--name-only',m['umd_source_commit'],commit,'--','drivers']).splitlines()
if changes!=[source]:raise ValueError('Exactly production clear_ddi.c may change')
row=next(r for r in m['compiled_units'] if r['source']==source)
oldroot=base.parent.parent
flags=[s.replace(str(oldroot),str(root)) for s in row['arguments']]
archive=next(r for r in m['archives'] if r['role']=='umd');pin(base/archive['file'],archive['sha256'])
out.mkdir(parents=True);objdir=out/'umd';objdir.mkdir()
members=[ntpath.basename(s) for s in cap(['llvm-ar','t',base/archive['file']]).splitlines()]
if len(members)!=len(set(members)) or members!=[r['name'] for r in archive['members']]:raise ValueError('Archive identity/order')
run(['llvm-ar','x','--output='+str(objdir),base/archive['file']])
for r in archive['members']:pin(objdir/r['name'],r['sha256'])
run(['clang',*flags,'-c',root/source,'-o',objdir/row['object']])
compiled={'source':source,'source_sha256':sha(root/source),'object':row['object'],'object_sha256':sha(objdir/row['object']),'arguments':flags,'role':'umd'}
for r in archive['members']:
 if r['name']!=row['object']:pin(objdir/r['name'],r['sha256'])
run(['llvm-lib','/machine:arm','/out:'+str(out/archive['file']),*[objdir/n for n in reversed(members)]])
if [ntpath.basename(s) for s in cap(['llvm-ar','t',out/archive['file']]).splitlines()]!=members:raise ValueError('Archive order changed')
compiler=dep/'compiler';pair=dep/'pair'
pin(compiler/'link-manifest.json','3d5c6feec663ffdc88919f44d2adf31ddb177205623df5e6eb279b87fe47d26b')
pin(pair/'manifest.json','c05e327474cb91c260b341e9c6de91f7074d8f749ea68f35a77ca2ccc1858e8a');pin(pair/'pco_pair.obj',load(pair/'manifest.json')['object_sha256'])
b327=load(Path('C:/mt8167-texture327/source/build/filter327/build-manifest.json'))
libs=[]
for r in b327['existing_compiler_libraries']:
 f=base/r['name'] if r['name']=='ge8300-pco-arm32.lib' else compiler/r['name']
 if r['name']=='ge8300-pco-arm32.lib':pin(f,next(x['sha256'] for x in m['archives'] if x['role']=='pco'))
 else:pin(f,r['sha256'])
 libs.append(f)
for n,h in [('umd_entry.obj','87bb4a93bce631f69729a022a2bd87cc7dc77d4266a220f87f844dc29bf0179e'),('umd_typed_entry.obj','963889685d63050a48a0e6963af9add448aa5cef96d86c851a285f49fa015f0e')]:pin(base/'umd'/n,h)
run(['lld-link','/dll','/machine:arm','/subsystem:windows,6.03','/entry:_DllMainCRTStartup','/nodefaultlib',base/'umd/umd_entry.obj',base/'umd/umd_typed_entry.obj',out/archive['file'],pair/'pco_pair.obj',*libs,'/out:'+str(out/'ge8300-umd.dll'),'/map:'+str(out/'ge8300-umd.map')])
thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
run([a.sign_tool,'sign','/fd','SHA256','/sha1',thumb,out/'ge8300-umd.dll']);run([a.sign_tool,'verify','/pa',out/'ge8300-umd.dll'])
pkg=out/'package335';pkg.mkdir()
for f in pkg0.iterdir():
 if f.is_file() and f.name not in ['ge8300-render.inf','ge8300-render.cat','ge8300-umd.dll']:
  shutil.copyfile(f,pkg/('baseline332-signed-manifest.json' if f.name=='signed-manifest.json' else f.name))
shutil.copyfile(out/'ge8300-umd.dll',pkg/'ge8300-umd.dll')
s=(pkg0/'ge8300-render.inf').read_text(encoding='utf-16');old='DriverVer=11/07/2026,0.0.3.32'
if s.count(old)!=1:raise ValueError('332 INF required')
(pkg/'ge8300-render.inf').write_text(s.replace(old,'DriverVer=11/07/2026,0.0.3.35'),encoding='utf-16')
run([a.inf2cat,'/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime']);run([a.sign_tool,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.cat'])
for n in ['ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw']:run([a.sign_tool,'verify','/pa','/c',pkg/'ge8300-render.cat',pkg/n])
n=dict(m);n.update(schema='lock-entire335-independent-sources-v1',package_source_commit=commit,umd_source_commit=commit,kernel_changed=False,native_owner_changed=False,client_changed=False,existing_clients_changed=False,umd_changed=True,root_reset_fix_proven=False,hardware_verified=False,runtime_verified=False,baseline_manifest_file='baseline332-signed-manifest.json',baseline_manifest_sha256=sha(pkg0/'signed-manifest.json'),inf_driverver='11/07/2026,0.0.3.35',compiled_units=[compiled],archives=[{'role':'umd','file':archive['file'],'sha256':sha(out/archive['file']),'members':[{'name':r['name'],'before_sha256':r['sha256'],'sha256':sha(objdir/r['name']),'compiled':r['name']==row['object']} for r in archive['members']]}],LockEntire=True,NumPages=0,pPages_null=True)
n['retained_outputs']=[r for r in m['outputs'] if r['name'] not in ['ge8300-render.inf','ge8300-render.cat','ge8300-umd.dll']]
for r in n['retained_outputs']:pin(pkg/r['name'],r['sha256'])
n['outputs']=[{'name':r['name'],'sha256':sha(pkg/r['name'])} for r in m['outputs']]
n['retained_provenance']=[{'name':f.name,'sha256':sha(f)} for f in pkg.iterdir() if f.is_file() and f.name not in [r['name'] for r in n['outputs']]]
n.pop('build_manifest_file',None);n.pop('build_manifest_sha256',None)
(pkg/'umd335-build-manifest.json').write_text(json.dumps(n,indent=2)+'\n');n.update(build_manifest_file='umd335-build-manifest.json',build_manifest_sha256=sha(pkg/'umd335-build-manifest.json'))
(pkg/'signed-manifest.json').write_text(json.dumps(n,indent=2)+'\n');(out/'build-manifest.json').write_text(json.dumps(n,indent=2)+'\n')
print('LOCK_ENTIRE335_COMPLETE',commit)
