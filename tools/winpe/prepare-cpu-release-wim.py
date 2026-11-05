#!/usr/bin/env python3
"""Update a fixed diagnostic WIM using local wimlib; never access a device."""
import argparse,hashlib,json,os,re,secrets,shutil,ssl,struct,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];workspace=root.parents[1]
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def header(p):
 with p.open('rb') as f:b=f.read(128)
 if len(b)!=128 or b[:8]!=b'MSWIM\0\0\0':raise ValueError('invalid WIM')
 return struct.unpack_from('<I',b,44)[0],struct.unpack_from('<I',b,120)[0]
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--source-sha256',required=True);p.add_argument('--wimlib',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--helper',type=Path);p.add_argument('--software',type=Path);p.add_argument('--certificate',type=Path);p.add_argument('--certificate-pem',type=Path);p.add_argument('--osslsigncode',type=Path);a=p.parse_args()
 out=a.output.resolve(); manifest=out.with_suffix(out.suffix+'.json')
 if workspace not in out.parents or out.exists() or manifest.exists():raise SystemExit('invalid output')
 if workspace not in a.source.resolve().parents or workspace not in a.wimlib.resolve().parents:raise SystemExit('source and wimlib must be in workspace')
 if sha(a.source)!=a.source_sha256.lower() or header(a.source)!=(1,1):raise SystemExit('source hash or boot index mismatch')
 if not re.fullmatch('[0-9a-fA-F]{64}',a.source_sha256):raise SystemExit('bad digest')
 signing=(a.helper,a.software,a.certificate,a.certificate_pem,a.osslsigncode)
 if any(signing) and not all(signing):raise SystemExit('helper, SOFTWARE, certificate, PEM and verifier must be supplied together')
 if a.helper:
  if any(workspace not in path.resolve().parents for path in signing):raise SystemExit('signing inputs must be in workspace')
  from regipy.registry import RegistryHive
  cert=a.certificate.read_bytes();thumb=hashlib.sha1(cert).hexdigest().upper();h=RegistryHive(str(a.software))
  pem=a.certificate_pem.read_text();pem=pem[pem.index('-----BEGIN CERTIFICATE-----'):];pem=pem[:pem.index('-----END CERTIFICATE-----')+len('-----END CERTIFICATE-----')]
  if ssl.PEM_cert_to_DER_cert(pem)!=cert:raise SystemExit('PEM and DER certificate mismatch')
  subprocess.run([str(a.osslsigncode.resolve()),'verify','-CAfile',str(a.certificate_pem.resolve()),'-in',str(a.helper.resolve())],check=True,stdout=subprocess.DEVNULL)
  blob=struct.pack('<III',3,1,20)+hashlib.sha1(cert).digest()+struct.pack('<III',32,1,len(cert))+cert
  for store in ('Root','TrustedPublisher'):
   k=h.get_key('\\Microsoft\\SystemCertificates\\'+store+'\\Certificates\\'+thumb);v=next(k.iter_values(max_len=100000));
   if v.name!='Blob' or bytes.fromhex(v.value)!=blob:raise SystemExit('WinPE certificate blob mismatch')
  b=a.helper.read_bytes();off=struct.unpack_from('<I',b,0x3c)[0]
  if b[:2]!=b'MZ' or b[off:off+4]!=b'PE\0\0' or struct.unpack_from('<H',b,off+4)[0]!=0x1c4:raise SystemExit('helper is not ARMNT')
 token=secrets.token_hex(8).upper(); template=(root/'tools/winpe/startnet-cpu-audit.cmd.in').read_text()
 if template.count('@TOKEN@')!=1:raise SystemExit('bad template')
 tempbase=root/'build/tmp';tempbase.mkdir(parents=True,exist_ok=True)
 if workspace not in tempbase.resolve().parents:raise SystemExit('temporary directory outside workspace')
 out.parent.mkdir(parents=True,exist_ok=True)
 try:
  with tempfile.TemporaryDirectory(prefix='cpu-release-',dir=tempbase) as t:
   t=Path(t);start=t/'startnet.cmd';start.write_text(template.replace('@TOKEN@',token),encoding='ascii',newline='\r\n')
   sources={'/a04br3/audit-toprgu-installed.cmd':root/'tools/winpe/audit-toprgu-installed.cmd', '/a04br3/release-toprgu-audit-volume.cmd':root/'tools/winpe/release-toprgu-audit-volume.cmd','/Windows/System32/startnet.cmd':start}
   if a.helper: sources.update({'/a04br3/audit-hive-uncached.exe':a.helper.resolve(),'/a04br3/audit-hive-signer.cer':a.certificate.resolve(),'/Windows/System32/config/SOFTWARE':a.software.resolve()})
   if any('"' in str(x) or '\n' in str(x) for x in sources.values()):raise SystemExit('unsupported source path')
   shutil.copyfile(a.source,out)
   commands=''.join(f'add "{src}" "{dst}"\n' for dst,src in sources.items())
   subprocess.run([str(a.wimlib.resolve()),'update',str(out),'1','--check'],input=commands,text=True,check=True)
   subprocess.run([str(a.wimlib.resolve()),'verify',str(out)],check=True)
   if header(out)!=(1,1):raise RuntimeError('boot index changed')
   check=t/'check';check.mkdir()
   for index,(dst,src) in enumerate(sources.items()):
    dest=check/str(index);dest.mkdir()
    subprocess.run([str(a.wimlib.resolve()),'extract',str(out),'1',dst,f'--dest-dir={dest}'],check=True,stdout=subprocess.DEVNULL)
    files=[p for p in dest.rglob('*') if p.is_file()]
    if len(files)!=1 or sha(files[0])!=sha(src):raise RuntimeError('WIM extraction mismatch: '+dst)
   m={'source_wim_sha256':a.source_sha256.lower(),'output_wim_sha256':sha(out),'wimlib_sha256':sha(a.wimlib.resolve()),'token':token,'input_scripts':{k:sha(v) for k,v in sources.items()},'image_count':1,'boot_index':1,'status':'host-validated only; device recovery and volume release unverified'}
   if a.helper:m['winpe_test_certificate_thumbprint']=thumb
   manifest.write_text(json.dumps(m,indent=2)+'\n');print(json.dumps(m,indent=2))
 except BaseException:
  out.unlink(missing_ok=True);raise
if __name__=='__main__':main()
