#!/usr/bin/env python3
"""Make a pinned 09c630d diagnostic WIM; never open a block device."""
import argparse, atexit, hashlib, json, shutil, struct, subprocess, tempfile
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
expected={'ge8300-mips.fw':'cfe6f58db2ad604b1729349629510c5a3434b266c99ea886d2959887bff4fed8','ge8300-pnp-status.exe':'72534d65c376e1b416cf0d0c05c05cbc843eed02d2c39b32c4442d4e8addfd3c','ge8300-status.exe':'51f95e13d5ab908372e0af5af734afe1467b1e41829a75cd523bae79278ce58a','ge8300-test.cer':'cac9bba64eb3a147755f5d1bd7a56fbb8303d4c1de1c473c6c9adb809a864cb0','mt8167-ge8300.cat':'d2c15df74185fd9547627d289939be18848f5e558fd736112d2d916684f13dc8','mt8167-ge8300.inf':'73afb7f2e63469d577317b68ed505c01340a81b36373eda416883ce5ae4edc0b','mt8167-ge8300.sys':'909e28ddfeb3a77f97b98551245237f0290faba21c85c84afccb1f9860987348'}
source_hash='f3808bb5fe590af7e21549312adf2f903dd5e5449b1d357967c270f3ae1a7648'
if a.output.exists() or a.output.with_suffix(a.output.suffix+'.json').exists():raise SystemExit('output exists')
if not a.source.is_file() or sha(a.source)!=source_hash or header(a.source)!=(1,1):raise SystemExit('source WIM hash or boot index mismatch')
for name,h in expected.items():
 if not (a.package/name).is_file() or sha(a.package/name)!=h:raise SystemExit('package mismatch: '+name)
source_cmd=Path(__file__).with_name('startnet-ge8300-asymmetric.cmd').resolve()
with tempfile.NamedTemporaryFile(prefix='a04-render-startnet-',suffix='.cmd',delete=False) as f:
 cmd=Path(f.name); f.write(source_cmd.read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'))
atexit.register(cmd.unlink,missing_ok=True)
paths=[a.source.resolve(),cmd,*[(a.package/name).resolve() for name in expected]]
if any(any(c.isspace() or c in "\"'" for c in str(path)) for path in paths):raise SystemExit('paths with whitespace or quotes unsupported by wimlib update')
a.output.parent.mkdir(parents=True,exist_ok=True)
with a.source.open('rb') as f,a.output.open('xb') as g:shutil.copyfileobj(f,g,1048576)
ops=[f'add {cmd} /Windows/System32/startnet.cmd']+[f'add {(a.package/name).resolve()} /a04br3/render-09c630d/{name}' for name in expected]
subprocess.run([a.wimlib,'update',str(a.output),'1'],input='\n'.join(ops)+'\n',text=True,check=True)
if header(a.output)!=(1,1):raise SystemExit('WIM boot index lost')
subprocess.run([a.wimlib,'verify',str(a.output)],check=True)
with tempfile.TemporaryDirectory(prefix='a04-render-check-') as d:
 subprocess.run([a.wimlib,'extract',str(a.output),'1','/a04br3/render-09c630d','/Windows/System32/startnet.cmd',f'--dest-dir={d}'],check=True)
 if sha(Path(d)/'startnet.cmd')!=sha(cmd):raise SystemExit('startnet extraction mismatch')
 for name,h in expected.items():
  if sha(Path(d)/'render-09c630d'/name)!=h:raise SystemExit('extraction mismatch: '+name)
m={'version':1,'source_wim_sha256':source_hash,'wim_sha256':sha(a.output),'boot_index':1,'driver_commit':'09c630d','package_sha256':expected,'startnet_sha256':sha(cmd),'limitations':'experimental; device boot, DMA safety and signature acceptance not verified'}
a.output.with_suffix(a.output.suffix+'.json').write_text(json.dumps(m,indent=2)+'\n')
print(json.dumps(m,indent=2))
