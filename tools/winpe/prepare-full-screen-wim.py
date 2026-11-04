#!/usr/bin/env python3
"""Create a full-resolution variant of the verified signed WinPE WIM."""
import argparse, hashlib, json, shutil, struct, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--sha256',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--wimlib',default='wimlib-imagex');a=p.parse_args()
def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def header(p):
 with p.open('rb') as f:b=f.read(128)
 if len(b)!=128 or b[:8]!=b'MSWIM\0\0\0':raise SystemExit('invalid WIM')
 return struct.unpack_from('<I',b,44)[0],struct.unpack_from('<I',b,120)[0]
if a.output.exists() or a.output.with_suffix(a.output.suffix+'.json').exists():raise SystemExit('output exists')
if digest(a.source)!=a.sha256.lower() or header(a.source)!=(1,1):raise SystemExit('source hash or boot index invalid')
with tempfile.TemporaryDirectory(prefix='a04-full-screen-') as t:
 t=Path(t);cmd=t/'startnet.cmd';subprocess.run([a.wimlib,'extract',str(a.source),'1','/Windows/System32/startnet.cmd',f'--dest-dir={t}'],check=True)
 content=cmd.read_bytes();old=b'X:\\a04br3\\uart-screen.exe\r\n';new=b'X:\\a04br3\\uart-screen.exe -f\r\n'
 if content.count(old)!=1:raise SystemExit('unexpected startnet')
 cmd.write_bytes(content.replace(old,new));a.output.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(a.source,a.output)
 subprocess.run([a.wimlib,'update',str(a.output),'1'],input=f'add {cmd} /Windows/System32/startnet.cmd\n',text=True,check=True)
if header(a.output)!=(1,1):raise SystemExit('boot index lost')
subprocess.run([a.wimlib,'verify',str(a.output)],check=True)
m={'source_sha256':a.sha256.lower(),'sha256':digest(a.output),'boot_index':1,'change':'startnet runs uart-screen.exe -f'};a.output.with_suffix(a.output.suffix+'.json').write_text(json.dumps(m,indent=2)+'\n');print(json.dumps(m,indent=2))
