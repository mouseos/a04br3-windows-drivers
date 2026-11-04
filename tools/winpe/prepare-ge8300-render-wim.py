#!/usr/bin/env python3
"""Make a pinned 751df54 diagnostic WIM; never open a block device."""
import argparse, hashlib, json, shutil, struct, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--source',type=Path,required=True);p.add_argument('--package',type=Path,required=True)
p.add_argument('--output',type=Path,required=True);p.add_argument('--wimlib',default='wimlib-imagex')
a=p.parse_args()
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for chunk in iter(lambda:f.read(1048576),b''):h.update(chunk)
 return h.hexdigest()
def header(p):
 with p.open('rb') as f:b=f.read(128)
 if len(b)!=128 or b[:8]!=b'MSWIM\0\0\0':raise SystemExit('invalid WIM')
 return struct.unpack_from('<I',b,44)[0],struct.unpack_from('<I',b,120)[0]
expected={'ge8300-mips.fw':'cfe6f58db2ad604b1729349629510c5a3434b266c99ea886d2959887bff4fed8','ge8300-pnp-status.exe':'c2b5a375024913ba2da4fbaf7514e84c5aa78bca735fa20675401c782f9b630f','ge8300-status.exe':'b82b1679dafd12fef76ebf5a28ec3925d044e3df206892940c7420858dbc9d7f','ge8300-test.cer':'6c6989cfd169e034fcbf0ff883d84f5a03b08434667dd4ac25cd45001e25903b','mt8167-ge8300.cat':'b9cdaf616feeab3fc8f82fc715f6682aaaf280c7336d8894475057d73d55eb31','mt8167-ge8300.inf':'73afb7f2e63469d577317b68ed505c01340a81b36373eda416883ce5ae4edc0b','mt8167-ge8300.sys':'e36f49aa1580224b6cc249f92272131e6849175608c10eb6dc7c13c66dc42866'}
source_hash='a51a478308832713ce877af51a4fecc69acbce40c680bbe08d41e4cc453386d8'
if a.output.exists() or a.output.with_suffix(a.output.suffix+'.json').exists():raise SystemExit('output exists')
if not a.source.is_file() or sha(a.source)!=source_hash or header(a.source)!=(1,1):raise SystemExit('source WIM hash or boot index mismatch')
for name,h in expected.items():
 if not (a.package/name).is_file() or sha(a.package/name)!=h:raise SystemExit('package mismatch: '+name)
cmd=Path(__file__).with_name('startnet-ge8300-render.cmd').resolve()
paths=[a.source.resolve(),cmd,*[(a.package/name).resolve() for name in expected]]
if any(any(c.isspace() or c in "\"'" for c in str(path)) for path in paths):raise SystemExit('paths with whitespace or quotes unsupported by wimlib update')
a.output.parent.mkdir(parents=True,exist_ok=True)
with a.source.open('rb') as f,a.output.open('xb') as g:shutil.copyfileobj(f,g,1048576)
ops=[f'add {cmd} /Windows/System32/startnet.cmd']+[f'add {(a.package/name).resolve()} /a04br3/render-751df54/{name}' for name in expected]
subprocess.run([a.wimlib,'update',str(a.output),'1'],input='\n'.join(ops)+'\n',text=True,check=True)
if header(a.output)!=(1,1):raise SystemExit('WIM boot index lost')
subprocess.run([a.wimlib,'verify',str(a.output)],check=True)
with tempfile.TemporaryDirectory(prefix='a04-render-check-') as d:
 subprocess.run([a.wimlib,'extract',str(a.output),'1','/a04br3/render-751df54','/Windows/System32/startnet.cmd',f'--dest-dir={d}'],check=True)
 if sha(Path(d)/'startnet.cmd')!=sha(cmd):raise SystemExit('startnet extraction mismatch')
 for name,h in expected.items():
  if sha(Path(d)/'render-751df54'/name)!=h:raise SystemExit('extraction mismatch: '+name)
m={'version':1,'source_wim_sha256':source_hash,'wim_sha256':sha(a.output),'boot_index':1,'driver_commit':'751df54','package_sha256':expected,'startnet_sha256':sha(cmd),'limitations':'experimental; device boot, DMA safety and signature acceptance not verified'}
a.output.with_suffix(a.output.suffix+'.json').write_text(json.dumps(m,indent=2)+'\n')
print(json.dumps(m,indent=2))
