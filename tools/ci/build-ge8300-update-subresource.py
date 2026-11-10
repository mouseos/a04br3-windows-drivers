#!/usr/bin/env python3
"""Two-UMD-TU source delta on pinned UItexture804d; no KMD/FW/PCO rebuild."""
from pathlib import Path
import argparse,json,hashlib,subprocess,shutil,re,zipfile
p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True);p.add_argument('--source-commit',required=True);p.add_argument('--resume-link',action='store_true');a=p.parse_args();t=a.task_root;r=t/'source';base=Path('C:/mt8167-ui-texture359/build');o=t/'build';load=lambda p:json.loads(Path(p).read_text(encoding='utf-8-sig'));sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p,h):
 if sha(p)!=h:raise ValueError('Exact input hash mismatch '+str(p))
def run(cmd,name):
 x=subprocess.run([str(c) for c in cmd],capture_output=True,text=True);(o/'logs'/name).write_text(x.stdout+x.stderr)
 if x.returncode:raise RuntimeError(name+' '+x.stdout[-900:]+x.stderr[-1800:])
 return x
if a.resume_link:
 if not(o/'compile-receipt.json').is_file():raise ValueError('Pinned completed two-TU receipt required')
else:
 if o.exists():raise ValueError('New isolated output required')
 o.mkdir();(o/'logs').mkdir()
llvm=Path('C:/Program Files/LLVM/bin');bm=base/'package-ui-texture359/signed-manifest.json';pin(bm,'959f6410149fbbc49facec2f4702a900cc1360ec4287a7f0007f73690605f93d');old=load(bm)
changes={'drivers/gpu/mt8167/wddm-render/umd/typed_resource.c','drivers/gpu/mt8167/wddm-render/umd/clear_ddi.c'};newheader='drivers/gpu/mt8167/wddm-render/umd/update_subresource.h'
units=[u for u in old['compiled_units'] if u['source'] in changes];assert len(units)==2 and all(u['role']=='umd' for u in units)
# Every reused UMD source/header, including Mencius layout, must match actual
# compiled closure. Do not replace the texture source/header with an older copy.
for u in old['actual_include_closure']:
 if u.get('role') not in ('umd','umd-entry'):continue
 for name,h in u.get('local_closure',{}).items():
  if name not in changes:pin(r/name,h)
for x in old['actual_compiler_link_inputs']:pin(Path(x['path']),x['sha256'])
ar=next(x for x in old['archives'] if x['role']=='umd');pin(base/ar['file'],ar['sha256']);d=o/'umd-objects'
if not a.resume_link:d.mkdir();run([llvm/'llvm-ar.exe','x','--output='+str(d),base/ar['file']],'extract.log')
for x in ar['members']:
 if not a.resume_link or x['name']not in {u['object'] for u in units}:pin(d/x['name'],x['sha256'])
compiled=[];closures=[]
for u in units:
 flags=[]
 for v in u['arguments']:
  v=re.sub('/+','/',v.replace('\\','/'));v=re.sub(r'C:/mt8167-[^/]+/source/',r.as_posix()+'/',v);flags.append(v)
 src=r/u['source'];dest=d/u['object']
 if a.resume_link:
  receipt=load(o/'compile-receipt.json');row=next(v for v in receipt['units'] if v['source']==u['source']);pin(src,row['source_sha256']);pin(dest,row['object_sha256']);text=(o/'logs'/(u['object']+'.include.log')).read_text()
 else:text=run([llvm/'clang.exe',*flags,'-H','-E',src,'-o','NUL'],u['object']+'.include.log').stderr
 local={u['source']}
 for line in text.splitlines():
  m=re.match(r'^\.+ (.+)$',line)
  if m:
   try:local.add(Path(m[1].strip()).resolve().relative_to(r.resolve()).as_posix())
   except ValueError:pass
 if not a.resume_link:run([llvm/'clang.exe',*flags,'-c',src,'-o',dest],u['object']+'.compile.log')
 compiled.append(dict(source=u['source'],object=u['object'],source_sha256=sha(src),object_sha256=sha(dest),arguments=flags))
 closures.append(dict(source=u['source'],local_closure={n:sha(r/n) for n in sorted(local)}))
if not a.resume_link:(o/'compile-receipt.json').write_text(json.dumps(dict(source_commit=a.source_commit,units=compiled),indent=2)+'\n')
assert newheader in next(c['local_closure'] for c in closures if c['source'].endswith('typed_resource.c'))
changed=[]
for x in ar['members']:
 if sha(d/x['name'])!=x['sha256']:changed.append(x['name'])
assert set(changed)=={u['object'] for u in units},changed
run([llvm/'llvm-ar.exe','rcs',o/ar['file'],*[d/x['name'] for x in ar['members']]],'archive.log')
entry=next(u for u in old['compiled_units'] if u['source'].endswith('/umd/typed_entry.c'));entryfile=base/entry['object'];pin(entryfile,entry['object_sha256'])
start=Path('C:/mt8167-startgap359/build');pair=start/'pco_pair.obj';bootstrap=start/'umd_entry.obj'
for name,file in [('umd_entry.obj',bootstrap),('pco_pair.obj',pair)]:pin(file,next(u['object_sha256'] for u in old['compiled_units'] if u['object']==name))
# Match the exact linker inputs of the sealed texture candidate, not a new SDK.
args=[llvm/'lld-link.exe','/dll','/machine:arm','/subsystem:windows,6.03','/entry:_DllMainCRTStartup','/nodefaultlib',bootstrap,entryfile,o/ar['file'],pair,*[x['path'] for x in old['actual_compiler_link_inputs']],'/out:'+str(o/'ge8300-umd.dll'),'/map:'+str(o/'ge8300-umd.map')]
run(args,'link.log');sign=Path('C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe');thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
run([sign,'sign','/fd','SHA256','/sha1',thumb,o/'ge8300-umd.dll'],'sign.log');run([sign,'verify','/pa','/v',o/'ge8300-umd.dll'],'verify.log')
# Export/name metadata inspection only; no disassembly.
run([llvm/'llvm-readobj.exe','--file-headers','--coff-imports','--coff-exports',o/'ge8300-umd.dll'],'pe.log')
manifest=dict(packaging_script_sha256=sha(Path(__file__)),resumed_link_without_compile=a.resume_link,schema='GE8300-DEFAULT-UPDATE-SUBRESOURCE-UMD-DELTA-V1',source_commit=a.source_commit,source_runtime_files=sorted(changes|{newheader}),baseline_manifest_sha256=sha(bm),baseline_SYS_sha256=sha(base/'package-ui-texture359/ge8300-render.sys'),baseline_UMD_sha256=sha(base/'package-ui-texture359/ge8300-umd.dll'),candidate_UMD_sha256=sha(o/'ge8300-umd.dll'),compiled_TUs=compiled,actual_include_closure=closures,archive=dict(name=ar['file'],sha256=sha(o/ar['file']),changed_members=changed,reused_members=[dict(name=x['name'],sha256=x['sha256'])for x in ar['members']if x['name']not in changed]),link_arguments=[str(x) for x in args],unchanged_link_inputs=[dict(path=str(x),sha256=sha(x))for x in [bootstrap,entryfile,pair]],compiler_link_inputs=old['actual_compiler_link_inputs'],signer=thumb,KMD_compiled=0,native_compiled=0,SDK_setup=0,PCO_compiled=0,hardware_verified=False,standalone_driver_package=False,final_forward_integration_and_catalog_required=True)
shutil.copyfile(bm,o/'baseline-ui-texture-signed-manifest.json');(o/'build-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
with zipfile.ZipFile(t/'runtime-delta.zip','w',zipfile.ZIP_DEFLATED)as z:
 for x in sorted(o.rglob('*')):
  if x.is_file():z.write(x,x.relative_to(o).as_posix())
print('UPDATE_SUBRESOURCE_2TU_SIGNED_FORWARD_DELTA',manifest['candidate_UMD_sha256'])
