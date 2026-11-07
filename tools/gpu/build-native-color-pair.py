#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""固定PCO sourceと実D3DCompile出力から有限COLOR pairを生成。GPU操作なし。"""
import argparse,hashlib,json,os,re,struct,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def tokens(path):
 b=path.read_bytes();assert b[:4]==b'DXBC' and struct.unpack_from('<I',b,24)[0]==len(b)
 n=struct.unpack_from('<I',b,28)[0];assert 0<n<=128 and 32+n*4<=len(b)
 result=None
 for i in range(n):
  off=struct.unpack_from('<I',b,32+4*i)[0];assert off+8<=len(b)
  size=struct.unpack_from('<I',b,off+4)[0];assert size<=len(b)-off-8
  if b[off:off+4] in (b'SHDR',b'SHEX'):
   assert result is None;result=b[off+8:off+8+size]
 assert result is not None and len(result)%4==0 and len(result)>=8
 assert struct.unpack_from('<I',result,4)[0]*4==len(result)
 return result
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for k in ['build','reference-manifest','environment-libs','official-inputs','output','header']:
  ap.add_argument('--'+k,type=Path,required=True)
 a=ap.parse_args();o=a.output.resolve();o.mkdir(parents=True,exist_ok=False);b=a.build.resolve()
 ref=json.loads(a.reference_manifest.read_text())
 for p,h in ref['reference_linked_objects'].items():assert sha(p)==h,'PCO link input changed: '+p
 source=Path(__file__).resolve().parent/'shaders/native-color-pair.c'
 c=ref['compile'][:];c[c.index('-c')+1]=str(source);c[c.index('-o')+1]=str(o/'pair.o')
 link=ref['link'][:];link[link.index('-o')+1]=str(o/'pair-compiler');link[link.index('-Wl,--start-group')-1]=str(o/'pair.o')
 env=dict(os.environ,TMPDIR=str(o),LD_LIBRARY_PATH=str(a.environment_libs.resolve()),MESA_SHADER_CACHE_DISABLE='true')
 with (o/'build.log').open('w') as log:
  subprocess.run(c,cwd=b,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
  subprocess.run(link,cwd=b,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 runs=[subprocess.check_output([str(o/'pair-compiler')],cwd=o,env=env) for _ in range(2)]
 assert runs[0]==runs[1];(o/'compiler-output.log').write_bytes(runs[0])
 header='/* SPDX-License-Identifier: MIT\n * Source-generated linked POSITION4/COLOR4. No GPU addresses.\n * tools/gpu/shaders/native-color-pair.c SHA256 '+sha(source)+' */\n'
 meta={}
 for m in re.finditer(rb'PROGRAM (vs|fs) bytes=(\d+) temps=(\d+) shared=(\d+) inputs=(\d+) outputs=(\d+) coeffs=(\d+) entry=(\d+) z=(\d+) w=(\d+)\n([0-9a-f]+)',runs[0]):
  stage=m[1].decode();code=bytes.fromhex(m[11].decode());assert len(code)==int(m[2]);(o/(stage+'.usc')).write_bytes(code)
  meta[stage]=dict(zip(['bytes','temps','shareds','vtxins','vtxouts','coeffs','entry','uses_z','uses_w'],map(int,m.groups()[1:10])))
  meta[stage]['sha256']=sha(o/(stage+'.usc'))
  header+='static const uint8_t a04_native_color_'+stage+'['+str(len(code))+']={\n'
  for i in range(0,len(code),8):header+=' '+','.join('0x%02x'%v for v in code[i:i+8])+',\n'
  header+='};\n'
 assert meta['vs']['bytes']==88 and meta['vs']['vtxins']==meta['vs']['vtxouts']==8
 assert meta['fs']['bytes']==80 and meta['fs']['coeffs']==20 and meta['fs']['uses_w']==1
 for stage,name in [('vs','color-vs'),('fs','color-ps')]:
  path=a.official_inputs/(name+'.dxbc');t=tokens(path);(o/(stage+'.tokens.bin')).write_bytes(t)
  meta[stage]['official_dxbc_sha256']=sha(path);meta[stage]['tokens_sha256']=sha(o/(stage+'.tokens.bin'))
  header+='static const uint32_t a04_native_color_'+stage+'_tokens[]={\n '+','.join(hex(v)+'u' for v in struct.unpack('<'+'I'*(len(t)//4),t))+'\n};\n'
 a.header.write_text(header)
 meta.update(two_processes_equal=True,source_sha256=sha(source),header_sha256=sha(a.header),reference_manifest_sha256=sha(a.reference_manifest),reference_linked_objects=ref['reference_linked_objects'],compile=c,link=link,gpu_executed=False)
 (o/'manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
if __name__=='__main__':main()
