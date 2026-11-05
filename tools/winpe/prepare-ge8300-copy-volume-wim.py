#!/usr/bin/env python3
"""固定された解除対応媒体に署名済みGE8300診断を追加する。実機は操作しない。"""
import argparse, hashlib, json, os, shutil, struct, subprocess, tempfile, zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
WIM='f5a647a143ce3f4568f49087ca03d4b7a21b47bd323b6c1ed7a6b4354d7847c6'
ESP='ba23a17b23bd7c7f42491788ff06004475bf0dabc427ebf4614f4d177b9c6bdc'
BOOT='2cc9dfbff72b37bbb9af530c92575dce9971346de7b2a7fbe1af504280eec049'
PKG='d5cd7969885614ea3eb62384905bc4fa1b90649c59e201b32a5ab5756a4b2244'
BCD='65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d'
START='ad5b59fd143f247518426b0d4c84e0d699f268dc9afbee3416513828e6b320a4'
FILES={'ge8300-mips.fw','ge8300-pnp-status.exe','ge8300-status.exe','ge8300-test.cer','ge8300-vendor-stage-status.exe','ge8300-vendor-start-status.exe','mt8167-ge8300.cat','mt8167-ge8300.inf','mt8167-ge8300.sys'}
OLD=('0982A8A74E3076EAC252F3EB244BAF77556D2229','B80231026FFEFE30224048CA83EE7631EFF3FABC')
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def run(c,**kw):return subprocess.run(c,check=True,**kw)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for x in ('source-wim','source-esp','boot','package','output'):p.add_argument('--'+x,type=Path,required=True)
 a=p.parse_args(); out=a.output.resolve()
 if not out.is_relative_to(ROOT) or not out.is_dir():raise SystemExit('出力先が作業ルート内のディレクトリではありません')
 for name in ('volume.wim','volume-esp.img','volume-esp.img.json','boot.img','manifest.json'):
  if (out/name).exists():raise SystemExit('出力が既に存在します: '+name)
 for path,want in ((a.source_wim,WIM),(a.source_esp,ESP),(a.boot,BOOT),(a.package,PKG)):
  if not path.resolve().is_relative_to(ROOT) or sha(path)!=want:raise SystemExit('入力の実体またはハッシュが不一致: '+str(path))
 if any(c.isspace() or c in "\"'" for c in str(out)):raise SystemExit('出力パスに空白または引用符があります')
 env=os.environ.copy();env.update(TMPDIR=str(out),PYTHONDONTWRITEBYTECODE='1')
 with tempfile.TemporaryDirectory(prefix='copy-volume-',dir=out) as d:
  tmp=Path(d);pkg=tmp/'package';pkg.mkdir()
  with zipfile.ZipFile(a.package) as z:
   members=[x for x in z.infolist() if not x.is_dir()]
   if len(members)!=len(FILES) or {x.filename for x in members}!=FILES:raise ValueError('パッケージの構成が不正')
   for x in members:(pkg/x.filename).write_bytes(z.read(x))
  cert=(pkg/'ge8300-test.cer').read_bytes();run(['openssl','x509','-inform','DER','-in',str(pkg/'ge8300-test.cer'),'-noout'])
  thumb=hashlib.sha1(cert).hexdigest().upper();blob=struct.pack('<III',32,1,len(cert))+cert
  run(['wimlib-imagex','extract',str(a.source_wim.resolve()),'1','/Windows/System32/config/SOFTWARE','/Windows/System32/config/SYSTEM','/Windows/System32/startnet.cmd',f'--dest-dir={tmp}'],env=env)
  hive=tmp/'SOFTWARE'; system=tmp/'SYSTEM'; start=tmp/'startnet.cmd'
  if sha(system)!='cc151bf339dd2683b313e95d42179cfe571f6392c7a294b9a98f19a8a83e5214' or sha(start)!=START:raise ValueError('元のSYSTEMまたはstartnetが不一致')
  old={};base='\\Microsoft\\SystemCertificates'
  for store in ('Root','TrustedPublisher'):
   path=base+'\\'+store+'\\Certificates'
   for t in OLD:
    old[store,t]=run(['hivexget',str(hive),path+'\\'+t,'Blob'],capture_output=True).stdout
    if not old[store,t]:raise ValueError('既存証明書がありません')
   listing=run(['hivexsh',str(hive)],input=f'cd {path}\nls\n',text=True,capture_output=True)
   if listing.stderr or thumb in listing.stdout:raise ValueError('証明書ストアが不正または追加済み')
   cmds=f'cd {path}\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:{blob.hex()}\ncommit\n'
   result=run(['hivexsh','-w',str(hive)],input=cmds,text=True,capture_output=True)
   if result.stderr:raise ValueError('証明書追加失敗: '+result.stderr)
   if run(['hivexget',str(hive),path+'\\'+thumb,'Blob'],capture_output=True).stdout!=blob:raise ValueError('新証明書不一致')
   for t in OLD:
    if run(['hivexget',str(hive),path+'\\'+t,'Blob'],capture_output=True).stdout!=old[store,t]:raise ValueError('既存証明書が変更された')
  marker=b' echo PnP and touch operation must be verified separately.\r\ngoto display\r\n'
  insertion=(b' echo PnP and touch operation must be verified separately.\r\n'
   b'set GPU=X:\\a04br3\\ge8300-copy\r\ncopy /Y %GPU%\\ge8300-mips.fw X:\\Windows\\System32\\drivers\\ge8300-mips.fw >nul\r\n'
   b'if errorlevel 1 goto gpu_failed\r\necho GE8300_COPY_DRVLOAD_START\r\ndrvload %GPU%\\mt8167-ge8300.inf\r\n'
   b'echo GE8300_COPY_DRVLOAD_EXIT=%errorlevel%\r\nif errorlevel 1 goto gpu_failed\r\n'
   b'%GPU%\\ge8300-pnp-status.exe\r\n%GPU%\\ge8300-status.exe\r\n%GPU%\\ge8300-vendor-stage-status.exe\r\n%GPU%\\ge8300-vendor-start-status.exe\r\n'
   b'echo GE8300_COPY_STATUS_RETURNED\r\ngoto display\r\n:gpu_failed\r\necho GE8300_COPY_LOAD_FAILED\r\ngoto display\r\n')
  data=start.read_bytes()
  if data.count(marker)!=1:raise ValueError('startnetの挿入位置が不一致')
  start.write_bytes(data.replace(marker,insertion))
  wim=out/'volume.wim';shutil.copyfile(a.source_wim,wim)
  targets={'/Windows/System32/config/SOFTWARE':hive,'/Windows/System32/config/SYSTEM':system,'/Windows/System32/startnet.cmd':start}
  targets.update({'/a04br3/ge8300-copy/'+n:pkg/n for n in FILES})
  ops=[f'add {v} {k}' for k,v in targets.items() if v!=system]
  run(['wimlib-imagex','update',str(wim),'1'],input='\n'.join(ops)+'\n',text=True,env=env)
  run(['wimlib-imagex','verify',str(wim)],env=env)
  for target,source in targets.items():
   check=tmp/('check-'+hashlib.sha256(target.encode()).hexdigest()[:12]);check.mkdir()
   run(['wimlib-imagex','extract',str(wim),'1',target,f'--dest-dir={check}'],env=env,stdout=subprocess.DEVNULL)
   found=[x for x in check.rglob('*') if x.is_file()]
   if len(found)!=1 or sha(found[0])!=sha(source):raise ValueError('WIM検証失敗: '+target)
  esp=out/'volume-esp.img'
  run(['python3',str(Path(__file__).with_name('replace-esp-wim.py')),'--template',str(a.source_esp.resolve()),'--wim',str(wim),'--output',str(esp),'--template-bcd-sha256',BCD,'--template-wim-sha256',WIM],env=env)
  boot=out/'boot.img';shutil.copyfile(a.boot,boot)
  if sha(boot)!=BOOT:raise ValueError('boot.imgが不一致')
  m={'driver_commit':'f2a2b5c','wdk_runner':'42106309260','build_flags':{'A04_GE8300_VENDOR_STAGE':1,'A04_GE8300_VENDOR_UNCACHED_STAGE':1,'A04_GE8300_VENDOR_IRQ':1,'A04_GE8300_VENDOR_START_DIAGNOSTIC':1,'A04_GE8300_VENDOR_RENDER_COLD':1,'A04_GE8300_VENDOR_COPY_DIAGNOSTIC':1,'A04_GE8300_VENDOR_CLEAR_DIAGNOSTIC':0,'A04_GE8300_VENDOR_SCREEN_DIAGNOSTIC':0},'signed_package_sha256':PKG,'package_files_sha256':{n:sha(pkg/n) for n in sorted(FILES)},'test_certificate_sha1':thumb,'source_wim_sha256':WIM,'source_esp_sha256':ESP,'boot_sha256':BOOT,'bcd_sha256':BCD,'system_hive_sha256':sha(system),'wim_sha256':sha(wim),'esp_sha256':sha(esp),'startnet_sha256':sha(start),'firmware_source_sha256':'5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1','usc_source_lib_sha256':'d32a4a224d06302d9bec0a66cbb7d17dec663d3985f1c8592f5bb77353ca1d4b','usc_sha256':'5c68917faa051238b23674b64c6b137ef7ff75425dbde9dbc5e1e5a36e02c4a3','usc_rea_evidence':'ev_910dc555760f50405b2c4dd9760a8681757e5dfaf091954580c73f0292857078','limitations':['元asset由来の実験用診断。コピー・署名受理・同一起動は実機未確認。','surprise removal時のMMIO、fabric DMA停止、cache可視性、WDF参照による資源寿命は未証明。','GPU drvload停止時は画面送信と解除listenerが起動しない。','C:解除の完全な確認より前に実機へ書き込まない。']}
  (out/'manifest.json').write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n');print(json.dumps(m,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
