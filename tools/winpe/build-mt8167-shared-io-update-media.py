#!/usr/bin/env python3
"""Recoveryで退避済みのhiveを根拠に、単一親の周辺機器packageを更新する媒体。"""
import argparse,hashlib,json,os,re,shutil,struct,subprocess
from pathlib import Path

def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def backup_check(manifest,root):
 b=json.loads(manifest.read_text());base=manifest.parent.resolve()
 if b.get('status')!='captured-unmounted' or not re.fullmatch(r'[0-9a-f-]{36}',b.get('recovery_boot_id','')):raise ValueError('完了したRecoveryバックアップが必要です')
 found={}
 for item in b['files']:
  p=(base/item['backup_path']).resolve(strict=True)
  if not p.is_relative_to(base) or not p.is_relative_to(root) or p.stat().st_size!=item['bytes'] or sha(p)!=item['sha256'].lower():raise ValueError('バックアップ内容不一致')
  name=item['source_path'].replace('\\','/')
  if name in found:raise ValueError('バックアップ項目の重複')
  found[name]=p
 for name in ('SYSTEM','SOFTWARE','DRIVERS'):
  stem='Windows/System32/config/'+name
  if stem not in found:raise ValueError('必須hive欠落')
  with found[stem].open('rb') as f:header=f.read(4096)
  if len(header)!=4096 or header[:4]!=b'regf':raise ValueError('hive形式不一致')
  seq=struct.unpack_from('<II',header,4)
  if seq[0]!=seq[1] and not any(stem+suffix in found and found[stem+suffix].stat().st_size>512 for suffix in ('.LOG1','.LOG2')):raise ValueError('dirty hiveのトランザクションログ欠落')
 return b

def main():
 ap=argparse.ArgumentParser()
 for n in ('workspace','base','package','firmware','backup','out','tools'):ap.add_argument('--'+n,type=Path,required=True)
 ap.add_argument('--probe-service',type=Path);ap.add_argument('--source-commit',required=True);a=ap.parse_args();root=a.workspace.resolve(strict=True)
 if not re.fullmatch('[0-9a-f]{40}',a.source_commit):raise SystemExit('完全な固定source commitが必要です')
 base,pkg,fw,backup,out,tools=[getattr(a,n).resolve() for n in ('base','package','firmware','backup','out','tools')]
 probe=a.probe_service.resolve() if a.probe_service else None
 if out.exists() or any(not p.is_relative_to(root) for p in (base,pkg,fw,backup,out,tools)) or (probe and not probe.is_relative_to(root)):raise SystemExit('作業ルート内の入力と新規出力先が必要です')
 if sha(base/'maintenance-staged.wim')!='0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4' or sha(base/'maintenance-staged-esp.img')!='22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce':raise SystemExit('base不一致')
 b=backup_check(backup,root)
 stems={'parent':('shared-parent','mt8167-shared'),'audio':('mt8167-afe-first-playback','mt8167-audio'),'usb':('mt8167-usb20','mt8167-usb20'),'wifi':('mt6630-runtime','mt6630-runtime')}
 declared=json.loads((pkg/'signed-manifest.json').read_text(encoding='utf-8-sig'))
 expected={x['path'].replace('\\','/'):x['sha256'].lower() for x in declared['files']};files={}
 for kind,(inf,sys) in stems.items():
  for name in (inf+'.inf',sys+'.sys',inf+'.cat'):
   rel=kind+'/'+name;digest=sha(pkg/rel)
   if expected.get(rel)!=digest:raise SystemExit('署名package内容不一致: '+rel)
   files[rel]=digest
  text=(pkg/kind/(inf+'.inf')).read_text()
  if '[Install.NT]' not in text or not re.search(r'DriverVer\s*=11/06/2026,2026\.11\.6\.1001',text):raise SystemExit('統合INF契約不一致')
 if len(list(pkg.rglob('mt8167-shared.sys')))!=1:raise SystemExit('親SYSは一つだけ必要です')
 firmware={'mt6630-wmt.bin':'4af7996b89ef110d8f270877f61aaf0b90fde21b6c5b0ed23ccbd32c2989dcee','mt6630-wlan.bin':'07fe1a225e6b9de6b310894db3b40bcc1cb66107b913e4c43eb6a62089aada22'}
 for name,digest in firmware.items():
  if sha(fw/name)!=digest:raise SystemExit('firmware不一致')
  files['firmware/'+name]=digest
 cert=(pkg/'mt8167-development.cer').read_bytes()
 if expected.get('mt8167-development.cer')!=hashlib.sha256(cert).hexdigest():raise SystemExit('証明書不一致')
 for name in ('mt6630-wlan-probe.exe','mt8167-audio-first-playback.exe'):
  rel='clients/'+name;digest=sha(pkg/rel)
  if expected.get(rel)!=digest:raise SystemExit('client不一致')
  files[rel]=digest
 if probe:files['MT8167HardwareProbe.exe']=sha(probe)
 here=Path(__file__).resolve().parent;out.mkdir();stage=out/'stage';stage.mkdir()
 token=hashlib.sha256((json.dumps(files,sort_keys=True)+sha(backup)+sha(here/'run-mt8167-shared-io-update.cmd.in')).encode()).hexdigest()[:16].upper()
 thumb=hashlib.sha1(cert).hexdigest().upper();blob=struct.pack('<III',3,1,20)+hashlib.sha1(cert).digest()+struct.pack('<III',32,1,len(cert))+cert
 trust=[]
 for store in ('Root','TrustedPublisher'):
  key='HKLM\\Mt8167IoOffline\\Microsoft\\SystemCertificates\\'+store+'\\Certificates\\'+thumb
  trust+=['reg.exe add "'+key+'" /v Blob /t REG_BINARY /d '+blob.hex()+' /f >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto trustfail']
 cmd=(here/'run-mt8167-shared-io-update.cmd.in').read_text().replace('@TOKEN@',token).replace('@JOURNAL@',token.lower()).replace('@HOST_BACKUP_SHA@',sha(backup)).replace('@TRUST_COMMANDS@','\n'.join(trust))
 if probe:cmd=cmd.replace('@PROBE_PREPARE@',(here/'prepare-mt8167-hardware-probe.cmd.in').read_text())
 else:cmd=cmd.replace('@PROBE_PREPARE@','rem Hardware probe service not included; PnP only.')
 if re.search('@[A-Z_]+@',cmd):raise SystemExit('未置換変数')
 (stage/'run-update-once.cmd').write_bytes(cmd.replace('\r','').replace('\n','\r\n').encode())
 (stage/'startnet.cmd').write_bytes(b'@echo off\r\nmountvol /N\r\nwpeinit\r\nif errorlevel 1 exit /b 2\r\nmountvol /N\r\ncall X:\\a04br3\\run-update-once.cmd\r\n')
 image=out/'update.wim';shutil.copyfile(base/'maintenance-staged.wim',image)
 env=os.environ.copy();env.update(TMPDIR=str(stage),LD_LIBRARY_PATH=str(tools/'wimtools'),PYTHONPATH=str(tools/'python-fat'),PYTHONDONTWRITEBYTECODE='1')
 pairs=[(stage/'startnet.cmd','/Windows/System32/startnet.cmd'),(stage/'run-update-once.cmd','/a04br3/run-update-once.cmd'),(pkg,'/a04br3/shared-io'),(fw,'/a04br3/shared-fw')]
 if probe:pairs.append((probe,'/a04br3/MT8167HardwareProbe.exe'))
 commands=''.join('add "'+str(src)+'" '+dst+'\n' for src,dst in pairs)
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'update',str(image),'1'],input=commands,text=True,env=env,check=True)
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'verify',str(image)],env=env,check=True)
 subprocess.run(['python3',str(here/'replace-esp-wim.py'),'--template',str(base/'maintenance-staged-esp.img'),'--wim',str(image),'--output',str(out/'update-esp.img'),'--template-bcd-sha256','65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d','--template-wim-sha256','0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'],env=env,check=True)
 (out/'manifest.json').write_text(json.dumps({'source_commit':a.source_commit,'token':token,'files':files,'host_backup_manifest_sha256':sha(backup),'recovery_boot_id':b['recovery_boot_id'],'builder_sha256':sha(Path(__file__).resolve()),'command_sha256':sha(stage/'run-update-once.cmd'),'esp_sha256':sha(out/'update-esp.img'),'status':'generated; hardware operation unverified'},indent=2)+'\n')
if __name__=='__main__':main()
