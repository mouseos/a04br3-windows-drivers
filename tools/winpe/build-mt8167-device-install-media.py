#!/usr/bin/env python3
"""標準PnP APIを一度だけ実行する通常Windowsサービスの準備媒体。"""
import argparse,hashlib,json,os,re,shutil,struct,subprocess
from pathlib import Path

def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 ap=argparse.ArgumentParser()
 for n in ('workspace','base','display','installer','out','tools'):ap.add_argument('--'+n,type=Path,required=True)
 ap.add_argument('--source-commit',required=True);a=ap.parse_args();root=a.workspace.resolve(strict=True)
 if not re.fullmatch('[0-9a-f]{40}',a.source_commit):raise SystemExit('固定commitが必要です')
 base,display,installer,out,tools=[getattr(a,n).resolve() for n in ('base','display','installer','out','tools')]
 if out.exists() or any(not p.is_relative_to(root) for p in (base,display,installer,out,tools)):raise SystemExit('作業ルート内の入力と新規出力先が必要です')
 if sha(base/'maintenance-staged.wim')!='0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4' or sha(base/'maintenance-staged-esp.img')!='22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce':raise SystemExit('検証済みbaseと不一致')
 names=('mt8167-display.inf','mt8167-display.sys','mt8167-display.cat','mt8167-development.cer')
 hashes={n:sha(display/n) for n in names};hashes['MT8167DeviceInstall.exe']=sha(installer)
 inf=(display/names[0]).read_text()
 if '[Install.NTarm]' not in inf or '[Install.NTarm.Services]' not in inf:raise SystemExit('ARMインストール節が必要です')
 out.mkdir();stage=out/'stage';stage.mkdir();token=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:16].upper()
 cert=(display/names[3]).read_bytes();thumb=hashlib.sha1(cert).hexdigest().upper();blob=struct.pack('<III',3,1,20)+hashlib.sha1(cert).digest()+struct.pack('<III',32,1,len(cert))+cert
 trust=[]
 for store in ('Root','TrustedPublisher'):
  k='HKLM\\Mt8167GpuOffline\\Microsoft\\SystemCertificates\\'+store+'\\Certificates\\'+thumb
  trust+=['reg.exe add "'+k+'" /v Blob /t REG_BINARY /d '+blob.hex()+' /f >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto trustfail']
 here=Path(__file__).resolve().parent
 cmd=(here/'run-mt8167-device-install-prepare.cmd.in').read_text().replace('@JOURNAL@',token.lower()).replace('@TOKEN@',token).replace('@TRUST_COMMANDS@','\n'.join(trust))
 if re.search('@[A-Z_]+@',cmd):raise SystemExit('未置換変数')
 (stage/'run-update-once.cmd').write_bytes(cmd.replace('\r','').replace('\n','\r\n').encode())
 (stage/'startnet.cmd').write_bytes(b'@echo off\r\nmountvol /N\r\nwpeinit\r\nif errorlevel 1 exit /b 2\r\nmountvol /N\r\ncall X:\\a04br3\\run-update-once.cmd\r\n')
 image=out/'update.wim';shutil.copyfile(base/'maintenance-staged.wim',image)
 env=os.environ.copy();env.update(TMPDIR=str(stage),LD_LIBRARY_PATH=str(tools/'wimtools'),PYTHONPATH=str(tools/'python-fat'),PYTHONDONTWRITEBYTECODE='1')
 pairs=((stage/'startnet.cmd','/Windows/System32/startnet.cmd'),(stage/'run-update-once.cmd','/a04br3/run-update-once.cmd'),(display,'/a04br3/upd/display'),(installer,'/a04br3/upd/MT8167DeviceInstall.exe'))
 commands=''.join('add "'+str(src)+'" '+dst+'\n' for src,dst in pairs)
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'update',str(image),'1'],input=commands,text=True,env=env,check=True)
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'verify',str(image)],env=env,check=True)
 subprocess.run(['python3',str(here/'replace-esp-wim.py'),'--template',str(base/'maintenance-staged-esp.img'),'--wim',str(image),'--output',str(out/'update-esp.img'),'--template-bcd-sha256','65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d','--template-wim-sha256','0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'],env=env,check=True)
 (out/'manifest.json').write_text(json.dumps({'source_commit':a.source_commit,'token':token,'files':hashes,'builder_sha256':sha(Path(__file__).resolve()),'command_sha256':sha(stage/'run-update-once.cmd'),'esp_sha256':sha(out/'update-esp.img'),'status':'generated; service execution and PnP bind unverified'},indent=2)+'\n')
if __name__=='__main__':main()
