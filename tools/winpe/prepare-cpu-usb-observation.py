#!/usr/bin/env python3
"""Integrate a signed USB/CPU parent into the fixed CPU WinPE baseline."""
import argparse, hashlib, json, os, shutil, struct, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'port/xnu/out'
BASE=OUT/'windows-rt-cpu-register-touch-gpu-5341b16'
WIM='fa0dc33357b24cd73d812de3cd468d6437edb28837b7e2b961166cda2db0e077'
ESP='eb94dc4da2e7e74f039063e1a4f54ff457936951caa60ee877979a45d78847ca'
BOOT='8dc66fe55b31f80106a15767c353d98d40001d3778a6d539a99a675ddd6feaf4'
START='87a5f5071f9fa4164bdbb8f50b782bb11fe1ca3e97dd97b5a5e600780abc734d'


def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def run(cmd,**kw):return subprocess.run([str(x) for x in cmd],check=True,**kw)
def within(p):
 p=p.resolve()
 if not p.is_relative_to(ROOT): raise ValueError('outside project: '+str(p))
 return p
def expect(p,d):
 p=within(p)
 if sha(p)!=d.lower():raise ValueError('hash mismatch '+str(p))
 return p

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--candidate',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--expected-commit',required=True);a=ap.parse_args()
 src=within(a.candidate);out=within(a.output)
 if out.exists():raise ValueError('output must not exist')
 manifest=json.loads((src/'manifest.json').read_text(encoding='utf-8-sig'))
 if not manifest['source_commit']==a.expected_commit or manifest['cpu_observe']!=1 or manifest['usb_observe']!=1 or manifest['gpio22_source']!=0:raise ValueError('candidate flags or source mismatch')
 for n,h in manifest['files'].items():
  if Path(n).is_absolute() or '..' in Path(n).parts:raise ValueError('invalid candidate file')
  expect(src/n,h)
 for n,h in [('volume.wim',WIM),('volume-esp.img',ESP),('boot.img',BOOT)]:expect(BASE/n,h)
 base=json.loads((BASE/'manifest.json').read_text());assert base['source_commit']=='5341b16' and base['uefi_commit']=='e969a1164133dc51b83015ae80d16534849e71f3'
 out.mkdir(parents=True); tools=OUT/'windows-rt-tools/wimtools';env=os.environ.copy();env.update(LD_LIBRARY_PATH=str(tools),TMPDIR=str(out))
 wimtool=tools/'wimlib-imagex'
 with tempfile.TemporaryDirectory(prefix='stage-',dir=out) as t:
  t=Path(t);run([wimtool,'extract',BASE/'volume.wim','1','/Windows/System32/config/SOFTWARE','/Windows/System32/startnet.cmd','--dest-dir='+str(t),'--no-acls'],env=env,stdout=subprocess.DEVNULL)
  hive=t/'SOFTWARE';start=t/'startnet.cmd';expect(start,START)
  data=start.read_bytes();needle=b'echo A04CPU_OBSERVE_LAUNCH\r\n';assert data.count(needle)==1
  data=data.replace(needle,b'echo A04USB_OBSERVATION_START\r\ncall X:\\a04br3\\usb-power-observe.cmd\r\necho A04USB_OBSERVATION_EXIT=%errorlevel%\r\necho A04BQHR_DRVLOAD_START\r\ndrvload X:\\a04br3\\usb-bqhr\\bqhr-observe.inf\r\necho A04BQHR_DRVLOAD_EXIT=%errorlevel%\r\nX:\\a04br3\\usb-bqhr-uart.exe\r\necho A04BQHR_OBSERVATION_EXIT=%errorlevel%\r\n'+needle);start.write_bytes(data)
  cert=(src/'cpu-observe-test.cer').read_bytes();thumb=hashlib.sha1(cert).hexdigest().upper();blob=struct.pack('<III',32,1,len(cert))+cert
  run(['openssl','x509','-inform','DER','-in',src/'cpu-observe-test.cer','-noout'],stdout=subprocess.DEVNULL)
  for store in ('Root','TrustedPublisher'):
   path='\\Microsoft\\SystemCertificates\\'+store+'\\Certificates'
   old=run(['hivexsh',hive],input='cd '+path+'\nls\n',text=True,capture_output=True)
   if old.stderr or thumb in old.stdout.upper():raise ValueError('certificate store unavailable or duplicate')
   script=f'cd {path}\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:{blob.hex()}\ncommit\n'
   result=run(['hivexsh','-w',hive],input=script,text=True,capture_output=True)
   if result.stderr or run(['hivexget',hive,path+'\\'+thumb,'Blob'],capture_output=True).stdout!=blob:raise ValueError('certificate write failed')
  wim=out/'volume.wim';shutil.copyfile(BASE/'volume.wim',wim)
  mapping={'mt8167-shared.sys':'/a04br3/mt8167-shared/mt8167-shared.sys','shared-parent.inf':'/a04br3/mt8167-shared/shared-parent.inf','shared-parent.cat':'/a04br3/mt8167-shared/shared-parent.cat','usb-power-uart.exe':'/a04br3/usb-power-uart.exe','usb-power-observe.cmd':'/a04br3/usb-power-observe.cmd','usb-bqhr-uart.exe':'/a04br3/usb-bqhr-uart.exe','bqhr/bqhr-observe.sys':'/a04br3/usb-bqhr/bqhr-observe.sys','bqhr/bqhr-observe.inf':'/a04br3/usb-bqhr/bqhr-observe.inf','bqhr/bqhr-observe.cat':'/a04br3/usb-bqhr/bqhr-observe.cat'}
  commands=[f'add {hive} /Windows/System32/config/SOFTWARE',f'add {start} /Windows/System32/startnet.cmd']+[f'add {src/n} {dest}' for n,dest in mapping.items()]
  if any(' ' in str(p) for p in (ROOT,src,out)):raise ValueError('spaces in wim update path')
  run([wimtool,'update',wim,'1'],input='\n'.join(commands)+'\n',text=True,env=env)
  run([wimtool,'verify',wim],env=env)
  check=t/'check';paths=list(mapping.values())+['/Windows/System32/config/SOFTWARE','/Windows/System32/startnet.cmd']
  run([wimtool,'extract',wim,'1',*paths,'--dest-dir='+str(check),'--no-acls'],env=env,stdout=subprocess.DEVNULL)
  if sha(check/'startnet.cmd')!=sha(start):raise ValueError('startup mismatch')
  for name,dest in mapping.items():
   candidates=list(check.rglob(Path(dest).name))
   if len(candidates)!=1 or sha(candidates[0])!=sha(src/name):raise ValueError('WIM mismatch '+name)
  for store in ('Root','TrustedPublisher'):
   path='\\Microsoft\\SystemCertificates\\'+store+'\\Certificates\\'+thumb
   if run(['hivexget',check/'SOFTWARE',path,'Blob'],capture_output=True).stdout!=blob:raise ValueError('WIM certificate mismatch')
  image=out/'volume-esp.img';shutil.copyfile(BASE/'volume-esp.img',image)
  run([tools/'mdel','-i',image,'::/sources/boot.wim'],env=env)
  run([tools/'mcopy','-i',image,wim,'::/sources/boot.wim'],env=env)
  run(['fsck.vfat','-n',image],env=env)
  verify=t/'fat-wim';run([tools/'mcopy','-i',image,'::/sources/boot.wim',verify],env=env)
  if sha(verify)!=sha(wim):raise ValueError('ESP WIM mismatch')
  bcd=t/'BCD';run([tools/'mcopy','-i',image,'::/EFI/Microsoft/Boot/BCD',bcd],env=env)
  expect(bcd,base['bcd_sha256'])
  shutil.copyfile(BASE/'boot.img',out/'boot.img');shutil.copyfile(BASE/'boot-manifest.json',out/'boot-manifest.json');shutil.copyfile(BASE/'firmware-manifest.json',out/'firmware-manifest.json')
  m={'source_commit':manifest['source_commit'],'base_cpu_commit':'5341b16','uefi_commit':base['uefi_commit'],'base_wim_sha256':WIM,'base_esp_sha256':ESP,'boot_sha256':sha(out/'boot.img'),'wim_sha256':sha(wim),'esp_sha256':sha(image),'bcd_sha256':sha(bcd),'signer_sha1':thumb,'candidate_files':manifest['files'],'startnet_sha256':sha(start),'limitations':['CPU and USB share the one parent and WACS lock; timeout may poison the bus','VCDT conversion and charger I2C control remain unavailable','BQHR diagnostic occupies MOU0008 function-driver binding and cannot coexist with the fuel gauge driver; record is not proof of ownership','CHRSTATUS and VUSB are register snapshots, not a connector voltage measurement','UART queue delivery, target trust, PnP and coexistence are unverified','No hardware access was performed']}
  (out/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
 print(out)
if __name__=='__main__':main()
