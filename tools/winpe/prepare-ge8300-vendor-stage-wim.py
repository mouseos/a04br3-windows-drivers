#!/usr/bin/env python3
"""Build a reproducible-input WinPE for CPU-only GE8300 vendor staging."""
import argparse, hashlib, json, shutil, struct, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser()
for name in ('source','package','manifest','output'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--source-sha256',required=True);p.add_argument('--driver-commit',required=True);p.add_argument('--wimlib',default='wimlib-imagex');a=p.parse_args()
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
def run(cmd,**kwargs):return subprocess.run(cmd,check=True,**kwargs)
def header(p):
 with p.open('rb') as f:b=f.read(128)
 if len(b)!=128 or b[:8]!=b'MSWIM\0\0\0':raise SystemExit('invalid WIM')
 return struct.unpack_from('<I',b,44)[0],struct.unpack_from('<I',b,120)[0]
m=json.loads(a.manifest.read_text());expected=m['files']
names={'mt8167-ge8300.sys','mt8167-ge8300.inf','mt8167-ge8300.cat','ge8300-mips.fw','ge8300-test.cer','ge8300-status.exe','ge8300-pnp-status.exe','ge8300-vendor-stage-status.exe'}
if m.get('driver_commit')!=a.driver_commit or not m.get('vendor_stage') or set(expected)!=names:raise SystemExit('unexpected package manifest')
if expected['ge8300-mips.fw']!='5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1':raise SystemExit('firmware mismatch')
if sha(a.source)!=a.source_sha256.lower() or header(a.source)!=(1,1):raise SystemExit('source WIM mismatch')
if a.output.exists() or a.output.with_suffix(a.output.suffix+'.json').exists():raise SystemExit('output exists')
for n in names:
 if not (a.package/n).is_file() or sha(a.package/n)!=expected[n]:raise SystemExit('package mismatch: '+n)
if any(any(c.isspace() or c in "\\\"'" for c in str(q.resolve())) for q in (a.source,a.package,a.output)):raise SystemExit('unsupported input path')
a.output.parent.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='vendor-wim-',dir=a.output.parent) as d:
 d=Path(d);run([a.wimlib,'extract',str(a.source),'1','/Windows/System32/config/SOFTWARE',f'--dest-dir={d}'],stdout=subprocess.DEVNULL)
 hive=d/'SOFTWARE';cert=(a.package/'ge8300-test.cer').read_bytes();thumb=hashlib.sha1(cert).hexdigest().upper();blob=struct.pack('<III',32,1,len(cert))+cert
 run(['openssl','x509','-inform','DER','-in',str(a.package/'ge8300-test.cer'),'-noout'],stdout=subprocess.DEVNULL)
 for store in ('Root','TrustedPublisher'):
  path=f'\\Microsoft\\SystemCertificates\\{store}\\Certificates'
  q=run(['hivexsh',str(hive)],input=f'cd {path}\nls\n',text=True,capture_output=True)
  if q.stderr or thumb in q.stdout.upper().splitlines():raise SystemExit('certificate store unavailable or entry exists')
  q=run(['hivexsh','-w',str(hive)],input=f'cd {path}\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:{blob.hex()}\ncommit\n',text=True,capture_output=True)
  if q.stderr:raise SystemExit('registry update failed')
  if run(['hivexget',str(hive),path+'\\'+thumb,'Blob'],capture_output=True).stdout!=blob:raise SystemExit('certificate mismatch')
 cmd=d/'startnet.cmd';cmd.write_bytes(Path(__file__).with_name('startnet-ge8300-vendor-stage.cmd').read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'))
 with a.source.open('rb') as f,a.output.open('xb') as g:shutil.copyfileobj(f,g,1048576)
 ops=[f'add {cmd} /Windows/System32/startnet.cmd',f'add {hive} /Windows/System32/config/SOFTWARE']+[f'add {(a.package/n).resolve()} /a04br3/vendor-m1-11/{n}' for n in sorted(names)]
 run([a.wimlib,'update',str(a.output),'1'],input='\n'.join(ops)+'\n',text=True)
 if header(a.output)!=(1,1):raise SystemExit('boot index changed')
 run([a.wimlib,'verify',str(a.output)])
 check=d/'check';run([a.wimlib,'extract',str(a.output),'1','/a04br3/vendor-m1-11','/Windows/System32/startnet.cmd','/Windows/System32/config/SOFTWARE',f'--dest-dir={check}'],stdout=subprocess.DEVNULL)
 if sha(check/'startnet.cmd')!=sha(cmd) or sha(check/'SOFTWARE')!=sha(hive):raise SystemExit('WIM metadata mismatch')
 for n in names:
  if sha(check/'vendor-m1-11'/n)!=expected[n]:raise SystemExit('WIM file mismatch: '+n)
 result={'source_sha256':a.source_sha256.lower(),'wim_sha256':sha(a.output),'driver_commit':m['driver_commit'],'package_sha256':expected,'certificate_thumbprint':thumb,'device_validated':False,'gpu_started':False}
 a.output.with_suffix(a.output.suffix+'.json').write_text(json.dumps(result,indent=2)+'\n')
print('Prepared CPU-only GE8300 vendor staging WIM')
