#!/usr/bin/env python3
"""固定GPU診断媒体のlistenerだけを署名済みREADY反復版へ交換する。"""
import argparse, hashlib, json, os, shutil, struct, subprocess, tempfile, zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
WIM='9434bd412368ca912fd8b3c2e9ef76a18bc7868bce6be09a5ffc6064ac8ececd'
ESP='041342399fa42cbcc19041e235f7b346a0c3bbaedc8c40fde8420d0d2e73917d'
BOOT='7121ca8b3e8bcf4c4487ed611eaffa17429418436689eda21ad03af6e34dceb6'
ZIP='b8ff87573ef02e0c39767aba062dcd3e00ae8613445999b259576bf472d00571'
EXE='3621ae28798c7ceeed3175fe88794a670524ca8383899681de4689751fe9244d'
BCD='65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d'
START='4afb2ebd2d57cca36add257fd82bc90677dca277d9d33e30b85a4d49b72664c3'
SYSTEM='cc151bf339dd2683b313e95d42179cfe571f6392c7a294b9a98f19a8a83e5214'
GPU='3d1791131becc3db447bbe9667ed97571d58dd06d45a76e6c17a99584211ae11'
THUMB='0982A8A74E3076EAC252F3EB244BAF77556D2229'
FILES={'guard-windows-volume.exe','mt8167-toprgu.cat','mt8167-toprgu.cer','mt8167-toprgu.inf','mt8167-toprgu.sys','mt8167-volume-listener.exe','release-toprgu-audit-volume.cmd'}
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def run(*a,**kw):return subprocess.run(*a,check=True,**kw)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('source-wim','source-esp','boot','package','output'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args(); out=a.output.resolve()
 if not out.is_dir() or not out.is_relative_to(ROOT) or any(out.iterdir()):raise SystemExit('出力先は作業ルート内の空ディレクトリに限る')
 for path,want in ((a.source_wim,WIM),(a.source_esp,ESP),(a.boot,BOOT),(a.package,ZIP)):
  if not path.resolve().is_relative_to(ROOT) or sha(path)!=want:raise SystemExit('入力実体またはSHA不一致: '+str(path))
 if any(c.isspace() or c in "\"'" for c in str(out)):raise SystemExit('出力パスに空白または引用符があります')
 env=os.environ.copy();env.update(TMPDIR=str(out),PYTHONDONTWRITEBYTECODE='1')
 with tempfile.TemporaryDirectory(prefix='listener-',dir=out) as d:
  tmp=Path(d)
  with zipfile.ZipFile(a.package) as z:
   entries=[x for x in z.infolist() if not x.is_dir()]
   if len(entries)!=len(FILES) or {x.filename for x in entries}!=FILES:raise ValueError('署名パッケージの構成不一致')
   cert=z.read('mt8167-toprgu.cer');exe=tmp/'mt8167-volume-listener.exe';exe.write_bytes(z.read(exe.name))
  if hashlib.sha1(cert).hexdigest().upper()!=THUMB or sha(exe)!=EXE:raise ValueError('listenerまたは署名証明書不一致')
  certfile=tmp/'signer.cer';certfile.write_bytes(cert)
  run(['openssl','x509','-inform','DER','-in',str(certfile),'-noout'])
  targets={'/Windows/System32/config/SOFTWARE':None,'/Windows/System32/config/SYSTEM':SYSTEM,'/Windows/System32/startnet.cmd':START,'/a04br3/ge8300-copy/mt8167-ge8300.sys':GPU}
  def extract(wim,target,tag):
   check=tmp/tag;check.mkdir();run(['wimlib-imagex','extract',str(wim),'1',target,f'--dest-dir={check}'],env=env,stdout=subprocess.DEVNULL)
   found=[x for x in check.rglob('*') if x.is_file()]
   if len(found)!=1:raise ValueError('抽出失敗: '+target)
   return found[0]
  original={}
  for i,(target,want) in enumerate(targets.items()):
   f=extract(a.source_wim.resolve(),target,f'base-{i}'); original[target]=sha(f)
   if want and original[target]!=want:raise ValueError('ベース内容不一致: '+target)
   if target.endswith('/SOFTWARE'):
    blob=struct.pack('<III',32,1,len(cert))+cert
    for store in ('Root','TrustedPublisher'):
     key='\\Microsoft\\SystemCertificates\\'+store+'\\Certificates\\'+THUMB
     if run(['hivexget',str(f),key,'Blob'],capture_output=True).stdout!=blob:raise ValueError('署名証明書が登録されていません')
  wim=out/'volume.wim';shutil.copyfile(a.source_wim,wim)
  run(['wimlib-imagex','update',str(wim),'1'],input=f'add {exe} /a04br3/mt8167-volume-listener.exe\n',text=True,env=env)
  run(['wimlib-imagex','verify',str(wim)],env=env)
  checks={**original,'/a04br3/mt8167-volume-listener.exe':EXE}
  for i,(target,want) in enumerate(checks.items()):
   if sha(extract(wim,target,f'new-{i}'))!=want:raise ValueError('更新後の内容不一致: '+target)
  esp=out/'volume-esp.img';run(['python3',str(Path(__file__).with_name('replace-esp-wim.py')),'--template',str(a.source_esp.resolve()),'--wim',str(wim),'--output',str(esp),'--template-bcd-sha256',BCD,'--template-wim-sha256',WIM],env=env)
  boot=out/'boot.img';shutil.copyfile(a.boot,boot)
  if sha(boot)!=BOOT:raise ValueError('boot.img不一致')
  meta={'listener_commit':'740117ca372cc8b9c1f4368a0fe39a678e097a06','listener_commit_publication':'Git bundleからビルド。当初commit_refs拒否、2026-11-06に公開を確認。','listener_package_sha256':ZIP,'listener_sha256':EXE,'listener_signer_sha1':THUMB,'gpu_driver_commit':'f2a2b5c','gpu_vendor_copy_diagnostic':1,'gpu_signed_package_sha256':'d5cd7969885614ea3eb62384905bc4fa1b90649c59e201b32a5ab5756a4b2244','gpu_firmware_source_sha256':'5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1','gpu_usc_source_sha256':'5c68917faa051238b23674b64c6b137ef7ff75425dbde9dbc5e1e5a36e02c4a3','gpu_usc_rea_evidence':'ev_910dc555760f50405b2c4dd9760a8681757e5dfaf091954580c73f0292857078','listener_build_runner':'42106309260','uefi_commit':'ea8b7dce315f36946d438aa25bf8af448ac3cce5','source_wim_sha256':WIM,'source_esp_sha256':ESP,'source_boot_sha256':BOOT,'system_hive_sha256':SYSTEM,'startnet_sha256':START,'gpu_sys_sha256':GPU,'wim_sha256':sha(wim),'esp_sha256':sha(esp),'boot_sha256':sha(boot),'limitations':['署名受理、同一起動、listenerとGPUコピーは実機未確認。','GPUの直接UART出力はlistener応答に混在し得る。欠落・混線したREADYや解除応答は失敗または不明として扱う。','C:解除はGPU DMA停止を意味せず、解除後resetのDMA安全性は未証明。','thermal観測は所有解放を意味せず、このWinPEでは未実機確認。','GPUのDMA停止、cache可視性、surprise removal時の資源寿命は未証明。','C:解除の完全な確認前に実機書込みしない。']}
  (out/'manifest.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2)+'\n');print(json.dumps(meta,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
