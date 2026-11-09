#!/usr/bin/env python3
"""fresh54専用・offline三package→matched boot→normal診断only。非配布媒体。"""
import argparse, hashlib, importlib.util, json, os, re, shutil, subprocess
from pathlib import Path
from render_profile_policy import load_map, generate, rooted, sha, require, FRESH54, SIGNER, TARGETS
from private_hardware_probe_input import prepared_system_check, certificate_trust_commands, private_path

def main():
 ap=argparse.ArgumentParser()
 for n in ('workspace','base','package-map','service-package','prepared-system','prepared-system-manifest','out','tools'):ap.add_argument('--'+n,type=Path,required=True)
 ap.add_argument('--prepared-system-sha256',required=True);ap.add_argument('--source-commit',required=True);ap.add_argument('--enable-render-profile',action='store_true');a=ap.parse_args()
 root=a.workspace.resolve(strict=True);require(a.enable_render_profile,'既定OFF/明示opt-in必須');require(re.fullmatch('[0-9a-f]{40}',a.source_commit),'固定source必須')
 here=Path(__file__).resolve().parent;repo=here.parents[1]
 require(subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()==a.source_commit,'builder source不一致')
 require(not subprocess.check_output(['git','-C',str(repo),'status','--porcelain'],text=True).strip(),'dirty source拒否')
 policy=load_map(a.package_map,root);backup=root/policy['backup_path'];b=json.loads(backup.read_text());base=rooted(root,a.base);tools=rooted(root,a.tools);service=rooted(root,a.service_package)
 out=a.out.resolve();private_path(out,root);require(not out.exists(),'既存出力を変更しません')
 require(sha(base/'maintenance-staged.wim')=='0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4' and sha(base/'maintenance-staged-esp.img')=='22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce','既知base不一致')
 sm=json.loads((service/'render-service-manifest.json').read_text(encoding='utf-8-sig'));require(sm.get('profile')=='offline-render-diagnostic-v1' and sm.get('service')=='MT8167RenderProbe' and sm.get('signed') is True and sm.get('signature_status')=='Valid' and sm.get('signing_thumbprint')==SIGNER and sm.get('source_commit')==a.source_commit and sm.get('source_dirty') is False,'固定署名service不一致')
 require(sm.get('policy')==policy and sha(service/'render-policy-manifest.json')==sm['policy_manifest_sha256'] and sha(service/'MT8167RenderProbe.exe')==sm['exe_sha256'],'service/policy/EXE pin不一致')
 require((service/'render-deferred-policy.h').read_text()==generate(policy),'compile policy不一致')
 require(sha(service/'compare-files.exe')==sm.get('comparator',{}).get('exe_sha256') and sm['comparator'].get('signature_status')=='Valid' and sm['comparator'].get('signer_thumbprint')==SIGNER,'同梱native comparator署名/hash不一致')
 require(hashlib.sha1((service/'mt8167-development.cer').read_bytes()).hexdigest().upper()==SIGNER,'service cert不一致')
 system=rooted(root,a.prepared_system);result=rooted(root,a.prepared_system_manifest)
 native=prepared_system_check(system,result,a.prepared_system_sha256,root,b,'MT8167RenderProbe');require(not native.get('engine_rearm_plan_sha256'),'初回Render専用、既存Engine再arm不可')
 out.mkdir(mode=0o700);stage=out/'stage';stage.mkdir(mode=0o700);files={}
 def copy(src,dest):
  target=stage/dest;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,target);files[dest]=sha(target)
 copy(service/'compare-files.exe','compare-files.exe');copy(system,'private-system/SYSTEM.patched');copy(service/'MT8167RenderProbe.exe','MT8167RenderProbe.exe');copy(service/'mt8167-development.cer','render-development.cer')
 copy((root/policy['client']['directory'])/'ge8300-d3d-clear.exe','render-io/clients/ge8300-d3d-clear.exe')
 for role,p in policy['packages'].items():
  for name in p['files']:copy((root/p['directory'])/name,'render-io/'+role+'/'+name)
 # Copy/compare only fixed core files. No sibling directory enumeration or old
 # Engine state mutation. NTFS replay-induced changes cause a real refusal.
 allowed_sys={'mt8167-shared.sys','mt8167-audio.sys','mt8167-usb20.sys','mt6630-runtime.sys','mt8167-display.sys','mt8167-ge8300.sys','mt8167-toprgu.sys','mt8167-sd.sys','mt8167-msdc.sys'}
 checks=[];hives=0
 for e in b['files']:
  name=e['source_path'].replace('\\','/');parts=name.split('/');fixed=False
  if name.startswith('Windows/System32/config/') and parts[-1] in {x+s for x in ('SYSTEM','SOFTWARE','DRIVERS') for s in ('','.LOG1','.LOG2')}:fixed=True;hives+=1
  if name.lower().startswith('windows/system32/drivers/') and parts[-1].lower() in allowed_sys:fixed=True
  if fixed:
   rel='fresh54/'+str(len(checks))+'.bin';copy(backup.parent/e['backup_path'],rel);checks+=['X:\\a04br3\\compare-files.exe X:\\a04br3\\'+rel.replace('/','\\')+' "C:\\'+name.replace('/','\\')+'" >> "%A04_REPORT%" 2>&1','set "A04_COMPARE_RC=%errorlevel%"','>> "%A04_REPORT%" echo HOST_BACKUP_COMPARE_INDEX='+str(len(checks)//4)+' EXIT=%A04_COMPARE_RC%','if not "%A04_COMPARE_RC%"=="0" goto dismfail']
 require(hives==9,'fresh54九hive/log必須')
 prepare=checks+['if exist C:\\Windows\\System32\\MT8167RenderProbe goto dismfail', 'mkdir C:\\Windows\\System32\\MT8167RenderProbe\\logs >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail','mkdir C:\\Windows\\System32\\MT8167RenderProbe\\clients >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail']
 for role,p in policy['packages'].items():
  dst='C:\\Windows\\System32\\MT8167RenderProbe\\drivers\\'+role;prepare+=['mkdir '+dst+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail']
  for name in p['files']:prepare+=['copy /b /y X:\\a04br3\\render-io\\'+role+'\\'+name+' '+dst+'\\'+name+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail']
 prepare+=['copy /b /y X:\\a04br3\\render-io\\clients\\ge8300-d3d-clear.exe C:\\Windows\\System32\\MT8167RenderProbe\\clients\\ >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail','copy /b /y X:\\a04br3\\MT8167RenderProbe.exe C:\\Windows\\System32\\MT8167RenderProbe.exe >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail','copy /b /y X:\\a04br3\\private-system\\SYSTEM.patched C:\\Windows\\System32\\config\\SYSTEM >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto dismfail']
 # Source template retains tested map/guard/journal/DISM/release/report framing.
 cmd=(here/'run-mt8167-shared-io-update.cmd.in').read_text();start=cmd.index('if not exist C:\\Windows\\System32\\drivers\\a04br3');end=cmd.index('copy /y X:\\a04br3\\dism-*.txt',start)
 driverlines=[]
 for role in TARGETS:driverlines+=['call :adddriver '+role.upper()+' X:\\a04br3\\render-io\\'+role+'\\'+TARGETS[role][3][0],'if errorlevel 1 goto dismfail']
 cmd=cmd[:start]+'\n'.join(driverlines)+'\n'+cmd[end:]
 token=hashlib.sha256((json.dumps(files,sort_keys=True)+sha(backup)+sha(result)+sha(here/'mt8167-render-probe.c')).encode()).hexdigest()[:16].upper()
 cmd=cmd.replace('@TOKEN@',token).replace('@JOURNAL@',token.lower()).replace('@HOST_BACKUP_SHA@',FRESH54).replace('@PROBE_PREPARE@','\n'.join(prepare)).replace('@TRUST_COMMANDS@','\n'.join(certificate_trust_commands({'render':(service/'mt8167-development.cer').read_bytes()})))
 require(not re.search('@[A-Z_]+@',cmd),'未置換template');require(not re.search(r'(?im)^reg(?:\.exe)?\s+load\s+.*config\\SYSTEM',cmd),'WinPE SYSTEM RegLoad禁止')
 (stage/'run-update-once.cmd').write_bytes(cmd.replace('\r','').replace('\n','\r\n').encode());files['run-update-once.cmd']=sha(stage/'run-update-once.cmd')
 (stage/'startnet.cmd').write_bytes(b'@echo off\r\nmountvol /N\r\nwpeinit\r\nif errorlevel 1 exit /b 2\r\nmountvol /N\r\ncall X:\\a04br3\\run-update-once.cmd\r\n')
 image=out/'update.wim';shutil.copyfile(base/'maintenance-staged.wim',image);env=os.environ.copy();env.update(TMPDIR=str(stage),LD_LIBRARY_PATH=str(tools/'wimtools'),PYTHONPATH=str(tools/'python-fat'),PYTHONDONTWRITEBYTECODE='1')
 commands='add "'+str(stage/'startnet.cmd')+'" /Windows/System32/startnet.cmd\n'
 for p in sorted(stage.rglob('*')):
  if p.is_file() and p.name!='startnet.cmd':commands+='add "'+str(p)+'" /a04br3/'+str(p.relative_to(stage))+'\n'
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'update',str(image),'1'],input=commands,text=True,env=env,check=True)
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'verify',str(image)],env=env,check=True)
 subprocess.run(['python3','-B',str(here/'replace-esp-wim.py'),'--template',str(base/'maintenance-staged-esp.img'),'--wim',str(image),'--output',str(out/'update-esp.img'),'--template-bcd-sha256','65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d','--template-wim-sha256','0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'],env=env,check=True)
 expected={n:h for role in policy['packages'].values() for n,h in role['files'].items() if n.endswith(('.sys','.dll','.fw'))}
 manifest={'private_do_not_distribute':True,'source_commit':a.source_commit,'profile':policy['profile'],'selected_service':'MT8167RenderProbe','host_backup_manifest_sha256':FRESH54,'recovery_boot_id':b['recovery_boot_id'],'token':token,'policy':policy,'policy_manifest_sha256':sha(service/'render-policy-manifest.json'),'service_exe_sha256':sha(service/'MT8167RenderProbe.exe'),'backup_comparator':sm['comparator'],'probe':sm,'expected_installed_sys':{k:v for k,v in expected.items() if k.endswith('.sys')},'prepared_system':native,'files':files,'esp_sha256':sha(out/'update-esp.img'),'wim_sha256':sha(image),'expected_installed_files':expected,'offline_dism_packages':list(TARGETS),'normal_install_allowed':False,'boot_sequence':policy['sequence'],'required_next_boot':policy['boot'],'hardware_verified':False,'status':'generated-host-verified-runtime-unverified'}
 (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 for p in out.rglob('*'):
  if p.is_file():p.chmod(0o600)
if __name__=='__main__':main()
