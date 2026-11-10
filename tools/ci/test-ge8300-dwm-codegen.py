#!/usr/bin/env python3
"""Compile the changed adapter against an explicit existing host PCO closure."""
import argparse, hashlib, json, shlex, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
import os

p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');p.add_argument('--host-source',type=Path,required=True);p.add_argument('--host-build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--reference-link-manifest',type=Path,required=True);p.add_argument('--reference-link-manifest-sha256',required=True);a=p.parse_args()
build=a.host_build.resolve();out=a.output.resolve()
if not out.is_relative_to(root) or out.exists():raise ValueError('Fresh checkout-local output required')
out.mkdir(parents=True)
hostsrc=a.host_source.resolve(strict=True)
api=['src/imagination/pco/pco.h','src/imagination/pco/pco_data.h','src/imagination/pco/pco_internal.h','src/imagination/pco/pco_nir.c','src/imagination/pco/pco_trans_nir.c','src/compiler/nir/nir.h']
for name in api:
 if (hostsrc/name).read_bytes()!=(root/'third_party/mesa-pco/mesa'/name).read_bytes():raise ValueError('Host API/source mismatch: '+name)
commands=json.loads((build/'compile_commands.json').read_text());row=next(r for r in commands if r['file'].endswith('/pco.c'));argv=shlex.split(row['command']);args=[];i=2
while i<len(argv):
    v=argv[i]
    if v in ('-o','-c','-MQ','-MF'):i+=2;continue
    if v=='-MD':i+=1;continue
    if v.startswith('-I'):args.append('-I'+str((build/v[2:]).resolve()))
    elif v!='-Werror=missing-prototypes':args.append(v)
    i+=1
units=['drivers/gpu/mt8167/wddm-render/compiler/'+n+'.c' for n in ['sm2','sm4','pco_sm2','witness_producer','witness_codec','witness_info','pco_texture2d','texture2d_sm4','pco_pair']]+['tests/ge8300_premultiplied_codegen.c'];objects=[]
native=['native_resources','native_draw_job','native_triangle_suite','native_triangle_pds','native_triangle_vdm','native_triangle_ppp','native_triangle_texture','native_clear_job','native_clear_surface','native_clear_data','native_clear_codespace','native_clear_pds','native_record_fields','native_hwrt_profile','native_varying_pds','native_d3d_viewport','native_texture2d_state','native_texture2d_program','native_eot_program','clear_tile','clear_image','native_terminate_stream']
units += ['drivers/gpu/mt8167/vendor_m1_11/render_owner/'+n+'.c' for n in native]
units += ['drivers/gpu/mt8167/wddm-render/'+n+'.c' for n in ['clear_protocol','frontend/draw_protocol','frontend/draw_link_protocol','frontend/texture_draw_protocol','frontend/shader_draw_protocol']]
os.environ['TMPDIR']=str(out)
san=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if a.sanitize else []
for u in units:
    o=out/(Path(u).stem+'.o');subprocess.run(['cc',*args,*san,('-UNDEBUG' if u.startswith('tests/') else '-DNDEBUG'),'-c',str(root/u),'-o',str(o)],check=True);objects.append(str(o))
ref=a.reference_link_manifest.resolve(strict=True)
if hashlib.sha256(ref.read_bytes()).hexdigest()!=a.reference_link_manifest_sha256:raise ValueError('Reference link manifest changed')
reference=json.loads(ref.read_text())
for v,h in reference['reference_linked_objects'].items():
 if hashlib.sha256(Path(v).read_bytes()).hexdigest()!=h:raise ValueError('Existing object changed: '+v)
link=[]
for x in reference['link']:
 if x.endswith('/load.o'):link.extend(objects)
 else:link.append(x)
link[link.index('-o')+1]=str(out/'test')
link[1:1]=san
subprocess.run(link,cwd=build,check=True)

r=subprocess.run([str(out/'test'),str(out)],capture_output=True,text=True);(out/'stdout.txt').write_text(r.stdout);(out/'stderr.txt').write_text(r.stderr)
receipt={'schema':'ge8300-dwm-production-codegen-v1','exit_code':r.returncode,'stdout':r.stdout,'PCO_rebuilt':False,'GPU_access':False,'sanitizers':a.sanitize,'compiled_inputs':[{'path':u,'sha256':hashlib.sha256((root/u).read_bytes()).hexdigest()} for u in units],'reference_manifest_sha256':hashlib.sha256(ref.read_bytes()).hexdigest(),'linked_reference_objects':reference['reference_linked_objects'],'Windows375_codegen_verified':False}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(r.stdout);print(r.stderr);raise SystemExit(r.returncode)
