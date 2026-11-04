#!/usr/bin/env python3
"""Add the pinned public test certificate to an offline WIM copy only."""
import argparse, hashlib, json, shutil, struct, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);p.add_argument('--source-sha256',required=True);p.add_argument('--serialized-cert',required=True,type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--wimlib',default='wimlib-imagex');a=p.parse_args()
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
def head(p):
 with p.open('rb') as f:b=f.read(128)
 if len(b)!=128 or b[:8]!=b'MSWIM\0\0\0':raise SystemExit('invalid WIM')
 return struct.unpack_from('<I',b,44)[0],struct.unpack_from('<I',b,120)[0]
thumb='67BFF50E2B288305DC74DA5F3850101F7A084934';blob=a.serialized_cert.read_bytes()
if sha(a.serialized_cert)!='c168b4a8042cbbdde064903833b41e314d8d0c765f44d2116a2d7e8589b58a4a':raise SystemExit('serialized certificate mismatch')
if sha(a.source)!=a.source_sha256.lower() or head(a.source)!=(1,1):raise SystemExit('source mismatch')
if a.output.exists() or a.output.with_suffix(a.output.suffix+'.json').exists():raise SystemExit('output exists')
if any(c.isspace() or c in "\"'" for c in str(a.output.resolve())):raise SystemExit('output path not supported')
with tempfile.TemporaryDirectory(prefix='a04-render-cert-') as d:
 d=Path(d);subprocess.run([a.wimlib,'extract',str(a.source),'1','/Windows/System32/config/SOFTWARE',f'--dest-dir={d}'],check=True)
 hive=d/'SOFTWARE';paths=[f'\\Microsoft\\SystemCertificates\\{store}\\Certificates' for store in ('Root','TrustedPublisher')]
 for path in paths:
  listing=subprocess.run(['hivexsh',str(hive)],input=f'cd {path}\nls\n',text=True,capture_output=True,check=True)
  if listing.stderr or thumb in listing.stdout:raise SystemExit('store missing or certificate already present')
  commands=f'cd {path}\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:{blob.hex()}\ncommit\n'
  edit=subprocess.run(['hivexsh','-w',str(hive)],input=commands,text=True,capture_output=True,check=True)
  if edit.stderr:raise SystemExit('offline registry edit failed: '+edit.stderr)
  got=subprocess.run(['hivexget',str(hive),path+'\\'+thumb,'Blob'],capture_output=True,check=True).stdout
  if got!=blob:raise SystemExit('offline certificate verification failed')
 a.output.parent.mkdir(parents=True,exist_ok=True)
 with a.source.open('rb') as src,a.output.open('xb') as dst:shutil.copyfileobj(src,dst,1048576)
 subprocess.run([a.wimlib,'update',str(a.output),'1'],input=f'add {hive} /Windows/System32/config/SOFTWARE\n',text=True,check=True)
 if head(a.output)!=(1,1):raise SystemExit('boot index lost')
 subprocess.run([a.wimlib,'verify',str(a.output)],check=True)
 with tempfile.TemporaryDirectory(prefix='a04-render-cert-check-') as check:
  subprocess.run([a.wimlib,'extract',str(a.output),'1','/Windows/System32/config/SOFTWARE',f'--dest-dir={check}'],check=True)
  for path in paths:
   if subprocess.run(['hivexget',str(Path(check)/'SOFTWARE'),path+'\\'+thumb,'Blob'],capture_output=True,check=True).stdout!=blob:raise SystemExit('WIM certificate mismatch')
m={'source_sha256':a.source_sha256.lower(),'wim_sha256':sha(a.output),'serialized_cert_sha256':sha(a.serialized_cert),'thumbprint_sha1':thumb,'stores':['Root','TrustedPublisher'],'note':'offline trust entries verified; signature acceptance and boot policy unverified'}
a.output.with_suffix(a.output.suffix+'.json').write_text(json.dumps(m,indent=2)+'\n');print(json.dumps(m,indent=2))
