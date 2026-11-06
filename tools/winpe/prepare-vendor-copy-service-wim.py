#!/usr/bin/env python3
"""0x714 WinPEへ署名済みm1.11 bounceコピーサービスを追加する。"""
import argparse,hashlib,json,os,re,shutil,struct,subprocess,tempfile,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
WIM='f794d86e11525a7508c02308b9ca43db1b3b86f0c8fc147f4e0d6c89843c3070'
ESP='78c99778bd885a410bc1e8bf780a39fd7faf83d49b06305a153b86aaa991005d'
BOOT='94c3ea8ec2201ca5412562042ce05e5329793cbe3efb416f063d8555e226b34d'
BCD='65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d'
LISTENER='3621ae28798c7ceeed3175fe88794a670524ca8383899681de4689751fe9244d'
SYSTEM='cc151bf339dd2683b313e95d42179cfe571f6392c7a294b9a98f19a8a83e5214'
PARENT='2d70526cd7b8d4ab8c09cb8a5a6e4299d7546c2d8537416cdefd6aea873a9cdb'
FILES={'ge8300-mips.fw','ge8300-pnp-status.exe','ge8300-status.exe','ge8300-test.cer','ge8300-vendor-stage-status.exe','ge8300-vendor-start-status.exe','ge8300-vendor-copy-test.exe','mt8167-ge8300.cat','mt8167-ge8300.inf','mt8167-ge8300.sys'}
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def run(*a,**kw):return subprocess.run(*a,check=True,**kw)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for x in ('source-wim','source-esp','boot','package','output'):p.add_argument('--'+x,type=Path,required=True)
 p.add_argument('--package-sha256',required=True);p.add_argument('--driver-commit',required=True)
 a=p.parse_args();out=a.output.resolve()
 if not out.is_relative_to(ROOT) or not out.is_dir() or any(out.iterdir()):p.error('outputはルート内の空ディレクトリ')
 if not re.fullmatch('[0-9a-f]{64}',a.package_sha256) or not re.fullmatch('[0-9a-f]{40}',a.driver_commit):p.error('SHA形式が不正')
 for path,want in ((a.source_wim,WIM),(a.source_esp,ESP),(a.boot,BOOT),(a.package,a.package_sha256)):
  if not path.resolve().is_relative_to(ROOT) or sha(path)!=want:p.error('入力実体またはSHA不一致: '+str(path))
 if any(c.isspace() or c in "\"'" for c in str(out)):p.error('空白入り出力先は非対応')
 env=os.environ.copy();env.update(TMPDIR=str(out),PYTHONDONTWRITEBYTECODE='1')
 with tempfile.TemporaryDirectory(prefix='copy-service-',dir=out) as d:
  tmp=Path(d);pkg=tmp/'pkg';pkg.mkdir()
  with zipfile.ZipFile(a.package) as z:
   entries=[x for x in z.infolist() if not x.is_dir()]
   if len(entries)!=len(FILES) or {x.filename for x in entries}!=FILES:raise ValueError('署名パッケージ構成不一致')
   for e in entries:(pkg/e.filename).write_bytes(z.read(e))
  cert=(pkg/'ge8300-test.cer').read_bytes();thumb=hashlib.sha1(cert).hexdigest().upper();blob=struct.pack('<III',32,1,len(cert))+cert
  run(['openssl','x509','-inform','DER','-in',str(pkg/'ge8300-test.cer'),'-noout'])
  def extract(wim,target,tag):
   q=tmp/tag;q.mkdir();run(['wimlib-imagex','extract',str(wim),'1',target,f'--dest-dir={q}'],env=env,stdout=subprocess.DEVNULL)
   fs=[x for x in q.rglob('*') if x.is_file()]
   if len(fs)!=1:raise ValueError('抽出失敗 '+target)
   return fs[0]
  known={'/Windows/System32/config/SYSTEM':SYSTEM,'/a04br3/mt8167-volume-listener.exe':LISTENER,'/a04br3/mt8167-shared/mt8167-shared.sys':PARENT}
  checks={}
  for i,(k,v) in enumerate(known.items()):
   f=extract(a.source_wim.resolve(),k,'base-'+str(i))
   if sha(f)!=v:raise ValueError('ベース不一致 '+k)
   checks[k]=v
  start=extract(a.source_wim.resolve(),'/Windows/System32/startnet.cmd','start');data=start.read_bytes()
  marker=b'%GPU%\\ge8300-status.exe\r\necho GE8300_COPY_STATUS_RETURNED\r\n'
  if data.count(marker)!=1:raise ValueError('startnet挿入位置不一致')
  start.write_bytes(data.replace(marker,b'%GPU%\\ge8300-status.exe\r\n%GPU%\\ge8300-vendor-copy-test.exe\r\necho GE8300_COPY_STATUS_RETURNED\r\n'))
  hive=extract(a.source_wim.resolve(),'/Windows/System32/config/SOFTWARE','hive');old={}
  for store in ('Root','TrustedPublisher'):
   base='\\Microsoft\\SystemCertificates\\'+store+'\\Certificates'
   for t in ('0982A8A74E3076EAC252F3EB244BAF77556D2229','15322192735D56804552FFB783B37AA9C547764E','477AA2039125221B85A5614DDA7BCC61A291B560'):
    old[store,t]=run(['hivexget',str(hive),base+'\\'+t,'Blob'],capture_output=True).stdout
    if not old[store,t]:raise ValueError('既存証明書がありません')
   listing=run(['hivexsh',str(hive)],input=f'cd {base}\nls\n',text=True,capture_output=True)
   if listing.stderr or thumb in listing.stdout:raise ValueError('証明書ストア不正')
   r=run(['hivexsh','-w',str(hive)],input=f'cd {base}\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:{blob.hex()}\ncommit\n',text=True,capture_output=True)
   if r.stderr or run(['hivexget',str(hive),base+'\\'+thumb,'Blob'],capture_output=True).stdout!=blob:raise ValueError('新証明書追加失敗')
   for (st,t),v in old.items():
    if st==store and run(['hivexget',str(hive),base+'\\'+t,'Blob'],capture_output=True).stdout!=v:raise ValueError('既存証明書変更')
  changes={'/Windows/System32/config/SOFTWARE':hive,'/Windows/System32/startnet.cmd':start}
  changes.update({'/a04br3/ge8300-copy/'+n:pkg/n for n in FILES})
  wim=out/'volume.wim';shutil.copyfile(a.source_wim,wim)
  run(['wimlib-imagex','update',str(wim),'1'],input='\n'.join(f'add {v} {k}' for k,v in changes.items())+'\n',text=True,env=env)
  run(['wimlib-imagex','verify',str(wim)],env=env)
  checks.update({k:sha(v) for k,v in changes.items()})
  for i,(k,v) in enumerate(checks.items()):
   if sha(extract(wim,k,'check-'+str(i)))!=v:raise ValueError('WIM照合失敗 '+k)
  esp=out/'volume-esp.img';run(['python3',str(Path(__file__).with_name('replace-esp-wim.py')),'--template',str(a.source_esp.resolve()),'--wim',str(wim),'--output',str(esp),'--template-bcd-sha256',BCD,'--template-wim-sha256',WIM],env=env)
  boot=out/'boot.img';shutil.copyfile(a.boot,boot)
  m={'service_driver_commit':a.driver_commit,'wdk_runner':'42132615250','build_flags':{'A04_GE8300_VENDOR_STAGE':1,'A04_GE8300_VENDOR_UNCACHED_STAGE':1,'A04_GE8300_VENDOR_IRQ':1,'A04_GE8300_VENDOR_START_DIAGNOSTIC':1,'A04_GE8300_VENDOR_RENDER_COLD':1,'A04_GE8300_VENDOR_COPY_DIAGNOSTIC':1,'A04_GE8300_VENDOR_COPY_SERVICE':1,'A04_GE8300_VENDOR_CLEAR_DIAGNOSTIC':0,'A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC':0},'firmware_sha256':'5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1','usc_sha256':'5c68917faa051238b23674b64c6b137ef7ff75425dbde9dbc5e1e5a36e02c4a3','service_package_sha256':a.package_sha256,'package_files_sha256':{n:sha(pkg/n) for n in sorted(FILES)},'certificate_sha1':thumb,'parent_commit':'5ee5528','listener_commit':'740117c','uefi_commit':'64c43d61','source_wim_sha256':WIM,'source_esp_sha256':ESP,'wim_sha256':sha(wim),'esp_sha256':sha(esp),'boot_sha256':sha(boot),'limitations':['実機未検証のWinPE診断のみ。GPU・署名・CPU cache可視性・同一起動は未実証。','CCBは折り返さず一起動あたり最大13回。timeout後はbounceと資源を隔離し再使用しない。','物理除去中のMMIO・DMA寿命は未証明。','C:解除はGPU DMA停止やreset安全性を示さない。','診断EXEは結果をコンソールへ出力する。自動UART画面送信は無効。','UART混線・欠落は失敗または不明とする。']}
  (out/'manifest.json').write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n');print(json.dumps(m,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
