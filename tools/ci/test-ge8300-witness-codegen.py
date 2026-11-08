#!/usr/bin/env python3
"""Compile the changed adapter against an explicit existing host PCO closure."""
import argparse, hashlib, json, shlex, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');p.add_argument('--host-build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
build=a.host_build.resolve();out=a.output.resolve()
if not out.is_relative_to(root) or out.exists():raise ValueError('Fresh checkout-local output required')
out.mkdir(parents=True)
commands=json.loads((build/'compile_commands.json').read_text());row=next(r for r in commands if r['file'].endswith('/pco.c'));argv=shlex.split(row['command']);args=[];i=2
while i<len(argv):
    v=argv[i]
    if v in ('-o','-c','-MQ','-MF'):i+=2;continue
    if v=='-MD':i+=1;continue
    if v.startswith('-I'):args.append('-I'+str((build/v[2:]).resolve()))
    elif v!='-Werror=missing-prototypes':args.append(v)
    i+=1
units=['drivers/gpu/mt8167/wddm-render/compiler/'+n+'.c' for n in ('sm2','sm4','pco_sm2','witness_producer','witness_codec')]+['tests/ge8300_witness_codegen.c'];objects=[]
native=['native_draw_job','native_triangle_suite','native_triangle_pds','native_triangle_vdm','native_triangle_ppp','native_triangle_texture','native_clear_job','native_clear_surface','native_clear_data','native_clear_codespace','native_clear_pds','native_record_fields','native_hwrt_profile','native_varying_pds','native_d3d_viewport','native_texture2d_state','native_texture2d_program','native_eot_program','clear_tile','clear_image','native_terminate_stream']
units += ['drivers/gpu/mt8167/vendor_m1_11/render_owner/'+n+'.c' for n in native]
units += ['drivers/gpu/mt8167/wddm-render/compiler/texture2d_sm4.c']
units += ['drivers/gpu/mt8167/wddm-render/'+n+'.c' for n in ['clear_protocol','frontend/draw_protocol','frontend/draw_link_protocol','frontend/texture_draw_protocol','frontend/shader_draw_protocol']]
san=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if a.sanitize else []
for u in units:
    o=out/(Path(u).stem+'.o');subprocess.run(['cc',*args,*san,('-UNDEBUG' if u.startswith('tests/') else '-DNDEBUG'),'-c',str(root/u),'-o',str(o)],check=True);objects.append(str(o))
libs=['src/imagination/pco/libpowervr_compiler.a','src/imagination/common/libpowervr_common.a','src/compiler/nir/libnir.a','src/compiler/libcompiler.a','src/util/libmesa_util.a','src/util/libmesa_util_clflush.a','src/util/libmesa_util_clflushopt.a','src/util/libmesa_util_simd.a','src/c11/impl/libmesa_util_c11.a','src/util/blake3/libblake3.a'];paths=[str(build/n) for n in libs]
subprocess.run(['g++',*san,'-Wl,--gc-sections','-o',str(out/'test'),*objects,'-Wl,--start-group',*paths,'-Wl,--end-group','-pthread','-lz','-lzstd','-lm','-ldl'],check=True)
r=subprocess.run([str(out/'test')],capture_output=True,text=True);(out/'stdout.txt').write_text(r.stdout);(out/'stderr.txt').write_text(r.stderr)
receipt={'schema':'native-shader329-real-codegen-v1','exit_code':r.returncode,'stdout':r.stdout,'PCO_rebuilt':False,'GPU_access':False,'sanitizers':a.sanitize,'compiled_inputs':[{'path':u,'sha256':hashlib.sha256((root/u).read_bytes()).hexdigest()} for u in units],'libraries':[{'path':v,'sha256':hashlib.sha256(Path(v).read_bytes()).hexdigest()} for v in paths]}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(r.stdout);print(r.stderr);raise SystemExit(r.returncode)
