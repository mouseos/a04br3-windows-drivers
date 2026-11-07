#!/usr/bin/env python3
import hashlib,json,os,re,shutil,struct,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[4];O=R/'port/xnu/out';BASE=O/'windows-rt-present-capture-integrated';CPU=O/'windows-rt-fourcpu-uart-43f3aac';BOOT=O/'windows-rt-fourcpu-l1-e969a116';OUT=O/'windows-rt-present-capture-fourcpu-43f3aac';HELPER=R/'windows-rt/a04br3-windows-ge8300-copy-safe-stop/tools/winpe/replace-esp-wim.py'
WIM='e1f1d0bb34e938c3e8648f93876ddacb8d5f742a9ba9acf7e16d0fc133c5219e';ESP='09c533465e3a8bf8dfe8a1389aff765738bc91602e98008e1c20d7c9c2f2c909';BOOT_HASH='8dc66fe55b31f80106a15767c353d98d40001d3778a6d539a99a675ddd6feaf4';BCD='65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d'
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def run(*a,**kw):return subprocess.run(*a,check=True,**kw)
for p,h in [(BASE/'volume.wim',WIM),(BASE/'volume-esp.img',ESP),(BOOT/'boot.img',BOOT_HASH)]:assert p.resolve().is_relative_to(R) and sha(p)==h,p
bm=json.loads((BASE/'manifest.json').read_text());cm=json.loads((CPU/'manifest.json').read_text());fm=json.loads((BOOT/'firmware-manifest.json').read_text());assert bm['wim_sha256']==WIM and bm['esp_sha256']==ESP and cm['source_commit']=='43f3aac' and sha(CPU/'fourcpu-load.exe')==cm['exe_sha256'] and sha(CPU/'fourcpu-test.cer')==cm['certificate_sha256'] and fm['shim_defines']['A04BR3_FOUR_CPU_L1_CLEAN']==1
if OUT.exists():raise ValueError('output exists')
OUT.mkdir();env=os.environ.copy();env.update(TMPDIR=str(OUT),PYTHONDONTWRITEBYTECODE='1',PYTHONPATH=str(O/'windows-rt-tools/python-fat'),PATH=str(O/'windows-rt-tools/wimtools')+':'+os.environ['PATH'],LD_LIBRARY_PATH=str(O/'windows-rt-tools/wimtools'))
with tempfile.TemporaryDirectory(prefix='cpu-',dir=OUT) as td:
 tmp=Path(td);counter=0
 pem=tmp/'cpu.pem';run(['openssl','x509','-inform','DER','-in',str(CPU/'fourcpu-test.cer'),'-out',str(pem)],env=env)
 verifier=O/'windows-rt-tools/osslsigncode-local/extracted/usr/bin/osslsigncode'
 verification=run([str(verifier),'verify','-CAfile',str(pem),'-in',str(CPU/'fourcpu-load.exe')],env=env,capture_output=True,text=True)
 assert 'Signature verification: ok' in verification.stdout and 'Succeeded' in verification.stdout
 (OUT/'cpu-signature.log').write_text(verification.stdout+verification.stderr)
 def extract(wim,path):
  global counter
  counter+=1;dest=tmp/('x'+str(counter));dest.mkdir();run(['wimlib-imagex','extract',str(wim),'1',path,f'--dest-dir={dest}'],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);files=[p for p in dest.rglob('*') if p.is_file()];assert len(files)==1,path;return files[0]
 checks={k:v for k,v in bm['verified_wim_files_sha256'].items() if k not in ['/Windows/System32/startnet.cmd','/Windows/System32/config/SOFTWARE']}
 for k,v in checks.items():assert sha(extract(BASE/'volume.wim',k))==v,k
 start=extract(BASE/'volume.wim','/Windows/System32/startnet.cmd');assert sha(start)==bm['verified_wim_files_sha256']['/Windows/System32/startnet.cmd'];data=start.read_bytes()
 marker=b'start "" /b X:\\a04br3\\mt8167-volume-listener.exe\r\n';insert=marker+b'echo A04CPU_LOAD_LAUNCH\r\nstart "" /b X:\\a04br3\\cpu\\fourcpu-load.exe\r\n'
 assert data.count(marker)==1 and b'fourcpu-load.exe' not in data;data=data.replace(marker,insert);start.write_bytes(data)
 assert data.index(b'mt8167-volume-listener.exe')<data.index(b'fourcpu-load.exe')<data.index(b'shared-parent.inf')<data.index(b'ge8300-vendor-copy-test.exe')
 hive=extract(BASE/'volume.wim','/Windows/System32/config/SOFTWARE');assert sha(hive)==bm['verified_wim_files_sha256']['/Windows/System32/config/SOFTWARE'];cert=(CPU/'fourcpu-test.cer').read_bytes();thumb=hashlib.sha1(cert).hexdigest().upper();assert thumb==cm['signer_sha1'];blob=struct.pack('<III',32,1,len(cert))+cert;old_count={}
 for store in ('Root','TrustedPublisher'):
  base='\\Microsoft\\SystemCertificates\\'+store+'\\Certificates'
  def get(t):return run(['hivexget',str(hive),base+'\\'+t,'Blob'],capture_output=True).stdout
  listing=run(['hivexsh',str(hive)],input=f'cd {base}\nls\n',text=True,capture_output=True);assert not listing.stderr
  old={t:get(t) for t in listing.stdout.splitlines() if re.fullmatch('[0-9A-Fa-f]{40}',t)};assert len(old)>3 and thumb not in old;old_count[store]=len(old)
  o=run(['hivexsh','-w',str(hive)],input=f'cd {base}\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:{blob.hex()}\ncommit\n',text=True,capture_output=True);assert not o.stderr
  assert get(thumb)==blob and all(get(t)==v for t,v in old.items())
 changes={'/Windows/System32/startnet.cmd':start,'/Windows/System32/config/SOFTWARE':hive,'/a04br3/cpu/fourcpu-load.exe':CPU/'fourcpu-load.exe','/a04br3/cpu/fourcpu-test.cer':CPU/'fourcpu-test.cer'}
 wim=OUT/'volume.wim';shutil.copyfile(BASE/'volume.wim',wim);run(['wimlib-imagex','update',str(wim),'1'],input='\n'.join(f'add {v} {k}' for k,v in changes.items())+'\n',text=True,env=env);run(['wimlib-imagex','verify',str(wim)],env=env)
 checks.update({k:sha(v) for k,v in changes.items()})
 for k,v in checks.items():assert sha(extract(wim,k))==v,k
 esp=OUT/'volume-esp.img';run(['python3',str(HELPER),'--template',str(BASE/'volume-esp.img'),'--wim',str(wim),'--output',str(esp),'--template-bcd-sha256',BCD,'--template-wim-sha256',WIM],env=env)
 for n in ('boot.img','firmware.fd','firmware-manifest.json'):shutil.copyfile(BOOT/n,OUT/n)
 shutil.copyfile(BOOT/'manifest.json',OUT/'boot-manifest.json');shutil.copyfile(CPU/'manifest.json',OUT/'cpu-signature-manifest.json')
 x={'status':'host-verified; no target operation','base_esp_sha256':ESP,'base_wim_sha256':WIM,'base_gpu_ovl_windows_commit':bm['windows_commit'],'integration_script_sha256':sha(Path(__file__)),'ovl_sample_steps':['90-93','94-97'],'present_capture_step':'9B','input_source_step':'9C','explicit_gpu_dma_flush_step':'98','wim_image_count':1,'wim_boot_index':1,'cpu_source_commit':cm['source_commit'],'cpu_exe_sha256':cm['exe_sha256'],'cpu_cert_sha1':thumb,'cpu_cert_sha256':cm['certificate_sha256'],'uefi_commit':fm['source_commit'],'uefi_cpu1_l1_clean':fm['shim_defines']['A04BR3_FOUR_CPU_L1_CLEAN'],'boot_sha256':sha(OUT/'boot.img'),'wim_sha256':sha(wim),'esp_sha256':sha(esp),'bcd_sha256':BCD,'old_certificate_counts_preserved':old_count,'verified_wim_files_sha256':checks,'gpu_certificate_thumbprint':bm['certificate_thumbprint'],'startup':'listener then background CPU load; parent/touch/GPU run concurrently','cpu_uart':'A04CU1 CRC32 records, 3 repeats; decoder requires 4 CPU progress intervals and complete 60s final report','limitations':['CPU executable test-signed; target acceptance and execution not established','logical CPUs and 60s workload are not physical topology, long-term stability or suspend/resume proof','direct GPU output and UART listener can interleave; incomplete or conflicting records are inconclusive','latest user-reported CRC audit LOCKED/FLUSHED/DISMOUNTED succeeded for that audit; does not guarantee current state','GPU DMA retention and external fabric quiescence remain unproved; do not infer reset safety from volume release','CPU load runs concurrently with GPU initialization and may affect timings','present uses a CPU POST snapshot or a synthetic fallback; no direct scanout','explicit GPU DMA transfer flush is experimental and has not run on hardware','OVL observations do not establish GCE ownership, display-DMA visibility, frame latch or GPU direct scanout','firmware and frame-seal boot were reported to start on hardware, this new combined WIM is not tested']}
 (OUT/'manifest.json').write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n');print(json.dumps(x,ensure_ascii=False,indent=2))
