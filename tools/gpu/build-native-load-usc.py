#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""既存固定PCO toolchainで所有target背景loadだけを生成。GPU操作なし。"""
import argparse,hashlib,json,os,subprocess,re
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for k in ['build','reference-manifest','environment-libs','output','header']:ap.add_argument('--'+k,type=Path,required=True)
 a=ap.parse_args();b=a.build.resolve();o=a.output.resolve();o.mkdir(exist_ok=False,parents=True)
 ref=json.loads(a.reference_manifest.read_text());old=ref['commands']
 # Verify ALL real archive/object inputs in the existing source build ledger.
 for path,digest in ref['linked_object_hashes'].items():
  assert sha(path)==digest, 'reference link input changed: '+path
 src=Path(__file__).resolve().parent/'shaders/native-load-usc.c'
 c=old[0][:];c[c.index('-c')+1]=str(src);c[c.index('-o')+1]=str(o/'load.o');c+=['-UNDEBUG','-Werror']
 env=os.environ.copy();env.update(TMPDIR=str(o),LD_LIBRARY_PATH=str(a.environment_libs.resolve()),MESA_SHADER_CACHE_DISABLE='true')
 subprocess.run(c,cwd=b,env=env,check=True)
 link=['c++','-o',str(o/'compiler'),str(o/'load.o'),'-Wl,--start-group']
 # Reuse library list only; do not silently link the H frontend/old main.
 link += [x for x in old[-1] if x.endswith(('.a','.so'))]
 link+=['-Wl,--end-group','-pthread','-lm']
 subprocess.run(link,cwd=b,env=env,check=True)
 runs=[subprocess.check_output([str(o/'compiler')],cwd=o,env=env) for i in range(2)]
 assert runs[0]==runs[1];(o/'compiler-output.log').write_bytes(runs[0])
 m=re.search(rb'PROGRAM bytes=(\d+) temps=(\d+) shared=(\d+) z=(\d+) w=(\d+)\n([0-9a-f]+)\n',runs[0]);assert m
 size,temps,shared,z,w=map(int,m.groups()[:5]);code=bytes.fromhex(m[6].decode());assert size==len(code) and shared==8 and z==w==0
 assert b'smp.2d' in runs[0] and b'w0 = po0' in runs[0] and b'.olchk.end' in runs[0] and b'load.global' not in runs[0]
 (o/'load.usc').write_bytes(code)
 text='/* SPDX-License-Identifier: MIT\n * Source-generated target loader, BVNC22.40.54.30.\n * Source SHA256 '+sha(src)+'. No GPU address. */\n'
 text+=f'#define A04_NATIVE_LOAD_TEMPS {temps}u\n#define A04_NATIVE_LOAD_SHAREDS {shared}u\nstatic const uint8_t a04_native_load_usc[{size}]={{\n'
 for at in range(0,size,8):text+=' '+','.join('0x%02x'%v for v in code[at:at+8])+',\n'
 text+='};\n';a.header.write_text(text)
 manifest={'source':str(src),'source_sha256':sha(src),'reference_manifest_sha256':sha(a.reference_manifest),'reference_linked_objects':ref['linked_object_hashes'],'compile':c,'link':link,'compiler_sha256':sha(o/'compiler'),'header_sha256':sha(a.header),'code_sha256':sha(o/'load.usc'),'bytes':size,'temps':temps,'shareds':shared,'uses_z':z,'uses_w':w,'two_processes_equal':True,'gpu_executed':False}
 (o/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
if __name__=='__main__':main()
