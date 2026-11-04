#!/usr/bin/env python3
"""Export a one-image WinPE WIM while retaining its bootable index."""
import argparse, hashlib, json, os, struct, subprocess
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--source',type=Path,required=True)
p.add_argument('--source-sha256',required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--wimlib',default='wimlib-imagex')
a=p.parse_args()
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def header(path):
 with path.open('rb') as f:b=f.read(128)
 if len(b)!=128 or b[:8]!=b'MSWIM\0\0\0' or struct.unpack_from('<I',b,8)[0]<208:raise SystemExit('invalid WIM header')
 return struct.unpack_from('<I',b,44)[0],struct.unpack_from('<I',b,120)[0]
if a.output.exists() or a.output.with_suffix(a.output.suffix+'.json').exists():raise SystemExit('output exists')
if sha(a.source)!=a.source_sha256.lower():raise SystemExit('source hash mismatch')
if header(a.source)!=(1,1):raise SystemExit('source WIM must contain one image and boot index 1')
subprocess.run([a.wimlib,'export',str(a.source),'1',str(a.output),'--boot','--compress=LZX','--check'],check=True)
if header(a.output)!=(1,1):raise SystemExit('exported boot index invalid')
subprocess.run([a.wimlib,'verify',str(a.output)],check=True)
m={'source_sha256':a.source_sha256.lower(),'output_sha256':sha(a.output),'source_image_count':1,'source_boot_index':1,'output_image_count':1,'output_boot_index':1,'compression':'LZX','verification':'wimlib verify'}
a.output.with_suffix(a.output.suffix+'.json').write_text(json.dumps(m,indent=2)+'\n')
print(json.dumps(m,indent=2))
