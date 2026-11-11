#!/usr/bin/env python3
"""既存26objectと検証済み2objectによる、明示opt-inの未署名UMD-only relink。"""
import argparse,hashlib,importlib.util,json,os,re,shutil,struct,subprocess
from pathlib import Path,PureWindowsPath
ROOT=Path(__file__).resolve().parents[2];HERE=ROOT/'tools/ci'
BASE='ace651ba4e8bc9eee2c4f2f7a5e7faafa0c596a6';NEW='323a42f594bb416d456f58df5c0d8aec795d6a5a';PCO='30cb20f79ec3c5f71c056d5783fe6743e92bf2ce'
PREFIX='drivers/gpu/mt8167/wddm-render/'
CHANGED={PREFIX+'umd/typed_draw.c',PREFIX+'umd/typed_entry.c'}
RECIPE=PREFIX+'build-native-development-wdk81.ps1'
BASE_SHA='d9a8e631572493e25a1ff85ff21be50621128b3e33135eae8b270fec21ef7b9a'
BROAD_SHA='d12012a2748d3bda9624fc8fc39d330986ddb5c31c858f9a645e761e874b2271'
# Reuse the public actual-byte COFF archive reader; do not infer member order.
_spec=importlib.util.spec_from_file_location('dwm372_archive_reader',HERE/'build-ge8300-dwm372-incremental.py')
_reader=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_reader)
members=_reader.members

def require(ok,text):
 if not ok:raise ValueError(text)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def git(*args):return subprocess.check_output(['git','-C',str(ROOT),*args])
def arm(p):require(struct.unpack_from('<H',p.read_bytes())[0]==0x1c4,'ARMNT object: '+str(p))
def name(p):return PureWindowsPath(p).name

def admit(stage,newdir,compiler,new_sha):
 locks={}
 def bind(p,h=None):
  p=Path(p).resolve();actual=sha(p);require(h is None or actual==h.lower(),'Input hash: '+str(p));locks[p]=actual;return p
 oldpath=bind(stage/'umd/build-manifest.json',BASE_SHA);old=read(oldpath)
 broadpath=bind(stage/'context/native-build-manifest.json',BROAD_SHA);broad=read(broadpath)
 newpath=bind(newdir/'compile-manifest.json',new_sha);new=read(newpath)
 require(old['source_commit']==BASE and old['source_dirty'] is False and old['unified_display'] and old['limited_development_profile'] and not old['signed'],'Original ace unsigned UMD')
 require(len(broad['sources'])==451,'Original broad451 source witness; not native-cache admission')
 delta=set()
 for row in broad['sources']:
  rel=row['path'].replace('\\','/');p=bind(ROOT/rel)
  if sha(p)!=row['sha256'].lower():delta.add(rel)
 require(delta==CHANGED,'Only UMD2C may differ in original broad451 runtime/header source set')
 require(new['schema']=='ge8300-ia-float-arm2tu-v1' and new['source_commit']==NEW and new['success'] and new['compile_count']==2 and new['link_count']==new['sign_count']==0 and len(new['units'])==2,'Actual successful323a two-TU compile')
 require({x['source'] for x in new['units']}==CHANGED,'Exactly two replacement TUs')
 recipe=bind(ROOT/RECIPE);text=recipe.read_text()
 oldrecipe=git('show',BASE+':'+RECIPE)
 require(recipe.read_bytes()==oldrecipe,'Original UMD source recipe/flags/order unchanged')
 units=re.findall("'([^']+)'",re.search(r'(?m)^ \$units=@\(([^\r\n]+)\)$',text)[1])
 require(len(units)==28 and [x['unit'] for x in old['compiled']]==units,'Original28 unit order')
 skip=['umd/typed_entry','umd/entry','testclient/memory','testclient/d3d-clear','testclient/d3d-texture','testclient/d3d-texture-boundary','testclient/d3d-ui-texture','testclient/dxgi-present356']
 common_expr=re.search(r'(?m)^ \$common=@\(([^\r\n]+)\)$',text)[1]
 common=[m[1] for m in re.findall(r"(['\"])(.*?)\1",common_expr)]
 require(new['UnifiedDisplay']==1,'UnifiedDisplay1')
 newrows={x['source']:x for x in new['units']};records=[];objects={};old_objects={}
 for x in old['compiled']:
  rel=PREFIX+x['unit']+'.c';leaf=x['unit'].replace('/','_')+'.obj';prior=bind(stage/'umd'/leaf,x['object_sha256']);arm(prior);old_objects[leaf]=prior
  if rel in CHANGED:
   n=newrows[rel];p=bind(newdir/n['object'],n['object_sha256']);arm(p)
   require(n['object']==leaf and n['compile_complete'] and n['exit_code']==0 and n['compiled_source_commit']==NEW and n['source_sha256']==sha(ROOT/rel),'New object/source binding')
   bind(newdir/(leaf+'.d'),n['dependency_sha256']);bind(newdir/'logs'/(Path(rel).stem+'.log'),n['log_sha256'])
   flags=n['flags'];inc=PureWindowsPath(next(f[2:] for f in flags if f.startswith('-I') and PureWindowsPath(f[2:]).name.lower()=='shared')).parent
   expected=[f.replace('$inc',str(inc)) for f in common]+['-DA04_GE8300_UNIFIED_DISPLAY356=1','-D_D3D10_CONSTANTS','-D_D3D10_1_CONSTANTS']
   if rel.endswith('typed_entry.c'):expected+=['-DGEO_TYPED_LIMITED_DEVELOPMENT=1','-DGEO_TYPED_NATIVE_PIPELINE10=1']
   require(flags==expected,'Original per-TU compile flags')
   args=n['arguments'];require(args[1:1+len(flags)]==flags and args[1+len(flags):1+len(flags)+4]==['-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types','-MD','-MF'],'Actual compile argv/errors/MD')
   require(args[0]==new['compiler']['path'] and new['compiler']['sha256']==read(HERE/'ge8300-wire378-inputs.json')['tools']['clang'],'Actual clang path/hash')
   require(len(n['actual_dependencies'])>10,'Actual new include closure')
   source_root=PureWindowsPath(args[-3])
   for _ in PureWindowsPath(rel).parts:source_root=source_root.parent
   for dep in n['actual_dependencies']:
    dp=PureWindowsPath(dep['path'])
    if dp.is_relative_to(source_root):bind(ROOT/dp.relative_to(source_root).as_posix(),dep['sha256'])
   origin=NEW
  else:
   require(sha(ROOT/rel)==x['source_sha256'].lower(),'Retained TU source bytes');p=prior;origin=BASE
  objects[leaf]=p;records.append(dict(unit=x['unit'],source=rel,source_sha256=sha(ROOT/rel),object=leaf,sha256=sha(p),compiled_source_commit=origin,reused_existing_object=True,original_object_sha256=x['object_sha256'].lower()))
 require(sum(x['compiled_source_commit']==BASE for x in records)==26,'26old+2new source provenance')
 for row in old['outputs']:bind(stage/'umd'/row['name'],row['sha256'])
 archive=stage/'umd/ge8300-native-umd.lib';oldmembers=members(archive)
 archive_units=[u for u in units if u not in skip];leaves=[u.replace('/','_')+'.obj' for u in archive_units]
 require(len(oldmembers)==20 and {n for n,b in oldmembers}==set(leaves),'Original20 archive members')
 for n,b in oldmembers:require(hashlib.sha256(b).hexdigest()==sha(old_objects[n]),'Original archive/object bytes')
 cm_path=bind(compiler/'link-manifest.json',old['compiler_manifest_sha256']);pm_path=bind(compiler/'pair-manifest.json',old['pair_manifest_sha256']);cm=read(cm_path);pair=read(pm_path)
 require(cm['source_commit']==old['compiler_source_commit']==PCO and cm['source_dirty'] is False and cm['compiled_units']==375 and cm['all_compile_exit_codes_zero'],'Original PCO30cb receipt; no relabel')
 require(pair['source_commit']==PCO and pair['source_dirty'] is False and pair['user_mode_fp'] and not pair['kernel_no_fp_applied'],'Original pair receipt')
 planpath=bind(ROOT/'third_party/mesa-pco/source-plan.json',cm['source_plan_sha256']);plan=read(planpath)
 require(pair['source_plan_sha256']==cm['source_plan_sha256'] and len(plan['files'])==605,'Unchanged PCO605 source plan')
 for row in plan['files']:bind(ROOT/row['path'],row['sha256'])
 for path,h in pair['dependencies'].items():
  normalized=path.replace('\\','/');i=normalized.find(PREFIX)
  if i>=0:bind(ROOT/normalized[i:],h)
 libs=[]
 for row in cm['inputs']:
  if row['role']=='verified standard UMD module':continue
  libs.append(bind(compiler/name(row['path']),row['sha256']))
 require(len(libs)==10 and len({p.name for p in libs})==10,'Original ordered10 link libraries')
 pair_obj=bind(compiler/'pco_pair.obj',pair['object_sha256']);arm(pair_obj)
 receipt=dict(schema='ge8300-ia-float-UMD-only-relink-v1',source_commit=git('rev-parse','HEAD').decode().strip(),baseline_compiled_source_commit=BASE,new_objects_compiled_source_commit=NEW,PCO_compiled_source_commit=PCO,baseline_manifest_sha256=BASE_SHA,broad451_recorded_source_commit=broad['source_commit'],broad451_manifest_sha256=BROAD_SHA,new_compile_manifest_sha256=new_sha,compiler_manifest_sha256=sha(cm_path),pair_manifest_sha256=sha(pm_path),objects=records,original_archive_member_order=[n for n,b in oldmembers],archive_command_order=leaves,library_order=[dict(path=str(p),sha256=sha(p)) for p in libs],pco_pair_sha256=sha(pair_obj),compile_count=0,archive_count=0,link_count=0,sign_count=0,native67_used=False,native_cache_guard_called=False,KMD_rebuilt=False,SDK_setup=False,PCO_rebuilt=False,strict_cache_qualified=False,historical_environment_header_snapshot_complete=False,qualification='Explicit retained candidate; broad451 current source+PCO605 and actual objects verified, not a proof of every historical environment/header input',signed=False,hardware_verified=False,success=False,steps={})
 return receipt,locks,objects,leaves,oldmembers,libs,pair_obj

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for n in ('baseline-stage','new-compile','compiler-directory','output'):ap.add_argument('--'+n,type=Path,required=True)
 ap.add_argument('--new-compile-manifest-sha256',required=True)
 for n in ('llvm-lib','lld-link','readobj'):ap.add_argument('--'+n,type=Path)
 ap.add_argument('--execute-link',action='store_true');a=ap.parse_args()
 require(not git('status','--porcelain'),'Clean published recipe source')
 out=a.output.resolve();require(not out.exists() and out.is_relative_to(ROOT),'Fresh checkout-local output only')
 for p in (a.baseline_stage.resolve(),a.new_compile.resolve(),a.compiler_directory.resolve()):require(not out.is_relative_to(p) and not p.is_relative_to(out),'Separate immutable input/output trees')
 receipt,locks,objects,leaves,oldmembers,libs,pair_obj=admit(a.baseline_stage.resolve(),a.new_compile.resolve(),a.compiler_directory.resolve(),a.new_compile_manifest_sha256)
 if not a.execute_link:print(json.dumps(receipt,indent=2));return
 require(os.name=='nt','Native Windows link execution only')
 tools={'llvm-lib':a.llvm_lib,'lld-link':a.lld_link,'llvm-readobj':a.readobj};pins=read(HERE/'ge8300-wire378-inputs.json')['tools']
 for n,p in tools.items():require(p and sha(p)==pins[n],'Pinned existing tool '+n);locks[p.resolve()]=sha(p)
 out.mkdir(parents=True);(out/'objects').mkdir();(out/'logs').mkdir();(out/'provenance').mkdir()
 def save():(out/'relink-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 def check():
  require(git('rev-parse','HEAD').decode().strip()==receipt['source_commit'] and not git('status','--porcelain'),'Recipe source changed')
  for p,h in locks.items():require(sha(p)==h,'Immutable input changed: '+str(p))
 def run(label,args):
  require(label not in receipt['steps'],'No stage replay');receipt['steps'][label]=dict(arguments=list(map(str,args)),exit_code=None);save()
  env=dict(os.environ,TMP=str(out/'logs'),TEMP=str(out/'logs'));p=subprocess.run(list(map(str,args)),cwd=ROOT,env=env,capture_output=True,text=True,errors='replace');log=out/'logs'/(label+'.log');log.write_text(p.stdout+p.stderr);receipt['steps'][label].update(exit_code=p.returncode,log_sha256=sha(log));save();require(p.returncode==0,'Stage failed; retain completions: '+label);return p.stdout
 try:
  save();check()
  for leaf,p in objects.items():shutil.copyfile(p,out/'objects'/leaf);require(sha(out/'objects'/leaf)==sha(p),'Copied object hash')
  for label,p in [('baseline-umd',a.baseline_stage/'umd/build-manifest.json'),('broad451-source-witness',a.baseline_stage/'context/native-build-manifest.json'),('new-twoTU',a.new_compile/'compile-manifest.json'),('original-PCO30cb',a.compiler_directory/'link-manifest.json'),('original-pair30cb',a.compiler_directory/'pair-manifest.json')]:shutil.copyfile(p,out/'provenance'/(label+'.json'))
  archive=out/'ge8300-native-umd.lib'
  run('archive',[tools['llvm-lib'],'/machine:arm','/out:'+str(archive),*[out/'objects'/n for n in leaves]])
  receipt['archive_count']=1;save()
  actual=members(archive);require([n for n,b in actual]==[n for n,b in oldmembers],'Preserve real archive member order')
  for n,b in actual:require(hashlib.sha256(b).hexdigest()==sha(out/'objects'/n),'Archive member payload')
  check();dll=out/'ge8300-umd.dll'
  run('DLL-link',[tools['lld-link'],'/dll','/machine:arm','/subsystem:windows,6.03','/entry:_DllMainCRTStartup','/nodefaultlib',out/'objects/umd_entry.obj',out/'objects/umd_typed_entry.obj',archive,pair_obj,*libs,'/out:'+str(dll),'/map:'+str(out/'ge8300-umd.map')]);receipt['link_count']=1;save()
  before=run('baseline-PE',[tools['llvm-readobj'],'--file-headers','--coff-imports','--coff-exports',a.baseline_stage/'umd/ge8300-umd.dll']);after=run('candidate-PE',[tools['llvm-readobj'],'--file-headers','--coff-imports','--coff-exports',dll])
  symbols=lambda s:[l.strip() for l in s.splitlines() if re.match(r'^\s*(Name|Symbol|Ordinal):',l)]
  require('IMAGE_FILE_MACHINE_ARMNT' in after and symbols(before)==symbols(after) and symbols(after),'ARMNT and same imports/exports')
  check();receipt.update(success=True,object_members_order_preserved=True,imports_exports_preserved=True,tools={n:dict(path=str(p),sha256=sha(p)) for n,p in tools.items()},outputs=[dict(path=p.name,bytes=p.stat().st_size,sha256=sha(p)) for p in [archive,dll,out/'ge8300-umd.map']]);save()
 except Exception as e:receipt['error']=str(e);raise
 finally:save()

if __name__=='__main__':main()
