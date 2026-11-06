#!/usr/bin/env python3
"""Recoveryで退避済みのhiveを根拠に、単一親の周辺機器packageを更新する媒体。"""
import argparse,hashlib,json,os,re,shutil,struct,subprocess
from pathlib import Path
from private_hardware_probe_input import prepared_system_check, probe_check, private_path, certificate_trust_commands
from engine_io_package import engine_package_check, engine_optional_check, ENGINE_CERT

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
 ap.add_argument('--audio-mode',choices=('Tone','EnginePcm'),default='Tone')
 ap.add_argument('--brightness-package',type=Path)
 ap.add_argument('--toprgu-package',type=Path)
 ap.add_argument('--engine-package',type=Path)
 ap.add_argument('--wifi-package',type=Path)
 ap.add_argument('--engine-client-package',type=Path)
 ap.add_argument('--probe-service',type=Path)
 for n in ('probe-manifest','probe-certificate','prepared-system','prepared-system-manifest'):
  ap.add_argument('--'+n,type=Path)
 ap.add_argument('--prepared-system-sha256')
 ap.add_argument('--private-media',action='store_true')
 ap.add_argument('--source-commit',required=True);a=ap.parse_args();root=a.workspace.resolve(strict=True)
 if not re.fullmatch('[0-9a-f]{40}',a.source_commit):raise SystemExit('完全な固定source commitが必要です')
 base,pkg,fw,backup,out,tools=[getattr(a,n).resolve() for n in ('base','package','firmware','backup','out','tools')]
 probe=a.probe_service.resolve() if a.probe_service else None
 engine=a.audio_mode=='EnginePcm'
 service_name='MT8167EngineProbe' if engine else 'MT8167HardwareProbe'
 audio_inf='mt8167-afe-engine-pcm.inf' if engine else 'mt8167-afe-first-playback.inf'
 audio_client='mt8167-audio-engine-probe.exe' if engine else 'mt8167-audio-first-playback.exe'
 if engine and (not probe or not a.engine_package or not a.engine_client_package):raise SystemExit('EnginePcmは専用serviceとEngine driver/client packageを明示指定してください')
 if not engine and (a.engine_package or a.engine_client_package or a.brightness_package or a.toprgu_package or a.wifi_package):raise SystemExit('Engine専用入力をTone modeへ渡せません')
 private_args=(a.probe_manifest,a.probe_certificate,a.prepared_system,a.prepared_system_manifest,a.prepared_system_sha256)
 if probe:
  if not a.private_media or not all(private_args):raise SystemExit('probeにはprivate媒体とnative SYSTEM/probe manifest/cert/hashの全入力が必要です')
  private_path(out,root)
 elif any(private_args):raise SystemExit('native SYSTEM入力はHardwareProbe private媒体だけに指定できます')
 if out.exists() or any(not p.is_relative_to(root) for p in (base,pkg,fw,backup,out,tools)) or (probe and not probe.is_relative_to(root)):raise SystemExit('作業ルート内の入力と新規出力先が必要です')
 if sha(base/'maintenance-staged.wim')!='0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4' or sha(base/'maintenance-staged-esp.img')!='22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce':raise SystemExit('base不一致')
 b=backup_check(backup,root)
 native=None;probe_info=None
 if probe:
  native=prepared_system_check(a.prepared_system,a.prepared_system_manifest,a.prepared_system_sha256,root,b,service_name)
  probe_info=probe_check(probe,a.probe_manifest,a.probe_certificate,root,a.audio_mode)
 components={};optional_drivers=[]
 if engine:
  sources,components,certificates=engine_package_check(pkg,a.engine_package,a.engine_client_package,root,a.wifi_package)
  extra_files,extra_components,extra_certificates,optional_drivers=engine_optional_check(a.brightness_package,a.toprgu_package,root)
  sources.update(extra_files);components.update(extra_components);certificates.update(extra_certificates)
  files={rel:sha(path) for rel,path in sources.items()}
 else:
  stems={'parent':('shared-parent','mt8167-shared'),'audio':('mt8167-afe-first-playback','mt8167-audio'),'usb':('mt8167-usb20','mt8167-usb20'),'wifi':('mt6630-runtime','mt6630-runtime')}
  declared=json.loads((pkg/'signed-manifest.json').read_text(encoding='utf-8-sig'))
  if declared.get('engine_pcm') or declared.get('audio_mode') not in (None,'private_fixed_tone_diagnostic'):raise SystemExit('旧tone probe用媒体へEnginePcmを混在できません')
  expected={x['path'].replace('\\','/'):x['sha256'].lower() for x in declared['files']};files={}
  for kind,(inf,sys) in stems.items():
   for name in (inf+'.inf',sys+'.sys',inf+'.cat'):
    rel=kind+'/'+name;digest=sha(pkg/rel)
    if expected.get(rel)!=digest:raise SystemExit('署名package内容不一致: '+rel)
    files[rel]=digest
   text=(pkg/kind/(inf+'.inf')).read_text()
   if '[Install.NT]' not in text or not re.search(r'DriverVer\s*=11/06/2026,2026\.11\.6\.1001',text):raise SystemExit('統合INF契約不一致')
  if len(list(pkg.rglob('mt8167-shared.sys')))!=1:raise SystemExit('親SYSは一つだけ必要です')
  cert=(pkg/'mt8167-development.cer').read_bytes()
  if expected.get('mt8167-development.cer')!=hashlib.sha256(cert).hexdigest():raise SystemExit('証明書不一致')
  for name in ('mt6630-wlan-probe.exe','mt8167-audio-first-playback.exe'):
   rel='clients/'+name;digest=sha(pkg/rel)
   if expected.get(rel)!=digest:raise SystemExit('client不一致')
   files[rel]=digest
  files['mt8167-development.cer']=sha(pkg/'mt8167-development.cer')
  sources={rel:pkg/rel for rel in files}
  certificates={'package_clients':cert}
  for kind,(inf,binary) in stems.items():
   names=[kind+'/'+name for name in (inf+'.inf',binary+'.sys',inf+'.cat')]
   components[kind]={'binary_source_commit':declared.get('driver_source_commit',declared.get('source_commit')),
    'inf_source_commit':declared.get('source_commit'),'driver_ver':'11/06/2026,2026.11.6.1001',
    'certificate_thumbprint':hashlib.sha1(cert).hexdigest().upper(),'files':{name:files[name] for name in names}}
 firmware={'mt6630-wmt.bin':'4af7996b89ef110d8f270877f61aaf0b90fde21b6c5b0ed23ccbd32c2989dcee','mt6630-wlan.bin':'07fe1a225e6b9de6b310894db3b40bcc1cb66107b913e4c43eb6a62089aada22'}
 for name,digest in firmware.items():
  if sha(fw/name)!=digest:raise SystemExit('firmware不一致')
  files['firmware/'+name]=digest
 if probe:
  files[service_name+'.exe']=probe_info['exe_sha256']
  if engine and probe_info['certificate_thumbprint']!=ENGINE_CERT:raise SystemExit('Engine service署名certの固定thumbprint不一致')
  files['hardware-probe-development.cer']=probe_info['certificate_sha256']
  files['private-system/SYSTEM.patched']=native['sha256']
 prepare_name='prepare-mt8167-engine-probe.cmd.in' if engine else 'prepare-mt8167-hardware-probe.cmd.in'
 here=Path(__file__).resolve().parent;out.mkdir(mode=0o700 if a.private_media else 0o755);stage=out/'stage';stage.mkdir(mode=0o700 if a.private_media else 0o755)
 # Stage only the validated package/firmware files; never import arbitrary siblings.
 staged_pkg=stage/'shared-io';staged_fw=stage/'shared-fw'
 staged_pkg.mkdir();staged_fw.mkdir()
 for rel,path in sources.items():
  dest=staged_pkg/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,dest)
  if sha(dest)!=files[rel]:raise SystemExit('staging中のcomponent変更: '+rel)
 for name in firmware:
  shutil.copyfile(fw/name,staged_fw/name)
  if sha(staged_fw/name)!=files['firmware/'+name]:raise SystemExit('staging中のfirmware変更')
 if len(list(staged_pkg.rglob('mt8167-shared.sys')))!=1:raise SystemExit('選択後の親SYSは厳密に一つだけです')
 if engine and any('first-playback' in path.name for path in staged_pkg.rglob('*')):raise SystemExit('Engineへ旧tone client/INFを含められません')
 if probe:
  private_system=stage/'SYSTEM.patched';shutil.copyfile(a.prepared_system.resolve(strict=True),private_system)
  if sha(private_system)!=native['sha256']:raise SystemExit('staging中のprivate SYSTEM変更を検出しました')
  staged_probe=stage/(service_name+'.exe');staged_probe_cert=stage/'hardware-probe-development.cer'
  shutil.copyfile(probe,staged_probe);shutil.copyfile(a.probe_certificate.resolve(strict=True),staged_probe_cert)
  if sha(staged_probe)!=probe_info['exe_sha256'] or sha(staged_probe_cert)!=probe_info['certificate_sha256']:raise SystemExit('staging中のprobe/cert変更')
 token=hashlib.sha256((json.dumps(files,sort_keys=True)+sha(backup)+sha(here/'run-mt8167-shared-io-update.cmd.in')+(sha(here/prepare_name)+native['manifest_sha256']+probe_info['manifest_sha256'] if probe else '')).encode()).hexdigest()[:16].upper()
 if probe:certificates['hardware_probe']=staged_probe_cert.read_bytes()
 trust=certificate_trust_commands(certificates)
 cmd=(here/'run-mt8167-shared-io-update.cmd.in').read_text().replace('@TOKEN@',token).replace('@JOURNAL@',token.lower()).replace('@HOST_BACKUP_SHA@',sha(backup)).replace('@TRUST_COMMANDS@','\n'.join(trust)).replace('@AUDIO_INF@',audio_inf).replace('@OPTIONAL_DRIVERS@','\n'.join(optional_drivers))
 if probe:
  prepare=(here/prepare_name).read_text().replace('@SYSTEM_SHA@',native['sha256']).replace('@SYSTEM_MANIFEST_SHA@',native['manifest_sha256'])
  cmd=cmd.replace('@PROBE_PREPARE@',prepare)
 else:cmd=cmd.replace('@PROBE_PREPARE@','rem Hardware probe service not included; PnP only.')
 if re.search('@[A-Z_]+@',cmd):raise SystemExit('未置換変数')
 if re.search(r'(?im)^reg(?:\.exe)?\s+(?:load|unload)\s+.*(?:config\\SYSTEM|Mt8167InstallSystem)',cmd):raise SystemExit('WinPE SYSTEM RegLoadは禁止です')
 (stage/'run-update-once.cmd').write_bytes(cmd.replace('\r','').replace('\n','\r\n').encode())
 (stage/'startnet.cmd').write_bytes(b'@echo off\r\nmountvol /N\r\nwpeinit\r\nif errorlevel 1 exit /b 2\r\nmountvol /N\r\ncall X:\\a04br3\\run-update-once.cmd\r\n')
 image=out/'update.wim';shutil.copyfile(base/'maintenance-staged.wim',image)
 env=os.environ.copy();env.update(TMPDIR=str(stage),LD_LIBRARY_PATH=str(tools/'wimtools'),PYTHONPATH=str(tools/'python-fat'),PYTHONDONTWRITEBYTECODE='1')
 pairs=[(stage/'startnet.cmd','/Windows/System32/startnet.cmd'),(stage/'run-update-once.cmd','/a04br3/run-update-once.cmd'),(staged_pkg,'/a04br3/shared-io'),(staged_fw,'/a04br3/shared-fw')]
 if probe:pairs.extend(((staged_probe,'/a04br3/'+service_name+'.exe'),(staged_probe_cert,'/a04br3/hardware-probe-development.cer'),(private_system,'/a04br3/private-system/SYSTEM.patched')))
 commands=''.join('add "'+str(src)+'" '+dst+'\n' for src,dst in pairs)
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'update',str(image),'1'],input=commands,text=True,env=env,check=True)
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'verify',str(image)],env=env,check=True)
 subprocess.run(['python3',str(here/'replace-esp-wim.py'),'--template',str(base/'maintenance-staged-esp.img'),'--wim',str(image),'--output',str(out/'update-esp.img'),'--template-bcd-sha256','65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d','--template-wim-sha256','0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'],env=env,check=True)
 (out/'manifest.json').write_text(json.dumps({'private_do_not_distribute':bool(a.private_media),'audio_mode':'engine_pcm_retained' if engine else 'private_fixed_tone_diagnostic','selected_service':service_name,'shared_parent_count':1,'components':components,'fixed_clients':['mt6630-wlan-probe.exe',audio_client],'prepared_system':native,'probe':probe_info,'trusted_certificates':{role:hashlib.sha1(der).hexdigest().upper() for role,der in certificates.items()},'source_commit':a.source_commit,'token':token,'files':files,'host_backup_manifest_sha256':sha(backup),'recovery_boot_id':b['recovery_boot_id'],'input_validator_sha256':sha(here/'private_hardware_probe_input.py'),'engine_selector_sha256':sha(here/'engine_io_package.py'),'builder_sha256':sha(Path(__file__).resolve()),'command_sha256':sha(stage/'run-update-once.cmd'),'esp_sha256':sha(out/'update-esp.img'),'signature_verification':'hash/source/certificate pins here; clients and Engine CAT WinVerifyTrust at runtime; INF/SYS catalog membership by Windows servicing/PnP','status':'generated; hardware operation unverified'},indent=2)+'\n')
 if a.private_media:
  for p in out.rglob('*'):p.chmod(0o700 if p.is_dir() else 0o600)
if __name__=='__main__':main()
