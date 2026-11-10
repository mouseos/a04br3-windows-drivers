#!/usr/bin/env python3
"""Nine-TU texture delta on sealed359; preserve start/display/FW/compiler objects."""
from pathlib import Path
import argparse,json,hashlib,subprocess,shutil,re,copy,ntpath,zipfile
p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True);p.add_argument('--source-commit',required=True);p.add_argument('--resume-link',action='store_true');p.add_argument('--packaging-commit');a=p.parse_args();t=a.task_root;r=t/'source';b=Path('C:/mt8167-startgap359/build');o=t/'build';pkg=o/'package-ui-texture359';llvm=Path('C:/Program Files/LLVM/bin');sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();load=lambda p:json.loads(Path(p).read_text(encoding='utf-8-sig'))
def pin(p,h):
 if sha(p)!=h:raise ValueError('Input pin mismatch '+str(p))
def run(cmd,name):
 q=subprocess.run([str(x)for x in cmd],capture_output=True,text=True);(o/'logs'/name).write_text(q.stdout+q.stderr)
 if q.returncode:raise RuntimeError(name+' '+q.stdout[-1600:]+q.stderr[-1600:])
 return q.stdout+q.stderr
pin(b/'package359/signed-manifest.json','3ecbc3a5df565251f3969e7e45c4465d281b183ba6333df1d761da01cfc9f140');old=load(b/'package359/signed-manifest.json')
for x in old['outputs']:pin(b/'package359'/x['name'],x['sha256'])
for ar in old['archives']:pin(b/ar['file'],ar['sha256'])
if a.resume_link:
 if not o.is_dir()or not pkg.is_dir():raise ValueError('Existing compiled workspace required')
else:
 if o.exists():raise ValueError('New output required')
 o.mkdir();(o/'logs').mkdir();shutil.copytree(b/'package359',pkg)
run(['python',r/'tools/ci/verify-ge8300-fullsource.py'],'fullsource.log')
m=copy.deepcopy(old);expected=set(load(t/'delta-input.json')['runtime_files']);changed_headers={p for p in expected if p.endswith('.h')};affected={u['source'] for u in old['actual_include_closure'] if expected&set(u.get('local_closure',{}))}|{p for p in expected if p.endswith('.c')}
units=[u for u in m['compiled_units'] if u['source']in affected]
if len(units)!=9:raise ValueError('Exactly nine texture-dependent TUs required '+str([(u['role'],u['source'])for u in units]))
for ar in m['archives']:
 if ar['role']not in ('native','kmd','umd'):continue
 d=o/(ar['role']+'-objects')
 if not a.resume_link:
  d.mkdir();run([llvm/'llvm-ar.exe','x','--output='+str(d),b/ar['file']],ar['role']+'.extract.log')
  for x in ar['members']:pin(d/x['name'],x['sha256'])
closures={};selected=[]
for u in units:
 src=r/u['source'];flags=[]
 for arg in u['arguments']:
  arg=re.sub('/+','/',str(arg).replace('\\','/'))
  arg=re.sub(r'C:/mt8167-[^/]+/source/',r.as_posix()+'/',arg)
  flags.append(arg)
 dest=o/(u['role']+'-objects')/u['object']if u['role']in ('native','kmd','umd')else o/u['object']
 include_log=o/'logs'/(u['role']+'-'+u['object']+'.include.log')
 if not a.resume_link:
  pp=subprocess.run([str(llvm/'clang.exe'),*flags,'-H','-E',str(src),'-o','NUL'],capture_output=True,text=True);include_log.write_text(pp.stderr);pp.check_returncode()
 else:
  if not dest.is_file()or not include_log.is_file()or not(o/'logs'/(u['role']+'-'+u['object']+'.compile.log')).is_file():raise ValueError('Missing compiled TU witness '+u['source'])
 local={u['source']}
 for line in include_log.read_text().splitlines():
  match=re.match(r'^\.+ (.+)$',line)
  if match:
   try:local.add(Path(match.group(1).strip()).resolve().relative_to(r.resolve()).as_posix())
   except ValueError:pass
 if not a.resume_link:run([llvm/'clang.exe',*flags,'-c',src,'-o',dest],u['role']+'-'+u['object']+'.compile.log')
 row=dict(source=u['source'],source_sha256=sha(src),object=u['object'],object_sha256=sha(dest),role=u['role'],arguments=flags,actual_compile_source_commit=a.source_commit);u.update(row);selected.append(row)
 closures[(u['role'],u['source'])]=dict(source=u['source'],role=u['role'],local_closure={p:sha(r/p)for p in sorted(local)},compile_required=True)
for c in old['actual_include_closure']:
 if c['source']not in affected and expected&set(c.get('local_closure',{})):raise ValueError('Omitted header consumer '+c['source'])
archives=[];deltas={}
for ar in m['archives']:
 if ar['role']in ('native','kmd','umd'):
  d=o/(ar['role']+'-objects')
  if not a.resume_link:run([llvm/'llvm-ar.exe','rcs',o/ar['file'],*[d/x['name']for x in ar['members']]],ar['role']+'.archive.log')
  ar['sha256']=sha(o/ar['file']);deltas[ar['role']]=[]
  for x in ar['members']:
   h=sha(d/x['name']);x['before_sha256']=x['sha256'];x['sha256']=h
   if h!=x['before_sha256']:deltas[ar['role']].append(x['name'])
 else:assert ar['role']=='pco';pin(b/ar['file'],ar['sha256'])
 archives.append(ar)
if {k:len(v)for k,v in deltas.items()}!={'native':3,'kmd':1,'umd':4}:raise ValueError('Unexpected archive delta '+str(deltas))
# Explicit old359 source objects, byte-preserved display/POST/start/FW ownership.
kit=Path('C:/mt8167-public-fullsource323/wdk81/Lib/winv6.3/km/arm');link=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm/link.exe');pin(link,'f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938')
bdd=[b/('display_'+n+'.obj')for n in ['bdd','bdd_dmm','bdd_util','bltfuncs','blthw','mt8167_brightness','unified_display','memory','ge8300_trace','ge8300_present']];libs=[kit/(n+'.lib')for n in ['displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk']]
kflags=['/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/largeaddressaware','/tsaware:no','/base:0x400000']
if not a.resume_link:run([link,*kflags,o/'ge8300-pair-kmd.lib',o/'ge8300-native-owner-core.lib',b/'display_adapter356.obj',*bdd,*libs,'/out:'+str(pkg/'ge8300-render.sys'),'/map:'+str(o/'ge8300-render.map')],'link-kmd.log')
ul=[]
for x in old['compiler_link_inputs']:
 n=ntpath.basename(x['file']);paths=[b/n,Path(x['file']),Path('C:/mt8167-present356/cache/compiler')/n,Path('C:/mt8167-segmented358/cache/compiler')/n,t/'compiler-link-delta'/n,Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/lib/arm')/n,Path('C:/Program Files (x86)/Windows Kits/10/Lib/10.0.17763.0/um/arm')/n,Path('C:/Program Files (x86)/Windows Kits/10/Lib/10.0.17763.0/ucrt/arm')/n]
 q=next((p for p in paths if p.is_file()and sha(p)==x['sha256']),None)
 if q is None:raise ValueError('Missing existing compiler/lib input '+n+' '+x['sha256'])
 ul.append(q)
if len(ul)!=10:raise ValueError('Pinned compiler library closure10')
run([llvm/'lld-link.exe','/dll','/machine:arm','/subsystem:windows,6.03','/entry:_DllMainCRTStartup','/nodefaultlib',b/'umd_entry.obj',o/'umd_typed_entry.obj',o/'ge8300-native-umd.lib',b/'pco_pair.obj',*ul,'/out:'+str(pkg/'ge8300-umd.dll'),'/map:'+str(o/'ge8300-umd.map')],'link-umd.log')
inf=pkg/'ge8300-render.inf';raw=inf.read_bytes();enc='utf-16'if raw[:2]in (b'\xff\xfe',b'\xfe\xff')else'utf-8-sig';text=raw.decode(enc)
if text.count('0.0.3.59')!=1:raise ValueError('Baseline359 INF version')
inf.write_text(text.replace('0.0.3.59','0.0.3.61'),encoding=enc)
sign=Path('C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe');thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
for n in ['ge8300-render.sys','ge8300-umd.dll']:run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/n],n+'.sign.log')
transport=pkg/'display-transport';saved=t/'preserved-display-transport';transport.rename(saved)
try:run(['C:/mt8167-hp1033/inputs/signing/bin/Inf2Cat.exe','/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime'],'catalog.log')
finally:saved.rename(transport)
run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.cat'],'cat-sign.log')
for n in ['ge8300-render.sys','ge8300-umd.dll','ge8300-render.cat']:run([sign,'verify','/pa',pkg/n],n+'.verify.log')
for n in ['ge8300-render.sys','ge8300-render.inf','ge8300-umd.dll','ge8300-mips.fw']:run([sign,'verify','/pa','/c',pkg/'ge8300-render.cat',pkg/n],n+'.CAT.log')
for x in old['outputs']:
 if x['name']not in {'ge8300-render.sys','ge8300-render.inf','ge8300-render.cat','ge8300-umd.dll'}:pin(pkg/x['name'],x['sha256'])
newclosures=[closures.get((c['role'],c['source']),c)for c in old['actual_include_closure']]
m.update(schema='GPU-ui-texture359-independent-delta-v1',package_source_commit=a.source_commit,kernel_binary_source_commit=a.source_commit,compile_source_commit=a.source_commit,packaging_source_commit=a.packaging_commit or a.source_commit,packaging_script_sha256=sha(Path(__file__)),inf_driverver='11/09/2026,0.0.3.61',compiled_ui_texture_units=selected,archives=archives,actual_include_closure=newclosures,texture_archive_delta=deltas,unchanged_display_POST_start_objects=[dict(file=str(p),sha256=sha(p))for p in [b/'display_adapter356.obj',*bdd]],runtime_build_id='Inherited GPU359 module ID; texture delta identified by exact SYS/UMD/source hashes',baseline359_manifest_sha256=sha(b/'package359/signed-manifest.json'),Carson360_included=False,clock185_included=False,hardware_verified=False,full_DWM_shader_support=False,per_allocation_bytes=4194304,texture_descriptor_edge_max=16384,texture_stride_bytes_max=131072,SDK_rebuilt=False,PCO_rebuilt=False,diagnostic_only=False,texture_functional_delta=True,resumed_link_without_recompile=a.resume_link,actual_compiler_link_inputs=[dict(path=str(p),sha256=sha(p))for p in ul])
m['outputs']=[dict(name=x['name'],sha256=sha(pkg/x['name']))for x in old['outputs']];m.pop('build_manifest_sha256',None);(o/'build-manifest-ui-texture.json').write_text(json.dumps(m,indent=2)+'\n');shutil.copyfile(o/'build-manifest-ui-texture.json',pkg/'ui-texture-build-manifest.json');m['build_manifest_sha256']=sha(pkg/'ui-texture-build-manifest.json');(pkg/'signed-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
with zipfile.ZipFile(t/'outputs.zip','w',zipfile.ZIP_DEFLATED)as z:
 for f in sorted(o.rglob('*')):
  if f.is_file():z.write(f,f.relative_to(o).as_posix())
print('UI_TEXTURE_NINE_TU_SIGNED_NO_HARDWARE_SUCCESS',sha(pkg/'signed-manifest.json'),sha(pkg/'ge8300-render.sys'),sha(pkg/'ge8300-umd.dll'))
