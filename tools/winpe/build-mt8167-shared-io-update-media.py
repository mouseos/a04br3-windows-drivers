#!/usr/bin/env python3
"""Recoveryで退避済みのhiveを根拠に、単一親の周辺機器packageを更新する媒体。"""
import argparse,hashlib,json,os,re,shutil,struct,subprocess
from pathlib import Path, PureWindowsPath
from private_hardware_probe_input import prepared_system_check, probe_check, private_path, certificate_trust_commands
from engine_rearm import make_plan, check_plan, archive_commands
from engine_io_package import engine_package_check, engine_optional_check, parent_override_check, check_final_audio_client, ENGINE_CERT

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

def validate_staged_package_copies(command, staged_pkg):
 for name in re.findall(r'(?im)^copy[^\r\n]*?X:\\a04br3\\shared-io\\([^ \r\n]+)',command):
  p=(staged_pkg/Path(*PureWindowsPath(name).parts)).resolve()
  if not p.is_relative_to(staged_pkg.resolve()) or not p.is_file():
   raise ValueError('WinPE copy参照の署名package実体がありません: '+name)

def main():
 ap=argparse.ArgumentParser()
 for n in ('workspace','base','package','firmware','backup','out','tools'):ap.add_argument('--'+n,type=Path,required=True)
 ap.add_argument('--deferred-package-map',type=Path)
 ap.add_argument('--audio-mode',choices=('Tone','EnginePcm'),default='Tone')
 ap.add_argument('--brightness-package',type=Path)
 ap.add_argument('--brightness-manifest-sha256')
 ap.add_argument('--toprgu-package',type=Path)
 ap.add_argument('--engine-package',type=Path)
 ap.add_argument('--wifi-package',type=Path)
 ap.add_argument('--parent-package',type=Path)
 ap.add_argument('--parent-manifest-sha256')
 ap.add_argument('--allow-missing-engine-service-log',action='store_true')
 ap.add_argument('--allow-known-sd-protocol-repair',action='store_true')
 ap.add_argument('--rearm-previous-media',type=Path)
 ap.add_argument('--write-engine-rearm-plan',type=Path)
 ap.add_argument('--engine-rearm-plan',type=Path)
 ap.add_argument('--engine-rearm-plan-sha256')
 ap.add_argument('--engine-client-package',type=Path)
 ap.add_argument('--probe-service',type=Path)
 for n in ('probe-manifest','probe-certificate','prepared-system','prepared-system-manifest'):
  ap.add_argument('--'+n,type=Path)
 ap.add_argument('--prepared-system-sha256')
 ap.add_argument('--preupdate-wifi-checkpoint',type=Path)
 ap.add_argument('--preupdate-wifi-checkpoint-sha256')
 ap.add_argument('--private-media',action='store_true')
 ap.add_argument('--source-commit',required=True);a=ap.parse_args();root=a.workspace.resolve(strict=True)
 if not re.fullmatch('[0-9a-f]{40}',a.source_commit):raise SystemExit('完全な固定source commitが必要です')
 base,pkg,fw,backup,out,tools=[getattr(a,n).resolve() for n in ('base','package','firmware','backup','out','tools')]
 probe=a.probe_service.resolve() if a.probe_service else None
 engine=a.audio_mode=='EnginePcm'
 plan_only=bool(a.write_engine_rearm_plan)
 rearming=bool(a.engine_rearm_plan)
 if a.allow_known_sd_protocol_repair and not plan_only:raise SystemExit('known SD repair opt-inはplan生成専用です')
 if a.allow_missing_engine_service_log and not plan_only:raise SystemExit('missing log opt-inは明示再arm plan生成専用です')
 if (plan_only or rearming or a.rearm_previous_media) and not engine:raise SystemExit('再armはEngine mode専用です')
 if plan_only and (not a.rearm_previous_media or rearming):raise SystemExit('plan生成は旧媒体manifestを明示し既存planと併用しないでください')
 if a.rearm_previous_media and not (plan_only or rearming):raise SystemExit('旧媒体指定は明示再armだけです')
 if a.brightness_manifest_sha256 and not a.brightness_package:raise SystemExit('brightness manifest hashにはpackage指定も必要です')
 if bool(a.parent_package)!=bool(a.parent_manifest_sha256):raise SystemExit('親overrideにはpackageとmanifest hash両方が必要です')
 if bool(a.engine_rearm_plan)!=bool(a.engine_rearm_plan_sha256):raise SystemExit('再arm planとhash両方が必要です')
 service_name='MT8167EngineProbe' if engine else 'MT8167HardwareProbe'
 audio_inf='mt8167-afe-engine-pcm.inf' if engine else 'mt8167-afe-first-playback.inf'
 audio_client='mt8167-audio-engine-probe.exe' if engine else 'mt8167-audio-first-playback.exe'
 if engine and (not probe or not a.engine_package or not a.engine_client_package):raise SystemExit('EnginePcmは専用serviceとEngine driver/client packageを明示指定してください')
 if a.deferred_package_map and (a.parent_package or a.wifi_package or a.brightness_package):raise SystemExit('延期mapと個別parent/WiFi/brightness overrideは併用できません')
 if a.deferred_package_map and (not engine or not probe):raise SystemExit('延期更新は明示Engine service専用です')
 if not engine and (a.engine_package or a.engine_client_package or a.brightness_package or a.toprgu_package or a.wifi_package or a.parent_package):raise SystemExit('Engine専用入力をTone modeへ渡せません')
 private_args=(a.probe_manifest,a.probe_certificate,a.prepared_system,a.prepared_system_manifest,a.prepared_system_sha256)
 if probe:
  if not a.private_media or not all(private_args[:2]) or (not plan_only and not all(private_args)):raise SystemExit('probeにはprivate媒体とnative SYSTEM/probe manifest/cert/hashの全入力が必要です')
  private_path(out,root)
 elif any(private_args):raise SystemExit('native SYSTEM入力はHardwareProbe private媒体だけに指定できます')
 if out.exists() or any(not p.is_relative_to(root) for p in (base,pkg,fw,backup,out,tools)) or (probe and not probe.is_relative_to(root)):raise SystemExit('作業ルート内の入力と新規出力先が必要です')
 if sha(base/'maintenance-staged.wim')!='0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4' or sha(base/'maintenance-staged-esp.img')!='22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce':raise SystemExit('base不一致')
 b=backup_check(backup,root)
 native=None;probe_info=None
 if probe:
  if not plan_only:native=prepared_system_check(a.prepared_system,a.prepared_system_manifest,a.prepared_system_sha256,root,b,service_name)
  probe_info=probe_check(probe,a.probe_manifest,a.probe_certificate,root,a.audio_mode)
 components={};optional_drivers=[]
 codec_batch=False
 if a.deferred_package_map:
  codec_batch=json.loads(a.deferred_package_map.read_text(encoding='utf-8-sig')).get('transaction')=='codec1026-msdc-quiet-render313-v3'
 if engine:
  if codec_batch:
   from engine_deferred_policy import load_map
   sources,components,certificates,deferred_policy=load_map(a.deferred_package_map,root)
  else:
   sources,components,certificates=engine_package_check(pkg,a.engine_package,a.engine_client_package,root,a.wifi_package)
  if a.parent_package:
   parent_files,parent_component,parent_cert=parent_override_check(a.parent_package,a.parent_manifest_sha256,root,sources,components)
   sources.update(parent_files);components['parent']=parent_component;certificates['parent_override']=parent_cert
  if codec_batch and (a.brightness_package or a.toprgu_package):raise SystemExit('codec/eMMC保持profileへ他driver overrideは禁止です')
  extra_files,extra_components,extra_certificates,optional_drivers=engine_optional_check(a.brightness_package,a.toprgu_package,root,a.brightness_manifest_sha256)
  sources.update(extra_files);components.update(extra_components);certificates.update(extra_certificates)
  if a.deferred_package_map:
   from engine_deferred_policy import load_map
   deferred_sources,deferred_components,deferred_certificates,deferred_policy=load_map(a.deferred_package_map,root)
   if (deferred_policy['backup_manifest_sha256']!=sha(backup)
       or not probe_info or probe_info.get('deferred_update')!=deferred_policy):raise SystemExit('延期policy/署名service/freshbackup不一致')
   if 'gpu' in deferred_components:
    installed={x['source_path'].replace('\\','/').lower():x['sha256'].lower() for x in b['files']}
    if installed.get('windows/system32/drivers/mt8167-ge8300.sys')!=deferred_components['gpu']['baseline']['files']['gpu/mt8167-ge8300.sys']:raise SystemExit('freshbackupのGPU SYSが標準policy baselineと不一致')
   for name in (() if codec_batch else ('mt6630-wlan-probe.exe','mt8167-audio-engine-probe.exe')):
    if sha(deferred_sources['clients/'+name])!=sha(sources['clients/'+name]):raise SystemExit('延期policy clientは既存固定WLAN/d0eb署名clientと一致が必要です')
   # Replace only the exact five targets and fixed clients, preserving TOPRGU and firmware.
   for role,component in deferred_components.items():
    for rel in list(sources):
     if rel.startswith(role+'/'):del sources[rel]
    components[role]=component
   sources.update(deferred_sources);certificates.update({'deferred_'+k:v for k,v in deferred_certificates.items()})
   if 'sd' in deferred_policy['packages']:optional_drivers+=['call :adddriver SD X:\\a04br3\\shared-io\\sd\\mt8167-sd.inf','if errorlevel 1 goto dismfail']
   optional_drivers+=['call :adddriver DISPLAY X:\\a04br3\\shared-io\\display\\mt8167-display.inf','if errorlevel 1 goto dismfail']
  elif probe_info and probe_info.get('deferred_update'):raise SystemExit('延期serviceには一致する明示package mapが必要です')
  if not codec_batch:check_final_audio_client(sources,components)
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
 wifi_archive=None
 if bool(a.preupdate_wifi_checkpoint)!=bool(a.preupdate_wifi_checkpoint_sha256):raise SystemExit('旧WiFi記録とhash両方が必要です')
 if a.preupdate_wifi_checkpoint:
  if not rearming or not a.private_media:raise SystemExit('旧WiFi記録保存はprivate再arm媒体だけです')
  wifi_archive=private_path(a.preupdate_wifi_checkpoint,root).resolve(strict=True)
  if sha(wifi_archive)!=a.preupdate_wifi_checkpoint_sha256:raise SystemExit('旧WiFi記録hash不一致')
  from wifi_checkpoint_archive import validate_archive
  validate_archive(wifi_archive,backup,b)
 rearm_plan=None
 if plan_only or rearming:
  pnp=probe_info.get('pnp_rebind',{}) if probe_info else {}
  if not a.deferred_package_map and (pnp.get('parent_updated') is not False or pnp.get('parent_policy')!='verify-selected-version-and-installed-bytes-only' or pnp.get('problem_status')!='CM_Get_DevNode_PropertyW/DEVPKEY_Device_ProblemStatus/DEVPROP_TYPE_NTSTATUS'):raise SystemExit('再armには親選択/配置照合・ProblemStatus対応serviceが必要です')
 if plan_only:
  plan=make_plan(a.rearm_previous_media,backup,b,components,root,a.allow_missing_engine_service_log,probe_info,a.allow_known_sd_protocol_repair)
  destination=private_path(a.write_engine_rearm_plan,root)
  with destination.open('x') as stream:
   os.chmod(destination,0o600);json.dump(plan,stream,indent=2);stream.write('\n')
  print('ENGINE_REARM_PLAN_SHA256='+sha(destination));return
 if rearming:
  rearm_plan=check_plan(a.engine_rearm_plan,a.engine_rearm_plan_sha256,backup,b,components,native,root,probe_info,a.rearm_previous_media)
 elif native and native.get('engine_rearm_plan_sha256'):raise SystemExit('再arm native候補には対応する明示planが必要です')
 if probe:
  files[service_name+'.exe']=probe_info['exe_sha256']
  if engine and probe_info['certificate_thumbprint']!=ENGINE_CERT:raise SystemExit('Engine service署名certの固定thumbprint不一致')
  files['hardware-probe-development.cer']=probe_info['certificate_sha256']
  files['private-system/SYSTEM.patched']=native['sha256']
 if rearming:files['engine-rearm-plan.json']=sha(a.engine_rearm_plan)
 if wifi_archive:files['previous-wifi-checkpoints.json']=sha(wifi_archive)
 prepare_name='prepare-codec-msdc-engine.cmd.in' if codec_batch else ('prepare-mt8167-engine-probe.cmd.in' if engine else 'prepare-mt8167-hardware-probe.cmd.in')
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
 if rearming:
  staged_rearm=stage/'engine-rearm-plan.json';shutil.copyfile(a.engine_rearm_plan,staged_rearm)
  if sha(staged_rearm)!=a.engine_rearm_plan_sha256.lower():raise SystemExit('staging中のrearm plan変更')
 if wifi_archive:
  staged_wifi_archive=stage/'previous-wifi-checkpoints.json';shutil.copyfile(wifi_archive,staged_wifi_archive)
  if sha(staged_wifi_archive)!=files['previous-wifi-checkpoints.json']:raise SystemExit('staging旧WiFi記録hash不一致')
 if probe:certificates['hardware_probe']=staged_probe_cert.read_bytes()
 trust=certificate_trust_commands(certificates)
 cmd=(here/'run-mt8167-shared-io-update.cmd.in').read_text().replace('@TOKEN@',token).replace('@JOURNAL@',token.lower()).replace('@HOST_BACKUP_SHA@',sha(backup)).replace('@TRUST_COMMANDS@','\n'.join(trust)).replace('@AUDIO_INF@',audio_inf).replace('@OPTIONAL_DRIVERS@','\n'.join(optional_drivers))
 if probe:
  prepare=(here/prepare_name).read_text().replace('@SYSTEM_SHA@',native['sha256']).replace('@SYSTEM_MANIFEST_SHA@',native['manifest_sha256'])
  prepare=prepare.replace('@ENGINE_REARM@',archive_commands(a.engine_rearm_plan_sha256.lower(),rearm_plan) if rearming else 'rem Explicit rearm not selected; refuse any existing marker.')
  if wifi_archive:
   archive='C:\\a04backup\\engine-rearm-'+a.engine_rearm_plan_sha256.lower()
   extra_archive=['copy /b /y X:\\a04br3\\previous-wifi-checkpoints.json '+archive+'\\previous-wifi-checkpoints.json >> \"%A04_REPORT%\" 2>&1','if errorlevel 1 goto dismfail','if not exist '+archive+'\\previous-wifi-checkpoints.json goto dismfail','>> \"%A04_REPORT%\" echo PREVIOUS_WIFI_CHECKPOINT_SHA256='+files['previous-wifi-checkpoints.json']]
   prepare=prepare.replace('if exist C:\\Windows\\System32\\MT8167EngineProbe\\attempted.bin goto dismfail','\n'.join(extra_archive)+'\nif exist C:\\Windows\\System32\\MT8167EngineProbe\\attempted.bin goto dismfail',1)
  if a.deferred_package_map and not codec_batch:
   extra=['if not exist C:\\Windows\\System32\\MT8167EngineProbe\\drivers\\display mkdir C:\\Windows\\System32\\MT8167EngineProbe\\drivers\\display >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail']
   for extension in ('inf','sys','cat'):
    extra+=['copy /b /y X:\\a04br3\\shared-io\\display\\mt8167-display.'+extension+' C:\\Windows\\System32\\MT8167EngineProbe\\drivers\\display\\mt8167-display.'+extension+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail']
   if 'sd' in deferred_policy['packages']:
    extra+=['if not exist C:\\Windows\\System32\\MT8167EngineProbe\\drivers\\sd mkdir C:\\Windows\\System32\\MT8167EngineProbe\\drivers\\sd >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail']
    for name in ('mt8167-sd.inf','mt8167-sd.sys','mt8167-sd.cat'):
     extra+=['copy /b /y X:\\a04br3\\shared-io\\sd\\'+name+' C:\\Windows\\System32\\MT8167EngineProbe\\drivers\\sd\\'+name+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail']
   if 'gpu' in deferred_policy['packages']:
    extra+=['if not exist C:\\Windows\\System32\\MT8167EngineProbe\\drivers\\gpu mkdir C:\\Windows\\System32\\MT8167EngineProbe\\drivers\\gpu >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail']
    for name in ('mt8167-ge8300.inf','mt8167-ge8300.sys','mt8167-ge8300.cat','ge8300-mips.fw'):
     extra+=['copy /b /y X:\\a04br3\\shared-io\\gpu\\'+name+' C:\\Windows\\System32\\MT8167EngineProbe\\drivers\\gpu\\'+name+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail']
   if 'mt8167-brightness-native.exe' in deferred_policy['clients']:
    extra+=['copy /b /y X:\\a04br3\\shared-io\\clients\\mt8167-brightness-native.exe C:\\Windows\\System32\\MT8167EngineProbe\\clients\\mt8167-brightness-native.exe >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail',
            'if exist '+(r'C:\Windows\System32\MT8167EngineProbe\logs\brightness-native.log' if deferred_policy.get('brightness_client',{}).get('log_path')==r'C:\Windows\System32\MT8167EngineProbe\logs\brightness-native.log' else r'C:\MT8167EngineProbe\brightness-native.log')+' goto dismfail']
   prepare=prepare.replace('copy /y X:\\a04br3\\MT8167EngineProbe.exe','\n'.join(extra)+'\ncopy /y X:\\a04br3\\MT8167EngineProbe.exe')
  validate_staged_package_copies(prepare,staged_pkg)
  cmd=cmd.replace('@PROBE_PREPARE@',prepare)
 else:cmd=cmd.replace('@PROBE_PREPARE@','rem Hardware probe service not included; PnP only.')
 from engine_staging_profile import apply_profile
 if a.rearm_previous_media and rearming and sha(a.rearm_previous_media)!=rearm_plan['previous_media_manifest_sha256']:raise SystemExit('staging旧媒体はnative再arm planのhashと一致が必要です')
 previous_for_staging=json.loads(a.rearm_previous_media.read_text()) if a.rearm_previous_media else None
 cmd,staging_info=apply_profile(cmd,deferred_policy if a.deferred_package_map else None,b,previous_for_staging,components)
 if re.search('@[A-Z_]+@',cmd):raise SystemExit('未置換変数')
 if re.search(r'(?im)^reg(?:\.exe)?\s+(?:load|unload)\s+.*(?:config\\SYSTEM|Mt8167InstallSystem)',cmd):raise SystemExit('WinPE SYSTEM RegLoadは禁止です')
 (stage/'run-update-once.cmd').write_bytes(cmd.replace('\r','').replace('\n','\r\n').encode())
 (stage/'startnet.cmd').write_bytes(b'@echo off\r\nmountvol /N\r\nwpeinit\r\nif errorlevel 1 exit /b 2\r\nmountvol /N\r\ncall X:\\a04br3\\run-update-once.cmd\r\n')
 image=out/'update.wim';shutil.copyfile(base/'maintenance-staged.wim',image)
 env=os.environ.copy();env.update(TMPDIR=str(stage),LD_LIBRARY_PATH=str(tools/'wimtools'),PYTHONPATH=str(tools/'python-fat'),PYTHONDONTWRITEBYTECODE='1')
 pairs=[(stage/'startnet.cmd','/Windows/System32/startnet.cmd'),(stage/'run-update-once.cmd','/a04br3/run-update-once.cmd'),(staged_pkg,'/a04br3/shared-io'),(staged_fw,'/a04br3/shared-fw')]
 if probe:pairs.extend(((staged_probe,'/a04br3/'+service_name+'.exe'),(staged_probe_cert,'/a04br3/hardware-probe-development.cer'),(private_system,'/a04br3/private-system/SYSTEM.patched')))
 if rearming:pairs.append((staged_rearm,'/a04br3/engine-rearm-plan.json'))
 if wifi_archive:pairs.append((staged_wifi_archive,'/a04br3/previous-wifi-checkpoints.json'))
 commands=''.join('add "'+str(src)+'" '+dst+'\n' for src,dst in pairs)
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'update',str(image),'1'],input=commands,text=True,env=env,check=True)
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'verify',str(image)],env=env,check=True)
 subprocess.run(['python3',str(here/'replace-esp-wim.py'),'--template',str(base/'maintenance-staged-esp.img'),'--wim',str(image),'--output',str(out/'update-esp.img'),'--template-bcd-sha256','65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d','--template-wim-sha256','0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'],env=env,check=True)
 (out/'manifest.json').write_text(json.dumps({'private_do_not_distribute':bool(a.private_media),'audio_mode':'engine_pcm_retained' if engine else 'private_fixed_tone_diagnostic','selected_service':service_name,'shared_parent_count':1,'engine_rearm_plan':rearm_plan,'expected_installed_sys':{name.rsplit('/',1)[-1]:digest for component in components.values() for name,digest in component.get('files',{}).items() if name.endswith('.sys')},'retained_files':deferred_policy.get('retained_files',{}) if a.deferred_package_map else {},'transaction':deferred_policy.get('transaction') if a.deferred_package_map else None,'installed_sys_hashes_verified':False,'components':components,'fixed_clients':list(deferred_policy['clients']) if a.deferred_package_map else ['mt6630-wlan-probe.exe',audio_client],'prepared_system':native,'probe':probe_info,'trusted_certificates':{role:hashlib.sha1(der).hexdigest().upper() for role,der in certificates.items()},'staging':staging_info,'source_commit':a.source_commit,'token':token,'files':files,'host_backup_manifest_sha256':sha(backup),'recovery_boot_id':b['recovery_boot_id'],'input_validator_sha256':sha(here/'private_hardware_probe_input.py'),'engine_selector_sha256':sha(here/'engine_io_package.py'),'builder_sha256':sha(Path(__file__).resolve()),'command_sha256':sha(stage/'run-update-once.cmd'),'esp_sha256':sha(out/'update-esp.img'),'signature_verification':'hash/source/certificate pins here; clients and Engine CAT WinVerifyTrust at runtime; INF/SYS catalog membership by Windows servicing/PnP','status':'generated; hardware operation unverified'},indent=2)+'\n')
 if a.private_media:
  for p in out.rglob('*'):p.chmod(0o700 if p.is_dir() else 0o600)
if __name__=='__main__':main()
