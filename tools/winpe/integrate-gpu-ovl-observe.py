#!/usr/bin/env python3
import hashlib,json,os,re,shutil,struct,subprocess,sys,tempfile,zipfile
from pathlib import Path
R=Path(__file__).resolve().parents[4];O=R/'port/xnu/out';B=O/'windows-rt-common-frame-d76005b-integrated';P=O/'windows-rt-gpu-ovl-observe-package';OUT=O/'windows-rt-gpu-ovl-observe-integrated';BOOT=O/'windows-rt-fourcpu-l1-e969a116';U=BOOT;HELP=R/'windows-rt/a04br3-windows-ge8300-copy-safe-stop/tools/winpe/replace-esp-wim.py'
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def run(*a,**k):return subprocess.run(*a,check=True,**k)
m=json.loads((B/'manifest.json').read_text());boot=json.loads((BOOT/'manifest.json').read_text());fm=json.loads((U/'firmware-manifest.json').read_text());em=json.loads((B/'volume-esp.img.json').read_text())
archive=P/'shared-diagnostic-signed.zip'
assert sha(archive)=='4050ceaf78521dd718236824f2941e398ea76b61211c3b548fb39d33addaa563'
assert sha(B/'volume.wim')==m['wim_sha256']==em['wim_sha256'] and sha(B/'volume-esp.img')==m['esp_sha256']==em['image_sha256']
assert boot['uefi_source_commit']=='e969a1164133dc51b83015ae80d16534849e71f3' and sha(BOOT/'boot.img')=='8dc66fe55b31f80106a15767c353d98d40001d3778a6d539a99a675ddd6feaf4' and sha(BOOT/'boot.img')==boot['boot_img_sha256'] and boot['uefi_source_commit']==fm['source_commit'] and boot['firmware_sha256']==fm['shimmed_fd_sha256']
assert not OUT.exists();OUT.mkdir();env=os.environ.copy();env.update(TMPDIR=str(OUT),PYTHONDONTWRITEBYTECODE='1',PYTHONPATH=str(O/'windows-rt-tools/python-fat'),PATH=str(O/'windows-rt-tools/wimtools')+':'+os.environ['PATH'],LD_LIBRARY_PATH=str(O/'windows-rt-tools/wimtools'))
with tempfile.TemporaryDirectory(prefix='gce-input-',dir=OUT) as td:
 q=Path(td);counter=0
 with zipfile.ZipFile(archive) as z:
  assert z.testzip() is None
  for item in z.infolist():
   dest=q/'package'/item.filename.replace('\\','/')
   assert dest.resolve().is_relative_to((q/'package').resolve()) and not item.is_dir()
   dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(z.read(item))
 package=q/'package'
 assert sha(package/'ge8300/ge8300-mips.fw')==m['gpu_fw_sha256']
 pem=q/'test-cert.pem';run(['openssl','x509','-inform','DER','-in',str(package/'mt8167-display-test.cer'),'-out',str(pem)],env=env)
 verifier=O/'windows-rt-tools/osslsigncode-local/extracted/usr/bin/osslsigncode'
 for folder,name in [('ge8300','mt8167-ge8300'),('display','mt8167-display')]:
  sysfile=package/folder/(name+'.sys');catfile=package/folder/(name+'.cat')
  for label,args in [('sys',['-in',str(sysfile)]),('catalog',['-in',str(catfile)]),('membership',['-catalog',str(catfile),'-in',str(sysfile)])]:
   result=run([str(verifier),'verify','-CAfile',str(pem),*args],env=env,capture_output=True,text=True)
   assert 'Signature verification: ok' in result.stdout and 'Succeeded' in result.stdout,(folder,label)
   if label=='membership':assert 'File is signed in catalog' in result.stdout
   (OUT/(folder+'-'+label+'-signature.log')).write_text(result.stdout+result.stderr)
 def extract(wim,path):
  global counter
  counter+=1;dest=q/str(counter);dest.mkdir();run(['wimlib-imagex','extract',str(wim),'1',path,f'--dest-dir={dest}'],env=env,stdout=subprocess.DEVNULL)
  files=[x for x in dest.rglob('*') if x.is_file()];assert len(files)==1,path;return files[0]
 start=extract(B/'volume.wim','/Windows/System32/startnet.cmd');hive=extract(B/'volume.wim','/Windows/System32/config/SOFTWARE')
 assert sha(start)==m['verified_wim_files_sha256']['/Windows/System32/startnet.cmd'] and sha(hive)==m['verified_wim_files_sha256']['/Windows/System32/config/SOFTWARE']
 raw=start.read_bytes()
 assert raw.count(b'A04_SHARED_DIAGNOSTIC_AUTOSTART_WAIT')==1
 assert raw.index(b'mt8167-volume-listener.exe')<raw.index(b'shared-parent.inf')<raw.index(b'shared-touch.inf')<raw.index(b'ge8300-vendor-copy-test.exe')<raw.index(b'display-shared\\mt8167-display.inf')
 cert=(package/'mt8167-display-test.cer').read_bytes();thumb=hashlib.sha1(cert).hexdigest().upper();blob=struct.pack('<III',32,1,len(cert))+cert
 for store in ('Root','TrustedPublisher'):
  base='\\Microsoft\\SystemCertificates\\'+store+'\\Certificates'
  def get(t):return run(['hivexget',str(hive),base+'\\'+t,'Blob'],capture_output=True).stdout
  listing=run(['hivexsh',str(hive)],input=f'cd {base}\nls\n',text=True,capture_output=True);assert not listing.stderr
  old={t:get(t) for t in listing.stdout.splitlines() if re.fullmatch('[0-9A-Fa-f]{40}',t)};assert len(old)>=4 and thumb not in old and all(old.values())
  x=run(['hivexsh','-w',str(hive)],input=f'cd {base}\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:{blob.hex()}\ncommit\n',text=True,capture_output=True);assert not x.stderr and get(thumb)==blob and all(get(t)==v for t,v in old.items())
 files={'/Windows/System32/startnet.cmd':start,'/Windows/System32/config/SOFTWARE':hive}
 for folder,target in [('display','display-shared'),('ge8300','ge8300-copy')]:
  for p in (package/folder).iterdir():
   if p.is_file():files['/a04br3/'+target+'/'+p.name]=p
 files['/a04br3/display-shared/gpu-post-test.cer']=package/'mt8167-display-test.cer'
 wim=OUT/'volume.wim';shutil.copyfile(B/'volume.wim',wim);run(['wimlib-imagex','update',str(wim),'1'],input='\n'.join(f'add {v} {k}' for k,v in files.items())+'\n',text=True,env=env);run(['wimlib-imagex','verify',str(wim)],env=env)
 preserved={p:h for p,h in m['verified_wim_files_sha256'].items() if p not in files}
 for path,h in (preserved|{k:sha(v) for k,v in files.items()}).items():assert sha(extract(wim,path))==h,path
 esp=OUT/'volume-esp.img';run(['python3',str(HELP),'--template',str(B/'volume-esp.img'),'--wim',str(wim),'--output',str(esp),'--template-bcd-sha256',em['bcd_sha256'],'--template-wim-sha256',em['wim_sha256']],env=env)
 for src,dst in [(BOOT/'boot.img','boot.img'),(BOOT/'manifest.json','boot-manifest.json'),(U/'firmware-manifest.json','firmware-manifest.json')]:shutil.copyfile(src,OUT/dst)
 sys.path.insert(0,str(O/'windows-rt-tools/python-fat'));from pyfatfs.PyFatFS import PyFatFS
 def fats(path):
  result={}
  with PyFatFS(str(path),read_only=True) as fat:
   for n in fat.walk.files():
    if n.lower()=='/sources/boot.wim':continue
    with fat.openbin(n,'r') as stream:result[n]=hashlib.file_digest(stream,'sha256').hexdigest()
  return result
 fat=fats(B/'volume-esp.img');assert fat==fats(esp)
 result={'integration_script_sha256':sha(Path(__file__)),'status':'host-verified-device-untested','base_manifest_sha256':sha(B/'manifest.json'),'base_wim_sha256':m['wim_sha256'],'base_esp_sha256':m['esp_sha256'],'uefi_commit':fm['source_commit'],'windows_commit':'808bd59','signature_workflow':'https://github.com/mouseos/a04br3-windows-drivers/actions/runs/42187724978','certificate_thumbprint':thumb,'boot_sha256':sha(OUT/'boot.img'),'firmware_manifest_sha256':sha(OUT/'firmware-manifest.json'),'signed_package_sha256':sha(archive),'gpu_fw_sha256':m['gpu_fw_sha256'],'usc_sha256':m['usc_sha256'],'wim_sha256':sha(wim),'esp_sha256':sha(esp),'bcd_sha256':em['bcd_sha256'],'wim_image_count':1,'wim_boot_index':1,'verified_wim_files_sha256':preserved|{k:sha(v) for k,v in files.items()},'preserved_fat_files_sha256':fat,'limitations':['未実機検証。所有権とsecure利用は不明','OVL観測付きGPUコピーをCPUでPOSTへ転送。直接scanout・3D加速ではない','GCE IRQ attach OFF、CMDQ投入とOVL切替OFF','thread/IRQのMMIO観測は所有権成立時のみ','テスト署名のWindows RT受理は未確認']}
 (OUT/'manifest.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print(json.dumps({k:result[k] for k in ('boot_sha256','wim_sha256','esp_sha256','certificate_thumbprint')},indent=2))
