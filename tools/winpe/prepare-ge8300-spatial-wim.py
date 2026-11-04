#!/usr/bin/env python3
"""Make a pinned 232d3d4 diagnostic WIM; never open a block device."""
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
expected={'ge8300-mips.fw':'cfe6f58db2ad604b1729349629510c5a3434b266c99ea886d2959887bff4fed8','ge8300-pnp-status.exe':'af542141ceae722728e54d9bc59f188fddf76ddc567142ade99eaf25b17e3b8d','ge8300-status.exe':'594ef1c87f93cd26a8149a873a7350e2bb24aed0062ded342ffbd9cbf76a79a3','ge8300-test.cer':'ea34adde1ec8ce31d890c0a1de6444b7634bffafd1fe8f925421520bb7dbf641','mt8167-ge8300.cat':'f40746a9eced2c7e29d53495a8635338081c230148db6e72d5e80003fce8b421','mt8167-ge8300.inf':'73afb7f2e63469d577317b68ed505c01340a81b36373eda416883ce5ae4edc0b','mt8167-ge8300.sys':'6e9c7051199daa5f81ad79f1ad6fb2efed25550b1ce07f191a42b734e9d08283'}
source_hash='f3808bb5fe590af7e21549312adf2f903dd5e5449b1d357967c270f3ae1a7648'
if a.output.exists() or a.output.with_suffix(a.output.suffix+'.json').exists():raise SystemExit('output exists')
if not a.source.is_file() or sha(a.source)!=source_hash or header(a.source)!=(1,1):raise SystemExit('source WIM hash or boot index mismatch')
for name,h in expected.items():
 if not (a.package/name).is_file() or sha(a.package/name)!=h:raise SystemExit('package mismatch: '+name)
source_cmd=Path(__file__).with_name('startnet-ge8300-spatial.cmd').resolve()
with tempfile.NamedTemporaryFile(prefix='a04-render-startnet-',suffix='.cmd',delete=False) as f:
 cmd=Path(f.name); f.write(source_cmd.read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'))
atexit.register(cmd.unlink,missing_ok=True)
paths=[a.source.resolve(),cmd,*[(a.package/name).resolve() for name in expected]]
if any(any(c.isspace() or c in "\"'" for c in str(path)) for path in paths):raise SystemExit('paths with whitespace or quotes unsupported by wimlib update')
a.output.parent.mkdir(parents=True,exist_ok=True)
with a.source.open('rb') as f,a.output.open('xb') as g:shutil.copyfileobj(f,g,1048576)
ops=[f'add {cmd} /Windows/System32/startnet.cmd']+[f'add {(a.package/name).resolve()} /a04br3/render-232d3d4/{name}' for name in expected]
subprocess.run([a.wimlib,'update',str(a.output),'1'],input='\n'.join(ops)+'\n',text=True,check=True)
if header(a.output)!=(1,1):raise SystemExit('WIM boot index lost')
subprocess.run([a.wimlib,'verify',str(a.output)],check=True)
with tempfile.TemporaryDirectory(prefix='a04-render-check-') as d:
 subprocess.run([a.wimlib,'extract',str(a.output),'1','/a04br3/render-232d3d4','/Windows/System32/startnet.cmd',f'--dest-dir={d}'],check=True)
 if sha(Path(d)/'startnet.cmd')!=sha(cmd):raise SystemExit('startnet extraction mismatch')
 for name,h in expected.items():
  if sha(Path(d)/'render-232d3d4'/name)!=h:raise SystemExit('extraction mismatch: '+name)
m={'version':1,'source_wim_sha256':source_hash,'wim_sha256':sha(a.output),'boot_index':1,'driver_commit':'232d3d4','package_sha256':expected,'startnet_sha256':sha(cmd),'limitations':'experimental; device boot, DMA safety and signature acceptance not verified'}
a.output.with_suffix(a.output.suffix+'.json').write_text(json.dumps(m,indent=2)+'\n')
print(json.dumps(m,indent=2))
