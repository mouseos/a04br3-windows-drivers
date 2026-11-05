#!/usr/bin/env python3
"""Add a test certificate to a copied WinPE SOFTWARE hive only."""
import argparse,hashlib,json,shutil,struct,subprocess
from pathlib import Path
from regipy.registry import RegistryHive
root=Path(__file__).resolve().parents[2];workspace=root.parents[1]
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def blob_at(h,store,thumb):
 key=h.get_key('\\Microsoft\\SystemCertificates\\'+store+'\\Certificates\\'+thumb)
 vals=list(key.iter_values(max_len=100000));matches=[v for v in vals if v.name=='Blob']
 if len(matches)!=1:raise ValueError('missing certificate blob')
 return bytes.fromhex(matches[0].value)
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--source-sha256',required=True);p.add_argument('--certificate',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 src=a.source.resolve();certpath=a.certificate.resolve();out=a.output.resolve();meta=out.with_suffix(out.suffix+'.json')
 if any(workspace not in x.parents for x in (src,certpath,out)) or out.exists() or meta.exists():raise SystemExit('paths must be new and in workspace')
 expected='69731033f0419d80942d66cd671abd1f400bd6bb0ea646435080a45c12fd62e8'
 if a.source_sha256.lower()!=expected or sha(src)!=expected:raise SystemExit('SOFTWARE is not the pinned WinPE hive')
 b=src.read_bytes()[:512];seq=struct.unpack_from('<II',b,4);v=struct.unpack_from('<II',b,20)
 if b[:4]!=b'regf' or seq[0]!=seq[1] or v!=(1,5):raise SystemExit('WinPE hive not clean version 1.5')
 cert=certpath.read_bytes();thumb=hashlib.sha1(cert).hexdigest().upper();blob=struct.pack('<III',3,1,20)+hashlib.sha1(cert).digest()+struct.pack('<III',32,1,len(cert))+cert
 original=RegistryHive(str(src)); old='5FC3D0043DE4A471F738FB923FEC87336E81D43C'
 old_blobs={s:blob_at(original,s,old) for s in ('Root','TrustedPublisher')}
 for store in ('Root','TrustedPublisher'):
  parent=original.get_key('\\Microsoft\\SystemCertificates\\'+store+'\\Certificates')
  if any(k.name.upper()==thumb for k in parent.iter_subkeys()):raise SystemExit('test certificate already present')
 shutil.copyfile(src,out)
 try:
  commands=''
  for store in ('Root','TrustedPublisher'):
   commands+=f'cd \\Microsoft\\SystemCertificates\\{store}\\Certificates\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:'+blob.hex()+'\n'
  commands+='commit\nquit\n'
  subprocess.run(['hivexsh','-w',str(out)],input=commands,text=True,check=True,capture_output=True)
  changed=RegistryHive(str(out))
  for store in ('Root','TrustedPublisher'):
   if blob_at(changed,store,thumb)!=blob or blob_at(changed,store,old)!=old_blobs[store]:raise ValueError('certificate store validation failed')
  data={'source_sha256':a.source_sha256.lower(),'output_sha256':sha(out),'certificate_sha256':sha(certpath),'thumbprint':thumb,'existing_signer_thumbprint':old,'scope':'test certificate added to copied WinPE SOFTWARE only; target Windows unchanged'}
  meta.write_text(json.dumps(data,indent=2)+'\n');print(json.dumps(data,indent=2))
 except BaseException:
  out.unlink(missing_ok=True);raise
if __name__=='__main__':main()
