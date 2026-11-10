#!/usr/bin/env python3
"""固定3dの実回収入力からlinkのみ再開。compile/SDK/PCO再buildは行わない。"""
import argparse,hashlib,json,os,shutil,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def pin(p,h):
 if sha(p)!=h.lower():raise ValueError('SHA mismatch: '+str(p))
def write(p,v):Path(p).write_text(json.dumps(v,indent=2)+'\n')
def members(path):
 data=Path(path).read_bytes()
 if data[:8]!=b'!<arch>\n':raise ValueError('Not a COFF archive: '+str(path))
 pos=8;longnames=b'';result={}
 while pos<len(data):
  h=data[pos:pos+60]
  if len(h)!=60 or h[58:]!=b'`\n':raise ValueError('Invalid archive header')
  size=int(h[48:58]);body=data[pos+60:pos+60+size];name=h[:16].decode().strip();pos+=60+size+(size%2)
  if len(body)!=size:raise ValueError('Truncated archive')
  if name=='//':longnames=body;continue
  if name=='/':continue
  if name.startswith('/') and name[1:].isdigit():
   off=int(name[1:]);tail=longnames[off:];ends=[x for x in (tail.find(b'\x00'),tail.find(b'/\n')) if x>=0]
   if not ends:raise ValueError('Invalid long archive name')
   name=tail[:min(ends)].decode()
  name=name.rstrip('/').replace('\\','/').rsplit('/',1)[-1]
  if name in result:raise ValueError('Duplicate archive member: '+name)
  result[name]=hashlib.sha256(body).hexdigest()
 return result
UNITS=['clear_protocol','frontend/draw_protocol','clear_engine','capture','kmd_clear','kmd_objects','query_adapter','preempt','render_contract','lifecycle','miniport','frontend_memory','umd/clear_ddi','umd/bootstrap','umd/frontend','umd/shader_state','umd/tokenized_shader','compiler/sm2','umd/entry','testclient/d3d-clear','testclient/d3d9-flat','testclient/memory']
DISPLAY=['bdd','bdd_dmm','bdd_util','bltfuncs','blthw','mt8167_brightness','unified_display','memory','ge8300_trace','ge8300_present']
KMD=[u.replace('/','_')+'.obj' for u in UNITS[:12]]+['mdl_pin.obj','display_adapter356.obj']+['display_'+u+'.obj' for u in DISPLAY]
UMD=['clear_protocol.obj','frontend_draw_protocol.obj','umd_clear_ddi.obj','umd_bootstrap.obj','umd_frontend.obj','umd_shader_state.obj','umd_tokenized_shader.obj','compiler_sm2.obj','testclient_memory.obj']
def validate(source,build,spec):
 for name,h in spec['source_files'].items():
  data=(source/name).read_bytes()
  if name.endswith('.ps1'):data=data.replace(b'\r\n',b'\n')
  if hashlib.sha256(data).hexdigest()!=h:raise ValueError('Fixed3d source/recipe changed: '+name)
 pin(build/'pre-resume-inventory.json',spec['inventory_sha256'])
 for name,h in spec['files'].items():pin(build/name,h)
 # Every archived member must be the same byte sequence as the actual saved obj.
 for name,expected in [('ge8300-kmd-clear.lib',KMD),('ge8300-umd-clear.lib',UMD)]:
  actual=members(build/'modules'/name)
  if set(actual)!=set(expected):raise ValueError('Wrong archive member set: '+name)
  for obj in expected:
   if actual[obj]!=sha(build/'modules'/obj):raise ValueError('Archive differs from saved object: '+obj)
 for obj in (build/'modules').glob('*.obj'):
  if obj.name not in {Path(n).name for n in spec['files'] if n.startswith('modules/')}:raise ValueError('Unpinned extra module object')
  if obj.read_bytes()[:2]!=b'\xc4\x01':raise ValueError('Non-ARMNT COFF object: '+obj.name)
 native=read(build/'native/build-manifest.json')
 if native['source_commit']!=spec['compiled_source_commit'] or native['source_dirty'] or len(native['units'])!=67:raise ValueError('Wrong native source provenance')
 for row in native['outputs']:pin(build/'native'/row['path'],row['sha256'])
 receipt=read(build/'umd-input/receipt.json')
 if receipt['UMDSourceCommit']!='aa6613149e09b7b7be718c90103a34323c362917' or receipt['current_kernel_source_commit']!=spec['compiled_source_commit'] or receipt['signed']:raise ValueError('Wrong fixed UMD receipt')
 pin(build/'umd-input/ge8300-umd.dll',receipt['dll_sha256'])
 return {'compiled_source_commit':spec['compiled_source_commit'],'fixed_source_files':len(spec['source_files']),'saved_inputs_checked':len(spec['files']),'KMD_members_checked':len(KMD),'UMD_module_members_checked':len(UMD),'native_units':67,'compile_invocations':0,'module_historical_include_trace_recorded':False,'module_metadata_reconstructed_afterfailure':True}
def validate_existing_sys(directory,spec,record,acknowledged):
 if not acknowledged:raise ValueError('Explicit acknowledgement of existing fixed-input SYS links required')
 # The original link script records output/tool/displib SHA, not input archives.
 # Preserve main's separate record, and do not upgrade it to independent input proof.
 result={};pins={};main_record=read(record)
 for name,meta,mapname in [('ge8300-render.sys','kernel-link.json','ge8300-render.map'),('mt8167-display-transport.sys','transport-link.json','transport.map')]:
  m=read(directory/meta);pin(directory/name,m['output_sha256'])
  if m['sha256'].lower()!=spec['LINK1429_sha256'] or m['displib_sha256'].lower()!=spec['displib_sha256']:raise ValueError('Existing SYS linker/displib mismatch')
  data=(directory/name).read_bytes()
  if data[:2]!=b'MZ':raise ValueError('Existing SYS not PE')
  offset=struct.unpack_from('<I',data,0x3c)[0]
  if data[offset:offset+4]!=b'PE\x00\x00' or struct.unpack_from('<H',data,offset+4)[0]!=0x1c4:raise ValueError('Existing SYS not ARMNT')
  for n in [name,meta,mapname]:pins[str(directory/n)]=sha(directory/n)
  result[name]={'output_sha256':sha(directory/name),'link_metadata_sha256':sha(directory/meta),'map_sha256':sha(directory/mapname)}
 pins[str(record)]=sha(record)
 return {'existing_SYS_outputs':result,'existing_link_input_archives_independently_bound_by_link_metadata':False,'main_fixed_input_link_attestation_acknowledged':True,'main_record_sha256':sha(record),'main_record':main_record},pins

def main():
 p=argparse.ArgumentParser();p.add_argument('--enable-candidate',action='store_true');p.add_argument('--validate-only',action='store_true');p.add_argument('--acknowledge-reconstructed-afterfailure',action='store_true')
 p.add_argument('--source-root',type=Path,default=ROOT);p.add_argument('--build-directory',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--existing-sys-directory',type=Path);p.add_argument('--existing-link-record',type=Path);p.add_argument('--acknowledge-existing-fixed-input-links',action='store_true');p.add_argument('--um-arm-libraries',type=Path);p.add_argument('--kernel-arm-libraries',type=Path);p.add_argument('--microsoft-linker',type=Path);a=p.parse_args()
 if not a.enable_candidate:raise ValueError('Explicit opt-in required')
 source=a.source_root.resolve();build=a.build_directory.resolve();out=a.output.resolve();specpath=ROOT/'tools/ci/ge8300-integrated197-afterfailure-pins.json';spec=read(specpath)
 if (not a.validate_only and not build.is_relative_to(source)) or not out.is_relative_to(source) or out.is_relative_to(build) or out.exists():raise ValueError('Existing checkout-local inputs and fresh separate output required')
 proof=validate(source,build,spec)
 existing=None;existingpins={}
 if a.existing_sys_directory:
  existingdir=a.existing_sys_directory.resolve();record=(a.existing_link_record or existingdir/'main-link-resume.json').resolve()
  existing,existingpins=validate_existing_sys(existingdir,spec,record,a.acknowledge_existing_fixed_input_links)
  proof.update(existing_SYS_reused=True,SYS_relink_invocations=0,existing_link_evidence=existing)
 if a.validate_only:print(json.dumps(proof));return
 if not a.acknowledge_reconstructed_afterfailure:raise ValueError('Explicit acknowledgement of reconstructed module metadata required')
 if os.name!='nt':raise ValueError('Link execution requires Windows; validate-only is portable')
 if not a.um_arm_libraries or (not existing and not all([a.kernel_arm_libraries,a.microsoft_linker])):raise ValueError('Explicit UM libraries required; new SYS links also require KM libraries and LINK14.29')
 um=a.um_arm_libraries.resolve();km=a.kernel_arm_libraries.resolve() if a.kernel_arm_libraries else None;link=a.microsoft_linker.resolve() if a.microsoft_linker else None
 if not existing:pin(link,spec['LINK1429_sha256']);pin(km/'displib.lib',spec['displib_sha256'])
 umnames=['kernel32.lib','setupapi.lib','dxgi.lib','d3d11.lib','d3d9.lib','user32.lib'];kmnames=['displib.lib','ntoskrnl.lib','hal.lib','libcntpr.lib','bufferoverflowfastfailk.lib']
 libpins={str(d/n):sha(d/n) for d,names in ([(um,umnames)] if existing else [(um,umnames),(km,kmnames)]) for n in names}
 tools={name:Path(shutil.which(name) or '') for name in (['lld-link','llvm-readobj'] if existing else ['lld-link','llvm-lib','llvm-readobj'])}
 if any(not path.is_file() for path in tools.values()):raise ValueError('Existing LLVM link/archive/inspection tools required')
 toolpins={str(path.resolve()):sha(path) for path in tools.values()}
 if not existing:toolpins[str(link)]=sha(link)
 out.mkdir(parents=True);(out/'modules').mkdir();(out/'logs').mkdir();env=dict(os.environ,TEMP=str(out/'logs'),TMP=str(out/'logs'),VSLANG='1033');commands=[]
 def run(args,name):
  result=subprocess.run(list(map(str,args)),capture_output=True,env=env);(out/'logs'/name).write_bytes(result.stdout+result.stderr);commands.append({'arguments':list(map(str,args)),'exit_code':result.returncode,'log_sha256':sha(out/'logs'/name)})
  if result.returncode:raise RuntimeError('Link/inspection failed: '+name)
  return result.stdout
 mod=build/'modules';dest=out/'modules'
 # Only the two failed/unreached client link steps are repeated; no compilation.
 for name,obj,libs in [('ge8300-d3d-clear.exe','testclient_d3d-clear.obj',umnames[:4]),('ge8300-d3d9-flat.exe','testclient_d3d9-flat.obj',['d3d9.lib','kernel32.lib','user32.lib'])]:
  run([tools['lld-link'],'/machine:arm','/subsystem:console','/entry:mainCRTStartup','/nodefaultlib',mod/obj,mod/'testclient_memory.obj',*[um/n for n in libs],'/out:'+str(dest/name)],name+'.link.log')
  pe=run([tools['llvm-readobj'],'--file-headers','--coff-imports',dest/name],name+'.pe.log')
  if b'IMAGE_FILE_MACHINE_ARMNT' not in pe:raise ValueError('Client is not ARMNT')
 for name in ['ge8300-kmd-clear.lib','ge8300-umd-clear.lib','umd_entry.obj']:shutil.copyfile(mod/name,dest/name)
 recipe='drivers/gpu/mt8167/wddm-render/build-wdk81.ps1';shutil.copyfile(source/recipe,out/'fixed3d-build-wdk81.ps1')
 metadata={**proof,'schema':'integrated197-module-afterfailure-v1','source_commit':spec['compiled_source_commit'],'source_dirty':False,'unified_display':True,'source_flags_evidence':'fixed 3d recipe plus saved objects/build.log; execution supplied by main; not compiler-emitted include/command trace','historical_compile_arguments_recorded':False,'recipe_derived_flags_only':True,'compile_recipe_sha256_lf_normalized':spec['source_files'][recipe],'compile_recipe_copy_sha256':sha(out/'fixed3d-build-wdk81.ps1'),'saved_build_log_sha256':spec['files']['modules/build.log'],'expected_microsoft_seh_compiler_sha256_from_fixed_recipe':spec['CL1429_sha256'],'msvc_tool_at_compile_independently_rechecked':False,'microsoft_compile_units':['mdl_pin']+['display_'+n for n in DISPLAY],'compile_units':UNITS,'sources':[{'path':n,'sha256':h} for n,h in spec['source_files'].items() if n.startswith('drivers/') and not n.endswith('.ps1')],'saved_objects':{n:h for n,h in spec['files'].items() if n.startswith('modules/')},'outputs':[{'name':f.name,'sha256':sha(f)} for f in dest.iterdir() if f.is_file()],'runtime_verified':False,'signed':False}
 write(dest/'build-manifest.json',metadata)
 native=build/'native/ge8300-native-owner-core.lib'
 if existing:
  for name in ['ge8300-render.sys','mt8167-display-transport.sys','kernel-link.json','transport-link.json','ge8300-render.map','transport.map']:shutil.copyfile(existingdir/name,out/name)
  shutil.copyfile(record,out/'main-link-resume.json')
  existinglog=existingdir/'mainbuildloglink-existing-host.log'
  if existinglog.is_file():
   existingpins[str(existinglog)]=sha(existinglog);shutil.copyfile(existinglog,out/existinglog.name)
 else:
  run([tools['llvm-lib'],'/machine:arm','/out:'+str(dest/'ge8300-display-transport.lib'),mod/'transport356.obj',mod/'scanout_vidmm356.obj'],'transport.archive.log')
  for lib,name,mapname,meta in [(dest/'ge8300-kmd-clear.lib','ge8300-render.sys','ge8300-render.map','kernel-link.json'),(dest/'ge8300-display-transport.lib','mt8167-display-transport.sys','transport.map','transport-link.json')]:
   run([link,'/largeaddressaware','/tsaware:no','/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/base:0x400000',lib,native,*[km/n for n in kmnames],'/out:'+str(out/name),'/map:'+str(out/mapname)],name+'.link.log')
   pe=run([tools['llvm-readobj'],'--file-headers',out/name],name+'.pe.log')
   if b'IMAGE_FILE_MACHINE_ARMNT' not in pe:raise ValueError('SYS is not ARMNT')
   write(out/meta,{'name':'Microsoft LINK14.29','sha256':sha(link),'output_sha256':sha(out/name),'displib_sha256':sha(km/'displib.lib'),'arguments':commands[-2]['arguments'],'module_metadata_reconstructed_afterfailure':True})
 shutil.copyfile(build/'umd-input/ge8300-umd.dll',out/'ge8300-umd.dll');shutil.copyfile(build/'umd-input/receipt.json',out/'historical-umd-input.json')
 # Recheck immutable inputs/tools/libraries before declaring the bundle complete.
 validate(source,build,spec)
 for path,h in {**libpins,**toolpins,**existingpins}.items():pin(path,h)
 write(out/'integrated-build-manifest.json',{**proof,'schema':'integrated197-link-only-afterfailure-v1','source_commit':spec['compiled_source_commit'],'recipe_source_commit':subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),'input_pins_sha256':sha(specpath),'input_directory':str(build),'native_manifest_path':str(build/'native/build-manifest.json'),'original_compilation_recipe':str(out/'fixed3d-build-wdk81.ps1'),'UMDSourceCommit':'aa6613149e09b7b7be718c90103a34323c362917','CompilerSourceCommit':'f4b92404c4bbe139524a997c5427a8245560e522','SYS_relink_invocations':0 if existing else 2,'existing_SYS_reused':bool(existing),'existing_SYS_evidence':existing,'native_recompiled':0,'KMD_recompiled':0,'UMD_recompiled':0,'PCO_recompiled':0,'SDK_rebuilt':0,'module_metadata_sha256':sha(dest/'build-manifest.json'),'native_manifest_sha256':sha(build/'native/build-manifest.json'),'native_archive_sha256':sha(native),'kernel_link_manifest_sha256':sha(out/'kernel-link.json'),'transport_link_manifest_sha256':sha(out/'transport-link.json'),'historical_UMD_receipt_sha256':sha(out/'historical-umd-input.json'),'library_live_hashes':libpins,'tool_live_hashes':toolpins,'commands_executed':commands,'outputs':{f.name:sha(f) for f in out.iterdir() if f.suffix=='.sys' or f.name=='ge8300-umd.dll'},'signed':False,'hardware_verified':False,'DWM_verified':False,'RT_SRV_implemented':False,'blend_implemented':False})
 print('INTEGRATED197_LINK_ONLY_AFTERFAILURE_COMPLETE')
if __name__=='__main__':main()
