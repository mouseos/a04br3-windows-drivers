from pathlib import Path
import argparse,subprocess,json,hashlib,shlex
p=argparse.ArgumentParser();p.add_argument('--output-dir',type=Path,required=True);p.add_argument('--host-build',type=Path,required=True);a=p.parse_args();S=Path(__file__).resolve().parents[1];O=a.output_dir.resolve();O.mkdir(parents=True,exist_ok=True);native=S/'drivers/gpu/mt8167/vendor_m1_11/render_owner'
names=['native_draw_job','native_texture2d_program','native_triangle_suite','native_triangle_pds','native_triangle_vdm','native_triangle_ppp','native_triangle_texture','native_clear_job','native_clear_surface','native_clear_data','native_clear_codespace','native_clear_pds','native_record_fields','native_hwrt_profile','native_varying_pds','native_d3d_viewport','native_texture2d_state','native_eot_program','clear_tile','clear_image','native_terminate_stream']
units=[native/(n+'.c')for n in names]+[S/'drivers/gpu/mt8167/wddm-render'/n for n in ['clear_protocol.c','frontend/draw_protocol.c','frontend/draw_link_protocol.c','frontend/texture_draw_protocol.c','frontend/shader_draw_protocol.c','compiler/texture2d_sm4.c','compiler/witness_codec.c','compiler/witness_info.c']]
build=a.host_build.resolve();db=json.loads((build/'compile_commands.json').read_text());row=next(x for x in db if x['file'].endswith('/pco.c'));cached=shlex.split(row['command']);inc=[('-I'+str((build/x[2:]).resolve()))if x.startswith('-I')else x for x in cached if x.startswith(('-I','-D'))]
flags=[*inc,'-ffunction-sections','-fdata-sections','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(native)];logs=[];objects=[]
for u in units:
 obj=O/(u.stem+'.o');subprocess.run(['clang',*flags,'-c',str(u),'-o',str(obj)],check=True);objects.append(obj)
for mode in ['baseline353','candidate354']:
 objs=list(objects)
 if mode=='baseline353':
  for name in ['native_draw_job','native_texture2d_program']:
   path=(native/(name+'.c')).relative_to(S).as_posix();old=O/(name+'-353.c');old.write_bytes(subprocess.check_output(['git','-C',str(S),'show','03048eef:'+path]));obj=old.with_suffix('.o');subprocess.run(['clang',*flags,'-c',str(old),'-o',str(obj)],check=True);objs[objects.index(O/(name+'.o'))]=obj
 exe=O/mode;subprocess.run(['clang','-Wl,--gc-sections',*flags,*(['-DEXPECT_OLD']if mode=='baseline353'else[]),str(S/'tests/ge8300_texture354_contract.c'),*[str(x)for x in objs],'-o',str(exe)],check=True)
 r=subprocess.run([str(exe),str(S/'tests/fixtures/texture354-reconstructed-request-v5.bin'),str(S/'tests/fixtures/texture354-fixed-blob.bin')],text=True,capture_output=True,check=True);print(r.stdout,end='');logs.append({'mode':mode,'exit':r.returncode,'stdout':r.stdout})
(O/'regression.json').write_text(json.dumps({'ASan_UBSan':True,'all_actual_R56_R69_fields_matched':True,'request_reconstructed_from_fixed_client_source_and_diagnostic_metadata_not_raw_capture':True,'compiler_libraries_rebuilt':False,'hardware_verified':False,'runs':logs},indent=2)+'\n')
