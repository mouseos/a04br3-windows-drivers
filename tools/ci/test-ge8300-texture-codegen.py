from pathlib import Path
import subprocess,json,shlex,hashlib,argparse
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description='Use an explicit existing Linux Mesa host compiler; never rebuild libraries')
parser.add_argument('--host-build',type=Path,required=True);parser.add_argument('--host-source',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
a=parser.parse_args();out=a.output.resolve();repo=root;build=a.host_build.resolve();hostsrc=a.host_source.resolve();snap=repo/'third_party/mesa-pco/mesa'
if not out.is_relative_to(repo) or out.exists():raise ValueError('Fresh checkout-local output required')
out.mkdir(parents=True)
key=['src/imagination/pco/pco.h','src/imagination/pco/pco_data.h','src/imagination/pco/pco_internal.h','src/imagination/pco/pco_nir.c','src/imagination/pco/pco_nir_tex.c','src/imagination/pco/pco_trans_nir.c','src/compiler/nir/nir.h','src/imagination/common/device_info/ge8300.h']
for p in key:assert (hostsrc/p).read_bytes()==(snap/p).read_bytes(),p
commands=json.load(open(build/'compile_commands.json'));record=next(e for e in commands if e['file'].endswith('/pco.c'));original=shlex.split(record['command']);args=[];i=2
while i<len(original):
 a=original[i]
 if a in ['-o','-c','-MQ','-MF']:i+=2;continue
 if a=='-MD':i+=1;continue
 if a.startswith('-I'):
  args.append('-I'+str((build/a[2:]).resolve()))
 elif a not in ['-DNDEBUG','-Werror=missing-prototypes']:args.append(a)
 i+=1
units=['drivers/gpu/mt8167/wddm-render/compiler/'+n+'.c' for n in ['sm2','sm4','pco_sm2','texture2d_sm4','pco_texture2d']]+['drivers/gpu/mt8167/vendor_m1_11/render_owner/native_texture2d_program.c','tests/ge8300_texture2d_codegen_host.c'];objs=[]
for u in units:
 obj=out/(Path(u).stem+'.o');subprocess.run(['cc',*args,'-c',str(repo/u),'-o',str(obj)],check=True);objs.append(str(obj))
libs=['src/imagination/pco/libpowervr_compiler.a','src/imagination/common/libpowervr_common.a','src/compiler/nir/libnir.a','src/compiler/libcompiler.a','src/util/libmesa_util.a','src/util/libmesa_util_clflush.a','src/util/libmesa_util_clflushopt.a','src/util/libmesa_util_simd.a','src/c11/impl/libmesa_util_c11.a','src/util/blake3/libblake3.a'];paths=[str(build/p) for p in libs]
subprocess.run(['g++','-Wl,--gc-sections','-o',str(out/'texture-codegen'),*objs,'-Wl,--start-group',*paths,'-Wl,--end-group','-pthread','-lz','-lzstd','-lm','-ldl'],check=True)
result=subprocess.run([str(out/'texture-codegen')],text=True,capture_output=True);(out/'stdout.txt').write_text(result.stdout);(out/'stderr.txt').write_text(result.stderr)
receipt={'command_source_compile_database':str(build/'compile_commands.json'),'source_headers_equal':key,'existing_libraries':[{'path':p,'sha256':hashlib.sha256(Path(p).read_bytes()).hexdigest()} for p in paths],'compiled_adapter_units':units,'source_inputs':[{'path':u,'sha256':hashlib.sha256((repo/u).read_bytes()).hexdigest()} for u in units],'PCO_libraries_rebuilt':False,'exit_code':result.returncode,'GPU_access':False};(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(result.stdout);print(result.stderr);raise SystemExit(result.returncode)
