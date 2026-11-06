#!/usr/bin/env python3
"""署名済み共有親・touchと0x714 UEFIをコピー診断媒体へ組み込む。"""
import argparse, hashlib, json, os, shutil, struct, subprocess, tempfile, zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
WIM='d9378ee260f628a23bae1ae2476d01b38f889206a833c15eb9db64252b5f1c38'
ESP='31657861560e13652cb459454ac8379873b9b9c6d57c8bb19e52e97e13bf8fe3'
BOOT='94c3ea8ec2201ca5412562042ce05e5329793cbe3efb416f063d8555e226b34d'
ZIP='901ce4f0d79a440002ca03a348fa8759449949d14e8c2cb3bdca7fc4072a9c88'
BCD='65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d'
SYSTEM='cc151bf339dd2683b313e95d42179cfe571f6392c7a294b9a98f19a8a83e5214'
START='4afb2ebd2d57cca36add257fd82bc90677dca277d9d33e30b85a4d49b72664c3'
LISTENER='3621ae28798c7ceeed3175fe88794a670524ca8383899681de4689751fe9244d'
GPU='3d1791131becc3db447bbe9667ed97571d58dd06d45a76e6c17a99584211ae11'
OLD=('0982A8A74E3076EAC252F3EB244BAF77556D2229','B80231026FFEFE30224048CA83EE7631EFF3FABC','477AA2039125221B85A5614DDA7BCC61A291B560')
FILES={'parent/mt8167-shared.sys','parent/shared-parent.cat','parent/shared-parent.inf','touch/bu21025.sys','touch/bu21025.cat','touch/shared-touch.inf','shared-touch-test.cer','manifest.txt','sha256.txt'}
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def run(*a,**kw):return subprocess.run(*a,check=True,**kw)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('source-wim','source-esp','boot','package','output'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();out=a.output.resolve()
 if not out.is_relative_to(ROOT) or not out.is_dir() or any(out.iterdir()):raise SystemExit('出力先はルート内の空ディレクトリに限ります')
 for path,want in ((a.source_wim,WIM),(a.source_esp,ESP),(a.boot,BOOT),(a.package,ZIP)):
  if not path.resolve().is_relative_to(ROOT) or sha(path)!=want:raise SystemExit('入力の実体またはSHAが不一致: '+str(path))
 if any(c.isspace() or c in "\"'" for c in str(out)):raise SystemExit('パスに空白または引用符があります')
 env=os.environ.copy();env.update(TMPDIR=str(out),PYTHONDONTWRITEBYTECODE='1')
 with tempfile.TemporaryDirectory(prefix='apmixed-',dir=out) as d:
  tmp=Path(d);pkg=tmp/'package';pkg.mkdir()
  with zipfile.ZipFile(a.package) as z:
   entries=[i for i in z.infolist() if not i.is_dir()];names=[i.filename.replace('\\','/') for i in entries]
   if len(entries)!=len(FILES) or set(names)!=FILES:raise ValueError('パッケージ構成不一致')
   for i,n in zip(entries,names):
    dest=pkg/n;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(z.read(i))
  cert=(pkg/'shared-touch-test.cer').read_bytes();thumb=hashlib.sha1(cert).hexdigest().upper();blob=struct.pack('<III',32,1,len(cert))+cert
  run(['openssl','x509','-inform','DER','-in',str(pkg/'shared-touch-test.cer'),'-noout'])
  def extract(wim,target,tag):
   outdir=tmp/tag;outdir.mkdir();run(['wimlib-imagex','extract',str(wim),'1',target,f'--dest-dir={outdir}'],env=env,stdout=subprocess.DEVNULL)
   files=[x for x in outdir.rglob('*') if x.is_file()]
   if len(files)!=1:raise ValueError('抽出失敗: '+target)
   return files[0]
  targets={'/Windows/System32/config/SYSTEM':SYSTEM,'/Windows/System32/startnet.cmd':START,'/a04br3/mt8167-volume-listener.exe':LISTENER,'/a04br3/ge8300-copy/mt8167-ge8300.sys':GPU}
  checks={}
  for i,(target,want) in enumerate(targets.items()):
   f=extract(a.source_wim.resolve(),target,'base-'+str(i))
   if sha(f)!=want:raise ValueError('ベース不一致: '+target)
   checks[target]=f
  start=checks['/Windows/System32/startnet.cmd'];data=start.read_bytes();old=b'X:\\a04br3\\bu21025-shared\\bu21025.inf';new=b'X:\\a04br3\\bu21025-shared\\shared-touch.inf'
  if data.count(old)!=1:raise ValueError('touch INF参照が不一致')
  start.write_bytes(data.replace(old,new))
  hive=extract(a.source_wim.resolve(),'/Windows/System32/config/SOFTWARE','base-hive');old_blobs={}
  for store in ('Root','TrustedPublisher'):
   base='\\Microsoft\\SystemCertificates\\'+store+'\\Certificates'
   for t in OLD:
    old_blobs[store,t]=run(['hivexget',str(hive),base+'\\'+t,'Blob'],capture_output=True).stdout
    if not old_blobs[store,t]:raise ValueError('既存証明書がない: '+t)
   listing=run(['hivexsh',str(hive)],input=f'cd {base}\nls\n',text=True,capture_output=True)
   if listing.stderr or thumb in listing.stdout:raise ValueError('証明書ストアが不正または追加済み')
   cmd=f'cd {base}\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:{blob.hex()}\ncommit\n'
   r=run(['hivexsh','-w',str(hive)],input=cmd,text=True,capture_output=True)
   if r.stderr:raise ValueError('証明書追加失敗')
   if run(['hivexget',str(hive),base+'\\'+thumb,'Blob'],capture_output=True).stdout!=blob:raise ValueError('新証明書不一致')
   for t in OLD:
    if run(['hivexget',str(hive),base+'\\'+t,'Blob'],capture_output=True).stdout!=old_blobs[store,t]:raise ValueError('既存証明書が変更された')
  newfiles={'/Windows/System32/config/SOFTWARE':hive,'/Windows/System32/startnet.cmd':start}
  for sub,dest in (('parent','mt8167-shared'),('touch','bu21025-shared')):
   for source in sorted((pkg/sub).iterdir()):newfiles['/a04br3/'+dest+'/'+source.name]=source
  wim=out/'volume.wim';shutil.copyfile(a.source_wim,wim)
  run(['wimlib-imagex','update',str(wim),'1'],input='\n'.join(f'add {src} {dest}' for dest,src in newfiles.items())+'\n',text=True,env=env)
  run(['wimlib-imagex','verify',str(wim)],env=env)
  compare={k:sha(v) for k,v in checks.items()};compare.update({k:sha(v) for k,v in newfiles.items()})
  for i,(target,want) in enumerate(compare.items()):
   if sha(extract(wim,target,'verify-'+str(i)))!=want:raise ValueError('更新後照合失敗: '+target)
  esp=out/'volume-esp.img';run(['python3',str(Path(__file__).with_name('replace-esp-wim.py')),'--template',str(a.source_esp.resolve()),'--wim',str(wim),'--output',str(esp),'--template-bcd-sha256',BCD,'--template-wim-sha256',WIM],env=env)
  boot=out/'boot.img';shutil.copyfile(a.boot,boot)
  if sha(boot)!=BOOT:raise ValueError('boot SHA不一致')
  m={'parent_commit':'5ee5528a905896ace1f9b95dee1e1c195b4f0423','uefi_commit':'64c43d61b1038612e00b26ad242ddd86b4f7e280','listener_commit':'740117ca372cc8b9c1f4368a0fe39a678e097a06','gpu_commit':'f2a2b5c','parent_package_sha256':ZIP,'parent_package_files_sha256':{str(x.relative_to(pkg)):sha(x) for x in sorted(pkg.rglob('*')) if x.is_file()},'parent_signer_sha1':thumb,'gpu_vendor_copy_diagnostic':1,'source_wim_sha256':WIM,'source_esp_sha256':ESP,'system_hive_sha256':SYSTEM,'listener_sha256':LISTENER,'gpu_sys_sha256':GPU,'new_startnet_sha256':sha(start),'wim_sha256':sha(wim),'esp_sha256':sha(esp),'boot_sha256':sha(boot),'package_build_flags':(pkg/'manifest.txt').read_text(encoding='utf-8-sig'),'limitations':['診断用。GPUコピー、共有親、touch、署名受理、UART解除と同一起動は実機未確認。','0x714資源とGPU・音声・USBの共有所有は未実証。親は音声診断・traceを有効にした署名ビルド。','GPU直接UART出力との混線や欠落は失敗または不明とする。','C:解除はGPU DMA停止を意味せず、解除後resetのDMA安全性は未証明。','thermal観測は所有解放を示さず、このWinPEでは未実機確認。','C:解除の完全確認前に実機書込みしない。']}
  (out/'manifest.json').write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n');print(json.dumps(m,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
