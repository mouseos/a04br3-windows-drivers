#!/usr/bin/env python3
"""Build the Present355 KMD over frozen354, retaining real native/UMD provenance."""
import argparse,hashlib,json,shutil,struct,subprocess,zipfile,re,ntpath
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True);p.add_argument('--recipe-commit',required=True);p.add_argument('--umd-build',type=Path,required=True);p.add_argument('--metadata-only',action='store_true');a=p.parse_args()
t=a.task_root;base=t/'baseline';old=base/'package354';out=t/'build';pkg=out/'package355'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def pin(p,h):
 if sha(p)!=h:raise ValueError('Input pin: '+str(p))
def run(c):subprocess.run([str(x)for x in c],check=True)
def header(p):
 b=p.read_bytes();at=struct.unpack_from('<I',b,60)[0];assert b[at:at+4]==b'PE\0\0';o=at+24
 machine,ns=struct.unpack_from('<HH',b,at+4);size=struct.unpack_from('<H',b,at+20)[0]
 return {'machine':machine,'characteristics':struct.unpack_from('<H',b,at+22)[0],'magic':struct.unpack_from('<H',b,o)[0],'subsystem':struct.unpack_from('<H',b,o+68)[0],'dll_characteristics':struct.unpack_from('<H',b,o+70)[0],'sections':{b[o+size+i*40:o+size+i*40+8].rstrip(b'\0').decode():struct.unpack_from('<I',b,o+size+i*40+36)[0]for i in range(ns)}}
if a.metadata_only:
 root=t/'source';root_commit=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
 if root_commit!=a.recipe_commit or subprocess.check_output(['git','-C',str(root),'status','--porcelain'],text=True).strip():raise ValueError('Clean final metadata recipe required')
 n=load(pkg/'signed-manifest.json');baseline=load(old/'signed-manifest.json')
 for row in n['outputs']:pin(pkg/row['name'],row['sha256'])
 compiled_commit=n.get('kernel_binary_source_commit')
 for arc in n['archives']:
  if arc['role']not in('native','kmd'):continue
  before=next(x for x in baseline['archives']if x['role']==arc['role']);old_members={x['name']:x['sha256']for x in before['members']}
  for row in arc['members']:row['before_sha256']=old_members[row['name']]
  if arc['role']=='native' and arc['sha256']!=before['sha256']:raise ValueError('Native must remain byte-identical')
 n['unchanged_linked_object_count']=sum(x['sha256']==x['before_sha256']for arc in n['archives']if arc['role']in('native','kmd')for x in arc['members'])
 n['package_source_commit']=a.recipe_commit;n['metadata_recipe_source_commit']=a.recipe_commit;n['actual_compile_source_commit']=compiled_commit
 n['packaging_metadata_only_resume']=True;n['resume_compile_count']=0;n['resume_link_count']=0;n['resume_sign_count']=0
 n.pop('build_manifest_sha256',None)
 (out/'build-manifest.json').write_text(json.dumps(n,indent=2)+'\n');shutil.copyfile(out/'build-manifest.json',pkg/'present355-build-manifest.json')
 n['build_manifest_sha256']=sha(pkg/'present355-build-manifest.json');(pkg/'signed-manifest.json').write_text(json.dumps(n,indent=2)+'\n')
 with zipfile.ZipFile(t/'outputs-final.zip','w',zipfile.ZIP_DEFLATED)as z:
  for f in out.rglob('*'):
   if f.is_file():z.write(f,f.relative_to(out).as_posix())
 print('PRESENT355_METADATA_ONLY_FINALIZED',sha(pkg/'signed-manifest.json'),n['unchanged_linked_object_count'])
 raise SystemExit(0)
if out.exists():raise ValueError('New output required')
pin(old/'signed-manifest.json','04d69dff41f363e92bee6dcf5d4946f0aecb23be1c7c0e01fa7ce1e5e5fd08d1')
m=load(old/'signed-manifest.json')
um=load(a.umd_build/'build-manifest.json');pin(a.umd_build/'ge8300-umd.dll',um['dll_sha256'])
if um['production_callback_host_test_exit']!=0 or um['primary_scanout_connected'] or um['display_caps_enabled']:raise ValueError('Exact bounded UMD boundary required')
for x in m['outputs']+m['retained_provenance']:pin(old/x['name'],x['sha256'])
libs=[]
for x in m['archives']:
 if x['role'] in ['kmd','native']:pin(base/x['file'],x['sha256']);libs.append(base/x['file'])
link=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm/link.exe')
pin(link,'f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938')
out.mkdir();pkg.mkdir()
root=t/'source';llvm=Path('C:/Program Files/LLVM/bin');ar=llvm/'llvm-ar.exe'
root_commit=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
if root_commit!=a.recipe_commit or subprocess.check_output(['git','-C',str(root),'status','--porcelain'],text=True).strip():raise ValueError('Clean fixed source required')
(out/'generated').mkdir()
asset=(t/'copy.usc').read_bytes()
if sha(t/'copy.usc')!='5c68917faa051238b23674b64c6b137ef7ff75425dbde9dbc5e1e5a36e02c4a3':raise ValueError('Copy USC pin')
(out/'generated/a04_copy_usc_asset.h').write_text('static const unsigned char a04_copy_usc_asset[152]={'+','.join(hex(x)for x in asset)+'};\n')
def relocate(x):
 import re
 x=str(x).replace(chr(92),'/')
 while '//' in x:x=x.replace('//','/')
 x=re.sub(r'C:/mt8167-[^\s]+?/(?:source|src)/(?=drivers|third_party)',str(root).replace(chr(92),'/')+'/',x,flags=re.I)
 x=re.sub(r'C:/mt8167-[^\s]+?/build/(?:copy336/)?generated',str(out/'generated').replace(chr(92),'/'),x,flags=re.I)
 return x
all_rows=load(t/'all-units.json');all_units={Path(x['source']).name:x for x in all_rows}
changed_paths=set(load(t/'changed-sources.json'));dependencies=[];wanted=set();(out/'dependencies').mkdir()
reuse=t/'reuse-include-closure.json'
if reuse.exists():
 prior=load(reuse)
 if prior['checked_units']!=80 or prior['changed_sources']!=sorted(changed_paths):raise ValueError('Reused include closure identity changed')
 for row in prior['units']:
  for n,h in row['local_closure'].items():pin(root/n,h)
 dependencies=prior['units'];wanted=set(prior['affected_units'])
 shutil.copytree(t/'reuse-dependencies',out/'dependencies',dirs_exist_ok=True)
else:
 for u in all_rows:
  args=[relocate(x)for x in u['arguments']];src=root/u['source'];log=out/'dependencies'/(u['object']+'.txt')
  if src.name=='mdl_pin.c':
   # SEH source explicitly rejects Clang. Use its actual MSVC compiler for
   # preprocessing only; no object/source recompilation is requested here.
   pp=[str(link.with_name('cl.exe')),'/nologo','/EP','/showIncludes','/TC']+[('/D'+x[2:]) for x in args if x.startswith('-D')]+[('/I'+x[2:]) for x in args if x.startswith('-I') and '/llvm/' not in x.lower()]+['/I'+str(link.parents[3]/'include'),str(src)]
   result=subprocess.run(pp,capture_output=True,text=True);deptext=result.stdout+'\n'+result.stderr
  else:
   result=subprocess.run([str(llvm/'clang.exe'),*args,'-H','-E',str(src),'-o','NUL'],capture_output=True,text=True);deptext=result.stderr
  log.write_text(deptext)
  if result.returncode:raise ValueError('Preprocessor dependency scan failed '+u['source']+' '+result.stderr[-1000:])
  included={u['source']}
  for line in deptext.splitlines():
   hit=re.match(r'^\.+ (.+)$',line) or re.match(r'^Note: including file:\s*(.+)$',line)
   if hit:
    path=Path(hit.group(1).strip())
    try:included.add(path.resolve().relative_to(root.resolve()).as_posix())
    except ValueError:pass
  affected=sorted(included&changed_paths)
  if affected:wanted.add(Path(u['source']).name)
  dependencies.append({'source':u['source'],'object':u['object'],'role':u['role'],'changed_includes':affected,'local_closure':{n:sha(root/n)for n in sorted(included)},'compile_required':bool(affected),'actual_preprocessor_exit_code':result.returncode,'dependency_log':str(log.relative_to(out))})
(out/'include-closure.json').write_text(json.dumps({'schema':'352-actual-clang-preprocessor-include-closure','checked_units':len(all_rows),'changed_sources':sorted(changed_paths),'affected_units':sorted(wanted),'units':dependencies},indent=2)+'\n')
if any(all_units[n]['role']!='kmd' for n in wanted):raise ValueError('Unexpected native include impact; do not mislabel preserved native')
if len(all_rows)!=80 or not wanted:raise ValueError('Actual full80 dependency closure required')
units=[];archives=[]
for archive in m['archives']:
 updated=dict(archive)
 if archive['role'] not in ('kmd','native'):
  if archive['role']=='umd':
   shutil.copyfile(a.umd_build/'ge8300-native-umd.lib',out/'ge8300-native-umd.lib');updated.update(sha256=sha(out/'ge8300-native-umd.lib'),members=um['archive_members'])
  archives.append(updated);continue
 if not any(all_units[n]['role']==archive['role'] for n in wanted):
  shutil.copyfile(base/archive['file'],out/archive['file']);updated['members']=[dict(x,before_sha256=x['sha256'],compiled=False)for x in archive['members']];archives.append(updated);continue
 objects=out/(archive['role']+'-objects');objects.mkdir()
 names=[ntpath.basename(n)for n in subprocess.check_output([str(ar),'t',str(base/archive['file'])],text=True).splitlines()]
 if len(names)!=len(archive['members']) or set(names)!={x['name']for x in archive['members']}:raise ValueError('Member closure mismatch')
 run([ar,'x','--output='+str(objects),base/archive['file']])
 for member in archive['members']:pin(objects/member['name'],member['sha256'])
 changed=set()
 for name in sorted(wanted):
  u=all_units[name]
  if u['role']!=archive['role']:continue
  args=[relocate(x)for x in u['arguments']];src=root/u['source'];obj=objects/u['object']
  run([llvm/'clang.exe',*args,'-c',src,'-o',obj]);changed.add(obj.name)
  units.append(dict(source=u['source'],source_sha256=sha(src),object=obj.name,object_sha256=sha(obj),arguments=args,role=u['role']))
 members=[]
 for member in archive['members']:
  h=sha(objects/member['name']);is_changed=member['name']in changed
  if not is_changed and h!=member['sha256']:raise ValueError('Unrelated member changed')
  members.append(dict(name=member['name'],sha256=h,before_sha256=member['sha256'],compiled=is_changed))
 run([llvm/'llvm-lib.exe','/machine:arm','/out:'+str(out/archive['file']),*[objects/n for n in reversed(names)]])
 if [ntpath.basename(n)for n in subprocess.check_output([str(ar),'t',str(out/archive['file'])],text=True).splitlines()]!=names:raise ValueError('Archive order changed')
 updated.update(sha256=sha(out/archive['file']),members=members);archives.append(updated)
if len(units)!=len(wanted):raise ValueError('Actual affected unit count mismatch')
kit=Path('C:/mt8167-public-fullsource323/wdk81/Lib/winv6.3/km/arm')
kmd=next(out/x['file']for x in m['archives']if x['role']=='kmd');native=next(out/x['file']for x in m['archives']if x['role']=='native')
args=['/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/largeaddressaware','/tsaware:no','/base:0x400000',kmd,native,*[kit/(n+'.lib')for n in ['displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk']],'/out:'+str(out/'ge8300-render.sys'),'/map:'+str(out/'ge8300-render.map')]
run([link,*args]);h=header(out/'ge8300-render.sys')
if (h['machine'],h['magic'],h['characteristics'],h['subsystem'],h['dll_characteristics'])!=(0x1c4,0x10b,0x122,1,0x140):raise ValueError('RT81 baseline PE contract not restored')
for n in ['.text','.rdata','.data','.pdata']:
 if not h['sections'].get(n,0)&0x08000000:raise ValueError('Baseline nonpageable section contract: '+n)
sign=Path('C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe');thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
run([sign,'sign','/fd','SHA256','/sha1',thumb,out/'ge8300-render.sys']);run([sign,'verify','/pa',out/'ge8300-render.sys'])
for f in old.iterdir():
 if f.is_file()and f.name not in ['ge8300-render.sys','ge8300-render.cat','ge8300-render.inf','signed-manifest.json']:shutil.copyfile(f,pkg/f.name)
shutil.copyfile(old/'signed-manifest.json',pkg/'baseline354-signed-manifest.json');shutil.copyfile(a.umd_build/'ge8300-umd.dll',pkg/'ge8300-umd.dll');shutil.copyfile(out/'ge8300-render.sys',pkg/'ge8300-render.sys')
b=(old/'ge8300-render.inf').read_bytes();enc='utf-16'if b[:2]in [b'\xff\xfe',b'\xfe\xff']else'utf-8-sig';s=b.decode(enc);before='DriverVer=11/09/2026,0.0.3.54';after='DriverVer=11/09/2026,0.0.3.55'
if s.count(before)!=1:raise ValueError('Fixed342 DriverVer required')
(pkg/'ge8300-render.inf').write_text(s.replace(before,after),encoding=enc)
cat=Path('C:/mt8167-hp1033/inputs/signing/bin/Inf2Cat.exe');run([cat,'/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime']);run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.cat'])
for n in ['ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw']:run([sign,'verify','/pa','/c',pkg/'ge8300-render.cat',pkg/n])
for x in m['outputs']:
 if x['name']not in ['ge8300-render.sys','ge8300-render.cat','ge8300-render.inf','ge8300-umd.dll']:pin(pkg/x['name'],x['sha256'])
new=dict(m);new.update(schema='present355-real-copy-boundary-incremental-v1',package_source_commit=a.recipe_commit,link_recipe_source_commit=a.recipe_commit,kernel_binary_source_commit=a.recipe_commit,native_owner_source_commit=m['native_owner_source_commit'],umd_source_commit='bce94f073dd26d0b49eac75f21db85d39e70511d',compiled_units=units,archives=archives,baseline_reused_342_compiled_units=m['reused_342_compiled_units'],unchanged_linked_object_count=sum(x['sha256']==x.get('before_sha256',x['sha256'])for a0 in archives if a0['role']in('kmd','native')for x in a0['members']),compiler_reexecuted=True,source_changed=True,native_owner_changed=False,umd_changed=True,linker_changed=False,linked_objects_reused_without_recompile=False,linker_path=str(link),linker_sha256=sha(link),link_arguments=[str(x)for x in args],PE_contract=h,runtime_build_id='GPU355-present-blt-v1',inf_driverver='11/09/2026,0.0.3.55',baseline_manifest_file='baseline354-signed-manifest.json',baseline_manifest_sha256=sha(pkg/'baseline354-signed-manifest.json'),build_manifest_file='present355-build-manifest.json',hardware_verified=False,runtime_verified=False,root_reset_fix_proven=False)
new['baseline_original_compile_source_commit']=new.pop('original_compile_source_commit',None)
new['baseline_compiled_input_receipt']=new.pop('compiled_input_receipt',None)
new['resume_packaging_without_compile']=False
new['progress_headers']=[{'path':p,'sha256':sha(root/p)}for p in ['drivers/gpu/mt8167/vendor_m1_11/native_factory_checkpoint.h','drivers/gpu/mt8167/vendor_m1_11/render_owner/native_factory_checkpoint.h']]
new['actual_include_closure']=load(out/'include-closure.json')
new['diagnostic_contract']=load(root/'tools/recovery/ge8300-boundaries354-contract.json')
shutil.copyfile(a.umd_build/'build-manifest.json',pkg/'present355-umd-build-manifest.json')
new['umd355_build_receipt']=um;new['umd_compiled_units']=um['compiled_units'];new['primary_scanout_connected']=False;new['display_caps_enabled']=False;new['normal_Windows_display_verified']=False;new['install_authorized']=False
new['outputs']=[{'name':x['name'],'sha256':sha(pkg/x['name'])}for x in m['outputs']]
new['retained_provenance']=[{'name':f.name,'sha256':sha(f)}for f in pkg.iterdir()if f.is_file()and f.name not in {x['name']for x in new['outputs']}]
(out/'build-manifest.json').write_text(json.dumps(new,indent=2)+'\n');shutil.copyfile(out/'build-manifest.json',pkg/'present355-build-manifest.json');new['build_manifest_sha256']=sha(pkg/'present355-build-manifest.json');(pkg/'signed-manifest.json').write_text(json.dumps(new,indent=2)+'\n')
with zipfile.ZipFile(t/'outputs.zip','w',zipfile.ZIP_DEFLATED)as z:
 for f in out.rglob('*'):
  if f.is_file():z.write(f,f.relative_to(out).as_posix())
print('PRESENT355_BOUNDARY_SIGNED_NOT_DISPLAY_VERIFIED',sha(pkg/'signed-manifest.json'),sha(pkg/'ge8300-render.sys'))
