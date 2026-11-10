#!/usr/bin/env python3
"""Opt-in warm196 kernel-only resume. Never compiles native/PCO/UMD or signs."""
import argparse,hashlib,json,os,re,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def pin(p,h):
 if sha(p)!=h.lower():raise ValueError('Pinned input/source changed: '+str(p))
def validate(root,warm,spec):
 integration=read(root/'tools/ci/ge8300-fullsource-origins.json').get('shared_main194_primary_integration')
 if integration and integration.get('warm196_driver_objects_compatible') is False:
  raise ValueError('Integrated driver source requires full native/KMD/UMD recompile; warm196 resume forbidden; retain only pinned PCO tool input')
 for name,h in spec['files'].items():pin(warm/name,h)
 m=read(warm/'warm-build-manifest.json')
 if m['source_commit']!=spec['baseline_source_commit'] or m['source_dirty']:raise ValueError('Wrong warm baseline')
 # Verify source bytes, not a permissive commit-diff whitelist. No old Git history needed.
 seen={}
 for part in ['native','modules']:
  old=read(warm/part/'build-manifest.json')
  if old['source_commit']!=spec['baseline_source_commit'] or old['source_dirty']:raise ValueError('Mixed baseline sources')
  for row in old['sources']:
   name=re.sub('/+','/',row['path'].replace('\\','/'));path=(root/name).resolve()
   if not path.is_relative_to(root):raise ValueError('Escaped source root')
   pin(path,row['sha256']);seen[name]=row['sha256'].lower()
 for row in read(warm/'modules/build-manifest.json')['unified_source_commands']:
  if row['source'].endswith('.cxx'):
   name='drivers/gpu/mt8167/'+row['source'];pin(root/name,row['sha256']);seen[name]=row['sha256'].lower()
 # Extra C/H files could shadow an old include: reject additions too.
 actual={p.relative_to(root).as_posix() for p in (root/'drivers/gpu/mt8167').rglob('*') if p.is_file() and p.suffix in ['.c','.h']}
 if actual!={n for n in seen if Path(n).suffix in ['.c','.h']}:raise ValueError('GPU C/H source closure added/dropped')
 return seen

def main():
 p=argparse.ArgumentParser();p.add_argument('--enable-candidate',action='store_true');p.add_argument('--validate-only',action='store_true')
 p.add_argument('--warm-directory',type=Path,default=Path('C:/mt8167-shared-surface-warm196/build/shared-surface-warm-f4'))
 p.add_argument('--kernel-vc-tools-directory',type=Path,default=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133'))
 p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 if not a.enable_candidate:raise ValueError('Explicit candidate opt-in required')
 root=ROOT;warm=a.warm_directory.resolve();out=a.output.resolve();specpath=root/'tools/ci/ge8300-warm196-resume-pins.json';spec=read(specpath)
 if subprocess.check_output(['git','-C',str(root),'status','--porcelain']):raise ValueError('Clean fixed source required')
 seen=validate(root,warm,spec)
 if a.validate_only:print(json.dumps({'source_snapshot_matches':True,'source_files':len(seen),'pinned_cache_files':len(spec['files']),'build_executed':False}));return
 if os.name!='nt':raise ValueError('Windows execution only; use --validate-only for host source checks')
 if out.exists() or not out.is_relative_to(root) or out.is_relative_to(warm):raise ValueError('Fresh separate checkout-local output required')
 vc=a.kernel_vc_tools_directory.resolve();cl=vc/'bin/Hostx64/arm/cl.exe';link=vc/'bin/Hostx64/arm/link.exe';pin(cl,spec['CL_sha256']);pin(link,spec['LINK_sha256'])
 km=Path(os.environ['ProgramFiles(x86)'])/'Windows Kits/8.1/Lib/winv6.3/km/arm';pin(km/'displib.lib',spec['displib_sha256'])
 for row in read(warm/'native/build-manifest.json')['wdk_inputs']:pin(Path(row['path']),row['sha256'])
 out.mkdir();(out/'logs').mkdir();(out/'objects').mkdir();env=dict(os.environ,VSLANG='1033',TEMP=str(out/'logs'),TMP=str(out/'logs'))
 def run(args,name):
  x=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=env);(out/'logs'/name).write_text(x.stdout+x.stderr)
  if x.returncode:raise RuntimeError(name)
  return x.stdout+x.stderr
 modules=read(warm/'modules/build-manifest.json');oldvc=read(warm/'warm-build-manifest.json')['licensed_toolchain_directory'];records=[];replacement={}
 for row in modules['unified_source_commands']:
  if not row['source'].endswith('.cxx'):continue
  source=root/'drivers/gpu/mt8167'/row['source'];obj=out/'objects'/row['object'];args=[]
  for flag in row['arguments']:
   flag=re.sub('/+','/',flag.replace('\\','/'))
   if 'MSVC/14.44.35207/include' in flag:flag='/I'+(vc/'include').as_posix()
   if flag.startswith('/I') and '/drivers/gpu/mt8167/wddm-display' in flag:flag='/I'+(root/'drivers/gpu/mt8167/wddm-display').as_posix()
   args.append(flag)
  text=run([cl,*args,'/showIncludes',source,'/Fo'+str(obj)],obj.stem+'.log');closure={source.relative_to(root).as_posix():sha(source)}
  for line in text.splitlines():
   if 'including file:' in line:
    header=Path(line.split('including file:',1)[1].strip());closure[str(header)]=sha(header)
  if len(closure)<2:raise ValueError('MSVC include trace missing; cannot record new compile closure')
  replacement[obj.name]=obj;records.append({'source':source.relative_to(root).as_posix(),'source_sha256':sha(source),'object':obj.name,'object_sha256':sha(obj),'arguments':args,'actual_include_closure':closure})
 if len(replacement)!=10:raise ValueError('Exactly10 display C++ units required')
 # Reconstruct the exact baseline SEH recipe; only CL and VC CRT include change.
 source=root/'drivers/gpu/mt8167/wddm-render/mdl_pin.c';obj=out/'objects/mdl_pin.obj';inc=km.parents[3]/'Include'
 args=['/nologo','/c','/O2','/GS-','/kernel','/Zl','/TC','/DA04_GE8300_UNIFIED_DISPLAY356=1','/D_ARM_','/DARM','/D_KERNEL_MODE','/D_WIN32_WINNT=0x0603','/DNTDDI_VERSION=0x06030000',*[f'/I{inc/x}' for x in ['shared','km','km/crt']],'/I'+str(vc/'include')]
 text=run([cl,*args,'/showIncludes',source,'/Fo'+str(obj)],'mdl_pin.log');closure={source.relative_to(root).as_posix():sha(source)}
 for line in text.splitlines():
  if 'including file:' in line:
   header=Path(line.split('including file:',1)[1].strip());closure[str(header)]=sha(header)
 if len(closure)<2:raise ValueError('SEH include trace missing')
 replacement[obj.name]=obj;records.append({'source':source.relative_to(root).as_posix(),'source_sha256':sha(source),'object':obj.name,'object_sha256':sha(obj),'arguments':args,'actual_include_closure':closure})
 # Re-archive a declared complete24-member KMD set, never mix old MSVC members.
 base=['clear_protocol','frontend_draw_protocol','clear_engine','capture','kmd_clear','kmd_objects','query_adapter','preempt','render_contract','lifecycle','miniport','frontend_memory','mdl_pin','display_adapter356']
 members=[replacement.get(n+'.obj',warm/'modules'/(n+'.obj')) for n in base]+[v for k,v in replacement.items() if k!='mdl_pin.obj']
 archive=out/'ge8300-kmd-clear.lib';run(['llvm-lib','/machine:arm','/out:'+str(archive),*members],'archive.log')
 native=warm/'native/ge8300-native-owner-core.lib';linkargs=['/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/largeaddressaware','/tsaware:no','/base:0x400000']
 for name,kernel in [('ge8300-render',archive),('mt8167-display-transport',warm/'modules/ge8300-display-transport.lib')]:
  run([link,*linkargs,kernel,native,*[km/(n+'.lib') for n in ['displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk']],'/out:'+str(out/(name+'.sys')),'/map:'+str(out/(name+'.map'))],name+'-link.log')
  (out/(name+'-link.json')).write_text(json.dumps({'linker_sha256':sha(link),'arguments':linkargs,'output_sha256':sha(out/(name+'.sys')),'displib_sha256':sha(km/'displib.lib')},indent=2)+'\n')
 # Preserve original evidence; receipts explicitly distinguish old compile source
 # from today's recipe and the newly compiled11 source-identical kernel units.
 for name in ['ge8300-umd.dll','ge8300-d3d-clear.exe','ge8300-d3d-texture.exe','ge8300-d3d-texture-boundary.exe','ge8300-d3d-ui-texture.exe']:shutil.copyfile(warm/name,out/name)
 shutil.copyfile(warm/'warm-build-manifest.json',out/'baseline-warm-build-manifest.json');shutil.copyfile(warm/'prebuilt-compiler-input.json',out/'prebuilt-compiler-input.json')
 validate(root,warm,spec)
 manifest={'schema':'warm196-msvc1429-resume-v1','recipe_source_commit':subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),'baseline_compiled_source_commit':spec['baseline_source_commit'],'source_snapshot_matches_current':True,'source_snapshot':seen,'baseline_manifest_sha256':spec['warm_manifest_sha256'],'resume_spec_sha256':sha(specpath),'MSVC_compiled_units':records,'MSVC_compile_count':11,'CL_sha256':sha(cl),'LINK_sha256':sha(link),'native_recompiled':0,'PCO_recompiled':0,'UMD_recompiled':0,'SDK_rebuilt':0,'native_archive_sha256':sha(native),'KMD_archive_sha256':sha(archive),'CompilerSourceCommit':'f4b92404c4bbe139524a997c5427a8245560e522','includeClosureRecorded':False,'UCRT_headers':'baseline22621 retained; not GPU36626100 equivalence','signed':False,'Registry_reference_static_check_pending':True,'hardware_verified':False,'DWM_verified':False,'RT_SRV_implemented':False,'blend_implemented':False,'outputs':{p.name:sha(p) for p in out.iterdir() if p.is_file()}}
 (out/'resume-build-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print('WARM196_MSVC1429_UNSIGNED_RESUME_COMPLETE')
if __name__=='__main__':main()
