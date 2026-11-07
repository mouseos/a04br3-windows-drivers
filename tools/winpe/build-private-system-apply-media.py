#!/usr/bin/env python3
"""非配布の保守媒体。正規Windows APIで準備したSYSTEMだけをコピーする。"""
import argparse,hashlib,importlib.util,json,os,re,shutil,struct,subprocess,sys
from pathlib import Path

def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 ap=argparse.ArgumentParser()
 for n in ('workspace','base','system','patch-manifest','backup','out','tools'):ap.add_argument('--'+n,type=Path,required=True)
 ap.add_argument('--source-commit',required=True)
 ap.add_argument('--gpu-installer-package',type=Path)
 a=ap.parse_args();root=a.workspace.resolve(strict=True)
 base,system,patch,backup,out,tools=[getattr(a,n.replace('-','_')).resolve() for n in ('base','system','patch-manifest','backup','out','tools')]
 if out.exists() or any(not p.is_relative_to(root) for p in (base,system,patch,backup,out,tools)) or not re.fullmatch('[0-9a-f]{40}',a.source_commit):raise SystemExit('固定sourceとroot内入力・新規private出力が必要です')
 here=Path(__file__).resolve().parent
 spec=importlib.util.spec_from_file_location('backup_validation',here/'build-mt8167-shared-io-update-media.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod);b=mod.backup_check(backup,root)
 if sha(base/'maintenance-staged.wim')!='0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4' or sha(base/'maintenance-staged-esp.img')!='22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce':raise SystemExit('base不一致')
 with system.open('rb') as f:header=f.read(4096)
 if len(header)!=4096 or header[:4]!=b'regf' or struct.unpack_from('<II',header,20)!=(1,5):raise SystemExit('SYSTEMのregf1.5形式が必要です')
 seq=struct.unpack_from('<II',header,4)
 if seq[0]!=seq[1]:raise SystemExit('SYSTEMがcleanではありません')
 checksum=0
 for v in struct.unpack('<127I',header[:508]):checksum^=v
 checksum=0xfffffffe if checksum==0xffffffff else 1 if checksum==0 else checksum
 if checksum!=struct.unpack_from('<I',header,508)[0]:raise SystemExit('SYSTEM header checksum不一致')
 sys.path.insert(0,str(tools/'python-hive'))
 from regipy.registry import RegistryHive
 h=RegistryHive(str(system));select={v.name:v.value for v in h.get_key(r'\Select').iter_values()}
 if select.get('Current')!=1:raise SystemExit('対象ControlSet不一致')
 vals={v.name:v.value for v in h.get_key(r'\ControlSet001\Services\MT8167DeviceInstall').iter_values()}
 if vals.get('Type')!=16 or vals.get('Start')!=2 or vals.get('ObjectName')!='LocalSystem':raise SystemExit('一回限りサービスの登録不一致')
 path=vals.get('ImagePath','').strip('"').lower()
 if path not in (r'c:\windows\system32\mt8167deviceinstall.exe',r'%systemroot%\system32\mt8167deviceinstall.exe'):raise SystemExit('サービスpath不一致')
 # Validate the patch record as JSON; the native patcher owns its detailed schema.
 json.loads(patch.read_text(encoding='utf-8-sig'))
 gpu_files={};gpu_record=None;native_record=None
 if a.gpu_installer_package:
  checkout=here.parents[1]
  if subprocess.check_output(['git','-C',str(checkout),'rev-parse','HEAD'],text=True).strip()!=a.source_commit or subprocess.check_output(['git','-C',str(checkout),'status','--porcelain'],text=True).strip():raise ValueError('GPU媒体はcleanな固定source checkoutから生成してください')
  from private_hardware_probe_input import prepared_system_check,private_path
  from gpu_media_input import gpu_input
  private_path(out,root)
  native_record=prepared_system_check(system,patch,sha(system),root,b,'MT8167DeviceInstall')
  gpu_files,gpu_record=gpu_input(a.gpu_installer_package,root)
 out.mkdir(mode=0o700);stage=out/'stage';stage.mkdir(mode=0o700)
 token=hashlib.sha256((sha(system)+sha(patch)+sha(backup)+sha(here/'run-private-system-apply.cmd.in')+sha(Path(__file__).resolve())+json.dumps(gpu_record,sort_keys=True)).encode()).hexdigest()[:16].upper()
 cmd=(here/'run-private-system-apply.cmd.in').read_text().replace('@TOKEN@',token).replace('@JOURNAL@',token.lower()).replace('@HOST_BACKUP_SHA@',sha(backup)).replace('@SYSTEM_SHA@',sha(system))
 extra_pairs=[]
 if gpu_files:
  fixed=stage/'gpu-fixed';fixed.mkdir(mode=0o700)
  for name,path in gpu_files.items():
   dest=fixed/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,dest)
   if sha(dest)!=gpu_record['files'][name]:raise ValueError('GPU staging中の入力変更')
  extra_pairs.append((fixed,'/a04br3/gpu-fixed'))
  commands=[r'if exist "C:\Windows\System32\MT8167GpuUpdate" goto backupfail',
   r'if not exist "C:\Windows\System32\MT8167DeviceInstall.exe" goto backupfail',
   r'mkdir "%A04_BK%\installer-before" >> "%A04_REPORT%" 2>&1',
   r'if errorlevel 1 goto backupfail',
   r'xcopy /h /y "C:\Windows\System32\MT8167DeviceInstall.exe" "%A04_BK%\installer-before" >> "%A04_REPORT%" 2>&1',
   r'if errorlevel 1 goto backupfail',
   r'mkdir "C:\Windows\System32\MT8167GpuUpdate\provider" >> "%A04_REPORT%" 2>&1',
   r'if errorlevel 1 goto dismfail']
  for name in sorted(gpu_files):
   target=((r'C:\Windows\System32' if name=='MT8167DeviceInstall.exe' else r'C:\Windows\System32\MT8167GpuUpdate')+'\\'+name).replace('/','\\')
   source=('X:\\a04br3\\gpu-fixed\\'+name).replace('/','\\')
   commands.extend(['copy /b /y "'+source+'" "'+target+'" >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail'])
  commands.append('>> "%A04_REPORT%" echo GPU_FIXED_FILES_COPY_EXIT=0')
  cmd=cmd.replace('rem No REG LOAD of SYSTEM in WinPE.', '\n'.join(commands)+'\nrem No REG LOAD of SYSTEM in WinPE.')
 if re.search('@[A-Z_]+@',cmd):raise SystemExit('未置換変数')
 (stage/'run-update-once.cmd').write_bytes(cmd.replace('\r','').replace('\n','\r\n').encode())
 (stage/'startnet.cmd').write_bytes(b'@echo off\r\nmountvol /N\r\nwpeinit\r\nif errorlevel 1 exit /b 2\r\nmountvol /N\r\ncall X:\\a04br3\\run-update-once.cmd\r\n')
 image=out/'update.wim';shutil.copyfile(base/'maintenance-staged.wim',image)
 env=os.environ.copy();env.update(TMPDIR=str(stage),LD_LIBRARY_PATH=str(tools/'wimtools'),PYTHONPATH=str(tools/'python-fat'),PYTHONDONTWRITEBYTECODE='1')
 pairs=[(stage/'startnet.cmd','/Windows/System32/startnet.cmd'),(stage/'run-update-once.cmd','/a04br3/run-update-once.cmd'),(system,'/a04br3/private-system/SYSTEM.patched')]+extra_pairs
 commands=''.join('add "'+str(src)+'" '+dst+'\n' for src,dst in pairs)
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'update',str(image),'1'],input=commands,text=True,env=env,check=True)
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'verify',str(image)],env=env,check=True)
 subprocess.run(['python3',str(here/'replace-esp-wim.py'),'--template',str(base/'maintenance-staged-esp.img'),'--wim',str(image),'--output',str(out/'update-esp.img'),'--template-bcd-sha256','65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d','--template-wim-sha256','0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'],env=env,check=True)
 (out/'manifest.json').write_text(json.dumps({'private_do_not_distribute':True,'source_commit':a.source_commit,'token':token,'recovery_boot_id':b['recovery_boot_id'],'host_backup_manifest_sha256':sha(backup),'system_sha256':sha(system),'patch_manifest_sha256':sha(patch),'gpu_fixed_update':gpu_record,'native_preparation':native_record,'builder_sha256':sha(Path(__file__).resolve()),'command_sha256':sha(stage/'run-update-once.cmd'),'esp_sha256':sha(out/'update-esp.img'),'status':'generated; service execution and PnP bind unverified'},indent=2)+'\n')
 for p in out.rglob('*'):
  if p.is_file():p.chmod(0o600)
if __name__=='__main__':main()
