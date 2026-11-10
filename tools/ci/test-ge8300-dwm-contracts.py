#!/usr/bin/env python3
from pathlib import Path
import subprocess,os,json
S=Path(__file__).resolve().parents[2];N=S/'drivers/gpu/mt8167/vendor_m1_11/render_owner'
import argparse
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);O=p.parse_args().output.resolve();O.mkdir(parents=True,exist_ok=True);os.environ['TMPDIR']=str(O)
test=S/'tests/ge8300_dwm_geometry_contract.c'
# Source-proven native structures require same Mesa include closure as existing host oracle.
plan=json.loads((S/'third_party/mesa-pco/source-plan.json').read_text());args=next(x['arguments'] for x in plan['units'] if x['source'].endswith('/pco.c'));incs=[]
for i,x in enumerate(args):
 if x=='-I':incs+=['-I',args[i+1].replace('${REPO}',str(S))]
names=['native_hwrt_profile','native_resources','native_d3d_viewport','native_copy_job','native_copy_state','clear_image']
cmd=['clang',*incs,'-I'+str(N),'-std=c11','-DA04_GE8300_UNIFIED_DISPLAY356=1','-O1','-g','-fsanitize=address,undefined','-ffunction-sections','-Wl,--gc-sections',str(test),*[str(N/(x+'.c')) for x in names],'-o',str(O/'geometry-contract')]
p=subprocess.run(cmd,text=True,capture_output=True);(O/'geometry-compile.log').write_text(p.stdout+p.stderr);assert p.returncode==0,(O/'geometry-compile.log')
p=subprocess.run([str(O/'geometry-contract')],text=True,capture_output=True);(O/'geometry-test.log').write_text(p.stdout+p.stderr);print(p.stdout+p.stderr);assert p.returncode==0

# Existing production V5 wire -> immutable draw image, including geometry.
names=['native_draw_job','native_texture2d_program','native_triangle_suite','native_triangle_pds','native_triangle_vdm','native_triangle_ppp','native_triangle_texture','native_clear_job','native_clear_surface','native_clear_data','native_clear_codespace','native_clear_pds','native_record_fields','native_hwrt_profile','native_varying_pds','native_d3d_viewport','native_texture2d_state','native_eot_program','clear_tile','clear_image','native_terminate_stream']
units=[N/(u+'.c') for u in names]+[S/'drivers/gpu/mt8167/wddm-render'/u for u in ['clear_protocol.c','frontend/draw_protocol.c','frontend/draw_link_protocol.c','frontend/texture_draw_protocol.c','frontend/shader_draw_protocol.c','compiler/texture2d_sm4.c','compiler/witness_codec.c','compiler/witness_info.c']]
plan=json.loads((S/'third_party/mesa-pco/source-plan.json').read_text());args=next(x['arguments'] for x in plan['units'] if x['source'].endswith('/pco.c'));incs=[]
for i,x in enumerate(args):
 if x=='-I':incs+=['-I',args[i+1].replace('${REPO}',str(S))]
cmd=['clang',*incs,'-DHAVE_ENDIAN_H','-DHAVE_PTHREAD','-DHAVE_PTHREAD_H','-D_GNU_SOURCE','-DHAVE_STRUCT_TIMESPEC','-DHAVE_FUNC_ATTRIBUTE_PACKED','-DPACKAGE_VERSION="27.0.0-devel"','-O1','-g','-ffunction-sections','-fdata-sections','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wl,--gc-sections','-I'+str(N),str(S/'tests/ge8300_texture354_contract.c'),*map(str,units),'-o',str(O/'draw-contract')]
p=subprocess.run(cmd,text=True,capture_output=True);(O/'compile.log').write_text(p.stdout+p.stderr);assert p.returncode==0,(O/'compile.log')
p=subprocess.run([str(O/'draw-contract'),str(S/'tests/fixtures/texture354-reconstructed-request-v5.bin'),str(S/'tests/fixtures/texture354-fixed-blob.bin')],text=True,capture_output=True);(O/'test.log').write_text(p.stdout+p.stderr);assert p.returncode==0,(O/'test.log')
print(p.stdout)
