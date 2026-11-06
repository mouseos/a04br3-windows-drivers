#!/usr/bin/env python3
"""Combine a pinned read-only audit WIM with matched GPU and GCE clock boot."""
import hashlib,json,os,re,shutil,struct,subprocess,sys,tempfile,zipfile
from pathlib import Path
R=Path(__file__).resolve().parents[4];O=R/'port/xnu/out'
BASE=O/'windows-rt-cpu-usb-audit-scope-v1';P=O/'windows-rt-gce-clock-wddm-package';BOOT=O/'windows-rt-gce-clock-ffbe1a35-boot';OUT=O/'windows-rt-gce-cpu-usb-audit-integrated'
HELP=R/'windows-rt/a04br3-windows-ge8300-copy-safe-stop/tools/winpe/replace-esp-wim.py'
WIM='50dd44a4c5e5d2e3b5bb0894951e6684281a52f87d17789b703ea78c035dedf8';ESP='af8466768709745c4bc8664601e1517e0b9d37bb54fdbd802191bec62c7044d8';ZIP='e05b2bde3aff3c2d75d2d4855ad2683a2dcc78a16c7c09eda9a5eb462a64683e';BOOT_SHA='63084a18aede15a6fcd0da7eabd0d7ee1c5a0f5c5dd75274425c18255e5213ef';BCD='65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d'
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def run(*a,**kw):return subprocess.run(*a,check=True,**kw)
assert sha(BASE/'maintenance-staged.wim')==WIM and sha(BASE/'maintenance-staged-esp.img')==ESP
assert sha(P/'shared-diagnostic-signed.zip')==ZIP and sha(BOOT/'boot.img')==BOOT_SHA
bm=json.loads((BASE/'delivery-manifest.json').read_text());boot=json.loads((BOOT/'manifest.json').read_text());fm=json.loads((BOOT/'firmware-manifest.json').read_text())
assert bm['wim_sha256']==WIM and bm['esp_sha256']==ESP and bm['wim_boot_index']==1 and bm['wim_image_count']==1
assert boot['uefi_source_commit']=='ffbe1a35bac5609f6f582c4ea103d5f29c4bcbe8' and boot['boot_img_sha256']==BOOT_SHA
assert sha(BOOT/'firmware-manifest.json')==boot['firmware_manifest_sha256'] and sha(BOOT/'firmware.fd')==boot['firmware_sha256']==fm['shimmed_fd_sha256'] and fm['build_defines']['A04BR3_GCE_OBSERVATION']=='1'
assert not OUT.exists();OUT.mkdir();env=os.environ.copy();env.update(TMPDIR=str(OUT),PYTHONDONTWRITEBYTECODE='1',PYTHONPATH=str(O/'windows-rt-tools/python-fat'),PATH=str(O/'windows-rt-tools/wimtools')+':'+os.environ['PATH'],LD_LIBRARY_PATH=str(O/'windows-rt-tools/wimtools'))
def inventory(wim):
 text=run(['wimlib-imagex','dir',str(wim),'1','--detailed'],env=env,capture_output=True,text=True).stdout;out={}
 for block in text.split('----------------------------------------------------------------------------'):
  path=re.search(r'Full Path\s*= "([^"]+)"',block)
  if not path:continue
  name=path.group(1);attrs=re.search(r'Attributes\s*= (0x[0-9a-fA-F]+)',block);sd=re.search(r'Security Descriptor\s*= ([^\r\n]+)',block)
  assert name not in out and attrs and '\n' not in name
  out[name]=(int(attrs.group(1),16),sd.group(1) if sd else None,tuple(re.findall(r'Hash\s*= (0x[0-9a-fA-F]+)',block)))
 return out
with tempfile.TemporaryDirectory(prefix='gce-audit-',dir=OUT) as td:
 q=Path(td);n=0
 def extract(wim,path):
  global n
  n+=1;dest=q/str(n);dest.mkdir();run(['wimlib-imagex','extract',str(wim),'1',path,f'--dest-dir={dest}'],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  files=[x for x in dest.rglob('*') if x.is_file()];assert len(files)==1,path;return files[0]
 with zipfile.ZipFile(P/'shared-diagnostic-signed.zip') as z:
  assert z.testzip() is None
  for item in z.infolist():
   dest=q/'package'/item.filename.replace('\\','/');assert not item.is_dir() and dest.resolve().is_relative_to((q/'package').resolve())
   dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(z.read(item))
 package=q/'package';cert=package/'mt8167-display-test.cer';pem=q/'gpu.pem';run(['openssl','x509','-inform','DER','-in',str(cert),'-out',str(pem)],env=env)
 verifier=O/'windows-rt-tools/osslsigncode-local/extracted/usr/bin/osslsigncode';verification={}
 for folder,name in [('ge8300','mt8167-ge8300'),('display','mt8167-display')]:
  sysfile=package/folder/(name+'.sys');catfile=package/folder/(name+'.cat')
  for label,args in [('sys',['-in',str(sysfile)]),('cat',['-in',str(catfile)]),('membership',['-catalog',str(catfile),'-in',str(sysfile)])]:
   v=run([str(verifier),'verify','-CAfile',str(pem),*args],env=env,capture_output=True,text=True)
   assert 'Signature verification: ok' in v.stdout and 'Succeeded' in v.stdout
   if label=='membership':assert 'File is signed in catalog' in v.stdout
   (OUT/(folder+'-'+label+'-signature.log')).write_text(v.stdout+v.stderr);verification[folder+'-'+label]=True
 for exe in (package/'ge8300').glob('*.exe'):
  v=run([str(verifier),'verify','-CAfile',str(pem),'-in',str(exe)],env=env,capture_output=True,text=True)
  assert 'Signature verification: ok' in v.stdout and 'Succeeded' in v.stdout;verification[exe.name]=True
 assert sha(package/'ge8300/ge8300-mips.fw')=='5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1'
 base=BASE/'maintenance-staged.wim';before=inventory(base);start=extract(base,'/Windows/System32/startnet.cmd');original=start.read_bytes()
 marker=b':gpu_stage\r\n';old=b'call X:\\a04br3\\run-audit-once.cmd';assert original.count(marker)==1 and original.count(old)==1
 insert=(b':gpu_stage\r\necho A04_AUDIT_BEFORE_GPU\r\ncall X:\\a04br3\\run-audit-once.cmd\r\n'
         b'echo A04_AUDIT_BEFORE_GPU_EXIT=%errorlevel%\r\nset A04_AUDIT_DONE=1\r\n')
 updated=original.replace(old,b'if not defined A04_AUDIT_DONE call X:\\a04br3\\run-audit-once.cmd').replace(marker,insert)
 assert updated.index(b'mt8167-volume-listener.exe')<updated.index(b'usb-power-observe.cmd')<updated.index(b'fourcpu-load.exe')<updated.index(b'A04_AUDIT_BEFORE_GPU')<updated.index(b'GE8300_COPY_DRVLOAD_START')
 start.write_bytes(updated);hive=extract(base,'/Windows/System32/config/SOFTWARE');der=cert.read_bytes();thumb=hashlib.sha1(der).hexdigest().upper();assert thumb=='8966067FC3A54A98AF31F3F3752B0817D6D650E1';blob=struct.pack('<III',32,1,len(der))+der;old_counts={}
 for store in ('Root','TrustedPublisher'):
  key='\\Microsoft\\SystemCertificates\\'+store+'\\Certificates'
  def get(t):return run(['hivexget',str(hive),key+'\\'+t,'Blob'],capture_output=True).stdout
  listing=run(['hivexsh',str(hive)],input=f'cd {key}\nls\n',text=True,capture_output=True);assert not listing.stderr
  oldcert={t:get(t) for t in listing.stdout.splitlines() if re.fullmatch('[0-9A-Fa-f]{40}',t)};assert len(oldcert)>3 and thumb not in oldcert;old_counts[store]=len(oldcert)
  change=run(['hivexsh','-w',str(hive)],input=f'cd {key}\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:{blob.hex()}\ncommit\n',text=True,capture_output=True);assert not change.stderr and get(thumb)==blob and all(get(t)==v for t,v in oldcert.items())
 files={'/Windows/System32/startnet.cmd':start,'/Windows/System32/config/SOFTWARE':hive}
 for folder,target in [('ge8300','ge8300-copy'),('display','display-shared')]:
  for p in (package/folder).iterdir():
   if p.is_file():files['/a04br3/'+target+'/'+p.name]=p
 files['/a04br3/display-shared/gpu-post-test.cer']=cert
 wim=OUT/'volume.wim';shutil.copyfile(base,wim);run(['wimlib-imagex','update',str(wim),'1'],input='\n'.join(f'add "{v}" "{k}"' for k,v in files.items())+'\n',text=True,env=env);run(['wimlib-imagex','verify',str(wim)],env=env)
 after=inventory(wim);untouched={k:v for k,v in before.items() if k not in files};assert all(after.get(k)==v for k,v in untouched.items())
 assert set(after)-set(before)<=set(files)
 for path,p in files.items():assert sha(extract(wim,path))==sha(p),path
 for path in ['/a04br3/run-audit-once.cmd','/a04br3/usb-power-observe.cmd','/a04br3/cpu/fourcpu-load.exe','/a04br3/cpu/cpu-observe.exe','/a04br3/audit-pnp-inf.exe','/a04br3/mt8167-msdc.sys','/a04br3/bu21025-shared/bu21025.sys','/a04br3/mt8167-shared/mt8167-shared.sys']:
  assert before[path]==after[path]
 esp=OUT/'volume-esp.img';run(['python3',str(HELP),'--template',str(BASE/'maintenance-staged-esp.img'),'--wim',str(wim),'--output',str(esp),'--template-bcd-sha256',BCD,'--template-wim-sha256',WIM],env=env)
 sys.path.insert(0,str(O/'windows-rt-tools/python-fat'));from pyfatfs.PyFatFS import PyFatFS
 def fat(path):
  result={}
  with PyFatFS(str(path),read_only=True) as f:
   for name in f.walk.files():
    if name.lower()=='/sources/boot.wim':continue
    with f.openbin(name,'r') as stream:result[name]=hashlib.file_digest(stream,'sha256').hexdigest()
  return result
 before_fat=fat(BASE/'maintenance-staged-esp.img');assert before_fat==fat(esp)
 for src,dest in [('boot.img','boot.img'),('manifest.json','boot-manifest.json'),('firmware-manifest.json','firmware-manifest.json'),('firmware.fd','firmware.fd')]:shutil.copyfile(BOOT/src,OUT/dest)
 shutil.copyfile(BASE/'delivery-manifest.json',OUT/'audit-base-delivery-manifest.json');shutil.copyfile(BASE/'manifest.json',OUT/'audit-base-manifest.json');shutil.copyfile(BASE/'base-signature-check.json',OUT/'audit-base-signature-check.json')
 result={'status':'host-verified-device-untested','integration_script_sha256':sha(Path(__file__)),'audit_base_wim_sha256':WIM,'audit_base_esp_sha256':ESP,'audit_base_manifest_sha256':sha(BASE/'manifest.json'),'audit_base_delivery_sha256':sha(BASE/'delivery-manifest.json'),'gpu_signed_package_sha256':ZIP,'gpu_windows_commit':'84e3035','uefi_commit':fm['source_commit'],'cpu_source_commit':'43f3aac','audit_source_commit':bm['audit_source_commit'],'usb_source_commit':bm['source_cpu_usb_commit'],'certificate_thumbprints':{'gpu':thumb,'audit':bm['audit_thumbprint'],'scope':bm['scope_thumbprint'],'storage':bm['storage_thumbprint']},'old_certificate_counts_preserved':old_counts,'signature_checks':verification,'boot_sha256':sha(OUT/'boot.img'),'wim_sha256':sha(wim),'esp_sha256':sha(esp),'bcd_sha256':BCD,'wim_image_count':1,'wim_boot_index':1,'verified_changed_files_sha256':{k:sha(v) for k,v in files.items()},'untouched_wim_record_count':len(untouched),'unchanged_fat_file_count':len(before_fat),'startup':'listener, parent/touch, USB observation, CPU observe, CPU load, audit/release/UART queue, GPU/display','uart_token':bm['uart_token'],'limitations':['test-signed; target acceptance and execution unverified','audit is observational, not an OS update or capacity authorization; queue acceptance does not prove UART delivery','GPU runs after audit transmission attempt; UART transmission drain remains unproven','parent CPU/USB observation version differs from earlier touch/GPU tested parent','GCE TOP2 snapshot only; owner/thread/event and secure state unknown','GCE direct scanout, switching, display-DMA visibility and safe retirement unproven; OVL switching OFF']}
 (OUT/'manifest.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps(result,ensure_ascii=False,indent=2))
